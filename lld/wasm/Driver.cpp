//===- Driver.cpp ---------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "lld/Common/Driver.h"
#include "Config.h"
#include "InputChunks.h"
#include "InputGlobal.h"
#include "InputTable.h"
#include "MarkLive.h"
#include "SymbolTable.h"
#include "Writer.h"
#include "lld/Common/Args.h"
#include "lld/Common/ErrorHandler.h"
#include "lld/Common/Filesystem.h"
#include "lld/Common/Memory.h"
#include "lld/Common/Reproduce.h"
#include "lld/Common/Strings.h"
#include "lld/Common/Version.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/Object/Wasm.h"
#include "llvm/Option/Arg.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/Parallel.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/TarWriter.h"
#include "llvm/Support/TargetSelect.h"

#define DEBUG_TYPE "lld"

using namespace llvm;
using namespace llvm::object;
using namespace llvm::sys;
using namespace llvm::wasm;

namespace lld {
namespace wasm {
Configuration *config;

namespace {

// Create enum with OPT_xxx values for each option in Options.td
enum {
  OPT_INVALID = 0,
#define OPTION(_1, _2, ID, _4, _5, _6, _7, _8, _9, _10, _11, _12) OPT_##ID,
#include "Options.inc"
#undef OPTION
};

class LinkerDriver {
public:
  void linkerMain(ArrayRef<const char *> argsArr);

private:
  void createFiles(opt::InputArgList &args);
  void addFile(StringRef path);
  void addLibrary(StringRef name);

  // True if we are in --whole-archive and --no-whole-archive.
  bool inWholeArchive = false;

  std::vector<InputFile *> files;
};
} // anonymous namespace

bool link(ArrayRef<const char *> args, bool canExitEarly, raw_ostream &stdoutOS,
          raw_ostream &stderrOS) {
  lld::stdoutOS = &stdoutOS;
  lld::stderrOS = &stderrOS;

  errorHandler().cleanupCallback = []() { freeArena(); };

  errorHandler().logName = args::getFilenameWithoutExe(args[0]);
  errorHandler().errorLimitExceededMsg =
      "too many errors emitted, stopping now (use "
      "-error-limit=0 to see all errors)";
  stderrOS.enable_colors(stderrOS.has_colors());

  config = make<Configuration>();
  symtab = make<SymbolTable>();

  LinkerDriver().linkerMain(args);

  // Exit immediately if we don't need to return to the caller.
  // This saves time because the overhead of calling destructors
  // for all globally-allocated objects is not negligible.
  if (canExitEarly)
    exitLld(errorCount() ? 1 : 0);

  return !errorCount();
}

// Create prefix string literals used in Options.td
#define PREFIX(NAME, VALUE) const char *const NAME[] = VALUE;
#include "Options.inc"
#undef PREFIX

// Create table mapping all options defined in Options.td
static const opt::OptTable::Info optInfo[] = {
#define OPTION(X1, X2, ID, KIND, GROUP, ALIAS, X7, X8, X9, X10, X11, X12)      \
  {X1, X2, X10,         X11,         OPT_##ID, opt::Option::KIND##Class,       \
   X9, X8, OPT_##GROUP, OPT_##ALIAS, X7,       X12},
#include "Options.inc"
#undef OPTION
};

namespace {
class WasmOptTable : public llvm::opt::OptTable {
public:
  WasmOptTable() : OptTable(optInfo) {}
  opt::InputArgList parse(ArrayRef<const char *> argv);
};
} // namespace

// Set color diagnostics according to -color-diagnostics={auto,always,never}
// or -no-color-diagnostics flags.
static void handleColorDiagnostics(opt::InputArgList &args) {
  auto *arg = args.getLastArg(OPT_color_diagnostics, OPT_color_diagnostics_eq,
                              OPT_no_color_diagnostics);
  if (!arg)
    return;
  if (arg->getOption().getID() == OPT_color_diagnostics) {
    lld::errs().enable_colors(true);
  } else if (arg->getOption().getID() == OPT_no_color_diagnostics) {
    lld::errs().enable_colors(false);
  } else {
    StringRef s = arg->getValue();
    if (s == "always")
      lld::errs().enable_colors(true);
    else if (s == "never")
      lld::errs().enable_colors(false);
    else if (s != "auto")
      error("unknown option: --color-diagnostics=" + s);
  }
}

static cl::TokenizerCallback getQuotingStyle(opt::InputArgList &args) {
  if (auto *arg = args.getLastArg(OPT_rsp_quoting)) {
    StringRef s = arg->getValue();
    if (s != "windows" && s != "posix")
      error("invalid response file quoting: " + s);
    if (s == "windows")
      return cl::TokenizeWindowsCommandLine;
    return cl::TokenizeGNUCommandLine;
  }
  if (Triple(sys::getProcessTriple()).isOSWindows())
    return cl::TokenizeWindowsCommandLine;
  return cl::TokenizeGNUCommandLine;
}

// Find a file by concatenating given paths.
static Optional<std::string> findFile(StringRef path1, const Twine &path2) {
  SmallString<128> s;
  path::append(s, path1, path2);
  if (fs::exists(s))
    return std::string(s);
  return None;
}

opt::InputArgList WasmOptTable::parse(ArrayRef<const char *> argv) {
  SmallVector<const char *, 256> vec(argv.data(), argv.data() + argv.size());

  unsigned missingIndex;
  unsigned missingCount;

  // We need to get the quoting style for response files before parsing all
  // options so we parse here before and ignore all the options but
  // --rsp-quoting.
  opt::InputArgList args = this->ParseArgs(vec, missingIndex, missingCount);

  // Expand response files (arguments in the form of @<filename>)
  // and then parse the argument again.
  cl::ExpandResponseFiles(saver, getQuotingStyle(args), vec);
  args = this->ParseArgs(vec, missingIndex, missingCount);

  handleColorDiagnostics(args);
  for (auto *arg : args.filtered(OPT_UNKNOWN))
    error("unknown argument: " + arg->getAsString(args));
  return args;
}

// Currently we allow a ".imports" to live alongside a library. This can
// be used to specify a list of symbols which can be undefined at link
// time (imported from the environment.  For example libc.a include an
// import file that lists the syscall functions it relies on at runtime.
// In the long run this information would be better stored as a symbol
// attribute/flag in the object file itself.
// See: https://github.com/WebAssembly/tool-conventions/issues/35
static void readImportFile(StringRef filename) {
  if (Optional<MemoryBufferRef> buf = readFile(filename))
    for (StringRef sym : args::getLines(*buf))
      config->allowUndefinedSymbols.insert(sym);
}

// Returns slices of MB by parsing MB as an archive file.
// Each slice consists of a member file in the archive.
std::vector<MemoryBufferRef> static getArchiveMembers(MemoryBufferRef mb) {
  std::unique_ptr<Archive> file =
      CHECK(Archive::create(mb),
            mb.getBufferIdentifier() + ": failed to parse archive");

  std::vector<MemoryBufferRef> v;
  Error err = Error::success();
  for (const Archive::Child &c : file->children(err)) {
    MemoryBufferRef mbref =
        CHECK(c.getMemoryBufferRef(),
              mb.getBufferIdentifier() +
                  ": could not get the buffer for a child of the archive");
    v.push_back(mbref);
  }
  if (err)
    fatal(mb.getBufferIdentifier() +
          ": Archive::children failed: " + toString(std::move(err)));

  // Take ownership of memory buffers created for members of thin archives.
  for (std::unique_ptr<MemoryBuffer> &mb : file->takeThinBuffers())
    make<std::unique_ptr<MemoryBuffer>>(std::move(mb));

  return v;
}

void LinkerDriver::addFile(StringRef path) {
  Optional<MemoryBufferRef> buffer = readFile(path);
  if (!buffer.hasValue())
    return;
  MemoryBufferRef mbref = *buffer;

  switch (identify_magic(mbref.getBuffer())) {
  case file_magic::archive: {
    SmallString<128> importFile = path;
    path::replace_extension(importFile, ".imports");
    if (fs::exists(importFile))
      readImportFile(importFile.str());

    // Handle -whole-archive.
    if (inWholeArchive) {
      for (MemoryBufferRef &m : getArchiveMembers(mbref)) {
        auto *object = createObjectFile(m, path);
        // Mark object as live; object members are normally not
        // live by default but -whole-archive is designed to treat
        // them as such.
        object->markLive();
        files.push_back(object);
      }

      return;
    }

    std::unique_ptr<Archive> file =
        CHECK(Archive::create(mbref), path + ": failed to parse archive");

    if (!file->isEmpty() && !file->hasSymbolTable()) {
      error(mbref.getBufferIdentifier() +
            ": archive has no index; run ranlib to add one");
    }

    files.push_back(make<ArchiveFile>(mbref));
    return;
  }
  case file_magic::bitcode:
  case file_magic::wasm_object:
    files.push_back(createObjectFile(mbref));
    break;
  default:
    error("unknown file type: " + mbref.getBufferIdentifier());
  }
}

// Add a given library by searching it from input search paths.
void LinkerDriver::addLibrary(StringRef name) {
  for (StringRef dir : config->searchPaths) {
    if (Optional<std::string> s = findFile(dir, "lib" + name + ".a")) {
      addFile(*s);
      return;
    }
  }

  error("unable to find library -l" + name);
}

void LinkerDriver::createFiles(opt::InputArgList &args) {
  for (auto *arg : args) {
    switch (arg->getOption().getID()) {
    case OPT_l:
      addLibrary(arg->getValue());
      break;
    case OPT_INPUT:
      addFile(arg->getValue());
      break;
    case OPT_whole_archive:
      inWholeArchive = true;
      break;
    case OPT_no_whole_archive:
      inWholeArchive = false;
      break;
    }
  }
  if (files.empty() && errorCount() == 0)
    error("no input files");
}

static StringRef getEntry(opt::InputArgList &args) {
  auto *arg = args.getLastArg(OPT_entry, OPT_no_entry);
  if (!arg) {
    if (args.hasArg(OPT_relocatable))
      return "";
    if (args.hasArg(OPT_shared))
      return "__wasm_call_ctors";
    return "_start";
  }
  if (arg->getOption().getID() == OPT_no_entry)
    return "";
  return arg->getValue();
}

// Determines what we should do if there are remaining unresolved
// symbols after the name resolution.
static UnresolvedPolicy getUnresolvedSymbolPolicy(opt::InputArgList &args) {
  UnresolvedPolicy errorOrWarn = args.hasFlag(OPT_error_unresolved_symbols,
                                              OPT_warn_unresolved_symbols, true)
                                     ? UnresolvedPolicy::ReportError
                                     : UnresolvedPolicy::Warn;

  if (auto *arg = args.getLastArg(OPT_unresolved_symbols)) {
    StringRef s = arg->getValue();
    if (s == "ignore-all")
      return UnresolvedPolicy::Ignore;
    if (s == "import-functions")
      return UnresolvedPolicy::ImportFuncs;
    if (s == "report-all")
      return errorOrWarn;
    error("unknown --unresolved-symbols value: " + s);
  }

  // Legacy --allow-undefined flag which is equivalent to
  // --unresolve-symbols=ignore-all
  if (args.hasArg(OPT_allow_undefined))
    return UnresolvedPolicy::ImportFuncs;

  return errorOrWarn;
}

// Initializes Config members by the command line options.
static void readConfigs(opt::InputArgList &args) {
  config->bsymbolic = args.hasArg(OPT_Bsymbolic);
  config->checkFeatures =
      args.hasFlag(OPT_check_features, OPT_no_check_features, true);
  config->compressRelocations = args.hasArg(OPT_compress_relocations);
  config->demangle = args.hasFlag(OPT_demangle, OPT_no_demangle, true);
  config->disableVerify = args.hasArg(OPT_disable_verify);
  config->emitRelocs = args.hasArg(OPT_emit_relocs);
  config->experimentalPic = args.hasArg(OPT_experimental_pic);
  config->entry = getEntry(args);
  config->exportAll = args.hasArg(OPT_export_all);
  config->exportTable = args.hasArg(OPT_export_table);
  config->growableTable = args.hasArg(OPT_growable_table);
  errorHandler().fatalWarnings =
      args.hasFlag(OPT_fatal_warnings, OPT_no_fatal_warnings, false);
  config->importMemory = args.hasArg(OPT_import_memory);
  config->sharedMemory = args.hasArg(OPT_shared_memory);
  config->importTable = args.hasArg(OPT_import_table);
  config->ltoo = args::getInteger(args, OPT_lto_O, 2);
  config->ltoPartitions = args::getInteger(args, OPT_lto_partitions, 1);
  config->ltoNewPassManager =
      args.hasFlag(OPT_no_lto_legacy_pass_manager, OPT_lto_legacy_pass_manager,
                   LLVM_ENABLE_NEW_PASS_MANAGER);
  config->ltoDebugPassManager = args.hasArg(OPT_lto_debug_pass_manager);
  config->mapFile = args.getLastArgValue(OPT_Map);
  config->optimize = args::getInteger(args, OPT_O, 0);
  config->outputFile = args.getLastArgValue(OPT_o);
  config->relocatable = args.hasArg(OPT_relocatable);
  config->gcSections =
      args.hasFlag(OPT_gc_sections, OPT_no_gc_sections, !config->relocatable);
  config->mergeDataSegments =
      args.hasFlag(OPT_merge_data_segments, OPT_no_merge_data_segments,
                   !config->relocatable);
  config->pie = args.hasFlag(OPT_pie, OPT_no_pie, false);
  config->printGcSections =
      args.hasFlag(OPT_print_gc_sections, OPT_no_print_gc_sections, false);
  config->saveTemps = args.hasArg(OPT_save_temps);
  config->searchPaths = args::getStrings(args, OPT_L);
  config->shared = args.hasArg(OPT_shared);
  config->stripAll = args.hasArg(OPT_strip_all);
  config->stripDebug = args.hasArg(OPT_strip_debug);
  config->stackFirst = args.hasArg(OPT_stack_first);
  config->trace = args.hasArg(OPT_trace);
  config->thinLTOCacheDir = args.getLastArgValue(OPT_thinlto_cache_dir);
  config->thinLTOCachePolicy = CHECK(
      parseCachePruningPolicy(args.getLastArgValue(OPT_thinlto_cache_policy)),
      "--thinlto-cache-policy: invalid cache policy");
  config->unresolvedSymbols = getUnresolvedSymbolPolicy(args);
  errorHandler().verbose = args.hasArg(OPT_verbose);
  LLVM_DEBUG(errorHandler().verbose = true);

  config->initialMemory = args::getInteger(args, OPT_initial_memory, 0);
  config->globalBase = args::getInteger(args, OPT_global_base, 1024);
  config->maxMemory = args::getInteger(args, OPT_max_memory, 0);
  config->zStackSize =
      args::getZOptionValue(args, OPT_z, "stack-size", WasmPageSize);

  // Default value of exportDynamic depends on `-shared`
  config->exportDynamic =
      args.hasFlag(OPT_export_dynamic, OPT_no_export_dynamic, config->shared);

  // Parse wasm32/64.
  if (auto *arg = args.getLastArg(OPT_m)) {
    StringRef s = arg->getValue();
    if (s == "wasm32")
      config->is64 = false;
    else if (s == "wasm64")
      config->is64 = true;
    else
      error("invalid target architecture: " + s);
  }

  // --threads= takes a positive integer and provides the default value for
  // --thinlto-jobs=.
  if (auto *arg = args.getLastArg(OPT_threads)) {
    StringRef v(arg->getValue());
    unsigned threads = 0;
    if (!llvm::to_integer(v, threads, 0) || threads == 0)
      error(arg->getSpelling() + ": expected a positive integer, but got '" +
            arg->getValue() + "'");
    parallel::strategy = hardware_concurrency(threads);
    config->thinLTOJobs = v;
  }
  if (auto *arg = args.getLastArg(OPT_thinlto_jobs))
    config->thinLTOJobs = arg->getValue();

  if (auto *arg = args.getLastArg(OPT_features)) {
    config->features =
        llvm::Optional<std::vector<std::string>>(std::vector<std::string>());
    for (StringRef s : arg->getValues())
      config->features->push_back(std::string(s));
  }

  if (args.hasArg(OPT_print_map))
    config->mapFile = "-";
}

// Some Config members do not directly correspond to any particular
// command line options, but computed based on other Config values.
// This function initialize such members. See Config.h for the details
// of these values.
static void setConfigs() {
  config->isPic = config->pie || config->shared;

  if (config->isPic) {
    if (config->exportTable)
      error("-shared/-pie is incompatible with --export-table");
    config->importTable = true;
  }

  if (config->shared) {
    config->importMemory = true;
    config->unresolvedSymbols = UnresolvedPolicy::ImportFuncs;
  }
}

// Some command line options or some combinations of them are not allowed.
// This function checks for such errors.
static void checkOptions(opt::InputArgList &args) {
  if (!config->stripDebug && !config->stripAll && config->compressRelocations)
    error("--compress-relocations is incompatible with output debug"
          " information. Please pass --strip-debug or --strip-all");

  if (config->ltoo > 3)
    error("invalid optimization level for LTO: " + Twine(config->ltoo));
  if (config->ltoPartitions == 0)
    error("--lto-partitions: number of threads must be > 0");
  if (!get_threadpool_strategy(config->thinLTOJobs))
    error("--thinlto-jobs: invalid job count: " + config->thinLTOJobs);

  if (config->pie && config->shared)
    error("-shared and -pie may not be used together");

  if (config->outputFile.empty())
    error("no output file specified");

  if (config->importTable && config->exportTable)
    error("--import-table and --export-table may not be used together");

  if (config->relocatable) {
    if (!config->entry.empty())
      error("entry point specified for relocatable output file");
    if (config->gcSections)
      error("-r and --gc-sections may not be used together");
    if (config->compressRelocations)
      error("-r -and --compress-relocations may not be used together");
    if (args.hasArg(OPT_undefined))
      error("-r -and --undefined may not be used together");
    if (config->pie)
      error("-r and -pie may not be used together");
    if (config->sharedMemory)
      error("-r and --shared-memory may not be used together");
  }

  // To begin to prepare for Module Linking-style shared libraries, start
  // warning about uses of `-shared` and related flags outside of Experimental
  // mode, to give anyone using them a heads-up that they will be changing.
  //
  // Also, warn about flags which request explicit exports.
  if (!config->experimentalPic) {
    // -shared will change meaning when Module Linking is implemented.
    if (config->shared) {
      warn("creating shared libraries, with -shared, is not yet stable");
    }

    // -pie will change meaning when Module Linking is implemented.
    if (config->pie) {
      warn("creating PIEs, with -pie, is not yet stable");
    }
  }

  if (config->bsymbolic && !config->shared) {
    warn("-Bsymbolic is only meaningful when combined with -shared");
  }
}

// Force Sym to be entered in the output. Used for -u or equivalent.
static Symbol *handleUndefined(StringRef name) {
  Symbol *sym = symtab->find(name);
  if (!sym)
    return nullptr;

  // Since symbol S may not be used inside the program, LTO may
  // eliminate it. Mark the symbol as "used" to prevent it.
  sym->isUsedInRegularObj = true;

  if (auto *lazySym = dyn_cast<LazySymbol>(sym))
    lazySym->fetch();

  return sym;
}

static void handleLibcall(StringRef name) {
  Symbol *sym = symtab->find(name);
  if (!sym)
    return;

  if (auto *lazySym = dyn_cast<LazySymbol>(sym)) {
    MemoryBufferRef mb = lazySym->getMemberBuffer();
    if (isBitcode(mb))
      lazySym->fetch();
  }
}

static UndefinedGlobal *
createUndefinedGlobal(StringRef name, llvm::wasm::WasmGlobalType *type) {
  auto *sym = cast<UndefinedGlobal>(symtab->addUndefinedGlobal(
      name, None, None, WASM_SYMBOL_UNDEFINED, nullptr, type));
  config->allowUndefinedSymbols.insert(sym->getName());
  sym->isUsedInRegularObj = true;
  return sym;
}

static InputGlobal *createGlobal(StringRef name, bool isMutable) {
  llvm::wasm::WasmGlobal wasmGlobal;
  if (config->is64.getValueOr(false)) {
    wasmGlobal.Type = {WASM_TYPE_I64, isMutable};
    wasmGlobal.InitExpr.Opcode = WASM_OPCODE_I64_CONST;
    wasmGlobal.InitExpr.Value.Int64 = 0;
  } else {
    wasmGlobal.Type = {WASM_TYPE_I32, isMutable};
    wasmGlobal.InitExpr.Opcode = WASM_OPCODE_I32_CONST;
    wasmGlobal.InitExpr.Value.Int32 = 0;
  }
  wasmGlobal.SymbolName = name;
  return make<InputGlobal>(wasmGlobal, nullptr);
}

static GlobalSymbol *createGlobalVariable(StringRef name, bool isMutable) {
  InputGlobal *g = createGlobal(name, isMutable);
  return symtab->addSyntheticGlobal(name, WASM_SYMBOL_VISIBILITY_HIDDEN, g);
}

static GlobalSymbol *createOptionalGlobal(StringRef name, bool isMutable) {
  InputGlobal *g = createGlobal(name, isMutable);
  return symtab->addOptionalGlobalSymbols(name, WASM_SYMBOL_VISIBILITY_HIDDEN,
                                          g);
}

// Create ABI-defined synthetic symbols
static void createSyntheticSymbols() {
  if (config->relocatable)
    return;

  static WasmSignature nullSignature = {{}, {}};
  static WasmSignature i32ArgSignature = {{}, {ValType::I32}};
  static WasmSignature i64ArgSignature = {{}, {ValType::I64}};
  static llvm::wasm::WasmGlobalType globalTypeI32 = {WASM_TYPE_I32, false};
  static llvm::wasm::WasmGlobalType globalTypeI64 = {WASM_TYPE_I64, false};
  static llvm::wasm::WasmGlobalType mutableGlobalTypeI32 = {WASM_TYPE_I32,
                                                            true};
  static llvm::wasm::WasmGlobalType mutableGlobalTypeI64 = {WASM_TYPE_I64,
                                                            true};
  WasmSym::callCtors = symtab->addSyntheticFunction(
      "__wasm_call_ctors", WASM_SYMBOL_VISIBILITY_HIDDEN,
      make<SyntheticFunction>(nullSignature, "__wasm_call_ctors"));

  if (config->isPic) {
    WasmSym::stackPointer =
        createUndefinedGlobal("__stack_pointer", config->is64.getValueOr(false)
                                                     ? &mutableGlobalTypeI64
                                                     : &mutableGlobalTypeI32);
    // For PIC code, we import two global variables (__memory_base and
    // __table_base) from the environment and use these as the offset at
    // which to load our static data and function table.
    // See:
    // https://github.com/WebAssembly/tool-conventions/blob/master/DynamicLinking.md
    WasmSym::memoryBase = createUndefinedGlobal(
        "__memory_base",
        config->is64.getValueOr(false) ? &globalTypeI64 : &globalTypeI32);
    WasmSym::tableBase = createUndefinedGlobal("__table_base", &globalTypeI32);
    WasmSym::memoryBase->markLive();
    WasmSym::tableBase->markLive();
  } else {
    // For non-PIC code
    WasmSym::stackPointer = createGlobalVariable("__stack_pointer", true);
    WasmSym::stackPointer->markLive();
  }

  if (config->sharedMemory && !config->relocatable) {
    WasmSym::tlsBase = createGlobalVariable("__tls_base", true);
    WasmSym::tlsSize = createGlobalVariable("__tls_size", false);
    WasmSym::tlsAlign = createGlobalVariable("__tls_align", false);
    WasmSym::initTLS = symtab->addSyntheticFunction(
        "__wasm_init_tls", WASM_SYMBOL_VISIBILITY_HIDDEN,
        make<SyntheticFunction>(
            config->is64.getValueOr(false) ? i64ArgSignature : i32ArgSignature,
            "__wasm_init_tls"));
  }
}

static void createOptionalSymbols() {
  if (config->relocatable)
    return;

  WasmSym::dsoHandle = symtab->addOptionalDataSymbol("__dso_handle");

  if (!config->shared)
    WasmSym::dataEnd = symtab->addOptionalDataSymbol("__data_end");

  if (!config->isPic) {
    WasmSym::globalBase = symtab->addOptionalDataSymbol("__global_base");
    WasmSym::heapBase = symtab->addOptionalDataSymbol("__heap_base");
    WasmSym::definedMemoryBase = symtab->addOptionalDataSymbol("__memory_base");
    WasmSym::definedTableBase = symtab->addOptionalDataSymbol("__table_base");
  }

  // For non-shared memory programs we still need to define __tls_base since we
  // allow object files built with TLS to be linked into single threaded
  // programs, and such object files can contains refernced to this symbol.
  //
  // However, in this case __tls_base is immutable and points directly to the
  // start of the `.tdata` static segment.
  //
  // __tls_size and __tls_align are not needed in this case since they are only
  // needed for __wasm_init_tls (which we do not create in this case).
  if (!config->sharedMemory)
    WasmSym::tlsBase = createOptionalGlobal("__tls_base", false);
}

// Reconstructs command line arguments so that so that you can re-run
// the same command with the same inputs. This is for --reproduce.
static std::string createResponseFile(const opt::InputArgList &args) {
  SmallString<0> data;
  raw_svector_ostream os(data);

  // Copy the command line to the output while rewriting paths.
  for (auto *arg : args) {
    switch (arg->getOption().getID()) {
    case OPT_reproduce:
      break;
    case OPT_INPUT:
      os << quote(relativeToRoot(arg->getValue())) << "\n";
      break;
    case OPT_o:
      // If -o path contains directories, "lld @response.txt" will likely
      // fail because the archive we are creating doesn't contain empty
      // directories for the output path (-o doesn't create directories).
      // Strip directories to prevent the issue.
      os << "-o " << quote(sys::path::filename(arg->getValue())) << "\n";
      break;
    default:
      os << toString(*arg) << "\n";
    }
  }
  return std::string(data.str());
}

// The --wrap option is a feature to rename symbols so that you can write
// wrappers for existing functions. If you pass `-wrap=foo`, all
// occurrences of symbol `foo` are resolved to `wrap_foo` (so, you are
// expected to write `wrap_foo` function as a wrapper). The original
// symbol becomes accessible as `real_foo`, so you can call that from your
// wrapper.
//
// This data structure is instantiated for each -wrap option.
struct WrappedSymbol {
  Symbol *sym;
  Symbol *real;
  Symbol *wrap;
};

static Symbol *addUndefined(StringRef name) {
  return symtab->addUndefinedFunction(name, None, None, WASM_SYMBOL_UNDEFINED,
                                      nullptr, nullptr, false);
}

// Handles -wrap option.
//
// This function instantiates wrapper symbols. At this point, they seem
// like they are not being used at all, so we explicitly set some flags so
// that LTO won't eliminate them.
static std::vector<WrappedSymbol> addWrappedSymbols(opt::InputArgList &args) {
  std::vector<WrappedSymbol> v;
  DenseSet<StringRef> seen;

  for (auto *arg : args.filtered(OPT_wrap)) {
    StringRef name = arg->getValue();
    if (!seen.insert(name).second)
      continue;

    Symbol *sym = symtab->find(name);
    if (!sym)
      continue;

    Symbol *real = addUndefined(saver.save("__real_" + name));
    Symbol *wrap = addUndefined(saver.save("__wrap_" + name));
    v.push_back({sym, real, wrap});

    // We want to tell LTO not to inline symbols to be overwritten
    // because LTO doesn't know the final symbol contents after renaming.
    real->canInline = false;
    sym->canInline = false;

    // Tell LTO not to eliminate these symbols.
    sym->isUsedInRegularObj = true;
    wrap->isUsedInRegularObj = true;
    real->isUsedInRegularObj = false;
  }
  return v;
}

// Do renaming for -wrap by updating pointers to symbols.
//
// When this function is executed, only InputFiles and symbol table
// contain pointers to symbol objects. We visit them to replace pointers,
// so that wrapped symbols are swapped as instructed by the command line.
static void wrapSymbols(ArrayRef<WrappedSymbol> wrapped) {
  DenseMap<Symbol *, Symbol *> map;
  for (const WrappedSymbol &w : wrapped) {
    map[w.sym] = w.wrap;
    map[w.real] = w.sym;
  }

  // Update pointers in input files.
  parallelForEach(symtab->objectFiles, [&](InputFile *file) {
    MutableArrayRef<Symbol *> syms = file->getMutableSymbols();
    for (size_t i = 0, e = syms.size(); i != e; ++i)
      if (Symbol *s = map.lookup(syms[i]))
        syms[i] = s;
  });

  // Update pointers in the symbol table.
  for (const WrappedSymbol &w : wrapped)
    symtab->wrap(w.sym, w.real, w.wrap);
}

static TableSymbol *createDefinedIndirectFunctionTable(StringRef name) {
  const uint32_t invalidIndex = -1;
  WasmLimits limits{0, 0, 0}; // Set by the writer.
  WasmTableType type{uint8_t(ValType::FUNCREF), limits};
  WasmTable desc{invalidIndex, type, name};
  InputTable *table = make<InputTable>(desc, nullptr);
  uint32_t flags = config->exportTable ? 0 : WASM_SYMBOL_VISIBILITY_HIDDEN;
  TableSymbol *sym = symtab->addSyntheticTable(name, flags, table);
  sym->markLive();
  sym->forceExport = config->exportTable;
  return sym;
}

static TableSymbol *createUndefinedIndirectFunctionTable(StringRef name) {
  WasmLimits limits{0, 0, 0}; // Set by the writer.
  WasmTableType *type = make<WasmTableType>();
  type->ElemType = uint8_t(ValType::FUNCREF);
  type->Limits = limits;
  StringRef module(defaultModule);
  uint32_t flags = config->exportTable ? 0 : WASM_SYMBOL_VISIBILITY_HIDDEN;
  flags |= WASM_SYMBOL_UNDEFINED;
  Symbol *sym =
      symtab->addUndefinedTable(name, name, module, flags, nullptr, type);
  sym->markLive();
  sym->forceExport = config->exportTable;
  return cast<TableSymbol>(sym);
}

static TableSymbol *resolveIndirectFunctionTable() {
  Symbol *existingTable = symtab->find(functionTableName);
  if (existingTable) {
    if (!isa<TableSymbol>(existingTable)) {
      error(Twine("reserved symbol must be of type table: `") +
            functionTableName + "`");
      return nullptr;
    }
    if (existingTable->isDefined()) {
      error(Twine("reserved symbol must not be defined in input files: `") +
            functionTableName + "`");
      return nullptr;
    }
  }

  if (config->importTable) {
    if (existingTable)
      return cast<TableSymbol>(existingTable);
    else
      return createUndefinedIndirectFunctionTable(functionTableName);
  } else if ((existingTable && existingTable->isLive()) ||
             config->exportTable) {
    // A defined table is required.  Either because the user request an exported
    // table or because the table symbol is already live.  The existing table is
    // guaranteed to be undefined due to the check above.
    return createDefinedIndirectFunctionTable(functionTableName);
  }

  // An indirect function table will only be present in the symbol table if
  // needed by a reloc; if we get here, we don't need one.
  return nullptr;
}

void LinkerDriver::linkerMain(ArrayRef<const char *> argsArr) {
  WasmOptTable parser;
  opt::InputArgList args = parser.parse(argsArr.slice(1));

  // Handle --help
  if (args.hasArg(OPT_help)) {
    parser.PrintHelp(lld::outs(),
                     (std::string(argsArr[0]) + " [options] file...").c_str(),
                     "LLVM Linker", false);
    return;
  }

  // Handle --version
  if (args.hasArg(OPT_version) || args.hasArg(OPT_v)) {
    lld::outs() << getLLDVersion() << "\n";
    return;
  }

  // Handle --reproduce
  if (auto *arg = args.getLastArg(OPT_reproduce)) {
    StringRef path = arg->getValue();
    Expected<std::unique_ptr<TarWriter>> errOrWriter =
        TarWriter::create(path, path::stem(path));
    if (errOrWriter) {
      tar = std::move(*errOrWriter);
      tar->append("response.txt", createResponseFile(args));
      tar->append("version.txt", getLLDVersion() + "\n");
    } else {
      error("--reproduce: " + toString(errOrWriter.takeError()));
    }
  }

  // Parse and evaluate -mllvm options.
  std::vector<const char *> v;
  v.push_back("wasm-ld (LLVM option parsing)");
  for (auto *arg : args.filtered(OPT_mllvm))
    v.push_back(arg->getValue());
  cl::ResetAllOptionOccurrences();
  cl::ParseCommandLineOptions(v.size(), v.data());

  errorHandler().errorLimit = args::getInteger(args, OPT_error_limit, 20);

  readConfigs(args);

  createFiles(args);
  if (errorCount())
    return;

  setConfigs();
  checkOptions(args);
  if (errorCount())
    return;

  if (auto *arg = args.getLastArg(OPT_allow_undefined_file))
    readImportFile(arg->getValue());

  // Fail early if the output file or map file is not writable. If a user has a
  // long link, e.g. due to a large LTO link, they do not wish to run it and
  // find that it failed because there was a mistake in their command-line.
  if (auto e = tryCreateFile(config->outputFile))
    error("cannot open output file " + config->outputFile + ": " + e.message());
  if (auto e = tryCreateFile(config->mapFile))
    error("cannot open map file " + config->mapFile + ": " + e.message());
  if (errorCount())
    return;

  // Handle --trace-symbol.
  for (auto *arg : args.filtered(OPT_trace_symbol))
    symtab->trace(arg->getValue());

  for (auto *arg : args.filtered(OPT_export))
    config->exportedSymbols.insert(arg->getValue());

  createSyntheticSymbols();

  // Add all files to the symbol table. This will add almost all
  // symbols that we need to the symbol table.
  for (InputFile *f : files)
    symtab->addFile(f);
  if (errorCount())
    return;

  // Handle the `--undefined <sym>` options.
  for (auto *arg : args.filtered(OPT_undefined))
    handleUndefined(arg->getValue());

  // Handle the `--export <sym>` options
  // This works like --undefined but also exports the symbol if its found
  for (auto *arg : args.filtered(OPT_export))
    handleUndefined(arg->getValue());

  Symbol *entrySym = nullptr;
  if (!config->relocatable && !config->entry.empty()) {
    entrySym = handleUndefined(config->entry);
    if (entrySym && entrySym->isDefined())
      entrySym->forceExport = true;
    else
      error("entry symbol not defined (pass --no-entry to suppress): " +
            config->entry);
  }

  // If the user code defines a `__wasm_call_dtors` function, remember it so
  // that we can call it from the command export wrappers. Unlike
  // `__wasm_call_ctors` which we synthesize, `__wasm_call_dtors` is defined
  // by libc/etc., because destructors are registered dynamically with
  // `__cxa_atexit` and friends.
  if (!config->relocatable && !config->shared &&
      !WasmSym::callCtors->isUsedInRegularObj &&
      WasmSym::callCtors->getName() != config->entry &&
      !config->exportedSymbols.count(WasmSym::callCtors->getName())) {
    if (Symbol *callDtors = handleUndefined("__wasm_call_dtors")) {
      if (auto *callDtorsFunc = dyn_cast<DefinedFunction>(callDtors)) {
        if (callDtorsFunc->signature &&
            (!callDtorsFunc->signature->Params.empty() ||
             !callDtorsFunc->signature->Returns.empty())) {
          error("__wasm_call_dtors must have no argument or return values");
        }
        WasmSym::callDtors = callDtorsFunc;
      } else {
        error("__wasm_call_dtors must be a function");
      }
    }
  }

  createOptionalSymbols();

  if (errorCount())
    return;

  // Create wrapped symbols for -wrap option.
  std::vector<WrappedSymbol> wrapped = addWrappedSymbols(args);

  // If any of our inputs are bitcode files, the LTO code generator may create
  // references to certain library functions that might not be explicit in the
  // bitcode file's symbol table. If any of those library functions are defined
  // in a bitcode file in an archive member, we need to arrange to use LTO to
  // compile those archive members by adding them to the link beforehand.
  //
  // We only need to add libcall symbols to the link before LTO if the symbol's
  // definition is in bitcode. Any other required libcall symbols will be added
  // to the link after LTO when we add the LTO object file to the link.
  if (!symtab->bitcodeFiles.empty())
    for (auto *s : lto::LTO::getRuntimeLibcallSymbols())
      handleLibcall(s);
  if (errorCount())
    return;

  // Do link-time optimization if given files are LLVM bitcode files.
  // This compiles bitcode files into real object files.
  symtab->addCombinedLTOObject();
  if (errorCount())
    return;

  // Resolve any variant symbols that were created due to signature
  // mismatchs.
  symtab->handleSymbolVariants();
  if (errorCount())
    return;

  // Apply symbol renames for -wrap.
  if (!wrapped.empty())
    wrapSymbols(wrapped);

  for (auto *arg : args.filtered(OPT_export)) {
    Symbol *sym = symtab->find(arg->getValue());
    if (sym && sym->isDefined())
      sym->forceExport = true;
    else if (config->unresolvedSymbols == UnresolvedPolicy::ReportError)
      error(Twine("symbol exported via --export not found: ") +
            arg->getValue());
    else if (config->unresolvedSymbols == UnresolvedPolicy::Warn)
      warn(Twine("symbol exported via --export not found: ") + arg->getValue());
  }

  if (!config->relocatable && !config->isPic) {
    // Add synthetic dummies for weak undefined functions.  Must happen
    // after LTO otherwise functions may not yet have signatures.
    symtab->handleWeakUndefines();
  }

  if (entrySym)
    entrySym->setHidden(false);

  if (errorCount())
    return;

  // Do size optimizations: garbage collection
  markLive();

  if (!config->relocatable) {
    // Provide the indirect funciton table if needed.
    WasmSym::indirectFunctionTable = resolveIndirectFunctionTable();

    if (errorCount())
      return;
  }

  // Write the result to the file.
  writeResult();
}

} // namespace wasm
} // namespace lld
