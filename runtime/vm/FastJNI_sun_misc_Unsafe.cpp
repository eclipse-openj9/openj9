/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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

#include "fastJNI.h"

#include "j9protos.h"
#include "j9consts.h"

extern "C" {

/* sun.misc.Unsafe: public native void park(boolean isAbsolute, long time); */
void JNICALL
Fast_sun_misc_Unsafe_park(J9VMThread *currentThread, jboolean isAbsolute, jlong time)
{
	threadParkImpl(currentThread, (IDATA)isAbsolute, (I_64)time);
}

/* sun.misc.Unsafe: public native void unpark(java.lang.Object thread); */
void JNICALL
Fast_sun_misc_Unsafe_unpark(J9VMThread *currentThread, j9object_t threadObject)
{
	threadUnparkImpl(currentThread, threadObject);
}

J9_FAST_JNI_METHOD_TABLE(sun_misc_Unsafe)
	J9_FAST_JNI_METHOD("park", "(ZJ)V", Fast_sun_misc_Unsafe_park,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
	J9_FAST_JNI_METHOD("unpark", "(Ljava/lang/Object;)V", Fast_sun_misc_Unsafe_unpark,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
J9_FAST_JNI_METHOD_TABLE_END

}
