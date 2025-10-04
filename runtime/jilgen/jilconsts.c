/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include <string.h>
#include "j9port.h"
#include "j9consts.h"
#include "jilconsts.h"

static char const *jilconsts = "oti/jilconsts.inc";
static char const *jilvalues = "oti/jilvalues.m4";
static char line[1024];
static int values = 0;

static UDATA writeConstant(OMRPortLibrary *OMRPORTLIB, IDATA fd, char const *name, UDATA value);
static jint writeConstants(OMRPortLibrary *OMRPORTLIB, IDATA fd);
static IDATA createConstant(OMRPortLibrary *OMRPORTLIB, char const *name, UDATA value);

#if defined(J9VM_ARCH_X86)
static jint writeMacros(OMRPortLibrary *OMRPORTLIB, IDATA fd);
#endif

static jint
writeHeader(OMRPortLibrary *OMRPORTLIB, IDATA fd)
{
	jint rc = JNI_OK;
	char const *headertext =
		"changequote({,})dnl\n"
		"define({J9CONST},{define({$1},{{$2}ifelse($}{#,0,,($}{@))})})dnl\n";

	if (0 != omrfile_write_text(fd, headertext, strlen(headertext))) {
		rc = JNI_ERR;
	}

	return rc;
}

static UDATA
writeConstant(OMRPortLibrary *OMRPORTLIB, IDATA fd, char const *name, UDATA value)
{
	UDATA err = 0;
	IDATA len = createConstant(OMRPORTLIB, name, value);
	if (0 != omrfile_write_text(fd, line, len)) {
		err = 1;
	}
	return err;
}

static IDATA
createConstant(OMRPortLibrary *OMRPORTLIB, char const *name, UDATA value)
{
	if (0 != values) {
		return omrstr_printf(line, sizeof(line), "J9CONST({%s},%zu)dnl\n", name, value);
	}
#if defined(J9VM_ARCH_POWER) || defined(J9VM_ARCH_ARM) || defined(J9VM_ARCH_AARCH64)
	return omrstr_printf(line, sizeof(line), "#define %s %zu\n", name, value);
#elif defined(J9VM_ARCH_X86) /* J9VM_ARCH_POWER || J9VM_ARCH_ARM || J9VM_ARCH_AARCH64 */
	return omrstr_printf(line, sizeof(line), "%%define %s %zu\n", name, value);
#elif defined(LINUX) /* J9VM_ARCH_X86 */
	return omrstr_printf(line, sizeof(line), "%s = %zu\n", name, value);
#elif defined(J9ZOS390) /* LINUX */
	return omrstr_printf(line, sizeof(line), "%s EQU %zu\n", name, value);
#else
#error "Unknown constant format"
#endif /* J9VM_ARCH_POWER || J9VM_ARCH_ARM || J9VM_ARCH_AARCH64 */
}

#if defined(J9VM_ARCH_X86)

#if defined(LINUX)
#if defined(J9VM_ENV_DATA64)
static char const *macroString = "\n\
%macro MoveHelper 2 ; register, helperName\n\
	lea %1, [rel %2]\n\
%endmacro\n\
\n\
%macro CallHelper 1 ; helperName\n\
	call %1\n\
%endmacro\n\
\n\
%macro CallHelperUseReg 2 ; helperName, register\n\
	call %1\n\
%endmacro\n\
\n\
%macro DECLARE_EXTERN 1 ; helperName\n\
	extern %1\n\
%endmacro\n\
\n\
%macro DECLARE_GLOBAL 1 ; helperName\n\
	global %1\n\
%endmacro\n\
\n\
%define _rax rax\n\
%define _rbx rbx\n\
%define _rcx rcx\n\
%define _rdx rdx\n\
%define _rsi rsi\n\
%define _rdi rdi\n\
%define _rsp rsp\n\
%define _rbp rbp\n\
%define _rel rel\n\
\n";
#else /* J9VM_ENV_DATA64 */
static char const *macroString = "\n\
%macro MoveHelper 2 ; MACRO register,helperName\n\
	lea %1, [%2]\n\
%endmacro\n\
\n\
%macro CallHelper 1 ; MACRO helperName\n\
	call %1\n\
%endmacro\n\
\n\
%macro CallHelperUseReg 2 ; MACRO helperName,register\n\
	call %1\n\
%endmacro\n\
\n\
%macro DECLARE_EXTERN 1 ; helperName\n\
	extern %1\n\
%endmacro\n\
\n\
%macro DECLARE_GLOBAL 1 ; helperName\n\
	global %1\n\
%endmacro\n\
\n\
%define _rax eax\n\
%define _rbx ebx\n\
%define _rcx ecx\n\
%define _rdx edx\n\
%define _rsi esi\n\
%define _rdi edi\n\
%define _rsp esp\n\
%define _rbp ebp\n\
%define _rel\n\
\n";
#endif /* J9VM_ENV_DATA64 */
#elif defined(OSX) /* LINUX */
static char const *macroString = "\n\
%macro MoveHelper 2 ; register, helperName\n\
	lea %1, [rel %2]\n\
%endmacro\n\
\n\
%macro CallHelper 1 ; helperName\n\
	call %1\n\
%endmacro\n\
\n\
%macro CallHelperUseReg 2 ; helperName, register\n\
	call %1\n\
%endmacro\n\
\n\
%macro DECLARE_EXTERN 1 ; helperName\n\
	extern _%1\n\
	%define %1 _%1\n\
%endmacro\n\
\n\
%macro DECLARE_GLOBAL 1 ; helperName\n\
	global _%1\n\
	%define %1 _%1\n\
%endmacro\n\
\n\
%define _rax rax\n\
%define _rbx rbx\n\
%define _rcx rcx\n\
%define _rdx rdx\n\
%define _rsi rsi\n\
%define _rdi rdi\n\
%define _rsp rsp\n\
%define _rbp rbp\n\
%define _rel rel\n\
\n";
#else /* LINUX */
#if defined(J9VM_ENV_DATA64)
static char const *macroString = "\n\
%macro MoveHelper 2 ; register, helperName\n\
	lea %1, [rel %2]\n\
%endmacro\n\
\n\
%macro CallHelper 1 ; helperName\n\
	call %1\n\
%endmacro\n\
\n\
%macro CallHelperUseReg 2 ; helperName, register\n\
	call %1\n\
%endmacro\n\
\n\
%macro DECLARE_EXTERN 1 ; helperName\n\
	extern %1\n\
%endmacro\n\
\n\
%macro DECLARE_GLOBAL 1 ; helperName\n\
	global %1\n\
%endmacro\n\
\n\
%define _rax rax\n\
%define _rbx rbx\n\
%define _rcx rcx\n\
%define _rdx rdx\n\
%define _rsi rsi\n\
%define _rdi rdi\n\
%define _rsp rsp\n\
%define _rbp rbp\n\
%define _rel rel\n\
\n";
#else /* J9VM_ENV_DATA64 */
static char const *macroString = "\n\
%macro MoveHelper 2 ; MACRO register,helperName\n\
	lea %1, [%2]\n\
%endmacro\n\
\n\
%macro CallHelper 1 ; MACRO helperName\n\
	call %1\n\
%endmacro\n\
\n\
%macro CallHelperUseReg 2 ; MACRO helperName,register\n\
	call %1\n\
%endmacro\n\
\n\
%macro DECLARE_EXTERN 1 ; helperName\n\
	extern _%1\n\
	%define %1 _%1\n\
%endmacro\n\
\n\
%macro DECLARE_GLOBAL 1 ; helperName\n\
	global _%1\n\
	%define %1 _%1\n\
%endmacro\n\
\n\
%define _rax eax\n\
%define _rbx ebx\n\
%define _rcx ecx\n\
%define _rdx edx\n\
%define _rsi esi\n\
%define _rdi edi\n\
%define _rsp esp\n\
%define _rbp ebp\n\
%define _rel\n\
\n";
#endif /* J9VM_ENV_DATA64 */
#endif /* LINUX */

static jint
writeMacros(OMRPortLibrary *OMRPORTLIB, IDATA fd)
{
	jint rc = JNI_OK;
	IDATA len = strlen(macroString);
	if (0 != omrfile_write_text(fd, macroString, len)) {
		rc = JNI_ERR;
	}
	return rc;
}

#endif /* J9VM_ARCH_X86 */

static jint
writeConstants(OMRPortLibrary *OMRPORTLIB, IDATA fd)
{
	jint rc = JNI_OK;
	UDATA err =
			/* Build flags */
#if defined(JAVA_SPEC_VERSION)
			writeConstant(OMRPORTLIB, fd, "ASM_JAVA_SPEC_VERSION", JAVA_SPEC_VERSION) |
#endif /* JAVA_SPEC_VERSION */
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
			writeConstant(OMRPORTLIB, fd, "ASM_J9VM_INTERP_ATOMIC_FREE_JNI", 1) |
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
#if defined(J9VM_JIT_NEW_DUAL_HELPERS)
			writeConstant(OMRPORTLIB, fd, "ASM_J9VM_JIT_NEW_DUAL_HELPERS", 1) |
#endif /* J9VM_JIT_NEW_DUAL_HELPERS */
#if defined(J9VM_ENV_LITTLE_ENDIAN)
			writeConstant(OMRPORTLIB, fd, "ASM_J9VM_ENV_LITTLE_ENDIAN", 1) |
#endif /* J9VM_ENV_LITTLE_ENDIAN */
#if defined(J9VM_ENV_DATA64)
			writeConstant(OMRPORTLIB, fd, "ASM_J9VM_ENV_DATA64", 1) |
#endif /* J9VM_ENV_DATA64 */
#if defined(J9VM_INTERP_COMPRESSED_OBJECT_HEADER)
			writeConstant(OMRPORTLIB, fd, "ASM_J9VM_INTERP_COMPRESSED_OBJECT_HEADER", 1) |
#endif /* J9VM_INTERP_COMPRESSED_OBJECT_HEADER */
#if defined(J9VM_INTERP_SMALL_MONITOR_SLOT)
			writeConstant(OMRPORTLIB, fd, "ASM_J9VM_INTERP_SMALL_MONITOR_SLOT", 1) |
#endif /* J9VM_INTERP_SMALL_MONITOR_SLOT */
#if defined(OMR_GC_COMPRESSED_POINTERS)
			writeConstant(OMRPORTLIB, fd, "ASM_OMR_GC_COMPRESSED_POINTERS", 1) |
#endif /* OMR_GC_COMPRESSED_POINTERS */
#if defined(OMR_GC_FULL_POINTERS)
			writeConstant(OMRPORTLIB, fd, "ASM_OMR_GC_FULL_POINTERS", 1) |
#endif /* OMR_GC_FULL_POINTERS */
#if defined(J9VM_GC_TLH_PREFETCH_FTA)
			writeConstant(OMRPORTLIB, fd, "ASM_J9VM_GC_TLH_PREFETCH_FTA", 1) |
#endif /* J9VM_GC_TLH_PREFETCH_FTA */
#if defined(J9VM_GC_TLH_PREFETCH_FTA_DISABLE)
			writeConstant(OMRPORTLIB, fd, "ASM_J9VM_GC_TLH_PREFETCH_FTA_DISABLE", 1) |
#endif /* J9VM_GC_TLH_PREFETCH_FTA_DISABLE */
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
			writeConstant(OMRPORTLIB, fd, "ASM_J9VM_GC_DYNAMIC_CLASS_UNLOADING", 1) |
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
#if defined(J9VM_JIT_32BIT_USES64BIT_REGISTERS)
			writeConstant(OMRPORTLIB, fd, "ASM_J9VM_JIT_32BIT_USES64BIT_REGISTERS", 1) |
#endif /* J9VM_JIT_32BIT_USES64BIT_REGISTERS */
#if defined(J9VM_JIT_FREE_SYSTEM_STACK_POINTER)
			writeConstant(OMRPORTLIB, fd, "ASM_J9VM_JIT_FREE_SYSTEM_STACK_POINTER", 1) |
#endif /* J9VM_JIT_FREE_SYSTEM_STACK_POINTER */
#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
			writeConstant(OMRPORTLIB, fd, "ASM_J9VM_OPT_OPENJDK_METHODHANDLE", 1) |
#endif /* J9VM_OPT_OPENJDK_METHODHANDLE */
#if defined(J9VM_PORT_ZOS_CEEHDLRSUPPORT)
			writeConstant(OMRPORTLIB, fd, "ASM_J9VM_PORT_ZOS_CEEHDLRSUPPORT", 1) |
			writeConstant(OMRPORTLIB, fd, "J9TR_ELS_ceehdlrGPRBase", offsetof(J9VMEntryLocalStorage, ceehdlrGPRBase)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_ELS_ceehdlrFPRBase", offsetof(J9VMEntryLocalStorage, ceehdlrFPRBase)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_ELS_ceehdlrFPCLocation", offsetof(J9VMEntryLocalStorage, ceehdlrFPCLocation)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_ELS_machineSPSaveSlot", offsetof(J9VMEntryLocalStorage, machineSPSaveSlot)) |
#endif /* J9VM_PORT_ZOS_CEEHDLRSUPPORT */

#if defined(OMR_GC_CONCURRENT_SCAVENGER)
#if defined(J9VM_ARCH_AARCH64) || defined(J9VM_ARCH_ARM) || defined(J9VM_ARCH_POWER)
			/* These platforms include j9cfg.h so don't redefine OMR_GC_CONCURRENT_SCAVENGER in jilconsts. */
			((0 != values) ? writeConstant(OMRPORTLIB, fd, "OMR_GC_CONCURRENT_SCAVENGER", 1) : 0) |
#else /* defined(J9VM_ARCH_AARCH64) || defined(J9VM_ARCH_ARM) || defined(J9VM_ARCH_POWER) */
			writeConstant(OMRPORTLIB, fd, "OMR_GC_CONCURRENT_SCAVENGER", 1) |
#endif /* defined(J9VM_ARCH_AARCH64) || defined(J9VM_ARCH_ARM) || defined(J9VM_ARCH_POWER) */
#endif /* defined(OMR_GC_CONCURRENT_SCAVENGER) */

			/* C stack frame */
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_sizeof", sizeof(J9CInterpreterStackFrame)) |
#if defined(J9VM_ARCH_S390)
			/* S390 */
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_jitGPRs", offsetof(J9CInterpreterStackFrame, jitGPRs)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_jitFPRs", offsetof(J9CInterpreterStackFrame, jitFPRs)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_jitVRs", offsetof(J9CInterpreterStackFrame, jitVRs)) |
#if defined(J9ZOS390)
			/* Z/OS */
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_argRegisterSave", offsetof(J9CInterpreterStackFrame, argRegisterSave)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_fpcCEEHDLR", offsetof(J9CInterpreterStackFrame, fpcCEEHDLR)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_preservedFPRs", offsetof(J9CInterpreterStackFrame, preservedFPRs)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_preservedVRs", offsetof(J9CInterpreterStackFrame, preservedVRs)) |
#if defined(J9VM_PORT_ZOS_CEEHDLRSUPPORT)
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_gprCEEHDLR", offsetof(J9CInterpreterStackFrame, gprCEEHDLR)) |
#endif /* J9VM_PORT_ZOS_CEEHDLRSUPPORT */
#else /* J9ZOS390 */
			/* z/Linux */
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_argRegisterSave", offsetof(J9CInterpreterStackFrame, argRegisterSave)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_preservedGPRs", offsetof(J9CInterpreterStackFrame, preservedGPRs)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_preservedFPRs", offsetof(J9CInterpreterStackFrame, preservedFPRs)) |
#endif /* J9ZOS390 */
#elif defined(J9VM_ARCH_POWER) /* J9VM_ARCH_S390 */
			/* PPC */
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_preservedCR", offsetof(J9CInterpreterStackFrame, preservedCR)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_preservedLR", offsetof(J9CInterpreterStackFrame, preservedLR)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_preservedGPRs", offsetof(J9CInterpreterStackFrame, preservedGPRs)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_preservedFPRs", offsetof(J9CInterpreterStackFrame, preservedFPRs)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_jitGPRs", offsetof(J9CInterpreterStackFrame, jitGPRs)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_jitFPRs", offsetof(J9CInterpreterStackFrame, jitFPRs)) |
#if defined(J9VM_ENV_DATA64)
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_jitVRs", offsetof(J9CInterpreterStackFrame, jitVRs)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_preservedVRs", offsetof(J9CInterpreterStackFrame, preservedVRs)) |
#endif /* J9VM_ENV_DATA64 */
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_jitCR", offsetof(J9CInterpreterStackFrame, jitGPRs.jitCR)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_jitLR", offsetof(J9CInterpreterStackFrame, jitGPRs.jitLR)) |
#if !defined(LINUX) || defined(J9VM_ENV_DATA64)
			/* Everyone but Linux PPC 32 */
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_currentTOC", offsetof(J9CInterpreterStackFrame, currentTOC)) |
#endif /* !LINUX || J9VM_ENV_DATA64 */
#elif defined(J9VM_ARCH_ARM) /* J9VM_ARCH_POWER */
			/* ARM */
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_preservedFPRs", offsetof(J9CInterpreterStackFrame, preservedFPRs)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_jitGPRs", offsetof(J9CInterpreterStackFrame, jitGPRs)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_jitFPRs", offsetof(J9CInterpreterStackFrame, jitFPRs)) |
#elif defined(J9VM_ARCH_AARCH64) /* J9VM_ARCH_ARM */
			/* AARCH64 */
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_preservedGPRs", offsetof(J9CInterpreterStackFrame, preservedGPRs)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_preservedFPRs", offsetof(J9CInterpreterStackFrame, preservedFPRs)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_jitGPRs", offsetof(J9CInterpreterStackFrame, jitGPRs)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_jitFPRs", offsetof(J9CInterpreterStackFrame, jitFPRs)) |
#elif defined(J9VM_ARCH_RISCV) /* J9VM_ARCH_AARCH64 */
			/* RISC-V */
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_preservedGPRs", offsetof(J9CInterpreterStackFrame, preservedGPRs)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_preservedFPRs", offsetof(J9CInterpreterStackFrame, preservedFPRs)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_jitGPRs", offsetof(J9CInterpreterStackFrame, jitGPRs)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_jitFPRs", offsetof(J9CInterpreterStackFrame, jitFPRs)) |
#elif defined(J9VM_ARCH_X86) /* J9VM_ARCH_RISCV */
			/* x86 */
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_vmStruct", offsetof(J9CInterpreterStackFrame, vmStruct)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_machineBP", offsetof(J9CInterpreterStackFrame, machineBP)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_jitGPRs", offsetof(J9CInterpreterStackFrame, jitGPRs)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_jitFPRs", offsetof(J9CInterpreterStackFrame, jitFPRs)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_maskRegisters", offsetof(J9CInterpreterStackFrame, maskRegisters)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_rax", offsetof(J9CInterpreterStackFrame, jitGPRs.jitGPRs.named.rax)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_rbx", offsetof(J9CInterpreterStackFrame, jitGPRs.jitGPRs.named.rbx)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_rcx", offsetof(J9CInterpreterStackFrame, jitGPRs.jitGPRs.named.rcx)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_rdx", offsetof(J9CInterpreterStackFrame, jitGPRs.jitGPRs.named.rdx)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_rdi", offsetof(J9CInterpreterStackFrame, jitGPRs.jitGPRs.named.rdi)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_rsi", offsetof(J9CInterpreterStackFrame, jitGPRs.jitGPRs.named.rsi)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_rbp", offsetof(J9CInterpreterStackFrame, jitGPRs.jitGPRs.named.rbp)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_rsp", offsetof(J9CInterpreterStackFrame, jitGPRs.jitGPRs.named.rsp)) |
#if defined(J9VM_ENV_DATA64)
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_r8", offsetof(J9CInterpreterStackFrame, jitGPRs.jitGPRs.named.r8)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_r9", offsetof(J9CInterpreterStackFrame, jitGPRs.jitGPRs.named.r9)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_r10", offsetof(J9CInterpreterStackFrame, jitGPRs.jitGPRs.named.r10)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_r11", offsetof(J9CInterpreterStackFrame, jitGPRs.jitGPRs.named.r11)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_r12", offsetof(J9CInterpreterStackFrame, jitGPRs.jitGPRs.named.r12)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_r13", offsetof(J9CInterpreterStackFrame, jitGPRs.jitGPRs.named.r13)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_r14", offsetof(J9CInterpreterStackFrame, jitGPRs.jitGPRs.named.r14)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_r15", offsetof(J9CInterpreterStackFrame, jitGPRs.jitGPRs.named.r15)) |
#if defined(WIN32)
			/* Windows x86-64 */
			writeConstant(OMRPORTLIB, fd, "J9TR_cframe_preservedFPRs", offsetof(J9CInterpreterStackFrame, preservedFPRs)) |
#endif /* WIN32 */
#endif /* J9VM_ENV_DATA64 */
#endif /* J9VM_ARCH_X86 */

			/* X86-specific */
#if defined(J9VM_ARCH_X86)
			writeConstant(OMRPORTLIB, fd, "J9TR_runtimeFlags_PatchingFenceRequired", J9JIT_PATCHING_FENCE_REQUIRED) |
			writeConstant(OMRPORTLIB, fd, "J9TR_runtimeFlags_PatchingFenceType", J9JIT_PATCHING_FENCE_TYPE) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_machineSP", sizeof(J9VMThread)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_machineSP_machineBP", offsetof(J9CInterpreterStackFrame, machineBP)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_machineSP_vmStruct", offsetof(J9CInterpreterStackFrame, vmStruct)) |
#endif /* J9VM_ARCH_X86 */

			/* ZOS-specific */
#if defined(J9TR_CAA_SAVE_OFFSET)
			writeConstant(OMRPORTLIB, fd, "J9TR_CAA_save_offset", J9TR_CAA_SAVE_OFFSET) |
#endif /* J9ZOS390 */

			/* J9VMThread */
#if defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS)
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThreadCompressObjectReferences", offsetof(J9VMThread, compressObjectReferences)) |
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS) */
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThreadCurrentException", offsetof(J9VMThread, currentException)) |
#if defined(J9VM_ENV_SHARED_LIBS_USE_GLOBAL_TABLE)
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThreadRTOCOffset", offsetof(J9VMThread, jitTOC)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_jitTOC", offsetof(J9VMThread, jitTOC)) |
#endif /* J9VM_ENV_SHARED_LIBS_USE_GLOBAL_TABLE */
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_floatTemp1", offsetof(J9VMThread, floatTemp1)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_floatTemp2", offsetof(J9VMThread, floatTemp2)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_floatTemp3", offsetof(J9VMThread, floatTemp3)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_floatTemp4", offsetof(J9VMThread, floatTemp4)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_stackOverflowMark", offsetof(J9VMThread, stackOverflowMark)) |
#if defined(J9VM_GC_THREAD_LOCAL_HEAP)
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_heapAlloc", offsetof(J9VMThread, heapAlloc)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_heapTop", offsetof(J9VMThread, heapTop)) |
#endif /* J9VM_GC_THREAD_LOCAL_HEAP */
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_tlhPrefetchFTA", offsetof(J9VMThread, tlhPrefetchFTA)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_publicFlags", offsetof(J9VMThread, publicFlags)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_privateFlags", offsetof(J9VMThread, privateFlags)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_privateFlags2", offsetof(J9VMThread, privateFlags2)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThreadJavaVM", offsetof(J9VMThread, javaVM)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_javaVM", offsetof(J9VMThread, javaVM)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_sp", offsetof(J9VMThread, sp)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_pc", offsetof(J9VMThread, pc)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_literals", offsetof(J9VMThread, literals)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_arg0EA", offsetof(J9VMThread, arg0EA)) |
#if defined(J9VM_JIT_FREE_SYSTEM_STACK_POINTER)
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_systemStackPointer", offsetof(J9VMThread, systemStackPointer)) |
#endif /* J9VM_JIT_FREE_SYSTEM_STACK_POINTER */
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_codertTOC", offsetof(J9VMThread, codertTOC)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_debugEventData1", offsetof(J9VMThread, debugEventData1)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_debugEventData2", offsetof(J9VMThread, debugEventData2)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_debugEventData3", offsetof(J9VMThread, debugEventData3)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_debugEventData4", offsetof(J9VMThread, debugEventData4)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_debugEventData5", offsetof(J9VMThread, debugEventData5)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_debugEventData6", offsetof(J9VMThread, debugEventData6)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_debugEventData7", offsetof(J9VMThread, debugEventData7)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_debugEventData8", offsetof(J9VMThread, debugEventData8)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_tempSlot", offsetof(J9VMThread, tempSlot)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_jitReturnAddress", offsetof(J9VMThread, jitReturnAddress)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_returnValue", offsetof(J9VMThread, returnValue)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_returnValue2", offsetof(J9VMThread, returnValue2)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_entryLocalStorage", offsetof(J9VMThread, entryLocalStorage)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_stackWalkState", offsetof(J9VMThread, stackWalkState)) |
#if defined(OMR_GC_CONCURRENT_SCAVENGER) && defined(J9VM_ARCH_S390)
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_gsParameters_GSECI", offsetof(J9VMThread, gsParameters) + 2) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_gsParameters_instructionAddr", offsetof(J9VMThread, gsParameters.instructionAddr)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_gsParameters_operandAddr", offsetof(J9VMThread, gsParameters.operandAddr)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_gsParameters_returnAddr", offsetof(J9VMThread, gsParameters.returnAddr)) |
#endif /* OMR_GC_CONCURRENT_SCAVENGER */

#if JAVA_SPEC_VERSION >= 19
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_ownedMonitorCount", offsetof(J9VMThread, ownedMonitorCount)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_VMThread_callOutCount", offsetof(J9VMThread, callOutCount)) |
#endif

			/* J9StackWalkState */
			writeConstant(OMRPORTLIB, fd, "J9TR_J9StackWalkState_restartPoint", offsetof(J9StackWalkState, restartPoint)) |

			/* J9JavaVM */
			writeConstant(OMRPORTLIB, fd, "J9TR_JavaVMJitConfig", offsetof(J9JavaVM, jitConfig)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JavaVM_runtimeFlags", offsetof(J9JavaVM, runtimeFlags)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JavaVM_cInterpreter", offsetof(J9JavaVM, cInterpreter)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JavaVM_bytecodeLoop", offsetof(J9JavaVM, bytecodeLoop)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JavaVM_extendedRuntimeFlags", offsetof(J9JavaVM, extendedRuntimeFlags)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JavaVM_extendedRuntimeFlags2", offsetof(J9JavaVM, extendedRuntimeFlags2)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JavaVM_extendedRuntimeFlags3", offsetof(J9JavaVM, extendedRuntimeFlags3)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JavaVMInternalFunctionTable", offsetof(J9JavaVM, internalVMFunctions)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JavaVM_memoryManagerFunctions", offsetof(J9JavaVM, memoryManagerFunctions)) |
#if defined(OMR_GC_CONCURRENT_SCAVENGER) && defined(J9VM_ARCH_S390)
			writeConstant(OMRPORTLIB, fd, "J9TR_JavaVM_invokeJ9ReadBarrier", offsetof(J9JavaVM, invokeJ9ReadBarrier)) |
#endif
#if defined(J9VM_ENV_SHARED_LIBS_USE_GLOBAL_TABLE)
			writeConstant(OMRPORTLIB, fd, "J9TR_JavaVM_jitTOC", offsetof(J9JavaVM, jitTOC)) |
#endif /* J9VM_ENV_SHARED_LIBS_USE_GLOBAL_TABLE */
#if defined(OMR_GC_COMPRESSED_POINTERS)
			writeConstant(OMRPORTLIB, fd, "J9TR_JavaVM_compressedPointersShift", offsetof(J9JavaVM, compressedPointersShift)) |
#endif /* OMR_GC_COMPRESSED_POINTERS */
			writeConstant(OMRPORTLIB, fd, "J9TR_J9MemoryManagerFunctions_J9ReadBarrier", offsetof(J9MemoryManagerFunctions, J9ReadBarrier)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_J9MemoryManagerFunctions_referenceArrayCopy", offsetof(J9MemoryManagerFunctions, referenceArrayCopy)) |
			/* J9VMEntryLocalStorage */
			writeConstant(OMRPORTLIB, fd, "J9TR_ELS_jitGlobalStorageBase", offsetof(J9VMEntryLocalStorage, jitGlobalStorageBase)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_ELS_jitFPRegisterStorageBase", offsetof(J9VMEntryLocalStorage, jitFPRegisterStorageBase)) |
#if defined(J9VM_ARCH_X86)
			writeConstant(OMRPORTLIB, fd, "J9TR_ELS_machineSPSaveSlot", offsetof(J9VMEntryLocalStorage, machineSPSaveSlot)) |
#endif /* J9VM_ARCH_X86 */

#if defined(J9VM_ARCH_X86)
			/* J9VMJITRegisterState */
			writeConstant(OMRPORTLIB, fd, "J9TR_J9VMJITRegisterState_jit_fpr0", offsetof(J9VMJITRegisterState, jit_fpr0)) |
#endif /* J9VM_ARCH_X86 */

			/* J9Class */
			writeConstant(OMRPORTLIB, fd, "J9TR_J9Class_classLoader", offsetof(J9Class, classLoader)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_J9Class_iTable", offsetof(J9Class, iTable)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_J9Class_lastITable", offsetof(J9Class, lastITable)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_J9Class_lockOffset", offsetof(J9Class, lockOffset)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_ArrayClass_componentType", offsetof(J9ArrayClass, componentType)) |

			/* J9ITable */
			writeConstant(OMRPORTLIB, fd, "J9TR_ITableOffset", sizeof(J9ITable)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_J9ITable_interfaceClass", offsetof(J9ITable, interfaceClass)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_J9ITable_next", offsetof(J9ITable, next)) |

			/* J9Method */
			writeConstant(OMRPORTLIB, fd, "J9TR_MethodFlagsOffset", offsetof(J9Method, constantPool)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_MethodPCStartOffset", offsetof(J9Method, extra)) |

			/* J9JITConfig */
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_runtimeFlags", offsetof(J9JITConfig, runtimeFlags)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_loadPreservedAndBranch", offsetof(J9JITConfig, loadPreservedAndBranch)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_pseudoTOC", offsetof(J9JITConfig, pseudoTOC));

	if (0 != values) {
		err |=
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitNewObject", offsetof(J9JITConfig, old_fast_jitNewObject)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitNewObject", offsetof(J9JITConfig, old_slow_jitNewObject)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitNewObjectNoZeroInit", offsetof(J9JITConfig, old_fast_jitNewObjectNoZeroInit)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitNewObjectNoZeroInit", offsetof(J9JITConfig, old_slow_jitNewObjectNoZeroInit)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitANewArray", offsetof(J9JITConfig, old_fast_jitANewArray)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitANewArray", offsetof(J9JITConfig, old_slow_jitANewArray)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitANewArrayNoZeroInit", offsetof(J9JITConfig, old_fast_jitANewArrayNoZeroInit)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitANewArrayNoZeroInit", offsetof(J9JITConfig, old_slow_jitANewArrayNoZeroInit)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitNewArray", offsetof(J9JITConfig, old_fast_jitNewArray)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitNewArray", offsetof(J9JITConfig, old_slow_jitNewArray)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitNewArrayNoZeroInit", offsetof(J9JITConfig, old_fast_jitNewArrayNoZeroInit)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitNewArrayNoZeroInit", offsetof(J9JITConfig, old_slow_jitNewArrayNoZeroInit)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitAMultiNewArray", offsetof(J9JITConfig, old_slow_jitAMultiNewArray)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitStackOverflow", offsetof(J9JITConfig, old_slow_jitStackOverflow)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitResolveString", offsetof(J9JITConfig, old_slow_jitResolveString)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_fast_jitAcquireVMAccess", offsetof(J9JITConfig, fast_jitAcquireVMAccess)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_fast_jitReleaseVMAccess", offsetof(J9JITConfig, fast_jitReleaseVMAccess)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitCheckAsyncMessages", offsetof(J9JITConfig, old_slow_jitCheckAsyncMessages)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitCheckCast", offsetof(J9JITConfig, old_fast_jitCheckCast)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitCheckCast", offsetof(J9JITConfig, old_slow_jitCheckCast)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitCheckCastForArrayStore", offsetof(J9JITConfig, old_fast_jitCheckCastForArrayStore)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitCheckCastForArrayStore", offsetof(J9JITConfig, old_slow_jitCheckCastForArrayStore)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitCheckIfFinalizeObject", offsetof(J9JITConfig, old_fast_jitCheckIfFinalizeObject)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitCollapseJNIReferenceFrame", offsetof(J9JITConfig, old_fast_jitCollapseJNIReferenceFrame)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitHandleArrayIndexOutOfBoundsTrap", offsetof(J9JITConfig, old_slow_jitHandleArrayIndexOutOfBoundsTrap)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitHandleIntegerDivideByZeroTrap", offsetof(J9JITConfig, old_slow_jitHandleIntegerDivideByZeroTrap)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitHandleNullPointerExceptionTrap", offsetof(J9JITConfig, old_slow_jitHandleNullPointerExceptionTrap)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitHandleInternalErrorTrap", offsetof(J9JITConfig, old_slow_jitHandleInternalErrorTrap)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitInstanceOf", offsetof(J9JITConfig, old_fast_jitInstanceOf)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitLookupInterfaceMethod", offsetof(J9JITConfig, old_fast_jitLookupInterfaceMethod)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitLookupInterfaceMethod", offsetof(J9JITConfig, old_slow_jitLookupInterfaceMethod)) |
#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitLookupDynamicPublicInterfaceMethod", offsetof(J9JITConfig, old_fast_jitLookupDynamicPublicInterfaceMethod)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitLookupDynamicPublicInterfaceMethod", offsetof(J9JITConfig, old_slow_jitLookupDynamicPublicInterfaceMethod)) |
#endif /* J9VM_OPT_OPENJDK_METHODHANDLE */
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitMethodIsNative", offsetof(J9JITConfig, old_fast_jitMethodIsNative)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitMethodIsSync", offsetof(J9JITConfig, old_fast_jitMethodIsSync)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitMethodMonitorEntry", offsetof(J9JITConfig, old_fast_jitMethodMonitorEntry)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitMethodMonitorEntry", offsetof(J9JITConfig, old_slow_jitMethodMonitorEntry)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitMonitorEntry", offsetof(J9JITConfig, old_fast_jitMonitorEntry)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitMonitorEntry", offsetof(J9JITConfig, old_slow_jitMonitorEntry)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitMethodMonitorExit", offsetof(J9JITConfig, old_fast_jitMethodMonitorExit)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitMethodMonitorExit", offsetof(J9JITConfig, old_slow_jitMethodMonitorExit)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitThrowIncompatibleReceiver", offsetof(J9JITConfig, old_slow_jitThrowIncompatibleReceiver)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitMonitorExit", offsetof(J9JITConfig, old_fast_jitMonitorExit)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitMonitorExit", offsetof(J9JITConfig, old_slow_jitMonitorExit)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitReportMethodEnter", offsetof(J9JITConfig, old_slow_jitReportMethodEnter)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitReportStaticMethodEnter", offsetof(J9JITConfig, old_slow_jitReportStaticMethodEnter)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitReportMethodExit", offsetof(J9JITConfig, old_slow_jitReportMethodExit)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitResolveClass", offsetof(J9JITConfig, old_slow_jitResolveClass)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitResolveClassFromStaticField", offsetof(J9JITConfig, old_slow_jitResolveClassFromStaticField)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitResolvedFieldIsVolatile", offsetof(J9JITConfig, old_fast_jitResolvedFieldIsVolatile)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitResolveField", offsetof(J9JITConfig, old_slow_jitResolveField)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitResolveFieldSetter", offsetof(J9JITConfig, old_slow_jitResolveFieldSetter)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitResolveFieldDirect", offsetof(J9JITConfig, old_slow_jitResolveFieldDirect)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitResolveFieldSetterDirect", offsetof(J9JITConfig, old_slow_jitResolveFieldSetterDirect)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitResolveStaticField", offsetof(J9JITConfig, old_slow_jitResolveStaticField)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitResolveStaticFieldSetter", offsetof(J9JITConfig, old_slow_jitResolveStaticFieldSetter)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitResolveStaticFieldDirect", offsetof(J9JITConfig, old_slow_jitResolveStaticFieldDirect)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitResolveStaticFieldSetterDirect", offsetof(J9JITConfig, old_slow_jitResolveStaticFieldSetterDirect)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitResolveInterfaceMethod", offsetof(J9JITConfig, old_slow_jitResolveInterfaceMethod)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitResolveSpecialMethod", offsetof(J9JITConfig, old_slow_jitResolveSpecialMethod)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitResolveStaticMethod", offsetof(J9JITConfig, old_slow_jitResolveStaticMethod)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitResolveVirtualMethod", offsetof(J9JITConfig, old_slow_jitResolveVirtualMethod)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitResolveMethodType", offsetof(J9JITConfig, old_slow_jitResolveMethodType)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitResolveMethodHandle", offsetof(J9JITConfig, old_slow_jitResolveMethodHandle)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitResolveInvokeDynamic", offsetof(J9JITConfig, old_slow_jitResolveInvokeDynamic)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitResolveConstantDynamic", offsetof(J9JITConfig, old_slow_jitResolveConstantDynamic)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitResolveHandleMethod", offsetof(J9JITConfig, old_slow_jitResolveHandleMethod)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitResolveFlattenableField", offsetof(J9JITConfig, old_slow_jitResolveFlattenableField)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitRetranslateCaller", offsetof(J9JITConfig, old_slow_jitRetranslateCaller)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitRetranslateCallerWithPreparation", offsetof(J9JITConfig, old_slow_jitRetranslateCallerWithPreparation)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitRetranslateMethod", offsetof(J9JITConfig, old_slow_jitRetranslateMethod)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitThrowCurrentException", offsetof(J9JITConfig, old_slow_jitThrowCurrentException)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitThrowException", offsetof(J9JITConfig, old_slow_jitThrowException)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitThrowUnreportedException", offsetof(J9JITConfig, old_slow_jitThrowUnreportedException)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitThrowAbstractMethodError", offsetof(J9JITConfig, old_slow_jitThrowAbstractMethodError)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitThrowArithmeticException", offsetof(J9JITConfig, old_slow_jitThrowArithmeticException)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitThrowArrayIndexOutOfBounds", offsetof(J9JITConfig, old_slow_jitThrowArrayIndexOutOfBounds)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitThrowArrayStoreException", offsetof(J9JITConfig, old_slow_jitThrowArrayStoreException)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitThrowArrayStoreExceptionWithIP", offsetof(J9JITConfig, old_slow_jitThrowArrayStoreExceptionWithIP)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitThrowExceptionInInitializerError", offsetof(J9JITConfig, old_slow_jitThrowExceptionInInitializerError)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitThrowIllegalAccessError", offsetof(J9JITConfig, old_slow_jitThrowIllegalAccessError)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitThrowIncompatibleClassChangeError", offsetof(J9JITConfig, old_slow_jitThrowIncompatibleClassChangeError)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitThrowInstantiationException", offsetof(J9JITConfig, old_slow_jitThrowInstantiationException)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitThrowNullPointerException", offsetof(J9JITConfig, old_slow_jitThrowNullPointerException)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitThrowWrongMethodTypeException", offsetof(J9JITConfig, old_slow_jitThrowWrongMethodTypeException)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitThrowIdentityException", offsetof(J9JITConfig, old_slow_jitThrowIdentityException)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitTypeCheckArrayStoreWithNullCheck", offsetof(J9JITConfig, old_fast_jitTypeCheckArrayStoreWithNullCheck)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitTypeCheckArrayStoreWithNullCheck", offsetof(J9JITConfig, old_slow_jitTypeCheckArrayStoreWithNullCheck)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitTypeCheckArrayStore", offsetof(J9JITConfig, old_fast_jitTypeCheckArrayStore)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitTypeCheckArrayStore", offsetof(J9JITConfig, old_slow_jitTypeCheckArrayStore)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitWriteBarrierBatchStore", offsetof(J9JITConfig, old_fast_jitWriteBarrierBatchStore)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitWriteBarrierBatchStoreWithRange", offsetof(J9JITConfig, old_fast_jitWriteBarrierBatchStoreWithRange)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitWriteBarrierJ9ClassBatchStore", offsetof(J9JITConfig, old_fast_jitWriteBarrierJ9ClassBatchStore)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitWriteBarrierJ9ClassStore", offsetof(J9JITConfig, old_fast_jitWriteBarrierJ9ClassStore)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitWriteBarrierStore", offsetof(J9JITConfig, old_fast_jitWriteBarrierStore)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitWriteBarrierStoreGenerational", offsetof(J9JITConfig, old_fast_jitWriteBarrierStoreGenerational)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitWriteBarrierStoreGenerationalAndConcurrentMark", offsetof(J9JITConfig, old_fast_jitWriteBarrierStoreGenerationalAndConcurrentMark)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitWriteBarrierClassStoreMetronome", offsetof(J9JITConfig, old_fast_jitWriteBarrierClassStoreMetronome)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitWriteBarrierStoreMetronome", offsetof(J9JITConfig, old_fast_jitWriteBarrierStoreMetronome)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitCallJitAddPicToPatchOnClassUnload", offsetof(J9JITConfig, old_slow_jitCallJitAddPicToPatchOnClassUnload)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitCallCFunction", offsetof(J9JITConfig, old_slow_jitCallCFunction)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_fast_jitPreJNICallOffloadCheck", offsetof(J9JITConfig, fast_jitPreJNICallOffloadCheck)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_fast_jitPostJNICallOffloadCheck", offsetof(J9JITConfig, fast_jitPostJNICallOffloadCheck)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitObjectHashCode", offsetof(J9JITConfig, old_fast_jitObjectHashCode)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitInduceOSRAtCurrentPC", offsetof(J9JITConfig, old_slow_jitInduceOSRAtCurrentPC)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitInduceOSRAtCurrentPCAndRecompile", offsetof(J9JITConfig, old_slow_jitInduceOSRAtCurrentPCAndRecompile)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitInterpretNewInstanceMethod", offsetof(J9JITConfig, old_slow_jitInterpretNewInstanceMethod)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitNewInstanceImplAccessCheck", offsetof(J9JITConfig, old_slow_jitNewInstanceImplAccessCheck)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitTranslateNewInstanceMethod", offsetof(J9JITConfig, old_slow_jitTranslateNewInstanceMethod)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitReportFinalFieldModified", offsetof(J9JITConfig, old_slow_jitReportFinalFieldModified)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitReportInstanceFieldRead", offsetof(J9JITConfig, old_slow_jitReportInstanceFieldRead)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitReportInstanceFieldWrite", offsetof(J9JITConfig, old_slow_jitReportInstanceFieldWrite)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitReportStaticFieldRead", offsetof(J9JITConfig, old_slow_jitReportStaticFieldRead)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_jitReportStaticFieldWrite", offsetof(J9JITConfig, old_slow_jitReportStaticFieldWrite)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_saveVectorRegisters", offsetof(J9JITConfig, saveVectorRegisters)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_restoreVectorRegisters", offsetof(J9JITConfig, restoreVectorRegisters)) |

			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitGetFlattenableField", offsetof(J9JITConfig, old_fast_jitGetFlattenableField)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitCloneValueType", offsetof(J9JITConfig, old_fast_jitCloneValueType)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitWithFlattenableField", offsetof(J9JITConfig, old_fast_jitWithFlattenableField)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitPutFlattenableField", offsetof(J9JITConfig, old_fast_jitPutFlattenableField)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitGetFlattenableStaticField", offsetof(J9JITConfig, old_fast_jitGetFlattenableStaticField)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitPutFlattenableStaticField", offsetof(J9JITConfig, old_fast_jitPutFlattenableStaticField)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitLoadFlattenableArrayElement", offsetof(J9JITConfig, old_fast_jitLoadFlattenableArrayElement)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitStoreFlattenableArrayElement", offsetof(J9JITConfig, old_fast_jitStoreFlattenableArrayElement)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitAcmpeqHelper", offsetof(J9JITConfig, old_fast_jitAcmpeqHelper)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitAcmpneHelper", offsetof(J9JITConfig, old_fast_jitAcmpneHelper)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_fast_jitNewValue", offsetof(J9JITConfig, fast_jitNewValue)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_fast_jitNewValueNoZeroInit", offsetof(J9JITConfig, fast_jitNewValueNoZeroInit)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_fast_jitNewObject", offsetof(J9JITConfig, fast_jitNewObject)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_fast_jitNewObjectNoZeroInit", offsetof(J9JITConfig, fast_jitNewObjectNoZeroInit)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_fast_jitANewArray", offsetof(J9JITConfig, fast_jitANewArray)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_fast_jitANewArrayNoZeroInit", offsetof(J9JITConfig, fast_jitANewArrayNoZeroInit)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_fast_jitNewArray", offsetof(J9JITConfig, fast_jitNewArray)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_fast_jitNewArrayNoZeroInit", offsetof(J9JITConfig, fast_jitNewArrayNoZeroInit)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_fast_jitCheckCast", offsetof(J9JITConfig, fast_jitCheckCast)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_fast_jitCheckCastForArrayStore", offsetof(J9JITConfig, fast_jitCheckCastForArrayStore)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_fast_jitMethodMonitorEntry", offsetof(J9JITConfig, fast_jitMethodMonitorEntry)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_fast_jitMonitorEntry", offsetof(J9JITConfig, fast_jitMonitorEntry)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_fast_jitMethodMonitorExit", offsetof(J9JITConfig, fast_jitMethodMonitorExit)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_fast_jitMonitorExit", offsetof(J9JITConfig, fast_jitMonitorExit)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_fast_jitTypeCheckArrayStore", offsetof(J9JITConfig, fast_jitTypeCheckArrayStore)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_fast_jitTypeCheckArrayStoreWithNullCheck", offsetof(J9JITConfig, fast_jitTypeCheckArrayStoreWithNullCheck)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitVolatileReadLong", offsetof(J9JITConfig, old_fast_jitVolatileReadLong)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitVolatileWriteLong", offsetof(J9JITConfig, old_fast_jitVolatileWriteLong)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitVolatileReadDouble", offsetof(J9JITConfig, old_fast_jitVolatileReadDouble)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_fast_jitVolatileWriteDouble", offsetof(J9JITConfig, old_fast_jitVolatileWriteDouble)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_old_slow_icallVMprJavaSendPatchupVirtual", offsetof(J9JITConfig, old_slow_icallVMprJavaSendPatchupVirtual)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_c_jitDecompileOnReturn", offsetof(J9JITConfig, c_jitDecompileOnReturn)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_c_jitDecompileAtExceptionCatch", offsetof(J9JITConfig, c_jitDecompileAtExceptionCatch)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_c_jitReportExceptionCatch", offsetof(J9JITConfig, c_jitReportExceptionCatch)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_c_jitDecompileAtCurrentPC", offsetof(J9JITConfig, c_jitDecompileAtCurrentPC)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_c_jitDecompileBeforeReportMethodEnter", offsetof(J9JITConfig, c_jitDecompileBeforeReportMethodEnter)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_c_jitDecompileBeforeMethodMonitorEnter", offsetof(J9JITConfig, c_jitDecompileBeforeMethodMonitorEnter)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_c_jitDecompileAfterAllocation", offsetof(J9JITConfig, c_jitDecompileAfterAllocation)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_JitConfig_c_jitDecompileAfterMonitorEnter", offsetof(J9JITConfig, c_jitDecompileAfterMonitorEnter));
	}

	err |=
			/* J9InternalVMFunctions */
			writeConstant(OMRPORTLIB, fd, "J9TR_InternalFunctionTableReleaseVMAccess", offsetof(J9InternalVMFunctions, internalReleaseVMAccess)) |
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
			writeConstant(OMRPORTLIB, fd, "J9TR_InternalFunctionTableExitVMToJNI", offsetof(J9InternalVMFunctions, internalExitVMToJNI)) |
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */

			/* Object headers */
			writeConstant(OMRPORTLIB, fd, "J9TR_J9Object_class", offsetof(J9Object, clazz)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_ObjectHeader_class", offsetof(J9Object, clazz)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_IndexableObjectContiguous_objectData", sizeof(J9IndexableObjectContiguous)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_IndexableObjectContiguousCompressed_objectData", sizeof(J9IndexableObjectContiguousCompressed)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_IndexableObjectContiguousFull_objectData", sizeof(J9IndexableObjectContiguousFull)) |

			/* J9SFJNICallInFrame */
			writeConstant(OMRPORTLIB, fd, "J9TR_J9SFJNICallInFrame_exitAddress", offsetof(J9SFJNICallInFrame, exitAddress)) |

			/* J9JITWatchedInstanceFieldData */
			writeConstant(OMRPORTLIB, fd, "J9TR_J9JITWatchedInstanceFieldData_method", offsetof(J9JITWatchedInstanceFieldData, method)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_J9JITWatchedInstanceFieldData_location", offsetof(J9JITWatchedInstanceFieldData, location)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_J9JITWatchedInstanceFieldData_offset", offsetof(J9JITWatchedInstanceFieldData, offset)) |

			/* J9JITWatchedStaticFieldData */
			writeConstant(OMRPORTLIB, fd, "J9TR_J9JITWatchedStaticFieldData_method", offsetof(J9JITWatchedStaticFieldData, method)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_J9JITWatchedStaticFieldData_location", offsetof(J9JITWatchedStaticFieldData, location)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_J9JITWatchedStaticFieldData_fieldAddress", offsetof(J9JITWatchedStaticFieldData, fieldAddress)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_J9JITWatchedStaticFieldData_fieldClass", offsetof(J9JITWatchedStaticFieldData, fieldClass)) |

			/* General constants */
			writeConstant(OMRPORTLIB, fd, "J9TR_bcloop_execute_bytecode", J9_BCLOOP_EXECUTE_BYTECODE) |
			writeConstant(OMRPORTLIB, fd, "J9TR_bcloop_handle_pop_frames", J9_BCLOOP_HANDLE_POP_FRAMES) |
			writeConstant(OMRPORTLIB, fd, "J9TR_bcloop_i2j_transition", J9_BCLOOP_I2J_TRANSITION) |
			writeConstant(OMRPORTLIB, fd, "J9TR_bcloop_j2i_invoke_exact", J9_BCLOOP_J2I_INVOKE_EXACT) |
			writeConstant(OMRPORTLIB, fd, "J9TR_bcloop_j2i_transition", J9_BCLOOP_J2I_TRANSITION) |
			writeConstant(OMRPORTLIB, fd, "J9TR_bcloop_j2i_virtual", J9_BCLOOP_J2I_VIRTUAL) |
			writeConstant(OMRPORTLIB, fd, "J9TR_bcloop_jump_bytecode_prototype", J9_BCLOOP_JUMP_BYTECODE_PROTOTYPE) |
			writeConstant(OMRPORTLIB, fd, "J9TR_bcloop_load_preserved_and_branch", J9_BCLOOP_LOAD_PRESERVED_AND_BRANCH) |
#if JAVA_SPEC_VERSION >= 16
			writeConstant(OMRPORTLIB, fd, "J9TR_bcloop_n2i_transition", J9_BCLOOP_N2I_TRANSITION) |
#endif /* JAVA_SPEC_VERSION >= 16 */
			writeConstant(OMRPORTLIB, fd, "J9TR_bcloop_return_from_jit", J9_BCLOOP_RETURN_FROM_JIT) |
			writeConstant(OMRPORTLIB, fd, "J9TR_bcloop_return_from_jit_ctor", J9_BCLOOP_RETURN_FROM_JIT_CTOR) |
			writeConstant(OMRPORTLIB, fd, "J9TR_bcloop_run_exception_handler", J9_BCLOOP_RUN_EXCEPTION_HANDLER) |
			writeConstant(OMRPORTLIB, fd, "J9TR_bcloop_run_jni_native", J9_BCLOOP_RUN_JNI_NATIVE) |
			writeConstant(OMRPORTLIB, fd, "J9TR_bcloop_run_method", J9_BCLOOP_RUN_METHOD) |
			writeConstant(OMRPORTLIB, fd, "J9TR_bcloop_run_method_compiled", J9_BCLOOP_RUN_METHOD_COMPILED) |
			writeConstant(OMRPORTLIB, fd, "J9TR_bcloop_run_method_handle", J9_BCLOOP_RUN_METHOD_HANDLE) |
			writeConstant(OMRPORTLIB, fd, "J9TR_bcloop_run_method_handle_compiled", J9_BCLOOP_RUN_METHOD_HANDLE_COMPILED) |
			writeConstant(OMRPORTLIB, fd, "J9TR_bcloop_run_method_interpreted", J9_BCLOOP_RUN_METHOD_INTERPRETED) |
			writeConstant(OMRPORTLIB, fd, "J9TR_bcloop_stack_overflow", J9_BCLOOP_STACK_OVERFLOW) |
			writeConstant(OMRPORTLIB, fd, "J9TR_bcloop_throw_current_exception", J9_BCLOOP_THROW_CURRENT_EXCEPTION) |
			writeConstant(OMRPORTLIB, fd, "J9TR_bcloop_enter_method_monitor", J9_BCLOOP_ENTER_METHOD_MONITOR) |
			writeConstant(OMRPORTLIB, fd, "J9TR_bcloop_report_method_enter", J9_BCLOOP_REPORT_METHOD_ENTER) |
			writeConstant(OMRPORTLIB, fd, "J9TR_bcloop_exit_interpreter", J9_BCLOOP_EXIT_INTERPRETER) |
			writeConstant(OMRPORTLIB, fd, "J9TR_bcloop_reenter_interpreter", J9_BCLOOP_REENTER_INTERPRETER) |
#if JAVA_SPEC_VERSION >= 24
			writeConstant(OMRPORTLIB, fd, "J9TR_bcloop_yield_monent", J9_BCLOOP_YIELD_FOR_JIT_MONENT) |
#endif /* JAVA_SPEC_VERSION >= 24 */
			writeConstant(OMRPORTLIB, fd, "J9TR_MethodNotCompiledBit", J9_STARTPC_NOT_TRANSLATED) |
			writeConstant(OMRPORTLIB, fd, "J9TR_InterpVTableOffset", J9JIT_INTERP_VTABLE_OFFSET) |
			writeConstant(OMRPORTLIB, fd, "J9TR_RequiredClassAlignment", J9_REQUIRED_CLASS_ALIGNMENT) |
			writeConstant(OMRPORTLIB, fd, "J9TR_RequiredClassAlignmentInBits", J9_REQUIRED_CLASS_SHIFT) |
			writeConstant(OMRPORTLIB, fd, "J9TR_pointerSize", sizeof(UDATA)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_ELSSize", sizeof(J9VMEntryLocalStorage)) |
			writeConstant(OMRPORTLIB, fd, "J9TR_J9_EXTENDED_RUNTIME_DEBUG_MODE", J9_EXTENDED_RUNTIME_DEBUG_MODE) |
			writeConstant(OMRPORTLIB, fd, "J9TR_J9_EXTENDED_RUNTIME_USE_VECTOR_REGISTERS", J9_EXTENDED_RUNTIME_USE_VECTOR_REGISTERS) |
			writeConstant(OMRPORTLIB, fd, "J9TR_J9_EXTENDED_RUNTIME3_USE_VECTOR_LENGTH_256", J9_EXTENDED_RUNTIME3_USE_VECTOR_LENGTH_256) |
			writeConstant(OMRPORTLIB, fd, "J9TR_J9_EXTENDED_RUNTIME3_USE_VECTOR_LENGTH_512", J9_EXTENDED_RUNTIME3_USE_VECTOR_LENGTH_512) |
			writeConstant(OMRPORTLIB, fd, "J9TR_J9_EXTENDED_RUNTIME2_COMPRESS_OBJECT_REFERENCES", J9_EXTENDED_RUNTIME2_COMPRESS_OBJECT_REFERENCES) |
			writeConstant(OMRPORTLIB, fd, "J9TR_J9_INLINE_JNI_MAX_ARG_COUNT", J9_INLINE_JNI_MAX_ARG_COUNT) |

			/* Flags for iTable offset in resolved interface snippet data */
			writeConstant(OMRPORTLIB, fd, "J9TR_J9_ITABLE_OFFSET_DIRECT", J9_ITABLE_OFFSET_DIRECT) |
			writeConstant(OMRPORTLIB, fd, "J9TR_J9_ITABLE_OFFSET_VIRTUAL", J9_ITABLE_OFFSET_VIRTUAL) |
			writeConstant(OMRPORTLIB, fd, "J9TR_J9_ITABLE_OFFSET_TAG_BITS", J9_ITABLE_OFFSET_TAG_BITS) |

			/* Flag for Nestmates invokeVirtual on private methods */
			writeConstant(OMRPORTLIB, fd, "J9TR_J9_VTABLE_INDEX_DIRECT_METHOD_FLAG", J9_VTABLE_INDEX_DIRECT_METHOD_FLAG);

	if (0 != err) {
		rc = JNI_ERR;
	}
	return rc;
}

jint
writeJitConstants(OMRPortLibrary *OMRPORTLIB)
{
	jint rc = JNI_OK;
	IDATA fd = -1;

	values = 0;
	omrtty_printf("Creating file %s\n", jilconsts);
	fd = omrfile_open(jilconsts, EsOpenWrite | EsOpenCreate | EsOpenTruncate, 0666);
	if (fd == -1) {
		omrtty_printf("ERROR: Failed to open output file\n");
		rc = JNI_ERR;
		goto done;
	}
	rc = writeConstants(OMRPORTLIB, fd);
	if (JNI_OK != rc) {
		omrtty_printf("ERROR: Failed to write constants\n");
		omrfile_close(fd);
		goto done;
	}
#if defined(J9VM_ARCH_X86)
	rc = writeMacros(OMRPORTLIB, fd);
	if (JNI_OK != rc) {
		omrtty_printf("ERROR: Failed to write macros\n");
		omrfile_close(fd);
		goto done;
	}
#endif /* J9VM_ARCH_X86 */
	if (0 != omrfile_close(fd)) {
		omrtty_printf("ERROR: Failed to close output file\n");
		rc = JNI_ERR;
		goto done;
	}

	values = 1;
	omrtty_printf("Creating file %s\n", jilvalues);
	fd = omrfile_open(jilvalues, EsOpenWrite | EsOpenCreate | EsOpenTruncate, 0666);
	if (fd == -1) {
		omrtty_printf("ERROR: Failed to open output file\n");
		rc = JNI_ERR;
		goto done;
	}
	rc = writeHeader(OMRPORTLIB, fd);
	if (JNI_OK != rc) {
		omrtty_printf("ERROR: Failed to write header\n");
		omrfile_close(fd);
		goto done;
	}
	rc = writeConstants(OMRPORTLIB, fd);
	if (JNI_OK != rc) {
		omrtty_printf("ERROR: Failed to write constants\n");
		omrfile_close(fd);
		goto done;
	}
	if (0 != omrfile_close(fd)) {
		omrtty_printf("ERROR: Failed to close output file\n");
		rc = JNI_ERR;
		goto done;
	}
done:
	return rc;
}

#if defined(WIN32)
int
wmain(int argc, wchar_t ** argv, wchar_t ** envp)
#else /* defined(WIN32) */
int
main(int argc, char ** argv, char ** envp)
#endif /* defined(WIN32) */
{
	OMRPortLibrary j9portLibrary;
	int rc = 257;

	if (0 == omrthread_init_library()) {
		omrthread_t attachedThread = NULL;
		if (0 == omrthread_attach_ex(&attachedThread, J9THREAD_ATTR_DEFAULT)) {
			/* Use portlibrary version which we compiled against, and have allocated space
			 * for on the stack. This version may be different from the one in the linked DLL.
			 */
			if (0 == omrport_init_library(&j9portLibrary, sizeof(j9portLibrary))) {
				if (JNI_OK == writeJitConstants(&j9portLibrary)) {
					rc = 0;
				}
				j9portLibrary.port_shutdown_library(&j9portLibrary);
				omrthread_detach(attachedThread);
				omrthread_shutdown_library();
			}
		}
	}

	return rc;
}
