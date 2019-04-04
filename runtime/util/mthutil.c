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

#include "j9.h"
#include "rommeth.h"
#include "util_internal.h"
#include "cfreader.h"
#include "ut_j9vmutil.h"
#include "j9cp.h"

VMINLINE J9MethodParametersData *
methodParametersFromROMMethod(J9ROMMethod *romMethod)
{
	U_32 * stackMap;

	stackMap = stackMapFromROMMethod(romMethod);

	/* skip the StackMap section if there is one */
	if (J9ROMMETHOD_HAS_STACK_MAP(romMethod)) {
		U_32 stackMapSize = *stackMap;
		return (J9MethodParametersData *)((UDATA)stackMap + stackMapSize);
	}
	return (J9MethodParametersData *)stackMap;
}

/*
 * Returns the first ROM method following the argument.
 * If this is called on the last ROM method in a ROM class
 * it will return an undefined value.
 */
J9ROMMethod*
nextROMMethod(J9ROMMethod * romMethod)
{
	void *result;
	J9MethodParametersData* methodParamsData = methodParametersFromROMMethod(romMethod);
	/* skip the MethodParameters section if there is one */
	if (J9ROMMETHOD_HAS_METHOD_PARAMETERS(romMethod)) {
		return J9_U32_ALIGNED_ADDRESS((U_8*)methodParamsData + J9_METHOD_PARAMS_SIZE_FROM_NUMBER_OF_PARAMS(methodParamsData->parameterCount));
	} else {
		result = methodParamsData;
	}
	return result;
}

static VMINLINE U_32 *
methodAnnotationsDataFromROMMethod(J9ROMMethod *romMethod)
{
	J9ExceptionInfo* exceptionInfo;
	void* result;

	exceptionInfo = J9_EXCEPTION_DATA_FROM_ROM_METHOD(romMethod);

	/* skip the exception table if there is one */
	if (J9ROMMETHOD_HAS_EXCEPTION_INFO(romMethod)) {
		result = J9EXCEPTIONINFO_THROWNAMES(exceptionInfo) + exceptionInfo->throwCount;
	} else {
		result = exceptionInfo;
	}
	return (U_32 *)result;
}

U_32 *
getMethodAnnotationsDataFromROMMethod(J9ROMMethod *romMethod)
{
	if (J9ROMMETHOD_HAS_ANNOTATIONS_DATA(romMethod)) {
		return methodAnnotationsDataFromROMMethod(romMethod);
	}
	return NULL;

}

/* annotation is a U_32 * that points to the length of annotation.
 * result is a void * that will be updated to hold a pointer to after the padding annotation.
 */
#define SKIP_OVER_LENGTH_DATA_AND_PADDING(annotation, result) \
do { \
	UDATA bytesToPad = sizeof(U_32) - (*annotation)%sizeof(U_32);\
	if (sizeof(U_32)==bytesToPad) {\
		bytesToPad = 0;\
	} \
	/* Add the size of the annotation data (*annotation) and the size of the size of the annotation (4) and a pad to U_32*/\
	result = (void *)((UDATA)annotation + *annotation + 4 + bytesToPad);\
} while (0);

U_32
getExtendedModifiersDataFromROMMethod(J9ROMMethod *romMethod)
{
	U_32 extendedModifiers = 0;
	if (J9ROMMETHOD_HAS_EXTENDED_MODIFIERS(romMethod)) {
		extendedModifiers = *J9_EXTENDED_MODIFIERS_ADDR_FROM_ROM_METHOD(romMethod);
	}
	return extendedModifiers;
}

static VMINLINE U_32 *
parameterAnnotationsFromROMMethod(J9ROMMethod *romMethod)
{
	U_32* annotation;
	void* result;

	annotation = methodAnnotationsDataFromROMMethod(romMethod);

	/* skip the AnnotationsData section if there is one */
	if (J9ROMMETHOD_HAS_ANNOTATIONS_DATA(romMethod)) {
		SKIP_OVER_LENGTH_DATA_AND_PADDING(annotation, result);
	} else {
		result = annotation;
	}
	return (U_32 *)result;
}

U_32 *
getParameterAnnotationsDataFromROMMethod(J9ROMMethod *romMethod)
{
	if (J9ROMMETHOD_HAS_PARAMETER_ANNOTATIONS(romMethod)) {
		return parameterAnnotationsFromROMMethod(romMethod);
	}
	return NULL;

}

static VMINLINE U_32 *
defaultAnnotationFromROMMethod(J9ROMMethod *romMethod)
{
	U_32* annotation;
	void* result;

	annotation = parameterAnnotationsFromROMMethod(romMethod);

	/* skip the ParameterAnnotations section if there is one */
	if (J9ROMMETHOD_HAS_PARAMETER_ANNOTATIONS(romMethod)) {
		SKIP_OVER_LENGTH_DATA_AND_PADDING(annotation, result);
	} else {
		result = annotation;
	}
	return (U_32 *)result;
}

U_32 *
getDefaultAnnotationDataFromROMMethod(J9ROMMethod *romMethod)
{
	if (J9ROMMETHOD_HAS_DEFAULT_ANNOTATION(romMethod)) {
		return defaultAnnotationFromROMMethod(romMethod);
	}
	return NULL;

}

static VMINLINE U_32 *
methodTypeAnnotationsFromROMMethod(J9ROMMethod *romMethod)
{
	U_32* annotation = defaultAnnotationFromROMMethod(romMethod);
	void* result = 0;

	/* skip the DefaultAnnotations section if there is some */
	if (J9ROMMETHOD_HAS_DEFAULT_ANNOTATION(romMethod)) {
		SKIP_OVER_LENGTH_DATA_AND_PADDING(annotation, result);
	} else {
		result = annotation;
	}
	return (U_32 *)result;
}

U_32 *
getMethodTypeAnnotationsDataFromROMMethod(J9ROMMethod *romMethod)
{
	U_32 *result = NULL;
	if (J9ROMMETHOD_HAS_METHOD_TYPE_ANNOTATIONS(getExtendedModifiersDataFromROMMethod(romMethod))) {
		result = methodTypeAnnotationsFromROMMethod(romMethod);
	}
	return result;
}

static VMINLINE U_32 *
codeTypeAnnotationsFromROMMethod(J9ROMMethod *romMethod)
{
	U_32* annotation = methodTypeAnnotationsFromROMMethod(romMethod);
	void* result = 0;

	/* skip the method type annotations section if there is some */
	if (J9ROMMETHOD_HAS_METHOD_TYPE_ANNOTATIONS(getExtendedModifiersDataFromROMMethod(romMethod))) {
		SKIP_OVER_LENGTH_DATA_AND_PADDING(annotation, result);
	} else {
		result = annotation;
	}
	return (U_32 *)result;
}

U_32 *
getCodeTypeAnnotationsDataFromROMMethod(J9ROMMethod *romMethod)
{
	U_32 *result = NULL;
	if (J9ROMMETHOD_HAS_CODE_TYPE_ANNOTATIONS(getExtendedModifiersDataFromROMMethod(romMethod))) {
		result = codeTypeAnnotationsFromROMMethod(romMethod);
	}
	return result;
}

J9Method *
iTableMethodAtIndex(J9Class *interfaceClass, UDATA index)
{
	J9Method *ramMethod = interfaceClass->ramMethods;
	while (0 != index) {
		if (J9ROMMETHOD_IN_ITABLE(J9_ROM_METHOD_FROM_RAM_METHOD(ramMethod))) {
			index -= 1;
		}
		ramMethod += 1;
	}
	return ramMethod;
}

UDATA
getITableIndexWithinDeclaringClass(J9Method *method)
{
	UDATA index = 0;
	J9Class * const methodClass = J9_CLASS_FROM_METHOD(method);
	J9Method *ramMethods = methodClass->ramMethods;
	U_32 *ordering = J9INTERFACECLASS_METHODORDERING(methodClass);
	if (NULL == ordering) {
		while (method != ramMethods) {
			if (J9ROMMETHOD_IN_ITABLE(J9_ROM_METHOD_FROM_RAM_METHOD(ramMethods))) {
				index += 1;
			}
			ramMethods += 1;
		}
	} else {
		for(;;) {
			J9Method *ramMethod = ramMethods + *ordering;
			if (method == ramMethod) {
				break;
			}
			if (J9ROMMETHOD_IN_ITABLE(J9_ROM_METHOD_FROM_RAM_METHOD(ramMethod))) {
				index += 1;
			}
			ordering += 1;
		}
	}
	return index;
}

UDATA
getITableIndexForMethod(J9Method * method, J9Class *targetInterface)
{
	J9Class *methodClass = J9_CLASS_FROM_METHOD(method);
	UDATA skip = 0;
	/* NULL targetInterface implies searching only within methodClass, which may be an obsolete class.
	 * This works because the methods for the local interface always appear first in the iTable, with
	 * extended interface methods appearing after.
	 */
	if (NULL != targetInterface) {
		/* Locate methodClass within the extends chain of targetInterface */
		J9ITable *allInterfaces = (J9ITable*)targetInterface->iTable;
		for(;;) {
			J9Class *interfaceClass = allInterfaces->interfaceClass;
			if (interfaceClass == methodClass) {
				break;
			}
			skip += J9INTERFACECLASS_ITABLEMETHODCOUNT(interfaceClass);
			allInterfaces = allInterfaces->next;
		}
	}
	return getITableIndexWithinDeclaringClass(method) + skip;
}

J9MethodDebugInfo *
methodDebugInfoFromROMMethod(J9ROMMethod *romMethod)
{
	U_32* annotation = codeTypeAnnotationsFromROMMethod(romMethod);
	void* result = NULL;

	/* skip the TypeAnnotations section if there is some */
	if (J9ROMMETHOD_HAS_CODE_TYPE_ANNOTATIONS(getExtendedModifiersDataFromROMMethod(romMethod))) {
		SKIP_OVER_LENGTH_DATA_AND_PADDING(annotation, result);
	} else {
		result = annotation;
	}
	return (J9MethodDebugInfo *)result;
}

J9MethodDebugInfo *
getMethodDebugInfoFromROMMethod(J9ROMMethod *romMethod)
{
	if (J9ROMMETHOD_HAS_DEBUG_INFO(romMethod)) {
		J9MethodDebugInfo* debugInfo = methodDebugInfoFromROMMethod(romMethod);
		if (1 ==(debugInfo->srpToVarInfo & 1)) {
			/* low tag indicates that the debug information is stored inline
			 * in the method */
			return debugInfo;
		} else {
			/* not low tag, debug information is stored out of line
			 * and this is an SRP to it. */
			return SRP_PTR_GET(debugInfo, J9MethodDebugInfo *);
		}
	}
	return NULL;
}


U_32*
stackMapFromROMMethod(J9ROMMethod *romMethod)
{
	J9MethodDebugInfo* debugInfo;
	void* result = NULL;

	debugInfo = methodDebugInfoFromROMMethod(romMethod);

	/* skip over the debug information if it exists*/
	if (J9ROMMETHOD_HAS_DEBUG_INFO(romMethod)) {
		U_32 debugInfoSize = *(U_32*)debugInfo;
		if (1 == (debugInfoSize & 1)) {
			/* low tagged, indicates debug information is stored 
			 * inline in the rom method and this is the size
			 * mask out the low tag */
			result = (U_8*)debugInfo + (debugInfoSize&~1);
		} else {
			/* not low tag, debug info is stored out of line
			 * skip over the SRP to that data */
			result = (U_8*)debugInfo + sizeof(J9SRP);
		}
	} else {
		result = debugInfo;
	}

	return (U_32 *)result;
}


U_32 *
getStackMapInfoForROMMethod(J9ROMMethod *romMethod)
{
	if (J9ROMMETHOD_HAS_STACK_MAP(romMethod)) {
		return stackMapFromROMMethod(romMethod);
	}
	return NULL;
}


J9MethodParametersData *
getMethodParametersFromROMMethod(J9ROMMethod *romMethod)
{
	if (J9ROMMETHOD_HAS_METHOD_PARAMETERS(romMethod)) {
		return methodParametersFromROMMethod(romMethod);
	}
	return NULL;
}


#define GET_BE_U16(x) ( (((U_16) x[0])<< 8) | ((U_16) x[1]) )


static VMINLINE U_8 *
walkStackMapSlots(U_8 *framePointer, U_16 typeInfoCount)
{
	/*
	 * Type info entries are variable sized and distinguished by a 8-bit type tag.
	 * Most entry types contain only the tag. The exceptions are described below.
	 */
	U_16 typeInfoIndex = 0;
	for(typeInfoIndex = 0; typeInfoIndex < typeInfoCount; ++typeInfoIndex) {
		U_8 typeInfoType = *framePointer;
		framePointer += 1; /* Extract typeInfoType */
		switch (typeInfoType) {
		case CFR_STACKMAP_TYPE_OBJECT:
			/*
			 * OBJECT {
			 *		U_8 type
			 * 		U_16 cpIndex
			 * };
			 */
			framePointer += 2; /* Skip cpIndex */
			break;
		case CFR_STACKMAP_TYPE_NEW_OBJECT:
			/*
			 * NEW_OBJECT {
			 *		U_8 type
			 * 		U_16 offset
			 * };
			 */
			framePointer += 2; /* Skip offset */
			break;
		case CFR_STACKMAP_TYPE_BYTE_ARRAY: /* fall through 7 cases */
		case CFR_STACKMAP_TYPE_BOOL_ARRAY:
		case CFR_STACKMAP_TYPE_CHAR_ARRAY:
		case CFR_STACKMAP_TYPE_DOUBLE_ARRAY:
		case CFR_STACKMAP_TYPE_FLOAT_ARRAY:
		case CFR_STACKMAP_TYPE_INT_ARRAY:
		case CFR_STACKMAP_TYPE_LONG_ARRAY:
		case CFR_STACKMAP_TYPE_SHORT_ARRAY:
			framePointer += 2; /* skip primitive array type */
			break;

		default:
			/* do nothing */
			break;
		}
	}

	return framePointer;
}

/**
 * Return the next stack map from the StackMap attribute for the ROM Method.
 * NOTE: It is the responsibility of the caller of this function to know when it has read
 * the final frame. Calling this function again after having reached the final frame has undefined
 * results.
 *
 * @param stackMap         - the StackMap attribute from the ROMMethod
 * @param previousFrame    - a pointer to the previously returned stack frame,
 *                           NULL is a directive to get the first stack map
 *
 * @return a pointer to the next stack map frame or NULL when there are no more frames.
 */
UDATA *
getNextStackMapFrame(U_32 *stackMap, UDATA *previousFrame)
{
	/*
	 *  *** ROMClass Stack Map format see ROMClassWriter.cpp for up to date details ***
	 * u4 attribute_length  (native endian)
	 * u2 number_of_entries  (big endian)
	 * stack_map_frame entries[number_of_entries]  (big endian)
	 * padding for u4 alignment
	 */


	UDATA * nextFrame = NULL;

	if ( NULL == previousFrame ) {
		/* get first frame */
		nextFrame = (UDATA *) (((U_8*)stackMap)+sizeof(U_32)+sizeof(U_16));
	} else {
		/* Given the previous frame, find the next frame.
		 * This involves walking the 'previousFrame'to find it's end.

		 * Stack Map Frame:
		 * consists of a 1 byte tag followed by zero or more bytes depending on the tag
		 *
		 * union stack_map_frame {
		 *	SAME
		 *	SAME_LOCALS_1_STACK
		 *	Reserved
		 *	SAME_LOCALS_1_STACK_EXTENDED
		 *	CHOP
		 *	SAME_EXTENDED
		 *	APPEND
		 *	FULL
		 * }
		 */
		U_8 *framePointer = (U_8*)previousFrame;
		U_8 tag = *framePointer;
		framePointer += 1;

		if (CFR_STACKMAP_SAME_LOCALS_1_STACK > tag) { /* 0..63 */
			/* SAME frame - no extra data */
		} else if (CFR_STACKMAP_SAME_LOCALS_1_STACK_END > tag) { /* 64..127 */
			/*
			 * SAME_LOCALS_1_STACK {
			 * 		TypeInfo stackItems[1]
			 * };
			 */
			framePointer = walkStackMapSlots(framePointer, 1);
		} else if (CFR_STACKMAP_SAME_LOCALS_1_STACK_EXTENDED > tag) { /* 128..246 */
			/* Reserved frame types - no extra data */
			Assert_VMUtil_ShouldNeverHappen();
		} else if (CFR_STACKMAP_SAME_LOCALS_1_STACK_EXTENDED == tag) { /* 247 */
			/*
			 * SAME_LOCALS_1_STACK_EXTENDED {
			 *		U_16 offsetDelta
			 *		TypeInfo stackItems[1]
			 * };
			 */
			framePointer += 2; /* Skip offsetDelta */
			framePointer = walkStackMapSlots(framePointer, 1);
		} else if (CFR_STACKMAP_SAME_EXTENDED > tag) { /* 248..250 */
			/*
			 * CHOP {
			 *		U_16 offsetDelta
			 * };
			 */
			framePointer += 2; /* Skip offsetDelta */
		} else if (CFR_STACKMAP_SAME_EXTENDED == tag) { /* 251 */
			/*
			 * SAME_EXTENDED {
			 *		U_16 offsetDelta
			 * };
			 */
			framePointer += 2; /* Skip offsetDelta */
		} else if (CFR_STACKMAP_FULL > tag) { /* 252..254 */
			/*
			 * APPEND {
			 *		U_16 offsetDelta
			 *		TypeInfo locals[frameType - CFR_STACKMAP_APPEND_BASE]
			 * };
			 */
			framePointer += 2; /* Skip offsetDelta */
			framePointer = walkStackMapSlots(framePointer, tag - CFR_STACKMAP_APPEND_BASE);
		} else if (CFR_STACKMAP_FULL == tag) { /* 255 */
			/*
			 * FULL {
			 *		U_16 offsetDelta
			 *		U_16 localsCount
			 *		TypeInfo locals[localsCount]
			 *		U_16 stackItemsCount
			 *		TypeInfo stackItems[stackItemsCount]
			 * };
			 */
			U_16 stackItemsCount = 0;
			U_16 localsCount = 0;
			framePointer += 2; /* Skip offsetDelta */
			localsCount = GET_BE_U16(framePointer);
			framePointer += 2; /* Extract localsCount */
			framePointer = walkStackMapSlots(framePointer, localsCount);
			stackItemsCount = GET_BE_U16(framePointer);
			framePointer += 2; /* Extract stackItemsCount */
			framePointer = walkStackMapSlots(framePointer, stackItemsCount);
		}

		nextFrame = (UDATA *) framePointer;
	}
	return nextFrame;
}


/**
 * Returns the index of the specified method within the
 * specified class, or UDATA_MAX if the method is not part of
 * the class.
 *
 * @param[in] method - the method
 * @return - the index of the method (0-based) or UDATA_MAX
 * 		if the method is not part of the class
 */
static VMINLINE UDATA
methodIndexWithinClass(J9Method *method, J9Class *clazz)
{
	J9ROMClass * romClass = clazz->romClass;
	UDATA methodCount = romClass->romMethodCount;
	UDATA methodIndex = method - clazz->ramMethods;

	if ((methodIndex >= methodCount) ||
		((((UDATA)method - (UDATA)clazz->ramMethods) % sizeof(J9Method)) != 0)
	) {
		methodIndex = UDATA_MAX;
	}
	return methodIndex;
}

UDATA
getMethodIndexUnchecked(J9Method *method)
{
	J9Class * declaringClass = J9_CLASS_FROM_METHOD(method);
	UDATA methodIndex = methodIndexWithinClass(method, declaringClass);

	/* If the method is not within its own declaring class, search
	 * the entire chain of replacements for this class.
	 */
	if (UDATA_MAX == methodIndex) {
		declaringClass = J9_CURRENT_CLASS(declaringClass);
		do {
			methodIndex = methodIndexWithinClass(method, declaringClass);
			if (UDATA_MAX != methodIndex) {
				break;
			}
			declaringClass = declaringClass->replacedClass;
		} while (NULL != declaringClass);
	}

	/*
	 * JAZZ 55908: move the assertion to getMethodIndex()
	 * so as to ignore the case when processing a walkback
	 */

	return methodIndex;
}

UDATA
getMethodIndex(J9Method *method)
{
	UDATA methodIndex = UDATA_MAX;

	methodIndex = getMethodIndexUnchecked(method);
	Assert_VMUtil_true(UDATA_MAX != methodIndex);

	return methodIndex;
}

IDATA
compareMethodNameAndSignature(
		U_8 *aNameData, U_16 aNameLength, U_8 *aSigData, U_16 aSigLength,
		U_8 *bNameData, U_16 bNameLength, U_8 *bSigData, U_16 bSigLength) 
{
	if (aNameLength != bNameLength) {
		return aNameLength > bNameLength ? 1 : -1;
	} else if (aSigLength != bSigLength) {
		return aSigLength > bSigLength ? 1 : -1;
	} else {
		IDATA nameCmp = memcmp(aNameData, bNameData, aNameLength);
		if (0 != nameCmp) {
			return nameCmp;
		}
		return memcmp(aSigData, bSigData, aSigLength);
	}
}

IDATA
compareMethodNameAndPartialSignature(
		U_8 *aNameData, U_16 aNameLength, U_8 *aSigData, U_16 aSigLength,
		U_8 *bNameData, U_16 bNameLength, U_8 *bSigData, U_16 bSigLength) 
{
	IDATA result = 0;
	if (aNameLength != bNameLength) {
		result =  (aNameLength > bNameLength) ? 1 : -1;
	} else {
		IDATA nameCmp = memcmp(aNameData, bNameData, aNameLength);
		if (0 != nameCmp) {
			result = nameCmp;
		} else {
			U_16 compareLength = OMR_MIN(aSigLength, bSigLength);
			result = memcmp(aSigData, bSigData, compareLength);
		}
	}
	return result;
}
