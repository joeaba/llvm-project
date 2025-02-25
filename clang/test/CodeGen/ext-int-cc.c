// RUN: %clang_cc1 -triple x86_64-gnu-linux -O3 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefixes=LIN64
// RUN: %clang_cc1 -triple x86_64-windows-pc -O3 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefixes=WIN64
// RUN: %clang_cc1 -triple i386-gnu-linux -O3 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefixes=LIN32
// RUN: %clang_cc1 -triple i386-windows-pc -O3 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefixes=WIN32
// RUN: %clang_cc1 -triple le32-nacl -O3 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefixes=NACL
// RUN: %clang_cc1 -triple nvptx64 -O3 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefixes=NVPTX64
// RUN: %clang_cc1 -triple nvptx -O3 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefixes=NVPTX
// RUN: %clang_cc1 -triple sparcv9 -O3 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefixes=SPARCV9
// RUN: %clang_cc1 -triple sparc -O3 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefixes=SPARC
// RUN: %clang_cc1 -triple mips64 -O3 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefixes=MIPS64
// RUN: %clang_cc1 -triple mips -O3 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefixes=MIPS
// RUN: %clang_cc1 -triple spir64 -O3 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefixes=SPIR64
// RUN: %clang_cc1 -triple spir -O3 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefixes=SPIR
// RUN: %clang_cc1 -triple hexagon -O3 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefixes=HEX
// RUN: %clang_cc1 -triple lanai -O3 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefixes=LANAI
// RUN: %clang_cc1 -triple r600 -O3 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefixes=R600
// RUN: %clang_cc1 -triple arc -O3 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefixes=ARC
// RUN: %clang_cc1 -triple xcore -O3 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefixes=XCORE
// RUN: %clang_cc1 -triple riscv64 -O3 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefixes=RISCV64
// RUN: %clang_cc1 -triple riscv32 -O3 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefixes=RISCV32
// RUN: %clang_cc1 -triple wasm64 -O3 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefixes=WASM
// RUN: %clang_cc1 -triple wasm32 -O3 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefixes=WASM
// RUN: %clang_cc1 -triple systemz -O3 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefixes=SYSTEMZ
// RUN: %clang_cc1 -triple ppc64 -O3 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefixes=PPC64
// RUN: %clang_cc1 -triple ppc -O3 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefixes=PPC32
// RUN: %clang_cc1 -triple aarch64 -O3 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefixes=AARCH64
// RUN: %clang_cc1 -triple aarch64 -target-abi darwinpcs -O3 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefixes=AARCH64DARWIN
// RUN: %clang_cc1 -triple arm64_32-apple-ios -O3 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefixes=AARCH64
// RUN: %clang_cc1 -triple arm64_32-apple-ios -target-abi darwinpcs -O3 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefixes=AARCH64DARWIN
// RUN: %clang_cc1 -triple arm -O3 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefixes=ARM
// RUN: %clang_cc1 -triple bpf -target-feature +solana -O3 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefixes=BPF

// Make sure 128 and 64 bit versions are passed like integers, and that >128
// is passed indirectly.
void ParamPassing(_ExtInt(129) a, _ExtInt(128) b, _ExtInt(64) c) {}
// LIN64: define{{.*}} void @ParamPassing(i129* byval(i129) align 8 %{{.+}}, i64 %{{.+}}, i64 %{{.+}}, i64 %{{.+}})
// WIN64: define dso_local void @ParamPassing(i129* %{{.+}}, i128* %{{.+}}, i64 %{{.+}})
// LIN32: define{{.*}} void @ParamPassing(i129* %{{.+}}, i128* %{{.+}}, i64 %{{.+}})
// WIN32: define dso_local void @ParamPassing(i129* %{{.+}}, i128* %{{.+}}, i64 %{{.+}})
// NACL: define{{.*}} void @ParamPassing(i129* byval(i129) align 8 %{{.+}}, i128* byval(i128) align 8 %{{.+}}, i64 %{{.+}})
// NVPTX64: define{{.*}} void @ParamPassing(i129* byval(i129) align 8 %{{.+}}, i128 %{{.+}}, i64 %{{.+}})
// NVPTX: define{{.*}} void @ParamPassing(i129* byval(i129) align 8 %{{.+}}, i128* byval(i128) align 8 %{{.+}}, i64 %{{.+}})
// SPARCV9: define{{.*}} void @ParamPassing(i129* %{{.+}}, i128 %{{.+}}, i64 %{{.+}})
// SPARC: define{{.*}} void @ParamPassing(i129* byval(i129) align 8 %{{.+}}, i128* byval(i128) align 8 %{{.+}}, i64 %{{.+}})
// MIPS64: define{{.*}} void @ParamPassing(i129* byval(i129) align 8 %{{.+}}, i128 signext  %{{.+}}, i64 signext %{{.+}})
// MIPS: define{{.*}} void @ParamPassing(i129* byval(i129) align 8 %{{.+}}, i128* byval(i128) align 8 %{{.+}}, i64 signext %{{.+}})
// SPIR64: define{{.*}} spir_func void @ParamPassing(i129* byval(i129) align 8 %{{.+}}, i128* byval(i128) align 8 %{{.+}}, i64 %{{.+}})
// SPIR: define{{.*}} spir_func void @ParamPassing(i129* byval(i129) align 8 %{{.+}}, i128* byval(i128) align 8 %{{.+}}, i64 %{{.+}})
// HEX: define{{.*}} void @ParamPassing(i129* byval(i129) align 8 %{{.+}}, i128* byval(i128) align 8 %{{.+}}, i64 %{{.+}})
// LANAI: define{{.*}} void @ParamPassing(i129* byval(i129) align 4 %{{.+}}, i128* byval(i128) align 4 %{{.+}}, i64 %{{.+}})
// R600: define{{.*}} void @ParamPassing(i129 addrspace(5)* byval(i129) align 8 %{{.+}}, i128 addrspace(5)* byval(i128) align 8 %{{.+}}, i64 %{{.+}})
// ARC: define{{.*}} void @ParamPassing(i129* byval(i129) align 4 %{{.+}}, i128* byval(i128) align 4 %{{.+}}, i64 inreg %{{.+}})
// XCORE: define{{.*}} void @ParamPassing(i129* byval(i129) align 4 %{{.+}}, i128* byval(i128) align 4 %{{.+}}, i64 %{{.+}})
// RISCV64: define{{.*}} void @ParamPassing(i129* %{{.+}}, i128 %{{.+}}, i64 %{{.+}})
// RISCV32: define{{.*}} void @ParamPassing(i129* %{{.+}}, i128* %{{.+}}, i64 %{{.+}})
// WASM: define{{.*}} void @ParamPassing(i129* byval(i129) align 8 %{{.+}}, i128 %{{.+}}, i64 %{{.+}})
// SYSTEMZ: define{{.*}} void @ParamPassing(i129* %{{.+}}, i128* %{{.+}}, i64 %{{.+}})
// PPC64: define{{.*}} void @ParamPassing(i129* byval(i129) align 8 %{{.+}}, i128 %{{.+}}, i64 %{{.+}})
// PPC32: define{{.*}} void @ParamPassing(i129* byval(i129) align 8 %{{.+}}, i128* byval(i128) align 8 %{{.+}}, i64 %{{.+}})
// AARCH64: define{{.*}} void @ParamPassing(i129* byval(i129) align 8 %{{.+}}, i128 %{{.+}}, i64 %{{.+}})
// AARCH64DARWIN: define{{.*}} void @ParamPassing(i129* byval(i129) align 8 %{{.+}}, i128 %{{.+}}, i64 %{{.+}})
// ARM: define{{.*}} arm_aapcscc void @ParamPassing(i129* byval(i129) align 8 %{{.+}}, i128* byval(i128) align 8 %{{.+}}, i64 %{{.+}})
// BPF: define{{.*}} void @ParamPassing(i129* byval(i129) align 8 %{{.+}}, i128 %{{.+}}, i64 %{{.+}})

void ParamPassing2(_ExtInt(129) a, _ExtInt(127) b, _ExtInt(63) c) {}
// LIN64: define{{.*}} void @ParamPassing2(i129* byval(i129) align 8 %{{.+}}, i64 %{{.+}}, i64 %{{.+}}, i64 %{{.+}})
// WIN64: define dso_local void @ParamPassing2(i129* %{{.+}}, i127* %{{.+}}, i63 %{{.+}})
// LIN32: define{{.*}} void @ParamPassing2(i129* %{{.+}}, i127* %{{.+}}, i63 %{{.+}})
// WIN32: define dso_local void @ParamPassing2(i129* %{{.+}}, i127* %{{.+}}, i63 %{{.+}})
// NACL: define{{.*}} void @ParamPassing2(i129* byval(i129) align 8 %{{.+}}, i127* byval(i127) align 8 %{{.+}}, i63 %{{.+}})
// NVPTX64: define{{.*}} void @ParamPassing2(i129* byval(i129) align 8 %{{.+}}, i127 %{{.+}}, i63 %{{.+}})
// NVPTX: define{{.*}} void @ParamPassing2(i129* byval(i129) align 8 %{{.+}}, i127* byval(i127) align 8 %{{.+}}, i63 %{{.+}})
// SPARCV9: define{{.*}} void @ParamPassing2(i129* %{{.+}}, i127 %{{.+}}, i63 signext %{{.+}})
// SPARC: define{{.*}} void @ParamPassing2(i129* byval(i129) align 8 %{{.+}}, i127* byval(i127) align 8 %{{.+}}, i63 %{{.+}})
// MIPS64: define{{.*}} void @ParamPassing2(i129* byval(i129) align 8 %{{.+}}, i127 signext  %{{.+}}, i63 signext %{{.+}})
// MIPS: define{{.*}} void @ParamPassing2(i129* byval(i129) align 8 %{{.+}}, i127* byval(i127) align 8 %{{.+}}, i63 signext %{{.+}})
// SPIR64: define{{.*}} spir_func void @ParamPassing2(i129* byval(i129) align 8 %{{.+}}, i127* byval(i127) align 8 %{{.+}}, i63 %{{.+}})
// SPIR: define{{.*}} spir_func void @ParamPassing2(i129* byval(i129) align 8 %{{.+}}, i127* byval(i127) align 8 %{{.+}}, i63 %{{.+}})
// HEX: define{{.*}} void @ParamPassing2(i129* byval(i129) align 8 %{{.+}}, i127* byval(i127) align 8 %{{.+}}, i63 %{{.+}})
// LANAI: define{{.*}} void @ParamPassing2(i129* byval(i129) align 4 %{{.+}}, i127* byval(i127) align 4 %{{.+}}, i63 %{{.+}})
// R600: define{{.*}} void @ParamPassing2(i129 addrspace(5)* byval(i129) align 8 %{{.+}}, i127 addrspace(5)* byval(i127) align 8 %{{.+}}, i63 %{{.+}})
// ARC: define{{.*}} void @ParamPassing2(i129* byval(i129) align 4 %{{.+}}, i127* byval(i127) align 4 %{{.+}}, i63 inreg %{{.+}})
// XCORE: define{{.*}} void @ParamPassing2(i129* byval(i129) align 4 %{{.+}}, i127* byval(i127) align 4 %{{.+}}, i63 %{{.+}})
// RISCV64: define{{.*}} void @ParamPassing2(i129* %{{.+}}, i127 %{{.+}}, i63 signext %{{.+}})
// RISCV32: define{{.*}} void @ParamPassing2(i129* %{{.+}}, i127* %{{.+}}, i63 %{{.+}})
// WASM: define{{.*}} void @ParamPassing2(i129* byval(i129) align 8 %{{.+}}, i127 %{{.+}}, i63 %{{.+}})
// SYSTEMZ: define{{.*}} void @ParamPassing2(i129* %{{.+}}, i127* %{{.+}}, i63 signext %{{.+}})
// PPC64: define{{.*}} void @ParamPassing2(i129* byval(i129) align 8 %{{.+}}, i127 %{{.+}}, i63 signext %{{.+}})
// PPC32: define{{.*}} void @ParamPassing2(i129* byval(i129) align 8 %{{.+}}, i127* byval(i127) align 8 %{{.+}}, i63 %{{.+}})
// AARCH64: define{{.*}} void @ParamPassing2(i129* byval(i129) align 8 %{{.+}}, i127 %{{.+}}, i63 %{{.+}})
// AARCH64DARWIN: define{{.*}} void @ParamPassing2(i129* byval(i129) align 8 %{{.+}}, i127 %{{.+}}, i63 %{{.+}})
// ARM: define{{.*}} arm_aapcscc void @ParamPassing2(i129* byval(i129) align 8 %{{.+}}, i127* byval(i127) align 8 %{{.+}}, i63 %{{.+}})
// BPF: define{{.*}} void @ParamPassing2(i129* byval(i129) align 8 %{{.+}}, i127 %{{.+}}, i63 %{{.+}})

// Make sure we follow the signext rules for promotable integer types.
void ParamPassing3(_ExtInt(15) a, _ExtInt(31) b) {}
// LIN64: define{{.*}} void @ParamPassing3(i15 signext %{{.+}}, i31 signext %{{.+}})
// WIN64: define dso_local void @ParamPassing3(i15 %{{.+}}, i31 %{{.+}})
// LIN32: define{{.*}} void @ParamPassing3(i15 signext %{{.+}}, i31 signext %{{.+}})
// WIN32: define dso_local void @ParamPassing3(i15 signext %{{.+}}, i31 signext %{{.+}})
// NACL: define{{.*}} void @ParamPassing3(i15 %{{.+}}, i31 %{{.+}})
// NVPTX64: define{{.*}} void @ParamPassing3(i15 signext %{{.+}}, i31 signext %{{.+}})
// NVPTX: define{{.*}} void @ParamPassing3(i15 signext %{{.+}}, i31 signext %{{.+}})
// SPARCV9: define{{.*}} void @ParamPassing3(i15 signext %{{.+}}, i31 signext %{{.+}})
// SPARC: define{{.*}} void @ParamPassing3(i15 signext %{{.+}}, i31 signext %{{.+}})
// MIPS64: define{{.*}} void @ParamPassing3(i15 signext %{{.+}}, i31 signext %{{.+}})
// MIPS: define{{.*}} void @ParamPassing3(i15 signext %{{.+}}, i31 signext %{{.+}})
// SPIR64: define{{.*}} spir_func void @ParamPassing3(i15 signext %{{.+}}, i31 signext %{{.+}})
// SPIR: define{{.*}} spir_func void @ParamPassing3(i15 signext %{{.+}}, i31 signext %{{.+}})
// HEX: define{{.*}} void @ParamPassing3(i15 signext %{{.+}}, i31 signext %{{.+}})
// LANAI: define{{.*}} void @ParamPassing3(i15 inreg %{{.+}}, i31 inreg %{{.+}})
// R600: define{{.*}} void @ParamPassing3(i15 signext %{{.+}}, i31 signext %{{.+}})
// ARC: define{{.*}} void @ParamPassing3(i15 inreg signext %{{.+}}, i31 inreg signext %{{.+}})
// XCORE: define{{.*}} void @ParamPassing3(i15 signext %{{.+}}, i31 signext %{{.+}})
// RISCV64: define{{.*}} void @ParamPassing3(i15 signext %{{.+}}, i31 signext %{{.+}})
// RISCV32: define{{.*}} void @ParamPassing3(i15 signext %{{.+}}, i31 signext %{{.+}})
// WASM: define{{.*}} void @ParamPassing3(i15 signext %{{.+}}, i31 signext %{{.+}})
// SYSTEMZ: define{{.*}} void @ParamPassing3(i15 signext %{{.+}}, i31 signext %{{.+}})
// PPC64: define{{.*}} void @ParamPassing3(i15 signext %{{.+}}, i31 signext %{{.+}})
// PPC32: define{{.*}} void @ParamPassing3(i15 signext %{{.+}}, i31 signext %{{.+}})
// AARCH64: define{{.*}} void @ParamPassing3(i15 %{{.+}}, i31 %{{.+}})
// AARCH64DARWIN: define{{.*}} void @ParamPassing3(i15 signext %{{.+}}, i31 signext %{{.+}})
// ARM: define{{.*}} arm_aapcscc void @ParamPassing3(i15 signext %{{.+}}, i31 signext %{{.+}})
// BPF: define{{.*}} void @ParamPassing3(i15 signext %{{.+}}, i31 signext %{{.+}})

_ExtInt(63) ReturnPassing(){}
// LIN64: define{{.*}} i64 @ReturnPassing(
// WIN64: define dso_local i63 @ReturnPassing(
// LIN32: define{{.*}} i63 @ReturnPassing(
// WIN32: define dso_local i63 @ReturnPassing(
// NACL: define{{.*}} i63 @ReturnPassing(
// NVPTX64: define{{.*}} i63 @ReturnPassing(
// NVPTX: define{{.*}} i63 @ReturnPassing(
// SPARCV9: define{{.*}} signext i63 @ReturnPassing(
// SPARC: define{{.*}} i63 @ReturnPassing(
// MIPS64: define{{.*}} i63 @ReturnPassing(
// MIPS: define{{.*}} i63 @ReturnPassing(
// SPIR64: define{{.*}} spir_func i63 @ReturnPassing(
// SPIR: define{{.*}} spir_func i63 @ReturnPassing(
// HEX: define{{.*}} i63 @ReturnPassing(
// LANAI: define{{.*}} i63 @ReturnPassing(
// R600: define{{.*}} i63 @ReturnPassing(
// ARC: define{{.*}} i63 @ReturnPassing(
// XCORE: define{{.*}} i63 @ReturnPassing(
// RISCV64: define{{.*}} signext i63 @ReturnPassing(
// RISCV32: define{{.*}} i63 @ReturnPassing(
// WASM: define{{.*}} i63 @ReturnPassing(
// SYSTEMZ: define{{.*}} signext i63 @ReturnPassing(
// PPC64: define{{.*}} signext i63 @ReturnPassing(
// PPC32: define{{.*}} i63 @ReturnPassing(
// AARCH64: define{{.*}} i63 @ReturnPassing(
// AARCH64DARWIN: define{{.*}} i63 @ReturnPassing(
// ARM: define{{.*}} arm_aapcscc i63 @ReturnPassing(
// BPF: define{{.*}} i63 @ReturnPassing(

_ExtInt(64) ReturnPassing2(){}
// LIN64: define{{.*}} i64 @ReturnPassing2(
// WIN64: define dso_local i64 @ReturnPassing2(
// LIN32: define{{.*}} i64 @ReturnPassing2(
// WIN32: define dso_local i64 @ReturnPassing2(
// NACL: define{{.*}} i64 @ReturnPassing2(
// NVPTX64: define{{.*}} i64 @ReturnPassing2(
// NVPTX: define{{.*}} i64 @ReturnPassing2(
// SPARCV9: define{{.*}} i64 @ReturnPassing2(
// SPARC: define{{.*}} i64 @ReturnPassing2(
// MIPS64: define{{.*}} i64 @ReturnPassing2(
// MIPS: define{{.*}} i64 @ReturnPassing2(
// SPIR64: define{{.*}} spir_func i64 @ReturnPassing2(
// SPIR: define{{.*}} spir_func i64 @ReturnPassing2(
// HEX: define{{.*}} i64 @ReturnPassing2(
// LANAI: define{{.*}} i64 @ReturnPassing2(
// R600: define{{.*}} i64 @ReturnPassing2(
// ARC: define{{.*}} i64 @ReturnPassing2(
// XCORE: define{{.*}} i64 @ReturnPassing2(
// RISCV64: define{{.*}} i64 @ReturnPassing2(
// RISCV32: define{{.*}} i64 @ReturnPassing2(
// WASM: define{{.*}} i64 @ReturnPassing2(
// SYSTEMZ: define{{.*}} i64 @ReturnPassing2(
// PPC64: define{{.*}} i64 @ReturnPassing2(
// PPC32: define{{.*}} i64 @ReturnPassing2(
// AARCH64: define{{.*}} i64 @ReturnPassing2(
// AARCH64DARWIN: define{{.*}} i64 @ReturnPassing2(
// ARM: define{{.*}} arm_aapcscc i64 @ReturnPassing2(
// BPF: define{{.*}} i64 @ReturnPassing2(

_ExtInt(127) ReturnPassing3(){}
// LIN64: define{{.*}} { i64, i64 } @ReturnPassing3(
// WIN64: define dso_local void @ReturnPassing3(i127* noalias sret
// LIN32: define{{.*}} void @ReturnPassing3(i127* noalias sret
// WIN32: define dso_local void @ReturnPassing3(i127* noalias sret
// NACL: define{{.*}} void @ReturnPassing3(i127* noalias sret
// NVPTX/64 makes the intentional choice to put all return values direct, even
// large structures, so we do the same here.
// NVPTX64: define{{.*}} i127 @ReturnPassing3(
// NVPTX: define{{.*}} i127 @ReturnPassing3(
// SPARCV9: define{{.*}} i127 @ReturnPassing3(
// SPARC: define{{.*}} void @ReturnPassing3(i127* noalias sret
// MIPS64: define{{.*}} i127 @ReturnPassing3(
// MIPS: define{{.*}} void @ReturnPassing3(i127* noalias sret
// SPIR64: define{{.*}} spir_func void @ReturnPassing3(i127* noalias sret
// SPIR: define{{.*}} spir_func void @ReturnPassing3(i127* noalias sret
// HEX: define{{.*}} void @ReturnPassing3(i127* noalias sret
// LANAI: define{{.*}} void @ReturnPassing3(i127* noalias sret
// R600: define{{.*}} void @ReturnPassing3(i127 addrspace(5)* noalias sret
// ARC: define{{.*}} void @ReturnPassing3(i127* noalias sret
// XCORE: define{{.*}} void @ReturnPassing3(i127* noalias sret
// RISCV64: define{{.*}} i127 @ReturnPassing3(
// RISCV32: define{{.*}} void @ReturnPassing3(i127* noalias sret
// WASM: define{{.*}} i127 @ReturnPassing3(
// SYSTEMZ: define{{.*}} void @ReturnPassing3(i127* noalias sret
// PPC64: define{{.*}} i127 @ReturnPassing3(
// PPC32: define{{.*}} void @ReturnPassing3(i127* noalias sret
// AARCH64: define{{.*}} i127 @ReturnPassing3(
// AARCH64DARWIN: define{{.*}} i127 @ReturnPassing3(
// ARM: define{{.*}} arm_aapcscc void @ReturnPassing3(i127* noalias sret
// BPF: define{{.*}} i127 @ReturnPassing3(

_ExtInt(128) ReturnPassing4(){}
// LIN64: define{{.*}} { i64, i64 } @ReturnPassing4(
// WIN64: define dso_local void @ReturnPassing4(i128* noalias sret
// LIN32: define{{.*}} void @ReturnPassing4(i128* noalias sret
// WIN32: define dso_local void @ReturnPassing4(i128* noalias sret
// NACL: define{{.*}} void @ReturnPassing4(i128* noalias sret
// NVPTX64: define{{.*}} i128 @ReturnPassing4(
// NVPTX: define{{.*}} i128 @ReturnPassing4(
// SPARCV9: define{{.*}} i128 @ReturnPassing4(
// SPARC: define{{.*}} void @ReturnPassing4(i128* noalias sret
// MIPS64: define{{.*}} i128 @ReturnPassing4(
// MIPS: define{{.*}} void @ReturnPassing4(i128* noalias sret
// SPIR64: define{{.*}} spir_func void @ReturnPassing4(i128* noalias sret
// SPIR: define{{.*}} spir_func void @ReturnPassing4(i128* noalias sret
// HEX: define{{.*}} void @ReturnPassing4(i128* noalias sret
// LANAI: define{{.*}} void @ReturnPassing4(i128* noalias sret
// R600: define{{.*}} void @ReturnPassing4(i128 addrspace(5)* noalias sret
// ARC: define{{.*}} void @ReturnPassing4(i128* noalias sret
// XCORE: define{{.*}} void @ReturnPassing4(i128* noalias sret
// RISCV64: define{{.*}} i128 @ReturnPassing4(
// RISCV32: define{{.*}} void @ReturnPassing4(i128* noalias sret
// WASM: define{{.*}} i128 @ReturnPassing4(
// SYSTEMZ: define{{.*}} void @ReturnPassing4(i128* noalias sret
// PPC64: define{{.*}} i128 @ReturnPassing4(
// PPC32: define{{.*}} void @ReturnPassing4(i128* noalias sret
// AARCH64: define{{.*}} i128 @ReturnPassing4(
// AARCH64DARWIN: define{{.*}} i128 @ReturnPassing4(
// ARM: define{{.*}} arm_aapcscc void @ReturnPassing4(i128* noalias sret
// BPF: define{{.*}} i128 @ReturnPassing4(

_ExtInt(129) ReturnPassing5(){}
// LIN64: define{{.*}} void @ReturnPassing5(i129* noalias sret
// WIN64: define dso_local void @ReturnPassing5(i129* noalias sret
// LIN32: define{{.*}} void @ReturnPassing5(i129* noalias sret
// WIN32: define dso_local void @ReturnPassing5(i129* noalias sret
// NACL: define{{.*}} void @ReturnPassing5(i129* noalias sret
// NVPTX64: define{{.*}} i129 @ReturnPassing5(
// NVPTX: define{{.*}} i129 @ReturnPassing5(
// SPARCV9: define{{.*}} i129 @ReturnPassing5(
// SPARC: define{{.*}} void @ReturnPassing5(i129* noalias sret
// MIPS64: define{{.*}} void @ReturnPassing5(i129* noalias sret
// MIPS: define{{.*}} void @ReturnPassing5(i129* noalias sret
// SPIR64: define{{.*}} spir_func void @ReturnPassing5(i129* noalias sret
// SPIR: define{{.*}} spir_func void @ReturnPassing5(i129* noalias sret
// HEX: define{{.*}} void @ReturnPassing5(i129* noalias sret
// LANAI: define{{.*}} void @ReturnPassing5(i129* noalias sret
// R600: define{{.*}} void @ReturnPassing5(i129 addrspace(5)* noalias sret
// ARC: define{{.*}} void @ReturnPassing5(i129* inreg noalias sret
// XCORE: define{{.*}} void @ReturnPassing5(i129* noalias sret
// RISCV64: define{{.*}} void @ReturnPassing5(i129* noalias sret
// RISCV32: define{{.*}} void @ReturnPassing5(i129* noalias sret
// WASM: define{{.*}} void @ReturnPassing5(i129* noalias sret
// SYSTEMZ: define{{.*}} void @ReturnPassing5(i129* noalias sret
// PPC64: define{{.*}} void @ReturnPassing5(i129* noalias sret
// PPC32: define{{.*}} void @ReturnPassing5(i129* noalias sret
// AARCH64: define{{.*}} void @ReturnPassing5(i129* noalias sret
// AARCH64DARWIN: define{{.*}} void @ReturnPassing5(i129* noalias sret
// ARM: define{{.*}} arm_aapcscc void @ReturnPassing5(i129* noalias sret
// BPF: define{{.*}} void @ReturnPassing5(i129* noalias sret

// SparcV9 is odd in that it has a return-size limit of 256, not 128 or 64
// like other platforms, so test to make sure this behavior will still work.
_ExtInt(256) ReturnPassing6() {}
// SPARCV9: define{{.*}} i256 @ReturnPassing6(
_ExtInt(257) ReturnPassing7() {}
// SPARCV9: define{{.*}} void @ReturnPassing7(i257* noalias sret
