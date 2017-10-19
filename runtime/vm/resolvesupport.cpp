/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "j9.h"
#include "j9protos.h"
#include "j9consts.h"
#include "omrgcconsts.h"
#include "j9vmnls.h"
#include "objhelp.h"
#include "vm_internal.h"
#include "ut_j9vm.h"
#include "rommeth.h"
#include "stackwalk.h"
#include "VMHelpers.hpp"

#define MAX_STACK_SLOTS 255

static void checkForDecompile(J9VMThread *currentThread, J9ROMMethodRef *romMethodRef, UDATA jitFlags);
static bool finalFieldSetAllowed(J9VMThread *currentThread, bool isStatic, J9Method *method, J9Class *fieldClass, J9ROMFieldShape *field, UDATA jitFlags);


/**
* @brief In class files with version 53 or later, setting of final fields is only allowed from initializer methods.
* Note that this is called only after verifying that the calling class and declaring class are identical.
*
* @param currentThread the current J9VMThread
* @param isStatic TRUE for static fields, FALSE for instance
* @param method the J9Method performing the resolve, NULL for no access check
* @param fieldClass the J9Class which declares the field
* @param field J9ROMFieldShape of the final field
* @param jitFlags 0 for a runtime resolve, non-zero for JIT compilation or AOT resolves
* @return true if access is legal, false if not
*/
static bool
finalFieldSetAllowed(J9VMThread *currentThread, bool isStatic, J9Method *method, J9Class *fieldClass, J9ROMFieldShape *field, UDATA jitFlags)
{
	bool legal = true;
	/* NULL method means do not do the access check */
	if (NULL != method) {
		J9ROMClass *romClass = fieldClass->romClass;
		if (romClass->majorVersion >= 53) {
			J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
			J9UTF8 *name = J9ROMMETHOD_NAME(romMethod);
			if (('<' != J9UTF8_DATA(name)[0]) || (J9UTF8_LENGTH(name) != strlen(isStatic ? "<clinit>" : "<init>"))) {
				if (0 == jitFlags) {
					setIllegalAccessErrorFinalFieldSet(currentThread, isStatic, romClass, field, romMethod);
				}
				legal = false;
			}
		}
	}
	return legal;
}


/**
* @brief Check to see if a resolved method name matches the -XXdecomp: value from the command line
* 		 and if so, force all running methods to be decompiled
* @param currentThread the current J9VMThread
* @param romMethodRef the J9ROMMethodRef being resolved
* @param jitFlags 0 for a runtime resolve, non-zero for JIT compilation or AOT resolves
* @return void
*/
static void
checkForDecompile(J9VMThread *currentThread, J9ROMMethodRef *romMethodRef, UDATA jitFlags)
{
	J9JavaVM *vm = currentThread->javaVM;

	if (NULL != vm->decompileName) {
		J9JITConfig *jitConfig = vm->jitConfig;

		if ((0 == jitFlags) && (NULL != vm->jitConfig)) {
			J9UTF8 *name = J9ROMNAMEANDSIGNATURE_NAME(J9ROMMETHODREF_NAMEANDSIGNATURE(romMethodRef));
			char *decompileName = vm->decompileName;
			if (J9UTF8_DATA_EQUALS(name->data, name->length, decompileName, strlen(decompileName))) {
				if (NULL != jitConfig->jitHotswapOccurred) {
					acquireExclusiveVMAccess(currentThread);
					jitConfig->jitHotswapOccurred(currentThread);
					releaseExclusiveVMAccess(currentThread);
				}
			}
		}
	}
}

UDATA   
packageAccessIsLegal(J9VMThread *currentThread, J9Class *targetClass, j9object_t protectionDomain, UDATA canRunJavaCode)
{
	UDATA legal = FALSE;
	j9object_t security = J9VMJAVALANGSYSTEM_SECURITY(currentThread, J9VMCONSTANTPOOL_CLASSREF_AT(currentThread->javaVM, J9VMCONSTANTPOOL_JAVALANGSYSTEM)->value);
	if (NULL == security) {
		legal = TRUE;
	} else if (canRunJavaCode) {
		sendCheckPackageAccess(currentThread, targetClass, protectionDomain, 0, 0);
		if (J9_ARE_NO_BITS_SET(currentThread->publicFlags, J9_PUBLIC_FLAGS_POP_FRAMES_INTERRUPT) && (NULL == currentThread->currentException)) {
			legal = TRUE;
		}
	}
	return legal;
}

j9object_t   
resolveStringRef(J9VMThread *vmStruct, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags)
{
	J9UTF8 *utf8Wrapper;
	j9object_t stringRef;
	J9ROMStringRef *romStringRef;

	Trc_VM_resolveStringRef_Entry(vmStruct, cpIndex, ramCP);

	romStringRef = (J9ROMStringRef *)&ramCP->romConstantPool[cpIndex];
	utf8Wrapper = J9ROMSTRINGREF_UTF8DATA(romStringRef);
	
	Trc_VM_resolveStringRef_utf8(vmStruct, &utf8Wrapper, J9UTF8_LENGTH(utf8Wrapper), J9UTF8_DATA(utf8Wrapper));

	/* Create a new string with shared char[] data */
	stringRef = vmStruct->javaVM->memoryManagerFunctions->j9gc_allocStringWithSharedCharData(vmStruct, J9UTF8_DATA(utf8Wrapper), J9UTF8_LENGTH(utf8Wrapper), resolveFlags);
	
	/* If stringRef is NULL, the exception has already been set. */
	if (stringRef != NULL) {
		J9Class *clazz = J9_CLASS_FROM_CP(ramCP);
		J9RAMStringRef *ramStringRef = (J9RAMStringRef *)&ramCP[cpIndex];
		j9object_t *stringObjectP = &ramStringRef->stringObject;
		/* Overwriting NULL with an immortal pointer, so no exception can occur */
		J9STATIC_OBJECT_STORE(vmStruct, clazz, stringObjectP, stringRef);
	}

	Trc_VM_resolveStringRef_Exit(vmStruct, stringRef);

	return stringRef;
}


J9Class *   
findFieldSignatureClass(J9VMThread *vmStruct, J9ConstantPool *ramCP, UDATA fieldRefCpIndex)
{
	J9ROMFieldRef *romFieldRef;
	J9Class *resolvedClass;
	J9ClassLoader *classLoader;
	J9ROMNameAndSignature *nameAndSig;
	J9UTF8 *signature;

	/* Get the class.  Stop immediately if an exception occurs. */
	romFieldRef = (J9ROMFieldRef *)&ramCP->romConstantPool[fieldRefCpIndex];
	nameAndSig = J9ROMFIELDREF_NAMEANDSIGNATURE(romFieldRef);
	signature = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig);

	classLoader = J9_CLASS_FROM_CP(ramCP)->classLoader;
	if (classLoader == NULL) {
		classLoader = vmStruct->javaVM->systemClassLoader;
	}

	if ('[' == J9UTF8_DATA(signature)[0]) {
		resolvedClass = internalFindClassUTF8(vmStruct, J9UTF8_DATA(signature), J9UTF8_LENGTH(signature), classLoader, J9_FINDCLASS_FLAG_THROW_ON_FAIL);
	} else {
		Assert_VM_true('L' == J9UTF8_DATA(signature)[0]);
		/* skip fieldSignature's L and ; to have only CLASSNAME required for internalFindClassUTF8 */
		resolvedClass = internalFindClassUTF8(vmStruct, &J9UTF8_DATA(signature)[1], J9UTF8_LENGTH(signature)-2, classLoader, J9_FINDCLASS_FLAG_THROW_ON_FAIL);
	}

	return resolvedClass;
}

#if defined(J9VM_OPT_VALHALLA_MVT)
static bool
ensureClassHasDVT(J9VMThread *vmStruct, J9Class *resolvedClass, U_16 classNameLength, U_8 *classNameData)
{
	PORT_ACCESS_FROM_VMC(vmStruct);
	bool verified = false;
	const char *nlsMsgFormat = NULL;

	if (J9_ARE_NO_BITS_SET(resolvedClass->romClass->extraModifiers, J9AccClassIsValueCapable)) {
		/* Error - Resolved class is not value capable */
		nlsMsgFormat = j9nls_lookup_message(
				J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
				J9NLS_VM_CLASS_NOT_VALUE_CAPABLE,
				"The resolved class %2$.*1$s is not value capable");
	} else if (NULL == resolvedClass->derivedValueType) {
		/* Error - DVT is not set */
		nlsMsgFormat = j9nls_lookup_message(
				J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
				J9NLS_VM_DVT_NOT_SET,
				"The value capable class %2$.*1$s does not have a derived value type");
	} else {
		/* Success - The DVT is available */
		verified = true;
	}

	if (!verified) {
		/* Construct error message */
		char *msgChars = NULL;
		UDATA msgCharLength = strlen(nlsMsgFormat);
		msgCharLength += classNameLength;
		msgChars = (char *)j9mem_allocate_memory(msgCharLength + 1, J9MEM_CATEGORY_VM);
		if (NULL != msgChars) {
			j9str_printf(PORTLIB, msgChars, msgCharLength, nlsMsgFormat, classNameLength, classNameData);
		}

		/* Throw InternalError */
		setCurrentExceptionUTF(vmStruct, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, msgChars);

		j9mem_free_memory(msgChars);
	}

	return verified;
}
#endif /* defined(J9VM_OPT_VALHALLA_MVT) */

J9Class *   
resolveClassRef(J9VMThread *vmStruct, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags)
{
	J9JavaVM *vm = vmStruct->javaVM;
	J9ClassLoader *bootstrapClassLoader = vm->systemClassLoader;
	J9Class *currentClass = NULL;
	J9RAMClassRef *ramClassRefWrapper = NULL;
	J9Class *resolvedClass = NULL;
	J9Class *accessClass = NULL;
	J9UTF8 *classNameWrapper = NULL;
	U_16 classNameLength = 0;
	U_8 *classNameData = NULL;
	J9ROMStringRef *romStringRef = NULL;
	J9ClassLoader *classLoader = NULL;
	UDATA jitCompileTimeResolve = J9_ARE_ANY_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_JIT_COMPILE_TIME | J9_RESOLVE_FLAG_AOT_LOAD_TIME);
	UDATA canRunJavaCode = !jitCompileTimeResolve && J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_REDEFINE_CLASS);
	UDATA findClassFlags = 0;
	UDATA accessModifiers = 0;
	j9object_t detailString = NULL;
#if defined(J9VM_OPT_VALHALLA_MVT)
	bool resolvingDVT = false;
#endif /* defined(J9VM_OPT_VALHALLA_MVT) */

	Trc_VM_resolveClassRef_Entry(vmStruct, ramCP, cpIndex, resolveFlags);

tryAgain:
	ramClassRefWrapper = (J9RAMClassRef *)&ramCP[cpIndex];
	resolvedClass = ramClassRefWrapper->value;
	/* If resolving for "new", check if the class is instantiable */
	if ((NULL != resolvedClass) && (J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_INSTANTIABLE) || J9_ARE_NO_BITS_SET(resolvedClass->romClass->modifiers, J9AccAbstract | J9AccInterface))) {
		/* ensure that the caller can safely read the modifiers field if it so desires */
		issueReadBarrier();
		goto done;
	}

	romStringRef = (J9ROMStringRef *)&ramCP->romConstantPool[cpIndex];
	classNameWrapper = J9ROMSTRINGREF_UTF8DATA(romStringRef);
	classNameLength = J9UTF8_LENGTH(classNameWrapper);
	classNameData = J9UTF8_DATA(classNameWrapper);

	currentClass = J9_CLASS_FROM_CP(ramCP);
	classLoader = currentClass->classLoader;
	if (NULL == classLoader) {
		classLoader = bootstrapClassLoader;
	}

	Trc_VM_resolveClassRef_lookup(vmStruct, classNameLength, classNameData);

#if defined(J9VM_OPT_VALHALLA_MVT)
	/* If the class name starts with ;Q, resolve to the derived value type (DVT).
	 * We get to the DVT by loading the value capable class (VCC) it was derived from,
	 * and then read the DVT from the VCC's derivedValueType field.
	 */
	if ((';' == classNameData[0]) && ('Q' == classNameData[1])) {
		/* To load the VCC, we extract the actual class name (remove ";Q" prefix and "$value" suffix). */
		classNameLength -= (strlen(";Q") + strlen("$value"));
		classNameData += strlen(";Q");
		resolvingDVT = true;
	}
#endif /* defined(J9VM_OPT_VALHALLA_MVT) */

	findClassFlags = 0;
	if (canRunJavaCode) {
		if (J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_NO_THROW_ON_FAIL)) {
			findClassFlags = J9_FINDCLASS_FLAG_THROW_ON_FAIL;
		}
	} else {
		findClassFlags = J9_FINDCLASS_FLAG_EXISTING_ONLY;
	}

	if (ramClassRefWrapper->modifiers == (UDATA)-1) {
		if ((findClassFlags & J9_FINDCLASS_FLAG_THROW_ON_FAIL) == J9_FINDCLASS_FLAG_THROW_ON_FAIL) {
			detailString = vm->memoryManagerFunctions->j9gc_createJavaLangString(vmStruct, classNameData, classNameLength, 0);
			if (NULL == vmStruct->currentException) {
				setCurrentException(vmStruct, J9VMCONSTANTPOOL_JAVALANGNOCLASSDEFFOUNDERROR, (UDATA *)detailString);
			}
		}
		goto done;
	}
	
	resolvedClass = internalFindClassUTF8(vmStruct, classNameData, classNameLength,
			classLoader, findClassFlags);

	/* Check for frame pop before permanently invalidating the CP entry */
	if (J9_ARE_ANY_BITS_SET(vmStruct->publicFlags, J9_PUBLIC_FLAGS_POP_FRAMES_INTERRUPT)) {
		goto bail;
	}

	if (NULL == resolvedClass) {
		j9object_t exception = vmStruct->currentException;
		if (J9_ARE_ANY_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_AOT_LOAD_TIME)) {
			/* Perform a name comparison - if the targetted class has the same name as the current
			 * class (from the CP), then return the current class.
			 */
			J9Class *currentClass = J9_CLASS_FROM_CP(ramCP);
			J9UTF8 *currentClassName = J9ROMCLASS_CLASSNAME(currentClass->romClass);
			if (0 == compareUTF8Length(classNameData, classNameLength,
					J9UTF8_DATA(currentClassName), J9UTF8_LENGTH(currentClassName)))
			{
				resolvedClass = currentClass;
				accessModifiers = resolvedClass->romClass->modifiers;
				goto updateCP;
			}
			/* J9_RESOLVE_FLAG_AOT_LOAD_TIME should never set modifiers = -1 */
			goto done;
		}
		/* Class not found - if NoClassDefFoundError was thrown, mark this ref as permanently unresolveable */
		if (NULL != exception) {
			if (instanceOfOrCheckCast(J9OBJECT_CLAZZ(vmStruct, exception), J9VMJAVALANGLINKAGEERROR_OR_NULL(vm))) {
				ramClassRefWrapper->modifiers = -1;
			}
		}
		goto done;
	}

#if defined(J9VM_OPT_VALHALLA_MVT)
	if (resolvingDVT) {
		if (!ensureClassHasDVT(vmStruct, resolvedClass, classNameLength, classNameData)) {
			/* The class doesn't have a DVT - exception has been set */
			goto done;
		}
		/* Read the DVT from the VCC's derivedValueType field */
		resolvedClass = resolvedClass->derivedValueType;
	}
#endif /* defined(J9VM_OPT_VALHALLA_MVT) */

	/* Perform a package access check from the current class to the resolved class.
	 * No check is required if any of the following is true:
	 * 		- the current class and resolved class are identical
	 * 		- the current class was loaded by the bootstrap class loader
	 */
	if ((currentClass != resolvedClass) && (classLoader != bootstrapClassLoader)) {
		/* AOT resolves class refs inside J9Classes which have not yet
		 * had the java/lang/Class associated with them.  canRunJavaCode must be false
		 * in this case.  The protectionDomain object is only used if canRunJavaCode
		 * is true, so don't bother fetching it in the false case.
		 */
		j9object_t protectionDomain = NULL;
		if (canRunJavaCode) {
			j9object_t classObject = J9VM_J9CLASS_TO_HEAPCLASS(currentClass);
			Assert_VM_notNull(classObject);
			protectionDomain = J9VMJAVALANGCLASS_PROTECTIONDOMAIN(vmStruct, classObject);
		}
		if (!packageAccessIsLegal(vmStruct, resolvedClass, protectionDomain, canRunJavaCode)) {
			goto bail;
		}
	}

	if (jitCompileTimeResolve) {
		if (J9_ARE_NO_BITS_SET(resolvedClass->romClass->modifiers, J9_JAVA_INTERFACE)) {
			if (J9ClassInitSucceeded != resolvedClass->initializeStatus) {
				goto bail;
			}
		}
	}

	/* check access permissions */
	accessModifiers = resolvedClass->romClass->modifiers;
	if (J9ROMCLASS_IS_ARRAY(resolvedClass->romClass)) {
		accessClass = ((J9ArrayClass *)resolvedClass)->leafComponentType;
		accessModifiers = accessClass->romClass->modifiers;
	} else {
		accessClass = resolvedClass;
	}
	{
		IDATA checkResult = checkVisibility(vmStruct, J9_CLASS_FROM_CP(ramCP), accessClass, accessModifiers, resolveFlags);
		if (checkResult < J9_VISIBILITY_ALLOWED) {
			if (canRunJavaCode) {
				char *errorMsg = NULL;
				PORT_ACCESS_FROM_VMC(vmStruct);
				if (J9_VISIBILITY_NON_MODULE_ACCESS_ERROR == checkResult) {
					errorMsg = illegalAccessMessage(vmStruct, accessModifiers, J9_CLASS_FROM_CP(ramCP), accessClass, J9_VISIBILITY_NON_MODULE_ACCESS_ERROR);
				} else {
					errorMsg = illegalAccessMessage(vmStruct, -1, J9_CLASS_FROM_CP(ramCP), accessClass, checkResult);
				}
				setCurrentExceptionUTF(vmStruct, J9VMCONSTANTPOOL_JAVALANGILLEGALACCESSERROR, errorMsg);
				j9mem_free_memory(errorMsg);
			}
			goto bail;
		}
	}

	accessModifiers = resolvedClass->romClass->modifiers;
	if (J9_ARE_ANY_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_INSTANTIABLE)) {
		if (J9_ARE_ANY_BITS_SET(accessModifiers, J9AccAbstract | J9AccInterface)) {
			if (canRunJavaCode) {
				setCurrentException(vmStruct, J9_EX_CTOR_CLASS + J9VMCONSTANTPOOL_JAVALANGINSTANTIATIONERROR,
						(UDATA *)resolvedClass->classObject);
			}
			goto bail;
		}
	}
	
	if (J9_ARE_ANY_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_INIT_CLASS)) {
		UDATA initStatus = resolvedClass->initializeStatus;
		if ((J9ClassInitSucceeded != initStatus) && ((UDATA)vmStruct != initStatus)) {
			UDATA preCount = vm->hotSwapCount;

			/* No need to check for JITCompileTimeResolve, since it wouldn't have got this far
			 * if the class weren't already initialized.
			 */
			initializeClass(vmStruct, resolvedClass);
			if (J9_ARE_ANY_BITS_SET(vmStruct->publicFlags, J9_PUBLIC_FLAGS_POP_FRAMES_INTERRUPT)) {
				goto bail;
			}
			if (NULL != vmStruct->currentException) {
				goto bail;
			}
			if (preCount != vm->hotSwapCount) {
				goto tryAgain;
			}
		}
	}

	/* Do not update the constant pool in the AOT load-time resolve case. */
	if (J9_ARE_ANY_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_AOT_LOAD_TIME)) {
		goto done;
	}

updateCP:
	ramClassRefWrapper->value = resolvedClass;
	ramClassRefWrapper->modifiers = accessModifiers;

done:
	Trc_VM_resolveClassRef_Exit(vmStruct, resolvedClass);
	return resolvedClass; 
bail:
	resolvedClass = NULL;
	goto done;
}


J9Method *   
resolveStaticMethodRefInto(J9VMThread *vmStruct, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags, J9RAMStaticMethodRef *ramCPEntry)
{
	J9ROMMethodRef *romMethodRef;
	J9Class *resolvedClass;
	J9Method *method = NULL;
	UDATA lookupOptions;
	UDATA jitFlags = 0;
	J9Class *cpClass;
	J9Class *methodClass = NULL;
	BOOLEAN isResolvedClassAnInterface = FALSE;

	Trc_VM_resolveStaticMethodRef_Entry(vmStruct, ramCP, cpIndex, resolveFlags);

#if defined(J9VM_INTERP_NATIVE_SUPPORT)
	jitFlags |= J9_RESOLVE_FLAG_JIT_COMPILE_TIME;
#endif
	jitFlags |= J9_RESOLVE_FLAG_AOT_LOAD_TIME;
	jitFlags = (jitFlags != 0) && ((resolveFlags & jitFlags) != 0);

tryAgain:
	romMethodRef = (J9ROMMethodRef *)&ramCP->romConstantPool[cpIndex];

	checkForDecompile(vmStruct, romMethodRef, jitFlags);

	/* Resolve the class. */
	resolvedClass = resolveClassRef(vmStruct, ramCP, romMethodRef->classRefCPIndex, resolveFlags);
	if (resolvedClass == NULL) {
		goto done;
	}
	isResolvedClassAnInterface = (J9_JAVA_INTERFACE == (resolvedClass->romClass->modifiers & J9_JAVA_INTERFACE));

	/* Find the method. */
	lookupOptions = J9_LOOK_STATIC;
	if ((resolveFlags & J9_RESOLVE_FLAG_JCL_CONSTANT_POOL) == J9_RESOLVE_FLAG_JCL_CONSTANT_POOL) {
		cpClass = NULL;
	} else {
		cpClass = J9_CLASS_FROM_CP(ramCP);
		lookupOptions |= J9_LOOK_CLCONSTRAINTS;
	}
	if (jitFlags) {
		lookupOptions |= J9_LOOK_NO_THROW;
	}
	if (isResolvedClassAnInterface) {
		lookupOptions |= J9_LOOK_INTERFACE;
	}
	method = (J9Method *)javaLookupMethod(vmStruct, resolvedClass, J9ROMMETHODREF_NAMEANDSIGNATURE(romMethodRef), cpClass, lookupOptions);

	Trc_VM_resolveStaticMethodRef_lookupMethod(vmStruct, method);

	if (method == NULL) {
		goto done;
	}

	methodClass = J9_CLASS_FROM_METHOD(method);
	if (methodClass != resolvedClass) {
		if (isResolvedClassAnInterface) {
			method = NULL;
			if (0 == jitFlags) {
				setCurrentException(vmStruct, J9VMCONSTANTPOOL_JAVALANGINCOMPATIBLECLASSCHANGEERROR, NULL);
			}
			goto done;
		}
	}

	/* Initialize the defining class of the method. */
	
	if (jitFlags) {
		UDATA initStatus = resolvedClass->initializeStatus;
		if (initStatus != J9ClassInitSucceeded) {
			method = NULL;
			goto done;
		}
	} else {
		J9Class *methodClass = J9_CLASS_FROM_METHOD(method);
		UDATA initStatus = methodClass->initializeStatus;
		if (initStatus != J9ClassInitSucceeded && initStatus != (UDATA)vmStruct) {

			{
				UDATA preCount = vmStruct->javaVM->hotSwapCount;

				if (J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_NO_CLASS_INIT)) {
					initializeClass(vmStruct, methodClass);
				}
				if (vmStruct->currentException != NULL) {
					method = NULL;
					goto done;
				}
				if (preCount != vmStruct->javaVM->hotSwapCount) {
					goto tryAgain;
				}
			}
		}
	}
#if defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES)
	if (NULL != ramCPEntry)
#else /* defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES) */
	if ((NULL != ramCPEntry) && ((NULL == cpClass) || (J9CPTYPE_SHARED_METHOD != J9_CP_TYPE(J9ROMCLASS_CPSHAPEDESCRIPTION(cpClass->romClass), cpIndex))))
#endif /* defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES) */
	{
		ramCPEntry->method = method;
	}

done:
	Trc_VM_resolveStaticMethodRef_Exit(vmStruct, method);
	return method;
}


J9Method *   
resolveStaticMethodRef(J9VMThread *vmStruct, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags)
{
	/* Bit of magic here, the resulting method must be in vmStruct->floatTemp1 in the CLINIT case
	 * as resolveHelper expects to find it there.
	 */
	J9RAMStaticMethodRef *ramStaticMethodRef = (J9RAMStaticMethodRef *)&vmStruct->floatTemp1;
	J9Method *method;

	method = resolveStaticMethodRefInto(vmStruct, ramCP, cpIndex, resolveFlags, ramStaticMethodRef);
	
	if (method != NULL) {
		/* Check for <clinit> case. */
		if (((resolveFlags & J9_RESOLVE_FLAG_CHECK_CLINIT) == J9_RESOLVE_FLAG_CHECK_CLINIT)
			&& (J9_CLASS_FROM_METHOD(method)->initializeStatus == (UDATA)vmStruct)
		) {
			return (J9Method *) -1;
		} else 
#if !defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES)
		if (J9CPTYPE_SHARED_METHOD != J9_CP_TYPE(J9ROMCLASS_CPSHAPEDESCRIPTION(J9_CLASS_FROM_CP(ramCP)->romClass), cpIndex))
#endif /* !defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES) */
		{
			((J9RAMStaticMethodRef *)&ramCP[cpIndex])->method = ramStaticMethodRef->method;
		}
	}

	return method;
}


#if defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES)
J9Method *   
resolveStaticSplitMethodRef(J9VMThread *vmStruct, J9ConstantPool *ramCP, UDATA splitTableIndex, UDATA resolveFlags)
{
	J9RAMStaticMethodRef *ramStaticMethodRef = (J9RAMStaticMethodRef *)&vmStruct->floatTemp1;
	U_16 cpIndex = *(J9ROMCLASS_STATICSPLITMETHODREFINDEXES(ramCP->ramClass->romClass) + splitTableIndex);
	J9Method *method = ramCP->ramClass->staticSplitMethodTable[splitTableIndex];

	if (NULL == method) {
		method = resolveStaticMethodRefInto(vmStruct, ramCP, cpIndex, resolveFlags, ramStaticMethodRef);

		if (NULL != method) {
			/* Check for <clinit> case. */
			if (((resolveFlags & J9_RESOLVE_FLAG_CHECK_CLINIT) == J9_RESOLVE_FLAG_CHECK_CLINIT)
				&& (J9_CLASS_FROM_METHOD(method)->initializeStatus == (UDATA)vmStruct)
			) {
				return (J9Method *) -1;
			} else {
				ramCP->ramClass->staticSplitMethodTable[splitTableIndex] = method;
			}
		}
	}

	return method;
}
#endif /* defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES) */


void *    
resolveStaticFieldRefInto(J9VMThread *vmStruct, J9Method *method, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags, J9ROMFieldShape **resolvedField, J9RAMStaticFieldRef *ramCPEntry)
{
	void *staticAddress;
	J9ROMFieldRef *romFieldRef;
	J9Class *resolvedClass;

	Trc_VM_resolveStaticFieldRef_Entry(vmStruct, method, ramCP, cpIndex, resolveFlags, resolvedField);

tryAgain:
	staticAddress = NULL;

	/* Get the class.  Stop immediately if an exception occurs. */
	romFieldRef = (J9ROMFieldRef *)&ramCP->romConstantPool[cpIndex];
	resolvedClass = resolveClassRef(vmStruct, ramCP, romFieldRef->classRefCPIndex, resolveFlags);
	/* If resolvedClass is NULL, the exception has already been set. */
	if (resolvedClass != NULL) {
		J9JavaVM *javaVM = vmStruct->javaVM;
		J9Class *classFromCP = J9_CLASS_FROM_CP(ramCP);
		J9ROMFieldShape *field;
		J9Class *definingClass;
		UDATA jitFlags;
		UDATA fieldLookupFlags;
		J9ROMNameAndSignature *nameAndSig;
		J9UTF8 *name;
		J9UTF8 *signature;
		IDATA checkResult = J9_VISIBILITY_NON_MODULE_ACCESS_ERROR;
		IDATA badMemberModifier = 0;
		J9Class *targetClass = NULL;
		UDATA modifiers = 0;
		J9Class *localClassAndFlags = NULL;
		UDATA initStatus = 0;

		jitFlags = 0;
#if defined(J9VM_INTERP_NATIVE_SUPPORT)
		jitFlags |= J9_RESOLVE_FLAG_JIT_COMPILE_TIME;
#endif
		jitFlags |= J9_RESOLVE_FLAG_AOT_LOAD_TIME;
		jitFlags = (jitFlags != 0) && ((resolveFlags & jitFlags) != 0);

		/* ensure that the class is visible */
		checkResult = checkVisibility(vmStruct, classFromCP, resolvedClass, resolvedClass->romClass->modifiers, resolveFlags);
		if (checkResult < J9_VISIBILITY_ALLOWED) {
			targetClass = resolvedClass;
			badMemberModifier = resolvedClass->romClass->modifiers;
			goto illegalAccess;
		}

		/* Get the field address. */
		fieldLookupFlags = 0;
		if (jitFlags) {
			UDATA initStatus = resolvedClass->initializeStatus;
			if ( (J9ClassInitSucceeded != initStatus) && (J9ClassInitTenant != initStatus) ) {
				goto done;
			}
			fieldLookupFlags |= J9_RESOLVE_FLAG_NO_THROW_ON_FAIL;
		} else if (0 != (J9_RESOLVE_FLAG_NO_THROW_ON_FAIL & resolveFlags)) {
			fieldLookupFlags |= J9_RESOLVE_FLAG_NO_THROW_ON_FAIL;
		}
		nameAndSig = J9ROMFIELDREF_NAMEANDSIGNATURE(romFieldRef);
		name = J9ROMNAMEANDSIGNATURE_NAME(nameAndSig);
		signature = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig);
		staticAddress = staticFieldAddress(vmStruct, resolvedClass, J9UTF8_DATA(name), J9UTF8_LENGTH(name), J9UTF8_DATA(signature), J9UTF8_LENGTH(signature), &definingClass, (UDATA *)&field, fieldLookupFlags, classFromCP);
		/* Stop if an exception occurred. */
		if (staticAddress != NULL) {
			modifiers = field->modifiers;
			localClassAndFlags = definingClass;
			initStatus = definingClass->initializeStatus;

			if (resolvedField != NULL) {
				/* save away field for callee */
				*resolvedField = field;
			}

			if (initStatus != J9ClassInitSucceeded && initStatus != (UDATA) vmStruct) {
				/* Initialize the class if we're not being called from the JIT */
				if (jitFlags == 0) {
					{
						UDATA preCount = javaVM->hotSwapCount;

						if (J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_NO_CLASS_INIT)) {
							initializeClass(vmStruct, definingClass);
						}
						if (vmStruct->currentException != NULL) {
							staticAddress = NULL;
							goto done;
						}
						if (preCount != javaVM->hotSwapCount) {
							goto tryAgain;
						}
					}
				}
			}

			if ((resolveFlags & J9_RESOLVE_FLAG_FIELD_SETTER) != 0 && (modifiers & J9_JAVA_FINAL) != 0) {
				checkResult = checkVisibility(vmStruct, classFromCP, definingClass, J9_JAVA_PRIVATE, resolveFlags);
				if (checkResult < J9_VISIBILITY_ALLOWED) {
					targetClass = definingClass;
					badMemberModifier = J9_JAVA_PRIVATE;
illegalAccess:
					staticAddress = NULL;
					if (!jitFlags) {
						char *errorMsg = NULL;
						PORT_ACCESS_FROM_VMC(vmStruct);
						if (J9_VISIBILITY_NON_MODULE_ACCESS_ERROR == checkResult) {
							errorMsg = illegalAccessMessage(vmStruct, badMemberModifier, classFromCP, targetClass, J9_VISIBILITY_NON_MODULE_ACCESS_ERROR);
						} else {
							errorMsg = illegalAccessMessage(vmStruct, -1, classFromCP, targetClass, checkResult);
						}
						setCurrentExceptionUTF(vmStruct, J9VMCONSTANTPOOL_JAVALANGILLEGALACCESSERROR, errorMsg);
						j9mem_free_memory(errorMsg);
					}
					goto done;
				}
				if (!finalFieldSetAllowed(vmStruct, true, method, definingClass, field, jitFlags)) {
					staticAddress = NULL;
					goto done;
				}
			}
			
			/* check class constraints */
			if ((javaVM->runtimeFlags & J9_RUNTIME_VERIFY) != 0) {
				J9ClassLoader *cl1 = classFromCP->classLoader;
				J9ClassLoader *cl2 = definingClass->classLoader;

				if (cl1 == NULL) {
					cl1 = javaVM->systemClassLoader;
				}
				if (cl1 != cl2) {
					J9UTF8 *fieldSignature = J9ROMFIELDSHAPE_SIGNATURE(field);
					if (j9bcv_checkClassLoadingConstraintsForSignature(vmStruct, cl1, cl2, signature, fieldSignature) != 0)
					{
						if ((resolveFlags & (J9_RESOLVE_FLAG_NO_THROW_ON_FAIL | J9_RESOLVE_FLAG_JIT_COMPILE_TIME)) == 0) {
							setCurrentException(vmStruct, J9VMCONSTANTPOOL_JAVALANGVERIFYERROR, NULL);
						}
						staticAddress = NULL;
						goto done;
					}
				}
			}
			
			/* If this is a JIT compile-time resolve, do not allow fields declared in uninitialized classes. */
			if (jitFlags) {
				UDATA initStatus = localClassAndFlags->initializeStatus;
				if ( (J9ClassInitSucceeded != initStatus) && (J9ClassInitTenant != initStatus) ) {
					staticAddress = NULL;
					goto done;
				}
			}

			if (ramCPEntry != NULL) {
				UDATA localClassAndFlagsData = (UDATA)localClassAndFlags;
				/* Add bits to the class for base type and double wide field. */
				if ((modifiers & J9FieldFlagObject) != J9FieldFlagObject) {
					localClassAndFlagsData |= J9StaticFieldRefBaseType;
					if ((modifiers & J9FieldSizeDouble) == J9FieldSizeDouble) {
						localClassAndFlagsData |= J9StaticFieldRefDouble;
					}
				}
				/* Check if volatile and set the localClassAndFlags to have StaticFieldRefVolatile. */
				if ((modifiers & J9AccVolatile) == J9AccVolatile) {
					localClassAndFlagsData |= J9StaticFieldRefVolatile;
				}


				if (0 != (resolveFlags & J9_RESOLVE_FLAG_FIELD_SETTER)) {
					localClassAndFlagsData |= J9StaticFieldRefPutResolved;
				}
				/* Swap the class address bits and the flag bits. */
				ramCPEntry->flagsAndClass = J9FLAGSANDCLASS_FROM_CLASSANDFLAGS(localClassAndFlagsData);
				/* Set the high bit in valueOffset to ensure that a resolved static field ref is always interpreted as an unresolved instance fieldref. */
				ramCPEntry->valueOffset = ((UDATA) staticAddress - (UDATA) localClassAndFlags->ramStatics) | (UDATA) IDATA_MIN;
			}
		}
	}

done:
	Trc_VM_resolveStaticFieldRef_Exit(vmStruct, staticAddress);
	return staticAddress;
}


void *   
resolveStaticFieldRef(J9VMThread *vmStruct, J9Method *method, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags, J9ROMFieldShape **resolvedField)
{
	/* Bit of magic here, the resulting field must be in vmStruct->floatTemp1 in the CLINIT case
	 * as resolveHelper expects to find it there.
	 */
	J9RAMStaticFieldRef *ramStaticFieldRef = (J9RAMStaticFieldRef *)&vmStruct->floatTemp1;
	void *staticAddress;

	staticAddress = resolveStaticFieldRefInto(vmStruct, method, ramCP, cpIndex, resolveFlags, resolvedField, ramStaticFieldRef);
	
	if (staticAddress != NULL) {
		/* Check for <clinit> case. */
		if ((resolveFlags & J9_RESOLVE_FLAG_CHECK_CLINIT) == J9_RESOLVE_FLAG_CHECK_CLINIT) {
			J9Class *clazz = J9RAMSTATICFIELDREF_CLASS(ramStaticFieldRef);
			if (clazz->initializeStatus == (UDATA)vmStruct) {
				return (void *) -1;
			}
		}
		((J9RAMStaticFieldRef *)&ramCP[cpIndex])->valueOffset = ramStaticFieldRef->valueOffset;
		((J9RAMStaticFieldRef *)&ramCP[cpIndex])->flagsAndClass = ramStaticFieldRef->flagsAndClass;
	}

	return staticAddress;
}


IDATA   
resolveInstanceFieldRefInto(J9VMThread *vmStruct, J9Method *method, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags, J9ROMFieldShape **resolvedField, J9RAMFieldRef *ramCPEntry)
{
	IDATA fieldOffset = -1;
	J9ROMFieldRef *romFieldRef;
	J9Class *resolvedClass;

	Trc_VM_resolveInstanceFieldRef_Entry(vmStruct, method, ramCP, cpIndex, resolveFlags, resolvedField);
	
	/* Get the class.  Stop immediately if an exception occurs. */
	romFieldRef = (J9ROMFieldRef *)&ramCP->romConstantPool[cpIndex];
	/* Resolve the class. */
	resolvedClass = resolveClassRef(vmStruct, ramCP, romFieldRef->classRefCPIndex, resolveFlags);
	
	/* If clazz is NULL, the exception has already been set. */
	if (resolvedClass != NULL) {
		J9JavaVM *javaVM = vmStruct->javaVM;
		J9Class *classFromCP = J9_CLASS_FROM_CP(ramCP);
		J9ROMFieldShape *field;
		J9Class *definingClass;
		UDATA jitFlags;
		UDATA findFieldFlags;
		J9ROMNameAndSignature *nameAndSig;
		J9UTF8 *name;
		J9UTF8 *signature;
		IDATA checkResult = J9_VISIBILITY_NON_MODULE_ACCESS_ERROR;
		IDATA badMemberModifier = 0;
		J9Class *targetClass = NULL;
		UDATA modifiers = 0;
		char *nlsStr = NULL;
		J9Class *currentTargetClass = NULL;
		J9Class *currentSenderClass = NULL;

		jitFlags = 0;
		jitFlags |= J9_RESOLVE_FLAG_NO_THROW_ON_FAIL;
#if defined(J9VM_INTERP_NATIVE_SUPPORT)
		jitFlags |= J9_RESOLVE_FLAG_JIT_COMPILE_TIME;
#endif
		jitFlags |= J9_RESOLVE_FLAG_AOT_LOAD_TIME;
		jitFlags = (jitFlags != 0) && ((resolveFlags & jitFlags) != 0);

		/* ensure that the class is visible */
		checkResult = checkVisibility(vmStruct, classFromCP, resolvedClass, resolvedClass->romClass->modifiers, resolveFlags);
		if (checkResult < J9_VISIBILITY_ALLOWED) {
			badMemberModifier = resolvedClass->romClass->modifiers;
			targetClass = resolvedClass;
			goto illegalAccess;
		}

		/* Get the field address. */
		findFieldFlags = 0;
		if (jitFlags) {
			findFieldFlags |= J9_RESOLVE_FLAG_NO_THROW_ON_FAIL;
		}
		
		nameAndSig = J9ROMFIELDREF_NAMEANDSIGNATURE(romFieldRef);
		name = J9ROMNAMEANDSIGNATURE_NAME(nameAndSig);
		signature = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig);
		fieldOffset = instanceFieldOffsetWithSourceClass(vmStruct, resolvedClass, J9UTF8_DATA(name), J9UTF8_LENGTH(name), J9UTF8_DATA(signature), J9UTF8_LENGTH(signature), &definingClass, (UDATA *)&field, findFieldFlags, classFromCP);
		
		/* Stop if an exception occurred. */
		if (fieldOffset != -1) {
			currentTargetClass = J9_CURRENT_CLASS(resolvedClass);
			currentSenderClass = J9_CURRENT_CLASS(classFromCP);
			modifiers = field->modifiers;

			/* Add the field checking to ensure IllegalAccessError
			 * gets thrown out in the case of protected field if in different packages
			 */
			if (J9_ARE_ALL_BITS_SET(modifiers, J9AccProtected)
			&& J9_ARE_NO_BITS_SET(modifiers, J9AccStatic)
			&& !J9ROMCLASS_IS_ARRAY(currentTargetClass->romClass)
			&& isSameOrSuperClassOf(definingClass, currentSenderClass)
			&& (definingClass->packageID != currentSenderClass->packageID)
			&& !isSameOrSuperClassOf(currentSenderClass, currentTargetClass)
			&& !isSameOrSuperClassOf(currentTargetClass, currentSenderClass)
			) {
				badMemberModifier = modifiers;
				targetClass = currentTargetClass;
				goto illegalAccess;
			}

			if ((resolveFlags & J9_RESOLVE_FLAG_FIELD_SETTER) != 0 && (modifiers & J9_JAVA_FINAL) != 0) {
				checkResult = checkVisibility(vmStruct, classFromCP, definingClass, J9_JAVA_PRIVATE, resolveFlags);
				if (checkResult < J9_VISIBILITY_ALLOWED) {
					badMemberModifier = J9_JAVA_PRIVATE;
					targetClass = definingClass;
illegalAccess:
					fieldOffset = -1;
					if (!jitFlags) {
						PORT_ACCESS_FROM_VMC(vmStruct);
						if (J9_VISIBILITY_NON_MODULE_ACCESS_ERROR == checkResult) {
							nlsStr = illegalAccessMessage(vmStruct, badMemberModifier, classFromCP, targetClass, J9_VISIBILITY_NON_MODULE_ACCESS_ERROR);
						} else {
							nlsStr = illegalAccessMessage(vmStruct, -1, classFromCP, targetClass, checkResult);
						}
						setCurrentExceptionUTF(vmStruct, J9VMCONSTANTPOOL_JAVALANGILLEGALACCESSERROR, nlsStr);
						j9mem_free_memory(nlsStr);
					}
					goto done;
				}
				if (!finalFieldSetAllowed(vmStruct, false, method, definingClass, field, jitFlags)) {
					fieldOffset = -1;
					goto done;
				}
			}

			/* check class constraints */
			if ((javaVM->runtimeFlags & J9_RUNTIME_VERIFY) != 0) {
				J9ClassLoader *cl1 = classFromCP->classLoader;
				J9ClassLoader *cl2 = definingClass->classLoader;

				if (cl1 == NULL) {
					cl1 = javaVM->systemClassLoader;
				}
				if (cl1 != cl2) {
					J9UTF8 *fieldSignature = J9ROMFIELDSHAPE_SIGNATURE(field);
					if (j9bcv_checkClassLoadingConstraintsForSignature(vmStruct, cl1, cl2, signature, fieldSignature) != 0)
					{
						if ((resolveFlags & (J9_RESOLVE_FLAG_NO_THROW_ON_FAIL | J9_RESOLVE_FLAG_JIT_COMPILE_TIME)) == 0) {
							setCurrentException(vmStruct, J9VMCONSTANTPOOL_JAVALANGVERIFYERROR, NULL);
						}
						fieldOffset = -1;
						goto done;
					}
				}
			}
		
			if (ramCPEntry != NULL) {
				UDATA valueOffset = fieldOffset;

				/* Sign extend the resolved constant to make sure that it is always larger than valueOffset field */
				modifiers |= (UDATA)(IDATA)(I_32) J9FieldFlagResolved;
				if (0 != (resolveFlags & J9_RESOLVE_FLAG_FIELD_SETTER)) {
					modifiers |= J9FieldFlagPutResolved;
				}
				ramCPEntry->valueOffset = valueOffset;
				ramCPEntry->flags = modifiers;
			}

			if (resolvedField != NULL) {
				*resolvedField = field;
			}
		}
	}

done:
	Trc_VM_resolveInstanceFieldRef_Exit(vmStruct, fieldOffset);
	return fieldOffset;
}


IDATA   
resolveInstanceFieldRef(J9VMThread *vmStruct, J9Method *method, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags, J9ROMFieldShape **resolvedField)
{
	J9RAMFieldRef *ramFieldRef = (J9RAMFieldRef *)&ramCP[cpIndex];
	return resolveInstanceFieldRefInto(vmStruct, method, ramCP, cpIndex, resolveFlags, resolvedField, ramFieldRef);
}


J9Method *   
resolveInterfaceMethodRefInto(J9VMThread *vmStruct, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags, J9RAMInterfaceMethodRef *ramCPEntry)
{
	J9Method *returnValue = NULL;
	J9ROMMethodRef *romMethodRef;
	J9Class *interfaceClass;
	UDATA jitFlags = 0;
	J9ROMNameAndSignature *nameAndSig;
	UDATA lookupOptions;
	J9Method *method;
	J9Class *cpClass;
	J9JavaVM *vm = vmStruct->javaVM;

	Trc_VM_resolveInterfaceMethodRef_Entry(vmStruct, ramCP, cpIndex, resolveFlags);

	romMethodRef = (J9ROMMethodRef *)&ramCP->romConstantPool[cpIndex];

#if defined(J9VM_INTERP_NATIVE_SUPPORT)
	jitFlags |= J9_RESOLVE_FLAG_JIT_COMPILE_TIME;
#endif
	jitFlags |= J9_RESOLVE_FLAG_AOT_LOAD_TIME;
	jitFlags = (jitFlags != 0) && ((resolveFlags & jitFlags) != 0);

	checkForDecompile(vmStruct, romMethodRef, jitFlags);

	/* Resolve the class. */
	interfaceClass = resolveClassRef(vmStruct, ramCP, romMethodRef->classRefCPIndex, resolveFlags);
	
	/* If interfaceClass is NULL, the exception has already been set. */
	if (interfaceClass == NULL) {
		goto done;
	}

	if ((interfaceClass->romClass->modifiers & J9_JAVA_INTERFACE) != J9_JAVA_INTERFACE) {
		J9UTF8 *className = J9ROMCLASS_CLASSNAME(interfaceClass->romClass);

		if (jitFlags) {
			goto done;
		} else {
			j9object_t detailMessage;
			detailMessage = vm->memoryManagerFunctions->j9gc_createJavaLangString(vmStruct, J9UTF8_DATA(className), J9UTF8_LENGTH(className), J9_STR_XLAT);
			setCurrentException(vmStruct, J9VMCONSTANTPOOL_JAVALANGINCOMPATIBLECLASSCHANGEERROR, (UDATA *)detailMessage);
			goto done;
		}
	}
	
	nameAndSig = J9ROMFIELDREF_NAMEANDSIGNATURE(romMethodRef);
	lookupOptions = J9_LOOK_INTERFACE;
	if ((resolveFlags & J9_RESOLVE_FLAG_JCL_CONSTANT_POOL) == J9_RESOLVE_FLAG_JCL_CONSTANT_POOL) {
		cpClass = NULL;
	} else {
		cpClass = J9_CLASS_FROM_CP(ramCP);
		lookupOptions |= J9_LOOK_CLCONSTRAINTS;
	}
	if (jitFlags) {
		lookupOptions |= J9_LOOK_NO_THROW;
	}
	method = (J9Method *)javaLookupMethod(vmStruct, interfaceClass, nameAndSig, cpClass, lookupOptions);

	Trc_VM_resolveInterfaceMethodRef_lookupMethod(vmStruct, method);
	
	/* If method is NULL, the exception has already been set. */
	if (method != NULL) {
		if (ramCPEntry != NULL) {
			J9RAMInterfaceMethodRef *ramInterfaceMethodRef = (J9RAMInterfaceMethodRef *)&ramCP[cpIndex];
			UDATA methodIndex = getITableIndexForMethod(method) << 8;
			UDATA oldArgCount = ramInterfaceMethodRef->methodIndexAndArgCount & 255;
			J9Class *methodClass;
			methodIndex |= oldArgCount;
			methodClass = J9_CLASS_FROM_METHOD(method);
			ramCPEntry->methodIndexAndArgCount = methodIndex;
			/* interfaceClass is used to indicate resolved. Make sure to write it last */
			issueWriteBarrier();
			ramCPEntry->interfaceClass = (UDATA)methodClass;
		}
		/* indicate success */
		returnValue = method;
	}

done:
	Trc_VM_resolveInterfaceMethodRef_Exit(vmStruct, returnValue);
	return returnValue;
}


J9Method *   
resolveInterfaceMethodRef(J9VMThread *vmStruct, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags)
{
	J9RAMInterfaceMethodRef *ramInterfaceMethodRef = (J9RAMInterfaceMethodRef *)&ramCP[cpIndex];
	return resolveInterfaceMethodRefInto(vmStruct, ramCP, cpIndex, resolveFlags, ramInterfaceMethodRef);
}


J9Method *   
resolveSpecialMethodRefInto(J9VMThread *vmStruct, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags, J9RAMSpecialMethodRef *ramCPEntry)
{
	J9ROMMethodRef *romMethodRef;
	J9Class *resolvedClass;
	J9Class *currentClass;
	J9ROMNameAndSignature *nameAndSig;
	UDATA jitFlags = 0;
	UDATA lookupOptions;
	J9Method *method = NULL;
	
	Trc_VM_resolveSpecialMethodRef_Entry(vmStruct, ramCP, cpIndex, resolveFlags);

#if defined(J9VM_INTERP_NATIVE_SUPPORT)
	jitFlags |= J9_RESOLVE_FLAG_JIT_COMPILE_TIME;
#endif
	jitFlags |= J9_RESOLVE_FLAG_AOT_LOAD_TIME;
	jitFlags = (jitFlags != 0) && ((resolveFlags & jitFlags) != 0);

	romMethodRef = (J9ROMMethodRef *)&ramCP->romConstantPool[cpIndex];

	checkForDecompile(vmStruct, romMethodRef, jitFlags);

	/* Resolve the class. */
	resolvedClass = resolveClassRef(vmStruct, ramCP, romMethodRef->classRefCPIndex, resolveFlags);
	
	/* If resolvedClass is NULL, the exception has already been set. */
	if (resolvedClass == NULL) {
		goto done;
	}

	jitFlags = 0;
#if defined(J9VM_INTERP_NATIVE_SUPPORT)
	jitFlags |= J9_RESOLVE_FLAG_JIT_COMPILE_TIME;
#endif
	jitFlags |= J9_RESOLVE_FLAG_AOT_LOAD_TIME;
	jitFlags = (jitFlags != 0) && ((resolveFlags & jitFlags) != 0);

	/* Find the targetted method. */
	currentClass = J9_CLASS_FROM_CP(ramCP);
	/* Obtain the most recent class version. ramCP->class might be pointing at an old
	 * class version after a possible HCR in the resolveClassRef above. Reaching for vtable slots in
	 * the old version will cause bogosity as they are never initialized by fastHCR */
	currentClass = J9_CURRENT_CLASS(currentClass);
	nameAndSig = J9ROMFIELDREF_NAMEANDSIGNATURE(romMethodRef);

	/* REASON FOR THE J9_LOOK_HANDLE_DEFAULT_METHOD_CONFLICTS:
	 * Virtual invocation modes may still find a method in the receiver's vtable that resolves the default method conflict.
	 * If not, the method in the vtable will be a special method for throwing the exception.
	 *
	 * Special invocations (defender supersends) will not look at the receiver's vtable, but instead invoke the result of javaLookupMethod.
	 * Default method conflicts must therefore be handled by the lookup code.
	 */
	lookupOptions = J9_LOOK_VIRTUAL | J9_LOOK_ALLOW_FWD | J9_LOOK_HANDLE_DEFAULT_METHOD_CONFLICTS;

	if ((resolveFlags & J9_RESOLVE_FLAG_JCL_CONSTANT_POOL) != J9_RESOLVE_FLAG_JCL_CONSTANT_POOL) {
		lookupOptions |= J9_LOOK_CLCONSTRAINTS;
	}
	if (jitFlags) {
		lookupOptions |= J9_LOOK_NO_THROW;
	}
	method = (J9Method *)javaLookupMethod(vmStruct, resolvedClass, nameAndSig, currentClass, lookupOptions);
	
	Trc_VM_resolveSpecialMethodRef_lookupMethod(vmStruct, method);
	
	if (method == NULL) {
		goto done;
	} else {
		/* JVMS 4.9.2: If resolvedClass is an interface, ensure that it is a DIRECT superinterface
		 * of currentClass (or resolvedClass == currentClass).
		 */
		if (!isDirectSuperInterface(vmStruct, resolvedClass, currentClass)) {
			if (0 == jitFlags) {
				setIncompatibleClassChangeErrorInvalidDefenderSupersend(vmStruct, resolvedClass, currentClass);
			}
			method = NULL;
			goto done;
		}
	}
	
	method = getMethodForSpecialSend(vmStruct, currentClass, resolvedClass, method);

#if defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES)
	if (NULL != ramCPEntry)
#else /* defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES) */
	if ((NULL != ramCPEntry) && (J9CPTYPE_SHARED_METHOD != J9_CP_TYPE(J9ROMCLASS_CPSHAPEDESCRIPTION(currentClass->romClass), cpIndex)))
#endif /* defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES) */
	{
		ramCPEntry->method = method;
	}

done:
	Trc_VM_resolveSpecialMethodRef_Exit(vmStruct, method);
	return method;
}


J9Method *   
resolveSpecialMethodRef(J9VMThread *vmStruct, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags)
{
	J9RAMSpecialMethodRef *ramSpecialMethodRef = (J9RAMSpecialMethodRef *)&ramCP[cpIndex];
	return resolveSpecialMethodRefInto(vmStruct, ramCP, cpIndex, resolveFlags, ramSpecialMethodRef);
}


#if defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES)
J9Method *   
resolveSpecialSplitMethodRef(J9VMThread *vmStruct, J9ConstantPool *ramCP, UDATA splitTableIndex, UDATA resolveFlags)
{
	U_16 cpIndex = *(U_16 *)(J9ROMCLASS_SPECIALSPLITMETHODREFINDEXES(ramCP->ramClass->romClass) + splitTableIndex);
	J9Method *method = ramCP->ramClass->specialSplitMethodTable[splitTableIndex];
	
	if (NULL == method) {
		method = resolveSpecialMethodRefInto(vmStruct, ramCP, cpIndex, resolveFlags, NULL);

		if (NULL != method) {
			ramCP->ramClass->specialSplitMethodTable[splitTableIndex] = method;
		}
	}
	return method;
}
#endif /* defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES) */


UDATA   
resolveVirtualMethodRefInto(J9VMThread *vmStruct, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags, J9Method **resolvedMethod, J9RAMVirtualMethodRef *ramCPEntry)
{
	UDATA vTableIndex = 0;
	J9ROMMethodRef *romMethodRef;
	J9Class *resolvedClass;
	J9JavaVM *vm = vmStruct->javaVM;
	UDATA jitFlags = 0;

	Trc_VM_resolveVirtualMethodRef_Entry(vmStruct, ramCP, cpIndex, resolveFlags, resolvedMethod);

#if defined(J9VM_INTERP_NATIVE_SUPPORT)
	jitFlags |= J9_RESOLVE_FLAG_JIT_COMPILE_TIME;
#endif
	jitFlags |= J9_RESOLVE_FLAG_AOT_LOAD_TIME;
	jitFlags = (jitFlags != 0) && ((resolveFlags & jitFlags) != 0);

	romMethodRef = (J9ROMMethodRef *)&ramCP->romConstantPool[cpIndex];

	checkForDecompile(vmStruct, romMethodRef, jitFlags);

	/* Resolve the class. */
	resolvedClass = resolveClassRef(vmStruct, ramCP, romMethodRef->classRefCPIndex, resolveFlags);

	/* If resolvedClass is NULL, the exception has already been set. */
	if (resolvedClass != NULL) {
		UDATA lookupOptions;
		J9ROMNameAndSignature *nameAndSig = J9ROMFIELDREF_NAMEANDSIGNATURE(romMethodRef);
		U_32 *cpShapeDescription = NULL;
		J9Method *method;
		J9Class *cpClass;

		/* Stack allocate a byte array for VarHandle method name and signature. The array size is:
		 *  - J9ROMNameAndSignature
		 *  - Modified method name
		 *      - U_16 for J9UTF8 length
		 *      - 26 bytes for the original method name ("compareAndExchangeVolatile" is the longest)
		 *      - 5 bytes for "_impl".
		 *  - J9UTF8 for empty signature
		 */
		U_8 nameAndNAS[sizeof(J9ROMNameAndSignature) + (sizeof(U_16) + 26 + 5) + sizeof(J9UTF8)];

		lookupOptions = J9_LOOK_VIRTUAL;
		if ((resolveFlags & J9_RESOLVE_FLAG_JCL_CONSTANT_POOL) == J9_RESOLVE_FLAG_JCL_CONSTANT_POOL) {
			cpClass = NULL;
		} else {
			cpClass = J9_CLASS_FROM_CP(ramCP);
			cpShapeDescription = J9ROMCLASS_CPSHAPEDESCRIPTION(cpClass->romClass);
			lookupOptions |= J9_LOOK_CLCONSTRAINTS;
		}
		if (jitFlags) {
			lookupOptions |= J9_LOOK_NO_THROW;
		}
		
#if defined(J9VM_OPT_METHOD_HANDLE)
		/*
		 * Check for MH.invoke and MH.invokeExact.
		 *
		 * Methodrefs corresponding to those methods already have their methodIndex set to index into
		 * cpClass->methodTypes. We resolve them by calling into MethodType.fromMethodDescriptorString()
		 * and storing the result into the cpClass->methodTypes table.
		 */
		if ((NULL != cpShapeDescription)
		&& (resolvedClass == J9VMJAVALANGINVOKEMETHODHANDLE(vm))
		&& (J9CPTYPE_INSTANCE_METHOD != J9_CP_TYPE(cpShapeDescription, cpIndex))
		) {
			J9UTF8 *nameUTF = J9ROMNAMEANDSIGNATURE_NAME(nameAndSig);
			J9UTF8 *sigUTF = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig);
			if ((J9CPTYPE_HANDLE_METHOD == J9_CP_TYPE(cpShapeDescription, cpIndex))
			|| J9UTF8_LITERAL_EQUALS(J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF), "invokeExact")
			|| J9UTF8_LITERAL_EQUALS(J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF), "invoke")
			) {
				J9RAMMethodRef *ramMethodRef = (J9RAMMethodRef *)&ramCP[cpIndex];
				UDATA methodTypeIndex = ramMethodRef->methodIndexAndArgCount >> 8;
				j9object_t methodType = NULL;

				/* Return NULL if called by the JIT compilation thread as that thread
				 * is not allowed to make call-ins into Java.  The only way to resolve
				 * a MethodType object is to call-in using MethodType.fromMethodDescriptorString()
				 * which runs Java code.
				 */
				if (jitFlags) {
					goto done;
				}

				/* Throw LinkageError if more than 255 stack slots are
				 * required by the MethodType and the MethodHandle.
				 */
				if (MAX_STACK_SLOTS == (ramMethodRef->methodIndexAndArgCount & MAX_STACK_SLOTS)) {
					setCurrentExceptionNLS(vmStruct, J9VMCONSTANTPOOL_JAVALANGLINKAGEERROR, J9NLS_VM_TOO_MANY_ARGUMENTS);
					goto done;
				}

				/* Call VM Entry point to create the MethodType - Result is put into the
				 * vmThread->returnValue as entry points don't "return" in the expected way
				 */
				sendFromMethodDescriptorString(vmStruct, sigUTF, J9_CLASS_FROM_CP(ramCP)->classLoader, NULL, 0);
				methodType = (j9object_t) vmStruct->returnValue;

				/* check if an exception is already pending */
				if (NULL != vmStruct->currentException) {
					/* Already a pending exception */
					methodType = NULL;
				} else if (NULL == methodType) {
					/* Resolved MethodType was null - throw NPE that includes the lookupSignature from the NaS */
					j9object_t lookupSigString = vm->memoryManagerFunctions->j9gc_createJavaLangString(vmStruct, J9UTF8_DATA(sigUTF), J9UTF8_LENGTH(sigUTF), 0);
					if (NULL == vmStruct->currentException) {
						setCurrentException(vmStruct, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, (UDATA*)lookupSigString);
					}
				}

				/* Only write the value in if its not null */
				if (NULL != methodType) {
					J9Class *clazz = J9_CLASS_FROM_CP(ramCP);
					j9object_t *methodTypeObjectP = clazz->methodTypes + methodTypeIndex;
					/* Overwriting NULL with an immortal pointer, so no exception can occur */
					J9STATIC_OBJECT_STORE(vmStruct, clazz, methodTypeObjectP, methodType);

					/* Record vTableIndex for the exit tracepoint. */
					vTableIndex = methodTypeIndex;
				}

				goto done;
			}
		} else if (resolvedClass == J9VMJAVALANGINVOKEVARHANDLE_OR_NULL(vm)) {
			J9UTF8 *nameUTF = J9ROMNAMEANDSIGNATURE_NAME(nameAndSig);
			J9UTF8 *sigUTF = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig);
			U_8* initialMethodName = J9UTF8_DATA(nameUTF);
			U_16 initialMethodNameLength = J9UTF8_LENGTH(nameUTF);
			BOOLEAN isVarHandle = FALSE;

			switch (initialMethodNameLength) {
			case 3:
				if (J9UTF8_LITERAL_EQUALS(initialMethodName, initialMethodNameLength, "get")
				 || J9UTF8_LITERAL_EQUALS(initialMethodName, initialMethodNameLength, "set")
				) {
					isVarHandle = TRUE;
				}
				break;
			case 9:
				if (J9UTF8_LITERAL_EQUALS(initialMethodName, initialMethodNameLength, "getOpaque")
				 || J9UTF8_LITERAL_EQUALS(initialMethodName, initialMethodNameLength, "setOpaque")
				 || J9UTF8_LITERAL_EQUALS(initialMethodName, initialMethodNameLength, "getAndSet")
				 || J9UTF8_LITERAL_EQUALS(initialMethodName, initialMethodNameLength, "getAndAdd")
				) {
					isVarHandle = TRUE;
				}
				break;
			case 10:
				if (J9UTF8_LITERAL_EQUALS(initialMethodName, initialMethodNameLength, "getAcquire")
				 || J9UTF8_LITERAL_EQUALS(initialMethodName, initialMethodNameLength, "setRelease")
				) {
					isVarHandle = TRUE;
				}
				break;
			case 11:
				if (J9UTF8_LITERAL_EQUALS(initialMethodName, initialMethodNameLength, "getVolatile")
				 || J9UTF8_LITERAL_EQUALS(initialMethodName, initialMethodNameLength, "setVolatile")
				) {
					isVarHandle = TRUE;
				}
				break;
			case 16:
				if (J9UTF8_LITERAL_EQUALS(initialMethodName, initialMethodNameLength, "getAndSetAcquire")
				 || J9UTF8_LITERAL_EQUALS(initialMethodName, initialMethodNameLength, "getAndSetRelease")
				 || J9UTF8_LITERAL_EQUALS(initialMethodName, initialMethodNameLength, "getAndAddAcquire")
				 || J9UTF8_LITERAL_EQUALS(initialMethodName, initialMethodNameLength, "getAndAddRelease")
				 || J9UTF8_LITERAL_EQUALS(initialMethodName, initialMethodNameLength, "getAndBitwiseAnd")
				 || J9UTF8_LITERAL_EQUALS(initialMethodName, initialMethodNameLength, "getAndBitwiseXor")
				) {
					isVarHandle = TRUE;
				}
				break;
			case 22:
				if (J9UTF8_LITERAL_EQUALS(initialMethodName, initialMethodNameLength, "getAndBitwiseOrAcquire")
				 || J9UTF8_LITERAL_EQUALS(initialMethodName, initialMethodNameLength, "getAndBitwiseOrRelease")
				 || J9UTF8_LITERAL_EQUALS(initialMethodName, initialMethodNameLength, "weakCompareAndSetPlain")
				) {
					isVarHandle = TRUE;
				}
				break;
			case 23:
				if (J9UTF8_LITERAL_EQUALS(initialMethodName, initialMethodNameLength, "getAndBitwiseAndAcquire")
				 || J9UTF8_LITERAL_EQUALS(initialMethodName, initialMethodNameLength, "getAndBitwiseAndRelease")
				 || J9UTF8_LITERAL_EQUALS(initialMethodName, initialMethodNameLength, "getAndBitwiseXorAcquire")
				 || J9UTF8_LITERAL_EQUALS(initialMethodName, initialMethodNameLength, "getAndBitwiseXorRelease")
				) {
					isVarHandle = TRUE;
				}
				break;
			case 24:
				if (J9UTF8_LITERAL_EQUALS(initialMethodName, initialMethodNameLength, "weakCompareAndSetAcquire")
				 || J9UTF8_LITERAL_EQUALS(initialMethodName, initialMethodNameLength, "weakCompareAndSetRelease")
				) {
					isVarHandle = TRUE;
				}
				break;
			case 25:
				if (J9UTF8_LITERAL_EQUALS(initialMethodName, initialMethodNameLength, "compareAndExchangeAcquire")
				 || J9UTF8_LITERAL_EQUALS(initialMethodName, initialMethodNameLength, "compareAndExchangeRelease")
				) {
					isVarHandle = TRUE;
				}
				break;
			default:
				if (J9UTF8_LITERAL_EQUALS(initialMethodName, initialMethodNameLength, "compareAndSet")
				 || J9UTF8_LITERAL_EQUALS(initialMethodName, initialMethodNameLength, "getAndBitwiseOr")
				 || J9UTF8_LITERAL_EQUALS(initialMethodName, initialMethodNameLength, "weakCompareAndSet")
				 || J9UTF8_LITERAL_EQUALS(initialMethodName, initialMethodNameLength, "compareAndExchange")
				) {
					isVarHandle = TRUE;
				}
				break;
			}

			if (isVarHandle) {
				J9UTF8 *modifiedMethodName = (J9UTF8 *)(nameAndNAS + sizeof(J9ROMNameAndSignature));
				J9UTF8 *modifiedMethodSig = (J9UTF8 *)(nameAndNAS + sizeof(nameAndNAS) - sizeof(J9UTF8));
				memset(nameAndNAS, 0, sizeof(nameAndNAS));

				/* Create new J9ROMNameAndSignature */
				nameAndSig = (J9ROMNameAndSignature *)nameAndNAS;
				NNSRP_SET(nameAndSig->name, modifiedMethodName);
				NNSRP_SET(nameAndSig->signature, modifiedMethodSig);

				/* Change method name to include the suffix "_impl", which are the methods with VarHandle send targets. */
				modifiedMethodName->length = initialMethodNameLength + 5;
				memcpy(modifiedMethodName->data, initialMethodName, initialMethodNameLength);
				memcpy(modifiedMethodName->data + initialMethodNameLength, "_impl", 5);

				/* Set flag for partial signature lookup. Signature length is already initialized to 0. */
				lookupOptions |= J9_LOOK_PARTIAL_SIGNATURE;

				/* Redirect resolution to VarHandleInternal */
				resolvedClass = VM_VMHelpers::getSuperclass(resolvedClass);

				/* Ensure visibility passes */
				cpClass = resolvedClass;

				/* Resolve the MethodType. */
				if (!jitFlags) {
					j9object_t methodType = NULL;
					J9Class *ramClass = ramCP->ramClass;
					J9ROMClass *romClass = ramClass->romClass;
					J9RAMMethodRef *ramMethodRef = (J9RAMMethodRef *)&ramCP[cpIndex];

					/* Throw LinkageError if more than 255 stack slots are
					 * required by the MethodType and the MethodHandle.
					 */
					if (MAX_STACK_SLOTS == (ramMethodRef->methodIndexAndArgCount & MAX_STACK_SLOTS)) {
						setCurrentExceptionNLS(vmStruct, J9VMCONSTANTPOOL_JAVALANGLINKAGEERROR, J9NLS_VM_TOO_MANY_ARGUMENTS);
						goto done;
					}

					/* Call VM Entry point to create the MethodType - Result is put into the
					 * vmThread->returnValue as entry points don't "return" in the expected way
					 *
					 * NB! An extra VarHandle argument is appended to the MethodType's argument list,
					 * so the returned MethodType doesn't represent the provided method signature.
					 * E.g. the MethodType for "(I)I" will have the following descriptor string:
					 *     "(Ijava/lang/invoke/VarHandle;)I"
					 */
					sendFromMethodDescriptorString(vmStruct, sigUTF, J9_CLASS_FROM_CP(ramCP)->classLoader, J9VMJAVALANGINVOKEVARHANDLE_OR_NULL(vm), 0);
					methodType = (j9object_t)vmStruct->returnValue;

					/* Check if an exception is already pending */
					if (NULL != vmStruct->currentException) {
						/* Already a pending exception */
						methodType = NULL;
					} else if (NULL == methodType) {
						/* Resolved MethodType was null - throw NPE that includes the lookupSignature from the NaS */
						j9object_t lookupSigString = vm->memoryManagerFunctions->j9gc_createJavaLangString(vmStruct, J9UTF8_DATA(sigUTF), J9UTF8_LENGTH(sigUTF), 0);
						if (NULL == vmStruct->currentException) {
							setCurrentException(vmStruct, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, (UDATA*)lookupSigString);
						}
					}

					/* Store MethodType in cache */
					if (NULL != methodType) {
						U_32 methodTypeIndex = VM_VMHelpers::lookupVarHandleMethodTypeCacheIndex(romClass, cpIndex);
						j9object_t *methodTypeObjectP = ramClass->varHandleMethodTypes + methodTypeIndex;
						J9STATIC_OBJECT_STORE(vmStruct, ramClass, methodTypeObjectP, methodType);
					}
				}
			}
		}
#endif /* J9VM_OPT_METHOD_HANDLE */

		method = (J9Method *)javaLookupMethod(vmStruct, resolvedClass, nameAndSig, cpClass, lookupOptions);

		Trc_VM_resolveVirtualMethodRef_lookupMethod(vmStruct, method);

		/* If method is NULL, the exception has already been set. */
		if (method != NULL) {
			/* Fill in the constant pool entry. Don't bother checking for failure on the vtable index, since we know the method is there. */
			vTableIndex = getVTableIndexForMethod(method, resolvedClass, vmStruct);
			if (vTableIndex == 0) {
				if (!jitFlags && (0 == (J9_RESOLVE_FLAG_NO_THROW_ON_FAIL & resolveFlags))) {
					j9object_t errorString = methodToString(vmStruct, method);
					setCurrentException(vmStruct, J9VMCONSTANTPOOL_JAVALANGINCOMPATIBLECLASSCHANGEERROR, (UDATA *)errorString);
				}
			} else {
				if (ramCPEntry != NULL) {
					UDATA argSlotCount = vTableIndex << 8;
					J9RAMVirtualMethodRef *ramVirtualMethodRef = (J9RAMVirtualMethodRef *)&ramCP[cpIndex];
					UDATA oldArgCount = ramVirtualMethodRef->methodIndexAndArgCount & 255;
					argSlotCount |= oldArgCount;
					ramCPEntry->methodIndexAndArgCount = argSlotCount;
				}
				if (resolvedMethod != NULL) {
					/* save away method for callee */
					*resolvedMethod = method;
				}
			}
		}
	}

#if defined(J9VM_OPT_METHOD_HANDLE)
done:
#endif /* J9VM_OPT_METHOD_HANDLE */
	Trc_VM_resolveVirtualMethodRef_Exit(vmStruct, vTableIndex);
	return vTableIndex;
}

	
UDATA   
resolveVirtualMethodRef(J9VMThread *vmStruct, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags, J9Method **resolvedMethod)
{
	J9RAMVirtualMethodRef *ramVirtualMethodRef = (J9RAMVirtualMethodRef *)&ramCP[cpIndex];
	return resolveVirtualMethodRefInto(vmStruct, ramCP, cpIndex, resolveFlags, resolvedMethod, ramVirtualMethodRef);
}


j9object_t   
resolveMethodTypeRefInto(J9VMThread *vmThread, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags, J9RAMMethodTypeRef *ramCPEntry) {
	j9object_t methodType;
	UDATA jitFlags = 0;
	J9ROMMethodTypeRef *romMethodTypeRef = NULL;
	J9UTF8 *lookupSig = NULL;

	Trc_VM_sendResolveMethodTypeRefInto_Entry(vmThread, ramCP, cpIndex, resolveFlags);

	/* Check if already resolved */
	if (ramCPEntry->type != NULL) {
		return ramCPEntry->type;
	}

	/* Return NULL if called by the JIT compilation thread as that thread
	 * is not allowed to make call-ins into Java.  The only way to resolve
	 * a MethodType object is to call-in using MethodType.fromMethodDescriptorString()
	 * which runs Java code.
	 */
#if defined(J9VM_INTERP_NATIVE_SUPPORT)
	jitFlags |= J9_RESOLVE_FLAG_JIT_COMPILE_TIME;
#endif
	jitFlags |= J9_RESOLVE_FLAG_AOT_LOAD_TIME;
	if ((resolveFlags & jitFlags) != 0) {
		return NULL;
	}

	/* Call VM Entry point to create the MethodType - Result is put into the
	 * vmThread->returnValue as entry points don't "return" in the expected way
	 */
	romMethodTypeRef = ((J9ROMMethodTypeRef *) &(J9_ROM_CP_FROM_CP(ramCP)[cpIndex]));
	lookupSig = J9ROMMETHODTYPEREF_SIGNATURE(romMethodTypeRef);
	sendFromMethodDescriptorString(vmThread, lookupSig, J9_CLASS_FROM_CP(ramCP)->classLoader, NULL, 0);
	methodType = (j9object_t) vmThread->returnValue;

	/* check if an exception is already pending */
	if (vmThread->currentException != NULL) {
		/* Already a pending exception */
		methodType = NULL;
	} else if (methodType == NULL) {
		/* Resolved MethodType was null - throw NPE that includes the lookupSignature from the NaS */
		j9object_t lookupSigString = vmThread->javaVM->memoryManagerFunctions->j9gc_createJavaLangString(vmThread, J9UTF8_DATA(lookupSig), J9UTF8_LENGTH(lookupSig), 0);
		if (NULL == vmThread->currentException) {
			setCurrentException(vmThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, (UDATA*)lookupSigString);
		}
	}

	/* perform visibility checks for the returnType and all parameters */
	if (NULL != methodType) {
		/* check returnType */
		J9Class *senderClass = ramCP->ramClass;
		J9Class *returnTypeClass = J9VM_J9CLASS_FROM_HEAPCLASS(vmThread, J9VMJAVALANGINVOKEMETHODTYPE_RETURNTYPE(vmThread, methodType));
		J9Class *illegalClass = NULL;
		IDATA visibilityReturnCode = 0;

		if (J9ROMCLASS_IS_ARRAY(senderClass->romClass)) {
			senderClass = ((J9ArrayClass *)senderClass)->leafComponentType;
		}
		if (J9ROMCLASS_IS_ARRAY(returnTypeClass->romClass)) {
			returnTypeClass = ((J9ArrayClass *)returnTypeClass)->leafComponentType;
		}

		visibilityReturnCode = checkVisibility(vmThread, senderClass, returnTypeClass, returnTypeClass->romClass->modifiers, resolveFlags);

		if (J9_VISIBILITY_ALLOWED != visibilityReturnCode) {
			illegalClass = returnTypeClass;
		} else {
			/* check paramTypes */
			j9object_t argTypesObject = J9VMJAVALANGINVOKEMETHODTYPE_ARGUMENTS(vmThread, methodType);
			U_32 typeCount = J9INDEXABLEOBJECT_SIZE(vmThread, argTypesObject);

			for (UDATA i = 0; i < typeCount; i++) {
				J9Class *paramClass = J9VM_J9CLASS_FROM_HEAPCLASS(vmThread, J9JAVAARRAYOFOBJECT_LOAD(vmThread, argTypesObject, i));

				if (J9ROMCLASS_IS_ARRAY(paramClass->romClass)) {
					paramClass = ((J9ArrayClass *)paramClass)->leafComponentType;
				}

				visibilityReturnCode = checkVisibility(vmThread, senderClass, paramClass, paramClass->romClass->modifiers, resolveFlags);

				if (J9_VISIBILITY_ALLOWED != visibilityReturnCode) {
					illegalClass = paramClass;
					break;
				}
			}
		}
		if (NULL != illegalClass) {
			char *errorMsg = illegalAccessMessage(vmThread, illegalClass->romClass->modifiers, senderClass, illegalClass, visibilityReturnCode);
			if (NULL == errorMsg) {
				setNativeOutOfMemoryError(vmThread, 0, 0);
			} else {
				setCurrentExceptionUTF(vmThread, J9VMCONSTANTPOOL_JAVALANGILLEGALACCESSERROR, errorMsg);
			}
			Trc_VM_sendResolveMethodTypeRefInto_Exception(vmThread, senderClass, illegalClass, visibilityReturnCode);
			methodType = NULL;
		}
	}

	/* Only write the value in if its not null */
	if (NULL != methodType ) {
		J9Class *clazz = J9_CLASS_FROM_CP(ramCP);
		j9object_t *methodTypeObjectP = &ramCPEntry->type;
		/* Overwriting NULL with an immortal pointer, so no exception can occur */
		J9STATIC_OBJECT_STORE(vmThread, clazz, methodTypeObjectP, methodType);
	}

	Trc_VM_sendResolveMethodTypeRefInto_Exit(vmThread, methodType);

	return methodType;
}

j9object_t   
resolveMethodTypeRef(J9VMThread *vmThread, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags)
{
	J9RAMMethodTypeRef *ramMethodTypeRef = (J9RAMMethodTypeRef *)&ramCP[cpIndex];
	return resolveMethodTypeRefInto(vmThread, ramCP, cpIndex, resolveFlags, ramMethodTypeRef);

}

j9object_t   
resolveMethodHandleRefInto(J9VMThread *vmThread, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags, J9RAMMethodHandleRef *ramCPEntry) {
	UDATA jitFlags = 0;
	J9ROMMethodHandleRef *romMethodHandleRef;
	U_32 fieldOrMethodIndex;
	J9ROMMethodRef *cpItem;
	J9Class *definingClass;
	J9ROMNameAndSignature* nameAndSig;
	j9object_t methodHandle = NULL;

	/* Check if already resolved */
	if (ramCPEntry->methodHandle != NULL) {
		return ramCPEntry->methodHandle;
	}

	/* Return NULL if called by the JIT compilation thread as that thread
	 * is not allowed to make call-ins into Java.
	 */
#if defined(J9VM_INTERP_NATIVE_SUPPORT)
	jitFlags |= J9_RESOLVE_FLAG_JIT_COMPILE_TIME;
#endif
	jitFlags |= J9_RESOLVE_FLAG_AOT_LOAD_TIME;
	if ((resolveFlags & jitFlags) != 0) {
		return NULL;
	}

	/* Use normal field/method resolution to validate whether we can see the method/field to
	 * get a MethodHandle on it and ensure that appropriate exceptions are thrown.
	 * MethodHandle.invoke{Exact,Generic} is a special case as resolution can't succeed.
	 */

	romMethodHandleRef = ((J9ROMMethodHandleRef *) &(J9_ROM_CP_FROM_CP(ramCP)[cpIndex]));
	fieldOrMethodIndex = romMethodHandleRef->methodOrFieldRefIndex;

	/* Shift the handleTypeAndCpType to remove the cpType */
	switch(romMethodHandleRef->handleTypeAndCpType >> J9DescriptionCpTypeShift) {
	/* Instance Fields */
	case MH_REF_GETFIELD:
	case MH_REF_PUTFIELD:
		if (resolveInstanceFieldRef(vmThread, NULL, ramCP, fieldOrMethodIndex, resolveFlags, NULL) == -1) {
			/* field resolution failed - exception should be set */
			goto _done;
		}
		break;
	/* Static Fields */
	case MH_REF_GETSTATIC:
	case MH_REF_PUTSTATIC:
		if (resolveStaticFieldRef(vmThread, NULL, ramCP, fieldOrMethodIndex, resolveFlags, NULL) == NULL) {
			/* field resolution failed - exception should be set */
			goto _done;
		}
		break;

	case MH_REF_INVOKEVIRTUAL:
		/* InvokeVirtual */
	{
		/*
		 * Given that MethodHandle invokeExact() & invoke() are @PolymorphicSignature methods,
		 * skip method/signature checking to enable MethodHandle invocation only when the resolved class is MethodHandle
		 * and the corresponding method is invokeExact() or invoke().
		 */
		J9ROMMethodRef *romMethodRef = (J9ROMMethodRef *) &((J9_ROM_CP_FROM_CP(ramCP))[fieldOrMethodIndex]);
		J9Class *resolvedClass = NULL;

		resolvedClass = resolveClassRef(vmThread, ramCP, romMethodRef->classRefCPIndex, resolveFlags);

		/* Assumes that if this is MethodHandle, the class was successfully resolved but the method was not */
		if (resolvedClass == J9VMJAVALANGINVOKEMETHODHANDLE(vmThread->javaVM)) {
			/* This is MethodHandle - confirm name is either invokeExact or invoke */
			J9ROMNameAndSignature* nameAndSig = J9ROMMETHODREF_NAMEANDSIGNATURE(romMethodRef);
			J9UTF8 *nameUTF = J9ROMNAMEANDSIGNATURE_NAME(nameAndSig);
			if (J9UTF8_LITERAL_EQUALS(J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF), "invokeExact")
			|| J9UTF8_LITERAL_EQUALS(J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF), "invoke")
			) {
				/* valid - must be resolvable - clear exception and continue */
				break;
			}
		}

		if (resolveVirtualMethodRef(vmThread, ramCP, fieldOrMethodIndex, resolveFlags, NULL) == 0) {
			/* private methods don't end up in the vtable - we need to determine if invokeSpecial is
			 * appropriate here.
			 */
			J9RAMSpecialMethodRef ramSpecialMethodRef;

			/* Clear the exception and let the resolveSpecialMethodRef set an exception if necessary */
			vmThread->currentException = NULL;
			vmThread->privateFlags &= ~J9_PRIVATE_FLAGS_REPORT_EXCEPTION_THROW;
			
			memset(&ramSpecialMethodRef, 0, sizeof(J9RAMSpecialMethodRef));
			if (resolveSpecialMethodRefInto(vmThread, ramCP, fieldOrMethodIndex, resolveFlags, &ramSpecialMethodRef) == NULL) {
				/* Only the class and {Name, Signature} are used by the java code so it is safe to use the fake specialMethodRef */
				goto _done;
			}
		}
		break;
	}
	case MH_REF_INVOKESTATIC:
		/* InvokeStatic */
		if (resolveStaticMethodRef(vmThread, ramCP, fieldOrMethodIndex, resolveFlags) == NULL) {
			goto _done;
		}
		break;
	case MH_REF_INVOKESPECIAL:
	case MH_REF_NEWINVOKESPECIAL:
		/* InvokeSpecial */
		if (resolveSpecialMethodRef(vmThread, ramCP, fieldOrMethodIndex, resolveFlags) == NULL) {
			/* Note: this can't find any polymorphic signature versions MethodHandle.invoke{Exact, Generic} */
			goto _done;
		}
		break;
	case MH_REF_INVOKEINTERFACE:
		/* InvokeInterface */
		if (resolveInterfaceMethodRef(vmThread, ramCP, fieldOrMethodIndex, resolveFlags) == NULL) {
			goto _done;
		}
		break;
	}

	/* cpItem will be either a field or a method ref - they both have the same shape so
	 * we can pretend it is always a methodref
	 */
	cpItem = (J9ROMMethodRef *) &(J9_ROM_CP_FROM_CP(ramCP)[fieldOrMethodIndex]);
	definingClass = ((J9RAMClassRef *) &(ramCP[cpItem->classRefCPIndex]))->value;
	if (definingClass == NULL) {
		goto _done;
	}
	nameAndSig = J9ROMMETHODREF_NAMEANDSIGNATURE(cpItem);

	sendResolveMethodHandle(vmThread, cpIndex, ramCP, definingClass, nameAndSig);
	methodHandle = (j9object_t) vmThread->returnValue;

	/* check if an exception is already pending */
	if (vmThread->currentException != NULL) {
		/* Already a pending exception */
		methodHandle = NULL;
	} else if (methodHandle == NULL) {
		setCurrentExceptionUTF(vmThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	}

	/* Only write the value in if its not null */
	if (NULL != methodHandle) {
		methodHandle = vmThread->javaVM->memoryManagerFunctions->j9gc_objaccess_asConstantPoolObject(
																		vmThread, 
																		methodHandle, 
																		J9_GC_ALLOCATE_OBJECT_TENURED | J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE | J9_GC_ALLOCATE_OBJECT_HASHED);
		if (NULL == methodHandle) {
			setHeapOutOfMemoryError(vmThread);
		} else {
			j9object_t *methodHandleObjectP = &ramCPEntry->methodHandle;
			/* Overwriting NULL with an immortal pointer, so no exception can occur */
			J9STATIC_OBJECT_STORE(vmThread,  J9_CLASS_FROM_CP(ramCP), methodHandleObjectP, methodHandle);
		}
	}

_done:

	return methodHandle;
}

j9object_t   
resolveMethodHandleRef(J9VMThread *vmThread, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags)
{
	J9RAMMethodHandleRef *ramMethodHandleRef = (J9RAMMethodHandleRef *)&ramCP[cpIndex];
	return resolveMethodHandleRefInto(vmThread, ramCP, cpIndex, resolveFlags, ramMethodHandleRef);

}

j9object_t   
resolveInvokeDynamic(J9VMThread *vmThread, J9ConstantPool *ramCP, UDATA callSiteIndex, UDATA resolveFlags)
{
	j9object_t *callSite = ramCP->ramClass->callSites + callSiteIndex;
	j9object_t methodHandle = *callSite;

	J9ROMClass *romClass = ramCP->ramClass->romClass;
	J9SRP *callSiteData = (J9SRP *) J9ROMCLASS_CALLSITEDATA(romClass);
	U_16 *bsmIndices = (U_16 *) (callSiteData + romClass->callSiteCount);
	U_16 *bsmData = bsmIndices + romClass->callSiteCount;
	J9ROMNameAndSignature* nameAndSig = SRP_PTR_GET(callSiteData + callSiteIndex, J9ROMNameAndSignature*);
	U_16 bsmIndex = bsmIndices[callSiteIndex];
	U_16 i;

	/* Check if already resolved */
	if (methodHandle != NULL) {
		return methodHandle;
	}

	/* Walk bsmData */
	for (i = 0; i < bsmIndex; i++) {
		bsmData += bsmData[1] + 2;
	}

	sendResolveInvokeDynamic(vmThread, ramCP, callSiteIndex, nameAndSig, bsmData);
	methodHandle = (j9object_t) vmThread->returnValue;

	/* check if an exception is already pending */
	if (vmThread->currentException != NULL) {
		/* Already a pending exception */
		methodHandle = NULL;
	} else if (methodHandle == NULL) {
		setCurrentExceptionUTF(vmThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	}

	/* Only write the value in if its not null */
	if (NULL != methodHandle) {
		J9MemoryManagerFunctions *gcFuncs = vmThread->javaVM->memoryManagerFunctions;
		methodHandle = gcFuncs->j9gc_objaccess_asConstantPoolObject(
									vmThread,
									methodHandle,
									J9_GC_ALLOCATE_OBJECT_TENURED | J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE | J9_GC_ALLOCATE_OBJECT_HASHED);

		if (NULL == methodHandle) {
			setHeapOutOfMemoryError(vmThread);
		} else {
			J9Class *j9class = J9_CLASS_FROM_CP(ramCP);
			if (0 == gcFuncs->j9gc_objaccess_staticCompareAndSwapObject(vmThread, j9class, callSite, NULL, methodHandle)) {
				/* Another thread beat this thread to updating the call site, ensure both threads return the same method handle. */
				methodHandle = *callSite;
			}
		}
	}

	return methodHandle;
}
