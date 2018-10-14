/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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

#include <string.h>
#include <stdlib.h>

#include "j9.h"
#include "jcl.h"
#include "j9consts.h"
#include "omrgcconsts.h"
#include "jni.h"
#include "j9protos.h"
#include "jclprots.h"
#include "ut_j9jcl.h"

#include "rommeth.h"
#include "j9vmnls.h"
#include "j9vmconstantpool.h"


/*
 * Native method that creates a copy of the object in old-space so that the ConstantPool entries do not
 * have to be walked by the gencon gc policy.
 *
 * This treats interned MethodTypes the same way String constants in the ConstantPool are treated.
 */
jobject JNICALL
Java_java_lang_invoke_MethodType_makeTenured(JNIEnv *env, jclass clazz, jobject receiverObject)
{
	J9VMThread *vmThread = (J9VMThread *) env;
	J9JavaVM const *vm = vmThread->javaVM;
	J9InternalVMFunctions const *vmFuncs = vm->internalVMFunctions;
	J9MemoryManagerFunctions const *gcFuncs = vm->memoryManagerFunctions;
	const UDATA allocateFlags = J9_GC_ALLOCATE_OBJECT_TENURED | J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE;
	j9object_t methodType;
	jobject methodTypeRef = NULL;

	vmFuncs->internalEnterVMFromJNI(vmThread);

	methodType = gcFuncs->j9gc_objaccess_asConstantPoolObject(
								vmThread,
								J9_JNI_UNWRAP_REFERENCE(receiverObject),
								allocateFlags);
	if (methodType == NULL) {
		vmFuncs->setHeapOutOfMemoryError(vmThread);
	} else {
		methodTypeRef = vmFuncs->j9jni_createLocalRef(env, methodType);
	}
	vmFuncs->internalExitVMToJNI(vmThread);
	return methodTypeRef;
}
