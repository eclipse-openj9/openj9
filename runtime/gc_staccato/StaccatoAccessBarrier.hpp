
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


#if !defined(METRONOMEACCESSBARRIER_HPP_)
#define METRONOMEACCESSBARRIER_HPP_

#include "j9.h"
#include "j9cfg.h"

/**
 * Although a purely metronome access barrier could exist in RTSJ, it is almost definitely not what
 * we want so exclude this to avoid accidentally compiling and using the wrong kind of barrier
 */

#include "RealtimeAccessBarrier.hpp"
#include "RememberedSetSATB.hpp"

class MM_EnvironmentBase;
class MM_EnvironmentRealtime;

/**
 * Access barrier for Staccato collector.
 */
 
class MM_StaccatoAccessBarrier : public MM_RealtimeAccessBarrier
{
/* Data members & types */
public:
protected:
private:
	
	bool _doubleBarrierActive; /**< Global indicator that the double barrier is active. New threads will be set to double barrier mode if this falg is true. */

/* Methods */
public:
	/* Constructors & destructors */
	static MM_StaccatoAccessBarrier *newInstance(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);
	
	MMINLINE void setDoubleBarrierActive() { _doubleBarrierActive = true; }
	MMINLINE void setDoubleBarrierInactive() { _doubleBarrierActive = false; }
	MMINLINE bool isDoubleBarrierActive() { return _doubleBarrierActive; }
	void setDoubleBarrierActiveOnThread(MM_EnvironmentBase* env);
	void setDoubleBarrierInactiveOnThread(MM_EnvironmentBase* env);
	virtual void initializeForNewThread(MM_EnvironmentBase* env);
	
	virtual bool preObjectStore(J9VMThread *vmThread, J9Object *destObject, fj9object_t *destAddress, J9Object *value, bool isVolatile=false);
	virtual bool preObjectStore(J9VMThread *vmThread, J9Object *destClass, J9Object **destAddress, J9Object *value, bool isVolatile=false);
	virtual bool preObjectStore(J9VMThread *vmThread, J9Object **destAddress, J9Object *value, bool isVolatile=false);
	
	virtual I_32 backwardReferenceArrayCopyIndex(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, I_32 srcIndex, I_32 destIndex, I_32 lengthInSlots);
	virtual I_32 forwardReferenceArrayCopyIndex(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, I_32 srcIndex, I_32 destIndex, I_32 lengthInSlots);
	
	/**
	 * Remember objects that are forced onto the finalizable list at shutdown.
	 * Called from FinalizerSupport finalizeForcedUnfinalizedToFinalizable
	 *
	 * @param vmThread		current J9VMThread (aka JNIEnv)
	 * @param object		object forced onto the finalizable list
	 */
	virtual void forcedToFinalizableObject(J9VMThread* vmThread, J9Object *object);

	MM_StaccatoAccessBarrier(MM_EnvironmentBase *env) :
		MM_RealtimeAccessBarrier(env),
		_doubleBarrierActive(false)
	{
		_typeId = __FUNCTION__;
	}
	
protected:
	/* Constructors & destructors */
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);
	
	/* Inherited from MM_RealtimeAccessBarrier */
	virtual void rememberObjectImpl(MM_EnvironmentBase *env, J9Object* object);
	
	/* New methods */
	bool preObjectStoreInternal(J9VMThread *vmThread, J9Object *destClass, J9Object **destAddress, J9Object *value, bool isVolatile);
	bool preObjectStoreInternal(J9VMThread *vmThread, J9Object *destObject, fj9object_t *destAddress, J9Object *value, bool isVolatile);
	bool preObjectStoreInternal(J9VMThread *vmThread, J9Object **destAddress, J9Object *value, bool isVolatile);
	

	
	/* New methods */
	MMINLINE bool isBarrierActive(MM_EnvironmentBase* env)
	{
		MM_GCExtensions* extensions = MM_GCExtensions::getExtensions(env);
		return !extensions->sATBBarrierRememberedSet->isGlobalFragmentIndexPreserved(env);
	}
	
	MMINLINE bool isDoubleBarrierActiveOnThread(J9VMThread *vmThread)
	{
		/* The double barrier is enabled in staccato by setting the threads remembered set fragment index
		 * to the special value, this ensures the JIT will go out-of line. We can determine if the double
		 * barrier is active simply by checking if the fragment index corresponds to the special value.
		 */
		return (J9GC_REMEMBERED_SET_RESERVED_INDEX == vmThread->sATBBarrierRememberedSetFragment.localFragmentIndex);
	}
	
	bool markAndScanContiguousArray(MM_EnvironmentRealtime *env, J9IndexableObject *objectPtr);
	void scanContiguousArray(MM_EnvironmentRealtime *env, J9IndexableObject *objectPtr);
private:	
};

#endif /* METRONOMEACCESSBARRIER_HPP_ */
