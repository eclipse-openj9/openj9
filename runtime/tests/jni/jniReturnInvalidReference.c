/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#include "jnitest_internal.h"

jobject JNICALL 
Java_j9vm_test_jnichk_ReturnInvalidReference_deletedLocalRef(JNIEnv * env, jobject caller)
{
	jobject ref = NULL;
	jobject ref2 = NULL;	

	ref = (*env)->NewLocalRef(env, caller);
	(*env)->DeleteLocalRef(env, ref);

	ref2 = (*env)->NewLocalRef(env, caller);
	(*env)->DeleteLocalRef(env, ref2);
	
	return ref;
}

jobject JNICALL 
Java_j9vm_test_jnichk_ReturnInvalidReference_validLocalRef(JNIEnv * env, jobject caller)
{
	jobject ref = NULL;
	jobject ref2 = NULL;	

	ref = (*env)->NewLocalRef(env, caller);

	ref2 = (*env)->NewLocalRef(env, caller);
	(*env)->DeleteLocalRef(env, ref2);
	
	return ref;
}

jobject JNICALL 
Java_j9vm_test_jnichk_ReturnInvalidReference_localRefFromPoppedFrame(JNIEnv * env, jobject caller)
{
	jobject ref = NULL;
	jobject result = NULL;	

	(*env)->PushLocalFrame(env, 5);
	ref = (*env)->NewLocalRef(env, caller);

	(*env)->PopLocalFrame(env, result);
		
	return ref;
}

jobject JNICALL 
Java_j9vm_test_jnichk_ReturnInvalidReference_explicitReturnOfNull(JNIEnv * env, jobject caller)
{
	return NULL;
}

jobject JNICALL 
Java_j9vm_test_jnichk_ReturnInvalidReference_deletedGlobalRef(JNIEnv * env, jobject caller)
{
	jobject ref = NULL;
	jobject ref2 = NULL;

	ref = (*env)->NewGlobalRef(env, caller);
	ref2 = (*env)->NewGlobalRef(env, caller);
	
	(*env)->DeleteGlobalRef(env, ref);
	(*env)->DeleteGlobalRef(env, ref2);
	
	return ref;
}

jobject JNICALL 
Java_j9vm_test_jnichk_ReturnInvalidReference_validGlobalRef(JNIEnv * env, jobject caller)
{
	jobject ref = NULL;
	jobject ref2 = NULL;

	ref = (*env)->NewGlobalRef(env, caller);
	ref2 = (*env)->NewGlobalRef(env, caller);
	
	(*env)->DeleteGlobalRef(env, ref2);
	
	return ref;
}
