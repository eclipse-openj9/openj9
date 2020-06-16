/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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

#include "jvmti_test.h"

const static char *versionNames[] =
{
	"JVMTI 1.0",
	"JVMTI 1.1",
	"JVMTI 1.2",
	"JVMTI 9.0",
	"JVMTI 11",
	"unknown"
};


jboolean
ensureVersion(agentEnv * agent_env, jint version)
{

	if ((agent_env->jvmtiVersion & ~JVMTI_VERSION_MASK_MICRO) == version) {
		const char *versionName = getVersionName(agent_env, version);

		error(agent_env, JVMTI_ERROR_UNSUPPORTED_VERSION, "Test requires %s",
				versionName ? versionName : "Unknown Version");

		return JNI_FALSE;
	}

	return JNI_TRUE;
}

const char *
getVersionName(agentEnv * agent_env, jint version)
{
	switch ((version & ~JVMTI_VERSION_MASK_MICRO)) {
		case JVMTI_VERSION_1_0:
			return versionNames[0];
		case JVMTI_VERSION_1_1:
			return versionNames[1];
		case JVMTI_VERSION_1_2:
			return versionNames[2];
		case JVMTI_VERSION_9:
			return versionNames[3];
		case JVMTI_VERSION_11:
			return versionNames[4];
		default:
			error(agent_env, JVMTI_ERROR_UNSUPPORTED_VERSION, "Query for an unknown version");
	}

	return NULL;
}
