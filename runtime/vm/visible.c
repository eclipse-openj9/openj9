/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
#include "j2sever.h"
#include "j9.h"
#include "j9cp.h"
#include "../util/ut_module.h"
#undef UT_MODULE_LOADED
#undef UT_MODULE_UNLOADED
#include "ut_j9vm.h"
#include "util_api.h"
#include "vm_internal.h"

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
	if (J9ROMCLASS_IS_UNSAFE(sourceClass->romClass) == 0) {
		if ( modifiers & J9AccPublic ) {
			/* Public */
			if ((sourceClass != destClass) 
				&& (J2SE_VERSION(vm) >= J2SE_19) 
				&& J9_ARE_ALL_BITS_SET(vm->runtimeFlags, J9_RUNTIME_JAVA_BASE_MODULE_CREATED)
				&& !J9ROMCLASS_IS_PRIMITIVE_TYPE(destClass->romClass)
			) {
				j9object_t srcClassObject = sourceClass->classObject;
				j9object_t destClassObject = destClass->classObject;
				Assert_VM_true((NULL != srcClassObject) && (NULL != destClassObject));
				{
					j9object_t srcModuleObject = J9VMJAVALANGCLASS_MODULE(currentThread, srcClassObject);
					j9object_t destModuleObject = J9VMJAVALANGCLASS_MODULE(currentThread, destClassObject);
					Assert_VM_true((NULL != srcModuleObject) && (NULL != destModuleObject));
					if (srcModuleObject != destModuleObject)	{
						UDATA rc = ERRCODE_GENERAL_FAILURE;
						J9Module *srcModule = J9OBJECT_ADDRESS_LOAD(currentThread, srcModuleObject, vm->modulePointerOffset);
						J9Module *destModule = J9OBJECT_ADDRESS_LOAD(currentThread, destModuleObject, vm->modulePointerOffset);

						if (srcModule != destModule) {
							if (!J9_ARE_ALL_BITS_SET(lookupOptions, J9_LOOK_REFLECT_CALL)) {
								if (!isAllowedReadAccessToModule(currentThread, srcModule, destModule, &rc)) {
									Trc_VM_checkVisibility_failed_with_errortype(currentThread,
											sourceClass, J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(sourceClass->romClass)), J9UTF8_DATA(J9ROMCLASS_CLASSNAME(sourceClass->romClass)),
											destClass, J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(destClass->romClass)), J9UTF8_DATA(J9ROMCLASS_CLASSNAME(destClass->romClass)), "read access not allowed");
									result = J9_VISIBILITY_MODULE_READ_ACCESS_ERROR;
								}
							}

							if (J9_VISIBILITY_ALLOWED == result) {
								const U_8* packageName = NULL;
								UDATA packageNameLength = 0;
								J9PackageIDTableEntry entry = {0};

								entry.taggedROMClass = destClass->packageID;
								packageName = getPackageName(&entry, &packageNameLength);

								omrthread_monitor_enter(vm->classLoaderModuleAndLocationMutex);
								if (!isPackageExportedToModuleWithName(currentThread, destModule, (U_8*) packageName, (U_16) packageNameLength, srcModule, isModuleUnnamed(currentThread, srcModuleObject), &rc)) {
									Trc_VM_checkVisibility_failed_with_errortype(currentThread,
											sourceClass, J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(sourceClass->romClass)), J9UTF8_DATA(J9ROMCLASS_CLASSNAME(sourceClass->romClass)),
											destClass, J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(destClass->romClass)), J9UTF8_DATA(J9ROMCLASS_CLASSNAME(destClass->romClass)), "package not exported");
									result = J9_VISIBILITY_MODULE_PACKAGE_EXPORT_ERROR;
								}
								omrthread_monitor_exit(vm->classLoaderModuleAndLocationMutex);
							}
						}
					}
				}
			}
		} else if (modifiers & J9AccPrivate) {
			/* Private */
			if ((sourceClass != destClass)
#if defined(J9VM_OPT_VALHALLA_NESTMATES)
					&& (sourceClass->memberOfNest != destClass->memberOfNest)
#endif /* defined(J9VM_OPT_VALHALLA_NESTMATES) */
			) {
				result = J9_VISIBILITY_NON_MODULE_ACCESS_ERROR;
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
