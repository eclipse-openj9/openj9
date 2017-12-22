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

#include "jcl.h"
#include <ctype.h>
#include <string.h>
#include "jclglob.h"
#include "iohelp.h"
#include "util_api.h"


jbyteArray JNICALL Java_com_ibm_oti_vm_VM_getPathFromClassPath(JNIEnv * env, jclass clazz, jint cpIndex)
{
#if defined(J9VM_OPT_DYNAMIC_LOAD_SUPPORT)
	J9JavaVM *javaVM;
	J9ClassPathEntry cpEntry;
	J9ClassLoader *classLoader;
	jbyteArray jbytes;
	jsize length;

	/* Check the parameters */
	javaVM = ((J9VMThread *) env)->javaVM;
	classLoader = (J9ClassLoader*)javaVM->systemClassLoader;

	/* Sniff the cpEntry */
	if (getClassPathEntry((J9VMThread *)env, classLoader, cpIndex, &cpEntry)) {
		return NULL;
	}
	if(cpEntry.type == CPE_TYPE_DIRECTORY || cpEntry.type == CPE_TYPE_JAR)
	{
		length = (jsize)cpEntry.pathLength;
		if (cpEntry.type == CPE_TYPE_DIRECTORY &&
			cpEntry.path[length-1] != '/' && cpEntry.path[length-1] != '\\')
				length++;
		jbytes = (*env)->NewByteArray(env, length);
		if (jbytes != NULL) {
			(*env)->SetByteArrayRegion(env, jbytes, 0, (jint)cpEntry.pathLength, (jbyte *) cpEntry.path);
			if (length != (jsize)cpEntry.pathLength)
				(*env)->SetByteArrayRegion(env, jbytes, length-1, 1, (jbyte *) DIR_SEPARATOR_STR);
		}
		return jbytes;
	}

	/* Not a directory. Return NULL; */
	return NULL;

#else
	/* No dynamic loader. */
	return NULL;
#endif
}
