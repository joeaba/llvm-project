; RUN: llc < %s | FileCheck %s


target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

declare void @llvm.memset.p0i8.i64(i8* nocapture writeonly, i8, i64, i1 immarg);

define void @foo() local_unnamed_addr #0 {
; CHECK-LABEL: foo:
; CHECK:       # %bb.0:
; CHECK-NEXT:    subq $4096, %rsp # imm = 0x1000
; CHECK-NEXT:    .cfi_adjust_cfa_offset 4096
; CHECK-NEXT:    movq $0, (%rsp)
; CHECK-NEXT:    subq $3912, %rsp # imm = 0xF48
; CHECK-NEXT:    .cfi_def_cfa_offset 8016
; CHECK-NEXT:    movq %rsp, %rdi
; CHECK-NEXT:    movl $8000, %edx # imm = 0x1F40
; CHECK-NEXT:    xorl %esi, %esi
; CHECK-NEXT:    callq memset@PLT
; CHECK-NEXT:    addq $8008, %rsp # imm = 0x1F48
; CHECK-NEXT:    .cfi_def_cfa_offset 8
; CHECK-NEXT:    retq
  %a = alloca i8, i64 8000, align 16
  call void @llvm.memset.p0i8.i64(i8* align 16 %a, i8 0, i64 8000, i1 false)
  ret void
}

attributes #0 =  {"probe-stack"="inline-asm"}
