
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

/**
 * @file
 * @ingroup GC_Modron_Metronome
 */

#include "j9.h"
#include "j9cfg.h"

#include "ConfigurationStaccato.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "StaccatoGC.hpp"


MM_Configuration *
MM_ConfigurationStaccato::newInstance(MM_EnvironmentBase *env)
{
	MM_ConfigurationStaccato *configuration;
	
	configuration = (MM_ConfigurationStaccato *) env->getForge()->allocate(sizeof(MM_ConfigurationStaccato), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if(NULL != configuration) {
		new(configuration) MM_ConfigurationStaccato(env);
		if(!configuration->initialize(env)) {
			configuration->kill(env);
			configuration = NULL;
		}
	}
	return configuration;
}

MM_GlobalCollector *
MM_ConfigurationStaccato::createGlobalCollector(MM_EnvironmentBase *env)
{
	return MM_StaccatoGC::newInstance(env);
}
