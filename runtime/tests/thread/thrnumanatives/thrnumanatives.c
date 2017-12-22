/*******************************************************************************
 * Copyright (c) 2010, 2016 IBM Corp. and others
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

#include "j9cfg.h"
#include "j9comp.h"
#include "j9port.h"
#include "omrthread.h"
#include "j9.h"
#include "vm_api.h"

/**
 * Native used to determine how many NUMA nodes are available
 *
 * @param env the JNIEnv
 * @param clazz the class on which the static method was invoked
 * @param processName a string used to describe this process
 * @return the number of nodes with computational resources 
 */
jint JNICALL
Java_com_ibm_j9_numa_NumaTest_getUsableNodeCount(JNIEnv* env, jclass clazz, jstring processName)
{
	J9VMThread* vmThread = (J9VMThread*)env;
	JavaVM* javaVM = (JavaVM*)vmThread->javaVM;
	PORT_ACCESS_FROM_VMC(vmThread);
	J9ThreadEnv* threadEnv;
	J9MemoryNodeDetail details[64];
	UDATA detailCount = sizeof(details) / sizeof(details[0]);
	IDATA port_rc = 0;
	UDATA usableNodes = 0;
	const char * description = (*env)->GetStringUTFChars(env, processName, NULL);

	(*javaVM)->GetEnv(javaVM, (void**)&threadEnv, J9THREAD_VERSION_1_1);

	port_rc = j9vmem_numa_get_node_details(details, &detailCount);
	if (0 == port_rc) {
		UDATA i = 0;
		j9tty_printf(PORTLIB, "%s: j9vmem_numa_get_node_details() = %zi; %zu nodes\n", description, port_rc, detailCount);
		for (i = 0; i < detailCount; i++) {
			j9tty_printf(PORTLIB, "%s: node[%zu].computationalResourcesAvailable = %zu\n", description, details[i].j9NodeNumber, details[i].computationalResourcesAvailable);
			if (0 < details[i].computationalResourcesAvailable) {
				usableNodes += 1;
			}
		}
	} else {
		j9tty_printf(PORTLIB, "j9vmem_numa_get_node_details() not supported on this machine\n");
	}
	
	j9tty_printf(PORTLIB, "%s: omrthread_get_priority() = %zu\n", description, threadEnv->get_priority(threadEnv->self()));

	(*env)->ReleaseStringUTFChars(env, processName, description);
	
	return (jint)usableNodes;	
}
