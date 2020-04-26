/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

#include "cfreader.h"
#include "stackwalk.h"
#include "ut_j9vm.h"

extern "C" {

char*
getCompleteNPEMessage(J9VMThread *vmThread, U_8 *bcCurrentPtr, J9ROMClass *romClass, const char *npeCauseMsg)
{
	char *npeMsg = NULL;
	UDATA msgLen = 0;
	const char *msgTemplate = NULL;
	U_8 bcCurrent = *bcCurrentPtr;

	PORT_ACCESS_FROM_VMC(vmThread);
	Trc_VM_GetCompleteNPEMessage_Entry(vmThread, bcCurrentPtr, bcCurrent, romClass, npeCauseMsg);
	if (((bcCurrent >= JBiaload) && (bcCurrent <= JBsaload))
		|| ((bcCurrent >= JBiastore) && (bcCurrent <= JBsastore))
	) {
		const char *elementType = NULL;
		switch (bcCurrent) {
		case JBiaload: /* FALLTHROUGH */
		case JBiastore:
			elementType = "int";
			break;
		case JBlaload: /* FALLTHROUGH */
		case JBlastore:
			elementType = "long";
			break;
		case JBfaload: /* FALLTHROUGH */
		case JBfastore:
			elementType = "float";
			break;
		case JBdaload: /* FALLTHROUGH */
		case JBdastore:
			elementType = "double";
			break;
		case JBaaload: /* FALLTHROUGH */
		case JBaastore:
			elementType = "object";
			break;
		case JBbaload: /* FALLTHROUGH */
		case JBbastore:
			elementType = "byte/boolean";
			break;
		case JBcaload: /* FALLTHROUGH */
		case JBcastore:
			elementType = "char";
			break;
		case JBsaload: /* FALLTHROUGH */
		case JBsastore:
			elementType = "short";
			break;
		default:
			Trc_VM_GetCompleteNPEMessage_Unreachable(vmThread, bcCurrent);
		}
		if ((bcCurrent >= JBiaload) && (bcCurrent <= JBsaload)) {
			msgTemplate = "Cannot load from %s array";
		} else {
			msgTemplate = "Cannot store to %s array";
		}
		msgLen = j9str_printf(PORTLIB, NULL, 0, msgTemplate, elementType);
		/* msg NULL check omitted since str_printf accepts NULL (as above) */
		npeMsg = (char *)j9mem_allocate_memory(msgLen + 1, OMRMEM_CATEGORY_VM);
		j9str_printf(PORTLIB, npeMsg, msgLen, msgTemplate, elementType);
	} else {
		switch (bcCurrent) {
		case JBarraylength: {
			const char *VM_NPE_ARRAYLENGTH = "Cannot read the array length";
			msgLen = j9str_printf(PORTLIB, NULL, 0, VM_NPE_ARRAYLENGTH);
			npeMsg = (char *)j9mem_allocate_memory(msgLen + 1, OMRMEM_CATEGORY_VM);
			j9str_printf(PORTLIB, npeMsg, msgLen, VM_NPE_ARRAYLENGTH);
			break;
		}
		case JBathrow: {
			const char *VM_NPE_ATHROW = "Cannot throw exception";
			msgLen = j9str_printf(PORTLIB, NULL, 0, VM_NPE_ATHROW);
			npeMsg = (char *)j9mem_allocate_memory(msgLen + 1, OMRMEM_CATEGORY_VM);
			j9str_printf(PORTLIB, npeMsg, msgLen, VM_NPE_ATHROW);
			break;
		}
		case JBmonitorenter: {
			const char *VM_NPE_MONITORENTER = "Cannot enter synchronized block";
			msgLen = j9str_printf(PORTLIB, NULL, 0, VM_NPE_MONITORENTER);
			npeMsg = (char *)j9mem_allocate_memory(msgLen+ 1, OMRMEM_CATEGORY_VM);
			j9str_printf(PORTLIB, npeMsg, msgLen, VM_NPE_MONITORENTER);
			break;
		}
		case JBmonitorexit: {
			const char *VM_NPE_MONITOREXIT = "Cannot exit synchronized block";
			msgLen = j9str_printf(PORTLIB, NULL, 0, VM_NPE_MONITOREXIT);
			npeMsg = (char *)j9mem_allocate_memory(msgLen + 1, OMRMEM_CATEGORY_VM);
			j9str_printf(PORTLIB, npeMsg, msgLen, VM_NPE_MONITOREXIT);
			break;
		}
		case JBgetfield: /* FALLTHROUGH */
		case JBputfield: {
			U_16 index = PARAM_16(bcCurrentPtr, 1);
			UDATA cpType = J9_CP_TYPE(J9ROMCLASS_CPSHAPEDESCRIPTION(romClass), index);

			Trc_VM_GetCompleteNPEMessage_Field_Index(vmThread, index);
			if (J9CPTYPE_FIELD == cpType) {
				J9ROMConstantPoolItem *constantPool = J9_ROM_CP_FROM_ROM_CLASS(romClass);
				J9ROMConstantPoolItem *cpItem = constantPool + index;
				J9ROMNameAndSignature *nameAndSig = J9ROMFIELDREF_NAMEANDSIGNATURE((J9ROMFieldRef *) cpItem);
				J9UTF8 *name = J9ROMNAMEANDSIGNATURE_NAME(nameAndSig);

				if (JBputfield == bcCurrent) {
					msgTemplate = "Cannot assign field \"%2$.*1$s\"";
				} else {
					msgTemplate = "Cannot read field \"%2$.*1$s\"";
				}
				msgLen = j9str_printf(PORTLIB, NULL, 0, msgTemplate, J9UTF8_LENGTH(name), J9UTF8_DATA(name));
				npeMsg = (char *)j9mem_allocate_memory(msgLen + 1, OMRMEM_CATEGORY_VM);
				j9str_printf(PORTLIB, npeMsg, msgLen, msgTemplate, J9UTF8_LENGTH(name), J9UTF8_DATA(name));
			} else {
				Trc_VM_GetCompleteNPEMessage_UnexpectedCPType(vmThread, cpType, bcCurrent);
			}
			break;
		}
		case JBinvokehandle: /* FALLTHROUGH */
		case JBinvokehandlegeneric: /* FALLTHROUGH */
		case JBinvokeinterface2: /* FALLTHROUGH */
		case JBinvokeinterface: /* FALLTHROUGH */
		case JBinvokespecial: /* FALLTHROUGH */
		case JBinvokedynamic: /* FALLTHROUGH */
		case JBinvokevirtual: /* FALLTHROUGH */
		case JBinvokestatic: {
			U_16 index = 0;
			J9ROMConstantPoolItem *constantPool = J9_ROM_CP_FROM_ROM_CLASS(romClass);

			if (JBinvokeinterface2 == bcCurrent) {
				index = PARAM_16(bcCurrentPtr + 2, 1); /* get JBinvokeinterface instead */
			} else {
				index = PARAM_16(bcCurrentPtr, 1);
			}
			Trc_VM_GetCompleteNPEMessage_Invoke_Index(vmThread, index);

			J9ROMMethodRef *romMethodRef = (J9ROMMethodRef *)&constantPool[index];
			J9ROMNameAndSignature *nameAndSig = J9ROMMETHODREF_NAMEANDSIGNATURE(romMethodRef);
			J9UTF8 *sig = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig);
			J9UTF8 *name = J9ROMNAMEANDSIGNATURE_NAME(nameAndSig);
			const char *VM_NPE_INVOKEMETHOD = "Cannot invoke \"%2$.*1$s.%4$.*3$s%6$.*5$s\"";
			J9UTF8 *definingUTF = J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *)&constantPool[romMethodRef->classRefCPIndex]);

			msgLen = j9str_printf(PORTLIB, NULL, 0, VM_NPE_INVOKEMETHOD,
				J9UTF8_LENGTH(definingUTF), J9UTF8_DATA(definingUTF),
				J9UTF8_LENGTH(name), J9UTF8_DATA(name), J9UTF8_LENGTH(sig), J9UTF8_DATA(sig));
			npeMsg = (char *)j9mem_allocate_memory(msgLen + 1, OMRMEM_CATEGORY_VM);
			j9str_printf(PORTLIB, npeMsg, msgLen, VM_NPE_INVOKEMETHOD,
				J9UTF8_LENGTH(definingUTF), J9UTF8_DATA(definingUTF),
				J9UTF8_LENGTH(name), J9UTF8_DATA(name), J9UTF8_LENGTH(sig), J9UTF8_DATA(sig));
			break;
		}
		default:
			Trc_VM_GetCompleteNPEMessage_MissedBytecode(vmThread, bcCurrent);
		}
	}
	Trc_VM_GetCompleteNPEMessage_Exit(vmThread, npeMsg);

	return npeMsg;
}

}
