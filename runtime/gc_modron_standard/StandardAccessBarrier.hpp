
/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/


/**
 * @file
 * @ingroup GC_Modron_Standard
 */

#if !defined(STANDARDACCESSBARRIER_HPP_)
#define STANDARDACCESSBARRIER_HPP_


#include "j9.h"
#include "j9cfg.h"	
#include "ObjectAccessBarrier.hpp"
#include "Configuration.hpp"
#include "GenerationalAccessBarrierComponent.hpp"
#if defined(OMR_GC_REALTIME)
#include "RememberedSetSATB.hpp"
#endif /* OMR_GC_REALTIME */

class MM_MarkingScheme;
class MM_Scavenger;

/**
 * Access barrier for Modron collector.
 */
 
class MM_StandardAccessBarrier : public MM_ObjectAccessBarrier
{
private:
#if defined(J9VM_GC_GENERATIONAL)
	MM_GenerationalAccessBarrierComponent _generationalAccessBarrierComponent;	/**< Generational Component of Access Barrier */
	MM_Scavenger *_scavenger;	/**< caching MM_GCExtensions::scavenger */
#endif /* J9VM_GC_GENERATIONAL */
	MM_MarkingScheme *_markingScheme;	/**< caching MM_MarkingScheme instance */

	void postObjectStoreImpl(J9VMThread *vmThread, J9Object *dstObject, J9Object *srcObject);
	void postBatchObjectStoreImpl(J9VMThread *vmThread, J9Object *dstObject);

#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	I_32 doCopyContiguousBackwardWithReadBarrier(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, I_32 srcIndex, I_32 destIndex, I_32 lengthInSlots);
	I_32 doCopyContiguousForwardWithReadBarrier(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, I_32 srcIndex, I_32 destIndex, I_32 lengthInSlots);
#endif /* OMR_GC_CONCURRENT_SCAVENGER */

protected:
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);
	
public:
	static MM_StandardAccessBarrier *newInstance(MM_EnvironmentBase *env, MM_MarkingScheme *markingScheme);
	virtual void kill(MM_EnvironmentBase *env);

	MM_StandardAccessBarrier(MM_EnvironmentBase *env, MM_MarkingScheme *markingScheme) :
		MM_ObjectAccessBarrier(env)
#if defined(J9VM_GC_GENERATIONAL)
		, _scavenger(_extensions->scavenger)
#endif /* J9VM_GC_GENERATIONAL */
		, _markingScheme(markingScheme)
	{
		_typeId = __FUNCTION__;
	}

	virtual J9Object* asConstantPoolObject(J9VMThread *vmThread, J9Object* toConvert, UDATA allocationFlags);

	virtual void rememberObjectImpl(MM_EnvironmentBase *env, J9Object* object);
	virtual bool preObjectStore(J9VMThread *vmThread, J9Object *destObject, fj9object_t *destAddress, J9Object *value, bool isVolatile=false);
	virtual bool preObjectStore(J9VMThread *vmThread, J9Object *destClass, J9Object **destAddress, J9Object *value, bool isVolatile=false);
	virtual bool preObjectStore(J9VMThread *vmThread, J9Object **destAddress, J9Object *value, bool isVolatile=false);

	virtual void postObjectStore(J9VMThread *vmThread, J9Object *destObject, fj9object_t *destAddress, J9Object *value, bool isVolatile=false);
	virtual void postObjectStore(J9VMThread *vmThread, J9Class *destClass, J9Object **destAddress, J9Object *value, bool isVolatile=false);
	virtual bool postBatchObjectStore(J9VMThread *vmThread, J9Object *destObject, bool isVolatile=false);
	virtual bool postBatchObjectStore(J9VMThread *vmThread, J9Class *destClass, bool isVolatile=false);
	virtual void recentlyAllocatedObject(J9VMThread *vmThread, J9Object *object); 

	virtual void preMountContinuation(J9VMThread *vmThread, j9object_t contObject);
	virtual void postUnmountContinuation(J9VMThread *vmThread, j9object_t contObject);

	virtual void* jniGetPrimitiveArrayCritical(J9VMThread* vmThread, jarray array, jboolean *isCopy);
	virtual void jniReleasePrimitiveArrayCritical(J9VMThread* vmThread, jarray array, void * elems, jint mode);
	virtual const jchar* jniGetStringCritical(J9VMThread* vmThread, jstring str, jboolean *isCopy);
	virtual void jniReleaseStringCritical(J9VMThread* vmThread, jstring str, const jchar* elems);
	virtual void initializeForNewThread(MM_EnvironmentBase* env);

	virtual I_32 backwardReferenceArrayCopyIndex(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, I_32 srcIndex, I_32 destIndex, I_32 lengthInSlots);
	virtual I_32 forwardReferenceArrayCopyIndex(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, I_32 srcIndex, I_32 destIndex, I_32 lengthInSlots);

#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	/* heap slot (possibly compressed refs) */
	virtual bool preObjectRead(J9VMThread *vmThread, J9Object *srcObject, fj9object_t *srcAddress);
	/* off-heap slot (always non-compressed) */
	virtual bool preObjectRead(J9VMThread *vmThread, J9Class *srcClass, j9object_t *srcAddress);
	virtual bool preWeakRootSlotRead(J9VMThread *vmThread, j9object_t *srcAddress);
	virtual bool preWeakRootSlotRead(J9JavaVM *vm, j9object_t *srcAddress);	
#endif	

	bool preObjectStoreImpl(J9VMThread *vmThread, J9Object *destObject, fj9object_t *destAddress, J9Object *value, bool isVolatile);
	bool preObjectStoreImpl(J9VMThread *vmThread, J9Object **destAddress, J9Object *value, bool isVolatile);

	void rememberObjectToRescan(MM_EnvironmentBase *env, J9Object *object);

	MMINLINE bool isIncrementalUpdateBarrierActive(J9VMThread *vmThread)
	{
		return ((vmThread->privateFlags & J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE) &&
				_extensions->configuration->isIncrementalUpdateBarrierEnabled());
	}

	virtual J9Object* referenceGet(J9VMThread *vmThread, J9Object *refObject);
	virtual void referenceReprocess(J9VMThread *vmThread, J9Object *refObject);

	/**
	 * Return the number of currently active JNI critical regions.
	 *
	 * Must hold exclusive VM access to call this!
	 */
	static UDATA getJNICriticalRegionCount(MM_GCExtensions *extensions);

	virtual void forcedToFinalizableObject(J9VMThread* vmThread, J9Object *object);

	virtual void jniDeleteGlobalReference(J9VMThread *vmThread, J9Object *reference);

	virtual void stringConstantEscaped(J9VMThread *vmThread, J9Object *stringConst);
	virtual bool checkStringConstantsLive(J9JavaVM *javaVM, j9object_t stringOne, j9object_t stringTwo);
	virtual bool checkStringConstantLive(J9JavaVM *javaVM, j9object_t string);

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	/**
	 * Check is class alive
	 * Required to prevent visibility of dead classes in SATB GC policies
	 * Check is J9_GC_CLASS_LOADER_DEAD flag set for classloader and try to mark
	 * class loader object if bit is not set to force class to be alive
	 * @param javaVM pointer to J9JavaVM
	 * @param classPtr class to check
	 * @return true if class is alive
	 */
	virtual bool checkClassLive(J9JavaVM *javaVM, J9Class *classPtr);
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */
};

#endif /* STANDARDACCESSBARRIER_HPP_ */
