/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
#include <stdlib.h>

#include "jvmti_test.h"


static void
processToken(agentEnv * env, char * token)
{
	IDATA tokenLen = strlen(token);

	if (!strncmp("test:", token, 5)) {
		if (tokenLen > 5) {
			env->testName = strdup(token + 5);
		}
		return;
	}

	if (!strncmp("args:", token, 5)) {
		if (tokenLen > 5) {
			env->testArgs = strdup(token + 5);
		}
		return;
	}

	if (!strncmp("outputLevel:", token, 12)) {
		if (tokenLen > 12) {
			env->outputLevel = atoi(token + 12);
		}
		return;
	}


	if (!strncmp("subtest:", token, 8)) {
		if (tokenLen > 8) {
			env->subtest = strdup(token + 8);
		}
		return;
	}

	return;
}

void
parseArguments(agentEnv * env, char * agentOptions)
{
	char *str, *token;

	str = strdup(agentOptions);

	freeArguments(env);

	token = strtok(str, ",");
	while (token) {
		processToken(env, token);
		token = strtok(NULL, ",");
	}

	free(str);
}

void
freeArguments(agentEnv * env)
{
	if (env->testName) {
		free(env->testName);
		env->testName = NULL;
	}
	if (env->testArgs) {
		free(env->testArgs);
		env->testArgs = NULL;
	}
	if (env->subtest) {
		free(env->subtest);
		env->subtest = NULL;
	}
}


