/*******************************************************************************
 * Copyright (c) 1998, 2018 IBM Corp. and others
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
#include "j9.h"
#include "mgmtinit.h"
#include "j9port.h"
#include "jclglob.h"

/**
 * Makes the Port Library function j9sysinfo_get_number_CPUs_by_type(type) available
 * for JCL. The valid types are (defined in j9port.h):
 *  - PHYSICAL: Number of physical CPU's on this platform
 * 	- BOUND: Number of physical CPU's bound to this process
 * 	- TARGET: Number of CPU's that should be used by the process. This is normally BOUND, but is overridden by ActiveCPUs if set.
 *
 * @param[in] type Flag to indicate the information type (see function description).
 *
 * @return The number of CPUs, qualified by the argument <code>type</code>.
 */
jint JNICALL
Java_com_ibm_lang_management_internal_ProcessorMXBeanImpl_getNumberCPUsImpl(JNIEnv *env, jobject o, jint type)
{
	jint toReturn = 0;
	PORT_ACCESS_FROM_ENV(env);
	toReturn = (jint) j9sysinfo_get_number_CPUs_by_type((UDATA) type);
	return toReturn;
}

/**
 * Makes the Port Library function j9sysinfo_set_number_user_specified_CPUs(number) available
 * for JCL.
 *
 * @param[in] number Number of user-specified active CPUs (non-negative).
 */
void JNICALL
Java_com_ibm_lang_management_internal_ProcessorMXBeanImpl_setNumberActiveCPUsImpl(JNIEnv *env, jobject o, jint number)
{
	PORT_ACCESS_FROM_ENV(env);
	j9sysinfo_set_number_user_specified_CPUs((UDATA) number);
	return;
}
