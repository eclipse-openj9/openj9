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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/


#if !defined(REMEMBEREDSETWORKPACKETS_HPP_)
#define REMEMBEREDSETWORKPACKETS_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"

#include "BaseNonVirtual.hpp"
#include "WorkPacketsRealtime.hpp"

class EnvironmentModron;

class MM_RememberedSetWorkPackets : public MM_BaseNonVirtual
{
/* Data members & types */
public:
	J9VMGCRememberedSet _rememberedSetStruct; /**< The VM-readable struct containing the remembered set "global" indexes. */
protected:
private:
	MM_WorkPacketsRealtime *_workPackets; /**< The workPackets struct used as backing store for the rememberedSet */

/* Methods */
public:
	/* Constructors & destructors */
	static MM_RememberedSetWorkPackets *newInstance(MM_EnvironmentBase *env, MM_WorkPacketsRealtime *workPackets);
	void kill(MM_EnvironmentBase *env);
	
	MM_RememberedSetWorkPackets(MM_EnvironmentBase *env, MM_WorkPacketsRealtime *workPackets) :
		MM_BaseNonVirtual(),
		_workPackets(workPackets)
	{
		_typeId = __FUNCTION__;
		/* Initializing the global fragment index to the reserved index means the GC starts
		 * with the barrier disabled. The preservedGlobalFragmentIndex must be initialized
		 * to any non-reserved value so that the call to MM_StaccatoGC::enableWriteBarrier which
		 * in turns restores the globalFragmentIndex from the preservedGlobalFragmentIndex actually
		 * restores a valid, non-reserved value.
		 */
		_rememberedSetStruct.globalFragmentIndex = J9GC_REMEMBERED_SET_RESERVED_INDEX;
		_rememberedSetStruct.preservedGlobalFragmentIndex = J9GC_REMEMBERED_SET_RESERVED_INDEX + 1; 
	};
	
	/* New methods */
	void initializeFragment(MM_EnvironmentBase* env, J9VMGCRememberedSetFragment* fragment); /* "Nulls" out a fragment. */
	void storeInFragment(MM_EnvironmentBase* env, J9VMGCRememberedSetFragment* fragment, UDATA* value); /* This guarantees the store will occur, but a new fragment may be fetched. */
	bool isFragmentValid(MM_EnvironmentBase* env, const J9VMGCRememberedSetFragment* fragment);
	void preserveLocalFragmentIndex(MM_EnvironmentBase* env, J9VMGCRememberedSetFragment* fragment); /* Called by the code that enables the double-barrier. */
	void restoreLocalFragmentIndex(MM_EnvironmentBase* env, J9VMGCRememberedSetFragment* fragment); /* Called by the root scanner to disable the double-barrier. */
	void preserveGlobalFragmentIndex(MM_EnvironmentBase* env); /* Called by the code that disables the barrier. */
	void restoreGlobalFragmentIndex(MM_EnvironmentBase* env); /* Called by the code that enables the barrier. */
	/* Used to determine if the staccato write barrier is enabled. */
	MMINLINE bool
	isGlobalFragmentIndexPreserved(MM_EnvironmentBase* env)
	{
		return (J9GC_REMEMBERED_SET_RESERVED_INDEX == _rememberedSetStruct.globalFragmentIndex);
	}
	void flushFragments(MM_EnvironmentBase* env); /* Ensures all fragments will be seen as invalid next time they are accessed. */
	bool refreshFragment(MM_EnvironmentBase *env, J9VMGCRememberedSetFragment* fragment);
	
protected:
	bool initialize(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);
	UDATA getLocalFragmentIndex(MM_EnvironmentBase* env, const J9VMGCRememberedSetFragment* fragment);
	UDATA getGlobalFragmentIndex(MM_EnvironmentBase* env);
	
private:
	void setGlobalIndex(MM_EnvironmentBase* env, UDATA indexValue); /* Increments the appropriate global index (global or preserved). */
};

#endif /* REMEMBEREDSETWORKPACKETS_HPP_ */

