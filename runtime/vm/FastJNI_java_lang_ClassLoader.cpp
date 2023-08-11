/*******************************************************************************
 * Copyright IBM Corp. and others 2001
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

#include "fastJNI.h"

#include "j9protos.h"
#include "j9consts.h"

extern "C" {

/* java.lang.ClassLoader: private native Class findLoadedClassImpl(String className); */
j9object_t JNICALL
Fast_java_lang_Classloader_findLoadedClassImpl(J9VMThread *currentThread, j9object_t classloaderObject, j9object_t className)
{
	j9object_t resultObject = NULL;
	if (NULL != className) {
		J9ClassLoader *loader = J9VMJAVALANGCLASSLOADER_VMREF(currentThread, classloaderObject);
		if (NULL != loader) {
			J9Class *j9Class = internalFindClassString(currentThread, NULL, className, loader,
														J9_FINDCLASS_FLAG_EXISTING_ONLY,
														CLASSNAME_VALID);
			/* macro handles NULL */
			resultObject = J9VM_J9CLASS_TO_HEAPCLASS(j9Class);
		}
	}
	return resultObject;
}

J9_FAST_JNI_METHOD_TABLE(java_lang_ClassLoader)
	J9_FAST_JNI_METHOD("findLoadedClassImpl", "(Ljava/lang/String;)Ljava/lang/Class;", Fast_java_lang_Classloader_findLoadedClassImpl,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS)
J9_FAST_JNI_METHOD_TABLE_END

}
