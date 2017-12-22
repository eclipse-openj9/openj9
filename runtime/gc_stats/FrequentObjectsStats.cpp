
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
#include "FrequentObjectsStats.hpp"

#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "ModronAssertions.h"

/**
 * Create and return a new instance of MM_FrequentObjectsStats.
 *
 * @return the new instance, or NULL on failure.
 */
MM_FrequentObjectsStats *
MM_FrequentObjectsStats::newInstance(MM_EnvironmentBase *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	MM_FrequentObjectsStats *frequentObjectsStats;
	U_32 frequentObjectAllocationSamplingDepth = MM_GCExtensions::getExtensions(env)->frequentObjectAllocationSamplingDepth;

	frequentObjectsStats = (MM_FrequentObjectsStats *)env->getForge()->allocate(sizeof(MM_FrequentObjectsStats), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if(NULL != frequentObjectsStats) {
		if (0 != frequentObjectAllocationSamplingDepth) {
			new(frequentObjectsStats) MM_FrequentObjectsStats(PORTLIB, frequentObjectAllocationSamplingDepth);
		} else {
			new(frequentObjectsStats) MM_FrequentObjectsStats(PORTLIB);
		}
		if(!frequentObjectsStats->initialize(env)) {
			frequentObjectsStats->kill(env);
			return NULL;
		}
	}
	return frequentObjectsStats;
}


bool
MM_FrequentObjectsStats::initialize(MM_EnvironmentBase *env)
{
	_spaceSaving = spaceSavingNew(OMRPORT_FROM_J9PORT(_portLibrary), getSizeForTopKFrequent(_topKFrequent));

	return (NULL != _spaceSaving);
}

void
MM_FrequentObjectsStats::tearDown(MM_EnvironmentBase *env)
{
	if(NULL != _spaceSaving){
		spaceSavingFree(_spaceSaving);
	}
}


void
MM_FrequentObjectsStats::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

void
MM_FrequentObjectsStats::traceStats(MM_EnvironmentBase *env)
{
	UDATA i;
	J9VMThread *vmThread = (J9VMThread *)env->getOmrVMThread()->_language_vmthread;
	MM_GCExtensionsBase *extensions = env->getExtensions();
	/* Should convert easily as frequentObjectAllocationSamplingRate is a UDATA representing a percentage*/
	float sampleFreq = 100/((float) extensions->frequentObjectAllocationSamplingRate);

	for(i=0; i < spaceSavingGetCurSize(_spaceSaving) && i < _topKFrequent; i++){
		J9Class * clazz = (J9Class *) spaceSavingGetKthMostFreq(_spaceSaving,i+1);
		UDATA count = spaceSavingGetKthMostFreqCount(_spaceSaving,i+1);

		if(J9ROMCLASS_IS_ARRAY(clazz->romClass)){
			J9ArrayClass* arrayClass = (J9ArrayClass*) clazz;
			UDATA arity = arrayClass->arity;
			J9UTF8* utf;

			/* Max arity is 255, so define a bracket array of size 255*2 */
			static const char * brackets =
				"[][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][]"
				"[][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][]"
				"[][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][]"
				"[][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][][]";


			utf = J9ROMCLASS_CLASSNAME(arrayClass->leafComponentType->romClass);
			Trc_MM_FrequentObjectStats_AllocationCacheIndexableObjectAllocation(
					vmThread, clazz, J9UTF8_LENGTH(utf), J9UTF8_DATA(utf), arity*2, brackets, count, (UDATA) (((float)count)*sampleFreq));
		}else{
			Trc_MM_FrequentObjectStats_AllocationCacheObjectAllocation(
					vmThread, clazz, J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(clazz->romClass)), J9UTF8_DATA(J9ROMCLASS_CLASSNAME(clazz->romClass)), clazz->totalInstanceSize, count, (UDATA) (((float)count)*sampleFreq));
		}
	}
	return;
}

void
MM_FrequentObjectsStats::merge(MM_FrequentObjectsStats* frequentObjectsStats)
{
	UDATA i;
	OMRSpaceSaving* spaceSaving = frequentObjectsStats->_spaceSaving;

	for(i = 0; i < spaceSavingGetCurSize(spaceSaving); i++ ){
		spaceSavingUpdate(_spaceSaving, spaceSavingGetKthMostFreq(spaceSaving,i+1), spaceSavingGetKthMostFreqCount(spaceSaving,i+1));
	}

}

