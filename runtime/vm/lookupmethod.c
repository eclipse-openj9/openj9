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

#include "j9.h"
#include "j9protos.h"
#include "rommeth.h"
#include "vm_internal.h"
#include "ut_j9vm.h"
#include "j9vmnls.h"
#include "objhelp.h"

#define SUPERCLASS(clazz) ((clazz)->superclasses[ J9CLASS_DEPTH(clazz) - 1 ])

#define STRINGBUILDER "java/lang/StringBuilder"
#define APPEND_NAME "append"
#define APPEND_SIG "(Ljava/lang/StringBuilder;)Ljava/lang/StringBuilder;"
#define NEW_SIG "(Ljava/lang/CharSequence;)Ljava/lang/StringBuilder;"
#define DEFAULT_INTERFACE_RESOLVE_ARRAY_SIZE	10

/**
 * Holds information about interface method resolution:
 *  - Exception information
 *  - Multiple resolved methods
 */
typedef struct J9InterfaceResolveData {
	UDATA exception;
	J9Class * exceptionClass;
	J9Method **methods;
	UDATA elements;
	IDATA errorType;
} J9InterfaceResolveData;

static char *
defaultMethodConflictExceptionMessage(J9VMThread *currentThread, J9Class *targetClass, UDATA nameLength, U_8 *name, UDATA sigLength, U_8 *sig, J9Method **methods, UDATA methodsLength);
static J9Method *
searchClassForMethodCommon(J9Class * clazz, U_8 * name, UDATA nameLength, U_8 * sig, UDATA sigLength, BOOLEAN partialMatch);
static J9Method* javaResolveInterfaceMethods(J9VMThread *currentThread, J9Class *targetClass, J9ROMNameAndSignature *nameAndSig, J9Class *senderClass, UDATA lookupOptions, J9InterfaceResolveData *data);

/**
 * Search method in target class
 * Note: this method is also used for searching enclosing methods to skip validation required by javaLookupMethod.
 *
 * @param clazz[in] the class or interface to start the search
 * @param name[in] the name of the method
 * @param nameLength[in] the length of the method name
 * @param sig[in] the signature of the method
 * @param sigLength[in] the length of the method signature
 *
 * @returns the method (if found) or NULL otherwise
 */
J9Method *
searchClassForMethod(J9Class * clazz, U_8 * name, UDATA nameLength, U_8 * sig, UDATA sigLength)
{
	return searchClassForMethodCommon(clazz, name, nameLength,sig, sigLength, FALSE);
}

/**
 * Search method in target class
 * Note: this method is also used for searching enclosing methods to skip validation required by javaLookupMethod.
 *
 * @param clazz[in] the class or interface to start the search
 * @param name[in] the name of the method
 * @param nameLength[in] the length of the method name
 * @param sig[in] the signature of the method
 * @param sigLength[in] the length of the method signature
 * @param partialMatch[in] If true, compare signature up to sigLength
 *
 * @returns the method (if found) or NULL otherwise
 */
static J9Method *
searchClassForMethodCommon(J9Class * clazz, U_8 * name, UDATA nameLength, U_8 * sig, UDATA sigLength, BOOLEAN partialMatch)
{
	J9ROMClass *romClass = clazz->romClass;
	U_32 romMethodCount = romClass->romMethodCount;
	J9Method * searchResult = NULL;

	if (romMethodCount != 0) {
		J9Method * methods = clazz->ramMethods;
		if (J9_ARE_ALL_BITS_SET(romClass->extraModifiers, J9AccClassUseBisectionSearch)) {
			IDATA startIndex = 0;
			IDATA endIndex = (romMethodCount - 1);
			IDATA midIndex = endIndex/2;

			/*
			 * This path searches for the required method using a binary search technique. At this
			 * point the methods have already been sorted using 'compareMethodNameAndSignature'. We will
			 * now iteratively compare the target name+sig with the method in the middle (midIndex). If it does
			 * not match the range is halved - 1. The half chosen is determined by the result of the compareMethod.
			 * This continues until the target is found, otherwise NULL is returned.
			 */

			Trc_VM_searchClass_useRomMethodBinarySearch(methods);

			do {
				J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(&(methods[midIndex]));
				J9UTF8 * nameUTF = J9ROMMETHOD_NAME(romMethod);
				J9UTF8 * sigUTF = J9ROMMETHOD_SIGNATURE(romMethod);
				IDATA result = partialMatch ?
						compareMethodNameAndPartialSignature(name, (U_16) nameLength, sig, (U_16) sigLength, J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF), J9UTF8_DATA(sigUTF), J9UTF8_LENGTH(sigUTF))
						: compareMethodNameAndSignature(name, (U_16) nameLength, sig, (U_16) sigLength, J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF), J9UTF8_DATA(sigUTF), J9UTF8_LENGTH(sigUTF));

				if (result < 0) {
					endIndex = midIndex - 1;
				} else if (result > 0) {
					startIndex = midIndex + 1;
				} else {
					searchResult = &(methods[midIndex]);
					break;
				}

				midIndex = (endIndex + startIndex)/2;
			} while (startIndex <= endIndex);
		} else {
			Trc_VM_searchClass_useRomMethodLinearSearch(methods);

			do {
				J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(methods);
				J9UTF8 * nameUTF = J9ROMMETHOD_NAME(romMethod);
				J9UTF8 * sigUTF = J9ROMMETHOD_SIGNATURE(romMethod);
				IDATA result = partialMatch ?
						compareMethodNameAndPartialSignature(name, (U_16) nameLength, sig, (U_16) sigLength, J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF), J9UTF8_DATA(sigUTF), J9UTF8_LENGTH(sigUTF))
						: compareMethodNameAndSignature(name, (U_16) nameLength, sig, (U_16) sigLength, J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF), J9UTF8_DATA(sigUTF), J9UTF8_LENGTH(sigUTF));
				if (0 == result) {
					searchResult = methods;
					break;
				}
				methods += 1;
			} while (--romMethodCount != 0);
		}
	}
	return searchResult;
}


static J9Method *
processMethod(J9VMThread * currentThread, UDATA lookupOptions, J9Method * method, J9Class * methodClass, UDATA * exception, J9Class ** exceptionClass, IDATA *errorType, J9ROMNameAndSignature * nameAndSig, J9Class * senderClass, J9Class * targetClass) {
	J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
	U_32 modifiers = romMethod->modifiers;
	J9JavaVM * vm = currentThread->javaVM;

	/* Check that the found method is visible from the sender */

	if (J9_ARE_NO_BITS_SET(lookupOptions, J9_LOOK_NO_VISIBILITY_CHECK) && (NULL != senderClass) && !J9CLASS_IS_EXEMPT_FROM_VALIDATION(senderClass)) {
		U_32 newModifiers = modifiers;
		BOOLEAN doVisibilityCheck = TRUE;

		if (lookupOptions & J9_LOOK_NEW_INSTANCE) {
			newModifiers &= ~J9AccProtected;
		}

		/* Additional protected method check to ensure that the senderClass
		 * is related to the target class if the method is protected virtual.
		 * Note that we need to first check whether the method
		 * is the public method 'clone' so as to skip the generic access checking in checkVisibility().
		 */
		if (methodClass->packageID != senderClass->packageID) {
			if (J9_ARE_ANY_BITS_SET(modifiers, J9AccProtected)
			&& (J9_ARE_NO_BITS_SET(modifiers, J9AccStatic))
			) {
				J9Class *currentTargetClass = J9_CURRENT_CLASS(targetClass);
				J9Class *currentSenderClass = J9_CURRENT_CLASS(senderClass);
				if (!isSameOrSuperClassOf(currentTargetClass, currentSenderClass)
				&& !isSameOrSuperClassOf(currentSenderClass, currentTargetClass)
				) {
					J9UTF8 * nameUTF = J9ROMMETHOD_NAME(romMethod);
					if (J9ROMCLASS_IS_ARRAY(targetClass->romClass)
					&& (J9UTF8_LITERAL_EQUALS(J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF), "clone"))
					) {
						/* JLS 10.7 Array Members: The public method clone, which overrides the
						 * method of the same name in class Object and throws no checked exceptions.
						 */
						doVisibilityCheck = FALSE;
					} else {
						*exception = J9VMCONSTANTPOOL_JAVALANGILLEGALACCESSERROR;
						*exceptionClass = methodClass;
						*errorType = J9_VISIBILITY_NON_MODULE_ACCESS_ERROR;
						return NULL;
					}
				}
			}
		}

		if (doVisibilityCheck) {
			IDATA checkResult = checkVisibility(currentThread, senderClass, methodClass, newModifiers, lookupOptions | J9_LOOK_NO_MODULE_CHECKS);
			if (checkResult < J9_VISIBILITY_ALLOWED) {
				*exception = J9VMCONSTANTPOOL_JAVALANGILLEGALACCESSERROR;
				*exceptionClass = methodClass;
				*errorType = checkResult;
				return NULL;
			}
		}
	}

	/* Check that we find the right kind of method (static / virtual / interface) */

	if (((lookupOptions & J9_LOOK_STATIC) && ((modifiers & J9AccStatic) == 0))
	|| ((lookupOptions & J9_LOOK_VIRTUAL) && (modifiers & J9AccStatic))
	) {
		*exception = J9VMCONSTANTPOOL_JAVALANGINCOMPATIBLECLASSCHANGEERROR;
		*exceptionClass = methodClass;
		*errorType = J9_VISIBILITY_NON_MODULE_ACCESS_ERROR;
		return NULL;
	}

	/* Check that no class loading constraints are violated.
	 * 
	 * NOTE: Asking for the constraint check implies that nameAndSig is a J9ROMNameAndSignature *
	 * and that senderClass is not NULL.
	 */

	if (lookupOptions & J9_LOOK_CLCONSTRAINTS) {
		if (vm->runtimeFlags & J9_RUNTIME_VERIFY) {
			J9ClassLoader * cl1 = senderClass->classLoader;
			J9ClassLoader * cl2 = methodClass->classLoader;

			if (cl1 != cl2) {
				J9UTF8 * lookupSig;
				J9UTF8 * methodSig = J9ROMMETHOD_SIGNATURE(romMethod);
				if (lookupOptions & J9_LOOK_DIRECT_NAS) {
					J9NameAndSignature * nas = (J9NameAndSignature *) nameAndSig;
					lookupSig = nas->signature;
				} else {
					lookupSig = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig);
				}
				
				if (0 != j9bcv_checkClassLoadingConstraintsForSignature(
						currentThread,
						cl1,
						cl2,
						lookupSig,
						methodSig,
						J9_ARE_ALL_BITS_SET(lookupOptions, J9_LOOK_DIRECT_NAS))
				) {
					*exception = J9VMCONSTANTPOOL_JAVALANGLINKAGEERROR; /* was VerifyError; but Sun throws Linkage */
					*exceptionClass = methodClass;
					*errorType = J9_VISIBILITY_NON_MODULE_ACCESS_ERROR;
					Trc_VM_processMethod_ClassLoaderConstraintFailure(currentThread, method, cl1, cl2);
					return NULL;
				}
			}
		}
	}

	/* Method is valid */

	return method;
}

/**
* @brief Lookup method in interface, which handles default method resolution for Java 8
*
* ***The caller must free the memory from the pointer data->errorModuleMsg after invoking setCurrentExceptionUTF() with this pointer***
*
* @param currentThread Current VM thread
* @param targetClass The class or interface to start the resolution in
* @param nameAndSig The name and signature of the method
* @param senderClass The caller class
* @param lookupOptions VM lookup options
* @param data Information about the resolution
*
* @return J9Method* The resolved method (if found)
 */
static J9Method*
javaResolveInterfaceMethods(J9VMThread *currentThread, J9Class *targetClass, J9ROMNameAndSignature *nameAndSig, J9Class *senderClass, UDATA lookupOptions, J9InterfaceResolveData *data)
{
	/* Return object */
	J9Method *resultMethod = NULL;
	/* Statically defined array to avoid mallocing */
	J9Method *staticArray[DEFAULT_INTERFACE_RESOLVE_ARRAY_SIZE];
	/* Reference to the current array */
	J9Method **workingArray = staticArray;
	/* Current length of workingArray */
	IDATA arrayLength = DEFAULT_INTERFACE_RESOLVE_ARRAY_SIZE;
	/* Number of valid methods inside workingArray */
	IDATA numElements = 0;
	/* Maximum occupied slot in workingArray */
	IDATA maxUsedSlotIndex = -1;
	IDATA i = 0;
	UDATA nameLength = 0;
	U_8 * name = NULL;
	UDATA sigLength = 0;
	U_8 * sig = NULL;
	J9ITable * iTable = NULL;
	PORT_ACCESS_FROM_VMC(currentThread);
	memset(staticArray, 0, DEFAULT_INTERFACE_RESOLVE_ARRAY_SIZE * sizeof(J9Method*));

	if (J9_LOOK_JNI == (lookupOptions & J9_LOOK_JNI)) {
		J9JNINameAndSignature * nas = (J9JNINameAndSignature *) nameAndSig;

		name = (U_8 *) nas->name;
		nameLength = nas->nameLength;
		sig = (U_8 *) nas->signature;
		sigLength = nas->signatureLength;
	} else {
		J9UTF8 * nameUTF = NULL;
		J9UTF8 * sigUTF = NULL;

		if (J9_LOOK_DIRECT_NAS == (lookupOptions & J9_LOOK_DIRECT_NAS)) {
			J9NameAndSignature * nas = (J9NameAndSignature *) nameAndSig;
			nameUTF = nas->name;
			sigUTF = nas->signature;
		} else {
			J9ROMNameAndSignature * nas = nameAndSig;

			nameUTF = J9ROMNAMEANDSIGNATURE_NAME(nas);
			sigUTF = J9ROMNAMEANDSIGNATURE_SIGNATURE(nas);
		}

		name = J9UTF8_DATA(nameUTF);
		nameLength = J9UTF8_LENGTH(nameUTF);
		sig = J9UTF8_DATA(sigUTF);
		sigLength = J9UTF8_LENGTH(sigUTF);
	}

	iTable = (J9ITable *) targetClass->iTable;
	data->methods = NULL;
	data->elements = 0;

	/* Quickly look for a direct match by looking for a match in targetClass */
	while (iTable != NULL) {
		J9Class * interfaceClass = iTable->interfaceClass;
		if (interfaceClass == targetClass) {
			J9Method * foundMethod = searchClassForMethodCommon(interfaceClass, name, nameLength, sig, sigLength, J9_ARE_ANY_BITS_SET(lookupOptions, J9_LOOK_PARTIAL_SIGNATURE));
			if (NULL != foundMethod) {
				/* As per spec, prevent super-interface private or static methods from being processed */
				if (J9_ARE_NO_BITS_SET(J9_ROM_METHOD_FROM_RAM_METHOD(foundMethod)->modifiers, J9AccPrivate | J9AccStatic)) {
					resultMethod = processMethod(currentThread, lookupOptions, foundMethod, interfaceClass, &data->exception, &data->exceptionClass, &data->errorType, nameAndSig, senderClass, targetClass);
					return resultMethod;
				}
			}
			/* No point in searching more, we won't find it */
			break;
		}
		iTable = iTable->next;
	}

	/* We didn't find it, restart the search */
	iTable = (J9ITable *) targetClass->iTable;

	while (iTable != NULL) {
		J9Method *foundMethod = NULL;
		J9Class * interfaceClass = iTable->interfaceClass;
		if (interfaceClass == targetClass) {
			/* Already checked this class in the quick search above */
			iTable = iTable->next;
			continue;
		}

		foundMethod = searchClassForMethodCommon(interfaceClass, name, nameLength, sig, sigLength, J9_ARE_ANY_BITS_SET(lookupOptions, J9_LOOK_PARTIAL_SIGNATURE));

		/* As per spec, prevent super-interface private or static methods from being processed */
		if (NULL != foundMethod) {
			if (J9_ARE_ANY_BITS_SET(J9_ROM_METHOD_FROM_RAM_METHOD(foundMethod)->modifiers, J9AccPrivate | J9AccStatic)){
				foundMethod = NULL;
			}
		}

		if (NULL != foundMethod) {
			J9Method *newMethod = NULL;
			BOOLEAN shouldAdd = TRUE;
			J9Class* newMethodClass = NULL;
			newMethod = processMethod(currentThread, lookupOptions, foundMethod, interfaceClass, &data->exception, &data->exceptionClass, &data->errorType, nameAndSig, senderClass, targetClass);

			/* Exit loop if exception have been directly set since direct exception must be from access control (Nestmates) */
			if (NULL != currentThread->currentException) {
				return NULL;
			}

			if (NULL == newMethod) {
				iTable = iTable->next;
				continue;
			}
			newMethodClass = J9_CLASS_FROM_METHOD(newMethod); 
			/* Iterate through workingArray to try to either rule out this method
			 * or replace other methods with this.  We only need to do this if
			 * we have a positive number of live elements
			 */
			for (i = 0; (i <= (maxUsedSlotIndex)) && (numElements > 0); i++) {
				J9Class* entryClass = NULL;

				if (NULL == workingArray[i]) {
					continue;
				}

				entryClass = J9_CLASS_FROM_METHOD(workingArray[i]);

				if (isSameOrSuperInterfaceOf(newMethodClass, entryClass)) {
					/* If we already have something that overrides newMethod, then we don't add it */
					shouldAdd = FALSE;
				} else if (isSameOrSuperInterfaceOf(entryClass, newMethodClass)) {
					/* If newMethod overrides this element, NULL it and be sure to add this */
					workingArray[i] = NULL;
					/* Ensure we correctly reflect live objects */
					numElements -= 1;
				}
			}
			if (shouldAdd) {
				BOOLEAN added = FALSE;
				/* Search for a free slot to add our method */
				for (i = 0; i <= (maxUsedSlotIndex + 1) && i < arrayLength; i++) {
					if (NULL == workingArray[i]) {
						workingArray[i] = newMethod;
						added = TRUE;
						if (i > maxUsedSlotIndex) {
							maxUsedSlotIndex = i;
						}
						break;
					}
				}
				if (!added) {
					/* Check to see if we have space in our current array and allocate more if needed */
					if ((maxUsedSlotIndex + 1) >= arrayLength) {
						IDATA newArrayLength = arrayLength * 2;
						J9Method **newArray = j9mem_allocate_memory(sizeof(J9Method*) * newArrayLength, OMRMEM_CATEGORY_VM);
						if (NULL == newArray) {
							data->exception = J9VMCONSTANTPOOL_JAVALANGOUTOFMEMORYERROR;
							data->errorType = J9_VISIBILITY_NON_MODULE_ACCESS_ERROR;
							return NULL;
						}
						/* Only memset the upper (new) portion of the array */
						memset(newArray + arrayLength, 0, sizeof(J9Method*) * arrayLength);
						memcpy(newArray, workingArray, sizeof(J9Method*) * arrayLength);
						if (workingArray != staticArray) {
							j9mem_free_memory(workingArray);
						}
						arrayLength = newArrayLength;
						workingArray = newArray;
					}
					workingArray[maxUsedSlotIndex++] = newMethod;
				}
				numElements += 1;
				added = TRUE;
			}
		}

		if (J9_LOOK_NO_CLIMB == (lookupOptions & J9_LOOK_NO_CLIMB)) {
			goto doneItableSearch;
		}
		iTable = iTable->next;
	}
doneItableSearch:
	if (1 == numElements) {
		/* Exactly one match, find it and return it */
		IDATA i = 0;
		resultMethod = NULL;
		for (i = 0; i <= maxUsedSlotIndex; i++) {
			if (workingArray[i] != NULL) {
				resultMethod = workingArray[i];
				break;
			}
		}
	} else if (numElements > 1) {
		BOOLEAN conflict = FALSE;
		BOOLEAN foundDefault = FALSE;
		J9Method *candidate = NULL;
		/* Find initial candidate, which is the first non-null method */
		for (i = 0; i <= maxUsedSlotIndex; i++) {
			J9Method *current = workingArray[i];
			if (NULL != current) {
				candidate = current;
				foundDefault = !(J9_ARE_ANY_BITS_SET(J9_ROM_METHOD_FROM_RAM_METHOD(current)->modifiers, J9AccAbstract));
				break;
			}
		}
		/* Keep searching for conflict cases (more than one non-abstract), or for
		 * non-abstract method that can replace initial abstract candidate.
		 */
		i += 1;	/* Ensure that the second loop starts with the next method */
		for (; i <= maxUsedSlotIndex; i++) {
			J9Method *current = workingArray[i];
			if (NULL != current) {
				BOOLEAN abstract = J9_ARE_ANY_BITS_SET(J9_ROM_METHOD_FROM_RAM_METHOD(current)->modifiers, J9AccAbstract);
				if (!abstract) {
					if (foundDefault) {
						/* More than one non-abstract method, conflict */
						conflict = TRUE;
						break;
					} else {
						/* Non-abstract method replaces initial abstract candidate */
						candidate = current;
						foundDefault = TRUE;
					}
				}
			}
		}

		/* The error handling code below clears the resultMethod */
		resultMethod = candidate;

		if (conflict && J9_ARE_ANY_BITS_SET(lookupOptions, J9_LOOK_HANDLE_DEFAULT_METHOD_CONFLICTS)) {
			/* We have a conflict, compact and copy the methods into
			 * a new array so that a nice exception can be thrown for
			 * the upcoming AbstractMethodError
			 */
			J9Method **readPtr = workingArray;
			J9Method **writePtr = workingArray;

			resultMethod = NULL;
			data->exception = J9VMCONSTANTPOOL_JAVALANGINCOMPATIBLECLASSCHANGEERROR;
			data->errorType = J9_VISIBILITY_NON_MODULE_ACCESS_ERROR;

			/* Iterate across workingArray and compacting it into one contiguous array */
			for (i = 0; i <= maxUsedSlotIndex; i++) {
				/* If the index contains a valid element (non-null) */
				if (NULL != *readPtr) {
					*writePtr = *readPtr;
					if (readPtr != writePtr) {
						*readPtr = NULL;
					}
					writePtr += 1;
				}
				/* Point to the next free slot */
				readPtr += 1;
			}

			/* Determine if we need to allocate a new array to store the methods in */
			if (workingArray == staticArray) {
				J9Method **newArray = j9mem_allocate_memory(sizeof(J9Method*) * numElements, OMRMEM_CATEGORY_VM);
				if (NULL == newArray) {
					data->exception = J9VMCONSTANTPOOL_JAVALANGOUTOFMEMORYERROR;
					data->errorType = J9_VISIBILITY_NON_MODULE_ACCESS_ERROR;
					return NULL;
				}
				memcpy(newArray, workingArray, sizeof(J9Method*) * numElements);
				data->methods = newArray;
			} else {
				data->methods = workingArray;
				/* Prevents the array from being deallocated */
				workingArray = NULL;
			}
			data->elements = numElements;
		}
	} else {
		/* No methods were found */
		resultMethod = NULL;
	}
	
	if (workingArray != staticArray) {
		j9mem_free_memory(workingArray);
	}
	return resultMethod;
}

/* Options explained:
 * 
 * 		J9_LOOK_VIRTUAL							Search for a virtual (instance) method
 * 		J9_LOOK_STATIC							Search for a static method
 * 		J9_LOOK_INTERFACE						Search for an interface method - assumes given class is interface class and searches only it and its superinterfaces (i.e. does not search Object)
 * 		J9_LOOK_JNI								Use JNI behaviour (allow virtual lookup to succeed when it finds a method in an interface class, throw only NoSuchMethodError on failure, VERIFIED C strings for name and sig)
 * 		J9_LOOK_NO_CLIMB						Search only the given class, no superclasses or superinterfaces
 * 		J9_LOOK_NO_INTERFACE					Do not search superinterfaces
 * 		J9_LOOK_NO_THROW						Do not set the exception if lookup fails
 * 		J9_LOOK_NO_JAVA							Do not run java code for any reason (implies NO_THROW)
 * 		J9_LOOK_NEW_INSTANCE					Use newInstance behaviour (translate IllegalAccessError -> IllegalAccessException, all other errors -> InstantiationException)
 * 		J9_LOOK_DIRECT_NAS						NAS contains direct pointers to UTF8, not SRPs (this option is mutually exclusive with lookupOptionsJNI)
 * 		J9_LOOK_CLCONSTRAINTS					Check that the found method doesn't violate any class loading constraints between the found class and the sender class.
 * 		J9_LOOK_PARTIAL_SIGNATURE				Allow the search to match a partial signature
 *		J9_LOOK_NO_VISIBILITY_CHECK				Do not perform any visilbity checking
 *		J9_LOOK_NO_JLOBJECT						When doing an interface lookup, do not consider method in java.lang.Object
 *		J9_LOOK_REFLECT_CALL					Use reflection behaviour when dealing with module visibility
 *		J9_LOOK_HANDLE_DEFAULT_METHOD_CONFLICTS	Handle default method conflicts
 *		J9_LOOK_IGNORE_INCOMPATIBLE_METHODS		If a static/virtual conflict occurs, ignore and move on to the next class
 *
 *  	If J9_LOOK_JNI is not specified, virtual and static lookup will fail with AbstractMethodError if the method is found
 * 		in an interface class.
 *
 *		If J9_LOOK_NO_VISIBILITY_CHECK is not specified and senderClass is not NULL, a visibility check will be
 *		performed.
 */

/* Change nameAndSig to void * */

UDATA
javaLookupMethod(J9VMThread *currentThread, J9Class *targetClass, J9ROMNameAndSignature *nameAndSig, J9Class *senderClass, UDATA lookupOptions)
{
	return javaLookupMethodImpl(currentThread, targetClass, nameAndSig, senderClass, lookupOptions, NULL);
}

j9object_t
getClassPathString(J9VMThread *currentThread, J9Class *clazz)
{
	j9object_t result = NULL;

	if (NULL != clazz) {
		UDATA pathLength = 0;
		U_8 *path = getClassLocation(currentThread, clazz, &pathLength);

		if (NULL != path) {
			result = currentThread->javaVM->memoryManagerFunctions->j9gc_createJavaLangString(currentThread, path, pathLength, 0);
		}
	}
	return result;
}

UDATA
javaLookupMethodImpl(J9VMThread *currentThread, J9Class *targetClass, J9ROMNameAndSignature *nameAndSig, J9Class *senderClass, UDATA lookupOptions, BOOLEAN *foundDefaultConflicts)
{
	J9Method * resultMethod;
	J9Method * badMethod;
	UDATA exception;
	J9Class * exceptionClass;
	IDATA errorType = J9_VISIBILITY_NON_MODULE_ACCESS_ERROR;
	UDATA nameLength;
	U_8 * name;
	UDATA sigLength;
	U_8 * sig;
	J9Class * lookupClass = NULL;
	BOOLEAN exceptionThrown = FALSE;
	BOOLEAN isInterfaceLookup = FALSE;

	Trc_VM_javaLookupMethod_Entry(currentThread, currentThread, targetClass, nameAndSig, senderClass, lookupOptions);

	/* If no method is found, the default error is NoSuchMethodError */

	exception = J9VMCONSTANTPOOL_JAVALANGNOSUCHMETHODERROR;
	exceptionClass = targetClass;
	resultMethod = NULL;
	badMethod = NULL;

	/* Get the name and signature data and length */

	if (lookupOptions & J9_LOOK_JNI) {
		J9JNINameAndSignature * nas = (J9JNINameAndSignature *) nameAndSig;

		name = (U_8 *) nas->name;
		nameLength = nas->nameLength;
		sig = (U_8 *) nas->signature;
		sigLength = nas->signatureLength;
	} else {
		J9UTF8 * nameUTF;
		J9UTF8 * sigUTF;

		if (lookupOptions & J9_LOOK_DIRECT_NAS) {
			J9NameAndSignature * nas = (J9NameAndSignature *) nameAndSig;

			nameUTF = nas->name;
			sigUTF = nas->signature;
		} else {
			J9ROMNameAndSignature * nas = nameAndSig;

			nameUTF = J9ROMNAMEANDSIGNATURE_NAME(nas);
			sigUTF = J9ROMNAMEANDSIGNATURE_SIGNATURE(nas);
		}

		name = J9UTF8_DATA(nameUTF);
		nameLength = J9UTF8_LENGTH(nameUTF);
		sig = J9UTF8_DATA(sigUTF);
		sigLength = J9UTF8_LENGTH(sigUTF);
	}

	Trc_VM_javaLookupMethod_Name(currentThread, nameLength, name);

	if (nameLength != 0) {
		/* If the name begins with < then it is either <init> or <clinit>.  In both of these cases, do not climb. */

		if (name[0] == '<') {
			lookupOptions |= J9_LOOK_NO_CLIMB;
		}

		goto retry; /* avoid warning */
retry:

		/* Interface lookup assumes that the targetClass is an interface and searches only interfaces, never real classes. */
	
		lookupClass = targetClass;
		if (lookupOptions & J9_LOOK_INTERFACE) {
			/* The first entry in the allSupportedInterface list for an interface class is the class itself,
			 * so we can skip straight to the interface search.  This will work even if the noClimb option
			 * is set.
			 */
			isInterfaceLookup = TRUE;
			if (0 == (lookupOptions & J9_LOOK_STATIC)) {
				lookupOptions |= J9_LOOK_VIRTUAL;
			}
		}
	
		/* Search for a matching method in the target class and its superclasses. */
	
		while (lookupClass != NULL) {
			J9Method * foundMethod = searchClassForMethodCommon(lookupClass, name, nameLength, sig, sigLength, J9_ARE_ANY_BITS_SET(lookupOptions, J9_LOOK_PARTIAL_SIGNATURE));

			if (foundMethod != NULL) {
				resultMethod = processMethod(currentThread, lookupOptions, foundMethod, lookupClass, &exception, &exceptionClass, &errorType, nameAndSig, senderClass, targetClass);
				if (NULL != currentThread->currentException) {
					goto end;
				}

				if (resultMethod == NULL) {
					if (J9VMCONSTANTPOOL_JAVALANGINCOMPATIBLECLASSCHANGEERROR == exception) {
						/* Incompatible method found - caller may request to ignore this case */
						if (J9_ARE_ANY_BITS_SET(lookupOptions, J9_LOOK_IGNORE_INCOMPATIBLE_METHODS)) {
							/* Reset exception state to initial values and move up the hierarchy */
							exception = J9VMCONSTANTPOOL_JAVALANGNOSUCHMETHODERROR;
							exceptionClass = targetClass;
							errorType = J9_VISIBILITY_NON_MODULE_ACCESS_ERROR;
							goto nextClass;
						}
					}
					badMethod = foundMethod; /* for clear illegal access message printing purpose */
					if (!isInterfaceLookup) {
						goto done;
					}
					if (lookupClass != J9VMJAVALANGOBJECT_OR_NULL(currentThread->javaVM)) {
						goto done;
					}
					/* interface found inaccessable method in Object - keep looking
					 * as valid interface method may be found by the iTable search.
					 * However, we need to reset the exception info.
					 */
					exception = J9VMCONSTANTPOOL_JAVALANGNOSUCHMETHODERROR;
					exceptionClass = targetClass;
				} else if (!isInterfaceLookup) {
					/* success */
					goto done;
				}
			}
			if (isInterfaceLookup && J9_ARE_ANY_BITS_SET(lookupOptions, J9_LOOK_NO_JLOBJECT)) {
				 break; /* don't look in superclass, i.e. j.l.Object */
			}
nextClass:
			if (J9_ARE_ANY_BITS_SET(lookupOptions, J9_LOOK_NO_CLIMB)) {
				goto done;
			}
			lookupClass = SUPERCLASS(lookupClass);
		}
	
		/* Not found in class hierarchy, search all superinterfaces unless directed not to */
	
		if ((lookupOptions & J9_LOOK_NO_INTERFACE) == 0) {
			J9Method *tempResultMethod = NULL;
			struct J9InterfaceResolveData data = {0};

			data.exception = exception;
			data.exceptionClass = exceptionClass;
			data.errorType = errorType;
			data.methods = NULL;
			data.elements = 0;

			tempResultMethod = javaResolveInterfaceMethods(currentThread, targetClass, nameAndSig, senderClass, lookupOptions, &data);
			if (NULL != currentThread->currentException) {
				exceptionThrown = TRUE;
				goto done;
			}
			if (NULL != tempResultMethod) {
				resultMethod = tempResultMethod;
			}
			
			exception = data.exception;
			exceptionClass = data.exceptionClass;
			errorType = data.errorType;

			if ((NULL == resultMethod) && data.elements > 1) {
				if (NULL != foundDefaultConflicts) {
					*foundDefaultConflicts = TRUE;
				}
				if ((lookupOptions & (J9_LOOK_NO_THROW | J9_LOOK_NO_JAVA)) == 0) {
					PORT_ACCESS_FROM_VMC(currentThread);
					char *buf = NULL;
					if (J9_VISIBILITY_NON_MODULE_ACCESS_ERROR == data.errorType) {
						/* no module access error */
						buf = defaultMethodConflictExceptionMessage(currentThread, targetClass, nameLength, name, sigLength, sig, data.methods, data.elements);
					} else {
						buf = illegalAccessMessage(currentThread, -1, senderClass, targetClass, data.errorType);
					}
					setCurrentExceptionUTF(currentThread, exception, buf);
					j9mem_free_memory(buf);
				}
				exceptionThrown = TRUE;
			}
		}
	}

done:
	if ((NULL == resultMethod) && (FALSE == exceptionThrown)) {
#if defined(J9VM_OPT_SIDECAR) || defined(J9VM_OPT_DEE)
		J9UTF8 * classNameUTF = J9ROMCLASS_CLASSNAME(targetClass->romClass);

		/* If the lookup fails, check for the StringBuilder.append trick
		 * TODO: Still necessary?
		 */

		if (J9UTF8_DATA_EQUALS(J9UTF8_DATA(classNameUTF), J9UTF8_LENGTH(classNameUTF), STRINGBUILDER, sizeof(STRINGBUILDER) - 1)) {
			if (J9UTF8_DATA_EQUALS(name, nameLength, APPEND_NAME, sizeof(APPEND_NAME) - 1)) {
				if (J9UTF8_DATA_EQUALS(sig, sigLength, APPEND_SIG, sizeof(APPEND_SIG) - 1)) {
					sig = (U_8*)NEW_SIG;
					sigLength = sizeof(NEW_SIG) - 1;

					/* The loading constraints function requires a ROM nameAndSig, so skip the check when we lookup the replacement method */

					lookupOptions &= ~J9_LOOK_CLCONSTRAINTS;
					goto retry;
				}
			}
		}
#endif

		if ((lookupOptions & (J9_LOOK_NO_THROW | J9_LOOK_NO_JAVA)) == 0) {
			j9object_t errorString = NULL;

			/* JNI throws NoSuchMethodError in all error cases */

			if (lookupOptions & J9_LOOK_JNI) {
				exception = J9VMCONSTANTPOOL_JAVALANGNOSUCHMETHODERROR;
				exceptionClass = targetClass;
				errorType = J9_VISIBILITY_NON_MODULE_ACCESS_ERROR;
			}

			/* If this lookup is for the <init> in Class.newInstance(), translate IllegalAccessError
			 * into IllegalAccessException and all others to InstantiationException.
			 */
			if (lookupOptions & J9_LOOK_NEW_INSTANCE) {
				if (exception == J9VMCONSTANTPOOL_JAVALANGILLEGALACCESSERROR) {
					exception = J9VMCONSTANTPOOL_JAVALANGILLEGALACCESSEXCEPTION;
				} else {
					exception = J9VMCONSTANTPOOL_JAVALANGINSTANTIATIONEXCEPTION + J9_EX_CTOR_CLASS;
					errorString = J9VM_J9CLASS_TO_HEAPCLASS(exceptionClass);
				}
			}

			if (errorString == NULL) {
				if ((badMethod != NULL) && ((exception == J9VMCONSTANTPOOL_JAVALANGILLEGALACCESSERROR) || (exception == J9VMCONSTANTPOOL_JAVALANGILLEGALACCESSEXCEPTION))) {

					J9ROMMethod * badRomMethod = J9_ROM_METHOD_FROM_RAM_METHOD(badMethod);
					char *buf = NULL;
					
					PORT_ACCESS_FROM_VMC(currentThread);
					
					buf = illegalAccessMessage(currentThread, badRomMethod->modifiers, senderClass, targetClass, errorType);

					setCurrentExceptionUTF(currentThread, exception, buf);
					
					j9mem_free_memory(buf);
				} else if ((badMethod != NULL) && (exception == J9VMCONSTANTPOOL_JAVALANGLINKAGEERROR)){
					/* Classloading constraint was violated in a resolved method signature */
					J9ClassLoader * cl1 = senderClass->classLoader;
					J9ClassLoader * cl2 = lookupClass->classLoader;
					setClassLoadingConstraintSignatureError(currentThread, cl1, senderClass, cl2, lookupClass, exceptionClass, name, nameLength, sig, sigLength);
				} else {
					J9Method *formatMethod = J9VMJAVALANGJ9VMINTERNALS_FORMATNOSUCHMETHOD_METHOD(currentThread->javaVM);
					J9UTF8 * classNameUTF = J9ROMCLASS_CLASSNAME(exceptionClass->romClass);
					j9object_t senderClassPath = NULL;
					j9object_t targetClassPath = NULL;
					j9object_t methodSigStr = catUtfToString4(currentThread,
							J9UTF8_DATA(classNameUTF), J9UTF8_LENGTH(classNameUTF),
							(U_8*)".", 1,
							name, nameLength,
							sig, sigLength);
					if (NULL != currentThread->currentException) {
						/* in case of OOM, just throw that exception instead */
						goto end;
					}
					PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, methodSigStr);

					if (NULL == senderClass) {
						/* check if we are being called from native method */
						J9Method *nativeMethod = NULL;
						switch((UDATA)currentThread->pc) {
						case J9SF_FRAME_TYPE_JNI_NATIVE_METHOD:
						case J9SF_FRAME_TYPE_JIT_JNI_CALLOUT:
							nativeMethod = ((J9SFJNINativeMethodFrame*)((U_8*)currentThread->sp + (UDATA)currentThread->literals))->method;
						}
						if (NULL != nativeMethod) {
							senderClass = J9_CLASS_FROM_METHOD(nativeMethod);
						}
					}
					senderClassPath = getClassPathString(currentThread, senderClass);
					if (NULL != currentThread->currentException) {
						/* in case of OOM, just throw that exception instead */
						DROP_OBJECT_IN_SPECIAL_FRAME(currentThread);
						goto end;
					}
					PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, senderClassPath);

					targetClassPath = getClassPathString(currentThread, targetClass);
					if (NULL != currentThread->currentException) {
						/* in case of OOM, just throw that exception instead */
						DROP_OBJECT_IN_SPECIAL_FRAME(currentThread);
						DROP_OBJECT_IN_SPECIAL_FRAME(currentThread);
						goto end;
					}

					senderClassPath = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
					methodSigStr = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);

					{
						UDATA args[] = {
							(UDATA) methodSigStr,
							(UDATA) J9VM_J9CLASS_TO_HEAPCLASS(targetClass),
							(UDATA) targetClassPath,
							(UDATA) J9VM_J9CLASS_TO_HEAPCLASS(senderClass),
							(UDATA) senderClassPath
						};
						internalRunStaticMethod(currentThread, formatMethod, TRUE, sizeof(args) / sizeof(UDATA), args);
						if (NULL == currentThread->currentException) {
							setCurrentException(currentThread, exception, (UDATA *) currentThread->returnValue);
						}
					}
				}
			} else {
				setCurrentException(currentThread, exception, (UDATA *) errorString);
			}
		}
	}
end:
	Trc_VM_javaLookupMethod_Exit(currentThread, resultMethod);
	return (UDATA) resultMethod;
}

static char *
defaultMethodConflictExceptionMessage(J9VMThread *currentThread, J9Class *targetClass, UDATA nameLength, U_8 *name, UDATA sigLength, U_8 *sig, J9Method **methods, UDATA methodsLength)
{
	J9UTF8 * targetClassNameUTF = J9ROMCLASS_CLASSNAME(targetClass->romClass);
	PORT_ACCESS_FROM_VMC(currentThread);
	const char *errorMsg = NULL;
	UDATA bufLen = 0;
	U_8 *listString = NULL;
	UDATA listLength = 0;
	UDATA i = 0;
	UDATA offset = 0;
	char *buf = NULL;

	Assert_VM_true(methodsLength > 1);

	errorMsg = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
			J9NLS_VM_DEFAULT_METHOD_CONFLICT_LIST, "conflicting default methods for '%.*s%.*s' in %.*s from classes [%s]");
	
	/* Construct class list string */
	for (i = 0; i < methodsLength; i++) {
		J9UTF8 *className = J9ROMCLASS_CLASSNAME(J9_CLASS_FROM_METHOD(methods[i])->romClass);
		listLength += (J9UTF8_LENGTH(className) + 2); /* for the ", " added to each element, will be 2 char to big */
	}
	listLength -= 2; /* Correct for the extra 2 */
	listString = j9mem_allocate_memory(listLength + 1, OMRMEM_CATEGORY_VM);
	if (NULL == listString) {
		return NULL;
	}
	for (i = 0; i < methodsLength; i++) {
		J9UTF8 *className = J9ROMCLASS_CLASSNAME(J9_CLASS_FROM_METHOD(methods[i])->romClass);
		if (i != 0) {
			memcpy(listString + offset, ", ", 2);
			offset += 2;
		}
		memcpy(listString + offset, J9UTF8_DATA(className), J9UTF8_LENGTH(className));
		offset += J9UTF8_LENGTH(className);
	}
	listString[listLength] = '\0'; /* Overallocated by 1 to enable null termination */

	/* Write error message to buffer */
	bufLen = j9str_printf(PORTLIB, NULL, 0, errorMsg,
				nameLength,
				name,
				sigLength,
				sig,
				(UDATA)J9UTF8_LENGTH(targetClassNameUTF),
				J9UTF8_DATA(targetClassNameUTF),
				listString);
	if (bufLen > 0) {
		buf = j9mem_allocate_memory(bufLen, OMRMEM_CATEGORY_VM);
		if (NULL != buf) {
			bufLen = j9str_printf(PORTLIB, buf, bufLen, errorMsg,
						nameLength,
						name,
						sigLength,
						sig,
						J9UTF8_LENGTH(targetClassNameUTF),
						J9UTF8_DATA(targetClassNameUTF),
						listString);
		}
	}
	j9mem_free_memory(listString);
	return buf;
}

#if JAVA_SPEC_VERSION >= 11
/**
 * Get Module Name
 *
 * ***The caller must free the memory from this pointer if the return value is NOT the buffer argument ***
 *
 * @param[in] currentThread the current J9VMThread
 * @param[in] module the module
 * @param[in] buffer the buffer for the module name
 * @param[in] bufferLength the buffer length
 *
 * @return a char pointer to the module name
 */
static char *
getModuleNameUTF(J9VMThread *currentThread, j9object_t	moduleObject, char *buffer, UDATA bufferLength)
{
	J9JavaVM const * const vm = currentThread->javaVM;
	J9Module *module = J9OBJECT_ADDRESS_LOAD(currentThread, moduleObject, vm->modulePointerOffset);
	char *nameBuffer = NULL;

	if ((NULL == module) || (NULL == module->moduleName)) {
#define UNNAMED_MODULE "unnamed module "
		/* ensure bufferLength is not less than 128 which is enough for unnamed module */
		PORT_ACCESS_FROM_VMC(currentThread);
		Assert_VM_true(bufferLength >= 128);
		j9str_printf(PORTLIB, buffer, bufferLength, "%s0x%p", UNNAMED_MODULE, moduleObject);
		nameBuffer = buffer;
#undef UNNAMED_MODULE
	} else {
		nameBuffer = copyStringToUTF8WithMemAlloc(
			currentThread, module->moduleName, J9_STR_NULL_TERMINATE_RESULT, "", 0, buffer, bufferLength, NULL);
	}
	return nameBuffer;
}
#endif /* JAVA_SPEC_VERSION >= 11 */

/**
 * illegalModuleAccessMessage
 * Construct a detailed message regarding _possible_ illegal access exception.
 *
 * ***The caller must free the memory from this pointer after invoking setCurrentExceptionUTF() with this pointer***
 *
 * @param[in] currentThread the current J9VMThread
 * @param[in] badMemberModifier modifier of the inspected field or method. -1 if no valid member modifier is present
 * @param[in] senderClass the accessing class (accessing the module/class in question)
 * @param[in] targetClass the accessed class (containing the field/member in question)
 * @param[in] errorType, 0 if non-module access error, -1 if module read access error, -2 if module package access error
 *
 * @return a char pointer to the detailed message, or NULL if NativeOutOfMemoryError occurred which a JAVALANGILLEGALACCESSEXCEPTION will take precedence.
 */
char *
illegalAccessMessage(J9VMThread *currentThread, IDATA badMemberModifier, J9Class *senderClass, J9Class *targetClass, IDATA errorType)
{
	J9UTF8 *senderClassNameUTF = J9ROMCLASS_CLASSNAME(senderClass->romClass);
	J9UTF8 *targetClassNameUTF = J9ROMCLASS_CLASSNAME(targetClass->romClass);
	UDATA modifiers = 0;
	const char *modifierStr = NULL;
	char *buf = NULL;
	UDATA bufLen = 0;
	const char *errorMsg = NULL;
	char srcModuleBuf[J9VM_PACKAGE_NAME_BUFFER_LENGTH];
	char *srcModuleMsg = NULL;
	char destModuleBuf[J9VM_PACKAGE_NAME_BUFFER_LENGTH];
	char *destModuleMsg = NULL;
	char packageNameBuf[J9VM_PACKAGE_NAME_BUFFER_LENGTH];
	char *packageNameMsg = NULL;

	PORT_ACCESS_FROM_VMC(currentThread);
	Trc_VM_illegalAccessMessage_Entry(currentThread, J9UTF8_LENGTH(senderClassNameUTF), J9UTF8_DATA(senderClassNameUTF),
			J9UTF8_LENGTH(targetClassNameUTF), J9UTF8_DATA(targetClassNameUTF), badMemberModifier);

#if JAVA_SPEC_VERSION >= 11
	/* If an issue with the nest host loading and verification occurred, then
	 * it will be one of:
	 * 		J9_VISIBILITY_NEST_HOST_LOADING_FAILURE_ERROR
	 * 		J9_VISIBILITY_NEST_HOST_DIFFERENT_PACKAGE_ERROR
	 * 		J9_VISIBILITY_NEST_MEMBER_NOT_CLAIMED_ERROR
	 * Otherwise, it is one of:
	 * 		J9_VISIBILITY_NON_MODULE_ACCESS_ERROR
	 * 		J9_VISIBILITY_MODULE_READ_ACCESS_ERROR
	 * 		J9_VISIBILITY_MODULE_PACKAGE_EXPORT_ERROR
	 */
	if ((J9_VISIBILITY_NEST_HOST_LOADING_FAILURE_ERROR == errorType)
	|| (J9_VISIBILITY_NEST_HOST_DIFFERENT_PACKAGE_ERROR == errorType)
	|| (J9_VISIBILITY_NEST_MEMBER_NOT_CLAIMED_ERROR == errorType)
	) {
		J9Class *unverifiedNestMemberClass = NULL;
		J9ROMClass *romClass = NULL;
		J9UTF8 *nestMemberNameUTF = NULL;
		J9UTF8 *nestHostNameUTF = NULL;

		/*
		 * The specification asserts that, during access checking, the accessing
		 * class's nest host is loaded first and the class being accessed loads
		 * its nest host after.
		 * Access checking produces an IllegalAccessError if an exception is thrown
		 * when loading the nest host and the nest host field is not set. Therefore,
		 * the class which throws the nestmates related error can be determined by
		 * checking which class
		 */
		if (NULL == senderClass->nestHost) {
			unverifiedNestMemberClass = senderClass;
		} else {
			unverifiedNestMemberClass = targetClass;
		}

		romClass = unverifiedNestMemberClass->romClass;
		nestMemberNameUTF = J9ROMCLASS_CLASSNAME(romClass);
		nestHostNameUTF = J9ROMCLASS_NESTHOSTNAME(romClass);

		if (J9_VISIBILITY_NEST_HOST_LOADING_FAILURE_ERROR == errorType) {
			errorMsg = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
					J9NLS_VM_NEST_HOST_FAILED_TO_LOAD,
					NULL);
		} else if (J9_VISIBILITY_NEST_HOST_DIFFERENT_PACKAGE_ERROR == errorType) {
			errorMsg = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
					J9NLS_VM_NEST_HOST_HAS_DIFFERENT_PACKAGE,
					NULL);
		} else if (J9_VISIBILITY_NEST_MEMBER_NOT_CLAIMED_ERROR == errorType) {
			errorMsg = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
					J9NLS_VM_NEST_MEMBER_NOT_CLAIMED_BY_NEST_HOST,
					NULL);
		}

		bufLen = j9str_printf(PORTLIB, NULL, 0, errorMsg,
				J9UTF8_LENGTH(nestMemberNameUTF),
				J9UTF8_DATA(nestMemberNameUTF),
				J9UTF8_LENGTH(nestHostNameUTF),
				J9UTF8_DATA(nestHostNameUTF));

		if (bufLen > 0) {
			buf = j9mem_allocate_memory(bufLen, OMRMEM_CATEGORY_VM);
			if (NULL == buf) {
				goto allocationFailure;
			}
			j9str_printf(PORTLIB, buf, bufLen, errorMsg,
					J9UTF8_LENGTH(nestMemberNameUTF),
					J9UTF8_DATA(nestMemberNameUTF),
					J9UTF8_LENGTH(nestHostNameUTF),
					J9UTF8_DATA(nestHostNameUTF));
		}
	} else if (J9_VISIBILITY_NON_MODULE_ACCESS_ERROR != errorType) {
		/* illegal module access */
		j9object_t srcModuleObject = J9VMJAVALANGCLASS_MODULE(currentThread, senderClass->classObject);
		j9object_t destModuleObject = J9VMJAVALANGCLASS_MODULE(currentThread, targetClass->classObject);
		/* caller of illegalAccessMessage already performed Assert_VM_true((NULL != srcClassObject) && (NULL != destClassObject)); */

		srcModuleMsg = getModuleNameUTF(currentThread, srcModuleObject, srcModuleBuf, J9VM_PACKAGE_NAME_BUFFER_LENGTH);
		if (NULL == srcModuleMsg) {
			goto allocationFailure;
		}
		destModuleMsg = getModuleNameUTF(currentThread, destModuleObject, destModuleBuf, J9VM_PACKAGE_NAME_BUFFER_LENGTH);
		if (NULL == destModuleMsg) {
			goto allocationFailure;
		}
		if (J9_VISIBILITY_MODULE_READ_ACCESS_ERROR == errorType) {
			/* module read access NOT allowed */
			errorMsg = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_VM_ILLEGAL_ACCESS_MODULE_READ, NULL);
			bufLen = j9str_printf(PORTLIB, NULL, 0, errorMsg,
					J9UTF8_LENGTH(senderClassNameUTF),
					J9UTF8_DATA(senderClassNameUTF),
					srcModuleMsg,
					J9UTF8_LENGTH(targetClassNameUTF),
					J9UTF8_DATA(targetClassNameUTF),
					destModuleMsg);
			if (bufLen > 0) {
				buf = j9mem_allocate_memory(bufLen, OMRMEM_CATEGORY_VM);
				if (NULL != buf) {
					j9str_printf(PORTLIB, buf, bufLen, errorMsg,
							J9UTF8_LENGTH(senderClassNameUTF),
							J9UTF8_DATA(senderClassNameUTF),
							srcModuleMsg,
							J9UTF8_LENGTH(targetClassNameUTF),
							J9UTF8_DATA(targetClassNameUTF),
							destModuleMsg);
				} else {
					goto allocationFailure;
				}
			}
		} else {
			/* J9_VISIBILITY_MODULE_PACKAGE_EXPORT_ERROR == errorType */
			/* package NOT exported to srcModule */
			const char* packageName = NULL;
			UDATA packageNameLength = 0;
			J9PackageIDTableEntry entry = {0};

			entry.taggedROMClass = targetClass->packageID;
			packageName = (char*)getPackageName(&entry, &packageNameLength);
			if (J9VM_PACKAGE_NAME_BUFFER_LENGTH < packageNameLength) {
				packageNameMsg = j9mem_allocate_memory(packageNameLength + 1, OMRMEM_CATEGORY_VM);
				if (NULL == packageNameMsg) {
					goto allocationFailure;
				}
				memcpy(packageNameMsg, packageName, packageNameLength);
			} else {
				memcpy(packageNameBuf, packageName, packageNameLength);
				packageNameMsg = packageNameBuf;
			}
			*(packageNameMsg + packageNameLength) = '\0';
			errorMsg = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_VM_ILLEGAL_ACCESS_MODULE_EXPORT, NULL);
			bufLen = j9str_printf(PORTLIB, NULL, 0, errorMsg,
					J9UTF8_LENGTH(senderClassNameUTF),
					J9UTF8_DATA(senderClassNameUTF),
					srcModuleMsg,
					J9UTF8_LENGTH(targetClassNameUTF),
					J9UTF8_DATA(targetClassNameUTF),
					destModuleMsg,
					packageNameMsg);
			if (bufLen > 0) {
				buf = j9mem_allocate_memory(bufLen, OMRMEM_CATEGORY_VM);
				if (NULL != buf) {
					j9str_printf(PORTLIB, buf, bufLen, errorMsg,
							J9UTF8_LENGTH(senderClassNameUTF),
							J9UTF8_DATA(senderClassNameUTF),
							srcModuleMsg,
							J9UTF8_LENGTH(targetClassNameUTF),
							J9UTF8_DATA(targetClassNameUTF),
							destModuleMsg,
							packageNameMsg);
				} else {
					goto allocationFailure;
				}
			}
		}
	} else
#endif /* JAVA_SPEC_VERSION >= 11 */
	{
		/* illegal non-module access */
		if (badMemberModifier == -1) { /* visibility failed from Class level */
			errorMsg = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
					J9NLS_VM_ILLEGAL_ACCESS_CLASS, NULL);
			/* getting the appropriate modifiers dependent on weather the class in question is an innerClass or not */
			if (J9_ARE_ALL_BITS_SET(targetClass->romClass->extraModifiers, J9AccClassInnerClass))	{
				modifiers = targetClass->romClass->memberAccessFlags;
			} else {
				modifiers = targetClass->romClass->modifiers;
			}
		} else { /* visibility failed at field/method level */
			errorMsg = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
					J9NLS_VM_ILLEGAL_ACCESS_DETAIL, NULL);
			modifiers = badMemberModifier;
		}

		if (modifiers & (J9AccPublic | J9AccPrivate | J9AccProtected)) {
			if (modifiers & J9AccPublic) {
				modifierStr = "\"public\"";
			}
			if (modifiers & J9AccPrivate) {
				modifierStr = "\"private\"";
			}
			if (modifiers & J9AccProtected) {
				modifierStr = "\"protected\"";
			}
		} else { /* default package access */
			modifierStr = "\"package private\"";
		}

		/**
		 * errorMsg format:
		 * "Class %.*s illegally accessing %s member of class %.*s"
		 * first  is calling class
		 * second is member access flag
		 * third  is declaring class
		 */
		bufLen = j9str_printf(PORTLIB, NULL, 0, errorMsg,
				J9UTF8_LENGTH(senderClassNameUTF),
				J9UTF8_DATA(senderClassNameUTF),
				modifierStr,
				J9UTF8_LENGTH(targetClassNameUTF),
				J9UTF8_DATA(targetClassNameUTF));
		if (bufLen > 0) {
			buf = j9mem_allocate_memory(bufLen, OMRMEM_CATEGORY_VM);
			if (buf) {
				/* j9str_printf return value doesn't include the NULL terminator */
				j9str_printf(PORTLIB, buf, bufLen, errorMsg,
						J9UTF8_LENGTH(senderClassNameUTF),
						J9UTF8_DATA(senderClassNameUTF),
						modifierStr,
						J9UTF8_LENGTH(targetClassNameUTF),
						J9UTF8_DATA(targetClassNameUTF));
			} else {
				goto allocationFailure;
			}
		}
	}

exit:
	if ((NULL != srcModuleMsg) && (srcModuleMsg != srcModuleBuf)) {
		j9mem_free_memory(srcModuleMsg);
	}
	if ((NULL != destModuleMsg) && (destModuleMsg != destModuleBuf)) {
		j9mem_free_memory(destModuleMsg);
	}
	if ((NULL != packageNameMsg) && (packageNameMsg != packageNameBuf)) {
		j9mem_free_memory(packageNameMsg);
	}
	Trc_VM_illegalAccessMessage_Exit(currentThread, buf);
	return buf;

allocationFailure:
	Trc_VM_illegalAccessMessage_No_Alloc_Buf(currentThread);
	/* JAVALANGILLEGALACCESSEXCEPTION takes precedence over NativeOutOfMemoryError */
	goto exit;
}
