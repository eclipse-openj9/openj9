/*******************************************************************************
 * Copyright (c) 2008, 2019 IBM Corp. and others
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
#include "j9cp.h"
#include "rommeth.h"
#include "ute.h"

#define J9_IS_STATIC_SPLIT_TABLE_INDEX(value)		J9_ARE_ALL_BITS_SET(value, J9_STATIC_SPLIT_TABLE_INDEX_FLAG)
#define J9_IS_SPECIAL_SPLIT_TABLE_INDEX(value) 		J9_ARE_ALL_BITS_SET(value, J9_SPECIAL_SPLIT_TABLE_INDEX_FLAG)
#define J9_IS_SPLIT_TABLE_INDEX(value)				J9_ARE_ANY_BITS_SET(value, J9_STATIC_SPLIT_TABLE_INDEX_FLAG | J9_SPECIAL_SPLIT_TABLE_INDEX_FLAG)

J9_DECLARE_CONSTANT_UTF8(newInstancePrototypeName, "newInstancePrototype");
J9_DECLARE_CONSTANT_UTF8(newInstancePrototypeSig, "(Ljava/lang/Class;)Ljava/lang/Object;");
const J9NameAndSignature newInstancePrototypeNameAndSig = { (J9UTF8*)&newInstancePrototypeName, (J9UTF8*)&newInstancePrototypeSig };

static void jitResetAllMethodsAtStartupHelper(J9VMThread *vmStruct, J9Class *root);

/**
 * gets the VM class, provided it is properly initialized.
 * @param vmStruct
 * @param callerCP ROM constant pool
 * @param signatureChars class name
 * @param signatureLength class name length
 * @return named class
 */

J9Class* 
jitGetClassFromUTF8(J9VMThread *vmStruct, J9ConstantPool *callerCP, void *signatureChars, UDATA signatureLength)
{
	return jitGetClassInClassloaderFromUTF8(vmStruct, callerCP->ramClass->classLoader, signatureChars, signatureLength);
}

/**
 * gets the VM class using a classloader, provided it is properly initialized.
 * @param vmStruct the current J9VMThread
 * @param classLoader
 * @param signatureChars class name
 * @param signatureLength class name length
 * @return the J9RAMClass * associated IFF it exists and is resolved in the system classloader.
 */
J9Class*
jitGetClassInClassloaderFromUTF8(J9VMThread *vmStruct, J9ClassLoader *classLoader, void *signatureChars, UDATA signatureLength) 
{
	J9Class *result = NULL;
	
	if (0 != signatureLength){
		result = vmStruct->javaVM->internalVMFunctions->internalFindClassUTF8(vmStruct, signatureChars, 
				signatureLength, classLoader, J9_FINDCLASS_FLAG_EXISTING_ONLY);
	}
	
	/* return NULL if the class failed init */
	if ((NULL != result) && (J9ClassInitFailed == result->initializeStatus)) {
		result = NULL;
	}	
	return result;
}

/**
 * @param vmStruct, the current J9VMThread
 * @param constantPool, the constant pool that the cpIndex is referring to
 * @param fieldIndex, the index of an entry in a constant pool, pointing at a static field ref.
 * 
 * @return the J9RAMClass associated IFF it exists and is resolved in the classloader associated
 * with constantPool
 */

J9Class*
jitGetClassOfFieldFromCP(J9VMThread *vmStruct, J9ConstantPool *constantPool, UDATA fieldIndex) 
{
	J9RAMStaticFieldRef *ramRefWrapper;
	J9Class *result = NULL;

	/* romConstantPool is a J9ROMConstantPoolItem */
	ramRefWrapper = ((J9RAMStaticFieldRef*) constantPool) + fieldIndex;
	if (J9RAMSTATICFIELDREF_IS_RESOLVED(ramRefWrapper)) {
		J9Class *classWrapper = J9RAMSTATICFIELDREF_CLASS(ramRefWrapper);
		UDATA initStatus = classWrapper->initializeStatus;
		if ((J9ClassInitSucceeded == initStatus) || ((UDATA) vmStruct == initStatus)) {
			result = classWrapper;
		}
	}
	return result;
}


/**
 * @param vmTread
 */

void  
jitAcquireClassTableMutex (J9VMThread *vmThread) 
{
	
	omrthread_monitor_enter (vmThread->javaVM->classTableMutex);
}

/**
 * @param vmTread
 */

void  
jitReleaseClassTableMutex (J9VMThread *vmThread) 
{
	
	omrthread_monitor_exit (vmThread->javaVM->classTableMutex);
}
 
/**
 * @param fieldIndex the index of an entry in a constant pool
 * @param ramMethod  the method from which the cpIndex was taken
 * @return information about the field.
 * Note that this function no longer returns the resolution status of the field
 * (defect 144239). 
 */

UDATA  
jitGetFieldType (UDATA fieldIndex, J9Method *ramMethod) 
{
	UDATA result = 0;
	J9ConstantPool* constantPool;
	J9UTF8 *signature;
	J9ROMFieldRef *romFieldRef;
	J9ROMNameAndSignature *nameAndSig;
	U_8 sigChar;

	constantPool = J9_CP_FROM_METHOD(ramMethod);

	romFieldRef = (J9ROMFieldRef*) &(((J9ROMConstantPoolItem*) constantPool->romConstantPool)[fieldIndex]);
	nameAndSig = J9ROMFIELDREF_NAMEANDSIGNATURE(romFieldRef);
	signature = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig);
	sigChar = J9UTF8_DATA(signature)[0];

	/* case on the character, setting the correct bits in the result */
	switch (sigChar) {
	case 'Z': result = J9FieldTypeBoolean; break;
	case 'C': result = J9FieldTypeChar; break;
	case 'F': result = J9FieldTypeFloat; break;
	case 'D': result = (J9FieldSizeDouble + J9FieldTypeDouble);  break;
	case 'B': result = J9FieldTypeByte; break;
	case 'S': result = J9FieldTypeShort; break;
	case 'I': result = J9FieldTypeInt; break;
	case 'J': result = (J9FieldSizeDouble + J9FieldTypeLong); break;
	default: result = J9FieldFlagObject; break;
	}
	/* we know that the Field* constants are defined to be << 16.  Move them back. */
	result = result >> 16;

	return result;
}

/**
 * Given a J9UTF8 *, fill:
 * @param paramBuffer an array of U_8 with the param types, plus the return type
 * @param paramElements with the number of params
 * @param paramSlots with the number of VM slots the params use
 * All return values returned by reference
 */
void  
jitParseSignature (const J9UTF8 *signature, U_8 *paramBuffer, UDATA *paramElements, UDATA *paramSlots)
{
	typedef enum ParseState {
		argList, 
		returnValue, 
		endOfSignature = 0x10000000 /* ensure 32-bit data width */
	} ParseState;
	UDATA slotCount = 0;
	UDATA paramCount = 0;
	const U_8 *sigChar;
	ParseState state = argList;

	sigChar = J9UTF8_DATA(signature)+1; /* skip the opening parenthesis */
	while (endOfSignature != state) {
		U_8 next = 0;

		if (')' == *sigChar) {
			/* these local variables get corrupted by the last pass through the loop */
			*paramElements = paramCount;
			*paramSlots = slotCount;
			state = returnValue;
		} else {
			switch (*sigChar) {
			case 'L': next = J9_NATIVE_TYPE_OBJECT; break;
			case '[': 
				next = J9_NATIVE_TYPE_OBJECT; 
				while ('[' == *sigChar) {
					/* 
					 * skip over the remaining open square brackets because 
					 * we don't care about the arity or base type (except to flush class names).
					 */
					++ sigChar;
				}
				break;
			case 'Z': next = J9_NATIVE_TYPE_BOOLEAN; break;
			case 'B': next = J9_NATIVE_TYPE_BYTE; break;
			case 'S': next = J9_NATIVE_TYPE_SHORT; break;
			case 'C': next = J9_NATIVE_TYPE_CHAR; break;
			case 'I': next = J9_NATIVE_TYPE_INT; break;
			case 'J':
				next = J9_NATIVE_TYPE_LONG;
				slotCount += 1; break;
			case 'F': next = J9_NATIVE_TYPE_FLOAT; break;
			case 'D':
				next = J9_NATIVE_TYPE_DOUBLE;
				slotCount += 1; break;
			case 'V': next = J9_NATIVE_TYPE_VOID; break;
			}
			
			if ('L' == *sigChar) {
				/* flush the name of the class */
				while (';' != *sigChar) {
					++sigChar;
				}
			}
			paramCount += 1;
			slotCount += 1;
			*paramBuffer = next;
			++paramBuffer;
			if (returnValue == state) {
				state = endOfSignature;
			}
		}
		++sigChar; /* eat the type char */
	}
}

/**
 * Check if an instance class is an instance of another class, or can be cast to it.
 * @param instanceClass candidate for casting
 * @param castClass class to which to be cast
 * @return 1 if the cast class is the same as, a superclass of, or an implemented interface of instanceClass.
 */
UDATA  
jitCTInstanceOf (J9Class *instanceClass, J9Class *castClass)
{
	return instanceOfOrCheckCast(instanceClass, castClass);
}

/**
 * @param vmStruct
 * @param method
 * @param fieldIndex
 * @param resolveFlags
 * @param resolvedField Updated if resolution succeeds
 * @return -1 if failure.
 */

IDATA  
jitCTResolveInstanceFieldRefWithMethod (J9VMThread *vmStruct, J9Method *method, UDATA fieldIndex, UDATA resolveFlags, J9ROMFieldShape **resolvedField)
{
	J9JavaVM *vm = vmStruct->javaVM;
	J9ConstantPool *constantPool = J9_CP_FROM_METHOD(method);
	IDATA result = 0;
	UDATA resolveFlagsBuffer = 0;
	J9ROMFieldShape *resolvedFieldBuffer;

	resolveFlagsBuffer = J9_RESOLVE_FLAG_JIT_COMPILE_TIME;
	if (0 != resolveFlags) {
		resolveFlagsBuffer |= J9_RESOLVE_FLAG_FIELD_SETTER;
	}
	result = vm->internalVMFunctions->resolveInstanceFieldRefInto(
			vmStruct, method, constantPool, fieldIndex, resolveFlagsBuffer, &resolvedFieldBuffer, NULL);

	if (-1 != result) {
		*resolvedField = resolvedFieldBuffer;
	}

	/* If the JIT is generating inline field watch notification code, there is no need to fail the
	 * compilation when watches are in place.
	 */
	if (J9_ARE_NO_BITS_SET(vm->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_JIT_INLINE_WATCHES)) {
		ALWAYS_TRIGGER_J9HOOK_JIT_CHECK_FOR_DATA_BREAKPOINT(vm->jitConfig->hookInterface,
				vmStruct,
				result,	/* can be modified */
				fieldIndex,
				constantPool,
				*resolvedField,
				0,
				resolveFlags);
	}

	return result;
}

/**
 * @param vmStruct
 * @param method
 * @param fieldIndex
 * @param resolveFlags
 * @param resolvedField Updated if resolution succeeds
 * @return -1 if failure.
 */

void* 
jitCTResolveStaticFieldRefWithMethod (J9VMThread *vmStruct, J9Method *method, UDATA fieldIndex, UDATA resolveFlags, J9ROMFieldShape **resolvedField)
{
	J9JavaVM *vm = vmStruct->javaVM;
	J9ConstantPool *constantPool = J9_CP_FROM_METHOD(method);
	UDATA result = 0;
	UDATA resolveFlagsBuffer = 0;
	J9ROMFieldShape *resolvedFieldBuffer;

	resolveFlagsBuffer = J9_RESOLVE_FLAG_JIT_COMPILE_TIME;
	if (0 != resolveFlags) {
		resolveFlagsBuffer |= J9_RESOLVE_FLAG_FIELD_SETTER;
	}
	result = (UDATA) vm->internalVMFunctions->resolveStaticFieldRefInto(
			vmStruct, method, constantPool, fieldIndex, resolveFlagsBuffer, &resolvedFieldBuffer, NULL);

	if (0 != result) {
		*resolvedField = resolvedFieldBuffer;
	}

	/* If the JIT is generating inline field watch notification code, there is no need to fail the
	 * compilation when watches are in place.
	 */
	if (J9_ARE_NO_BITS_SET(vm->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_JIT_INLINE_WATCHES)) {
		ALWAYS_TRIGGER_J9HOOK_JIT_CHECK_FOR_DATA_BREAKPOINT(vm->jitConfig->hookInterface,
				vmStruct,
				result,	/* can be modified */
				fieldIndex,
				constantPool,
				*resolvedField,
				1,
				resolveFlags);
	}

	return (void* ) result;
}

/**
 * @param vmStruct
 * Walks the class structure using a linked list of classes
 * and initializes the methodRunAddress
 */
void  
jitResetAllMethodsAtStartup (J9VMThread *vmStruct) 
{
	J9JavaVM *javaVM = vmStruct->javaVM;
	J9Class *root = NULL;

	root = J9VMJAVALANGOBJECT(javaVM);
	jitResetAllMethodsAtStartupHelper(vmStruct, root);
}

/**
 * @param vmStruct
 * @param root
 * Walks the class hierarchy starting from the given root class.
 */
static void
jitResetAllMethodsAtStartupHelper(J9VMThread *vmStruct, J9Class *root)
{
	J9JavaVM *javaVM = vmStruct->javaVM;
	J9Class *subclass = NULL;
	J9SubclassWalkState subclassState;

	subclass = allSubclassesStartDo(root, &subclassState, TRUE);
	while (subclass != NULL) { /* walkClassesInDepthOrder */
		UDATA count = subclass->romClass->romMethodCount;
		J9Method *method = subclass->ramMethods;

		while (count != 0) {
			J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);

			if (romMethod->modifiers & J9AccNative) {
				if ((UDATA)method->constantPool & J9_STARTPC_JNI_NATIVE) {
					method->methodRunAddress = javaVM->jniSendTarget;
				}
			} else {
				javaVM->internalVMFunctions->initializeMethodRunAddress(vmStruct, method);
			}
			count -= 1;
			method += 1;
		}
		subclass = allSubclassesNextDo(&subclassState);
	}
}

/**
 * @param constantPool RAM constant pool
 * @param index index of field 
 * @param isStatic set to TRUE to look up a static field, FALSE for instance field
 * @param declaringClass set to the declaring class of the field, or NULL if the lookup fails
 * @return pointer cast to UDATA 
 */
static UDATA
findField(J9VMThread *vmStruct, J9ConstantPool *constantPool, UDATA index, BOOLEAN isStatic, J9Class **declaringClass)
{
	J9JavaVM *javaVM = vmStruct->javaVM;
	J9ROMFieldRef *romRef; 
	J9ROMClassRef *classRef; /* ROM class of the field */
	U_32 classRefCPIndex;
	J9UTF8 *classNameUTF;
	J9Class *clazz;
	UDATA result = 0;

	*declaringClass = NULL;
	romRef = (J9ROMFieldRef*) &(((J9ROMConstantPoolItem*) constantPool->romConstantPool)[index]);
	classRefCPIndex = romRef->classRefCPIndex;
	classRef = (J9ROMClassRef*) &(((J9ROMConstantPoolItem*) constantPool->romConstantPool)[classRefCPIndex]);
	classNameUTF = J9ROMCLASSREF_NAME(classRef);
	clazz = javaVM->internalVMFunctions->internalFindClassUTF8(vmStruct, J9UTF8_DATA(classNameUTF),
			J9UTF8_LENGTH(classNameUTF), constantPool->ramClass->classLoader, J9_FINDCLASS_FLAG_EXISTING_ONLY);
	if (NULL != clazz) {
		J9ROMNameAndSignature *nameAndSig = J9ROMFIELDREF_NAMEANDSIGNATURE(romRef);
		J9UTF8 *signature = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig);
		J9UTF8 *name = J9ROMNAMEANDSIGNATURE_NAME(nameAndSig);

		if (!isStatic) {
			UDATA instanceField;
			IDATA offset = javaVM->internalVMFunctions->instanceFieldOffset(
					vmStruct, clazz, J9UTF8_DATA(name),
					J9UTF8_LENGTH(name), J9UTF8_DATA(signature), J9UTF8_LENGTH(signature), declaringClass,
					&instanceField, J9_LOOK_NO_JAVA);
			
			if (-1 != offset) {
				result = instanceField;
			}
		} else{
			UDATA staticField;
			void * addr = javaVM->internalVMFunctions->staticFieldAddress(
					vmStruct, clazz, J9UTF8_DATA(name),
					J9UTF8_LENGTH(name), J9UTF8_DATA(signature), J9UTF8_LENGTH(signature), declaringClass,
					&staticField, J9_LOOK_NO_JAVA, NULL);

			if (NULL != addr) {
				result = staticField;
			}
		}
	}
	return result;
}

/**
 * @param vmStruct
 * @param cp1 RAM ramConstantPool
 * @param index1 index into cp1
 * @param cp2 RAM ramConstantPool
 * @param index2 index into cp2
 * @param isStatic
 * The two constant pools may refer to a class and its subclass.
 */

UDATA  
jitFieldsAreIdentical (J9VMThread *vmStruct, J9ConstantPool *cp1, UDATA index1, J9ConstantPool *cp2, UDATA index2, UDATA isStatic) 
{
	UDATA identical = 0;
	BOOLEAN compareFields = FALSE;
	
	if (TRUE != isStatic) {
		J9RAMFieldRef *ramRef1 = (J9RAMFieldRef*) &(((J9RAMConstantPoolItem *)cp1)[index1]);
		
		if (!J9RAMFIELDREF_IS_RESOLVED(ramRef1)) {
			compareFields = TRUE;
		} else {
			J9RAMFieldRef *ramRef2 = (J9RAMFieldRef*) &(((J9RAMConstantPoolItem *)cp2)[index2]);
			if (!J9RAMFIELDREF_IS_RESOLVED(ramRef2)) {
				compareFields = TRUE;
			} else if (ramRef1->valueOffset == ramRef2->valueOffset){
				compareFields = TRUE;
			}
		}
	} else { /* fields are static */
		J9RAMStaticFieldRef *ramRef1 = ((J9RAMStaticFieldRef*) cp1) + index1;

		if (!J9RAMSTATICFIELDREF_IS_RESOLVED(ramRef1)) {
			compareFields = TRUE;
		} else {
			J9RAMStaticFieldRef *ramRef2 = ((J9RAMStaticFieldRef*) cp2) + index2;
			if (!J9RAMSTATICFIELDREF_IS_RESOLVED(ramRef2)) {
				compareFields = TRUE;
			} else if (ramRef1->valueOffset == ramRef2->valueOffset){
				compareFields = TRUE;
			}
		}
	}
	if (compareFields) {
		J9Class *c1 = NULL;
		UDATA f1 = findField(vmStruct, cp1, index1, isStatic, &c1);
		if (0 != f1) {
			J9Class *c2 = NULL;
			UDATA f2 = findField(vmStruct, cp2, index2, isStatic, &c2);
			if (0 != f2) {
					identical = ((f1 == f2) && (c1 == c2));
			}
		}
	}		
	return identical;	
}

/**
 * Check if method enter tracing is enabled
 * @param currentThread	The current J9VMThread
 * @param method		the method to be checked if enter tracing is required
 * @return 				non-zero if the method enter tracing is required, otherwise 0 is returned
 */
UDATA
jitMethodEnterTracingEnabled(J9VMThread *currentThread, J9Method *method)
{
	J9JavaVM *vm = currentThread->javaVM;

	if (vm->extendedRuntimeFlags & J9_EXTENDED_RUNTIME_METHOD_TRACE_ENABLED) {
		U_8 *pmethodflags = fetchMethodExtendedFlagsPointer(method);
		if (J9_RAS_IS_METHOD_TRACED(*pmethodflags)) {
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * Check if method exit tracing is enabled
 * @param currentThread	The current J9VMThread
 * @param method		The method to be checked if exit tracing is required
 * @return 				non-zero if the method exit tracing is required, otherwise 0 is returned
 */
UDATA
jitMethodExitTracingEnabled(J9VMThread *currentThread, J9Method *method)
{
	J9JavaVM *vm = currentThread->javaVM;

	if (vm->extendedRuntimeFlags & J9_EXTENDED_RUNTIME_METHOD_TRACE_ENABLED) {
		U_8 *pmethodflags = fetchMethodExtendedFlagsPointer(method);
		if (J9_RAS_IS_METHOD_TRACED(*pmethodflags)) {
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * Check if the current method is breakpointed.  If the method is breakpointed,
 * it should not be inlined.  Note, native methods cannot be breakpointed.
 * @param currentThread	The current J9VMThread
 * @param method		The method to be checked for breakpoints
 * @return 				non-zero if the method has a breakpoint, otherwise 0 is returned
 */
UDATA
jitMethodIsBreakpointed(J9VMThread *currentThread, J9Method *method)
{
	UDATA result = 0;

	if (J9_FSD_ENABLED(currentThread->javaVM)) {
		if (J9_STARTPC_METHOD_BREAKPOINTED == ((UDATA)method->constantPool & J9_STARTPC_METHOD_BREAKPOINTED)) {
			/* Breakpoint bit is re-used in Z/OS JNI natives for offload.  Native methods can never be breakpointed.*/
			const U_32 modifiers = J9_ROM_METHOD_FROM_RAM_METHOD(method)->modifiers;
			if (J9AccNative != (modifiers & J9AccNative)) {
				result = 1;
			}
		}
	}
	return result;
}

/**
 * This function checks if given 'cpOrSplitIndex' is an index into split table.
 * If so, it returns the constant pool index by fetching it from the split table stored in J9ROMClass.
 * Otherwise, 'cpOrSplitIndex' represents an index into constant pool, and is returned as it is.
 *
 * @param currentThread		The current J9VMThread
 * @param romClass			Pointer to J9ROMClass that 'owns' the 'cpOrSplitIndex'
 * @param cpOrSplitIndex	Index of a method. Either an index into constant pool or into split table
 *
 * @return 					Constant pool index of the method represented by 'cpOrSplitIndex'
 */
UDATA
jitGetRealCPIndex(J9VMThread *currentThread, J9ROMClass *romClass, UDATA cpOrSplitIndex)
{
	UDATA realCPIndex = cpOrSplitIndex;

	if (J9_IS_SPLIT_TABLE_INDEX(cpOrSplitIndex)) {
		UDATA splitTableIndex = cpOrSplitIndex & J9_SPLIT_TABLE_INDEX_MASK;

		if (J9_IS_STATIC_SPLIT_TABLE_INDEX(cpOrSplitIndex)) {
			realCPIndex = *(J9ROMCLASS_STATICSPLITMETHODREFINDEXES(romClass) + splitTableIndex);
		} else {
			realCPIndex = *(J9ROMCLASS_SPECIALSPLITMETHODREFINDEXES(romClass) + splitTableIndex);
		}
	}
	return realCPIndex;
}

/**
 * This function returns J9Method represented by 'cpOrSplitIndex'.
 * If 'cpOrSplitIndex' is an index into split table, J9Method is obtained by accessing the split table stored in J9Class.
 * Otherwise, J9Method is obtained by accessing the constant pool of J9Class.
 *
 * @param currentThread		The current J9VMThread
 * @param constantPool		Constant pool of the class that 'owns' the 'cpOrSplitIndex'
 * @param cpOrSplitIndex	Index of a method. Either an index into constant pool or into split table
 *
 * @return 				J9Method represented by 'cpOrSplitIndex'
 */
J9Method *
jitGetJ9MethodUsingIndex(J9VMThread *currentThread, J9ConstantPool *constantPool, UDATA cpOrSplitIndex)
{
	J9Method *method = NULL;

	if (J9_IS_SPLIT_TABLE_INDEX(cpOrSplitIndex)) {
		UDATA splitTableIndex = cpOrSplitIndex & J9_SPLIT_TABLE_INDEX_MASK;
		J9Class *clazz = J9_CLASS_FROM_CP(constantPool);

		if (J9_IS_STATIC_SPLIT_TABLE_INDEX(cpOrSplitIndex)) {
			method = clazz->staticSplitMethodTable[splitTableIndex];
			if (method == (J9Method*)currentThread->javaVM->initialMethods.initialStaticMethod) {
				method = NULL;
			}
		} else {
			method = clazz->specialSplitMethodTable[splitTableIndex];
			if (method == (J9Method*)currentThread->javaVM->initialMethods.initialSpecialMethod) {
				method = NULL;
			}
		}
	} else {
		method = ((J9RAMStaticMethodRef*)(constantPool + cpOrSplitIndex))->method;
	}
	return method;
}

/**
 * This function returns the J9Method obtained by resolving 'cpOrSplitIndex'.
 * If 'cpOrSplitIndex' is an index into split table, then it is resolved by calling resolveStaticSplitMethodRef
 * Otherwise, it is resolved by calling resolveStaticMethodRef
 *
 * @param vmStruct			The current J9VMThread
 * @param constantPool		Constant pool of the class which owns 'cpOrSplitIndex'
 * @param cpOrSplitIndex	Index of a method. Either an index into constant pool or into split table
 * @param resolveFlags		Bit flags used to indicate resolution context
 *
 * @return 					J9Method obtained by resolving 'cpOrSplitIndex'
 */
J9Method *
jitResolveStaticMethodRef(J9VMThread *vmStruct, J9ConstantPool *constantPool, UDATA cpOrSplitIndex, UDATA resolveFlags)
{
	J9Method *method = NULL;

	if (J9_IS_STATIC_SPLIT_TABLE_INDEX(cpOrSplitIndex)) {
		method = vmStruct->javaVM->internalVMFunctions->resolveStaticSplitMethodRef(vmStruct, constantPool, (cpOrSplitIndex & J9_SPLIT_TABLE_INDEX_MASK), resolveFlags);
	} else {
		method = vmStruct->javaVM->internalVMFunctions->resolveStaticMethodRef(vmStruct, constantPool, cpOrSplitIndex, resolveFlags);
	}
	return method;
}


/**
 * This function returns the J9Method obtained by resolving 'cpOrSplitIndex'.
 * If 'cpOrSplitIndex' is an index into split table, then it is resolved by calling resolveSpecialSplitMethodRef
 * Otherwise, it is resolved by calling resolveSpecialMethodRef
 *
 * @param vmStruct			The current J9VMThread
 * @param constantPool		Constant pool of the class which owns 'cpOrSplitIndex'
 * @param cpOrSplitIndex	Index of a method. Either an index into constant pool or into split table
 * @param resolveFlags		Bit flags used to indicate resolution context
 *
 * @return 					J9Method obtained by resolving 'cpOrSplitIndex'
 */
J9Method *
jitResolveSpecialMethodRef(J9VMThread *vmStruct, J9ConstantPool *constantPool, UDATA cpOrSplitIndex, UDATA resolveFlags)
{
	J9Method *method = NULL;

	if (J9_IS_SPECIAL_SPLIT_TABLE_INDEX(cpOrSplitIndex)) {
		method = vmStruct->javaVM->internalVMFunctions->resolveSpecialSplitMethodRef(vmStruct, constantPool, (cpOrSplitIndex & J9_SPLIT_TABLE_INDEX_MASK), resolveFlags);
	} else {
		method = vmStruct->javaVM->internalVMFunctions->resolveSpecialMethodRef(vmStruct, constantPool, cpOrSplitIndex, resolveFlags);
	}
	return method;
}
