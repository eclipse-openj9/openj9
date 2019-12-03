/*******************************************************************************
 * Copyright (c) 1991, 2020 IBM Corp. and others
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

#include "bcvcfr.h"
#include "bcverify.h"
#include "bcverify_internal.h"
#include "cfreader.h" 
#include "j9protos.h" 
#include "j9consts.h" 
#include "vrfyconvert.h"
#include "j9cp.h"
#include "j9bcvnls.h"
#include "rommeth.h"
#include "ut_j9bcverify.h"

#define SUPERCLASS(clazz) ((clazz)->superclasses[ J9CLASS_DEPTH(clazz) - 1 ])
#define J9CLASS_INDEX_FROM_CLASS_ENTRY(clazz) ((clazz & BCV_CLASS_INDEX_MASK) >> BCV_CLASS_INDEX_SHIFT)
#define J9CLASS_ARITY_FROM_CLASS_ENTRY(clazz)	(((clazz & BCV_ARITY_MASK) >> BCV_ARITY_SHIFT) + ((clazz & BCV_TAG_BASE_ARRAY_OR_NULL) >> 1))

#define CLONEABLE_CLASS_NAME "java/lang/Cloneable"
#define SERIALIZEABLE_CLASS_NAME "java/io/Serializable"
#define CLONEABLE_CLASS_NAME_LENGTH (sizeof(CLONEABLE_CLASS_NAME) - 1)
#define SERIALIZEABLE_CLASS_NAME_LENGTH (sizeof(SERIALIZEABLE_CLASS_NAME) - 1)

static VMINLINE UDATA compareTwoUTF8s (J9UTF8 * first, J9UTF8 * second);
static UDATA addClassName (J9BytecodeVerificationData * verifyData, U_8 * name, UDATA length, UDATA index);
static IDATA findFieldFromRamClass (J9Class ** ramClass, J9ROMFieldRef * field, UDATA firstSearch);
static IDATA findMethodFromRamClass (J9BytecodeVerificationData * verifyData, J9Class ** ramClass, J9ROMNameAndSignature * method, UDATA firstSearch);
static VMINLINE UDATA * pushType (J9BytecodeVerificationData *verifyData, U_8 * signature, UDATA * stackTop);
static IDATA isRAMClassCompatible(J9BytecodeVerificationData *verifyData, U_8* parentClass, UDATA parentLength, U_8* childClass, UDATA childLength, IDATA *reasonCode);

J9_DECLARE_CONSTANT_UTF8(j9_vrfy_Object, "java/lang/Object");
J9_DECLARE_CONSTANT_UTF8(j9_vrfy_String, "java/lang/String");
J9_DECLARE_CONSTANT_UTF8(j9_vrfy_Throwable, "java/lang/Throwable");
J9_DECLARE_CONSTANT_UTF8(j9_vrfy_Class, "java/lang/Class");
J9_DECLARE_CONSTANT_UTF8(j9_vrfy_MethodType, "java/lang/invoke/MethodType");
J9_DECLARE_CONSTANT_UTF8(j9_vrfy_MethodHandle, "java/lang/invoke/MethodHandle");


/* return the new index in the class list table for this name, add it if missing
 * returns BCV_ERR_INSUFFICIENT_MEMORY on OOM
 */
static UDATA 
addClassName(J9BytecodeVerificationData * verifyData, U_8 * name, UDATA length, UDATA index)
{
	J9ROMClass * romClass = verifyData->romClass;
	U_32 *offset;
	J9UTF8 *utf8;
	UDATA classNameInClass = TRUE;
	UDATA newSize, i, delta;
	U_8 *newBuffer;
	PORT_ACCESS_FROM_PORT(verifyData->portLib);

	/* grow the buffers if necessary - will ask for an always conservative amount - assumes classNameInClass = FALSE */
	if ((verifyData->classNameSegmentFree + sizeof(UDATA) + sizeof(J9UTF8) + length + sizeof(UDATA)) >= verifyData->classNameSegmentEnd) {
		/* Do some magic to grow the classNameSegment. */
		newSize = (UDATA) (((length + sizeof(UDATA) + sizeof(J9UTF8) + sizeof(UDATA)) < (32 * (sizeof(UDATA) + sizeof(J9UTF8))))
										? (32 * (sizeof(UDATA) + sizeof(J9UTF8)))
										: (length + sizeof(UDATA) + sizeof(J9UTF8) + sizeof(UDATA) - 1) & ~(sizeof(UDATA) - 1));
		delta = (UDATA) (verifyData->classNameSegmentFree - verifyData->classNameSegment);
		newSize += verifyData->classNameSegmentEnd - verifyData->classNameSegment;
		newBuffer =  j9mem_allocate_memory( newSize , J9MEM_CATEGORY_CLASSES);
		if( !newBuffer ) {
			return BCV_ERR_INSUFFICIENT_MEMORY; /* fail */
		}
		verifyData->classNameSegmentFree = newBuffer + delta;
		memcpy( newBuffer, verifyData->classNameSegment, (UDATA) (verifyData->classNameSegmentEnd - verifyData->classNameSegment) );
		delta = (UDATA) ((newBuffer - verifyData->classNameSegment) / sizeof (J9UTF8));
		bcvfree( verifyData, verifyData->classNameSegment );
		/* adjust the classNameList for the moved strings */
		i = 0;
		while (verifyData->classNameList[i]) {
			/* Second pass verification can have new strings and preverify data references - move only the new string references */
			if (((U_8 *)(verifyData->classNameList[i]) >= verifyData->classNameSegment) && ((U_8 *)(verifyData->classNameList[i]) < verifyData->classNameSegmentEnd)) {
				verifyData->classNameList[i] += delta;
			}
			i++;
		}
		verifyData->classNameSegment = newBuffer;
		verifyData->classNameSegmentEnd = newBuffer + newSize;
	} 

	if (&(verifyData->classNameList[index + 1]) >= verifyData->classNameListEnd) {
		/* Do some magic to grow the classNameList. */
		newSize = (UDATA) ((U_8 *) verifyData->classNameListEnd - (U_8 *) verifyData->classNameList + (32 * sizeof(UDATA)));
		newBuffer =  j9mem_allocate_memory( newSize , J9MEM_CATEGORY_CLASSES);
		if( !newBuffer ) {
			return BCV_ERR_INSUFFICIENT_MEMORY; /* fail */
		}
		memcpy( newBuffer, (U_8*) verifyData->classNameList, (UDATA) (((U_8*) verifyData->classNameListEnd) - ((U_8*) verifyData->classNameList)) );
		bcvfree( verifyData, verifyData->classNameList );
		verifyData->classNameList = (J9UTF8 **) newBuffer;
		verifyData->classNameListEnd = (J9UTF8 **) (newBuffer + newSize);
	}

	/* Is the new name found in the ROM class? */
	if (((UDATA) name < (UDATA) romClass) || ((UDATA) name >= ((UDATA) romClass + (UDATA) romClass->romSize))) {
		classNameInClass = FALSE;
	}

	offset = (U_32 *) verifyData->classNameSegmentFree;
	utf8 = (J9UTF8 *) (offset + 1);
	J9UTF8_SET_LENGTH(utf8, (U_16) length);
	verifyData->classNameSegmentFree += (sizeof(U_32) + sizeof(J9UTF8));
	if (classNameInClass) {
		offset[0] = (U_32) ((UDATA) name - (UDATA) romClass);
	} else {
		offset[0] = 0;
		strncpy((char *) J9UTF8_DATA(utf8), (char *) name, length);
		verifyData->classNameSegmentFree += (length - 2 + sizeof(U_32) - 1) & ~(sizeof(U_32) - 1);	/* next U_32 boundary */
	}
	verifyData->classNameList[index] = (J9UTF8 *) offset;
	verifyData->classNameList[index + 1] = NULL;


	return index;
}



/* Return the new index in the class list table for this name, add it if missing */

UDATA 
findClassName(J9BytecodeVerificationData * verifyData, U_8 * name, UDATA length)
{
	J9ROMClass * romClass = verifyData->romClass;
	U_32 *offset = NULL;
	U_8 *data = NULL;
	J9UTF8 *utf8 = NULL;
	UDATA index = 0;



	while ((offset = (U_32 *) verifyData->classNameList[index]) != NULL) {
		utf8 = (J9UTF8 *) offset + 1;
		if ((UDATA) J9UTF8_LENGTH(utf8) == length) {
			data = (U_8 *) ((UDATA) offset[0] + (UDATA) romClass);

			/* check for identical pointers */
			if (data == name) {
				return index;
			}

			/* handle names found not in the romClass, but in the classNameSegment only */
			if (0 == offset[0]) {
				data = J9UTF8_DATA(utf8);
			}

			if (0 == memcmp(data, name, length)) {
				return index;
			}

		}
		index++;
	}

	/* add a class name if not found */
	return addClassName(verifyData, name, length, index);
}

/* Return the encoded stackmap type for the class.  add class into class list table if missing */

UDATA
convertClassNameToStackMapType(J9BytecodeVerificationData * verifyData, U_8 *name, U_16 length, UDATA type, UDATA arity)
{
	J9ROMClass * romClass = verifyData->romClass;
	U_32 *offset = NULL;
	U_8 *data = NULL;
	J9UTF8 *utf8 = NULL;
	J9UTF8 *utf8_p = NULL;
	UDATA index = 0;


	while ((offset = (U_32 *) verifyData->classNameList[index]) != NULL) {
		utf8 = (J9UTF8 *) offset + 1;
		if ((UDATA) J9UTF8_LENGTH(utf8) == (UDATA)length) {
			data = (U_8 *) ((UDATA) offset[0] + (UDATA) romClass);

			/* check for identical pointers */
			if (data == name) {
				return type  | (index << BCV_CLASS_INDEX_SHIFT);
			}

			/* handle names found not in the romClass, but in the classNameSegment only */
			if (0 == offset[0]) {
				data = J9UTF8_DATA(utf8);
			}

			if (0 == memcmp(data, name, (UDATA)length)) {
				return type | (index << BCV_CLASS_INDEX_SHIFT);
			}

		}
		index++;
	}

	/* add a class name if not found */
	return type | (addClassName(verifyData, name, length, index) << BCV_CLASS_INDEX_SHIFT);
}


/**
 * @returns BOOLEAN indicating if uninitializedThis is on the stack
 */
BOOLEAN
buildStackFromMethodSignature( J9BytecodeVerificationData *verifyData, UDATA **stackTopPtr, UDATA *argCount)
{
	/* 	@romMethod is a pointer to a romMethod structure
		@stackTop is a pointer to a pointer to the start of the stack (grows towards larger values)

	The modified stack is the return result, stackTop is modified accordingly.
	*/
	const J9ROMClass *romClass = verifyData->romClass;
	const J9ROMMethod *romMethod = verifyData->romMethod;
	const UDATA argMax = J9_ARG_COUNT_FROM_ROM_METHOD(romMethod);
	UDATA i;
	U_8 *args;
	UDATA arity, baseType;
	UDATA classIndex = 0;
	UDATA *stackTop;
	BOOLEAN isUninitializedThis = FALSE;

	stackTop = *stackTopPtr;
	*argCount = 0;

	/* if this is a virtual method, push the receiver */

	if ((!(romMethod->modifiers & J9AccStatic)) && (argMax > 0)) {	
		/* this is a virtual method, an object compatible with this class is on the stack */
		J9UTF8* utf8string  = J9ROMMETHOD_NAME(romMethod);
		J9UTF8 *className = J9ROMCLASS_CLASSNAME(romClass);

		classIndex = findClassName(verifyData, J9UTF8_DATA(className), J9UTF8_LENGTH(className));

		/* In the <init> method of Object the type of this is Object.  In other <init> methods, the type of this is uninitializedThis */
		if ((J9UTF8_DATA(utf8string)[0] == '<')	&& (J9UTF8_DATA(utf8string)[1] == 'i') && (classIndex != BCV_JAVA_LANG_OBJECT_INDEX)) {
			/* This is <init>, not java/lang/Object */
			PUSH(BCV_SPECIAL_INIT | (classIndex << BCV_CLASS_INDEX_SHIFT));
			isUninitializedThis = TRUE;
		} else {
			PUSH(BCV_GENERIC_OBJECT | (classIndex << BCV_CLASS_INDEX_SHIFT));
		}
		(*argCount)++;
	}

	/* Walk the signature of the method to determine the arg shape */

	args = J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(romMethod));
 
	for (i = 1; args[i] != ')'; i++) {
		(*argCount)++;
		if ((*argCount) > argMax) {
			continue;
		}
		arity = 0;
		while (args[i] == '[') {
			i++;
			arity++;
		}

		if (args[i] == 'L') {
			U_8 *string;
			U_16 length = 0;

			i++;
			string = &args[i];	/* remember the start of the string */
			while (args[i] != ';') {
				i++;
				length++;
			}
			classIndex = convertClassNameToStackMapType(verifyData, string, length, 0, arity);
			PUSH(classIndex | (arity << BCV_ARITY_SHIFT));
		} else {
			if (arity) {
				/* Base arrays have implicit arity of 1 */
				arity--;
				PUSH(BCV_TAG_BASE_ARRAY_OR_NULL | (UDATA) baseTypeCharConversion[args[i] - 'A'] | (arity << BCV_ARITY_SHIFT));
			} else {
				baseType = (UDATA) argTypeCharConversion[args[i] - 'A'];
				PUSH(baseType);
				if ((args[i] == 'J') || (args[i] == 'D')) {
					(*argCount)++;
					PUSH(BCV_BASE_TYPE_TOP);
				}
			}
		}
	}

	for (i = 0; i < J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod); i++) {
		/* Now push a bunch of uninitialized variables */
		PUSH (BCV_BASE_TYPE_TOP);
	}
	*stackTopPtr = stackTop;

	return isUninitializedThis;
}


UDATA* 
pushReturnType(J9BytecodeVerificationData *verifyData, J9UTF8 * utf8string, UDATA * stackTop)
{
	U_8 *signature;

	/* TODO: Determine if faster to walk utf8string backwards to get the return type as
	 * there is only 1 return type and potentially many arg types: ie:
	 * (Ljava/lang/String;II)Ljava/lang/String;  is fewer iterations if walking backwards
	 *
	 * signature = J9UTF8_DATA(utf8string) + length - 1;
	 * while (*signature-- != ')');
	 */

	signature = J9UTF8_DATA(utf8string);
	while (*signature++ != ')');

	return pushType(verifyData, signature, stackTop);
}



UDATA* 
pushFieldType(J9BytecodeVerificationData *verifyData, J9UTF8 * utf8string, UDATA *stackTop)
{
	return pushType(verifyData, J9UTF8_DATA(utf8string), stackTop);
}




UDATA *
pushClassType(J9BytecodeVerificationData * verifyData, J9UTF8 * utf8string, UDATA * stackTop)
{
	if (J9UTF8_DATA(utf8string)[0] == '[') {
		UDATA arrayType = parseObjectOrArrayName(verifyData, J9UTF8_DATA(utf8string));
		PUSH(arrayType);
	} else {
		PUSH(convertClassNameToStackMapType(verifyData, J9UTF8_DATA(utf8string),J9UTF8_LENGTH(utf8string), BCV_OBJECT_OR_ARRAY, 0));
	}

	return stackTop;
}



void 
initializeClassNameList(J9BytecodeVerificationData *verifyData)
{
	J9UTF8 *name;

	/* reset the pointer and zero terminate the class name list */
	verifyData->classNameSegmentFree = verifyData->classNameSegment;
	verifyData->classNameList[0] = NULL;

	/* Add the "known" classes to the classNameList.  The order
	 * here must exactly match the indexes as listed in bytecodewalk.h.
	 */

	/* BCV_JAVA_LANG_OBJECT_INDEX 0 */
	name = (J9UTF8*)&j9_vrfy_Object;
	findClassName( verifyData, J9UTF8_DATA(name), J9UTF8_LENGTH(name));
	/* BCV_JAVA_LANG_STRING_INDEX 1 */
	name = (J9UTF8*)&j9_vrfy_String;
	findClassName( verifyData, J9UTF8_DATA(name), J9UTF8_LENGTH(name));
	/* BCV_JAVA_LANG_THROWABLE_INDEX 2 */
	name = (J9UTF8*)&j9_vrfy_Throwable;
	findClassName( verifyData, J9UTF8_DATA(name), J9UTF8_LENGTH(name));
	/* BCV_JAVA_LANG_CLASS_INDEX 3 */
	name = (J9UTF8*)&j9_vrfy_Class;
	findClassName( verifyData, J9UTF8_DATA(name), J9UTF8_LENGTH(name));
	/* BCV_JAVA_LANG_INVOKE_METHOD_TYPE_INDEX 4 */
	name = (J9UTF8*)&j9_vrfy_MethodType;
	findClassName( verifyData, J9UTF8_DATA(name), J9UTF8_LENGTH(name));
	/* BCV_JAVA_LANG_INVOKE_METHODHANDLE_INDEX 5 */
	name = (J9UTF8*)&j9_vrfy_MethodHandle;
	findClassName( verifyData, J9UTF8_DATA(name), J9UTF8_LENGTH(name));
}


UDATA * 
pushLdcType(J9BytecodeVerificationData *verifyData, J9ROMClass * romClass, UDATA index, UDATA * stackTop)
{
	switch(J9_CP_TYPE(J9ROMCLASS_CPSHAPEDESCRIPTION(romClass), index)) {
		case J9CPTYPE_CLASS:
			PUSH(BCV_JAVA_LANG_CLASS_INDEX << BCV_CLASS_INDEX_SHIFT);
			break;
		case J9CPTYPE_STRING:
			PUSH(BCV_JAVA_LANG_STRING_INDEX << BCV_CLASS_INDEX_SHIFT);
			break;
		case J9CPTYPE_INT:
			PUSH_INTEGER_CONSTANT;
			break;
		case J9CPTYPE_FLOAT:
			PUSH_FLOAT_CONSTANT;
			break;
		case J9CPTYPE_METHOD_TYPE:
			PUSH(BCV_JAVA_LANG_INVOKE_METHOD_TYPE_INDEX << BCV_CLASS_INDEX_SHIFT);
			break;
		case J9CPTYPE_METHODHANDLE:
			PUSH(BCV_JAVA_LANG_INVOKE_METHODHANDLE_INDEX << BCV_CLASS_INDEX_SHIFT);
			break;
		case J9CPTYPE_CONSTANT_DYNAMIC:
			{
				J9ROMConstantDynamicRef* romConstantDynamicRef = (J9ROMConstantDynamicRef *)(J9_ROM_CP_FROM_ROM_CLASS(romClass) + index);
				J9UTF8 *signature = J9ROMNAMEANDSIGNATURE_SIGNATURE(J9ROMCONSTANTDYNAMICREF_NAMEANDSIGNATURE(romConstantDynamicRef));
				/* The signature referenced by a ConstantDynamic entry is a field descriptor */
				stackTop = pushType(verifyData, J9UTF8_DATA(signature), stackTop);
			}
			break;
	}

	return stackTop;
}



/* 
 * NOTE:
 *  targetClass must be:
 *		- null
 *		- an object
 *		- an array of objects
 *		- an array of base types
 *  sourceClass can be any valid type descriptor, including NULL, base types, and special objects
 *
 *  returns TRUE if class are compatible
 *  returns FALSE if class are NOT compatible
 *  		isInterfaceClass() or isRAMClassCompatible() sets reasonCode to BCV_ERR_INSUFFICIENT_MEMORY in OOM
 */

IDATA 
isClassCompatible(J9BytecodeVerificationData *verifyData, UDATA sourceClass, UDATA targetClass, IDATA *reasonCode)
{ 
	UDATA sourceIndex, targetIndex;
	UDATA sourceArity, targetArity;
	IDATA rc;
	U_8 *sourceName, *targetName;
	UDATA sourceLength, targetLength;
	BOOLEAN classRelationshipSnippetsEnabled = (NULL == verifyData->classRelationshipSnippetsHashTable) ? FALSE : TRUE;

	*reasonCode = 0;

	/* if they are identical, then we're done */
	if( sourceClass == targetClass ) {
		return (IDATA) TRUE;
	}

	/* NULL is magically compatible */
	if( sourceClass == BCV_BASE_TYPE_NULL ) {
		return (IDATA) TRUE;
	}

	/* Covers the following cases:
	 * 1) Objects are not compatible with base types
	 * 2) uninitialized(x) can only merge to itself (covered by srcClass == trgClass check)
	 * 3) uninitialized_this can only merge to itself (covered by srcClass == trgClass check)
	 */
	if( sourceClass & BCV_BASE_OR_SPECIAL ) {
		return (IDATA) FALSE;
	} 

	/* if the target is java/lang/Object, we're done */
	if( targetClass == (BCV_JAVA_LANG_OBJECT_INDEX << BCV_CLASS_INDEX_SHIFT) ) {
		return (IDATA) TRUE;
	}

	/* only NULL is compatible with NULL */
	if ( targetClass == BCV_BASE_TYPE_NULL ) {
		return (IDATA) FALSE;
	}

	sourceArity = J9CLASS_ARITY_FROM_CLASS_ENTRY(sourceClass);
	targetArity = J9CLASS_ARITY_FROM_CLASS_ENTRY(targetClass);
	
	/* You can't cast to a larger arity */
	if( targetArity > sourceArity ) {
		return (IDATA) FALSE;
	}

	/* load up the indices, but be aware that these might be base type arrays */
	sourceIndex = J9CLASS_INDEX_FROM_CLASS_ENTRY(sourceClass);
	targetIndex = J9CLASS_INDEX_FROM_CLASS_ENTRY(targetClass);

	if( targetArity < sourceArity ) {
		/* if target is an interface, or an object -- but we need to make sure its not a base type array */
		if (targetClass & BCV_TAG_BASE_ARRAY_OR_NULL) {
			return (IDATA) FALSE;
		}

		if (targetIndex == BCV_JAVA_LANG_OBJECT_INDEX) {
			return (IDATA) TRUE;
		}

		getNameAndLengthFromClassNameList(verifyData, targetIndex, &targetName, &targetLength);

		/* Jazz 109803: Java 8 VM Spec at 4.10.1.2 Verification Type System says:
		 * For assignments, interfaces are treated like Object.
		 * isJavaAssignable(class(_, _), class(To, L)) :- loadedClass(To, L, ToClass), classIsInterface(ToClass).
		 * isJavaAssignable(From, To) :- isJavaSubclassOf(From, To).
		 *
		 * Array types are subtypes of Object. The intent is also that array types
		 * are subtypes of Cloneable and java.io.Serializable.
		 * isJavaAssignable(arrayOf(_), class('java/lang/Object', BL)) :- isBootstrapLoader(BL).
		 * isJavaAssignable(arrayOf(_), X) :- isArrayInterface(X).
		 * isArrayInterface(class('java/lang/Cloneable', BL)) :- isBootstrapLoader(BL).
		 * isArrayInterface(class('java/io/Serializable', BL)) :- isBootstrapLoader(BL).
		 *
		 * According to the statements above, it emphasizes two cases from the code perspective:
		 * 1) if the arity of both sourceClass and targetClass is 0,
		 *    then targetClass (ToClass) should be an interface for compatibility.
		 * 2) if not the case and sourceClass is an array type (sourceArity > 0),
		 *    array type can't be assigned to arbitrary interface or array of interface
		 *    except Object (already checked above), java/lang/Cloneable and java/io/Serializable,
		 *    which means targetClass must be one/array of Object, java/lang/Cloneable and java/io/Serializable.
		 */
		if (((CLONEABLE_CLASS_NAME_LENGTH == targetLength) && ((0 == strncmp((const char*)targetName, CLONEABLE_CLASS_NAME, CLONEABLE_CLASS_NAME_LENGTH))))
		|| ((SERIALIZEABLE_CLASS_NAME_LENGTH == targetLength) && (0 == strncmp((const char*)targetName, SERIALIZEABLE_CLASS_NAME, SERIALIZEABLE_CLASS_NAME_LENGTH)))
		) {
			rc = isInterfaceClass(verifyData, targetName, targetLength, reasonCode);

			if (BCV_ERR_CLASS_RELATIONSHIP_RECORD_REQUIRED == *reasonCode) {
				if (classRelationshipSnippetsEnabled) {
					rc = j9bcv_recordClassRelationshipSnippet(verifyData, sourceIndex, targetIndex, reasonCode);
				} else {
					getNameAndLengthFromClassNameList(verifyData, sourceIndex, &sourceName, &sourceLength);
					rc = j9bcv_recordClassRelationship(verifyData->vmStruct, verifyData->classLoader, sourceName, sourceLength, targetName, targetLength, reasonCode);
				}
			}

			return rc;
		}

		return (IDATA) FALSE;
	}

	/* At this point we know the arity is equal -- see if either is a base type array */
	if( (sourceClass & BCV_TAG_BASE_ARRAY_OR_NULL) || (targetClass & BCV_TAG_BASE_ARRAY_OR_NULL) ) {
		/* then they should have been identical, so fail */
		return (IDATA) FALSE;
	}
	
	if (targetIndex == BCV_JAVA_LANG_OBJECT_INDEX) {
		return (IDATA) TRUE;
	}

	getNameAndLengthFromClassNameList(verifyData, targetIndex, &targetName, &targetLength);

	/* if the target is an interface, be permissive */
	rc = isInterfaceClass(verifyData, targetName, targetLength, reasonCode);

	if (BCV_ERR_CLASS_RELATIONSHIP_RECORD_REQUIRED == *reasonCode) {
		if (classRelationshipSnippetsEnabled) {
			rc = j9bcv_recordClassRelationshipSnippet(verifyData, sourceIndex, targetIndex, reasonCode);
		} else {
			getNameAndLengthFromClassNameList(verifyData, sourceIndex, &sourceName, &sourceLength);
			rc = j9bcv_recordClassRelationship(verifyData->vmStruct, verifyData->classLoader, sourceName, sourceLength, targetName, targetLength, reasonCode);
		}
	}

	if ((IDATA) FALSE != rc) {
		return rc;
	}

	if (NULL != verifyData->vmStruct->currentException) {
		return (IDATA) FALSE;
	}

	getNameAndLengthFromClassNameList(verifyData, sourceIndex, &sourceName, &sourceLength);
	rc = isRAMClassCompatible(verifyData, targetName, targetLength , sourceName, sourceLength, reasonCode);

	if (BCV_ERR_CLASS_RELATIONSHIP_RECORD_REQUIRED == *reasonCode) {
		if (classRelationshipSnippetsEnabled) {
			rc = j9bcv_recordClassRelationshipSnippet(verifyData, sourceIndex, targetIndex, reasonCode);
		} else {
			getNameAndLengthFromClassNameList(verifyData, sourceIndex, &sourceName, &sourceLength);
			rc = j9bcv_recordClassRelationship(verifyData->vmStruct, verifyData->classLoader, sourceName, sourceLength, targetName, targetLength, reasonCode);
		}
	}

	return rc;
}

/*
	API
		@verifyData		-	internal data structure
		@parentClass	-	U_8 pointer to parent class name
		@parentLength	- UDATA length of parent class name
		@childClass 	-	U_8 pointer to child class name
		@childLength	- UDATA length of child class name

	If the parentClass is not a super class of the child class, answer FALSE, otherwise answer TRUE.
		When FALSE, reasonCode (set by j9rtv_verifierGetRAMClass) is
			BCV_ERR_INSUFFICIENT_MEMORY :in OOM error case


*/
static IDATA
isRAMClassCompatible(J9BytecodeVerificationData *verifyData, U_8* parentClass, UDATA parentLength, U_8* childClass, UDATA childLength, IDATA *reasonCode)
{
	J9Class *sourceRAMClass, *targetRAMClass;

	/* Go get the ROM class for the source and target */
	targetRAMClass = j9rtv_verifierGetRAMClass( verifyData, verifyData->classLoader, parentClass, parentLength, reasonCode );
	if (NULL == targetRAMClass) {
		return FALSE;
	}

	/* if the target is an interface, be permissive */
	if( targetRAMClass->romClass->modifiers & J9AccInterface ) {
		return (IDATA) TRUE;
	}

	sourceRAMClass = j9rtv_verifierGetRAMClass( verifyData, verifyData->classLoader, childClass, childLength, reasonCode );
	if (NULL == sourceRAMClass) {
		return FALSE;
	}

	targetRAMClass = J9_CURRENT_CLASS(targetRAMClass);
	return isSameOrSuperClassOf( targetRAMClass, sourceRAMClass );
}


/*
	API
		@verifyData	-	internal data structure
		@className	-	U_8 pointer to class name
		@classLength	- UDATA length of class name

	If class is an interface answer TRUE, otherwise answer FALSE.
		When FALSE, reasonCode (set by j9rtv_verifierGetRAMClass) is
			BCV_ERR_INSUFFICIENT_MEMORY :in OOM error case


*/
IDATA
isInterfaceClass(J9BytecodeVerificationData * verifyData, U_8* className, UDATA classLength, IDATA *reasonCode)
{
	J9Class *ramClass;

	*reasonCode = 0;

	ramClass = j9rtv_verifierGetRAMClass(verifyData, verifyData->classLoader, className, classLength, reasonCode);
	if (NULL == ramClass) {
		return FALSE;
	}

	/* if the target is an interface, be permissive */
	if (ramClass->romClass->modifiers & J9AccInterface) {
		return TRUE;
	}

	return FALSE;
}


static UDATA *
pushType(J9BytecodeVerificationData *verifyData, U_8 * signature, UDATA * stackTop)
{
	if (*signature != 'V') {
		if ((*signature == '[') || (*signature == 'L')) {
			PUSH(parseObjectOrArrayName(verifyData, signature));
		} else {
			UDATA baseType = (UDATA) argTypeCharConversion[*signature - 'A'];
			PUSH(baseType);
			if ((*signature == 'J') || (*signature == 'D')) {
				PUSH(BCV_BASE_TYPE_TOP);
			}
		}
	}

	return stackTop;
}



/*
 * decode the 'special' type described by type, and answer it's normal type.
 * If type is not a NEW or INIT object, return type unchanged
 */

UDATA 
getSpecialType(J9BytecodeVerificationData *verifyData, UDATA type, U_8* bytecodes)
{
	J9ROMClass * romClass = verifyData->romClass;
	J9UTF8* newClassUTF8;

	/* verify that this is a NEW object */
	if ((type & BCV_SPECIAL_NEW) == (UDATA) (BCV_SPECIAL_NEW)) {
		/* Lookup the Class from the cpEntry used in the 'JBnew' bytecode */
		U_8* bcTempPtr;
		U_16 index;
		J9ROMConstantPoolItem* constantPool;

		bcTempPtr = bytecodes + ((type & BCV_CLASS_INDEX_MASK) >> BCV_CLASS_INDEX_SHIFT);

		index = PARAM_16(bcTempPtr, 1);

		constantPool = (J9ROMConstantPoolItem *) ((U_8 *) romClass + sizeof(J9ROMClass));
		newClassUTF8 = J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *) (&constantPool[index]));
	} else {
		/* verify that this is an INIT object  and un-tag it */
		 if ((type & BCV_SPECIAL_INIT) == (UDATA) (BCV_SPECIAL_INIT)) {
			newClassUTF8 = J9ROMCLASS_CLASSNAME(romClass);
		} else {
			return type;
		}
	}

	return convertClassNameToStackMapType(verifyData, J9UTF8_DATA(newClassUTF8), J9UTF8_LENGTH(newClassUTF8), 0, 0);
}


/*
 * Validates classes compatibility by their name
 *
 * returns TRUE if class are compatible
 * returns FALSE it not compatible
 * 		reasonCode (set by isClassCompatible) is:
 *			BCV_ERR_INSUFFICIENT_MEMORY :in OOM error case
 */
IDATA 
isClassCompatibleByName(J9BytecodeVerificationData *verifyData, UDATA sourceClass, U_8* targetClassName, UDATA targetClassNameLength, IDATA *reasonCode)
{ 
	UDATA index;

	*reasonCode = 0;

	/* NULL is magically compatible */
	if( sourceClass == BCV_BASE_TYPE_NULL ) 
		return (IDATA) TRUE;

	/* If the source is special, or a base type -- fail */
	if( sourceClass & BCV_BASE_OR_SPECIAL ) 
		return (IDATA) FALSE;

	if (*targetClassName == '[') {
		index = parseObjectOrArrayName(verifyData, targetClassName);
	} else {
		index = convertClassNameToStackMapType(verifyData, targetClassName, (U_16)targetClassNameLength, 0, 0);
	}

	return isClassCompatible(verifyData, sourceClass, index, reasonCode);
}



/* This function is used by the bytecode verifier */

U_8 *
j9bcv_createVerifyErrorString(J9PortLibrary * portLib, J9BytecodeVerificationData * error)
{
	const char *formatString;
	const char *errorString;
	U_8 *verifyError;
	UDATA stringLength = 0;
	/* Jazz 82615: Statistics data indicates the buffer size of 97% JCK cases is less than 1K */
	U_8 byteArray[1024];
	U_8* detailedErrMsg = NULL;
	UDATA detailedErrMsgLength = 0;

	PORT_ACCESS_FROM_PORT(portLib);

	if ((IDATA) error->errorCode == -1) {
		/* We were called with an uninitialized buffer, just return a generic error */
		return NULL;
	}

	if ((IDATA)error->errorModule == -1) {
		return NULL;
	}

	if ((IDATA) error->errorPC == -1) {
		/* J9NLS_BCV_ERROR_TEMPLATE_NO_PC=%1$s; class=%3$.*2$s, method=%5$.*4$s%7$.*6$s */
		formatString = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_BCV_ERROR_TEMPLATE_NO_PC, "%s; class=%.*s, method=%.*s%.*s");
	} else {
		/* Jazz 82615: Generate the error message framework by default.
		 * The error message framework is not required when the -XX:-VerifyErrorDetails option is specified.
		 */
		if (J9_ARE_ALL_BITS_SET(error->verificationFlags, J9_VERIFY_ERROR_DETAILS)
		&& (0 != error->errorDetailCode)
		) {
			/* Jazz 82615: The value of detailedErrMsg may change if the initial byteArray is insufficient to contain the error message framework.
			 * Under such circumstances, it points to the address of newly allocated memory.
			 */
			detailedErrMsg = byteArray;
			detailedErrMsgLength = sizeof(byteArray);
			/* In the case of failure to allocating memory in building the error message framework,
			 * detailedErrMsg is set to NULL and detailedErrMsgLength is set to 0 on return from getRtvExceptionDetails().
			 * Therefore, it ends up with a simple error message as the buffer for the error message framework has been cleaned up.
			 */
			detailedErrMsg = error->javaVM->verboseStruct->getRtvExceptionDetails(error, detailedErrMsg, &detailedErrMsgLength);
		}

		/*Jazz 82615: fetch the corresponding NLS message when type mismatch error is detected */
		if (NULL == error->errorSignatureString) {
			/* J9NLS_BCV_ERROR_TEMPLATE_WITH_PC=%1$s; class=%3$.*2$s, method=%5$.*4$s%7$.*6$s, pc=%8$u */
			formatString = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_BCV_ERROR_TEMPLATE_WITH_PC, "%s; class=%.*s, method=%.*s%.*s, pc=%u");
		} else {
			/*Jazz 82615: J9NLS_BCV_ERROR_TEMPLATE_TYPE_MISMATCH=%1$s; class=%3$.*2$s, method=%5$.*4$s%7$.*6$s, pc=%8$u; Type Mismatch, argument %9$d in signature %11$.*10$s.%13$.*12$s:%15$.*14$s does not match */
			formatString = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_BCV_ERROR_TEMPLATE_TYPE_MISMATCH,
					"%s; class=%.*s, method=%.*s%.*s, pc=%u; Type Mismatch, argument %d in signature %.*s.%.*s:%.*s does not match");
		}
	}

	/* Determine the size of the error string we need to allocate */
	errorString = OMRPORT_FROM_J9PORT(PORTLIB)->nls_lookup_message(OMRPORT_FROM_J9PORT(PORTLIB), J9NLS_DO_NOT_APPEND_NEWLINE, (U_32)(error->errorModule), (U_32)(error->errorCode), NULL);
	stringLength = strlen(errorString);

	stringLength += J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(error->romClass));
	stringLength += J9UTF8_LENGTH(J9ROMMETHOD_NAME(error->romMethod));
	stringLength += J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(error->romMethod));
	stringLength += 10; /* for the PC */
	if (NULL != error->errorSignatureString) {
		stringLength += 10;  /* for the argument index */
		stringLength += J9UTF8_LENGTH(error->errorClassString);
		stringLength += J9UTF8_LENGTH(error->errorMethodString);
		stringLength += J9UTF8_LENGTH(error->errorSignatureString);
	}
	stringLength += strlen(formatString);
	stringLength += detailedErrMsgLength;

	verifyError = j9mem_allocate_memory(stringLength, J9MEM_CATEGORY_CLASSES);
	if (NULL != verifyError) {
		UDATA errStrLength = 0;
		J9UTF8 * romClassName = J9ROMCLASS_CLASSNAME(error->romClass);
		J9UTF8 * romMethodName = J9ROMMETHOD_NAME(error->romMethod);
		J9UTF8 * romMethodSignatureString = J9ROMMETHOD_SIGNATURE(error->romMethod);

		if (NULL == error->errorSignatureString) {
			errStrLength = j9str_printf(PORTLIB, (char*) verifyError, stringLength, formatString, errorString,
					(U_32) J9UTF8_LENGTH(romClassName), J9UTF8_DATA(romClassName),
					(U_32) J9UTF8_LENGTH(romMethodName), J9UTF8_DATA(romMethodName),
					(U_32) J9UTF8_LENGTH(romMethodSignatureString), J9UTF8_DATA(romMethodSignatureString),
					error->errorPC);
		} else {
			/* Jazz 82615: Print the corresponding NLS message to buffer for type mismatch error */
			errStrLength = j9str_printf(PORTLIB, (char*) verifyError, stringLength, formatString, errorString,
					(U_32) J9UTF8_LENGTH(romClassName), J9UTF8_DATA(romClassName),
					(U_32) J9UTF8_LENGTH(romMethodName), J9UTF8_DATA(romMethodName),
					(U_32) J9UTF8_LENGTH(romMethodSignatureString), J9UTF8_DATA(romMethodSignatureString),
					error->errorPC,
					error->errorArgumentIndex,
					(U_32) J9UTF8_LENGTH(error->errorClassString), J9UTF8_DATA(error->errorClassString),
					(U_32) J9UTF8_LENGTH(error->errorMethodString), J9UTF8_DATA(error->errorMethodString),
					(U_32) J9UTF8_LENGTH(error->errorSignatureString), J9UTF8_DATA(error->errorSignatureString));
		}

		/* Jazz 82615: Print the error message framework to the existing error buffer */
		if (detailedErrMsgLength > 0) {
			j9str_printf(PORTLIB, (char*)&verifyError[errStrLength], stringLength - errStrLength, "%.*s", detailedErrMsgLength, detailedErrMsg);
		}
	}

	/* Jazz 82615: Release the memory pointed by detailedErrMsg if allocated with new memory in getRtvExceptionDetails */
	if (detailedErrMsg != byteArray) {
		j9mem_free_memory(detailedErrMsg);
	}

	/* reset the error buffer */
	RESET_VERIFY_ERROR(error);

	return verifyError;
}

/*
 * Validates field access compatibility
 *
 * returns TRUE if class are compatible
 * returns FALSE it not compatible
 * 		reasonCode (set by isClassCompatibleByName) is:
 *			BCV_ERR_INSUFFICIENT_MEMORY :in OOM error case
 */
IDATA 
isFieldAccessCompatible(J9BytecodeVerificationData * verifyData, J9ROMFieldRef * fieldRef, UDATA bytecode, UDATA receiver, IDATA *reasonCode)
{
	J9ROMClass * romClass = verifyData->romClass;
	J9ROMConstantPoolItem * constantPool = (J9ROMConstantPoolItem *) (romClass + 1);
	J9UTF8 * utf8string = J9ROMCLASSREF_NAME((J9ROMClassRef *) &constantPool[fieldRef->classRefCPIndex]);

	*reasonCode = 0;

	if (bytecode == 181) {/* JBputfield */
		if ((receiver & BCV_SPECIAL_INIT) == (UDATA) BCV_SPECIAL_INIT) {
			J9UTF8 *classString = ((J9UTF8 *) J9ROMCLASS_CLASSNAME(romClass));
			if (utf8string != classString) {
				/* The following test is not necessary if the class name is uniquely referenced in a class */
				if (J9UTF8_LENGTH(utf8string) == J9UTF8_LENGTH(classString)) {
					IDATA i;

					/* Reversed scan typically faster for inequality - most classes share common prefixes */
					for (i = (IDATA) (J9UTF8_LENGTH(utf8string) - 1); i >= 0; i--) {
						if (J9UTF8_DATA(utf8string)[i] != J9UTF8_DATA(classString)[i]) {
							break;
						}
					}
					/* check for comparison success */
					if (i < 0) {
						return (IDATA) TRUE;
					}
				}
				return (IDATA) FALSE;
			} else {
				return (IDATA) TRUE;
			}
		}
	}
	return isClassCompatibleByName(verifyData, receiver, J9UTF8_DATA(utf8string), J9UTF8_LENGTH(utf8string), reasonCode);
}

/* Determine if protected member access is permitted.
 *   currentClass is the current class
 *   declaringClass is the class which defines the member
 *   targetClass is the class for which the member is being used
 *   member: either a J9ROMFieldRef* or a J9ROMNameAndSignature*
 *
 * returns TRUE if protected access is permitted,
 * returns FALSE if protected access is not permitted
 *		reasonCode (set by j9rtv_verifierGetRAMClass) is :
 *         	BCV_ERR_INSUFFICIENT_MEMORY if OOM is encountered
 */

UDATA 
isProtectedAccessPermitted(J9BytecodeVerificationData *verifyData, J9UTF8* declaringClassName, UDATA targetClass, void* member, UDATA isField, IDATA *reasonCode )
{
	J9ROMClass * romClass = verifyData->romClass;
	
	*reasonCode = 0;

	/* for performance reasons, don't do protection checks unless -Xfuture or -Xverify:doProtectedAccessCheck is specified */
	if ((0 == (verifyData->vmStruct->javaVM->runtimeFlags & J9RuntimeFlagXfuture)) 
	&& (0 == (verifyData->verificationFlags & J9_VERIFY_DO_PROTECTED_ACCESS_CHECK))
	) {
		return TRUE;
	}

	/* Skip protection checks for arrays, they are in the NULL package */
	if (J9CLASS_ARITY_FROM_CLASS_ENTRY(targetClass) == 0) {
		J9Class * definingRamClass;
		J9Class * currentRamClass;
		J9UTF8 * currentClassName;
		IDATA rc;

		/* Short circuit if the classes are the same */
		currentClassName = J9ROMCLASS_CLASSNAME(romClass);
		if (compareTwoUTF8s(declaringClassName, currentClassName)) return TRUE;

		/* Get the ram classes for the current and defining classes */
		currentRamClass = j9rtv_verifierGetRAMClass (verifyData, verifyData->classLoader, J9UTF8_DATA(currentClassName), J9UTF8_LENGTH(currentClassName), reasonCode);
		if ((NULL == currentRamClass) && (BCV_ERR_INSUFFICIENT_MEMORY == *reasonCode)) {
			return FALSE;
		}

		/* short circuit - check currentRamClass hierarchy for member - if not found, protected access is permitted */
		definingRamClass = currentRamClass;
		if (isField) {
			rc = findFieldFromRamClass(&definingRamClass, (J9ROMFieldRef *) member, FALSE);
		} else {
			rc = findMethodFromRamClass(verifyData, &definingRamClass, (J9ROMNameAndSignature *) member, FALSE);
		}

		/* Return if member is not found if currentRamClass hierarchy - access okay */
		if (BCV_NOT_FOUND == rc) return TRUE;

		/* Now chase the definition from the declaringClass */
		definingRamClass = j9rtv_verifierGetRAMClass (verifyData, verifyData->classLoader, J9UTF8_DATA(declaringClassName), J9UTF8_LENGTH(declaringClassName), reasonCode);
		if (NULL == definingRamClass) {
			return FALSE;
		}

		/* Jazz 106868:
		 * According to VM Spec at 4.10.1.8 Type Checking for protected Members,
		 * the protected check applies only to protected members of superclasses of the current class
		 * protected members in other classes will be caught by the access checking done at resolution.
		 * So we ignore the protected checking since declaringClass is not the same or super class of currentClass.
		 */
		if (!isSameOrSuperClassOf (definingRamClass, currentRamClass)) {
		    return TRUE;
		}

		if (isField) {
			rc = findFieldFromRamClass(&definingRamClass, (J9ROMFieldRef *) member, TRUE);
		} else {
			rc = findMethodFromRamClass(verifyData, &definingRamClass, (J9ROMNameAndSignature *) member, TRUE);
		}

		/* Return if the member is not protected or not found */
		if ((BCV_SUCCESS == rc) || (BCV_NOT_FOUND == rc)) {
			return TRUE;
		}

		/* Check if in the same package */
		if (currentRamClass->packageID == definingRamClass->packageID) return TRUE;

		/* Determine if the defining class is the same class or a super class of current class */
		if (isSameOrSuperClassOf (definingRamClass, currentRamClass)) {
			U_8 * targetClassName;
			UDATA targetClassLength;
			J9Class * targetRamClass;

			/* NULL is compatible */
			if (targetClass != BCV_BASE_TYPE_NULL) {
				/* Get the targetRamClass */
				getNameAndLengthFromClassNameList(verifyData, J9CLASS_INDEX_FROM_CLASS_ENTRY(targetClass), &targetClassName, &targetClassLength);
				targetRamClass = j9rtv_verifierGetRAMClass (verifyData, verifyData->classLoader, targetClassName, targetClassLength, reasonCode);
				if (NULL == targetRamClass) {
					return FALSE;
				}

				/* Determine if the targetRamClass is the same class or a sub class of the current class */
				/* flipped logic - currentRamClass is the same class or a super class of the target class */
				if (!isSameOrSuperClassOf (currentRamClass, targetRamClass)) {
					/* fail */
					return FALSE;
				}
			}
		}
	}
	return TRUE;
}



/* return TRUE if identical, FALSE otherwise */
static UDATA compareTwoUTF8s(J9UTF8 * first, J9UTF8 * second)
{
	U_8 * f = J9UTF8_DATA(first);
	U_8 * s = J9UTF8_DATA(second);

	if (J9UTF8_LENGTH(first) != J9UTF8_LENGTH(second)) return 0;

	return (memcmp(f, s, J9UTF8_LENGTH(first)) == 0);
}



void getNameAndLengthFromClassNameList(J9BytecodeVerificationData *verifyData, UDATA listIndex, U_8 **name, UDATA *length)
{
	U_32 * offset;

	offset = (U_32 *) verifyData->classNameList[listIndex];
	*length = (U_32) J9UTF8_LENGTH(offset + 1);
	if (offset[0] == 0) {
		*name = J9UTF8_DATA(offset + 1);
	} else {
		J9ROMClass * romClass = verifyData->romClass;
		*name = (U_8 *) ((UDATA) offset[0] + (UDATA) romClass);
	}
}

/* return BCV_SUCCESS if field is found
 * return BCV_NOT_FOUND if field is not found
 * return BCV_FAIL otherwise
 */

static IDATA findFieldFromRamClass (J9Class ** ramClass, J9ROMFieldRef * field, UDATA firstSearch)
{
	J9UTF8 * searchName = J9ROMNAMEANDSIGNATURE_NAME(J9ROMFIELDREF_NAMEANDSIGNATURE(field));

	for (;;) {
		J9ROMFieldShape * currentField;
		J9ROMClass * romClass = (*ramClass)->romClass;
		J9ROMFieldWalkState state;

		/* Walk the instance fields looking for a match */
		currentField = romFieldsStartDo(romClass, &state);
		while (currentField != NULL) {
			if ((currentField->modifiers & J9AccStatic) == 0) {
				if (compareTwoUTF8s(searchName, J9ROMFIELDSHAPE_NAME(currentField))) {
					if (currentField->modifiers & CFR_ACC_PROTECTED) return BCV_FAIL;
					if (firstSearch) return BCV_SUCCESS;
				}
			}
			currentField = romFieldsNextDo(&state);
		}

		/* Next try the super class */
		*ramClass = SUPERCLASS(*ramClass);
		if (NULL == *ramClass) return BCV_NOT_FOUND;
	}
}

/* return BCV_SUCCESS if method is found
 * return BCV_NOT_FOUND if method is not found
 * return BCV_FAIL otherwise
 */
static IDATA 
findMethodFromRamClass(J9BytecodeVerificationData * verifyData, J9Class ** ramClass, J9ROMNameAndSignature * method, UDATA firstSearch)
{
	J9UTF8 * searchName = J9ROMNAMEANDSIGNATURE_NAME(method);
	J9UTF8 * searchSignature = J9ROMNAMEANDSIGNATURE_SIGNATURE(method);

	for (;;) {
		J9ROMClass * romClass = (*ramClass)->romClass;
		UDATA redefinedClassIndex = 0;
		J9UTF8 * currentClassName = J9ROMCLASS_CLASSNAME(romClass);
		J9ROMMethod * currentRomMethod = NULL;
		UDATA methodIndex = 0;

		/* CMVC 193469: Walk through the redefined classes looking for a class name that
		 *              matches the current class name and use the new class instead */
		for (redefinedClassIndex = 0; redefinedClassIndex < verifyData->redefinedClassesCount; redefinedClassIndex++) {
			J9ROMClass * currentRedefinedClass = verifyData->redefinedClasses[redefinedClassIndex].replacementClass.romClass;
			Assert_RTV_true(NULL != currentRedefinedClass);

			if (0 != compareTwoUTF8s(currentClassName, J9ROMCLASS_CLASSNAME(currentRedefinedClass))) {
				/* use the redefined class when checking the methods */
				romClass = currentRedefinedClass;
				break;
			}
		}

		/* Walk the methods looking for a match */
		currentRomMethod = J9ROMCLASS_ROMMETHODS(romClass);
		for (methodIndex = 0; methodIndex < romClass->romMethodCount; methodIndex++) {
			if ((0 != compareTwoUTF8s(searchName, J9ROMMETHOD_NAME(currentRomMethod)))
			&& (0 != compareTwoUTF8s(searchSignature, J9ROMMETHOD_SIGNATURE(currentRomMethod)))
			) {
				if (currentRomMethod->modifiers & CFR_ACC_PROTECTED) return BCV_FAIL;
				if (firstSearch) return BCV_SUCCESS;
			}
			currentRomMethod = J9_NEXT_ROM_METHOD(currentRomMethod);
		}

		/* Next try the super class */
		*ramClass = SUPERCLASS(*ramClass);
		if (NULL == *ramClass) return BCV_NOT_FOUND;
	}
}

UDATA 
parseObjectOrArrayName(J9BytecodeVerificationData *verifyData, U_8 *signature) 
{
	UDATA arity, arrayType;
	U_8 *string = signature;

	while (*signature == '[') {
		signature++;
	}
	arity = (UDATA) (signature - string);
	if (*signature == 'L') {
		U_16 length = 0;
		UDATA classIndex = 0;

		signature++;
		string = signature;	/* remember the start of the string */
		while (*signature++ != ';') {
			length++;
		}
		arrayType = convertClassNameToStackMapType(verifyData, string, length, 0, arity);

	} else {
		/* Base arrays have implicit arity of 1 */
		arity--;
		arrayType = (UDATA) (BCV_TAG_BASE_ARRAY_OR_NULL + (UDATA) baseTypeCharConversion[*signature - 'A']);
	}

	return arrayType | (arity << BCV_ARITY_SHIFT);
}


void
storeVerifyErrorData (J9BytecodeVerificationData * verifyData, I_16 errorDetailCode, U_32 errorCurrentFramePosition, UDATA errorTargetType, UDATA errorTempData, IDATA currentPC)
{
	J9BranchTargetStack *liveStack = (J9BranchTargetStack *) verifyData->liveStack;
	verifyData->errorDetailCode = errorDetailCode;
	verifyData->errorCurrentFramePosition = errorCurrentFramePosition;
	verifyData->errorTargetType = errorTargetType;
	verifyData->errorTempData = errorTempData;

	/* Jazz 82615: Set liveStack->pc to the current pc value in the current frame (liveStack)
	 * info of the detailed error message.
	 */
	liveStack->pc =  (UDATA)currentPC;
}
