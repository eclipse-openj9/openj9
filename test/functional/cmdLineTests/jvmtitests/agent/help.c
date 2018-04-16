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


void
getHelp(char *h, int helpSize)
{
	int i;
	int testCount = getTestCount();
	jvmtiTest *t;

	sprintf(h, "JVMTI TestRunner\n\n");
	sprintf(h + strlen(h), "Usage:\n");
	sprintf(h + strlen(h), "\tjava -agentlib:jvmtitest=test:TESTNAME[,args:\"args\"] -cp TESTCP com.ibm.jvmti.tests.util.TestRunner\n\n");
	sprintf(h + strlen(h), "Test List:\n");

	for (i = 0; i < testCount; i++) {
		t = getTestAtIndex(i);
		sprintf(h + strlen(h), "\t%10s - %s\n", t->name, t->description);
	}

	return;
}



void
printHelp()
{
	char *h;

	h = malloc(16 * 1024);
	if (h) {
		getHelp(h, 16 * 1024);
		fprintf(stderr, "%s\n", h);
		free(h);
	} else {
		fprintf(stderr, "Error: unable to allocate memory to print help\n");
	}
}
