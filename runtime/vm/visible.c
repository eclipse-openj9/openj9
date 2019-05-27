/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
#include "j2sever.h"
#include "j9.h"
#include "j9cp.h"
#include "../util/ut_module.h"
#undef UT_MODULE_LOADED
#undef UT_MODULE_UNLOADED
#include "ut_j9vm.h"
#include "util_api.h"
#include "vm_internal.h"
#include "j9protos.h"
#include "j9vmnls.h"

IDATA
checkModuleAccess(J9VMThread *currentThread, J9JavaVM* vm, J9ROMClass* srcRomClass, J9Module* srcModule, J9ROMClass* destRomClass, J9Module* destModule, UDATA destPackageID, UDATA lookupOptions)
{
	UDATA result = J9_VISIBILITY_ALLOWED;
	if (srcModule != destModule) {
		UDATA rc = ERRCODE_GENERAL_FAILURE;
		if (!J9_ARE_ALL_BITS_SET(lookupOptions, J9_LOOK_REFLECT_CALL)) {
			if (!isAllowedReadAccessToModule(currentThread, srcModule, destModule, &rc)) {
				Trc_VM_checkVisibility_failed_with_errortype_romclass(currentThread,
						srcRomClass, J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(srcRomClass)), J9UTF8_DATA(J9ROMCLASS_CLASSNAME(srcRomClass)), srcModule,
						destRomClass, J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(destRomClass)), J9UTF8_DATA(J9ROMCLASS_CLASSNAME(destRomClass)), destModule, rc, "read access not allowed");
				result = J9_VISIBILITY_MODULE_READ_ACCESS_ERROR;
			}
		}

		if (J9_VISIBILITY_ALLOWED == result) {
			const U_8* packageName = NULL;
			UDATA packageNameLength = 0;
			J9PackageIDTableEntry entry = {0};

			entry.taggedROMClass = destPackageID;
			packageName = getPackageName(&entry, &packageNameLength);

			omrthread_monitor_enter(vm->classLoaderModuleAndLocationMutex);
			if (!isPackageExportedToModuleWithName(currentThread, destModule, (U_8*) packageName, (U_16) packageNameLength, srcModule, J9_IS_J9MODULE_UNNAMED(vm, srcModule), &rc)) {
				Trc_VM_checkVisibility_failed_with_errortype_romclass(currentThread,
						srcRomClass, J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(srcRomClass)), J9UTF8_DATA(J9ROMCLASS_CLASSNAME(srcRomClass)), srcModule,
						destRomClass, J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(destRomClass)), J9UTF8_DATA(J9ROMCLASS_CLASSNAME(destRomClass)), destModule, rc, "package not exported");
				result = J9_VISIBILITY_MODULE_PACKAGE_EXPORT_ERROR;
			}
			omrthread_monitor_exit(vm->classLoaderModuleAndLocationMutex);
		}

	}
	return result;
}


/**
 * Check visibility from sourceClass to destClass with modifiers specified
 *
 * @param[in] currentThread the current J9VMThread
 * @param[in] sourceClass the accessing class
 * @param[in] destClass: the accessed class
 * @param[in] modifiers the modifier
 * @param[in] lookupOptions J9_LOOK* options
 *
 * @return 	J9_VISIBILITY_ALLOWED if the access is allowed,
 * 			J9_VISIBILITY_NON_MODULE_ACCESS_ERROR if the access is NOT allowed due to non-module access error,
 * 			J9_VISIBILITY_MODULE_READ_ACCESS_ERROR if module read access error occurred,
 * 			J9_VISIBILITY_MODULE_PACKAGE_EXPORT_ERROR if module package access error
 */
IDATA
checkVisibility(J9VMThread *currentThread, J9Class* sourceClass, J9Class* destClass, UDATA modifiers, UDATA lookupOptions)
{
	UDATA result = J9_VISIBILITY_ALLOWED;
	J9JavaVM * const vm = currentThread->javaVM;

	Trc_VM_checkVisibility_Entry(currentThread, sourceClass, destClass, modifiers);
	sourceClass = J9_CURRENT_CLASS(sourceClass);
	destClass = J9_CURRENT_CLASS(destClass);
#ifdef DEBUG
	printf("checkModVis: [%*s] [%*s]\n",
				J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(sourceClass->romClass)), J9UTF8_DATA(J9ROMCLASS_CLASSNAME(sourceClass->romClass)),
				J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(destClass->romClass)), J9UTF8_DATA(J9ROMCLASS_CLASSNAME(destClass->romClass)));
#endif
	if (!J9CLASS_IS_EXEMPT_FROM_VALIDATION(sourceClass)) {
		if ( modifiers & J9AccPublic ) {
			/* Public */
			if ((sourceClass != destClass)
				&& (J2SE_VERSION(vm) >= J2SE_V11) 
				&& J9_ARE_ALL_BITS_SET(vm->runtimeFlags, J9_RUNTIME_JAVA_BASE_MODULE_CREATED)
				&& !J9ROMCLASS_IS_PRIMITIVE_TYPE(destClass->romClass)
			) {
				J9Module *srcModule = sourceClass->module;
				J9Module *destModule = destClass->module;

				result = checkModuleAccess(currentThread, vm, sourceClass->romClass, srcModule, destClass->romClass, destModule, destClass->packageID, lookupOptions );
			}
		} else if (modifiers & J9AccPrivate) {
			/* Private */
			if (sourceClass != destClass) {
#if defined(J9VM_OPT_VALHALLA_NESTMATES)
				/* loadAndVerifyNestHost returns an error code if setting the
				 * nest host field fails.
				 *
				 * Note that the specification asserts an order for nest host
				 * loading during access checking. The accessing class's nest
				 * host is loaded first and the class being accessed loads its
				 * nest host after.
				 */
				if (NULL == sourceClass->nestHost) {
					result = loadAndVerifyNestHost(currentThread, sourceClass, lookupOptions);
					if (J9_VISIBILITY_ALLOWED != result) {
						goto _exit;
					}
					sourceClass = J9_CURRENT_CLASS(sourceClass);
					destClass = J9_CURRENT_CLASS(destClass);
				}
				if (NULL == destClass->nestHost) {
					result = loadAndVerifyNestHost(currentThread, destClass, lookupOptions);
					if (J9_VISIBILITY_ALLOWED != result) {
						goto _exit;
					}
					sourceClass = J9_CURRENT_CLASS(sourceClass);
					destClass = J9_CURRENT_CLASS(destClass);
				}

				if (sourceClass->nestHost != destClass->nestHost) {
#endif /* defined(J9VM_OPT_VALHALLA_NESTMATES) */
					result = J9_VISIBILITY_NON_MODULE_ACCESS_ERROR;
#if defined(J9VM_OPT_VALHALLA_NESTMATES)
				}
#endif /* defined(J9VM_OPT_VALHALLA_NESTMATES) */
			}
		} else if (modifiers & J9AccProtected) {
			/* Protected */
			if (sourceClass->packageID != destClass->packageID) {
				if (J9_ARE_ANY_BITS_SET(sourceClass->romClass->modifiers, J9AccInterface)) {
					/* Interfaces are types, not classes, and do not have protected access to 
					 * their 'superclass' (java.lang.Object) 
					 */
					result = J9_VISIBILITY_NON_MODULE_ACCESS_ERROR;
				} else {
					/* not same package so sourceClass must be a subclass of destClass */
					if (!isSameOrSuperClassOf(destClass, sourceClass)) {
						result = J9_VISIBILITY_NON_MODULE_ACCESS_ERROR;
					}
				}
			}
		} else {
			/* Default (package access) */
			if (sourceClass->packageID != destClass->packageID) {
				result = J9_VISIBILITY_NON_MODULE_ACCESS_ERROR;
			}
		}
	}

#if defined(J9VM_OPT_VALHALLA_NESTMATES)
_exit:
#endif /* defined(J9VM_OPT_VALHALLA_NESTMATES) */
	if (J9_VISIBILITY_NON_MODULE_ACCESS_ERROR == result) {
		/* "checkVisibility from %p (%.*s) to %p (%.*s) modifiers=%zx failed" */
		Trc_VM_checkVisibility_Failed(currentThread,
			sourceClass, J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(sourceClass->romClass)), J9UTF8_DATA(J9ROMCLASS_CLASSNAME(sourceClass->romClass)),
			destClass, J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(destClass->romClass)), J9UTF8_DATA(J9ROMCLASS_CLASSNAME(destClass->romClass)),
			modifiers);
	}

	Trc_VM_checkVisibility_Exit(currentThread, result);

	return result;
}

#if defined(J9VM_OPT_VALHALLA_NESTMATES)

/**
 * Sets the nestmates error based on the errorCode
 *
 * @param vmThread vmthread token
 * @param nestMember the j9lass requesting the nesthost
 * @param nestHost the actual nest host, this may be NULL
 * @param errorCode the error code representing the exception to throw
 * 					J9_VISIBILITY_NEST_HOST_LOADING_FAILURE_ERROR
 * 					J9_VISIBILITY_NEST_HOST_DIFFERENT_PACKAGE_ERROR
 * 					J9_VISIBILITY_NEST_MEMBER_NOT_CLAIMED_ERROR
 */
void
setNestmatesError(J9VMThread *vmThread, J9Class *nestMember, J9Class *nestHost, IDATA errorCode)
{
	PORT_ACCESS_FROM_VMC(vmThread);
	J9UTF8 *nestMemberName = J9ROMCLASS_CLASSNAME(nestMember->romClass);
	J9UTF8 *nestHostName = NULL;
	UDATA exception = J9VMCONSTANTPOOL_JAVALANGINCOMPATIBLECLASSCHANGEERROR;
	const char *nlsTemplate = NULL;
	char *msg = NULL;

	if (NULL == nestHost) {
		nestHostName = J9ROMCLASS_NESTHOSTNAME(nestMember->romClass);
	} else {
		nestHostName = J9ROMCLASS_CLASSNAME(nestHost->romClass);
	}

	switch (errorCode) {
	case J9_VISIBILITY_NEST_HOST_LOADING_FAILURE_ERROR:
		exception = J9VMCONSTANTPOOL_JAVALANGNOCLASSDEFFOUNDERROR;
		nlsTemplate = j9nls_lookup_message(
				J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
				J9NLS_VM_NESTMATES_CLASS_FAILED_TO_LOAD,
				NULL);
		break;
	case J9_VISIBILITY_NEST_HOST_DIFFERENT_PACKAGE_ERROR:
		nlsTemplate = j9nls_lookup_message(
				J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
				J9NLS_VM_NEST_HOST_HAS_DIFFERENT_PACKAGE,
				NULL);
		break;
	case J9_VISIBILITY_NEST_MEMBER_NOT_CLAIMED_ERROR:
		nlsTemplate = j9nls_lookup_message(
				J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
				J9NLS_VM_NEST_MEMBER_NOT_CLAIMED_BY_NEST_HOST,
				NULL);
		break;
	default:
		Assert_VM_unreachable();
	}

	if (NULL != nlsTemplate) {
		UDATA msgLen = j9str_printf(PORTLIB, NULL, 0, nlsTemplate,
				J9UTF8_LENGTH(nestMemberName), J9UTF8_DATA(nestMemberName),
				J9UTF8_LENGTH(nestHostName), J9UTF8_DATA(nestHostName),
				J9UTF8_LENGTH(nestMemberName), J9UTF8_DATA(nestMemberName));

		msg = (char *)j9mem_allocate_memory(msgLen, OMRMEM_CATEGORY_VM);

		/* If msg is NULL we can still throw the exception without it. */
		if (NULL != msg) {
			j9str_printf(PORTLIB, msg, msgLen, nlsTemplate,
					J9UTF8_LENGTH(nestMemberName), J9UTF8_DATA(nestMemberName),
					J9UTF8_LENGTH(nestHostName), J9UTF8_DATA(nestHostName),
					J9UTF8_LENGTH(nestMemberName), J9UTF8_DATA(nestMemberName));
		}
	}

	setCurrentExceptionUTF(vmThread, exception, msg);
	j9mem_free_memory(msg);
}

/*
 * Loads and verifies the nesthost
 *
 * @param vmThread vmthread token
 * @param clazz the j9lass requesting the nesthost
 * @param options loading options (e.g. J9_LOOK_NO_THROW)
 * @return 	J9_VISIBILITY_ALLOWED if nest host is loaded, otherwise
 * 			J9_VISIBILITY_NEST_HOST_LOADING_FAILURE_ERROR
 * 			J9_VISIBILITY_NEST_HOST_DIFFERENT_PACKAGE_ERROR
 * 			J9_VISIBILITY_NEST_MEMBER_NOT_CLAIMED_ERROR
 */
UDATA
loadAndVerifyNestHost(J9VMThread *vmThread, J9Class *clazz, UDATA options)
{
	UDATA result = J9_VISIBILITY_ALLOWED;
	if (NULL == clazz->nestHost) {
		J9Class *nestHost = NULL;
		J9ROMClass *romClass = clazz->romClass;
		J9UTF8 *nestHostName = J9ROMCLASS_NESTHOSTNAME(romClass);
		BOOLEAN canRunJavaCode = J9_ARE_NO_BITS_SET(options, J9_LOOK_NO_JAVA);
		BOOLEAN throwException = canRunJavaCode && J9_ARE_NO_BITS_SET(options, J9_LOOK_NO_THROW);
		/* If no nest host is named, class is own nest host */
		if (NULL == nestHostName) {
			nestHost = clazz;
		} else {
			UDATA classLoadingFlags = 0;
			if (canRunJavaCode) {
				if (!throwException) {
					classLoadingFlags = J9_FINDCLASS_FLAG_THROW_ON_FAIL;
				}
			} else {
				classLoadingFlags = J9_FINDCLASS_FLAG_EXISTING_ONLY;
			}

			nestHost = internalFindClassUTF8(vmThread, J9UTF8_DATA(nestHostName), J9UTF8_LENGTH(nestHostName), clazz->classLoader, classLoadingFlags);

			/* Nest host must be successfully loaded by the same classloader in the same package & verify the nest member */
			if (NULL == nestHost) {
				result = J9_VISIBILITY_NEST_HOST_LOADING_FAILURE_ERROR;
			} else if (clazz->packageID != nestHost->packageID) {
				result = J9_VISIBILITY_NEST_HOST_DIFFERENT_PACKAGE_ERROR;
			} else {
				/* The nest host must have a nestmembers attribute that claims this class. */
				J9UTF8 *className = J9ROMCLASS_CLASSNAME(romClass);
				J9SRP *nestMembers = J9ROMCLASS_NESTMEMBERS(nestHost->romClass);
				U_16 nestMemberCount = nestHost->romClass->nestMemberCount;
				U_16 i = 0;

				result = J9_VISIBILITY_NEST_MEMBER_NOT_CLAIMED_ERROR;
				for (i = 0; i < nestMemberCount; i++) {
					J9UTF8 *nestMemberName = NNSRP_GET(nestMembers[i], J9UTF8*);
					if (J9UTF8_EQUALS(className, nestMemberName)) {
						result = J9_VISIBILITY_ALLOWED;
						break;
					}
				}
			}
		}

		/* If a problem occurred in nest host verification then the nest host value is invalid */
		if (J9_VISIBILITY_ALLOWED == result) {
			clazz->nestHost = nestHost;
		} else if (throwException) {
			/* Only set an exception is there isn't only already pending (and pop frame is not requested) */
			if (J9_ARE_NO_BITS_SET(vmThread->publicFlags, J9_PUBLIC_FLAGS_POP_FRAMES_INTERRUPT)
			&& (NULL == vmThread->currentException)
			) {
				setNestmatesError(vmThread, clazz, nestHost, result);
			}
		} else {
			/* Asked not to throw - clear any pending exception */
			vmThread->currentException = NULL;
			vmThread->privateFlags &= ~(UDATA)J9_PRIVATE_FLAGS_REPORT_EXCEPTION_THROW;
		}
	}

	return result;
}
#endif /* defined(J9VM_OPT_VALHALLA_NESTMATES) */
