
/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#include "CollectorLanguageInterfaceImpl.hpp"

#include "j9nongenerated.h"
#include "mmhook.h"
#include "mmomrhook_internal.h"
#include "mmprivatehook.h"
#include "mmprivatehook_internal.h"

#include "Collector.hpp"
#include "ConfigurationDelegate.hpp"
#include "GCExtensions.hpp"

class MM_AllocationContext;

/**
 * Initialization
 */
MM_CollectorLanguageInterfaceImpl *
MM_CollectorLanguageInterfaceImpl::newInstance(MM_EnvironmentBase *env)
{
	MM_CollectorLanguageInterfaceImpl *cliJava = NULL;

	cliJava = (MM_CollectorLanguageInterfaceImpl *)env->getForge()->allocate(sizeof(MM_CollectorLanguageInterfaceImpl), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL != cliJava) {
		new(cliJava) MM_CollectorLanguageInterfaceImpl((J9JavaVM*)env->getLanguageVM());
		if (!cliJava->initialize(env)) {
			cliJava->kill(env);
			cliJava = NULL;
		}
	}

	return cliJava;
}

void
MM_CollectorLanguageInterfaceImpl::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

/**
 * Initialize the collector's internal structures and values.
 * @return true if initialization completed, false otherwise
 */
bool
MM_CollectorLanguageInterfaceImpl::initialize(MM_EnvironmentBase *env)
{
	return true;
}

/**
 * Free any internal structures associated to the receiver.
 */
void
MM_CollectorLanguageInterfaceImpl::tearDown(MM_EnvironmentBase *env)
{
}

