
/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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


#if !defined(RUNTIMEEXECMANAGER_HPP_)
#define RUNTIMEEXECMANAGER_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "j9protos.h"
#include "j9consts.h"
#include "jni.h"
#include "modronopt.h"

#include "BaseNonVirtual.hpp"

class MM_EnvironmentBase;

/**
 * Due to a limitation of the way Linux supports NUMA we need to intercept Runtime.exec() to ensure that 
 * child processes don't inherit the NUMA binding of the thread which called Runtime.exec(). We do this
 * by temporarily deaffinitizing the thread while we run the native function.
 * 
 * Since this is only necessary on Linux the body of this class is basically empty on other platforms.
 */
class MM_RuntimeExecManager : public MM_BaseNonVirtual
{
/* Data members & types */
public:
protected:
private:
#if defined(LINUX) && !defined(J9ZTPF)
	void* _savedForkAndExecNative; /**< The original native function for forkAndExec, saved here when we replace it with our own wrapper */
#endif /* defined (LINUX) && !defined (J9ZTPF) */

/* Methods */
public:
	/**
	 * Initialize the instance
	 * @param env[in] the current thread
	 * @return true on success, false on failure
	 */
	bool initialize(MM_EnvironmentBase *env);
	
	/**
	 * Tear down the instance
	 * @param env[in] the current thread
	 */
	void tearDown(MM_EnvironmentBase *env);

	/**
	 * Construct the instance
	 * @param env[in] the current thread
	 */
	MM_RuntimeExecManager(MM_EnvironmentBase *env);
	
private:
	
#if defined (LINUX) && !defined(J9ZTPF)
	/**
	 * Listen to native bind events in order to intercept java/lang/UNIXProcess.forkAndExec()
	 */
	static void jniNativeBindHook(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);

	/**
	 * Java 6 wrapper for forkAndExec native function
	 */
	static jint JNICALL forkAndExecNativeV6(JNIEnv* env, jobject receiver, jobject arg1, jobject arg2, jint arg3, jobject arg4, jint arg5, jobject arg6, jboolean arg7, jobject arg8, jobject arg9, jobject arg10);

	/**
	 * Java 7 wrapper for forkAndExec native function
	 */
	static jint JNICALL forkAndExecNativeV7(JNIEnv* env, jobject receiver, jobject arg1, jobject arg2, jint arg3, jobject arg4, jint arg5, jobject arg6, jobject arg7, jboolean arg8);

	/**
	 * Java 8 wrapper for forkAndExec native function.
	 * Also used by Oracle 7u55-b08 and newer
	 */
	static jint JNICALL forkAndExecNativeV8(JNIEnv* jniEnv, jobject receiver, jint arg1, jobject arg2, jobject arg3, jobject arg4, jint arg5, jobject arg6, jint arg7, jobject arg8, jobject arg9, jboolean arg10);
#endif /* defined (LINUX) && !defined (J9ZTPF) */
};

#endif /* RUNTIMEEXECMANAGER_HPP_ */
