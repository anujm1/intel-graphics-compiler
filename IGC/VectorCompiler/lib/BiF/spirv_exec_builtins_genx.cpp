/*========================== begin_copyright_notice ============================

Copyright (C) 2021 Intel Corporation

SPDX-License-Identifier: MIT

============================= end_copyright_notice ===========================*/

#include <cm-cl/exec.h>

using namespace cm;

CM_NODEBUG CM_INLINE uint __spirv_BuiltInWorkDim() { return 3; }

CM_NODEBUG CM_INLINE ulong __spirv_BuiltInGlobalSize(int dim) {
  return static_cast<ulong>(cm::exec::get_local_size(dim)) *
         cm::exec::get_group_count(dim);
}

CM_NODEBUG CM_INLINE ulong __spirv_BuiltInGlobalInvocationId(int dim) {
  return cm::exec::get_local_id(dim) +
         static_cast<ulong>(cm::exec::get_group_id(dim)) *
             cm::exec::get_local_size(dim);
}

CM_NODEBUG CM_INLINE ulong __spirv_BuiltInWorkgroupSize(int dim) {
  return cm::exec::get_local_size(dim);
}

CM_NODEBUG CM_INLINE ulong __spirv_BuiltInEnqueuedWorkgroupSize(int dim) {
  return cm::exec::get_local_size(dim);
}

CM_NODEBUG CM_INLINE ulong __spirv_BuiltInLocalInvocationId(int dim) {
  return cm::exec::get_local_id(dim);
}

CM_NODEBUG CM_INLINE ulong __spirv_BuiltInNumWorkgroups(int dim) {
  return cm::exec::get_group_count(dim);
}

CM_NODEBUG CM_INLINE ulong __spirv_BuiltInWorkgroupId(int dim) {
  return cm::exec::get_group_id(dim);
}

CM_NODEBUG CM_INLINE ulong __spirv_BuiltInGlobalOffset(int dim) {
  // VC does not support global offset yet
  return 0;
}

CM_NODEBUG CM_INLINE ulong __spirv_BuiltInLocalInvocationIndex() {
  using namespace cm::exec;
  return get_local_size(0) * get_local_size(1) * get_local_id(2) +
         get_local_size(0) * get_local_id(1) + get_local_id(0);
}

CM_NODEBUG CM_INLINE ulong __spirv_BuiltInGlobalLinearId() {
  using namespace cm::exec;
  uint group_id = get_group_count(0) * get_group_count(1) * get_group_id(2) +
                  get_group_count(0) * get_group_id(1) + get_group_id(0);
  uint local_size = get_local_size(0) * get_local_size(1) * get_local_size(2);
  uint local_id = __spirv_BuiltInLocalInvocationIndex();

  return group_id * local_size + local_id;
}

CM_NODEBUG CM_INLINE uint __spirv_BuiltInSubgroupSize() {
  // VC code always uses SubgroupSize == 1
  return 1;
}

CM_NODEBUG CM_INLINE uint __spirv_BuiltInSubgroupMaxSize() {
  // VC code always uses SubgroupSize == 1
  return 1;
}

CM_NODEBUG CM_INLINE uint __spirv_BuiltInNumSubgroups() {
  return __spirv_BuiltInGlobalSize(0) * __spirv_BuiltInGlobalSize(1) *
         __spirv_BuiltInGlobalSize(2);
}

CM_NODEBUG CM_INLINE uint __spirv_BuiltInNumEnqueuedSubgroups() {
  return __spirv_BuiltInGlobalSize(0) * __spirv_BuiltInGlobalSize(1) *
         __spirv_BuiltInGlobalSize(2);
}

CM_NODEBUG CM_INLINE uint __spirv_BuiltInSubgroupId() {
  return __spirv_BuiltInGlobalLinearId();
}

CM_NODEBUG CM_INLINE uint __spirv_BuiltInSubgroupLocalInvocationId() {
  // VC code always uses SubgroupSize == 1
  return 0;
}

CM_NODEBUG CM_INLINE void __spirv_ControlBarrier(int scope, int memory_scope,
                                                 int memory_semantics) {
  cm::exec::fence(memory_scope, memory_semantics);
  cm::exec::barrier(scope);
}

CM_NODEBUG CM_INLINE void __spirv_MemoryBarrier(int scope, int semantics) {
  cm::exec::fence(scope, semantics);
}

CM_NODEBUG CM_INLINE void
__spirv_ControlBarrierArriveINTEL(int scope, int memory_scope,
                                  int memory_semantics) {
  cm::exec::fence(memory_scope, memory_semantics);
  cm::exec::barrier_arrive(scope);
}

CM_NODEBUG CM_INLINE void
__spirv_ControlBarrierWaitINTEL(int scope, int memory_scope,
                                int memory_semantics) {
  cm::exec::fence(memory_scope, memory_semantics);
  cm::exec::barrier_wait(scope);
}
