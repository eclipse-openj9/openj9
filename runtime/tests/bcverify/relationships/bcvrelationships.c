/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#include <jni.h>
#include "j9protos.h"
#include "omrlinkedlist.h"

/**
 * Check if a relationship has been recorded in the
 * classloader relationship table for the specified
 * child class and parent class.
 *
 * Class: org_openj9_test_classRelationshipVerifier_TestClassRelationshipVerifier
 * Method: isRelationshipRecorded
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;)Z
 *
 * @param[in] env the JNI interface pointer
 * @param[in] clazz the class on which the static method was invoked
 * @param[in] childNameString the name of the child class
 * @param[in] parentNameString the name of the parent class
 * @param[in] classLoaderObject the class loader used to load the parent and child classes
 */
JNIEXPORT jboolean JNICALL
Java_org_openj9_test_classRelationshipVerifier_TestClassRelationshipVerifier_isRelationshipRecorded(JNIEnv *env, jclass clazz, jstring childNameString, jstring parentNameString, jobject classLoaderObject)
{
	jboolean isRelationshipRecorded = JNI_FALSE;
	J9VMThread *currentThread  = (J9VMThread *) env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	J9ClassLoader *classLoader = NULL;
	U_8 *childName = NULL;
	U_8 *parentName = NULL;
	char childNameStackBuffer[J9VM_PACKAGE_NAME_BUFFER_LENGTH] = {0};
	char parentNameStackBuffer[J9VM_PACKAGE_NAME_BUFFER_LENGTH] = {0};
	PORT_ACCESS_FROM_JAVAVM(vm);

	if ((NULL == childNameString) || (NULL == parentNameString)) {
		goto done;
	}

	vmFuncs->internalEnterVMFromJNI(currentThread);
	classLoader = J9VMJAVALANGCLASSLOADER_VMREF(currentThread, J9_JNI_UNWRAP_REFERENCE(classLoaderObject));

	if (NULL != classLoader) {
		if (NULL != classLoader->classRelationshipsHashTable) {
			UDATA childNameLength = 0;
			childName = (U_8 *) vmFuncs->copyStringToUTF8WithMemAlloc(currentThread, J9_JNI_UNWRAP_REFERENCE(childNameString), J9_STR_NULL_TERMINATE_RESULT | J9_STR_XLAT, "", 0, childNameStackBuffer, J9VM_PACKAGE_NAME_BUFFER_LENGTH, &childNameLength);

			if (NULL != childName) {
				J9ClassRelationship exemplar = {0};
				J9ClassRelationship *childEntry = NULL;

				exemplar.className = childName;
				exemplar.classNameLength = childNameLength;
				childEntry = hashTableFind(classLoader->classRelationshipsHashTable, &exemplar);

				if (NULL != childEntry) {
					UDATA parentNameLength = 0;
					parentName = (U_8 *) vmFuncs->copyStringToUTF8WithMemAlloc(currentThread, J9_JNI_UNWRAP_REFERENCE(parentNameString), J9_STR_NULL_TERMINATE_RESULT | J9_STR_XLAT, "", 0, parentNameStackBuffer, J9VM_PACKAGE_NAME_BUFFER_LENGTH, &parentNameLength);

					if (NULL != parentName) {
						J9ClassRelationshipNode *currentNode = J9_LINKED_LIST_START_DO(childEntry->root);

						while (NULL != currentNode) {
							if (J9UTF8_DATA_EQUALS(currentNode->className, currentNode->classNameLength, parentName, parentNameLength)) {
								isRelationshipRecorded = JNI_TRUE;
								break;
							}
							currentNode = J9_LINKED_LIST_NEXT_DO(childEntry->root, currentNode);
						}
					} else {
						vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
					}
				}
			} else {
				vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
			}
		}
	}

done:
	vmFuncs->internalExitVMToJNI(currentThread);

	if ((U_8*) childNameStackBuffer != childName) {
		j9mem_free_memory(childName);
	}

	if ((U_8*) parentNameStackBuffer != parentName) {
		j9mem_free_memory(parentName);
	}

	return isRelationshipRecorded;
}
