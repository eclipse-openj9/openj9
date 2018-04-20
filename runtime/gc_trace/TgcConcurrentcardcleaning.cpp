/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

/**
 * @file
 * @ingroup GC_Trace
 */ 


#include "j9.h"
#include "j9cfg.h"
#include "mmhook.h"
#include "j9port.h"
#include "modronopt.h"

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
#include "ConcurrentCardTableStats.hpp"
#include "ConcurrentGCStats.hpp"
#include "GCExtensions.hpp"
#include "EnvironmentBase.hpp"
#include "TgcExtensions.hpp"

/**
 * Final card cleaning has finished.
 * Function called by a hook when the final card cleaning has completed.
 * 
 * @param concurrentGCStats The address of the concurrent statistics structure
 * @param cardTableStats The address of the card table statistics structure
 */
static void
tgcHookCardCleaningComplete(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_ConcurrentCollectionCardCleaningEndEvent* event = (MM_ConcurrentCollectionCardCleaningEndEvent*)eventData;
	MM_GCExtensions *extensions =  MM_GCExtensions::getExtensions(event->currentThread);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);

	tgcExtensions->printf("Card cleaning for GC(%zu)\n",
#if defined(J9VM_GC_MODRON_SCAVENGER)
		extensions->scavengerStats._gcCount +
#endif /* J9VM_GC_MODRON_SCAVENGER */
		extensions->globalGCStats.gcCount + 
		1
	);
	
	/* All card clean thresholds initialized to HIGH_VALUES so cast to IDATA so they are printed as -1
	 * if we never got as far asKO of a particular phase of card cleaning 
	 */
   	tgcExtensions->printf("  concurrent card cleaning KO: Threshold=\"%zu\" Phase1= \"%zi\" Phase2= \"%zi\" Phase3= \"%zi\" \n",
   		event->cardCleaningThreshold,
   		(IDATA)event->cardCleaningPhase1KickOff,
   		(IDATA)event->cardCleaningPhase2KickOff,
   		(IDATA)event->cardCleaningPhase3KickOff
	);
   
   	tgcExtensions->printf("  concurrent cards cleaned: Phase1= \"%zu\" Phase2= \"%zu\" Phase3= \"%zu\" Total= \"%zu\" \n",
   		event->concleanedCardsPhase1,
   		event->concleanedCardsPhase2,
   		event->concleanedCardsPhase3,
   		event->concleanedCards
	);
	
	tgcExtensions->printf("  final cards cleaned: Phase1= \"%zu\" Phase2= \"%zu\" Total= \"%zu\" \n",
   		event->finalcleanedCardsPhase1,
   		event->finalcleanedCardsPhase2,
   		event->finalcleanedCards
	);
}

/**
 * Initialize cardcleaning tgc tracing.
 * Initializes the TgcConcurrentExtensions object associated with concurrent tgc tracing.
 * Attaches hooks to the appropriate functions handling events used by card cleaning tgc tracing.
 */
bool
tgcConcurrentCardCleaningInitialize(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions =  MM_GCExtensions::getExtensions(javaVM);
	bool result = true;

	J9HookInterface** privateHooks = J9_HOOK_INTERFACE(extensions->privateHookInterface);
	(*privateHooks)->J9HookRegisterWithCallSite(privateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_COLLECTION_CARD_CLEANING_END, tgcHookCardCleaningComplete, OMR_GET_CALLSITE(), NULL);

	return result;
}

#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
