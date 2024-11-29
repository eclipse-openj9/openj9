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

#include "j2sever.h"
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
#include "j9modifiers_api.h"
#include "VMHelpers.hpp"
#include "vm_api.h"

#define MAX_STACK_SLOTS 255

static void checkForDecompile(J9VMThread *currentThread, J9ROMMethodRef *romMethodRef, bool jitCompileTimeResolve);
static bool finalFieldSetAllowed(J9VMThread *currentThread, bool isStatic, J9Method *method, J9Class *fieldClass, J9Class *callerClass, J9ROMFieldShape *field, bool canRunJavaCode);


/**
 * @brief See if the thread has an exception or immediate async pending
 * @param currentThread the current J9VMThread
 * @return true if something is pending, false if the thread may proceed
 */
static bool
threadEventsPending(J9VMThread *currentThread)
{
	return VM_VMHelpers::immediateAsyncPending(currentThread) || VM_VMHelpers::exceptionPending(currentThread);
}

/**
* @brief In class files with version 53 or later, setting of final fields is only allowed from initializer methods.
* Note that this is called only after verifying that the calling class and declaring class are identical (or that the
* caller class is exempt from verification).
*
* @param currentThread the current J9VMThread
* @param isStatic true for static fields, false for instance
* @param method the J9Method performing the resolve, NULL for no access check
* @param fieldClass the J9Class which declares the field
* @param callerClass the J9Class which tries to access the field
* @param field J9ROMFieldShape of the final field
* @param canRunJavaCode true if java code can be run, false if not
* @return true if access is legal, false if not
*/
static bool
finalFieldSetAllowed(J9VMThread *currentThread, bool isStatic, J9Method *method, J9Class *fieldClass, J9Class *callerClass, J9ROMFieldShape *field, bool canRunJavaCode)
{
	bool legal = true;
	/* NULL method means do not do the access check */
	if (NULL != method) {
		if (VM_VMHelpers::ramClassChecksFinalStores(callerClass)) {
			J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
			if (!J9ROMMETHOD_ALLOW_FINAL_FIELD_WRITES(romMethod, isStatic ? J9AccStatic : 0)) {
				if (canRunJavaCode) {
					setIllegalAccessErrorFinalFieldSet(currentThread, isStatic, fieldClass->romClass, field, romMethod);
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
* @param jitCompileTimeResolve true for JIT compilation or AOT resolves, false for a runtime resolve
* @return void
*/
static void
checkForDecompile(J9VMThread *currentThread, J9ROMMethodRef *romMethodRef, bool jitCompileTimeResolve)
{
	J9JavaVM *vm = currentThread->javaVM;

	if (NULL != vm->decompileName) {
		J9JITConfig *jitConfig = vm->jitConfig;

		if (!jitCompileTimeResolve && (NULL != vm->jitConfig)) {
			J9UTF8 *name = J9ROMNAMEANDSIGNATURE_NAME(J9ROMMETHODREF_NAMEANDSIGNATURE(romMethodRef));
			char *decompileName = vm->decompileName;
			if (J9UTF8_DATA_EQUALS(J9UTF8_DATA(name), J9UTF8_LENGTH(name), decompileName, strlen(decompileName))) {
				if (NULL != jitConfig->jitHotswapOccurred) {
					acquireExclusiveVMAccess(currentThread);
					jitConfig->jitHotswapOccurred(currentThread);
					releaseExclusiveVMAccess(currentThread);
				}
			}
		}
	}
}

#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
static BOOLEAN
isMethodHandleINL(U_8 *methodName, U_16 methodNameLength)
{
	BOOLEAN isMethodHandle = FALSE;

	switch (methodNameLength) {
	case 11:
		if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "invokeBasic")) {
			isMethodHandle = TRUE;
		}
		break;
	case 12:
		if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "linkToStatic")
#if JAVA_SPEC_VERSION >= 22
			|| J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "linkToNative")
#endif /* JAVA_SPEC_VERSION >= 22 */
		) {
			isMethodHandle = TRUE;
		}
		break;
	case 13:
		if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "linkToSpecial")
			|| J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "linkToVirtual")
		) {
			isMethodHandle = TRUE;
		}
		break;
	case 15:
		if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "linkToInterface")) {
			isMethodHandle = TRUE;
		}
		break;
	default:
		break;
	}

	return isMethodHandle;
}
#endif /* defined(J9VM_OPT_OPENJDK_METHODHANDLE) */

UDATA   
packageAccessIsLegal(J9VMThread *currentThread, J9Class *targetClass, j9object_t protectionDomain, UDATA canRunJavaCode)
{
	UDATA legal = FALSE;
	j9object_t security = J9VMJAVALANGSYSTEM_SECURITY(currentThread, J9VMCONSTANTPOOL_CLASSREF_AT(currentThread->javaVM, J9VMCONSTANTPOOL_JAVALANGSYSTEM)->value);
	if (NULL == security) {
		legal = TRUE;
	} else if (canRunJavaCode) {
		if (J9_ARE_NO_BITS_SET(currentThread->privateFlags2, J9_PRIVATE_FLAGS2_CHECK_PACKAGE_ACCESS)) {
			currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_CHECK_PACKAGE_ACCESS;
			sendCheckPackageAccess(currentThread, targetClass, protectionDomain);
			currentThread->privateFlags2 &= ~J9_PRIVATE_FLAGS2_CHECK_PACKAGE_ACCESS;
		}
		if (!threadEventsPending(currentThread)) {
			legal = TRUE;
		}
	}
	return legal;
}

BOOLEAN
requirePackageAccessCheck(J9JavaVM *vm, J9ClassLoader *srcClassLoader, J9Module *srcModule, J9Class *targetClass)
{
	BOOLEAN checkFlag = TRUE;
	if (J2SE_VERSION(vm) >= J2SE_V11) {
		if (srcModule == targetClass->module) {
			if (NULL != srcModule) {
				/* same named module */
				checkFlag = FALSE;
			} else if (srcClassLoader == targetClass->classLoader) {
				/* same unnamed module */
				checkFlag = FALSE;
			}
		}
	}
	
	return checkFlag;
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

	/* Create a new string */
	stringRef = vmStruct->javaVM->memoryManagerFunctions->j9gc_createJavaLangString(vmStruct, J9UTF8_DATA(utf8Wrapper), J9UTF8_LENGTH(utf8Wrapper), J9_STR_TENURE | J9_STR_INTERN);
	
	/* If stringRef is NULL, the exception has already been set. */
	if ((NULL != stringRef) && J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_NO_CP_UPDATE)) {
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
		Assert_VM_true(IS_CLASS_SIGNATURE(J9UTF8_DATA(signature)[0]));
		/* skip fieldSignature's L and ; to have only CLASSNAME required for internalFindClassUTF8 */
		resolvedClass = internalFindClassUTF8(vmStruct, &J9UTF8_DATA(signature)[1], J9UTF8_LENGTH(signature)-2, classLoader, J9_FINDCLASS_FLAG_THROW_ON_FAIL);
	}

	return resolvedClass;
}

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
	bool jitCompileTimeResolve = J9_ARE_ANY_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_JIT_COMPILE_TIME);
	bool canRunJavaCode = !jitCompileTimeResolve && J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_REDEFINE_CLASS);
	bool throwException = canRunJavaCode && J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_NO_THROW_ON_FAIL);
	UDATA lookupOptions = 0;
	if (canRunJavaCode) {
		if (!throwException) {
			lookupOptions = J9_LOOK_NO_THROW;			
		}
	} else {
		lookupOptions = J9_LOOK_NO_JAVA;
	}
	UDATA findClassFlags = 0;
	UDATA accessModifiers = 0;
	j9object_t detailString = NULL;
	Trc_VM_resolveClassRef_Entry(vmStruct, ramCP, cpIndex, resolveFlags);

tryAgain:
	ramClassRefWrapper = (J9RAMClassRef *)&ramCP[cpIndex];
	resolvedClass = ramClassRefWrapper->value;
	/* If resolving for "new", check if the class is instantiable */
	if ((NULL != resolvedClass) && (J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_INSTANTIABLE) || J9ROMCLASS_ALLOCATES_VIA_NEW(resolvedClass->romClass))) {
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

	if (canRunJavaCode) {
		if (throwException) {
			findClassFlags |= J9_FINDCLASS_FLAG_THROW_ON_FAIL;
		}
		if (J9_ARE_ANY_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_NO_CLASS_LOAD)) {
			findClassFlags |= J9_FINDCLASS_FLAG_EXISTING_ONLY;		
		}
	} else {
		findClassFlags |= J9_FINDCLASS_FLAG_EXISTING_ONLY;
	}

	if (ramClassRefWrapper->modifiers == (UDATA)-1) {
		if ((findClassFlags & J9_FINDCLASS_FLAG_THROW_ON_FAIL) == J9_FINDCLASS_FLAG_THROW_ON_FAIL) {
			detailString = vm->memoryManagerFunctions->j9gc_createJavaLangString(vmStruct, classNameData, classNameLength, 0);
			if (throwException) {
				if (NULL == vmStruct->currentException) {
					setCurrentException(vmStruct, J9VMCONSTANTPOOL_JAVALANGNOCLASSDEFFOUNDERROR, (UDATA *)detailString);
				}
			} else {
				VM_VMHelpers::clearException(vmStruct);
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
		if (J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_NO_CP_UPDATE)) {
			j9object_t exception = vmStruct->currentException;
			/* Class not found - if NoClassDefFoundError was thrown, mark this ref as permanently unresolveable */
			if (NULL != exception) {
				if (instanceOfOrCheckCast(J9OBJECT_CLAZZ(vmStruct, exception), J9VMJAVALANGLINKAGEERROR_OR_NULL(vm))) {
					ramClassRefWrapper->modifiers = -1;
				}
			}
		}
		goto done;
	}

	/* Perform a package access check from the current class to the resolved class.
	 * No check is required if any of the following is true:
	 * 		- the current class and resolved class are identical
	 * 		- the current class was loaded by the bootstrap class loader
	 * 		- the current class and resolved class are in same module
	 */
	if ((currentClass != resolvedClass) 
		&& (classLoader != bootstrapClassLoader)
		&& requirePackageAccessCheck(vm, classLoader, currentClass->module, resolvedClass)
	) {
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
		if (J9_ARE_NO_BITS_SET(resolvedClass->romClass->modifiers, J9AccInterface)) {
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
		IDATA checkResult = checkVisibility(vmStruct, J9_CLASS_FROM_CP(ramCP), accessClass, accessModifiers, lookupOptions);
		if (checkResult < J9_VISIBILITY_ALLOWED) {
			/* Check for pending exception for (ie. Nesthost class loading/verify), do not overwrite these exceptions */
			if (canRunJavaCode && (!VM_VMHelpers::exceptionPending(vmStruct))) {
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
		if (!J9ROMCLASS_ALLOCATES_VIA_NEW(resolvedClass->romClass)) {
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

	if (J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_NO_CP_UPDATE)) {
		ramClassRefWrapper->value = resolvedClass;
		ramClassRefWrapper->modifiers = accessModifiers;
	}

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
	J9Class *cpClass;
	J9Class *methodClass = NULL;
	BOOLEAN isResolvedClassAnInterface = FALSE;
	J9ROMNameAndSignature *nameAndSig = NULL;
	bool jitCompileTimeResolve = J9_ARE_ANY_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_JIT_COMPILE_TIME);
	bool canRunJavaCode = !jitCompileTimeResolve && J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_REDEFINE_CLASS);
	bool throwException = canRunJavaCode && J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_NO_THROW_ON_FAIL);
	UDATA lookupOptions = 0;
	if (canRunJavaCode) {
		if (!throwException) {
			lookupOptions = J9_LOOK_NO_THROW;			
		}
	} else {
		lookupOptions = J9_LOOK_NO_JAVA;
	}

	Trc_VM_resolveStaticMethodRef_Entry(vmStruct, ramCP, cpIndex, resolveFlags);

tryAgain:
	romMethodRef = (J9ROMMethodRef *)&ramCP->romConstantPool[cpIndex];
	nameAndSig = J9ROMMETHODREF_NAMEANDSIGNATURE(romMethodRef);

	checkForDecompile(vmStruct, romMethodRef, jitCompileTimeResolve);

#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
	/* Stack allocate a J9NameAndSignature structure for polymorphic signature methods */
	J9NameAndSignature onStackNAS = {NULL, NULL};
	J9UTF8 nullSignature = {0};
#endif /* defined(J9VM_OPT_OPENJDK_METHODHANDLE) */

	/* Resolve the class. */
	resolvedClass = resolveClassRef(vmStruct, ramCP, romMethodRef->classRefCPIndex, resolveFlags);
	if (resolvedClass == NULL) {
		goto done;
	}
	isResolvedClassAnInterface = (J9AccInterface == (resolvedClass->romClass->modifiers & J9AccInterface));

	/* Find the method. */
	lookupOptions |= J9_LOOK_STATIC;
	if ((resolveFlags & J9_RESOLVE_FLAG_JCL_CONSTANT_POOL) == J9_RESOLVE_FLAG_JCL_CONSTANT_POOL) {
		cpClass = NULL;
	} else {
		cpClass = J9_CLASS_FROM_CP(ramCP);
		lookupOptions |= J9_LOOK_CLCONSTRAINTS;

		if (J2SE_VERSION(vmStruct->javaVM) >= J2SE_V11) {
			/* This check is only required in Java9 and there have been applications that
			 * fail when this check is enabled on Java8.
			 */
			if ((cpClass != NULL) && (cpClass->romClass != NULL)) {
				UDATA cpType = J9_CP_TYPE(J9ROMCLASS_CPSHAPEDESCRIPTION(cpClass->romClass), cpIndex);
				if (isResolvedClassAnInterface) {
					if ((J9CPTYPE_INTERFACE_STATIC_METHOD != cpType)
					&& (J9CPTYPE_INTERFACE_INSTANCE_METHOD != cpType)
					&& (J9CPTYPE_INTERFACE_METHOD != cpType)
					) {
						goto incompat;
					}
				} else {
					if ((J9CPTYPE_INTERFACE_STATIC_METHOD == cpType)
					|| (J9CPTYPE_INTERFACE_INSTANCE_METHOD == cpType)
					|| (J9CPTYPE_INTERFACE_METHOD == cpType)
					) {
						goto incompat;
					}
				}
			}
		}
	}
	if (isResolvedClassAnInterface) {
		lookupOptions |= J9_LOOK_INTERFACE;
	}

#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
	if (resolvedClass == J9VMJAVALANGINVOKEMETHODHANDLE(vmStruct->javaVM)) {
		J9UTF8 *nameUTF = J9ROMNAMEANDSIGNATURE_NAME(nameAndSig);
		/**
		 * Check for MH intrinsic methods
		 *
		 * Modify the signature to avoid signature mismatch due to varargs
		 * These methods have special INL send targets
		 */
		U_8* methodName = J9UTF8_DATA(nameUTF);
		U_16 methodNameLength = J9UTF8_LENGTH(nameUTF);

		if (isMethodHandleINL(methodName, methodNameLength)) {
			/* Create new J9NameAndSignature */
			onStackNAS.name = nameUTF;
			onStackNAS.signature = &nullSignature;
			nameAndSig = (J9ROMNameAndSignature *)(&onStackNAS);

			/* Set flag for partial signature lookup. Signature length is already initialized to 0. */
			lookupOptions |= (J9_LOOK_DIRECT_NAS | J9_LOOK_PARTIAL_SIGNATURE);
		}
	}
#endif /* defined(J9VM_OPT_OPENJDK_METHODHANDLE) */
	method = (J9Method *)javaLookupMethod(vmStruct, resolvedClass, nameAndSig, cpClass, lookupOptions);

	Trc_VM_resolveStaticMethodRef_lookupMethod(vmStruct, method);

	if (method == NULL) {
		goto done;
	}

	methodClass = J9_CLASS_FROM_METHOD(method);
	if (methodClass != resolvedClass) {
		if (isResolvedClassAnInterface) {
incompat:
			if (throwException) {
				setCurrentException(vmStruct, J9VMCONSTANTPOOL_JAVALANGINCOMPATIBLECLASSCHANGEERROR, NULL);
			}
			method = NULL;
			goto done;
		}
	}

	/* Initialize the defining class of the method. */
	
	if (J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_NO_CLASS_INIT)) {
		J9Class *methodClass = J9_CLASS_FROM_METHOD(method);
		UDATA initStatus = methodClass->initializeStatus;
		if (jitCompileTimeResolve && (J9ClassInitSucceeded != initStatus)) {
			method = NULL;
			goto done;
		}
		if (initStatus != J9ClassInitSucceeded && initStatus != (UDATA)vmStruct) {
			/* Initialize the class if java code is allowed */
			if (canRunJavaCode) {
				UDATA preCount = vmStruct->javaVM->hotSwapCount;

				initializeClass(vmStruct, methodClass);
				if (threadEventsPending(vmStruct)) {
					method = NULL;
					goto done;
				}
				if (preCount != vmStruct->javaVM->hotSwapCount) {
					goto tryAgain;
				}
			} else {
				/* Can't initialize the class, so fail the resolve */
				method = NULL;
				goto done;
			}
		}
	}
	if ((NULL != ramCPEntry) && J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_NO_CP_UPDATE)) {
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
		} else if (J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_NO_CP_UPDATE)) {
			((J9RAMStaticMethodRef *)&ramCP[cpIndex])->method = ramStaticMethodRef->method;
		}
	}

	return method;
}


J9Method *   
resolveStaticSplitMethodRef(J9VMThread *vmStruct, J9ConstantPool *ramCP, UDATA splitTableIndex, UDATA resolveFlags)
{
	J9RAMStaticMethodRef *ramStaticMethodRef = (J9RAMStaticMethodRef *)&vmStruct->floatTemp1;
	U_16 cpIndex = *(J9ROMCLASS_STATICSPLITMETHODREFINDEXES(ramCP->ramClass->romClass) + splitTableIndex);
	J9Method *method = ramCP->ramClass->staticSplitMethodTable[splitTableIndex];

	if (method == (J9Method*)vmStruct->javaVM->initialMethods.initialStaticMethod) {
		method = resolveStaticMethodRefInto(vmStruct, ramCP, cpIndex, resolveFlags, ramStaticMethodRef);

		if (NULL != method) {
			/* Check for <clinit> case. */
			if (((resolveFlags & J9_RESOLVE_FLAG_CHECK_CLINIT) == J9_RESOLVE_FLAG_CHECK_CLINIT)
				&& (J9_CLASS_FROM_METHOD(method)->initializeStatus == (UDATA)vmStruct)
			) {
				return (J9Method *) -1;
			} else if (J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_NO_CP_UPDATE)) {			
				ramCP->ramClass->staticSplitMethodTable[splitTableIndex] = method;
			}
		}
	}

	return method;
}


void *    
resolveStaticFieldRefInto(J9VMThread *vmStruct, J9Method *method, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags, J9ROMFieldShape **resolvedField, J9RAMStaticFieldRef *ramCPEntry)
{
	void *staticAddress;
	J9ROMFieldRef *romFieldRef;
	J9Class *resolvedClass;
	bool jitCompileTimeResolve = J9_ARE_ANY_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_JIT_COMPILE_TIME);
	bool canRunJavaCode = !jitCompileTimeResolve && J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_REDEFINE_CLASS);
	bool throwException = canRunJavaCode && J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_NO_THROW_ON_FAIL);
	UDATA lookupOptions = 0;
	if (canRunJavaCode) {
		if (!throwException) {
			lookupOptions = J9_LOOK_NO_THROW;			
		}
	} else {
		lookupOptions = J9_LOOK_NO_JAVA;
	}

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
		J9ROMNameAndSignature *nameAndSig;
		J9UTF8 *name;
		J9UTF8 *signature;
		IDATA checkResult = J9_VISIBILITY_NON_MODULE_ACCESS_ERROR;
		IDATA badMemberModifier = 0;
		J9Class *targetClass = NULL;
		UDATA modifiers = 0;
		J9Class *localClassAndFlags = NULL;
		UDATA initStatus = 0;

		/* ensure that the class is visible */
		checkResult = checkVisibility(vmStruct, classFromCP, resolvedClass, resolvedClass->romClass->modifiers, lookupOptions);
		if (checkResult < J9_VISIBILITY_ALLOWED) {
			targetClass = resolvedClass;
			badMemberModifier = resolvedClass->romClass->modifiers;
			goto illegalAccess;
		}

		/* Get the field address. */
		nameAndSig = J9ROMFIELDREF_NAMEANDSIGNATURE(romFieldRef);
		name = J9ROMNAMEANDSIGNATURE_NAME(nameAndSig);
		signature = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig);
		staticAddress = staticFieldAddress(vmStruct, resolvedClass, J9UTF8_DATA(name), J9UTF8_LENGTH(name), J9UTF8_DATA(signature), J9UTF8_LENGTH(signature), &definingClass, (UDATA *)&field, lookupOptions, classFromCP);
		/* Stop if an exception occurred. */
		if (staticAddress != NULL) {
			modifiers = field->modifiers;
			localClassAndFlags = definingClass;
			initStatus = definingClass->initializeStatus;

			if (resolvedField != NULL) {
				/* save away field for callee */
				*resolvedField = field;
			}

			if (J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_NO_CLASS_INIT)) {
				if (initStatus != J9ClassInitSucceeded && initStatus != (UDATA) vmStruct) {
					/* Initialize the class if java code is allowed */
					if (canRunJavaCode) {
						UDATA preCount = javaVM->hotSwapCount;

						initializeClass(vmStruct, definingClass);
						if (threadEventsPending(vmStruct)) {
							staticAddress = NULL;
							goto done;
						}
						if (preCount != javaVM->hotSwapCount) {
							goto tryAgain;
						}
					} else {
						/* Can't initialize the class, so fail the resolve */
						staticAddress = NULL;
						goto done;
					}
				}
			}

			if ((resolveFlags & J9_RESOLVE_FLAG_FIELD_SETTER) != 0 && (modifiers & J9AccFinal) != 0) {
				checkResult = checkVisibility(vmStruct, classFromCP, definingClass, J9AccPrivate, lookupOptions | J9_LOOK_NO_NESTMATES);
				if (checkResult < J9_VISIBILITY_ALLOWED) {
					targetClass = definingClass;
					badMemberModifier = J9AccPrivate;
illegalAccess:
					staticAddress = NULL;
					if (canRunJavaCode && !threadEventsPending(vmStruct)) {
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
				if (!finalFieldSetAllowed(vmStruct, true, method, definingClass, classFromCP, field, canRunJavaCode)) {
					staticAddress = NULL;
					goto done;
				} else { /* finalFieldSetAllowed */
					if (jitCompileTimeResolve) {
						/* Don't report the final field modification for JIT compile-time resolves.
						 * Reporting may deadlock due to interaction between safepoint and ClassUnloadMutex.
						 */
						staticAddress = NULL;
						goto done;
					}
					VM_VMHelpers::reportFinalFieldModified(vmStruct, definingClass);
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
					if (0 != j9bcv_checkClassLoadingConstraintsForSignature(vmStruct, cl1, cl2, signature, fieldSignature, FALSE)) {
						if (throwException) {
							setCurrentException(vmStruct, J9VMCONSTANTPOOL_JAVALANGVERIFYERROR, NULL);
						}
						staticAddress = NULL;
						goto done;
					}
				}
			}
			
			/* If this is a JIT compile-time resolve, do not allow fields declared in uninitialized classes. */
			if (jitCompileTimeResolve && (J9ClassInitSucceeded != initStatus)) {
				goto done;
			}

			if ((NULL != ramCPEntry) && J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_NO_CP_UPDATE)) {
				UDATA localClassAndFlagsData = (UDATA)localClassAndFlags;
				/* Add bits to the class for the field type */
				if ((modifiers & J9FieldFlagObject) == J9FieldFlagObject) {
					localClassAndFlagsData |= J9StaticFieldRefTypeObject;
				} else {
					switch(modifiers & J9FieldTypeMask) {
					case J9FieldTypeBoolean:
						localClassAndFlagsData |= J9StaticFieldRefTypeBoolean;
						break;
					case J9FieldTypeByte:
						localClassAndFlagsData |= J9StaticFieldRefTypeByte;
						break;
					case J9FieldTypeChar:
						localClassAndFlagsData |= J9StaticFieldRefTypeChar;
						break;
					case J9FieldTypeShort:
						localClassAndFlagsData |= J9StaticFieldRefTypeShort;
						break;
					case J9FieldTypeInt:
					case J9FieldTypeFloat:
						localClassAndFlagsData |= J9StaticFieldRefTypeIntFloat;
						break;
					case J9FieldTypeLong:
					case J9FieldTypeDouble:
						localClassAndFlagsData |= J9StaticFieldRefTypeLongDouble;
						break;
					}
				}
				/* Set the volatile, final and setter bits in the flags as needed */
				if ((modifiers & J9AccVolatile) == J9AccVolatile) {
					localClassAndFlagsData |= J9StaticFieldRefVolatile;
				}
				if ((modifiers & J9AccFinal) == J9AccFinal) {
					localClassAndFlagsData |= J9StaticFieldRefFinal;
				}
				if (0 != (resolveFlags & J9_RESOLVE_FLAG_FIELD_SETTER)) {
					localClassAndFlagsData |= J9StaticFieldRefPutResolved;
				}
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
				if (J9_ARE_ALL_BITS_SET(modifiers, J9FieldFlagIsNullRestricted)) {
					localClassAndFlagsData |= J9StaticFieldIsNullRestricted;
				}
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
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
		if (J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_NO_CP_UPDATE)) {
			((J9RAMStaticFieldRef *)&ramCP[cpIndex])->valueOffset = ramStaticFieldRef->valueOffset;
			((J9RAMStaticFieldRef *)&ramCP[cpIndex])->flagsAndClass = ramStaticFieldRef->flagsAndClass;
		}
	}

	return staticAddress;
}


IDATA   
resolveInstanceFieldRefInto(J9VMThread *vmStruct, J9Method *method, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags, J9ROMFieldShape **resolvedField, J9RAMFieldRef *ramCPEntry)
{
	IDATA fieldOffset = -1;
	J9ROMFieldRef *romFieldRef;
	J9Class *resolvedClass;
	bool jitCompileTimeResolve = J9_ARE_ANY_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_JIT_COMPILE_TIME);
	bool canRunJavaCode = !jitCompileTimeResolve && J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_REDEFINE_CLASS);
	bool throwException = canRunJavaCode && J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_NO_THROW_ON_FAIL);
	UDATA lookupOptions = 0;
	if (canRunJavaCode) {
		if (!throwException) {
			lookupOptions = J9_LOOK_NO_THROW;			
		}
	} else {
		lookupOptions = J9_LOOK_NO_JAVA;
	}

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
		J9ROMNameAndSignature *nameAndSig;
		J9UTF8 *name;
		J9UTF8 *signature;
		IDATA checkResult = J9_VISIBILITY_NON_MODULE_ACCESS_ERROR;
		IDATA badMemberModifier = 0;
		J9Class *targetClass = NULL;
		UDATA modifiers = 0;
		char *nlsStr = NULL;
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
		UDATA fieldIndex = 0;
		bool fccEntryFieldNotSet = true;
		J9Class *flattenableClass = NULL;
		J9FlattenedClassCache *flattenedClassCache = NULL;
#endif /* J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES */
		J9Class *currentTargetClass = NULL;
		J9Class *currentSenderClass = NULL;

		/* ensure that the class is visible */
		checkResult = checkVisibility(vmStruct, classFromCP, resolvedClass, resolvedClass->romClass->modifiers, lookupOptions);
		if (checkResult < J9_VISIBILITY_ALLOWED) {
			badMemberModifier = resolvedClass->romClass->modifiers;
			targetClass = resolvedClass;
			goto illegalAccess;
		}
		/* Reset checkResult to the default error case */
		checkResult = J9_VISIBILITY_NON_MODULE_ACCESS_ERROR;

		/* Get the field address. */
		nameAndSig = J9ROMFIELDREF_NAMEANDSIGNATURE(romFieldRef);
		name = J9ROMNAMEANDSIGNATURE_NAME(nameAndSig);
		signature = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig);
		/**
		 * This is an optimization that searches for a field offset in the FCC. 
		 * If the offset is found there is no need to repeat the process. 
		 * Also, since this optimization is only done for ValueTypes, 
		 * the resolvedClass will always be the class that owns the field, 
		 * since ValueType superclasses can not have fields.
		 */
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
		if (J9_IS_J9CLASS_VALUETYPE(resolvedClass)) {
			flattenedClassCache = resolvedClass->flattenedClassCache;
			fieldIndex = findIndexInFlattenedClassCache(flattenedClassCache, nameAndSig);
			if (UDATA_MAX != fieldIndex) {
				J9FlattenedClassCacheEntry * flattenedClassCacheEntry = J9_VM_FCC_ENTRY_FROM_FCC(flattenedClassCache, fieldIndex);
				fieldOffset = flattenedClassCacheEntry->offset;
				if (-1 != fieldOffset) {
					definingClass = resolvedClass;
					field = flattenedClassCacheEntry->field;
					flattenableClass = J9_VM_FCC_CLASS_FROM_ENTRY(flattenedClassCacheEntry);
					fccEntryFieldNotSet = false;
				}
			}
		}
		if (fccEntryFieldNotSet) 
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
		{
			fieldOffset = instanceFieldOffsetWithSourceClass(vmStruct, resolvedClass, J9UTF8_DATA(name), J9UTF8_LENGTH(name), J9UTF8_DATA(signature), J9UTF8_LENGTH(signature), &definingClass, (UDATA *)&field, lookupOptions, classFromCP);
		}
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

			if ((modifiers & J9AccFinal) != 0) {
				if (J9_ARE_ANY_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_FIELD_SETTER)) {
					checkResult = checkVisibility(vmStruct, classFromCP, definingClass, J9AccPrivate, lookupOptions | J9_LOOK_NO_NESTMATES);
					if (checkResult < J9_VISIBILITY_ALLOWED) {
						badMemberModifier = J9AccPrivate;
						targetClass = definingClass;
illegalAccess:
						fieldOffset = -1;
						if (throwException) {
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
					if (!finalFieldSetAllowed(vmStruct, false, method, definingClass, classFromCP, field, canRunJavaCode)) {
						fieldOffset = -1;
						goto done;
					}
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
					if (0 != j9bcv_checkClassLoadingConstraintsForSignature(vmStruct, cl1, cl2, signature, fieldSignature, FALSE)) {
						if (throwException) {
							setCurrentException(vmStruct, J9VMCONSTANTPOOL_JAVALANGVERIFYERROR, NULL);
						}
						fieldOffset = -1;
						goto done;
					}
				}
			}
		
			if ((NULL != ramCPEntry) && J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_NO_CP_UPDATE)) {
				UDATA valueOffset = fieldOffset;
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
				if (J9ROMFIELD_IS_NULL_RESTRICTED(field)) {
					if (fccEntryFieldNotSet) {
						flattenedClassCache = definingClass->flattenedClassCache;
						fieldIndex = findIndexInFlattenedClassCache(flattenedClassCache, nameAndSig);
						flattenableClass = J9_VM_FCC_ENTRY_FROM_FCC(flattenedClassCache, fieldIndex)->clazz;
					}
					if (J9_IS_FIELD_FLATTENED(flattenableClass, field)) {
						if (fccEntryFieldNotSet) {
							J9_VM_FCC_ENTRY_FROM_FCC(flattenedClassCache, fieldIndex)->offset = valueOffset;
						}
						modifiers |= J9FieldFlagFlattened;
						valueOffset = (UDATA) J9_VM_FCC_ENTRY_FROM_FCC(flattenedClassCache, fieldIndex);
						/* offset must be written to flattenedClassCache before fieldref is marked as resolved */
						issueWriteBarrier();
					}
				}
#endif /* J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES */
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
	J9ROMNameAndSignature *nameAndSig;
	J9Method *method;
	J9Class *cpClass;
	J9JavaVM *vm = vmStruct->javaVM;
	bool jitCompileTimeResolve = J9_ARE_ANY_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_JIT_COMPILE_TIME);
	bool canRunJavaCode = !jitCompileTimeResolve && J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_REDEFINE_CLASS);
	bool throwException = canRunJavaCode && J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_NO_THROW_ON_FAIL);
	UDATA lookupOptions = 0;
	if (canRunJavaCode) {
		if (!throwException) {
			lookupOptions = J9_LOOK_NO_THROW;			
		}
	} else {
		lookupOptions = J9_LOOK_NO_JAVA;
	}

	Trc_VM_resolveInterfaceMethodRef_Entry(vmStruct, ramCP, cpIndex, resolveFlags);

	romMethodRef = (J9ROMMethodRef *)&ramCP->romConstantPool[cpIndex];

	checkForDecompile(vmStruct, romMethodRef, jitCompileTimeResolve);

	/* Resolve the class. */
	interfaceClass = resolveClassRef(vmStruct, ramCP, romMethodRef->classRefCPIndex, resolveFlags);
	
	/* If interfaceClass is NULL, the exception has already been set. */
	if (interfaceClass == NULL) {
		goto done;
	}

	if ((interfaceClass->romClass->modifiers & J9AccInterface) != J9AccInterface) {
		if (throwException) {
			J9UTF8 *className = J9ROMCLASS_CLASSNAME(interfaceClass->romClass);
			j9object_t detailMessage = vm->memoryManagerFunctions->j9gc_createJavaLangString(vmStruct, J9UTF8_DATA(className), J9UTF8_LENGTH(className), J9_STR_XLAT);
			setCurrentException(vmStruct, J9VMCONSTANTPOOL_JAVALANGINCOMPATIBLECLASSCHANGEERROR, (UDATA *)detailMessage);
		}
		goto done;
	}
	
	nameAndSig = J9ROMFIELDREF_NAMEANDSIGNATURE(romMethodRef);
	lookupOptions |= J9_LOOK_INTERFACE;
	if ((resolveFlags & J9_RESOLVE_FLAG_JCL_CONSTANT_POOL) == J9_RESOLVE_FLAG_JCL_CONSTANT_POOL) {
		cpClass = NULL;
	} else {
		cpClass = J9_CLASS_FROM_CP(ramCP);
		lookupOptions |= J9_LOOK_CLCONSTRAINTS;
	}
	method = (J9Method *)javaLookupMethod(vmStruct, interfaceClass, nameAndSig, cpClass, lookupOptions);

	Trc_VM_resolveInterfaceMethodRef_lookupMethod(vmStruct, method);
	
	/* If method is NULL, the exception has already been set. */
	if (method != NULL) {
		if (ramCPEntry != NULL) {
			J9RAMInterfaceMethodRef *ramInterfaceMethodRef = (J9RAMInterfaceMethodRef *)&ramCP[cpIndex];
			J9Class *methodClass = J9_CLASS_FROM_METHOD(method);
			UDATA methodIndex = 0;
			UDATA oldArgCount = ramInterfaceMethodRef->methodIndexAndArgCount & 255;
			UDATA tagBits = 0;
			J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
			if (J9_ARE_ANY_BITS_SET(methodClass->romClass->modifiers, J9AccInterface)) {
				/* Resolved method is in an interface class */
				if (J9_ARE_ANY_BITS_SET(romMethod->modifiers, J9AccPrivate)) {
					/* Resolved method is a private interface method which does not appear in the
					 * vTable or iTable.  Use the index into ramMethods in interfaceClass. This is
					 * only allowed in JDK11 and beyond.
					 */
					if (J2SE_VERSION(vm) < J2SE_V11) {
nonpublic:
						if (throwException) {
							setIllegalAccessErrorNonPublicInvokeInterface(vmStruct, method);
						}
						goto done;
					}
					methodIndex = (method - methodClass->ramMethods);
					tagBits |= J9_ITABLE_INDEX_METHOD_INDEX;
				} else {
					/* Resolved method is a public interface method which appears in the
					 * vTable and iTable.  Use the iTable index in interfaceClass.
					 */
					methodIndex = getITableIndexForMethod(method, interfaceClass);
				}
			} else {
				/* Resolved method is in Object */
				Assert_VM_true(methodClass == J9VMJAVALANGOBJECT_OR_NULL(vm));
				/* Interfaces inherit only public methods from Object */
				if (J9_ARE_NO_BITS_SET(romMethod->modifiers, J9AccPublic)) {
					goto nonpublic;
				}
				if (J9ROMMETHOD_HAS_VTABLE(romMethod)) {
					/* Resolved method is in the vTable, so it must be invoked via the
					 * receiver's vTable.
					 */
					methodIndex = getVTableOffsetForMethod(method, methodClass, vmStruct);
				} else {
					/* Resolved method is not in the vTable, so invoke it directly
					 * via the index into ramMethods in Object.
					 */
					methodIndex = (method - methodClass->ramMethods);
					tagBits |= J9_ITABLE_INDEX_METHOD_INDEX;
				}
				tagBits |= J9_ITABLE_INDEX_OBJECT;
			}
			/* Ensure methodIndex can be shifted without losing any bits */
			Assert_VM_true(methodIndex < ((UDATA)1 << ((sizeof(UDATA) * 8) - J9_ITABLE_INDEX_SHIFT)));
			if (J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_NO_CP_UPDATE)) {
				methodIndex <<= J9_ITABLE_INDEX_SHIFT;
				methodIndex = methodIndex | tagBits | oldArgCount;
				ramCPEntry->methodIndexAndArgCount = methodIndex;
				ramCPEntry->interfaceClass = (UDATA)interfaceClass;
			}
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
	J9Method *method = NULL;
	bool jitCompileTimeResolve = J9_ARE_ANY_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_JIT_COMPILE_TIME);
	bool canRunJavaCode = !jitCompileTimeResolve && J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_REDEFINE_CLASS);
	bool throwException = canRunJavaCode && J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_NO_THROW_ON_FAIL);
	UDATA lookupOptions = 0;
	if (canRunJavaCode) {
		if (!throwException) {
			lookupOptions = J9_LOOK_NO_THROW;			
		}
	} else {
		lookupOptions = J9_LOOK_NO_JAVA;
	}

	Trc_VM_resolveSpecialMethodRef_Entry(vmStruct, ramCP, cpIndex, resolveFlags);

	romMethodRef = (J9ROMMethodRef *)&ramCP->romConstantPool[cpIndex];

	checkForDecompile(vmStruct, romMethodRef, jitCompileTimeResolve);

	/* Resolve the class. */
	resolvedClass = resolveClassRef(vmStruct, ramCP, romMethodRef->classRefCPIndex, resolveFlags);
	
	/* If resolvedClass is NULL, the exception has already been set. */
	if (resolvedClass == NULL) {
		goto done;
	}

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
	lookupOptions |= (J9_LOOK_VIRTUAL | J9_LOOK_HANDLE_DEFAULT_METHOD_CONFLICTS);

	if ((resolveFlags & J9_RESOLVE_FLAG_JCL_CONSTANT_POOL) != J9_RESOLVE_FLAG_JCL_CONSTANT_POOL) {
		lookupOptions |= J9_LOOK_CLCONSTRAINTS;
	}

	if (J2SE_VERSION(vmStruct->javaVM) >= J2SE_V11) {
		/* This check is only required in Java9 and there have been applications that
		 * fail when this check is enabled on Java8.
		 */
		if (currentClass != NULL) {
			if ((resolvedClass->romClass != NULL) && (currentClass->romClass != NULL)) {
				/* Ensure the cpType is taken from the original class.  The cpIndex
				 * and J9Class must match, even if a redefinition occurs, or the
				 * check may incorrectly fail.
				 */
				UDATA cpType = J9_CP_TYPE(J9ROMCLASS_CPSHAPEDESCRIPTION(J9_CLASS_FROM_CP(ramCP)->romClass), cpIndex);
				if (J9AccInterface == (resolvedClass->romClass->modifiers & J9AccInterface)) {
					if ((J9CPTYPE_INTERFACE_INSTANCE_METHOD != cpType)
					&& (J9CPTYPE_INTERFACE_STATIC_METHOD != cpType)
					&& (J9CPTYPE_INTERFACE_METHOD != cpType)
					) {
incompat:
						if (throwException) {
							setCurrentException(vmStruct, J9VMCONSTANTPOOL_JAVALANGINCOMPATIBLECLASSCHANGEERROR, NULL);
						}
						goto done;
					}
				} else {
					if ((J9CPTYPE_INTERFACE_INSTANCE_METHOD == cpType)
					|| (J9CPTYPE_INTERFACE_STATIC_METHOD == cpType)
					|| (J9CPTYPE_INTERFACE_METHOD == cpType)
					) {
						goto incompat;
					}
				}
			}
		}
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
			if (throwException) {
				setIncompatibleClassChangeErrorInvalidDefenderSupersend(vmStruct, resolvedClass, currentClass);
			}
			method = NULL;
			goto done;
		}
	}

	/* Select the correct method for invocation - ignore visibility in the super send case */
	method = getMethodForSpecialSend(vmStruct, currentClass, resolvedClass, method, lookupOptions | J9_LOOK_NO_VISIBILITY_CHECK | J9_LOOK_IGNORE_INCOMPATIBLE_METHODS);
	if (NULL == method) {
		goto done;
	}

	if ((NULL != ramCPEntry) && J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_NO_CP_UPDATE)) {
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


J9Method *   
resolveSpecialSplitMethodRef(J9VMThread *vmStruct, J9ConstantPool *ramCP, UDATA splitTableIndex, UDATA resolveFlags)
{
	U_16 cpIndex = *(U_16 *)(J9ROMCLASS_SPECIALSPLITMETHODREFINDEXES(ramCP->ramClass->romClass) + splitTableIndex);
	J9Method *method = ramCP->ramClass->specialSplitMethodTable[splitTableIndex];
	
	if (method == (J9Method*)vmStruct->javaVM->initialMethods.initialSpecialMethod) {
		method = resolveSpecialMethodRefInto(vmStruct, ramCP, cpIndex, resolveFlags, NULL);

		if ((NULL != method) && J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_NO_CP_UPDATE)) {
			ramCP->ramClass->specialSplitMethodTable[splitTableIndex] = method;
		}
	}
	return method;
}


UDATA   
resolveVirtualMethodRefInto(J9VMThread *vmStruct, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags, J9Method **resolvedMethod, J9RAMVirtualMethodRef *ramCPEntry)
{
	UDATA vTableOffset = 0;
	J9ROMMethodRef *romMethodRef = NULL;
	J9Class *resolvedClass = NULL;
	J9JavaVM *vm = vmStruct->javaVM;
	bool jitCompileTimeResolve = J9_ARE_ANY_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_JIT_COMPILE_TIME);
	bool canRunJavaCode = !jitCompileTimeResolve && J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_REDEFINE_CLASS);
	bool throwException = canRunJavaCode && J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_NO_THROW_ON_FAIL);
	UDATA lookupOptions = 0;
	if (canRunJavaCode) {
		if (!throwException) {
			lookupOptions = J9_LOOK_NO_THROW;			
		}
	} else {
		lookupOptions = J9_LOOK_NO_JAVA;
	}

	Trc_VM_resolveVirtualMethodRef_Entry(vmStruct, ramCP, cpIndex, resolveFlags, resolvedMethod);

	romMethodRef = (J9ROMMethodRef *)&ramCP->romConstantPool[cpIndex];

	checkForDecompile(vmStruct, romMethodRef, jitCompileTimeResolve);

	/* Resolve the class. */
	resolvedClass = resolveClassRef(vmStruct, ramCP, romMethodRef->classRefCPIndex, resolveFlags);

	/* If resolvedClass is NULL, the exception has already been set. */
	if (NULL != resolvedClass) {
		J9ROMNameAndSignature *nameAndSig = J9ROMFIELDREF_NAMEANDSIGNATURE(romMethodRef);
		U_32 *cpShapeDescription = NULL;
		J9Method *method = NULL;
		J9Class *cpClass = NULL;

#if defined(J9VM_OPT_OPENJDK_METHODHANDLE) || (defined(J9VM_OPT_METHOD_HANDLE) && (JAVA_SPEC_VERSION >= 11))
		/* Stack allocate a byte array for MethodHandle & VarHandle method name and signature. The array size is:
		 *  - J9ROMNameAndSignature
		 *  - Modified method name
		 *      - U_16 for J9UTF8 length
		 *      - 26 bytes for the original method name ("compareAndExchangeAcquire" is the longest)
		 *      - 5 bytes for "_impl".
		 *  - J9UTF8 for empty signature
		 */
		U_8 onStackNAS[sizeof(J9ROMNameAndSignature) + (sizeof(U_16) + 26 + 5) + sizeof(J9UTF8)];
#endif /* defined(J9VM_OPT_OPENJDK_METHODHANDLE) || (defined(J9VM_OPT_METHOD_HANDLE) && (JAVA_SPEC_VERSION >= 11)) */

		lookupOptions |= J9_LOOK_VIRTUAL;
		if ((resolveFlags & J9_RESOLVE_FLAG_JCL_CONSTANT_POOL) == J9_RESOLVE_FLAG_JCL_CONSTANT_POOL) {
			cpClass = NULL;
		} else {
			cpClass = J9_CLASS_FROM_CP(ramCP);
			cpShapeDescription = J9ROMCLASS_CPSHAPEDESCRIPTION(cpClass->romClass);
			lookupOptions |= J9_LOOK_CLCONSTRAINTS;
		}

#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
		J9UTF8 nullSignature = {0};
		if ((NULL != cpShapeDescription) && (resolvedClass == J9VMJAVALANGINVOKEMETHODHANDLE(vm))) {
			/**
			 * Check for MH intrinsic methods
			 *
			 * Modify the signature to avoid signature mismatch due to varargs
			 * These methods have special INL send targets
			 */
			J9UTF8 *nameUTF = J9ROMNAMEANDSIGNATURE_NAME(nameAndSig);
			U_8* methodName = J9UTF8_DATA(nameUTF);
			U_16 methodNameLength = J9UTF8_LENGTH(nameUTF);

			if (isMethodHandleINL(methodName, methodNameLength)) {
				/* Create new J9NameAndSignature */
				((J9NameAndSignature *)onStackNAS)->name = nameUTF;
				((J9NameAndSignature *)onStackNAS)->signature = &nullSignature;
				nameAndSig = (J9ROMNameAndSignature *)(&onStackNAS);

				/* Set flag for partial signature lookup. Signature length is already initialized to 0. */
				lookupOptions |= (J9_LOOK_DIRECT_NAS | J9_LOOK_PARTIAL_SIGNATURE);

				/* Skip CL constraint check as the varargs signature may not match */
				lookupOptions &= ~J9_LOOK_CLCONSTRAINTS;
			}
		}
#elif defined(J9VM_OPT_METHOD_HANDLE) /* defined(J9VM_OPT_OPENJDK_METHODHANDLE) */
		if ((NULL != cpShapeDescription) && (resolvedClass == J9VMJAVALANGINVOKEMETHODHANDLE(vm))) {
			/*
			* Check for MH.invoke and MH.invokeExact.
			*
			* Methodrefs corresponding to those methods already have their methodIndex set to index into
			* cpClass->methodTypes. We resolve them by calling into MethodType.fromMethodDescriptorString()
			* and storing the result into the cpClass->methodTypes table.
			*/
			J9UTF8 *nameUTF = J9ROMNAMEANDSIGNATURE_NAME(nameAndSig);
			J9UTF8 *sigUTF = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig);
			if ((J9CPTYPE_HANDLE_METHOD == J9_CP_TYPE(cpShapeDescription, cpIndex))
			|| J9UTF8_LITERAL_EQUALS(J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF), "invokeExact")
			|| J9UTF8_LITERAL_EQUALS(J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF), "invoke")
			) {
				J9RAMMethodRef *ramMethodRef = (J9RAMMethodRef *)&ramCP[cpIndex];
				UDATA methodTypeIndex = ramMethodRef->methodIndexAndArgCount >> 8;
				j9object_t methodType = NULL;

				/* Return NULL if not allowed to run java code. The only way to resolve
				 * a MethodType object is to call-in using MethodType.fromMethodDescriptorString()
				 * which runs Java code.
				 */
				if (!canRunJavaCode) {
					goto done;
				}

				/* Throw LinkageError if more than 255 stack slots are
				 * required by the MethodType and the MethodHandle.
				 */
				if (MAX_STACK_SLOTS == (ramMethodRef->methodIndexAndArgCount & MAX_STACK_SLOTS)) {
					if (throwException) {
						setCurrentExceptionNLS(vmStruct, J9VMCONSTANTPOOL_JAVALANGLINKAGEERROR, J9NLS_VM_TOO_MANY_ARGUMENTS);
					}
					goto done;
				}

				/* Call VM Entry point to create the MethodType - Result is put into the
				 * vmThread->returnValue as entry points don't "return" in the expected way
				 */
				sendFromMethodDescriptorString(vmStruct, sigUTF, J9_CLASS_FROM_CP(ramCP)->classLoader, NULL);
				methodType = (j9object_t) vmStruct->returnValue;

				/* check if an exception is already pending */
				if (threadEventsPending(vmStruct)) {
					/* Already a pending exception */
					methodType = NULL;
				} else if (NULL == methodType) {
					/* Resolved MethodType was null - throw NPE that includes the lookupSignature from the NaS */
					j9object_t lookupSigString = vm->memoryManagerFunctions->j9gc_createJavaLangString(vmStruct, J9UTF8_DATA(sigUTF), J9UTF8_LENGTH(sigUTF), 0);
					if (throwException) {
						if (NULL == vmStruct->currentException) {
							setCurrentException(vmStruct, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, (UDATA*)lookupSigString);
						}
					} else {
						VM_VMHelpers::clearException(vmStruct);
					}
				}

				/* Only write the value in if its not null */
				if (NULL != methodType) {
					J9Class *clazz = J9_CLASS_FROM_CP(ramCP);
					j9object_t *methodTypeObjectP = clazz->methodTypes + methodTypeIndex;
					/* Overwriting NULL with an immortal pointer, so no exception can occur */
					J9STATIC_OBJECT_STORE(vmStruct, clazz, methodTypeObjectP, methodType);

					/* Record vTableOffset for the exit tracepoint. */
					vTableOffset = methodTypeIndex;
				}

				goto done;
			}
#if JAVA_SPEC_VERSION >= 11
		} else if (resolvedClass == J9VMJAVALANGINVOKEVARHANDLE_OR_NULL(vm)) {
			J9UTF8 *nameUTF = J9ROMNAMEANDSIGNATURE_NAME(nameAndSig);
			J9UTF8 *sigUTF = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig);
			U_8* initialMethodName = J9UTF8_DATA(nameUTF);
			U_16 initialMethodNameLength = J9UTF8_LENGTH(nameUTF);

			if (VM_VMHelpers::isPolymorphicVarHandleMethod(initialMethodName, initialMethodNameLength)) {
				J9UTF8 *modifiedMethodName = (J9UTF8 *)(onStackNAS + sizeof(J9ROMNameAndSignature));
				J9UTF8 *modifiedMethodSig = (J9UTF8 *)(onStackNAS + sizeof(onStackNAS) - sizeof(J9UTF8));
				memset(onStackNAS, 0, sizeof(onStackNAS));

				/* Create new J9ROMNameAndSignature */
				nameAndSig = (J9ROMNameAndSignature *)onStackNAS;
				NNSRP_SET(nameAndSig->name, modifiedMethodName);
				NNSRP_SET(nameAndSig->signature, modifiedMethodSig);

				/* Change method name to include the suffix "_impl", which are the methods with VarHandle send targets. */
				J9UTF8_SET_LENGTH(modifiedMethodName, initialMethodNameLength + 5);
				memcpy(J9UTF8_DATA(modifiedMethodName), initialMethodName, initialMethodNameLength);
				memcpy(J9UTF8_DATA(modifiedMethodName) + initialMethodNameLength, "_impl", 5);

				/* Set flag for partial signature lookup. Signature length is already initialized to 0. */
				lookupOptions |= J9_LOOK_PARTIAL_SIGNATURE;

				/* Redirect resolution to VarHandleInternal */
				resolvedClass = VM_VMHelpers::getSuperclass(resolvedClass);

				/* Ensure visibility passes */
				cpClass = resolvedClass;

				/* Resolve the MethodType. */
				if (canRunJavaCode) {
					j9object_t methodType = NULL;
					J9Class *ramClass = ramCP->ramClass;
					J9ROMClass *romClass = ramClass->romClass;
					J9RAMMethodRef *ramMethodRef = (J9RAMMethodRef *)&ramCP[cpIndex];

					/* Throw LinkageError if more than 255 stack slots are
					 * required by the MethodType and the MethodHandle.
					 */
					if (MAX_STACK_SLOTS == (ramMethodRef->methodIndexAndArgCount & MAX_STACK_SLOTS)) {
						if (throwException) {
							setCurrentExceptionNLS(vmStruct, J9VMCONSTANTPOOL_JAVALANGLINKAGEERROR, J9NLS_VM_TOO_MANY_ARGUMENTS);
						}
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
					sendFromMethodDescriptorString(vmStruct, sigUTF, J9_CLASS_FROM_CP(ramCP)->classLoader, J9VMJAVALANGINVOKEVARHANDLE_OR_NULL(vm));
					methodType = (j9object_t)vmStruct->returnValue;

					/* Check if an exception is already pending */
					if (threadEventsPending(vmStruct)) {
						/* Already a pending exception */
						methodType = NULL;
					} else if (NULL == methodType) {
						/* Resolved MethodType was null - throw NPE that includes the lookupSignature from the NaS */
						j9object_t lookupSigString = vm->memoryManagerFunctions->j9gc_createJavaLangString(vmStruct, J9UTF8_DATA(sigUTF), J9UTF8_LENGTH(sigUTF), 0);
						if (throwException) {
							if (NULL == vmStruct->currentException) {
								setCurrentException(vmStruct, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, (UDATA*)lookupSigString);
							}
						} else {
							VM_VMHelpers::clearException(vmStruct);
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
#endif /* JAVA_SPEC_VERSION >= 11 */
		}
#else /* defined(J9VM_OPT_METHOD_HANDLE) */
		Trc_VM_Assert_ShouldNeverHappen();
#endif /* defined(J9VM_OPT_METHOD_HANDLE) */

		method = (J9Method *)javaLookupMethod(vmStruct, resolvedClass, nameAndSig, cpClass, lookupOptions);

		Trc_VM_resolveVirtualMethodRef_lookupMethod(vmStruct, method);

		/* If method is NULL, the exception has already been set. */
		if (NULL != method) {
			J9ROMMethod* romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
			/* Only allow non-interface method to call invokePrivate, private interface method should use "invokeInterface" bytecode
			 * The else case will throw ICCE for private interface method 
			 */
			if (!J9ROMMETHOD_HAS_VTABLE(romMethod) && J9_ARE_NO_BITS_SET(resolvedClass->romClass->modifiers, J9AccInterface)) {
				/* Private method found, will not be in vTable, point vTable index to invokePrivate */
				if (NULL != ramCPEntry) {
					ramCPEntry->method = method;
					UDATA methodIndexAndArgCount = J9VTABLE_INVOKE_PRIVATE_OFFSET << 8;
					methodIndexAndArgCount |= (ramCPEntry->methodIndexAndArgCount & 255);
					ramCPEntry->methodIndexAndArgCount = methodIndexAndArgCount;
				}
				if (NULL != resolvedMethod) {
					/* save away method for callee */
					*resolvedMethod = method;
				}
			} else {
				/* Fill in the constant pool entry. Don't bother checking for failure on the vtable index, since we know the method is there. */
				vTableOffset = getVTableOffsetForMethod(method, resolvedClass, vmStruct);
				if (0 == vTableOffset) {
					if (throwException) {
						j9object_t errorString = methodToString(vmStruct, method);
						if (NULL != errorString) {
							setCurrentException(vmStruct, J9VMCONSTANTPOOL_JAVALANGINCOMPATIBLECLASSCHANGEERROR, (UDATA *)errorString);
						}
					}
				} else {
					if ((NULL != ramCPEntry) && J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_NO_CP_UPDATE)) {
						UDATA argSlotCount = vTableOffset << 8;
						argSlotCount |= (ramCPEntry->methodIndexAndArgCount & 255);
						ramCPEntry->methodIndexAndArgCount = argSlotCount;
					}
					if (NULL != resolvedMethod) {
						/* save away method for callee */
						*resolvedMethod = method;
					}
				}
			}
		}
	}

#if defined(J9VM_OPT_METHOD_HANDLE)
done:
#endif /* defined(J9VM_OPT_METHOD_HANDLE) */
	Trc_VM_resolveVirtualMethodRef_Exit(vmStruct, vTableOffset);
	return vTableOffset;
}

	
UDATA   
resolveVirtualMethodRef(J9VMThread *vmStruct, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags, J9Method **resolvedMethod)
{
	J9RAMVirtualMethodRef *ramVirtualMethodRef = (J9RAMVirtualMethodRef *)&ramCP[cpIndex];
	return resolveVirtualMethodRefInto(vmStruct, ramCP, cpIndex, resolveFlags, resolvedMethod, ramVirtualMethodRef);
}

j9object_t
resolveMethodTypeRefInto(J9VMThread *vmThread, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags, J9RAMMethodTypeRef *ramCPEntry)
{
	j9object_t methodType;
	J9ROMMethodTypeRef *romMethodTypeRef = NULL;
	J9UTF8 *lookupSig = NULL;
	bool jitCompileTimeResolve = J9_ARE_ANY_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_JIT_COMPILE_TIME);
	bool canRunJavaCode = !jitCompileTimeResolve && J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_REDEFINE_CLASS);
	bool throwException = canRunJavaCode && J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_NO_THROW_ON_FAIL);
	UDATA lookupOptions = 0;
	if (canRunJavaCode) {
		if (!throwException) {
			lookupOptions = J9_LOOK_NO_THROW;
		}
	} else {
		lookupOptions = J9_LOOK_NO_JAVA;
	}

	Trc_VM_sendResolveMethodTypeRefInto_Entry(vmThread, ramCP, cpIndex, resolveFlags);

	/* Check if already resolved */
	if (ramCPEntry->type != NULL) {
		return ramCPEntry->type;
	}

	/* Return NULL if not able to run java code. The only way to resolve
	 * a MethodType object is to call-in using MethodType.fromMethodDescriptorString()
	 * which runs Java code.
	 */
	if (!canRunJavaCode) {
		return NULL;
	}

	/* Call VM Entry point to create the MethodType - Result is put into the
	 * vmThread->returnValue as entry points don't "return" in the expected way
	 */
	romMethodTypeRef = ((J9ROMMethodTypeRef *) &(J9_ROM_CP_FROM_CP(ramCP)[cpIndex]));
	lookupSig = J9ROMMETHODTYPEREF_SIGNATURE(romMethodTypeRef);
	sendFromMethodDescriptorString(vmThread, lookupSig, J9_CLASS_FROM_CP(ramCP)->classLoader, NULL);
	methodType = (j9object_t) vmThread->returnValue;

	/* check if an exception is already pending */
	if (threadEventsPending(vmThread)) {
		/* Already a pending exception */
		methodType = NULL;
	} else if (methodType == NULL) {
		/* Resolved MethodType was null - throw NPE that includes the lookupSignature from the NaS */
		j9object_t lookupSigString = vmThread->javaVM->memoryManagerFunctions->j9gc_createJavaLangString(vmThread, J9UTF8_DATA(lookupSig), J9UTF8_LENGTH(lookupSig), 0);
		if (throwException) {
			if (NULL == vmThread->currentException) {
				setCurrentException(vmThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, (UDATA*)lookupSigString);
			}
		} else {
			VM_VMHelpers::clearException(vmThread);
		}
	}

	/* perform visibility checks for the returnType and all parameters */
	if (NULL != methodType) {
		/* check returnType */
		J9Class *senderClass = ramCP->ramClass;
		J9Class *returnTypeClass = J9VM_J9CLASS_FROM_HEAPCLASS(vmThread, J9VMJAVALANGINVOKEMETHODTYPE_RTYPE(vmThread, methodType));
		J9Class *illegalClass = NULL;
		IDATA visibilityReturnCode = 0;

		if (J9ROMCLASS_IS_ARRAY(senderClass->romClass)) {
			senderClass = ((J9ArrayClass *)senderClass)->leafComponentType;
		}
		if (J9ROMCLASS_IS_ARRAY(returnTypeClass->romClass)) {
			returnTypeClass = ((J9ArrayClass *)returnTypeClass)->leafComponentType;
		}

		visibilityReturnCode = checkVisibility(vmThread, senderClass, returnTypeClass, returnTypeClass->romClass->modifiers, lookupOptions);

		if (J9_VISIBILITY_ALLOWED != visibilityReturnCode) {
			illegalClass = returnTypeClass;
		} else {
			/* check paramTypes */
			j9object_t argTypesObject = J9VMJAVALANGINVOKEMETHODTYPE_PTYPES(vmThread, methodType);
			U_32 typeCount = J9INDEXABLEOBJECT_SIZE(vmThread, argTypesObject);

			for (UDATA i = 0; i < typeCount; i++) {
				J9Class *paramClass = J9VM_J9CLASS_FROM_HEAPCLASS(vmThread, J9JAVAARRAYOFOBJECT_LOAD(vmThread, argTypesObject, i));

				if (J9ROMCLASS_IS_ARRAY(paramClass->romClass)) {
					paramClass = ((J9ArrayClass *)paramClass)->leafComponentType;
				}

				visibilityReturnCode = checkVisibility(vmThread, senderClass, paramClass, paramClass->romClass->modifiers, lookupOptions);

				if (J9_VISIBILITY_ALLOWED != visibilityReturnCode) {
					illegalClass = paramClass;
					break;
				}
			}
		}
		if (NULL != illegalClass) {
			if (throwException) {
				char *errorMsg = illegalAccessMessage(vmThread, illegalClass->romClass->modifiers, senderClass, illegalClass, visibilityReturnCode);
				if (NULL == errorMsg) {
					setNativeOutOfMemoryError(vmThread, 0, 0);
				} else {
					setCurrentExceptionUTF(vmThread, J9VMCONSTANTPOOL_JAVALANGILLEGALACCESSERROR, errorMsg);
				}
			}
			Trc_VM_sendResolveMethodTypeRefInto_Exception(vmThread, senderClass, illegalClass, visibilityReturnCode);
			methodType = NULL;
		}
	}

	/* Only write the value in if its not null */
	if ((NULL != methodType) && J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_NO_CP_UPDATE)) {
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
resolveMethodHandleRefInto(J9VMThread *vmThread, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags, J9RAMMethodHandleRef *ramCPEntry)
{
	J9ROMMethodHandleRef *romMethodHandleRef;
	U_32 fieldOrMethodIndex;
	J9ROMMethodRef *cpItem;
	J9Class *definingClass;
	J9ROMNameAndSignature* nameAndSig;
	j9object_t methodHandle = NULL;
	bool jitCompileTimeResolve = J9_ARE_ANY_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_JIT_COMPILE_TIME);
	bool canRunJavaCode = !jitCompileTimeResolve && J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_REDEFINE_CLASS);
	bool throwException = canRunJavaCode && J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_NO_THROW_ON_FAIL);

	/* Check if already resolved */
	if (ramCPEntry->methodHandle != NULL) {
		return ramCPEntry->methodHandle;
	}

	/* Return NULL if not allowed to run java code */
	if (!canRunJavaCode) {
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
		if (resolveStaticFieldRefInto(vmThread, NULL, ramCP, fieldOrMethodIndex, resolveFlags, NULL, NULL) == NULL) {
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
		if (NULL == resolvedClass) {
			goto _done;
		}

		/* Assumes that if this is MethodHandle, the class was successfully resolved but the method was not */
		if (resolvedClass == J9VMJAVALANGINVOKEMETHODHANDLE(vmThread->javaVM)) {
			/* This is MethodHandle - confirm name is either invokeExact or invoke */
			J9ROMNameAndSignature* nameAndSig = J9ROMMETHODREF_NAMEANDSIGNATURE(romMethodRef);
			J9UTF8 *nameUTF = J9ROMNAMEANDSIGNATURE_NAME(nameAndSig);
			if (J9UTF8_LITERAL_EQUALS(J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF), "invokeExact")
			|| J9UTF8_LITERAL_EQUALS(J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF), "invoke")
			) {
				/* valid - must be resolvable */
				break;
			}
		}
#if defined(J9VM_OPT_OPENJDK_METHODHANDLE) && (JAVA_SPEC_VERSION >= 11)
		if (resolvedClass == J9VMJAVALANGINVOKEVARHANDLE(vmThread->javaVM)) {
			J9ROMNameAndSignature* nameAndSig = J9ROMMETHODREF_NAMEANDSIGNATURE(romMethodRef);
			J9UTF8 *nameUTF = J9ROMNAMEANDSIGNATURE_NAME(nameAndSig);
			/* Check if name is a polymorphic VarHandle Method */
			if (VM_VMHelpers::isPolymorphicVarHandleMethod(J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF))) {
				/* valid - must be resolvable */
				break;
			}
		}
#endif /* defined(J9VM_OPT_OPENJDK_METHODHANDLE) && (JAVA_SPEC_VERSION >= 11) */

		if (resolveVirtualMethodRef(vmThread, ramCP, fieldOrMethodIndex, resolveFlags, NULL) == 0) {
			/* private methods don't end up in the vtable - we need to determine if invokeSpecial is
			 * appropriate here.
			 */
			J9RAMSpecialMethodRef ramSpecialMethodRef;

			/* Clear the exception and let the resolveSpecialMethodRef set an exception if necessary */
			VM_VMHelpers::clearException(vmThread);

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
		if (resolveStaticMethodRefInto(vmThread, ramCP, fieldOrMethodIndex, resolveFlags, NULL) == NULL) {
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
		if (throwException) {
			setCurrentExceptionUTF(vmThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
		}
		goto _done;
	}
	nameAndSig = J9ROMMETHODREF_NAMEANDSIGNATURE(cpItem);

	sendResolveMethodHandle(vmThread, cpIndex, ramCP, definingClass, nameAndSig);
	methodHandle = (j9object_t) vmThread->returnValue;

	/* check if an exception is already pending */
	if (threadEventsPending(vmThread)) {
		/* Already a pending exception */
		methodHandle = NULL;
	} else if (methodHandle == NULL) {
		if (throwException) {
			setCurrentExceptionUTF(vmThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
		}
	}

	/* Only write the value in if its not null */
	if (NULL != methodHandle) {
		methodHandle = vmThread->javaVM->memoryManagerFunctions->j9gc_objaccess_asConstantPoolObject(
																		vmThread, 
																		methodHandle, 
																		J9_GC_ALLOCATE_OBJECT_TENURED | J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE | J9_GC_ALLOCATE_OBJECT_HASHED);
		if (NULL == methodHandle) {
			if (throwException) {
				setHeapOutOfMemoryError(vmThread);
			}
		} else if ((NULL != ramCPEntry) && J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_NO_CP_UPDATE)) {
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
resolveOpenJDKInvokeHandle(J9VMThread *vmThread, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags)
{
#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
	bool canRunJavaCode = J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_JIT_COMPILE_TIME | J9_RESOLVE_FLAG_REDEFINE_CLASS);
	J9RAMMethodRef *ramCPEntry = (J9RAMMethodRef*)ramCP + cpIndex;
	UDATA invokeCacheIndex = ramCPEntry->methodIndexAndArgCount >> 8;
	J9Class *ramClass = J9_CLASS_FROM_CP(ramCP);
	j9object_t *invokeCache = ramClass->invokeCache + invokeCacheIndex;
	j9object_t result = *invokeCache;

	Trc_VM_resolveInvokeHandle_Entry(vmThread, ramCP, cpIndex, resolveFlags);

	Assert_VM_true(J9_ARE_NO_BITS_SET(resolveFlags, J9_RESOLVE_FLAG_NO_CP_UPDATE));

	if ((NULL == result) && canRunJavaCode) {
		J9ROMMethodRef *romMethodRef = (J9ROMMethodRef *)&ramCP->romConstantPool[cpIndex];
		J9ROMNameAndSignature *nameAndSig = J9ROMMETHODREF_NAMEANDSIGNATURE(romMethodRef);

		/* Resolve the class. */
		J9Class *resolvedClass = resolveClassRef(vmThread, ramCP, romMethodRef->classRefCPIndex, resolveFlags);
		if (resolvedClass != NULL) {
			sendResolveOpenJDKInvokeHandle(vmThread, ramCP, cpIndex, MH_REF_INVOKEVIRTUAL, resolvedClass, nameAndSig);
			result = (j9object_t)vmThread->returnValue;

			if (vmThread->currentException != NULL) {
				/* Already a pending exception */
				result = NULL;
			} else if (result == NULL) {
				setCurrentExceptionUTF(vmThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
			} else {
				/* Only write the value in if it is not null */
				J9MemoryManagerFunctions *gcFuncs = vmThread->javaVM->memoryManagerFunctions;

				/* Ensure that result array elements are written before the array reference is stored */
				VM_AtomicSupport::writeBarrier();
				if (0 == gcFuncs->j9gc_objaccess_staticCompareAndSwapObject(vmThread, ramClass, invokeCache, NULL, result)) {
					/* Another thread beat this thread to updating the call site, ensure both threads return the same method handle. */
					result = *invokeCache;
				}
			}
		}
	}
	Trc_VM_resolveInvokeHandle_Exit(vmThread, result);
	return result;
#else /* defined(J9VM_OPT_OPENJDK_METHODHANDLE) */
	Trc_VM_Assert_ShouldNeverHappen();
	return NULL;
#endif /* defined(J9VM_OPT_OPENJDK_METHODHANDLE) */
}

j9object_t
resolveConstantDynamic(J9VMThread *vmThread, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags)
{
	j9object_t value = NULL;
#if JAVA_SPEC_VERSION >= 11
	Assert_VM_true(J9_RESOLVE_FLAG_RUNTIME_RESOLVE == resolveFlags);
	J9RAMConstantDynamicRef *ramCPEntry = (J9RAMConstantDynamicRef*)ramCP + cpIndex;
	J9JavaVM *vm = vmThread->javaVM;
	bool resolved = false;

retry:
	value = ramCPEntry->value;
	/* Fast path resolution for previously resolved entries */
	if (NULL != value) {
		resolved = true;
	} else {
		j9object_t cpException = ramCPEntry->exception;
		/* check if already resolved to NULL or exception */
		if (NULL != cpException) {
			J9Class *throwable = J9VMJAVALANGTHROWABLE_OR_NULL(vm);
			J9Class *clazz = J9OBJECT_CLAZZ(vmThread, cpException);
			if (cpException == vm->voidReflectClass->classObject) {
				/* Resolved with null reference */
				resolved = true;
			} else if (isSameOrSuperClassOf(throwable, clazz)) {
				/* Resolved with exception */
				VM_VMHelpers::setExceptionPending(vmThread, cpException);
				resolved = true;
			}
		}
	}

	if (!resolved) {
		/* Slow path, attempt to acquire resolve mutex or retry, or wait */
		omrthread_monitor_enter(vm->constantDynamicMutex);
		value = ramCPEntry->value;
		if (NULL != value) {
			omrthread_monitor_exit(vm->constantDynamicMutex);
			goto retry;
		} else {
			j9object_t cpException = ramCPEntry->exception;
			/* check if already resolved to NULL or exception */
			if (NULL != cpException) {
				J9Class *throwable = J9VMJAVALANGTHROWABLE_OR_NULL(vm);
				J9Class *clazz = J9OBJECT_CLAZZ(vmThread, cpException);
				if (cpException == vm->voidReflectClass->classObject) {
					/* Resolved with null reference */
					omrthread_monitor_exit(vm->constantDynamicMutex);
					goto retry;
				} else if (isSameOrSuperClassOf(throwable, clazz)) {
					/* Resolved with exception */
					omrthread_monitor_exit(vm->constantDynamicMutex);
					goto retry;
				} else if (cpException != vmThread->threadObject) {
					/* Another thread is currently resolving this entry */
					internalReleaseVMAccess(vmThread);
					omrthread_monitor_wait(vm->constantDynamicMutex);
					omrthread_monitor_exit(vm->constantDynamicMutex);
					internalAcquireVMAccess(vmThread);
					goto retry;
				}
			}
		}

		J9Class *ramClass = J9_CLASS_FROM_CP(ramCP);

		/* Write threadObject in exception slot to indicate current thread is performing the resolution
		 * this is the equivalent of: ramCPEntry->exception = vmThread->threadObject;
		 */
		J9STATIC_OBJECT_STORE(vmThread, ramClass, &(ramCPEntry->exception), vmThread->threadObject);
		omrthread_monitor_exit(vm->constantDynamicMutex);

		/* Enter if not previously resolved */
		J9ROMClass *romClass = ramClass->romClass;
		J9ROMConstantDynamicRef *romConstantRef = (J9ROMConstantDynamicRef*)(J9_ROM_CP_FROM_CP(ramCP) + cpIndex);
		J9SRP *callSiteData = (J9SRP *) J9ROMCLASS_CALLSITEDATA(romClass);
		U_16 *bsmIndices = (U_16 *) (callSiteData + romClass->callSiteCount);
		U_16 *bsmData = bsmIndices + romClass->callSiteCount;

		/* clear the J9DescriptionCpPrimitiveType flag with mask to get bsmIndex */
		U_32 bsmIndex = (romConstantRef->bsmIndexAndCpType >> J9DescriptionCpTypeShift) & J9DescriptionCpBsmIndexMask;
		J9ROMNameAndSignature* nameAndSig = SRP_PTR_GET(&romConstantRef->nameAndSignature, J9ROMNameAndSignature*);

		/* Walk bsmData - skip all bootstrap methods before bsmIndex */
		for (U_32 i = 0; i < bsmIndex; i++) {
			/* increment by size of bsm data plus header */
			bsmData += (bsmData[1] + 2);
		}

		/* Invoke BootStrap method*/
		sendResolveConstantDynamic(vmThread, ramCP, cpIndex, nameAndSig, bsmData);
		value = (j9object_t)vmThread->returnValue;

		/* Check if entry resolved by nested constantDynamic calls */
		if (ramCPEntry->exception != vmThread->threadObject) {
			goto retry;
		}

		j9object_t exceptionObject = NULL;
		if (NULL != vmThread->currentException) {
			exceptionObject = vmThread->currentException;
		} else if (NULL == value) {
			/* Java.lang.void is used as a special flag to indicate null reference */
			exceptionObject = vm->voidReflectClass->classObject;
		}

		omrthread_monitor_enter(vm->constantDynamicMutex);
		/* Write the value and exceptionObject into the cp entry using
		 * the store barriers so that the GC is notified of the stores.
		 * This is the equivalent of doing:
		 * 	ramCPEntry->value = value;
		 *	ramCPEntry->exception = exceptionObject;
		 */
		J9STATIC_OBJECT_STORE(vmThread, ramClass, &(ramCPEntry->value), value);
		J9STATIC_OBJECT_STORE(vmThread, ramClass, &(ramCPEntry->exception), exceptionObject);
		omrthread_monitor_notify_all(vm->constantDynamicMutex);
		omrthread_monitor_exit(vm->constantDynamicMutex);
	}
#else /* JAVA_SPEC_VERSION >= 11 */
	Trc_VM_Assert_ShouldNeverHappen();
#endif /* JAVA_SPEC_VERSION >= 11 */
	return value;
}

j9object_t
resolveInvokeDynamic(J9VMThread *vmThread, J9ConstantPool *ramCP, UDATA callSiteIndex, UDATA resolveFlags)
{
	Assert_VM_true(J9_RESOLVE_FLAG_RUNTIME_RESOLVE == resolveFlags);
	j9object_t *callSite = ramCP->ramClass->callSites + callSiteIndex;
	j9object_t result = *callSite;

	J9ROMClass *romClass = ramCP->ramClass->romClass;
	J9SRP *callSiteData = (J9SRP *) J9ROMCLASS_CALLSITEDATA(romClass);
	U_16 *bsmIndices = (U_16 *) (callSiteData + romClass->callSiteCount);
	U_16 *bsmData = bsmIndices + romClass->callSiteCount;
	J9ROMNameAndSignature* nameAndSig = SRP_PTR_GET(callSiteData + callSiteIndex, J9ROMNameAndSignature*);
	U_16 bsmIndex = bsmIndices[callSiteIndex];
	U_16 i = 0;

	Trc_VM_resolveInvokeDynamic_Entry(vmThread, callSiteIndex, bsmIndex, resolveFlags);

	/* Check if already resolved */
	if (NULL == result) {
		/* Walk bsmData - skip all bootstrap methods before bsmIndex */
		for (i = 0; i < bsmIndex; i++) {
			/* increment by size of bsm data plus header */
			bsmData += bsmData[1] + 2;
		}

		sendResolveInvokeDynamic(vmThread, ramCP, callSiteIndex, nameAndSig, bsmData);
		result = (j9object_t) vmThread->returnValue;

		Trc_VM_resolveInvokeDynamic_Resolved(vmThread, callSiteIndex, result);

#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
		/* Check if an exception is already pending */
		if (vmThread->currentException != NULL) {
			/* Already a pending exception */
			result = NULL;
		} else if (NULL == result) {
			setCurrentExceptionUTF(vmThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
		} else {
			/* The result is an array. Ensure that the result and its elements are
			 * written/published before the result reference is stored.
			 */
			VM_AtomicSupport::writeBarrier();

			J9MemoryManagerFunctions *gcFuncs = vmThread->javaVM->memoryManagerFunctions;
			J9Class *j9class = J9_CLASS_FROM_CP(ramCP);
			if (0 == gcFuncs->j9gc_objaccess_staticCompareAndSwapObject(vmThread, j9class, callSite, NULL, result)) {
				/* Another thread beat this thread to updating the call site, ensure both threads
				 * return the same method handle.
				 */
				result = *callSite;
			}
		}
#else /* defined(J9VM_OPT_OPENJDK_METHODHANDLE) */
		/* Check if an exception is already pending */
		if (vmThread->currentException != NULL) {
			/* Already a pending exception */
			result = NULL;
		} else if (result == NULL) {
			setCurrentExceptionUTF(vmThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
		} else { /* (result != NULL) */
			/* Only write the value in if it is not null */
			J9MemoryManagerFunctions *gcFuncs = vmThread->javaVM->memoryManagerFunctions;
			result = gcFuncs->j9gc_objaccess_asConstantPoolObject(
										vmThread,
										result,
										J9_GC_ALLOCATE_OBJECT_TENURED | J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE | J9_GC_ALLOCATE_OBJECT_HASHED);

			if (NULL == result) {
				setHeapOutOfMemoryError(vmThread);
			} else {
				J9Class *j9class = J9_CLASS_FROM_CP(ramCP);
				if (0 == gcFuncs->j9gc_objaccess_staticCompareAndSwapObject(vmThread, j9class, callSite, NULL, result)) {
					/* Another thread beat this thread to updating the call site, ensure both threads return the same method handle. */
					result = *callSite;
				}
			}
		}
#endif /* defined(J9VM_OPT_OPENJDK_METHODHANDLE) */
	}
	Trc_VM_resolveInvokeDynamic_Exit(vmThread, result);
	return result;
}

#if JAVA_SPEC_VERSION >= 16
/**
 * @brief The function calls into the interpreter via sendResolveFfiCallInvokeHandle()
 * to fetch the MemberName object plus appendix intended for the downcall/upcall method.
 *
 * @param vmThread the pointer to J9VMThread
 * @param handle the downcall/upcall MethodHandle object
 * @return the cache array that stores MemberName and appendix
 */
j9object_t
resolveFfiCallInvokeHandle(J9VMThread *vmThread, j9object_t handle)
{
	j9object_t invokeCache = NULL;
	Trc_VM_resolveFfiCallInvokeHandle_Entry(vmThread);

	sendResolveFfiCallInvokeHandle(vmThread, handle);
	invokeCache = (j9object_t)vmThread->returnValue;
	if (VM_VMHelpers::exceptionPending(vmThread)) {
		/* Already a pending exception */
		invokeCache = NULL;
	} else if (NULL == invokeCache) {
		setCurrentExceptionUTF(vmThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	}

	Trc_VM_resolveFfiCallInvokeHandle_Exit(vmThread, invokeCache);
	return invokeCache;
}
#endif /* JAVA_SPEC_VERSION >= 16 */
