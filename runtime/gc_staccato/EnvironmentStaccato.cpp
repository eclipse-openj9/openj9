
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "j9.h"
#include "j9cfg.h"

#include "EnvironmentStaccato.hpp"
#include "GCExtensions.hpp"


MM_EnvironmentStaccato *
MM_EnvironmentStaccato::newInstance(MM_GCExtensionsBase *extensions, OMR_VMThread *omrVMThread)
{
	void *envPtr;
	MM_EnvironmentStaccato *env = NULL;
	
	envPtr = (void *)pool_newElement(extensions->environments);
	if(envPtr) {
		env = new(envPtr) MM_EnvironmentStaccato(omrVMThread);
		if (!env->initialize(extensions)) {
			env->kill();
			env = NULL;	
		}
	}	

	return env;
}

void
MM_EnvironmentStaccato::kill()
{
	MM_EnvironmentBase::kill();
}

bool
MM_EnvironmentStaccato::initialize(MM_GCExtensionsBase *extensions )
{
	/* initialize super class */
	if(!MM_EnvironmentRealtime::initialize(extensions)) {
		return false;
	}
	
	return true;
}	

void
MM_EnvironmentStaccato::tearDown(MM_GCExtensionsBase *extensions)
{	
	/* tearDown base class */
	MM_EnvironmentRealtime::tearDown(extensions);
}
