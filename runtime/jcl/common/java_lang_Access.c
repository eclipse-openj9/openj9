/*******************************************************************************
 * Copyright (c) 1998, 2014 IBM Corp. and others
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

#include "jni.h"
#include "jcl.h"
#include "jclprots.h"
#include "jclglob.h"
#include "jniidcacheinit.h"

/**
 * Return the constant pool for a class.
 * Note: This method is also called from annparser.c:Java_com_ibm_oti_reflect_AnnotationParser_getConstantPool.
 *
 * @param env				The current thread.
 * @param unusedClass		See note above - this parameter should not be used.
 * @param classToIntrospect	The Class instance the ConstantPool should represent.
 *
 * @return a ConstantPool object reference or NULL on error.
 */
jobject JNICALL 
Java_java_lang_Access_getConstantPool(JNIEnv *env, jclass unusedClass, jobject classToIntrospect)
{
	jclass sun_reflect_ConstantPool = JCL_CACHE_GET(env,CLS_sun_reflect_ConstantPool);
	jfieldID constantPoolOop;
	jobject constantPool;

	/* lazy-initialize the cached field IDs */
	if (NULL == sun_reflect_ConstantPool) {
		if (!initializeSunReflectConstantPoolIDCache(env)) {
			return NULL;
		}
		sun_reflect_ConstantPool = JCL_CACHE_GET(env, CLS_sun_reflect_ConstantPool);
	}
	
	/* allocate the new ConstantPool object */
	constantPool = (*env)->AllocObject(env, sun_reflect_ConstantPool);
	if (NULL == constantPool) {
		return NULL;
	}

	/* and set the private constantPoolOop member */
	constantPoolOop = JCL_CACHE_GET(env, FID_sun_reflect_ConstantPool_constantPoolOop);
	(*env)->SetObjectField(env, constantPool, constantPoolOop, classToIntrospect);
	
	return constantPool;
}
