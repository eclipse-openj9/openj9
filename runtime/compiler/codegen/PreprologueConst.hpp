/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#if defined(TR_HOST_X86)
   #define SAMPLING_CALL_SIZE (5)
   #define LINKAGE_INFO_SIZE (4)
   #if defined(TR_TARGET_64BIT)
      #define SAVE_AREA_SIZE (2)
      #define JITTED_BODY_INFO_SIZE (8)
   #else
      #define SAVE_AREA_SIZE (0)
      #define JITTED_BODY_INFO_SIZE (4)
   #endif
#elif defined(TR_TARGET_S390)
   #if defined(TR_TARGET_64BIT)
      // Offset from interpreter entry point to the JIT entry point save / restore location used for patching
      #define OFFSET_INTEP_JITEP_SAVE_RESTORE_LOCATION -16

      // Offset from interpreter entry point to the VM call helper trampoline
      #define OFFSET_INTEP_VM_CALL_HELPER_TRAMPOLINE -20

      // Offset from interpreter entry point to the samplingRecompileMethod trampoline
      #define OFFSET_INTEP_SAMPLING_RECOMPILE_METHOD_TRAMPOLINE 24

      // Offset from interpreter entry point to the samplingRecompileMethod address
      #define OFFSET_INTEP_SAMPLING_RECOMPILE_METHOD_ADDRESS 32

      // Offset from JIT entry point to the countingPatchCallSite snippet start
      #define OFFSET_JITEP_COUNTING_PATCH_CALL_SITE_SNIPPET_START 72

      // Offset from JIT entry point to the countingPatchCallSite snippet end
      #define OFFSET_JITEP_COUNTING_PATCH_CALL_SITE_SNIPPET_END 124

      // Size in bytes of the counting prologue
      #define COUNTING_PROLOGUE_SIZE 134
   #else
      // Offset from interpreter entry point to the JIT entry point save / restore location used for patching
      #define OFFSET_INTEP_JITEP_SAVE_RESTORE_LOCATION -12

      // Offset from interpreter entry point to the VM call helper trampoline
      #define OFFSET_INTEP_VM_CALL_HELPER_TRAMPOLINE -16

      // Offset from interpreter entry point to the samplingRecompileMethod trampoline
      #define OFFSET_INTEP_SAMPLING_RECOMPILE_METHOD_TRAMPOLINE 20

      // Offset from interpreter entry point to the samplingRecompileMethod address
      #define OFFSET_INTEP_SAMPLING_RECOMPILE_METHOD_ADDRESS 24

      // Offset from JIT entry point to the countingPatchCallSite snippet start
      #define OFFSET_JITEP_COUNTING_PATCH_CALL_SITE_SNIPPET_START 58

      // Offset from JIT entry point to the countingPatchCallSite snippet end
      #define OFFSET_JITEP_COUNTING_PATCH_CALL_SITE_SNIPPET_END 96

      // Size in bytes of the counting prologue
      #define COUNTING_PROLOGUE_SIZE 104
   #endif
#endif
