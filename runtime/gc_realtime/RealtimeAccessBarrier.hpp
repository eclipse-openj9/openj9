
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


#if !defined(REALTIMEACCESSBARRIER_HPP_)
#define REALTIMEACCESSBARRIER_HPP_

#include "j9.h"
#include "j9cfg.h"

#if defined(J9VM_GC_REALTIME)

#include "Metronome.hpp"
#include "ObjectAccessBarrier.hpp"

class MM_EnvironmentBase;
class MM_RealtimeMarkingScheme;
class MM_RealtimeGC;

/**
 * Base access barrier for the realtime collectors.
 * The realtime collectors may use objects during a GC phase, and relies on the
 * access barrier to ensure that mutator threads only ever see objects in their
 * new location. This is implemented using a forwarding pointer in each object
 * that normally points to the object itself, but after an object has been
 * moved, points to the new location. This class also implements the snapshot
 * at the beggining barrier that remembers overwritten values.
 */

class MM_RealtimeAccessBarrier : public MM_ObjectAccessBarrier
{
/* Data members & types */
public:
protected:
	MM_RealtimeMarkingScheme *_markingScheme;	
	MM_RealtimeGC *_realtimeGC;
private:
	
/* Methods */
public:
	/* Constructors & destructors */
	MM_RealtimeAccessBarrier(MM_EnvironmentBase *env) :
		MM_ObjectAccessBarrier(env),
		_markingScheme(NULL),
		_realtimeGC(NULL)
	{
		_typeId = __FUNCTION__;
	}
	
	/* Inherited from MM_ObjectAccessBarrier */
	virtual J9Object* referenceGet(J9VMThread *vmThread, J9Object *refObject);
	virtual void jniDeleteGlobalReference(J9VMThread *vmThread, J9Object *reference);
	virtual void stringConstantEscaped(J9VMThread *vmThread, J9Object *stringConst);
	virtual void deleteHeapReference(MM_EnvironmentBase *env, J9Object *object);
	
	virtual void storeObjectToInternalVMSlot(J9VMThread *vmThread, J9Object** destSlot, J9Object *value);

	virtual void* jniGetPrimitiveArrayCritical(J9VMThread* vmThread, jarray array, jboolean *isCopy);
	virtual void jniReleasePrimitiveArrayCritical(J9VMThread* vmThread, jarray array, void * elems, jint mode);
	virtual const jchar* jniGetStringCritical(J9VMThread* vmThread, jstring str, jboolean *isCopy);
	virtual void jniReleaseStringCritical(J9VMThread* vmThread, jstring str, const jchar* elems);

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	/**
	 * Check is class alive
	 * Required to prevent visibility of dead classes in incremental GC policies 
	 * Check is J9_GC_CLASS_LOADER_DEAD flag set for classloader and try to mark
	 * class loader object if bit is not set to force class to be alive
	 * @param javaVM pointer to J9JavaVM
	 * @param classPtr class to check
	 * @return true if class is alive
	 */
	virtual bool checkClassLive(J9JavaVM *javaVM, J9Class *classPtr);
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */

protected:
	/* Constructors & destructors */
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);
	
	/* Inherited from MM_ObjectAccessBarrier */
	virtual mm_j9object_t readObjectFromInternalVMSlotImpl(J9VMThread *vmThread, j9object_t *srcAddress, bool isVolatile=false);
	
	/* New methods */
	void validateWriteBarrier(J9VMThread *vmThread, J9Object *dstObject, fj9object_t *dstAddress, J9Object *srcObject);
	virtual void rememberObjectImpl(MM_EnvironmentBase *env, J9Object *object) = 0;
	
	/* New methods */
	void rememberObject(MM_EnvironmentBase *env, J9Object *object);
	void rememberObjectIfBarrierEnabled(J9VMThread *vmThread, J9Object* object);
	
private:
	void printClass(J9JavaVM *javaVM, J9Class* clazz);
};

#endif /* J9VM_GC_REALTIME */

#endif /*REALTIMEACCESSBARRIER_HPP_*/
