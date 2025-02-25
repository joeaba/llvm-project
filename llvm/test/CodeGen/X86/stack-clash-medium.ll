; RUN: llc -mtriple=x86_64-linux-android < %s | FileCheck -check-prefix=CHECK-X86-64 %s
; RUN: llc -mtriple=i686-linux-android < %s | FileCheck -check-prefix=CHECK-X86-32 %s

define i32 @foo() local_unnamed_addr #0 {
  %a = alloca i32, i64 2000, align 16
  %b = getelementptr inbounds i32, i32* %a, i64 200
  store volatile i32 1, i32* %b
  %c = load volatile i32, i32* %a
  ret i32 %c
}

attributes #0 =  {"probe-stack"="inline-asm"}

; CHECK-X86-64-LABEL: foo:
; CHECK-X86-64:      # %bb.0:
; CHECK-X86-64-NEXT: subq	$4096, %rsp             # imm = 0x1000
; CHECK-X86-64-NEXT: .cfi_adjust_cfa_offset 4096
; CHECK-X86-64-NEXT: movq	$0, (%rsp)
; CHECK-X86-64-NEXT: subq	$3784, %rsp             # imm = 0xEC8
; CHECK-X86-64-NEXT: .cfi_def_cfa_offset 7888
; CHECK-X86-64-NEXT: movl	$1, 672(%rsp)
; CHECK-X86-64-NEXT: movl	-128(%rsp), %eax
; CHECK-X86-64-NEXT: addq	$7880, %rsp             # imm = 0x1EC8
; CHECK-X86-64-NEXT: .cfi_def_cfa_offset 8
; CHECK-X86-64-NEXT: retq

; CHECK-X86-32-LABEL: foo:{{.*}}
; CHECK-X86-32:      # %bb.0:
; CHECK-X86-32-NEXT: subl	$4096, %esp # imm = 0x1000
; CHECK-X86-32-NEXT: .cfi_adjust_cfa_offset 4096
; CHECK-X86-32-NEXT: movl	$0, (%esp)
; CHECK-X86-32-NEXT: subl	$3916, %esp # imm = 0xF4C
; CHECK-X86-32-NEXT: .cfi_def_cfa_offset 8016
; CHECK-X86-32-NEXT: movl	$1, 800(%esp)
; CHECK-X86-32-NEXT: movl	(%esp), %eax
; CHECK-X86-32-NEXT: addl	$8012, %esp # imm = 0x1F4C
; CHECK-X86-32-NEXT: .cfi_def_cfa_offset 4
; CHECK-X86-32-NEXT: retl
