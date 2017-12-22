
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/


/**
 * @file
 * @ingroup GC_Base
 */

#include "j9.h"
#include "j9cfg.h"
#include "j9consts.h"
#include "j9protos.h"
#include "ModronAssertions.h"
#include "j9nongenerated.h"

#if defined(J9VM_GC_GENERATIONAL)

#include "GenerationalAccessBarrierComponent.hpp"

#include "AtomicOperations.hpp"
#include "Debug.hpp"
#include "EnvironmentBase.hpp"
#include "mmhook_internal.h"
#include "GCExtensions.hpp"
#include "ObjectModel.hpp"
#include "SublistFragment.hpp"

extern "C" {
/**
 * Inform consumers of RememberedSet overflow.
 */
static void
reportRememberedSetOverflow(J9VMThread *vmThread)
{
	Trc_MM_RememberedSetOverflow(vmThread);
	TRIGGER_J9HOOK_MM_PRIVATE_REMEMBEREDSET_OVERFLOW(MM_GCExtensions::getExtensions(vmThread->javaVM)->privateHookInterface, (OMR_VMThread *)vmThread->omrVMThread);
}
} /* extern "C" */

bool 
MM_GenerationalAccessBarrierComponent::initialize(MM_EnvironmentBase *env)
{
	return true;
}

void
MM_GenerationalAccessBarrierComponent::tearDown(MM_EnvironmentBase *env)
{
}

/**
 * Generational write barrier call when a single object is stored into another.
 * The remembered set system consists of a physical list of objects in the OLD area that
 * may contain references to the new area.  The mutator is responsible for adding these old
 * area objects to the remembered set; the collectors are responsible for removing these objects
 * from the list when they no longer contain references.  Objects that are to be remembered have their
 * REMEMBERED bit set in the flags field.  For performance reasons, sublists are used to maintain the
 * remembered set.
 * 
 * @param vmThread The current thread that has performed the store.
 * @param dstObject The object which is being stored into.
 * @param srcObject The object being stored.
 * 
 * @note The write barrier can be called with minimal, all, or no validation checking.
 * @note Any object that contains a new reference MUST have its REMEMBERED bit set.
 */
void
MM_GenerationalAccessBarrierComponent::postObjectStore(J9VMThread *vmThread, J9Object *dstObject, J9Object *srcObject)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);

	/* If the source object is NULL, there is no need for a write barrier. */
	/* If scavenger not enabled then no need to add to remembered set */	
	if ((NULL != srcObject) && extensions->scavengerEnabled) {
		if (extensions->isOld(dstObject) && !extensions->isOld(srcObject)) {
			if (extensions->objectModel.atomicSetRememberedState(dstObject, STATE_REMEMBERED)) {
				/* Successfully set the remembered bit in the object.  Now allocate an entry from the
				 * remembered set fragment of the current thread and store the destination object into
				 * the remembered set. 
				 */
				MM_SublistFragment fragment((J9VMGC_SublistFragment*)&vmThread->gcRememberedSet);

				if (!fragment.add(env, (UDATA)dstObject )) {
					/* No slot was available from any fragment.  Set the remembered set overflow flag.
					 * The REMEMBERED bit is kept in the object for optimization purposes (only scan objects
					 * whose REMEMBERED bit is set in an overflow scan) 
					 */
					extensions->setRememberedSetOverflowState();
					reportRememberedSetOverflow(vmThread);
				}
			}
		}
	}
}

/**
 * Generational write barrier call when a group of objects are stored into a single object.
 * The remembered set system consists of a physical list of objects in the OLD area that
 * may contain references to the new area.  The mutator is responsible for adding these old
 * area objects to the remembered set; the collectors are responsible for removing these objects
 * from the list when they no longer contain references.  Objects that are to be remembered have their
 * REMEMBERED bit set in the flags field.  For performance reasons, sublists are used to maintain the
 * remembered set.
 * 
 * @param vmThread The current thread that has performed the store.
 * @param dstObject The object which is being stored into.
 * 
 * @note The write barrier can be called with minimal, all, or no validation checking.
 * @note Any object that contains a new reference MUST have its REMEMBERED bit set.
 * @note This call is typically used by array copies, when it may be more efficient
 * to optimistically add an object to the remembered set without checking too hard.
 */
void 
MM_GenerationalAccessBarrierComponent::preBatchObjectStore(J9VMThread *vmThread, J9Object *dstObject)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	
	/* If scavenger not enabled then no need to add to remembered set */	
	if (extensions->scavengerEnabled) {
		/* Since we don't know what the references stored into dstObject were, we have to pessimistically
		 * assume they included old->new references, and the object should be added to the remembered set */
		if (extensions->isOld(dstObject)) {
			if (extensions->objectModel.atomicSetRememberedState(dstObject, STATE_REMEMBERED)) {
				/* Successfully set the remembered bit in the object.  Now allocate an entry from the
				 * remembered set fragment of the current thread and store the destination object into
				 * the remembered set. */
				UDATA *rememberedSlot;
				MM_SublistFragment fragment((J9VMGC_SublistFragment*)&vmThread->gcRememberedSet);
				if (NULL == (rememberedSlot = (UDATA *)fragment.allocate(env))) {
					/* No slot was available from any fragment.  Set the remembered set overflow flag.
					 * The REMEMBERED bit is kept in the object for optimization purposes (only scan objects
					 * whose REMEMBERED bit is set in an overflow scan) */
					extensions->setRememberedSetOverflowState();
					reportRememberedSetOverflow(vmThread);
				} else {
					/* Successfully allocated a slot from the remembered set.  Record the object. */
					*rememberedSlot = (UDATA)dstObject;
				}
			}
		}
	}
}

#endif /* J9VM_GC_GENERATIONAL */

