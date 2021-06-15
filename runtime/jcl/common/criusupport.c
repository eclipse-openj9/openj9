/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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
#if defined(LINUX)
#include <criu/criu.h>
#endif /* defined(LINUX) */

#include "jni.h"
#include "j9.h"
#include "jclprots.h"

jboolean JNICALL
Java_com_ibm_jvm_CRIUSupport_isCRIUSupportEnabledImpl(JNIEnv *env, jclass unused)
{
	J9VMThread *currentThread = (J9VMThread *) env;
	J9JavaVM *vm = currentThread->javaVM;
	jboolean res = JNI_FALSE;

	if (J9_ARE_ALL_BITS_SET(vm->extendedRuntimeFlags2, J9_EXTENDED_RUNTIME2_ENABLE_CRIU_SUPPORT)) {
#if defined(LINUX)
		if (0 == criu_init_opts()) {
			res = JNI_TRUE;
		}
#endif /* defined(LINUX) */
	}

	return res;
}
