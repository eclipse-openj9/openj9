/*******************************************************************************
 * Copyright (c) 1998, 2021 IBM Corp. and others
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
#include "jclexception.h"
#include "jvminit.h"
#include "objhelp.h"
#include "omrgcconsts.h"
#include "rommeth.h"
#include "vm_api.h"
#include "ut_j9jcl.h"

#if JAVA_SPEC_VERSION >= 11
static void setStackTraceElementFields(J9VMThread *vmThread, j9object_t element, J9ClassLoader *classLoader);
#endif /* JAVA_SPEC_VERSION >= 11 */

static UDATA getStackTraceIterator(J9VMThread * vmThread, void * voidUserData, UDATA bytecodeOffset, J9ROMClass * romClass, J9ROMMethod * romMethod, J9UTF8 * fileName, UDATA lineNumber, J9ClassLoader* classLoader, J9Class* ramClass);

/**
 * Saves enough context into the StackTraceElement to allow printing later.  For
 * bootstrap classes we store a java/lang/String, for application classes a
 * ProtectionDomain.
 * @param vmThread
 * @param stackTraceElement The StackTraceElement to update.
 * @param classLoader The loader in which the ROM class is defined.
 * @param romClass The ROM class whose path is to be determined.
 * @return TRUE if the element was updated, FALSE otherwise.
 * @note Assumes VM access
 */
static BOOLEAN
setStackTraceElementSource(J9VMThread* vmThread, j9object_t stackTraceElement, J9ClassLoader* classLoader, J9ROMClass* romClass)
{
	J9InternalVMFunctions * vmFuncs = vmThread->javaVM->internalVMFunctions;
	J9UTF8* name = J9ROMCLASS_CLASSNAME(romClass);
	j9object_t element = stackTraceElement;
	j9object_t heapClass, protectionDomain;
	j9object_t string = NULL;
	U_8 *path = NULL;
	UDATA pathLen = 0;

	J9Class* clazz = vmFuncs->internalFindClassUTF8(vmThread, J9UTF8_DATA(name), J9UTF8_LENGTH(name), classLoader,	J9_FINDCLASS_FLAG_EXISTING_ONLY);
	if (NULL == clazz) {
		return FALSE;
	}

	/* For bootstrap loaders we can consult the classpath entries */
	path = getClassLocation(vmThread, clazz, &pathLen);
	if (NULL != path) {
		PUSH_OBJECT_IN_SPECIAL_FRAME(vmThread, element);
		string = vmThread->javaVM->memoryManagerFunctions->j9gc_createJavaLangString(vmThread, path, pathLen, 0);
		element = POP_OBJECT_IN_SPECIAL_FRAME(vmThread);
		if (NULL == string) {
			/* exception is pending from the call */
			return FALSE;
		}
		J9VMJAVALANGSTACKTRACEELEMENT_SET_SOURCE(vmThread, element, string);
		return TRUE;
	}

	/* For application loaders we must consult the protection domain */
	heapClass = J9VM_J9CLASS_TO_HEAPCLASS(clazz);
	protectionDomain = J9VMJAVALANGCLASS_PROTECTIONDOMAIN(vmThread, heapClass);
	J9VMJAVALANGSTACKTRACEELEMENT_SET_SOURCE(vmThread, element, protectionDomain);
	return TRUE;
}


static UDATA
getStackTraceIterator(J9VMThread * vmThread, void * voidUserData, UDATA bytecodeOffset, J9ROMClass * romClass, J9ROMMethod * romMethod, J9UTF8 * fileName, UDATA lineNumber, J9ClassLoader* classLoader, J9Class* ramClass)
{
	J9GetStackTraceUserData * userData = voidUserData;
	J9JavaVM * vm = vmThread->javaVM;
	J9InternalVMFunctions const *  vmFuncs = vm->internalVMFunctions;
	J9MemoryManagerFunctions const * mmfns = vm->memoryManagerFunctions;
	j9object_t element = NULL;
	UDATA rc = TRUE;
	const I_32 currentIndex = (I_32)userData->index;

	/* If the stack trace is larger than the array, bail */

	if (userData->index == userData->maxFrames) {
		userData->index += 1; /* Indicate error */
		return FALSE;
	}

	/* Prevent the current class from being unloaded during allocation */
	PUSH_OBJECT_IN_SPECIAL_FRAME(vmThread, (NULL == classLoader) ? NULL : classLoader->classLoaderObject);

	/* Create the new StackTraceElement and put it in the array at the correct index */

	element = mmfns->J9AllocateObject(vmThread, userData->elementClass, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
	if (element == NULL) {
		rc = FALSE;
		vmFuncs->setHeapOutOfMemoryError(vmThread);
	} else {
		j9array_t result = (j9array_t) PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 1);
		J9JAVAARRAYOFOBJECT_STORE(vmThread, result, currentIndex, element);
		userData->index += 1;

		/* If there is a valid method at this frame, fill in the information for it in the StackTraceElement */

		if (romMethod != NULL) {
			J9UTF8 const * utfClassName = J9ROMCLASS_CLASSNAME(romClass);
			J9UTF8 * utf = NULL;
			j9object_t string = NULL;
			UDATA j2seVersion = J2SE_VERSION(vm) & J2SE_VERSION_MASK;

			PUSH_OBJECT_IN_SPECIAL_FRAME(vmThread, element);

			/* Lookup the J9Class for this method if it can be found as it makes
			 * a number of the remaining operations faster.  Code still needs to be
			 * able to handle the case where the J9Class cannot be found
			 */
			if (NULL != classLoader) {
				if (NULL == ramClass) {
					ramClass = vmFuncs->peekClassHashTable(vmThread, classLoader, J9UTF8_DATA(utfClassName), J9UTF8_LENGTH(utfClassName));
				}
				if (NULL != ramClass) {
					/* ramClass can never be an array here as arrays can't define methods so we don't need to
					* take them into account in the code below when writing the interned string back to
					* the Class object.
					*/
					Assert_JCL_false(J9CLASS_IS_ARRAY(ramClass));
				}
			}

			/* Fill in module name and version */
			if (j2seVersion >= J2SE_V11) {
				J9Module *module = NULL;
				if (classLoader != NULL) {
					j9object_t classLoaderName = J9VMJAVALANGCLASSLOADER_CLASSLOADERNAME(vmThread, classLoader->classLoaderObject);
					J9VMJAVALANGSTACKTRACEELEMENT_SET_CLASSLOADERNAME(vmThread, element, classLoaderName);
				}
				if (J9_ARE_NO_BITS_SET(vm->runtimeFlags, J9_RUNTIME_JAVA_BASE_MODULE_CREATED)) {
					string = mmfns->j9gc_createJavaLangString(vmThread, (U_8 *)JAVA_BASE_MODULE, LITERAL_STRLEN(JAVA_BASE_MODULE), J9_STR_XLAT);
					if (string == NULL) {
						rc = FALSE;
						/* exception is pending from the call */
						goto done;
					}
					element = PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 0);
					J9VMJAVALANGSTACKTRACEELEMENT_SET_MODULENAME(vmThread, element, string);

					string = mmfns->j9gc_createJavaLangString(vmThread, (U_8 *)JAVA_MODULE_VERSION, LITERAL_STRLEN(JAVA_MODULE_VERSION), J9_STR_XLAT);
					if (string == NULL) {
						rc = FALSE;
						/* exception is pending from the call */
						goto done;
					}
					element = PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 0);
					J9VMJAVALANGSTACKTRACEELEMENT_SET_MODULEVERSION(vmThread, element, string);
				} else {
					/* Fetch the J9Module from the j.l.Class->j.l.Module field if we have a class.
					 * Otherwise the more painful package-based lookup must be performed
					 */
					if (ramClass != NULL) {
						j9object_t moduleObject = J9VMJAVALANGCLASS_MODULE(vmThread, ramClass->classObject);
						module = J9OBJECT_ADDRESS_LOAD(vmThread, moduleObject, vm->modulePointerOffset);
					} else {
						UDATA length = packageNameLength(romClass);
						omrthread_monitor_enter(vm->classLoaderModuleAndLocationMutex);
						module = vmFuncs->findModuleForPackage(vmThread, classLoader, J9UTF8_DATA(utfClassName), (U_32) length);
						omrthread_monitor_exit(vm->classLoaderModuleAndLocationMutex);
					}

					if (NULL != module) {
						J9VMJAVALANGSTACKTRACEELEMENT_SET_MODULENAME(vmThread, element, module->moduleName);
						J9VMJAVALANGSTACKTRACEELEMENT_SET_MODULEVERSION(vmThread, element, module->version);
					}
				}
#if JAVA_SPEC_VERSION >= 11
				setStackTraceElementFields(vmThread, element, classLoader);
#endif /* JAVA_SPEC_VERSION >= 11 */
			}

			/* Fill in method class */
			string = NULL;
			{
				if (NULL != ramClass) {
					string = J9VMJAVALANGCLASS_CLASSNAMESTRING(vmThread, J9VM_J9CLASS_TO_HEAPCLASS(ramClass));
				}
				if (NULL == string) {
					UDATA flags = J9_STR_XLAT | J9_STR_TENURE | J9_STR_INTERN;
					if (J9_ARE_ANY_BITS_SET(romClass->extraModifiers, J9AccClassAnonClass | J9AccClassHidden)) {
						flags |= J9_STR_ANON_CLASS_NAME;
					}
					string = mmfns->j9gc_createJavaLangString(vmThread, J9UTF8_DATA(utfClassName), (U_32) J9UTF8_LENGTH(utfClassName), flags);
					if (NULL == string) {
						rc = FALSE;
						/* exception is pending from the call */
						goto done;
					} else {
						if (NULL != ramClass) {
							/* Class name was interned so it's safe to write it back to the Class Object */
							J9VMJAVALANGCLASS_SET_CLASSNAMESTRING(vmThread, J9VM_J9CLASS_TO_HEAPCLASS(ramClass), string);
						}
					}
				}
				element = PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 0);
				J9VMJAVALANGSTACKTRACEELEMENT_SET_DECLARINGCLASS(vmThread, element, string);
			}

			/* Fill in method name - intern the string as it's also interned by the StackWalker API and by Reflection */
			utf = J9ROMMETHOD_NAME(romMethod);
			string = mmfns->j9gc_createJavaLangString(vmThread, J9UTF8_DATA(utf), (U_32) J9UTF8_LENGTH(utf), J9_STR_TENURE | J9_STR_INTERN);
			if (string == NULL) {
				rc = FALSE;
				/* exception is pending from the call */
				goto done;
			}
			element = PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 0);
			J9VMJAVALANGSTACKTRACEELEMENT_SET_METHODNAME(vmThread, element, string);

			/* Fill in file name, if any.
			 * Attempt to reuse the cached string if it is available.  It may be found
			 * either in the Class object if it's be previous cached, or it may be found
			 * in the StackTraceElement[] if the previous filename is the same.
			 *
			 * The previous filename cache covers the case where multiple classes were
			 * defined in the same file.
			 *
			 * This avoids additional allocations during stack trace generation
			 */
			string = NULL;
			if (ramClass != NULL) {
				string = J9VMJAVALANGCLASS_FILENAMESTRING(vmThread, J9VM_J9CLASS_TO_HEAPCLASS(ramClass));
			}
			if (string == NULL) {
				if (fileName != NULL) {
					J9UTF8 *previousFileName = userData->previousFileName;
					/* Use an == comparison here as the previousFileName may have been from
					* a classloader that was unloaded.  We can safely do an == comparison
					* as we know the current class is deeper in the stack and can't have
					* incorrectly been loaded into the space the previous loader was removed
					* from.  We can't do a string compare here but the == should be sufficient
					* provided the utf8 interning is hitting on common strings.
					*/
					if (previousFileName == fileName) {
						j9array_t result = (j9array_t) PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 2);
						element = J9JAVAARRAYOFOBJECT_LOAD(vmThread, result, currentIndex - 1);
						string = J9VMJAVALANGSTACKTRACEELEMENT_FILENAME(vmThread, element);
					} else {
						string = mmfns->j9gc_createJavaLangString(vmThread, J9UTF8_DATA(fileName), (U_32) J9UTF8_LENGTH(fileName), 0);
						if (string == NULL) {
							rc = FALSE;
							/* exception is pending from the call */
							goto done;
						}
					}
					Assert_JCL_notNull(string);
					if (ramClass != NULL) {
						/* Update the cached fileNameString on the class so subsequent calls will find it */
						J9VMJAVALANGCLASS_SET_FILENAMESTRING(vmThread, J9VM_J9CLASS_TO_HEAPCLASS(ramClass), string);
					}
				}
			}
			/* Update previous filename as it must always match the contents of the StackTraceElement[n-1]'s
			 * value.  This means it must be null if the previous filename was null or we'll copy the wrong
			 * name into the StackTraceElement
			 */
			userData->previousFileName = fileName;
			if (string != NULL) {
				element = PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 0);
				J9VMJAVALANGSTACKTRACEELEMENT_SET_FILENAME(vmThread, element, string);
			}

			/* Fill in line number - Java wants -2 for natives, -1 for no line number (which will be 0 coming in from the iterator) */

			if (romMethod->modifiers & J9AccNative) {
				lineNumber = -2;
			} else if (lineNumber == 0) {
				lineNumber = -1;
			}
			J9VMJAVALANGSTACKTRACEELEMENT_SET_LINENUMBER(vmThread, element, (I_32) lineNumber);

			if (vm->verboseLevel & VERBOSE_STACKTRACE) {
				setStackTraceElementSource(vmThread, element, classLoader, romClass);
			}

done:
			DROP_OBJECT_IN_SPECIAL_FRAME(vmThread);
		} else {
			/* Update previous filename as it must always match the contents of the StackTraceElement[n-1]'s
			 * value.  This means it must be null if the previous filename was null or we'll copy the wrong
			 * name into the StackTraceElement.  As we didn't have a ROMMethod here, nothing to fill in / process
			 * and so we reset the previousFileName.
			 */
			userData->previousFileName = NULL;
		}
	}
	DROP_OBJECT_IN_SPECIAL_FRAME(vmThread);

	return rc;
}

J9IndexableObject *  
getStackTrace(J9VMThread * vmThread, j9object_t * exceptionAddr, UDATA pruneConstructors)
{
	J9JavaVM * vm = vmThread->javaVM;
	J9InternalVMFunctions * vmFuncs = vm->internalVMFunctions;
	J9MemoryManagerFunctions * mmfns = vm->memoryManagerFunctions;
	UDATA numberOfFrames;
	J9Class * elementClass;
	J9Class * arrayClass;
	J9GetStackTraceUserData userData;
	J9IndexableObject * result;

	/* Note that exceptionAddr might be a pointer into the current thread's stack, so no java code is allowed to run
	   (nothing which could cause the stack to grow).
	*/

retry:

	/* Get the total number of entries in the trace */

	numberOfFrames = vmFuncs->iterateStackTrace(vmThread, exceptionAddr, NULL, NULL, pruneConstructors);

	/* Create the result array */

	elementClass = J9VMJAVALANGSTACKTRACEELEMENT_OR_NULL(vm);
	arrayClass = elementClass->arrayClass;
	if (arrayClass == NULL) {
		/* the first class in vm->arrayROMClasses is the array class for Objects */
		arrayClass = vmFuncs->internalCreateArrayClass(vmThread,
			(J9ROMArrayClass *) J9ROMIMAGEHEADER_FIRSTCLASS(vm->arrayROMClasses), 
			elementClass);
		if (arrayClass == NULL) {
			/* exception is pending from the call */
			return NULL;
		}
	}
	result = (j9array_t) mmfns->J9AllocateIndexableObject(
		vmThread, arrayClass, (U_32)numberOfFrames, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
	if (result == NULL) {
		vmFuncs->setHeapOutOfMemoryError(vmThread);
		return NULL;
	}

	/* Fill in the stack trace */

	userData.elementClass = elementClass;
	userData.index = 0;
	userData.maxFrames = numberOfFrames;
	userData.previousFileName = NULL;
	PUSH_OBJECT_IN_SPECIAL_FRAME(vmThread, (j9object_t) result);
	vmFuncs->iterateStackTrace(vmThread, exceptionAddr, getStackTraceIterator, &userData, pruneConstructors);
	result = (j9array_t) POP_OBJECT_IN_SPECIAL_FRAME(vmThread);

	/* If the stack trace sizes are inconsistent between pass 1 and 2, start again */

	if (vmThread->currentException == NULL) {
		if (userData.index != numberOfFrames) {
			goto retry;
		}
	}

	/* Return the result - any pending exception will be checked by the caller and the result discarded */

	return result;
}

#if JAVA_SPEC_VERSION >= 11
/**
 * Set the includeClassLoaderName and includeModuleVersion fields for a StackTraceElement.
 *
 * @param vmThread The VM thread.
 * @param element The element to set fields for.
 * @param classLoader The classloader to check.
 */
static void
setStackTraceElementFields(J9VMThread *vmThread, j9object_t element, J9ClassLoader *classLoader) {
	J9JavaVM *vm = vmThread->javaVM;
	BOOLEAN includeClassLoaderName = TRUE;
	BOOLEAN includeModuleVersion = TRUE;

	/**
	 * If the classloader is one of the Platform or Bootstrap built-in classloaders,
	 * don't include its name or module version in the stack trace. If it is the
	 * Application/System built-in classloader, don't include the class name, but
	 * include the module version.
	 */
	if ((NULL == classLoader)
		|| (vm->extensionClassLoader == classLoader) // JRE: Platform ClassLoader
		|| (vm->systemClassLoader == classLoader) // JRE: Bootstrap ClassLoader
	) {
		includeClassLoaderName = FALSE;
		includeModuleVersion = FALSE;
	} else if (vm->applicationClassLoader == classLoader) { // JRE: System ClassLoader
		includeClassLoaderName = FALSE;
	}

	J9VMJAVALANGSTACKTRACEELEMENT_SET_INCLUDECLASSLOADERNAME(vmThread, element, (U_32) includeClassLoaderName);
	J9VMJAVALANGSTACKTRACEELEMENT_SET_INCLUDEMODULEVERSION(vmThread, element, (U_32) includeModuleVersion);
}
#endif /* JAVA_SPEC_VERSION >= 11 */
