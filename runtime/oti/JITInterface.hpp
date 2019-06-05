/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#if !defined(JITINTERFACE_HPP_)
#define JITINTERFACE_HPP_

#include "j9port.h"

/* TODO: Implement jitVTableIndex() to remove this #include */
#if defined(J9VM_ARCH_AARCH64)
#include "assert.h"
#endif

extern "C" {

#if defined(J9VM_ARCH_X86)

#if defined(J9VM_ENV_DATA64)

/* x86-64 */

typedef struct {
	union {
		UDATA numbered[16];
		struct {
			UDATA rax;
			UDATA rbx;
			UDATA rcx;
			UDATA rdx;
			UDATA rdi;
			UDATA rsi;
			UDATA rbp;
			UDATA rsp;
			UDATA r8;
			UDATA r9;
			UDATA r10;
			UDATA r11;
			UDATA r12;
			UDATA r13;
			UDATA r14;
			UDATA r15;
		} named;
	} gpr;
	U_64 fpr[16];
} J9JITRegisters;

#else /*J9VM_ENV_DATA64 */

/* x86-32*/

typedef struct {
	union {
		UDATA numbered[8];
		struct {
			UDATA eax;
			UDATA ebx;
			UDATA ecx;
			UDATA edx;
			UDATA edi;
			UDATA esi;
			UDATA ebp;
			UDATA esp;
		} named;
	} gpr;
	U_64 fpr[8];
} J9JITRegisters;

#endif /* J9VM_ENV_DATA64 */

#elif defined(J9VM_ARCH_POWER)

typedef struct {
	union {
		UDATA numbered[32];
	} gpr;
	U_64 fpr[32];
	UDATA cr;
	UDATA lr;
} J9JITRegisters;

#elif defined(J9VM_ARCH_S390)

typedef struct {
	union {
#if defined(J9VM_JIT_32BIT_USES64BIT_REGISTERS)
		UDATA numbered[32];
#else
		UDATA numbered[16];
#endif
	} gpr;
	U_64 fpr[16];
} J9JITRegisters;

#elif defined(J9VM_ARCH_ARM)

typedef struct {
	union {
		UDATA numbered[16];
	} gpr;
	U_64 fpr[16];
} J9JITRegisters;

#elif defined(J9VM_ARCH_AARCH64)

typedef struct {
	union {
		UDATA numbered[32];
	} gpr;
	U_64 fpr[32];
} J9JITRegisters;

#else
#error UNKNOWN PROCESSOR
#endif

} /* extern "C" */

class VM_JITInterface
{
/*
 * Data members
 */
private:
protected:
public:

/*
 * Function members
 */
private:
protected:
public:

	static VMINLINE UDATA
	jitVTableIndex(void *jitReturnAddress, UDATA interfaceVTableIndex)
	{
		UDATA jitVTableIndex = interfaceVTableIndex;
#if defined(J9VM_ARCH_X86)
		if (0xE8 != ((U_8*)jitReturnAddress)[-5]) {
			/* Virtual call - decode from instruction */
			jitVTableIndex = (UDATA)(IDATA)(((I_32*)jitReturnAddress)[-1]);
		}
#elif defined(J9VM_ARCH_POWER)
		if (0x7D8903A6 == ((U_32*)jitReturnAddress)[-2]) {
			U_32 minus3 = ((U_32*)jitReturnAddress)[-3];
			/* always fetch low 16 bits of vTable index from virtual send */
			IDATA methodIndex = (IDATA)(I_16)minus3;
			/* determine if we are a >32k send */
			U_32 minus4 = ((U_32*)jitReturnAddress)[-4];
			if (0x3D800000 == (minus4 & 0xFFE00000)) {
				if (0x000C0000 == (minus3 & 0x001F0000)) {
					/* fetch high 16 bits of vTable index from virtual send and merge */
					IDATA indexHigh = (IDATA)(I_16)minus4;
					methodIndex += (indexHigh << 16);
				}
			}
			jitVTableIndex = (UDATA)methodIndex;
		}
#elif defined(J9VM_ARCH_ARM)
		/* Virtual send layout is:
		 *	...
		 *	ldr r15,[register - vTableOffset]
		 *	<- return address points here
		 *
		 * Interface send layout is:
		 *	...
		 *	b snippet
		 *	<- return address points here
		 */
		UDATA instruction = ((U_32*)jitReturnAddress)[-1];
		if (0x0410F000 == (instruction & 0x0E10F000)) {
			/* Virtual - unsigned offset is in the low 12 bits, assume the sign bit is set (i.e. the offset is always negative) */
			jitVTableIndex = 0 - (instruction & 0x00000FFF);
		}
#elif defined(J9VM_ARCH_AARCH64)
		/* TODO: Implement this */
		assert(0);
#elif defined(J9VM_ARCH_S390)
		/* The vtable index is always in the register */
#else
#error UNKNOWN PROCESSOR
#endif
		return jitVTableIndex;
	}

	static VMINLINE void*
	peekJITReturnAddress(J9VMThread *vmThread, UDATA *sp)
	{
#if defined(J9VM_ARCH_X86)
		return *(void**)sp;
#elif defined(J9VM_ARCH_POWER)
		return (void*)((J9JITRegisters*)vmThread->entryLocalStorage->jitGlobalStorageBase)->lr;
#elif defined(J9VM_ARCH_ARM)
		return (void*)((J9JITRegisters*)vmThread->entryLocalStorage->jitGlobalStorageBase)->gpr.numbered[14];
#elif defined(J9VM_ARCH_AARCH64)
		return (void*)((J9JITRegisters*)vmThread->entryLocalStorage->jitGlobalStorageBase)->gpr.numbered[30]; // LR
#elif defined(J9VM_ARCH_S390)
		return (void*)((J9JITRegisters*)vmThread->entryLocalStorage->jitGlobalStorageBase)->gpr.numbered[14];
#else
#error UNKNOWN PROCESSOR
#endif
	}

	static VMINLINE void
	restoreJITReturnAddress(J9VMThread *vmThread, UDATA * &sp, void *returnAddress)
	{
#if defined(J9VM_ARCH_X86)
		*--sp = (UDATA)returnAddress;
#elif defined(J9VM_ARCH_POWER)
		((J9JITRegisters*)vmThread->entryLocalStorage->jitGlobalStorageBase)->lr = (UDATA)returnAddress;
#elif defined(J9VM_ARCH_ARM)
		((J9JITRegisters*)vmThread->entryLocalStorage->jitGlobalStorageBase)->gpr.numbered[14] = (UDATA)returnAddress;
#elif defined(J9VM_ARCH_AARCH64)
		((J9JITRegisters*)vmThread->entryLocalStorage->jitGlobalStorageBase)->gpr.numbered[30] = (UDATA)returnAddress; // LR
#elif defined(J9VM_ARCH_S390)
		((J9JITRegisters*)vmThread->entryLocalStorage->jitGlobalStorageBase)->gpr.numbered[14] = (UDATA)returnAddress;
#else
#error UNKNOWN PROCESSOR
#endif
	}

	static VMINLINE void*
	fetchJITReturnAddress(J9VMThread *vmThread, UDATA * &sp)
	{
		void *returnAddress = peekJITReturnAddress(vmThread, sp);
		sp += J9SW_JIT_STACK_SLOTS_USED_BY_CALL;
		return returnAddress;
	}

	static VMINLINE void
	enableRuntimeInstrumentation(J9VMThread *currentThread)
	{
#if defined(J9VM_JIT_RUNTIME_INSTRUMENTATION)
		if (J9_ARE_ANY_BITS_SET(currentThread->jitCurrentRIFlags, J9_JIT_TOGGLE_RI_ON_TRANSITION)) {
			PORT_ACCESS_FROM_VMC(currentThread);
			j9ri_enable(NULL);
		}
#endif /* J9VM_JIT_RUNTIME_INSTRUMENTATION */
	}

	static VMINLINE void
	disableRuntimeInstrumentation(J9VMThread *currentThread)
	{
#if defined(J9VM_JIT_RUNTIME_INSTRUMENTATION)
		if (J9_ARE_ANY_BITS_SET(currentThread->jitCurrentRIFlags, J9_JIT_TOGGLE_RI_ON_TRANSITION)) {
			PORT_ACCESS_FROM_VMC(currentThread);
			j9ri_disable(NULL);
		}
#endif /* J9VM_JIT_RUNTIME_INSTRUMENTATION */
	}

};

#endif /* JITINTERFACE_HPP_ */
