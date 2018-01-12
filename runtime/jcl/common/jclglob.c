/*******************************************************************************
 * Copyright (c) 1998, 2016 IBM Corp. and others
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
#include "jcl.h"

#include "jclglob.h"
#include "jcltrace.h"
#if defined(J9VM_PORT_OMRSIG_SUPPORT)
#include "omrsig.h"
#endif /* defined(J9VM_PORT_OMRSIG_SUPPORT) */

static UDATA keyInitCount = 0;

void * JCL_ID_CACHE = NULL;



jint handleOnLoadEvent(JavaVM * vm);
void *jclCachePointer( JNIEnv *env );
void freeHack(JNIEnv * env);

/**
 * @internal
 */
void freeHack(JNIEnv * env)
{
		jclass classRef;

		/* clean up class references */
		/* PR 116213 - remove jnicheck warnings */

		classRef = JCL_CACHE_GET(env, CLS_java_lang_String);
		if (classRef) (*env)->DeleteGlobalRef(env, (jweak) classRef);

		classRef = JCL_CACHE_GET(env, CLS_java_lang_reflect_Parameter);
		if (classRef) (*env)->DeleteGlobalRef(env, (jweak) classRef);

}


/**
 * This DLL is being loaded, do any initialization required.
 * This may be called more than once.
 */
jint JNICALL JCL_OnLoad(JavaVM * vm, void *reserved)
{
	return handleOnLoadEvent(vm);
}


/**
 * This DLL is being unloaded, do any clean up required.
 * This may be called more than once!!
 */
void JNICALL
JNI_OnUnload(JavaVM * vm, void *reserved)
{
	JNIEnv *env;
	void *keyInitCountPtr = GLOBAL_DATA( keyInitCount );
	void **jclIdCache = GLOBAL_DATA( JCL_ID_CACHE);

	if ((*vm)->GetEnv(vm, (void **) &env, JNI_VERSION_1_2) == JNI_OK) {
		JniIDCache *idCache = (JniIDCache *) J9VMLS_GET(env, *jclIdCache);

		if (idCache) {
			PORT_ACCESS_FROM_ENV(env);

			/* Had to move some of the code out of this function to get the IBM C compiler to generate a valid DLL */
			freeHack(env);

			/* Free VMLS keys */
			idCache = (JniIDCache *) J9VMLS_GET(env, *jclIdCache);
			terminateTrace(env);

			J9VMLS_FNTBL(env)->J9VMLSFreeKeys(env, keyInitCountPtr, jclIdCache, NULL);
			j9mem_free_memory(idCache);
		}
	}
}


/**
 * @internal
 * This DLL is being loaded, do any initialization required.
 * This may be called more than once.
 */
jint handleOnLoadEvent(JavaVM * vm)
{
	JniIDCache *idCache;
	JNIEnv *env;
	void *keyInitCountPtr = GLOBAL_DATA( keyInitCount );
	void **jclIdCache = GLOBAL_DATA( JCL_ID_CACHE );

	if ((*vm)->GetEnv(vm, (void **) &env, JNI_VERSION_1_2) == JNI_OK) {
		PORT_ACCESS_FROM_ENV(env);

		if (J9VMLS_FNTBL(env)->J9VMLSAllocKeys(env, keyInitCountPtr, jclIdCache, NULL)) {
			goto fail;
		}

		idCache = (JniIDCache *) j9mem_allocate_memory(sizeof(JniIDCache), J9MEM_CATEGORY_VM_JCL);
		if (!idCache)
			goto fail2;

		memset(idCache, 0, sizeof(JniIDCache));
		J9VMLS_SET(env, *jclIdCache, idCache);

		return JNI_VERSION_1_2;
	}

  fail2:
	J9VMLS_FNTBL(env)->J9VMLSFreeKeys(env, keyInitCountPtr, jclIdCache, NULL);
  fail:
	return 0;
}
