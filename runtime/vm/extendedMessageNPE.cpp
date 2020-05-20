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
#include "util_api.h"
#include "ut_j9vm.h"

extern "C" {

static char* convertToJavaFullyQualifiedName(J9VMThread *vmThread, J9UTF8 *fullyQualifiedNameUTF);
static char* convertMethodSignature(J9VMThread *vmThread, J9UTF8 *methodSig);


/**
 * Replace '/' with '.' for an internal fully qualified name
 *
 * The caller is responsible for freeing the returned string.
 *
 * @param[in] vmThread current J9VMThread
 * @param[in] fullyQualifiedNameUTF pointer to J9UTF8 containing the fully qualified name
 *
 * @return a char pointer to the fully qualified name,
 *         NULL if not successful, but keep application exception instead of throwing OOM.
 */
static char*
convertToJavaFullyQualifiedName(J9VMThread *vmThread, J9UTF8 *fullyQualifiedNameUTF)
{
	PORT_ACCESS_FROM_VMC(vmThread);
	UDATA length = J9UTF8_LENGTH(fullyQualifiedNameUTF);
	char *result =  (char *)j9mem_allocate_memory(length + 1, OMRMEM_CATEGORY_VM);

	if (NULL != result) {
		char *cursor = result;
		char *end = result + length;

		memcpy(result, J9UTF8_DATA(fullyQualifiedNameUTF), length);
		while (cursor < end) {
			if ('/' == *cursor) {
				*cursor = '.';
			}
			cursor += 1;
		}
		*end = '\0';
	}

	Trc_VM_ConvertToJavaFullyQualifiedName_Get_ClassName(vmThread, result, length, fullyQualifiedNameUTF);

	return result;
}

/**
 * Convert a signature string
 * from
 *  ([[[Ljava/lang/String;Ljava/lang/Void;BLjava/lang/String;ICDFJLjava/lang/Object;SZ)Ljava/lang/String;
 * to
 * (java.lang.String[][][], java.lang.Void, byte, java.lang.String, int, char, double, float, long, java.lang.Object, short, boolean)
 *
 * Note:
 * The return type is ignored.
 * Assuming the method signature is well formed.
 * The caller is responsible for freeing the returned string.
 *
 * @param[in] vmThread current J9VMThread
 * @param[in] methodSig pointer to J9UTF8 containing the method signature
 *
 * @return a char pointer to the class name within fullQualifiedUTF,
 *         NULL if not successful, but keep application exception instead of OOM.
 */
static char*
convertMethodSignature(J9VMThread *vmThread, J9UTF8 *methodSig)
{
	UDATA i = 0;
	UDATA j = 0;
	U_8 *string = J9UTF8_DATA(methodSig);
	UDATA stringLength = J9UTF8_LENGTH(methodSig);
	UDATA bufferSize = 0;
	char *result = NULL;

	PORT_ACCESS_FROM_VMC(vmThread);

	/* first scan to calculate the buffer size required */
	/* first character is '(' */
	bufferSize += 1;
	for (i = 1; (')' != string[i]); i++) {
		UDATA arity = 0;
		while ('[' == string[i]) {
			arity += 1;
			i += 1;
		}
		switch (string[i]) {
		case 'B': /* FALLTHROUGH */
		case 'C': /* FALLTHROUGH */
		case 'J':
			/* byte, char, long */
			bufferSize += 4;
			break;
		case 'D':
			/* double */
			bufferSize += 6;
			break;
		case 'F': /* FALLTHROUGH */
		case 'S':
			/* float, short */
			bufferSize += 5;
			break;
		case 'I':
			/* int */
			bufferSize += 3;
			break;
		case 'L': {
			i += 1;
			UDATA objSize = 0;
			while (';' != string[i]) {
				objSize += 1;
				i += 1;
			}
			bufferSize += objSize;
			break;
		}
		case 'Z':
			/* boolean */
			bufferSize += 7;
			break;
		default:
			Trc_VM_ConvertMethodSignature_Malformed_Signature(vmThread, stringLength, string, i);
			break;
		}
		bufferSize += (2 * arity);
		if (')' != string[i + 1]) {
			/* ", " */
			bufferSize += 2;
		}
	}
	/* ')' and the extra byte for '\0' */
	bufferSize += 2;
	Trc_VM_ConvertMethodSignature_Signature_BufferSize(vmThread, stringLength, string, bufferSize);
	result = (char *)j9mem_allocate_memory(bufferSize, OMRMEM_CATEGORY_VM);
	if (NULL != result) {
		char *cursor = result;
		UDATA availableSize = bufferSize;

		memset(result, 0, bufferSize);
		/* first character is '(' */
		j9str_printf(PORTLIB, cursor, availableSize, "(");
		cursor += 1;
		availableSize -= 1;
		for (i = 1; (')' != string[i]); i++) {
			UDATA arity = 0;
			while ('[' == string[i]) {
				arity += 1;
				i += 1;
			}
			const char *elementType = NULL;
			if ('L' == string[i]) {
				i += 1;

				UDATA objSize = 0;
				while (';' != string[i]) {
					*cursor = string[i];
					if ('/' == string[i]) {
						*cursor = '.';
					}
					objSize += 1;
					cursor += 1;
					i += 1;
				}
				availableSize -= objSize;
			} else {
				switch (string[i]) {
				case 'B':
					elementType = "byte";
					break;
				case 'C':
					elementType = "char";
					break;
				case 'D':
					elementType = "double";
					break;
				case 'F':
					elementType = "float";
					break;
				case 'I':
					elementType = "int";
					break;
				case 'J':
					elementType = "long";
					break;
				case 'S':
					elementType = "short";
					break;
				case 'Z':
					elementType = "boolean";
					break;
				default:
					Trc_VM_ConvertMethodSignature_Malformed_Signature(vmThread, stringLength, string, i);
					break;
				}
				UDATA elementLength = strlen(elementType);
				j9str_printf(PORTLIB, cursor, availableSize, elementType);
				cursor += elementLength;
				availableSize -= elementLength;
			}
			for(j = 0; j < arity; j++) {
				j9str_printf(PORTLIB, cursor, availableSize, "[]");
				cursor += 2;
				availableSize -= 2;
			}

			if (')' != string[i + 1]) {
				j9str_printf(PORTLIB, cursor, availableSize, ", ");
				cursor += 2;
				availableSize -= 2;
			}
		}
		j9str_printf(PORTLIB, cursor, availableSize, ")");
	} else {
		bufferSize = 0;
	}
	Trc_VM_ConvertMethodSignature_Signature_Result(vmThread, result, bufferSize);

	return result;
}

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
				J9ROMNameAndSignature *fieldNameAndSig = J9ROMFIELDREF_NAMEANDSIGNATURE((J9ROMFieldRef *)cpItem);
				J9UTF8 *fieldName = J9ROMNAMEANDSIGNATURE_NAME(fieldNameAndSig);

				if (JBputfield == bcCurrent) {
					msgTemplate = "Cannot assign field \"%.*s\"";
				} else {
					msgTemplate = "Cannot read field \"%.*s\"";
				}
				msgLen = j9str_printf(PORTLIB, NULL, 0, msgTemplate, J9UTF8_LENGTH(fieldName), J9UTF8_DATA(fieldName));
				npeMsg = (char *)j9mem_allocate_memory(msgLen + 1, OMRMEM_CATEGORY_VM);
				j9str_printf(PORTLIB, npeMsg, msgLen, msgTemplate, J9UTF8_LENGTH(fieldName), J9UTF8_DATA(fieldName));
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
		case JBinvokevirtual: {
			U_16 index = 0;
			J9ROMConstantPoolItem *constantPool = J9_ROM_CP_FROM_ROM_CLASS(romClass);

			if (JBinvokeinterface2 == bcCurrent) {
				index = PARAM_16(bcCurrentPtr + 2, 1); /* get JBinvokeinterface instead */
			} else {
				index = PARAM_16(bcCurrentPtr, 1);
			}
			Trc_VM_GetCompleteNPEMessage_Invoke_Index(vmThread, index);

			J9ROMMethodRef *romMethodRef = (J9ROMMethodRef *)&constantPool[index];
			J9ROMNameAndSignature *methodNameAndSig = J9ROMMETHODREF_NAMEANDSIGNATURE(romMethodRef);
			J9UTF8 *methodName = J9ROMNAMEANDSIGNATURE_NAME(methodNameAndSig);
			const char *VM_NPE_INVOKEMETHOD = "Cannot invoke \"%s.%.*s%s\"";

			J9UTF8 *definingClassFullQualifiedName = J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *)&constantPool[romMethodRef->classRefCPIndex]);
			char *fullyQualifiedDefiningClassName = convertToJavaFullyQualifiedName(vmThread, definingClassFullQualifiedName);
			char *methodSigParameters = convertMethodSignature(vmThread, J9ROMNAMEANDSIGNATURE_SIGNATURE(methodNameAndSig));

			if ((NULL != definingClassFullQualifiedName) && (NULL != methodSigParameters)) {
				msgLen = j9str_printf(PORTLIB, NULL, 0, VM_NPE_INVOKEMETHOD,
					fullyQualifiedDefiningClassName, J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName), methodSigParameters);
				npeMsg = (char *)j9mem_allocate_memory(msgLen + 1, OMRMEM_CATEGORY_VM);
				j9str_printf(PORTLIB, npeMsg, msgLen, VM_NPE_INVOKEMETHOD,
					fullyQualifiedDefiningClassName, J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName), methodSigParameters);
			}
			j9mem_free_memory(fullyQualifiedDefiningClassName);
			j9mem_free_memory(methodSigParameters);
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
