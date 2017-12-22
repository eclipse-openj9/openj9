
/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#include "GCExtensions.hpp"
#include "VerboseEvent.hpp"
#include "VerboseEventStream.hpp"
#include "VerboseOutputAgent.hpp"

/**
 * Kill the instance.
 * Tears down the related structures and frees any storage.
 */
void
MM_VerboseOutputAgent::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env->getOmrVM());
	extensions->getForge()->free(this);
}

void
MM_VerboseOutputAgent::tearDown(MM_EnvironmentBase *env)
{
}

/**
 * Routine that passes the agent to the formatted output routine in each event in the event stream.
 * @param eventStream The event stream.
 */
void
MM_VerboseOutputAgent::processEventStream(MM_EnvironmentBase *env, MM_VerboseEventStream *eventStream)
{
	MM_VerboseEvent *event = eventStream->getHead();
	
	while(NULL != event) {
		event->formattedOutput(this);
		event = event->getNextEvent();
	}
}
