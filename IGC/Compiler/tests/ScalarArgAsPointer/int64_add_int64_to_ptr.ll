;=========================== begin_copyright_notice ============================
;
; Copyright (C) 2022 Intel Corporation
;
; SPDX-License-Identifier: MIT
;
;============================ end_copyright_notice =============================

; RUN: igc_opt --igc-scalar-arg-as-pointer-analysis --serialize-igc-metadata -S %s | FileCheck %s
;
; OpenCL kernel:
;
;   kernel void test(long addr) {
;     *((global int*) a + b) = 39;
;   }
;
; Two scalar kernel args are combined into pointer to global memory, match both.
;
; CHECK: !{!"m_OpenCLArgScalarAsPointersSet[0]", i32 0}
; CHECK: !{!"m_OpenCLArgScalarAsPointersSet[0]", i32 1}

define spir_kernel void @test(i64 %a, i64 %b) #0 {
entry:
  %0 = inttoptr i64 %a to i32 addrspace(1)*
  %add.ptr = getelementptr inbounds i32, i32 addrspace(1)* %0, i64 %b
  store i32 39, i32 addrspace(1)* %add.ptr, align 4
  ret void
}

!igc.functions = !{!0}

!0 = !{void (i64, i64)* @test, !1}
!1 = !{!2}
!2 = !{!"function_type", i32 0}
