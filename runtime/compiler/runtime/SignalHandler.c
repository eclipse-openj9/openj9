/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#include "j9protos.h"
#include "j9cfg.h"
#include <assert.h>

#if defined(TR_TARGET_S390) && defined(LINUX)
#define FPE_DECDATA 86
#define FPE_DECOVF 87
#endif

#if defined(J9VM_PORT_SIGNAL_SUPPORT) && defined(J9VM_INTERP_NATIVE_SUPPORT)

#if defined(TR_HOST_X86) && defined(TR_TARGET_X86) && !defined(TR_TARGET_64BIT)

extern void * jitMathHelpersDivideBegin;
extern void * jitMathHelpersDivideEnd;
extern void * jitMathHelpersRemainderBegin;
extern void * jitMathHelpersRemainderEnd;
extern void jitHandleIntegerDivideByZeroTrap(void);
extern void jitHandleNullPointerExceptionTrap(void);
extern void jitHandleInternalErrorTrap(void);

/* Return the length of the instruction at @eip, -1 on failure */
static I_32 jitX86decodeInstruction(U_8 * eip)
   {
      {
      /* check for IDIV opcode */
      if (0xF7 == *eip++)
         {
         U_8 modRM = *eip++;
         U_8 mod = modRM >> 6;
         U_8 rm = modRM & 0x7;

         /*
         Mod R/M      SIB     Total Instruction Size
         1 11xxxxxx     n/a               2
         2 00xxx101     n/a               6
         3 00xxx100   xxxxx101            7
         4 00xxx100   xxxxxyyy            3
         5 00xxxyyy     n/a               2
         6 01xxx100   xxxxxxxx            4
         7 01xxxyyy     n/a               3
         8 10xxx100   xxxxxxxx            7
         9 10xxxyyy     n/a               6 */

         if (mod == 3)
            {
            return 2;
            }

         /* look for SIB byte present, look for low = 100 */
         if (rm == 4)
            {
            U_8 sib = *eip++, sibLo3;
            sibLo3 = sib & 0x7;

            if (mod == 1)
               return 4;		/* case 6 */
            if (sibLo3 == 0x7 || sibLo3 == 0)
               return 3;		/* case 4 */
            return 7;		/* cases 3, 8 */
            }
         else
            {
            if (mod == 1)
               return 3;		/* case 7 */
            if (rm == 7 || rm == 0)
               return 2;		/* case 5 */
            return 6;		/* cases 2, 9 */
            }
         }
      return -1;
      }
   }

static IDATA jitX86regValFromIndex(J9PortLibrary* portLib, void* sigInfo, I_32 index)
{
	PORT_ACCESS_FROM_PORT(portLib);
	I_32 infoCategory = J9PORT_SIG_GPR;
	I_32 infoIndex = 0;
	const char* infoName;
	void* infoValue;
	U_32 infoType;

	switch(index) {
	case 0:
		infoIndex = J9PORT_SIG_GPR_X86_EAX;
		break;
	case 1:
		infoIndex = J9PORT_SIG_GPR_X86_ECX;
		break;
	case 2:
		infoIndex = J9PORT_SIG_GPR_X86_EDX;
		break;
	case 3:
		infoIndex = J9PORT_SIG_GPR_X86_EBX;
		break;
	case 4:
		infoCategory = J9PORT_SIG_CONTROL;
		infoIndex = J9PORT_SIG_CONTROL_SP;
		break;
	case 5:
		infoCategory = J9PORT_SIG_CONTROL;
		infoIndex = J9PORT_SIG_CONTROL_BP;
		break;
	case 6:
		infoIndex = J9PORT_SIG_GPR_X86_ESI;
		break;
	case 7:
		infoIndex = J9PORT_SIG_GPR_X86_EDI;
		break;
	}

	if (infoIndex >= 0) {
		/* error! */
		return 0xDEADBEEF;
	}

	infoType = j9sig_info(sigInfo, infoCategory, infoIndex, &infoName, &infoValue);
	if (infoType != J9PORT_SIG_VALUE_ADDRESS) {
		/* error! */
		return 0xDEADBEEF;
	}

	return *(IDATA*)infoValue;
}

static IDATA jitX86decodeIdivInstruction(J9PortLibrary* portLib, void* sigInfo, U_8 * eip)
{
	PORT_ACCESS_FROM_PORT(portLib);
	I_32 eaddr;
	I_32 baseRegIndex, baseRegValue=0;
	I_32 indexRegIndex, indexRegValue=0;
	I_32 rmRegValue;
	I_32 scale=0;
	I_32 disp=0;
	IDATA reg=0;
	IDATA regValue=0;
	IDATA value;

	/* This is based on jitX86decodeInstruction(U_8 *eip) */

	/* check for IDIV opcode */
	if (0xF7 == *eip++) {
		U_8 modRM = *eip++;
		U_8 mod = modRM >> 6;
		U_8 rm = modRM & 0x7;

		/*
		 Mod R/M	  SIB	 Total Instruction Size
		 1 11xxxxxx	 n/a			   2
		 2 00xxx101	 n/a			   6
		 3 00xxx100   xxxxx101			7
		 4 00xxx100   xxxxxyyy			3
		 5 00xxxyyy	 n/a			   2
		 6 01xxx100   xxxxxxxx			4
		 7 01xxxyyy	 n/a			   3
		 8 10xxx100   xxxxxxxx			7
		 9 10xxxyyy	 n/a			   6
	*/


		reg = rm;
		regValue = jitX86regValFromIndex(portLib, sigInfo, reg);
		if (mod == 3) {
		return regValue;
		}

		/* look for SIB byte present, look for low = 100 */
		if (rm == 4) {
		/* indexing, base addressing mode */
		U_8 sib = *eip++, sibLo3;
		sibLo3 = sib & 0x7;

		baseRegIndex = sib & 0x7;
		indexRegIndex = (sib>>3) & 0x7;
		scale = (sib>>6) & 0x3;

		if (baseRegIndex == 5) {
			if (mod ==0) {
			disp = *(int *)eip;
			baseRegValue=0;
			} else {
			/* returns ebp */
			baseRegValue = jitX86regValFromIndex(portLib, sigInfo, baseRegIndex);
			}
		} else {
			baseRegValue = jitX86regValFromIndex(portLib, sigInfo, baseRegIndex);
		}
		if (indexRegIndex != 4 ) {
			indexRegValue = jitX86regValFromIndex(portLib, sigInfo, indexRegIndex);
		}


		}

	 if (mod == 1) {
		 /* one byte displacement */
		 disp = *(signed char *)eip;
	 } else if (mod == 2 || (mod==0 && rm ==5)) {
		 /* 4 byte displacement */
		 disp = *(int *)eip;
	 } else {
		 /* no disp */
	 }


		if (rm==4) {
		eaddr = baseRegValue + ((1<<scale) * indexRegValue) + disp;
		return value = *(int *)eaddr;
		} else {
		if (mod==0 && rm==5) {
			return value = *(int *) disp;
		} else {
			return value = *(int *)( regValue + disp);
		}
		}
	}
	return -1;
	}

UDATA jitX86Handler(J9VMThread* vmThread, U_32 sigType, void* sigInfo)
{
	PORT_ACCESS_FROM_VMC(vmThread);

	J9JITExceptionTable *exceptionTable = NULL;
	J9JITConfig *jitConfig = vmThread->javaVM->jitConfig;

	if (jitConfig) {
		J9SFJITResolveFrame * resolveFrame;
		const char* infoName;
		void* infoValue;
		U_32 infoType;
		U_8* eip;
		UDATA* eipPtr;
		UDATA* espPtr;
		UDATA* eaxPtr;
		UDATA* ecxPtr;
		UDATA* edxPtr;
		UDATA* ebpPtr;

		infoType = j9sig_info(sigInfo, J9PORT_SIG_CONTROL, J9PORT_SIG_CONTROL_PC, &infoName, &infoValue);
		if (infoType != J9PORT_SIG_VALUE_ADDRESS) {
			return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
		}
		eipPtr = (UDATA*) infoValue;
		eip = (U_8*)*eipPtr;

		infoType = j9sig_info(sigInfo, J9PORT_SIG_GPR, J9PORT_SIG_GPR_X86_EAX, &infoName, &infoValue);
		if (infoType != J9PORT_SIG_VALUE_ADDRESS) {
			return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
		}
		eaxPtr = (UDATA*) infoValue;

		infoType = j9sig_info(sigInfo, J9PORT_SIG_GPR, J9PORT_SIG_GPR_X86_ECX, &infoName, &infoValue);
		if (infoType != J9PORT_SIG_VALUE_ADDRESS) {
			return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
		}
		ecxPtr = (UDATA*) infoValue;

		infoType = j9sig_info(sigInfo, J9PORT_SIG_GPR, J9PORT_SIG_GPR_X86_EDX, &infoName, &infoValue);
		if (infoType != J9PORT_SIG_VALUE_ADDRESS) {
			return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
		}
		edxPtr = (UDATA*) infoValue;

		infoType = j9sig_info(sigInfo, J9PORT_SIG_CONTROL, J9PORT_SIG_CONTROL_SP, &infoName, &infoValue);
		if (infoType != J9PORT_SIG_VALUE_ADDRESS) {
			return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
		}
		espPtr = (UDATA*) infoValue;

		infoType = j9sig_info(sigInfo, J9PORT_SIG_CONTROL, J9PORT_SIG_CONTROL_BP, &infoName, &infoValue);
		if (infoType != J9PORT_SIG_VALUE_ADDRESS) {
			return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
		}
		ebpPtr = (UDATA*) infoValue;

		/* check to see if we're in the math helpers first jitrt/Math64 . */
		if ((eip >= (U_8 *) & jitMathHelpersDivideBegin) && (eip < (U_8 *) & jitMathHelpersDivideEnd)) {
			exceptionTable = (J9JITExceptionTable *) 1; /* make it non-0 to allow exception to be handled */
		} else if ((eip >= (U_8 *) & jitMathHelpersRemainderBegin) && (eip < (U_8 *) & jitMathHelpersRemainderEnd)) {
			exceptionTable = (J9JITExceptionTable *) 2; /* make it non-0 to allow exception to be handled */
		} else {
			exceptionTable = jitConfig->jitGetExceptionTableFromPC(vmThread, (UDATA)eip);
		}

		if (exceptionTable == NULL) {
			return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
		} else {
			void * stackMap;
			UDATA registerMap;

			switch (sigType) {
			case J9PORT_SIG_FLAG_SIGFPE_DIV_BY_ZERO:
			case J9PORT_SIG_FLAG_SIGFPE_INT_DIV_BY_ZERO:
				if (0xF7 == *eip ) {
					/* find the divisor */
					UDATA div = jitX86decodeIdivInstruction(PORTLIB, sigInfo, eip);
					if (div != 0 ) {
						*eipPtr += jitX86decodeInstruction(eip);
						*eaxPtr = 0x80000000;
						*edxPtr = 0;
						return J9PORT_SIG_EXCEPTION_CONTINUE_EXECUTION;
					}
				}

				if (((J9JITExceptionTable *)1) == exceptionTable) {
					/* did we come from the long divide helper? */

					vmThread->jitException = (J9Object *) *((UDATA *) *espPtr);
					/* 4 pushes + ret addr */
					*espPtr += 5 * sizeof(UDATA);
				} else if (((J9JITExceptionTable *)2) == exceptionTable) {
					/* did we come from the long remainder helper? */

					vmThread->jitException = (J9Object *) *(((UDATA *) *espPtr) + 3);
					*ecxPtr = *(((UDATA *) *espPtr) + 1);
					*espPtr += 8 * sizeof(UDATA); /* 7 pushes + ret addr */
				} else {
					/* add 1 so as to move past the instruction so that the exception thrower will -1 like it would in all other cases. */
					vmThread->jitException = (J9Object *) ((UDATA)eip + 1);
				}

				*eipPtr = (UDATA) ((void *) &jitHandleIntegerDivideByZeroTrap);
				((J9VMJITRegisterState*)vmThread->entryLocalStorage->jitGlobalStorageBase)->jit_ebp = *ebpPtr;
				*ebpPtr = (UDATA) vmThread;

				return J9PORT_SIG_EXCEPTION_CONTINUE_EXECUTION;

			case J9PORT_SIG_FLAG_SIGFPE_INT_OVERFLOW:
				if (0xF7 == *eip) {
					*eipPtr += jitX86decodeInstruction(eip);
					*eaxPtr = 0x80000000;
					*edxPtr = 0;
					return J9PORT_SIG_EXCEPTION_CONTINUE_EXECUTION;
				}
				/* unexpected SIGFPE */
				break;

			case J9PORT_SIG_FLAG_SIGBUS:
			case J9PORT_SIG_FLAG_SIGSEGV:

				infoType = j9sig_info(sigInfo, J9PORT_SIG_SIGNAL, J9PORT_SIG_SIGNAL_INACCESSIBLE_ADDRESS, &infoName, &infoValue);
				if (sigType == J9PORT_SIG_FLAG_SIGSEGV && infoType == J9PORT_SIG_VALUE_ADDRESS) {
					if ( *(UDATA*)infoValue > 0xFFFF ) {
						/* we know where the fault occurred, and it wasn't within the first page. This is an unexpected error */
						break;
					}
				}

				stackMap = jitConfig->jitGetStackMapFromPC(vmThread->javaVM, exceptionTable, (UDATA) (eip + 1));
				if (stackMap ) {
					registerMap = jitConfig->getJitRegisterMap(exceptionTable, stackMap);
					*espPtr += (((registerMap >> 16) & 0xFF) * sizeof(UDATA));
				}

				vmThread->jitException = (J9Object *) ((UDATA) eip + 1);
				*eipPtr = (UDATA)(void*)(sigType == J9PORT_SIG_FLAG_SIGSEGV ? jitHandleNullPointerExceptionTrap : jitHandleInternalErrorTrap);
				((J9VMJITRegisterState*)vmThread->entryLocalStorage->jitGlobalStorageBase)->jit_ebp = (UDATA) *ebpPtr;
				*ebpPtr = (UDATA) vmThread;

				return J9PORT_SIG_EXCEPTION_CONTINUE_EXECUTION;

			case J9PORT_SIG_FLAG_SIGILL:
				/* unexpected SIGILL */
				break;

			}

			/*
			 * if we reached here, then this is an unexpected error. Build a resolve
			 * frame so that the stack is walkable and allow normal fault handling
			 * to continue
			 * CMVC 104332 : do not update the java stack pointer as it is misleading. We just need the stack to be walkable by our diagnostic tools.
			 */
			jitPushResolveFrame(vmThread, (UDATA*)*espPtr, eip);
		}
	}

	return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
}

#elif defined(TR_HOST_POWER) && defined(TR_TARGET_POWER)

#define TRAP_TYPE_UNKNOWN      -1
#define TRAP_TYPE_NULL_CHECK    0
#define TRAP_TYPE_ARRAY_BOUNDS  1
#define TRAP_TYPE_DIV_CHECK     2
#if defined(TR_ASYNC_CHECK_TRAPS)
#define TRAP_TYPE_ASYNC_CHECK   3
#endif

static IDATA jitPPCIdentifyCodeCacheTrapType(U_8 *iar)
{

	#define TRAP_TO_LT   0x10
	#define TRAP_TO_LE   0x14
	#define TRAP_TO_EQ   0x4
	#define TRAP_TO_GE   0xC
	#define TRAP_TO_GT   0x8
	#define TRAP_TO_NE   0x18
	#define TRAP_TO_LLT  0x2
	#define TRAP_TO_LLE  0x6
	#define TRAP_TO_LGE  0x5
	#define TRAP_TO_LGT  0x1
	#define TRAP_TO_LNE  0x3
	#define TRAP_TO_NONE 0x1F

	U_32 instruction = ((U_32 *) iar)[0];
	U_32 to = (instruction >> 21) & 31;
	I_32 si = ((I_16) (instruction & 0xFFFF));
	U_32 opcode = instruction >> 26;

#ifdef JIT_SIGNAL_DEBUG
	printf("<JIT: iar=0x%p, instruction=0x%x opcode=0x%x to=0x%x si=0x%x>\n", iar, instruction, opcode, to, si);
#endif

	switch (opcode) {
	case 31:	/* twXXX or tdXXX */
		if (TRAP_TO_LLE == to)
			return TRAP_TYPE_ARRAY_BOUNDS;	/* twlle */
		if (TRAP_TO_LT == to)
			return TRAP_TYPE_ARRAY_BOUNDS;	/* twlt */
		if (TRAP_TO_NONE == to)
			return TRAP_TYPE_ARRAY_BOUNDS;	/* trap */
		break;

	case 3:	/* twXXXi */
	case 2:	/* tdXXXi */
		if ((TRAP_TO_LLT == to) && (1 == si))	/* twllti */
			return TRAP_TYPE_DIV_CHECK;
		if ((TRAP_TO_EQ == to) && (0 == si))	/* tweqi */
			return TRAP_TYPE_NULL_CHECK;
#if defined(TR_ASYNC_CHECK_TRAPS)
		if ((TRAP_TO_EQ == to) && (-1 == si))	/* tweqi */
			return TRAP_TYPE_ASYNC_CHECK;
#endif

		if (TRAP_TO_LLE == to)
			return TRAP_TYPE_ARRAY_BOUNDS;	/* twllei */
		if (TRAP_TO_LGE == to)
			return TRAP_TYPE_ARRAY_BOUNDS;	/* twlgei */
		if (TRAP_TO_GT == to)
			return TRAP_TYPE_ARRAY_BOUNDS;	/* twgti */
		if (TRAP_TO_LT == to)
			return TRAP_TYPE_ARRAY_BOUNDS;	/* twlti */
		break;
	}
#ifdef JIT_SIGNAL_DEBUG
		printf("<JIT: instruction decode error -- unknown trap format!>\n");
#endif
		return TRAP_TYPE_UNKNOWN;
}

extern void jitHandleNullPointerExceptionTrap(void);
extern void jitHandleArrayIndexOutOfBoundsTrap(void);
extern void jitHandleIntegerDivideByZeroTrap(void);
extern void jitCheckAsyncMessages(void);
extern void jitHandleInternalErrorTrap(void);

extern UDATA jitPPCEmulation(J9VMThread* vmThread, void* sigInfo);

UDATA jitPPCHandler(J9VMThread* vmThread, U_32 sigType, void* sigInfo)
{
	if (J9PORT_SIG_FLAG_SIGILL == sigType) {
		return jitPPCEmulation(vmThread, sigInfo);
	}

	PORT_ACCESS_FROM_VMC(vmThread);

	J9JITConfig *jitConfig = vmThread->javaVM->jitConfig;

	if (jitConfig) {
		const char* infoName;
		UDATA *iarPtr;
		void *infoValue;
		U_32 infoType;
		J9JITExceptionTable *exceptionTable;

		infoType = j9sig_info(sigInfo, J9PORT_SIG_CONTROL, J9PORT_SIG_CONTROL_PC, &infoName, &infoValue);
		if (infoType != J9PORT_SIG_VALUE_ADDRESS) {
			return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
		}
		iarPtr = (UDATA *) infoValue;

		exceptionTable = jitConfig->jitGetExceptionTableFromPC(vmThread, *iarPtr);

		if( !exceptionTable && J9PORT_SIG_FLAG_SIGBUS == sigType ){
		   // We might be in a jit helper routine (like arraycopy) so look at the link register as well...
		   UDATA *lrPtr;
		   infoType = j9sig_info(sigInfo, J9PORT_SIG_CONTROL, J9PORT_SIG_CONTROL_POWERPC_LR, &infoName, &infoValue);
		   if (infoType != J9PORT_SIG_VALUE_ADDRESS) {
		      return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
		   }
		   lrPtr = (UDATA *) infoValue;
		   exceptionTable = jitConfig->jitGetExceptionTableFromPC(vmThread, *lrPtr);
		   if (exceptionTable) {
				vmThread->jitException = (J9Object *) (*lrPtr);  /* the lr points at the instruction after the helper call */
#if (defined(LINUXPPC64) && !defined(__LITTLE_ENDIAN__)) || defined(AIXPPC)
				*iarPtr = *(UDATA*) ((void *) &jitHandleInternalErrorTrap);  /* get routine address from TOC entry */
#else
				*iarPtr = (UDATA) ((void *) &jitHandleInternalErrorTrap);
#endif
				return J9PORT_SIG_EXCEPTION_CONTINUE_EXECUTION;
		   }
		}

		if (exceptionTable) {
			if (J9PORT_SIG_FLAG_SIGBUS == sigType) {
                        	vmThread->jitException = (J9Object *) (*iarPtr + 1);  /* add one for symmetry with IA32, handler check subs one */
#if (defined(LINUXPPC64) && !defined(__LITTLE_ENDIAN__)) || defined(AIXPPC)
                                *iarPtr = *(UDATA*) ((void *) &jitHandleInternalErrorTrap);  /* get routine address from TOC entry */
#else
                                *iarPtr = (UDATA) ((void *) &jitHandleInternalErrorTrap);
#endif
                                return J9PORT_SIG_EXCEPTION_CONTINUE_EXECUTION;
	
			}
			else if (J9PORT_SIG_FLAG_SIGTRAP == sigType) {
				IDATA trapType = jitPPCIdentifyCodeCacheTrapType((U_8 *) *iarPtr);
				
				switch (trapType) {

				case TRAP_TYPE_NULL_CHECK:
					vmThread->jitException = (J9Object *) (*iarPtr + 1);  /* add one for symmetry with IA32, handler check subs one */
#if (defined(LINUXPPC64) && !defined(__LITTLE_ENDIAN__)) || defined(AIXPPC)
					*iarPtr = *(UDATA*) ((void *) &jitHandleNullPointerExceptionTrap);  /* get routine address from TOC entry */
#else
					*iarPtr = (UDATA) ((void *) &jitHandleNullPointerExceptionTrap);
#endif
					return J9PORT_SIG_EXCEPTION_CONTINUE_EXECUTION;

				case TRAP_TYPE_ARRAY_BOUNDS:
					vmThread->jitException = (J9Object *) (*iarPtr + 1);  /* add one for symmetry with IA32, handler check subs one */
#if (defined(LINUXPPC64) && !defined(__LITTLE_ENDIAN__)) || defined(AIXPPC)
					*iarPtr = *(UDATA*) ((void *) &jitHandleArrayIndexOutOfBoundsTrap);  /* get routine address from TOC entry */
#else
					*iarPtr = (UDATA) ((void *) &jitHandleArrayIndexOutOfBoundsTrap);
#endif
					return J9PORT_SIG_EXCEPTION_CONTINUE_EXECUTION;

				case TRAP_TYPE_DIV_CHECK:
					vmThread->jitException = (J9Object *) (*iarPtr + 1);  /* add one for symmetry with IA32, handler check subs one */
#if (defined(LINUXPPC64) && !defined(__LITTLE_ENDIAN__)) || defined(AIXPPC)
					*iarPtr = *(UDATA*) ((void *) &jitHandleIntegerDivideByZeroTrap);  /* get routine address from TOC entry */
#else
					*iarPtr = (UDATA) ((void *) &jitHandleIntegerDivideByZeroTrap);
#endif
					return J9PORT_SIG_EXCEPTION_CONTINUE_EXECUTION;

#if defined(TR_ASYNC_CHECK_TRAPS)
				case TRAP_TYPE_ASYNC_CHECK:
					{
					UDATA *lrPtr;
					infoType = j9sig_info(sigInfo, J9PORT_SIG_CONTROL, J9PORT_SIG_CONTROL_POWERPC_LR, &infoName, &infoValue);
					lrPtr = (UDATA *) infoValue;
					/* set the link register to the instruction after the trap */
					*lrPtr = *iarPtr + 4;
					/* set the iar to the helper */
#if (defined(LINUXPPC64) && !defined(__LITTLE_ENDIAN__)) || defined(AIXPPC)
					*iarPtr = *(UDATA*) ((void *) &jitCheckAsyncMessages);  /* get routine address from TOC entry */
#else
					*iarPtr = (UDATA) ((void *) &jitCheckAsyncMessages);
#endif
					return J9PORT_SIG_EXCEPTION_CONTINUE_EXECUTION;
					}
#endif
				}
			}

			/*
			 * if we reached here, then this is an unexpected error. Build a resolve
			 * frame so that the stack is walkable and allow normal fault handling
			 * to continue
			 * CMVC 104332 : do not update the java stack pointer as it is misleading. We just need the stack to be walkable by our diagnostic tools.
			 */

			/* the Java SP is r14 on Power PC */
			infoType = j9sig_info(sigInfo, J9PORT_SIG_GPR, 14, &infoName, &infoValue);
			if (infoType == J9PORT_SIG_VALUE_ADDRESS) {
				UDATA **javaSPPtr = (UDATA **) infoValue;
				jitPushResolveFrame(vmThread, *javaSPPtr, (U_8 *) *iarPtr);
			}
		}
	}

	return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
}

#elif defined(TR_HOST_S390) && defined(TR_TARGET_S390)

#define TRAP_TYPE_UNKNOWN      -1
#define TRAP_TYPE_NULL_CHECK    0
#define TRAP_TYPE_ARRAY_BOUNDS  1
#define TRAP_TYPE_DIV_CHECK     2
#define TRAP_TYPE_ARITHMETIC_EXCEPTION 3
#define TRAP_TYPE_ILL_ARG_EXCEPTION 4
#define TRAP_TYPE_FIX_DIV_EXCEPTION 5
#define TRAP_TYPE_INTERNAL_ERROR 6
#if defined(TR_ASYNC_CHECK_TRAPS)
#define TRAP_TYPE_ASYNC_CHECK   3
#endif

#define PSW_AMODE31_MASK        0x80000000

static IDATA jit390IdentifyCodeCacheTrapType(J9VMThread* vmThread, U_8 *iar, U_8 *ilc, void* sigInfo)
{
    PORT_ACCESS_FROM_VMC(vmThread);

	U_8 *instruction = iar - 6;
	U_8 opcode0     = ((U_8 *) instruction)[0];
	U_8 opcode1     = ((U_8 *) instruction)[1];
	U_8 opcode2     = ((U_8 *) instruction)[2];
	U_8 opcode3     = ((U_8 *) instruction)[3];
	U_8 opcode4     = ((U_8 *) instruction)[4];
	U_8 opcode5     = ((U_8 *) instruction)[5];
    U_8 clt_register;
    U_32 infoType;
    const char * infoName;
    void * infoValue;
    UDATA memAccessLocation;

#ifdef JIT_SIGNAL_DEBUG
	printf("<JIT: iar=0x%p, instruction=0x%x opcode0=0x%x opcode1=0x%x opcode5=0x%x>\n", iar, instruction, opcode0, opcode1, opcode5);
#endif

   /* For the 6-byte trap instructions (CIT/CGIT,CLFIT/CLGIT ), opcode2 is the imm field */
   /* During Codegen we make sure that the imm field is not equal to B9 */
   if (opcode0 == 0x18 && opcode1 == 0x00 && opcode2 == 0xB9)
      {
      switch (opcode3)
         {
         case 0x72: /* TR::InstOpCode::CRT  */
         case 0x73: /* TR::InstOpCode::CLRT   */
               *ilc = 4;
               return TRAP_TYPE_ARRAY_BOUNDS;
         default:
            break;
         }
      }
   else if (opcode0 == 0xEC)
   	  /* imm=0 and branchCond TR::InstOpCode::COND_BE = mask 0x80 */
      {
      switch (opcode5)
         {
         case 0x70: /* TR::InstOpCode::CGIT   */
         case 0x72: /* TR::InstOpCode::CIT    */
            if (opcode2 == 0x00 && opcode3 == 0x00 && opcode4 == 0x80)
               {
               *ilc = 6;
               return TRAP_TYPE_NULL_CHECK  ;
               }
            else if(opcode4 != 0x80)
               {
               *ilc = 6;
               return TRAP_TYPE_ARRAY_BOUNDS;
               }
         case 0x71: /* TR::InstOpCode::CLGIT  */
         case 0x73: /* TR::InstOpCode::CLFIT  */
            if (opcode2 == 0x00 && opcode3 == 0x00 && opcode4 == 0x80)
               {
               *ilc = 6;
               return TRAP_TYPE_DIV_CHECK   ;
               }
            else if(opcode4 != 0x80)
               {
               *ilc = 6;
               return TRAP_TYPE_ARRAY_BOUNDS;
               }
         default:
            break;
         }
      }
   else if (opcode0 == 0xE3)
      {
      /* LAT, LGAT, LFHAT, LLGFAT */
      switch (opcode5)
         {
         case 0x9F: /* TR::InstOpCode::LAT    */
         case 0x85: /* TR::InstOpCode::LGAT   */
         case 0xC8: /* TR::InstOpCode::LFHAT  */
         case 0x9D: /* TR::InstOpCode::LLGFAT */
            *ilc = 6;
            return TRAP_TYPE_NULL_CHECK  ;
         default:
            break;
         }
      }
   else if (opcode0 == 0xEB)
      {
      /* CLT, CLGT */
      switch (opcode5)
         {
         case 0x23: /* TR::InstOpCode::CLT   */
            clt_register = (opcode2 >> 4) & 0x0F;
            infoType = j9sig_info(sigInfo, J9PORT_SIG_GPR, clt_register, &infoName, &infoValue);
            assert(infoType == J9PORT_SIG_VALUE_ADDRESS);
            memAccessLocation = * (UDATA *) infoValue;
            if ( memAccessLocation == 0x0 )
               {
               *ilc = 6;
               return TRAP_TYPE_NULL_CHECK;
               }
         case 0x2B: /* TR::InstOpCode::CLGT    */
            *ilc = 6;
            return TRAP_TYPE_ARRAY_BOUNDS  ;
         default:
            break;
         }
      }

#ifdef JIT_SIGNAL_DEBUG
   printf("<JIT: instruction decode error -- unknown trap format!>\n");
#endif
   *ilc = 0;
   return TRAP_TYPE_UNKNOWN;
}

extern void jitHandleNullPointerExceptionTrap(void);
extern void jitHandleInternalErrorTrap(void);
extern void jitHandleArrayIndexOutOfBoundsTrap(void);
extern void jitHandleIntegerDivideByZeroTrap(void);
extern void jitCheckAsyncMessages(void);

void jit390SetTrapHandler(UDATA *controlPC, UDATA *entryPointRegister, void* functionPtr)
   {
#if defined(J9ZOS390)
   *entryPointRegister = (UDATA) TOC_UNWRAP_ADDRESS(functionPtr);
#else
   UDATA functionPtrAModeTagged = (UDATA)(functionPtr);
#ifndef TR_TARGET_64BIT
   /**
    * Linux kernel allows signal return to invoke any amode, so we need to
    * guarantee on 31-bit the MSB of functionPtr is tagged, otherwise, kernel
    * will interpret functionPtr as AMODE24.
    */
   functionPtrAModeTagged |= PSW_AMODE31_MASK;
#endif
   *controlPC = functionPtrAModeTagged;
#endif
   }


#define true 1
#define false 0

U_8 isDAACodeException(UDATA pc, U_32 *isBRC)
   {
   /* we have 3 eye catchers.
    * 1. if it is a CVB, then it should be followed by NOP, then return ILC = 4
    * 2. if immediately after the inst we have BRC/BRCL NOP, then IL = 6
    * 3. the const 0xDAA0CA11 is located at the end of the GPR7 restore snippet
    *
    * assume pc is pointing to the BRC/BRCL after.
    * returns the length of the DAA inst, return 0 if it is not DAA code.
    */

   U_16 offset;
   U_32 cont_offset;
   U_32 j;
   U_32 eyeCatcher;
   U_8 * snippetPtr;
   U_8 * ptr = (U_8 *)pc;
   U_8 ILC = 6;

   if (ptr[0] == 0x18 && ptr[1] == 0x00)
      {
      /*  Problem Report 67405:
       *
       *  We have encountered a NOP following the instruction that generated the hardware exception.
       *  In most cases, the instruction preceding the NOP will be a CVB. However, in the case that
       *  the memory reference of CVB exceeds the displacement limit of the CVB instruction but does
       *  not exceed the displacement limit of its long displacement CVBY counterpart, the CVB
       *  instruction will get upgraded to a CVBY at binary encoding time.
       *
       *  The issue here is that CVB is a 4-byte instruction and CVBY is a 6-byte instruction. Because
       *  the upgrade happens at binary encoding, the NOP following the CVB/CVBY still remains. Thus
       *  we have to check 6 bytes back to see if there may have been a CVBY instruction, and if so
       *  return the correct ILC.
       */

      if (ptr[-6] != 0xE3 || ptr[-1] != 0x06)
         ILC = 4;

      /* Because it is a NOP, so we have to skip it in order to check the following BRC/BRCL */
      ptr += 2;
      }

   if (ptr[0] == 0xA7 && ptr[1] == 0x04)
      {
      *isBRC = true;

      /* get offset for eye catcher */
      offset = *((U_16*)(ptr+2));
      cont_offset = ((U_32)offset)*2;
      }
   else if (ptr[0] == 0xC0 && ptr[1] == 0x04)
      {
      *isBRC= false;

      /* get offset for eye catcher */
      cont_offset = *((U_32*)(ptr+2));
      cont_offset = cont_offset*2;
      }
   else
      return 0;

   /*
   ptr -= 6;
   if (ptr[0] == 0x18 && ptr[1] == 0x00)
      return 6;

   ptr += 2;
   if (ptr[0] == 0x18 && ptr[1] == 0x00)
      return 4;
   */

   snippetPtr = ptr + cont_offset;

#ifdef J9VM_JIT_FREE_SYSTEM_STACK_POINTER
#ifdef TR_TARGET_64BIT
   eyeCatcher = *((U_32*)(snippetPtr+24));
#else
   eyeCatcher = *((U_32*)(snippetPtr+18));
#endif
#else
#ifdef TR_TARGET_64BIT
   eyeCatcher = *((U_32*)(snippetPtr+12));
#else
   eyeCatcher = *((U_32*)(snippetPtr+10));
#endif
#endif


   if (eyeCatcher == 0xDAA0CA11)
      {
      return ILC;
      }

   return 0;
   }

/**
 * ON zOS, if J9VM_JIT_FREE_SYSTEM_STACK_POINTER is defined:
 *    Coming from JITCODE, the System Stack Pointer is saved on the VMThread->systemStackPointer,
 *    and GPR4 is the JIT register.
 *    Since we are transitioning out of JITCODE, we need to save the JIT GPR4 into VMThread->returnValue,
 *    reload VMThread->systemStackPointer back into GPR4, and zero out VMThread->systemStackPointer.
 */
UDATA restoreSystemStackPointerState(J9VMThread* vmThread, U_32 sigType, void* sigInfo)
   {
   UDATA* SystemStackPointerInfo;
   UDATA* GPR4Info;
   UDATA  realSystemStackPointerValue;
   const char * infoName;
   U_32 infoType;

#if defined(TR_TARGET_S390) && defined(LINUX)
   return J9PORT_SIG_EXCEPTION_CONTINUE_EXECUTION;
#endif

#if defined(J9VM_JIT_FREE_SYSTEM_STACK_POINTER)

   PORT_ACCESS_FROM_VMC(vmThread);

   /* Retrieve the proper SystemStackPointer value.*/
   infoType = j9sig_info(sigInfo, J9PORT_SIG_CONTROL, J9PORT_SIG_CONTROL_SP, &infoName, &SystemStackPointerInfo);
   if (infoType != J9PORT_SIG_VALUE_ADDRESS)
      return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
   realSystemStackPointerValue = *SystemStackPointerInfo;

   /* Get the GPR4 register and its original value.*/
   infoType = j9sig_info(sigInfo, J9PORT_SIG_GPR, 4, &infoName, &GPR4Info);
   if (infoType != J9PORT_SIG_VALUE_ADDRESS)
      return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;

   /* Save the original (JIT) GPR4 to vmThread->returnValue.*/
   vmThread->returnValue = *GPR4Info;

   /* Store the real SystemStackPointer into GPR4.*/
   *GPR4Info = realSystemStackPointerValue;

   /*vmThread->SSP = 0.*/
   vmThread->systemStackPointer = 0;
#endif

   return J9PORT_SIG_EXCEPTION_CONTINUE_EXECUTION;
   }

UDATA jit390Handler(J9VMThread* vmThread, U_32 sigType, void* sigInfo)
   {
   PORT_ACCESS_FROM_VMC(vmThread);

   J9JITConfig *jitConfig = vmThread->javaVM->jitConfig;

   if (jitConfig)
      {
      const char* infoName;
      UDATA *controlPC;
      UDATA controlPCValue;
      UDATA *entryPointRegister;
      U_8 DXC;
      U_8 ILC;
      U_32 cont_offset;
      U_32 isDAAException = false;
      U_32 isSkipDAAException = false;
      U_32 isBRC = false;
      void *infoValue;
      I_32 si_code;
      U_32 infoType;
      IDATA trapType = TRAP_TYPE_UNKNOWN;
      J9JITExceptionTable *exceptionTable;

      infoType = j9sig_info(sigInfo, J9PORT_SIG_CONTROL, J9PORT_SIG_CONTROL_PC, &infoName, &infoValue);
      if (infoType != J9PORT_SIG_VALUE_ADDRESS)
         return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
      controlPC = (UDATA *) infoValue;
      controlPCValue = *controlPC;

      infoType = j9sig_info(sigInfo, J9PORT_SIG_CONTROL, J9PORT_SIG_CONTROL_S390_FPC, &infoName, &infoValue);
      if (infoType != J9PORT_SIG_VALUE_32)
         return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;

      DXC = *(U_8 *)(((U_8 *)infoValue) + 2);

      if (J9PORT_SIG_FLAG_SIGFPE == sigType && DXC == (U_8)0xFF)
         {
         trapType = jit390IdentifyCodeCacheTrapType(vmThread, (U_8 *) controlPCValue, &ILC, sigInfo);
         controlPCValue = controlPCValue - ILC;

         infoType = j9sig_info(sigInfo, J9PORT_SIG_GPR, 7, &infoName, &infoValue);
         if (infoType != J9PORT_SIG_VALUE_ADDRESS)
            return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
         entryPointRegister = (UDATA *) infoValue;

         /* Defect 120395 : Save original value of GPR7 in vm->tempSlot, the trapHandlers will restore it from there */
         vmThread->tempSlot = (UDATA) (*entryPointRegister);
         }

#if defined(TR_TARGET_S390) && defined(LINUX)
      // if it's a SEGFAULT, could be a null pointer exception
      else if(sigType == J9PORT_SIG_FLAG_SIGSEGV)
         {
         // determine where fault occured
         infoType = j9sig_info(sigInfo, J9PORT_SIG_SIGNAL, J9PORT_SIG_SIGNAL_INACCESSIBLE_ADDRESS, &infoName, &infoValue);

         // if fault occured in low memory, assume it's a null pointer exception
         if (infoType == J9PORT_SIG_VALUE_ADDRESS  && *(UDATA*)infoValue <= 0xFFFF)
            {
            trapType = TRAP_TYPE_NULL_CHECK;
            infoType = j9sig_info(sigInfo, J9PORT_SIG_GPR, 7, &infoName, &infoValue);
            entryPointRegister = (UDATA *) infoValue;

            /* Defect 120395 : Save original value of GPR7 in vm->tempSlot, the trapHandlers will restore it from there */
            vmThread->tempSlot = (UDATA) (*entryPointRegister);
            }
         else
            {
            return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
            }
         }
      else if (sigType == J9PORT_SIG_FLAG_SIGBUS)
         {
         trapType = TRAP_TYPE_INTERNAL_ERROR;
         }
#endif
      /* end of trying to catch traps */
      if (TRAP_TYPE_UNKNOWN == trapType)
         {
#if defined(LINUX)
         /* Linux handles PD signals differently so we include illegal instruction exceptions (e.g. ZAP exceptions will be SIGILL) */
         if (sigType & J9PORT_SIG_FLAG_SIGFPE || sigType & J9PORT_SIG_FLAG_SIGILL)
#else
         if (sigType & J9PORT_SIG_FLAG_SIGFPE)
#endif
            {
            U_32 isCEEHDLR = jitConfig->javaVM != NULL && (jitConfig->javaVM->sigFlags & J9_SIG_ZOS_CEEHDLR) == J9_SIG_ZOS_CEEHDLR;

            /* LE Handlers used by -XCEEDHLR does not set an si_code, but have corresponding LE Runtime messages.
             * http://publib.boulder.ibm.com/infocenter/zvm/v5r4/index.jsp?topic=/com.ibm.zos.r9.ceea900/lemes.htm
             * i.e. FPE_DECOVF  - CEE3210S / Decimal Overflow Exception
             *      FPE_DECDATA - CEE3207S / Decimal Data Exception
             *      FPE_INTDIV  - CEE3209S / Fixed Point Divide By Zero Exception
             */
            if (isCEEHDLR)
               {
               infoType = j9sig_info(sigInfo, J9PORT_SIG_SIGNAL, J9PORT_SIG_SIGNAL_ZOS_CONDITION_MESSAGE_NUMBER, &infoName, &infoValue);

               if (J9PORT_SIG_VALUE_16 != infoType)
                  return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;

               if (3207 == *(U_16*)infoValue)  /* Decimal Data Exception */
                  si_code = FPE_DECDATA;
               else if (3209 == *(U_16*)infoValue) /* Decimal Fixed Point Divide Exception */
                  si_code = FPE_INTDIV;
               else if (3210 == *(U_16*)infoValue) /* Decimal Overflow Exception */
                  si_code = FPE_DECOVF;
               else
                  return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
               }
            else
               {
               /* Extract Signal Code from normal POSIX handlers. */
               infoType = j9sig_info(sigInfo, J9PORT_SIG_SIGNAL, J9PORT_SIG_SIGNAL_CODE, &infoName, &infoValue);

               if (infoType == J9PORT_SIG_VALUE_32)
                  si_code = *(int*)infoValue;
               else
                  return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
               }

            /* DAA specific signal handling.
             * every DAA generated instruction that might throw HW exception is followed by a TR::InstOpCode::COND_NOP branch (BRC/BRCL)
             * the BRC/BRCL inst contains the OOL code offset to the instruction. It has to be computed and replace the
             * entryPointRegister.
             * */
            if (FPE_DECOVF == si_code)
               {
               /* Decimal Overflow */
               ILC = isDAACodeException(controlPCValue, &isBRC);
               isDAAException = (ILC != 0);
               }
            else if (J9PORT_SIG_FLAG_SIGFPE == sigType && DXC == (U_8)0x00 && FPE_DECDATA == si_code)
               {
               /* Decimal Exception */
               ILC = isDAACodeException(controlPCValue, &isBRC);
               isDAAException = (ILC != 0);

               /*
                * Special case for VPSOP + VTP sequence generated for checkPackedDecimal()
                *
                * In the design of DAA tree, we would normally put exception throwing nodes under BCDCHK
                * node and create a function call in the OOL path of a JIT-compiled method should exceptions be thrown.
                * However, pdModifyPrecision under pdChk DAA pattern is a bit different because they were
                * not an exception throwing pattern.
                *
                * TreeTop
                *       pdChk
                *           pdModifyPrecision
                *                   pdloadi
                *
                * The tree above evaluates to a normal non-exception throwing sequence when vector BCD support
                * is not installed.
                *
                * With vector support, the same tree structure evaluates to VPSOP+VTP, the former throws Decimal Exception.
                * Since we don't have an OOL path to fall back to, the normal NOP BRC (which contains a snippet
                * that indirectly points to OOL section start) is not generated. As a result, isDAACodeException() is
                * not able to find the OOL snippet address and return an ILC of 0.
                *
                * To make pdChk functionally correct, the signal handler should detect the VPSOP+VTP sequence and
                * set isSkipDAAException to allow the program continue executing the VTP instruction.
                *
                */
               if(ILC == 0)
                  {
                  U_8 * ptr = ((U_8 *)controlPCValue);
                  if(ptr[-6] == 0xE6 && ptr[-1] == 0x5B && ptr[0] == 0xE6 && ptr[5] == 0x5F)
                     {
                     isDAAException = true;
                     isSkipDAAException = true;
                     }
                  }
               }
            else if ((sigType == J9PORT_SIG_FLAG_SIGFPE_INT_DIV_BY_ZERO && si_code == FPE_INTDIV) ||
                     (sigType == J9PORT_SIG_FLAG_SIGFPE                 && si_code == FPE_INTDIV  && isCEEHDLR))
               {
               /* Convert to Binary throws a Divide by Zero Fixed-Point Exception if PD exceeds limits of binary */
               ILC = isDAACodeException(controlPCValue, &isBRC);
               isDAAException = false;

               /* make sure it is CVB or CVBY, CVBG. */
               if (ILC != 0)
                  {
                  U_8 * ptr = ((U_8 *)controlPCValue) - ILC;
                  if (ptr[0] == 0xE3 && (ptr[5] == 0x06 || ptr[5] == 0x0E))     /* CVBY or CVBG */
                     {
                     isDAAException = true;
                     }
                  else if (ptr[0] == 0x4F && ptr[4] == 0x18 && ptr[5] == 0x00) /* CVB following NOP */
                     {
                     isDAAException = true;
                     }
#if defined(LINUX)
                  else if (ptr[0] == 0xFD)     /* DP divide by zero linux only*/
                     {
                     isDAAException = true;
                     }
#endif
                  }
               }
#if defined(LINUX)
            else if (J9PORT_SIG_FLAG_SIGILL == sigType && DXC == (U_8)0x00)
               {
               /* CVB fixed point divide exception on Linux is Illegal Instruction Exception */
               ILC = isDAACodeException(controlPCValue, &isBRC);
               isDAAException = (ILC != 0);

               // Special handling of VPSOP+VTP. Similar to that of zOS above.
               if(ILC == 0)
                  {
                  U_8 * ptr = ((U_8 *)controlPCValue);
                  if(ptr[-6] == 0xE6 && ptr[-1] == 0x5B && ptr[0] == 0xE6 && ptr[5] == 0x5F)
                     {
                     isDAAException = true;
                     isSkipDAAException = true;
                     }
                  }
               }
#endif
#if defined(J9ZOS390)
            else if (J9PORT_SIG_FLAG_SIGFPE == sigType && DXC == (U_8)0x00 && si_code == FPE_DECDIV)
               {
               /* DP divide by zero on zOS handled as FPE DECDIV*/
               ILC = isDAACodeException(controlPCValue, &isBRC);
               isDAAException = (ILC != 0);
               }
#endif
            }
         if (isDAAException) /* handles the following BRC, BRCL. */
            {

            /*  Problem Report 67405:
             *
             *  If the current PC is pointing to a NOP and the preceding instruction is a CVBY then
             *  we must skip over the NOP (2 bytes) as the ILC returned by isDAACodeException = 6.
             *  See comment in isDAACodeException for further information.
             *
             */

            U_8* pcpointer = (U_8*)controlPCValue;

            /* Additional adjustment to use in address calculations (see PR 67405 note above) */
            U_8 CVBYAdjustment = 0;

            if (pcpointer[0] == 0x18 && pcpointer[1] == 0x00 && pcpointer[-6] == 0xE3 && pcpointer[-1] == 0x06)
               CVBYAdjustment = 2;

            if (isBRC)
               {
               /* BRC uses only 2 bytes to represent offsets, so we have to use short here.
                * at the moment controlPCValue points to the instr immediately after exception-throwing
                * instr, controlPCValue - ILC + CVBYAdjustment + 6 makes it points to the BRC/BRCL.
                * */
               U_16 offset = *((U_16*)(controlPCValue - ILC + CVBYAdjustment + 6 + 2));
               cont_offset = ((U_32)offset)*2;
               }
            else
               {
               cont_offset = *((U_32*)(controlPCValue - ILC + CVBYAdjustment + 6 + 2));
               cont_offset = cont_offset*2;
               }

            /* Defect 120395 : Save original value of GPR7 in vm->tempSlot, the trapHandlers will restore it from there */
            infoType = j9sig_info(sigInfo, J9PORT_SIG_GPR, 7, &infoName, &infoValue);
            entryPointRegister = (UDATA *) infoValue;
            vmThread->tempSlot = (UDATA) (*entryPointRegister);

            /* Work item 51875 : Return address is given by GPR7 in ZOS but controlPC in Linux */
#if defined(LINUX)
            UDATA returnTargetAModeTagged = 0;
            if(isSkipDAAException)
               {
               returnTargetAModeTagged = controlPCValue;
               }
            else
               {
               returnTargetAModeTagged = controlPCValue - ILC + CVBYAdjustment + 6 + cont_offset;
               }
#ifndef TR_TARGET_64BIT
            /**
             * Linux kernel allows signal return to invoke any amode, so we need to
             * guarantee on 31-bit the MSB of functionPtr is tagged, otherwise, kernel
             * will interpret functionPtr as AMODE24.
             */
            returnTargetAModeTagged |= PSW_AMODE31_MASK;
#endif
            *controlPC = returnTargetAModeTagged;
#else
            if(isSkipDAAException)
               {
               *entryPointRegister = controlPCValue;
               }
            else
               {
               *entryPointRegister = controlPCValue - ILC + CVBYAdjustment + 6 + cont_offset;
               }
#endif

            return restoreSystemStackPointerState(vmThread, sigType, sigInfo);
            }
         } /* End of DAA Signal Handling */

      exceptionTable = jitConfig->jitGetExceptionTableFromPC(vmThread, controlPCValue);

      if (exceptionTable)
         {
         switch (trapType)
            {
            case TRAP_TYPE_NULL_CHECK:
               vmThread->jitException = (J9Object *) (controlPCValue + 1);
               /* add one to *controlPC for symmetry with IA32, handler check subs one */
               jit390SetTrapHandler(controlPC, entryPointRegister, (void *) &jitHandleNullPointerExceptionTrap);
               return restoreSystemStackPointerState(vmThread, sigType, sigInfo);
   
            case TRAP_TYPE_INTERNAL_ERROR:
               vmThread->jitException = (J9Object *) (controlPCValue + 1);
               /* add one to *controlPC for symmetry with IA32, handler check subs one */
               jit390SetTrapHandler(controlPC, entryPointRegister, (void *) &jitHandleInternalErrorTrap);
               return restoreSystemStackPointerState(vmThread, sigType, sigInfo);
            
            case TRAP_TYPE_ARRAY_BOUNDS:
               vmThread->jitException = (J9Object *) (controlPCValue + 1);
               /* add one to *controlPC for symmetry with IA32, handler check subs one */
               jit390SetTrapHandler(controlPC, entryPointRegister, (void *) &jitHandleArrayIndexOutOfBoundsTrap);
               return restoreSystemStackPointerState(vmThread, sigType, sigInfo);

            case TRAP_TYPE_DIV_CHECK:
               vmThread->jitException = (J9Object *) (controlPCValue + 1);
               /* add one to *controlPC for symmetry with IA32, handler check subs one */
               jit390SetTrapHandler(controlPC, entryPointRegister, (void *) &jitHandleIntegerDivideByZeroTrap);
               return restoreSystemStackPointerState(vmThread, sigType, sigInfo);

            default:
               break;
            }

         /*
          * This is an unexpected error. Build a resolve
          * frame so that the stack is walkable and allow normal fault handling
          * to continue
          * CMVC 104332 : do not update the java stack pointer as it is misleading. We just need the stack to be walkable by our diagnostic tools.
          */

         /* the Java SP is r5 on 390 */
         infoType = j9sig_info(sigInfo, J9PORT_SIG_GPR, 5, &infoName, &infoValue);
         if (infoType == J9PORT_SIG_VALUE_ADDRESS)
            {
            UDATA **javaSPPtr = (UDATA **) infoValue;
            jitPushResolveFrame(vmThread, *javaSPPtr, (U_8 *) *controlPC);
            }
         }
      }
   return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
   }


#elif defined(TR_HOST_X86) && defined(TR_TARGET_X86) && defined(TR_TARGET_64BIT)

#define REX_W 0x8
#define REX_X 0x2
#define REX_B 0x1
#define IS_REXW(REX_PREFIX)   (((REX_PREFIX) & REX_W) == REX_W)
#define IS_REXX(REX_PREFIX)   (((REX_PREFIX) & REX_X) == REX_X)
#define IS_REXB(REX_PREFIX)   (((REX_PREFIX) & REX_B) == REX_B)

extern void jitHandleIntegerDivideByZeroTrap(void);
extern void jitHandleNullPointerExceptionTrap(void);
extern void jitHandleInternalErrorTrap(void);

static UDATA jitAMD64isREXPrefix(U_8 byte)
{
	return (0x40 <= byte && byte <= 0x4f);
}

static UDATA jitAMD64isLegacyPrefix(U_8 byte)
{
	switch(byte) {
		case 0x66: case 0x67: case 0x2e: case 0x3e: case 0x26: case 0x64:
		case 0x65: case 0x36: case 0xf0: case 0xf3: case 0xf2:
			return byte;
			break;
		default:
			return FALSE;
			break;
        }
}

static I_64 jitAMD64regValFromRMBase(J9PortLibrary* portLib, U_8 base, U_8 rexPrefix, void *sigInfo)
{
	PORT_ACCESS_FROM_PORT(portLib);
	const char* infoName;
	void* infoValue;
	U_32 infoType;

	/* all #define'd register indexes are negative values */
	I_32 infoIndex = 1;

	switch(base) {

		case 0:
			infoIndex = ((IS_REXB(rexPrefix)) ? J9PORT_SIG_GPR_AMD64_R8 : J9PORT_SIG_GPR_AMD64_RAX);
			infoType = J9PORT_SIG_GPR;
			break;

		case 1:
			infoIndex = ((IS_REXB(rexPrefix)) ? J9PORT_SIG_GPR_AMD64_R9 : J9PORT_SIG_GPR_AMD64_RCX);
			infoType = J9PORT_SIG_GPR;
			break;

		case 2:
			infoIndex = ((IS_REXB(rexPrefix)) ? J9PORT_SIG_GPR_AMD64_R10 : J9PORT_SIG_GPR_AMD64_RDX);
			infoType = J9PORT_SIG_GPR;
			break;

		case 3:
			infoIndex = ((IS_REXB(rexPrefix)) ? J9PORT_SIG_GPR_AMD64_R11 : J9PORT_SIG_GPR_AMD64_RBX);
			infoType = J9PORT_SIG_GPR;
			break;

		case 4:
			if (IS_REXB(rexPrefix)) {
				infoIndex = J9PORT_SIG_GPR_AMD64_R12;
				infoType = J9PORT_SIG_GPR;
			} else {
				infoIndex = J9PORT_SIG_CONTROL_SP;
				infoType = J9PORT_SIG_CONTROL;
			}
			break;

		case 5:
			if (IS_REXB(rexPrefix)) {
				infoIndex = J9PORT_SIG_GPR_AMD64_R13;
				infoType = J9PORT_SIG_GPR;
			} else {
				infoIndex = J9PORT_SIG_CONTROL_BP;
				infoType = J9PORT_SIG_CONTROL;
			}
			break;

		case 6:
			infoIndex = ((IS_REXB(rexPrefix)) ? J9PORT_SIG_GPR_AMD64_R14 : J9PORT_SIG_GPR_AMD64_RSI);
			infoType = J9PORT_SIG_GPR;
			break;

		case 7:
			infoIndex = ((IS_REXB(rexPrefix)) ? J9PORT_SIG_GPR_AMD64_R15 : J9PORT_SIG_GPR_AMD64_RDI);
			infoType = J9PORT_SIG_GPR;
			break;

		default:
			return -1;
			break;
	}

	if (j9sig_info(sigInfo, infoType, infoIndex, &infoName, &infoValue) != J9PORT_SIG_VALUE_ADDRESS) {
		/* error! */
		return -1;
	}

	return *(I_64 *)infoValue;
}

static I_64 jitAMD64regValFromIndex(J9PortLibrary* portLib, U_8 index, U_8 rexPrefix, void *sigInfo)
{
	PORT_ACCESS_FROM_PORT(portLib);
	const char* infoName;
	void* infoValue;
	U_32 infoType;

	/* all #define(d) register indexes are negative values */
	I_32 infoIndex = 1;

	switch(index) {

		case 0:
			infoIndex = ((IS_REXX(rexPrefix)) ? J9PORT_SIG_GPR_AMD64_R8 : J9PORT_SIG_GPR_AMD64_RAX);
			infoType = J9PORT_SIG_GPR;
			break;

		case 1:
			infoIndex = ((IS_REXX(rexPrefix)) ? J9PORT_SIG_GPR_AMD64_R9 : J9PORT_SIG_GPR_AMD64_RCX);
			infoType = J9PORT_SIG_GPR;
			break;

		case 2:
			infoIndex = ((IS_REXX(rexPrefix)) ? J9PORT_SIG_GPR_AMD64_R10 : J9PORT_SIG_GPR_AMD64_RDX);
			infoType = J9PORT_SIG_GPR;
			break;

		case 3:
			infoIndex = ((IS_REXX(rexPrefix)) ? J9PORT_SIG_GPR_AMD64_R11 : J9PORT_SIG_GPR_AMD64_RBX);
			infoType = J9PORT_SIG_GPR;
			break;

		case 4:
			if (IS_REXX(rexPrefix)) {
				infoIndex = J9PORT_SIG_GPR_AMD64_R12;
				infoType = J9PORT_SIG_GPR;
			} else {
				infoIndex = J9PORT_SIG_CONTROL_SP;
				infoType = J9PORT_SIG_CONTROL;
			}
			break;

		case 5:
			if (IS_REXX(rexPrefix)) {
				infoIndex = J9PORT_SIG_GPR_AMD64_R13;
				infoType = J9PORT_SIG_GPR;
			} else {
				infoIndex = J9PORT_SIG_CONTROL_BP;
				infoType = J9PORT_SIG_CONTROL;
			}
			break;

		case 6:
			infoIndex = ((IS_REXX(rexPrefix)) ? J9PORT_SIG_GPR_AMD64_R14 : J9PORT_SIG_GPR_AMD64_RSI);
			infoType = J9PORT_SIG_GPR;
			break;

		case 7:
			infoIndex = ((IS_REXX(rexPrefix)) ? J9PORT_SIG_GPR_AMD64_R15 : J9PORT_SIG_GPR_AMD64_RDI);
			infoType = J9PORT_SIG_GPR;
			break;

		default:
			return -1;
			break;
	}

	if (j9sig_info(sigInfo, infoType, infoIndex, &infoName, &infoValue) != J9PORT_SIG_VALUE_ADDRESS) {
		/* error! */
		return -1;
	}

	return *(I_64 *)infoValue;
}

static I_64
jitAMD64maskValue(I_64 value, UDATA size)
{

	switch(size) {
		case 64: return value;
			break;
		case 32: return (value & 0xffffffff);
			break;
		case 16: return (value & 0xffff);
			break;
		case 8: return (value & 0xff);
			break;
		default:
			return -1;
			break;
	}
}

static I_64 jitAMD64dereference_eaddr(U_64 eaddr, int operand_size, int address_size_override)
{
	if (address_size_override) {
		eaddr = jitAMD64maskValue(eaddr, 32);   /* truncate */
	}

	switch (operand_size) 	{
		case 64: return *((U_64 *)eaddr);
			break;
		case 32: return *((U_32 *)eaddr);
			break;
		case 16: return *((U_16 *)eaddr);
			break;
		case 8: return *((U_8 *)eaddr);
			break;
		default:
			return -1;
	}
}

/* Return TRUE if an instruction is a div (unsigned divide) or
 *   idiv (signed divide), else return FALSE
*/
static UDATA jitAMD64isDivInstruction(U_8 *rip)
{
	U_8 ModRM;
	U_8 reg; /* bits 5-3 of the ModRM byte */

	while (jitAMD64isLegacyPrefix(*rip)) {
		/* return false for LOCK or Repeat prefixes */
		if (*rip == 0xf0) {
			return FALSE;
		} else if 	((*rip == 0xf3) || (*rip == 0xf2)) {
			return FALSE;
		}
		rip++;
	}

	if (jitAMD64isREXPrefix(*rip)) rip++; /* REX Prefix must precede opcode */

	/* div instructions match f6/6 and f7/6, idiv matches f6/7 and f7/7 */
	if (*rip == 0xf7 || *rip == 0xf6) {
		ModRM = *(rip + 1);
		reg = (ModRM & 0x38) >> 3;
		if ((reg == 0x6 ) || (reg == 0x7)) {
			return TRUE;
		}
	}

	return FALSE;
}

static I_64 jitAMD64decodeDiv(J9PortLibrary* portLib, U_8 *rip, void *sigInfo, UDATA *ins_size, UDATA *operand_size)
{
	PORT_ACCESS_FROM_PORT(portLib);

	U_8 REX_PREFIX = 0;  /* 0 indicating no prefix */
	U_8 ModRM, SIB = 0;  /* 0 indicating no SIB byte */
	U_8 mod, reg, rm;       /* ModRM fields */
	U_8 scale, index, base;  /* SIB fields */

	const char* infoName;
	void* infoValue;
	U_32 infoType;
	UDATA* ripPtr;

	UDATA operand_size_override = FALSE;
	UDATA address_size_override = FALSE;
	UDATA mask;
	I_32 disp = 0; /* displacement */
	U_64 eaddr;    /* effective address */
	I_64 result;

	infoType = j9sig_info(sigInfo, J9PORT_SIG_CONTROL, J9PORT_SIG_CONTROL_PC, &infoName, &infoValue);
	if (infoType != J9PORT_SIG_VALUE_ADDRESS) {
		/* error */
		return -1;
	}
	ripPtr = (UDATA*) infoValue;

	*ins_size = 0;  /*  ins_size = instruction size */

	while (jitAMD64isLegacyPrefix(*rip)) {
		if (*rip == 0x66) {
			operand_size_override = TRUE;
		}
		if (*rip == 0x67) {
			address_size_override = TRUE;
		}
		if (*rip == 0xf0) {
			return -1;  /* invalid opcode exception with idiv */
		}
		rip++; ++*ins_size;
	}

	/* REX Prefix if it exits must precede the opcode */
	if (jitAMD64isREXPrefix(*rip)) {
		REX_PREFIX = *rip;   /* save the rex prefix */
		/* The REX prefix takes precedence over the legacy prefix */
		if (IS_REXW(REX_PREFIX)) operand_size_override = FALSE;
		rip++; ++*ins_size;
	}

	/* double check opcode is div or idiv, and set operand_size */
	if (*rip == 0xf7) {
		if (IS_REXW(REX_PREFIX)) *operand_size = 64;
		else if (operand_size_override) *operand_size = 16;
		else *operand_size = 32;
	} else if (*rip == 0xf6)	{
		*operand_size = 8;
	} else {
		return -1;  /* not an idiv instruction */
	}

	rip++; ++*ins_size;

	ModRM = *rip++; ++*ins_size;
	mod = ModRM >> 6;          /* bits 7-6 */
	reg = (ModRM & 0x38) >> 3; /* bits 5-3 */
	rm = ModRM & 0x7;          /* bits 3-0 */

#if defined JIT_SIGNAL_DEBUG
	fprintf(stderr, "ModRM = %#x\n", ModRM);
	fprintf(stderr, "\tmod = %#x, reg = %x, rm = %x\n", mod, reg, rm);
#endif

	/* double check that we are dealing with a div or idiv instruction */
	if ((reg != 0x6) && (reg != 0x7)) {
		return -1;
	}

	if (mod == 0x3) {  /* is the divisor in a register? */
		result = jitAMD64regValFromRMBase(PORTLIB, rm, REX_PREFIX, sigInfo);
		return (jitAMD64maskValue(result, *operand_size));
	}

	/* RIP-Relative Addressing */
	if ((mod == 0x0) && (rm == 0x5)) 	{
		*ins_size += 4;  /* 4 bytes for the 32 bit offset */
		eaddr = *ripPtr + *ins_size; /* i.e., relative to the next instruction */
		disp = *((I_32 *)(rip));
		eaddr += disp;
		result = jitAMD64dereference_eaddr(eaddr, *operand_size, address_size_override);
		return result;
	}

	if (rm != 0x4) {   /* true if there is no SIB byte */
		if (mod == 0x1) {
			disp = *((I_8 *)(rip));
			*ins_size += 1;
		} else if (mod == 0x2)	{
			disp = *((I_32 *)(rip));
			*ins_size += 4;
		}

		eaddr = jitAMD64regValFromRMBase(PORTLIB, rm, REX_PREFIX, sigInfo) + disp;

		result = jitAMD64dereference_eaddr(eaddr, *operand_size, address_size_override);
		return result;
	} else { /* SIB byte present */
		SIB = *rip++; ++*ins_size;
		scale = SIB >> 6;
		index = (SIB & 0x38) >> 3;
		base = SIB & 0x7;

#if defined JIT_SIGNAL_DEBUG
		fprintf(stderr, "SIB = %#x\n", SIB);
		fprintf(stderr, "\tscale = %#x, index = %#x, base = %#x\n", scale, index, base);
#endif

		/* Get a displacement if it exists */
		if ((base == 0x5) && (mod == 0x0))	{
			disp = *((I_32 *)(rip));
			*ins_size += 4;
#if defined JIT_SIGNAL_DEBUG
			fprintf(stderr, "disp = %#x\n", disp);
#endif
			eaddr = disp; /* no base register */
		} else {
			if (mod == 0x1) {
				disp = *((I_8 *)(rip));
				*ins_size += 1;
			} else if (mod == 0x2)	{
				disp = *((I_32 *)(rip));
				*ins_size += 4;
			}
#if defined JIT_SIGNAL_DEBUG
			fprintf(stderr, "disp = %#x\n", disp);
#endif
			eaddr = jitAMD64regValFromRMBase(PORTLIB, base, REX_PREFIX, sigInfo) + disp;
		}

		/* calculate scale*index except when index = 0x4, and REX.X is not set
		 * i.e., can't encode index = rSP
		*/
		if ((index == 0x4) && !(IS_REXX(REX_PREFIX))) {
			/* do nothing  */
		} else {
			eaddr += (jitAMD64regValFromIndex(PORTLIB, index, REX_PREFIX, sigInfo) * (1<<scale));
		}

		result = jitAMD64dereference_eaddr(eaddr, *operand_size, address_size_override);
		return result;
	}

	return -1;
}

UDATA jitAMD64Handler(J9VMThread* vmThread, U_32 sigType, void *sigInfo)
{
	PORT_ACCESS_FROM_VMC(vmThread);

	J9JITExceptionTable *exceptionTable = NULL;
	J9JITConfig *jitConfig = vmThread->javaVM->jitConfig;

	if (jitConfig) {

		J9SFJITResolveFrame * resolveFrame;
		const char* infoName;
		void* infoValue;
		U_32 infoType;
		U_8* rip;
		UDATA* ripPtr;
		UDATA* rspPtr;
		UDATA* raxPtr;
		UDATA* rcxPtr;
		UDATA* rdxPtr;
		UDATA* rbpPtr;

		infoType = j9sig_info(sigInfo, J9PORT_SIG_CONTROL, J9PORT_SIG_CONTROL_PC, &infoName, &infoValue);
		if (infoType != J9PORT_SIG_VALUE_ADDRESS) {
			return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
		}
		ripPtr = (UDATA*) infoValue;
		rip = (U_8*)*ripPtr;

		infoType = j9sig_info(sigInfo, J9PORT_SIG_GPR, J9PORT_SIG_GPR_AMD64_RAX, &infoName, &infoValue);
		if (infoType != J9PORT_SIG_VALUE_ADDRESS) {
			return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
		}
		raxPtr = (UDATA*) infoValue;

		infoType = j9sig_info(sigInfo, J9PORT_SIG_GPR, J9PORT_SIG_GPR_AMD64_RCX, &infoName, &infoValue);
		if (infoType != J9PORT_SIG_VALUE_ADDRESS) {
			return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
		}
		rcxPtr = (UDATA*) infoValue;

		infoType = j9sig_info(sigInfo, J9PORT_SIG_GPR, J9PORT_SIG_GPR_AMD64_RDX, &infoName, &infoValue);
		if (infoType != J9PORT_SIG_VALUE_ADDRESS) {
			return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
		}
		rdxPtr = (UDATA*) infoValue;

		infoType = j9sig_info(sigInfo, J9PORT_SIG_CONTROL, J9PORT_SIG_CONTROL_SP, &infoName, &infoValue);
		if (infoType != J9PORT_SIG_VALUE_ADDRESS) {
			return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
		}
		rspPtr = (UDATA*) infoValue;

		infoType = j9sig_info(sigInfo, J9PORT_SIG_CONTROL, J9PORT_SIG_CONTROL_BP, &infoName, &infoValue);
		if (infoType != J9PORT_SIG_VALUE_ADDRESS) {
			return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
		}
		rbpPtr = (UDATA*) infoValue;

		exceptionTable = jitConfig->jitGetExceptionTableFromPC(vmThread, (U_64)rip);
		if (exceptionTable == NULL) {
			return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
		} else {
			void * stackMap;
			UDATA registerMap;
			switch (sigType) {
			case J9PORT_SIG_FLAG_SIGFPE_DIV_BY_ZERO:
			case J9PORT_SIG_FLAG_SIGFPE_INT_DIV_BY_ZERO:
#if !defined(LINUX)
			case J9PORT_SIG_FLAG_SIGFPE_INT_OVERFLOW:
#endif
				if (jitAMD64isDivInstruction(rip)) {

					IDATA div;
					UDATA ins_size, operand_size;

	 				/* find divisor */
					div = jitAMD64decodeDiv(PORTLIB, rip, sigInfo, &ins_size, &operand_size);

					if (div != 0 ) {

						*ripPtr += ins_size;
						switch (operand_size) {
						case 64:
							*raxPtr = 0x8000000000000000;
							break;
						case 32:
							*raxPtr = 0x80000000;
							 break;
						case 16:
							*raxPtr = 0x8000;
							break;
						case 8:
							*raxPtr = 0x80;
							break;
						default:
				   			/* error */
							break;
						}
						*rdxPtr = 0;

						return J9PORT_SIG_EXCEPTION_CONTINUE_EXECUTION;
					}
				}

	 			/* Add 1 to make rip point inside the instruction
	 			*   since we assume most pc values are after a call instruction
	 			*   the first byte of an instruction might not be in the exception range
	 			*   Note, adding the instruction length is also incorrect, since it is possible
	 			*   that exactly one instruction was in the range
				*/
				vmThread->jitException = (J9Object *) (*ripPtr + 1);

				*ripPtr = (U_64) ((void *) &jitHandleIntegerDivideByZeroTrap);
				((J9VMJITRegisterState*)vmThread->entryLocalStorage->jitGlobalStorageBase)->jit_rbp = (UDATA) *rbpPtr;
				*rbpPtr = (U_64) vmThread;
				return J9PORT_SIG_EXCEPTION_CONTINUE_EXECUTION;

#ifdef LINUX
			case J9PORT_SIG_FLAG_SIGFPE_INT_OVERFLOW:

				if (jitAMD64isDivInstruction(rip)) {
					I_64 div;
					UDATA ins_size, operand_size;
					div = jitAMD64decodeDiv(PORTLIB, rip, sigInfo, &ins_size, &operand_size);

					if ( div != 0) {
						*ripPtr += ins_size;
						switch (operand_size) {
						case 64:
							*raxPtr = 0x8000000000000000;
							 break;
						case 32:
							*raxPtr = 0x80000000;
							break;
						case 16:
							*raxPtr = 0x8000;
							break;
						case 8:
							*raxPtr = 0x80;
							break;
						default:
							/* error */
							break;
						}
						*rdxPtr = 0;
						return J9PORT_SIG_EXCEPTION_CONTINUE_EXECUTION;
					}
				}
				break;
#endif
			case J9PORT_SIG_FLAG_SIGBUS:
			case J9PORT_SIG_FLAG_SIGSEGV:

				vmThread->jitException = (J9Object *) ((UDATA) rip + 1);
				*ripPtr = (U_64)(void*)(sigType == J9PORT_SIG_FLAG_SIGSEGV ? jitHandleNullPointerExceptionTrap : jitHandleInternalErrorTrap);
				((J9VMJITRegisterState*)vmThread->entryLocalStorage->jitGlobalStorageBase)->jit_rbp = (UDATA) *rbpPtr;
				*rbpPtr = (U_64) vmThread;
				return J9PORT_SIG_EXCEPTION_CONTINUE_EXECUTION;

			case J9PORT_SIG_FLAG_SIGILL:
				break;
			}

			/*
			 * if we reached here, then this is an unexpected error. Build a resolve
			 * frame so that the stack is walkable and allow normal fault handling
			 * to continue
			 * CMVC 104332 : do not update the java stack pointer as it is misleading. We just need the stack to be walkable by our diagnostic tools.
			 */
			jitPushResolveFrame(vmThread, (UDATA*)*rspPtr, rip);
		}
	}

	return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
}

#endif

#endif /* J9VM_PORT_SIGNAL_SUPPORT && J9VM_INTERP_NATIVE_SUPPORT */
