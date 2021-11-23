
/*******************************************************************************
 * Copyright (c) 1991, 2021 IBM Corp. and others
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
 * @ingroup GC_Modron_Standard
 */

#if !defined(STANDARDSATBACCESSBARRIER_HPP_)
#define STANDARDSATBACCESSBARRIER_HPP_


#include "j9.h"
#include "j9cfg.h"
#include "StandardAccessBarrier.hpp"
#include "RememberedSetSATB.hpp"

/**
 * Access barrier for Standard Concurrent SATB collector.
 */

class MM_StandardSATBAccessBarrier : public MM_StandardAccessBarrier
{
private:

protected:

public:
	static MM_StandardSATBAccessBarrier *newInstance(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);

	virtual void initializeForNewThread(MM_EnvironmentBase* env);
	virtual void rememberObjectImpl(MM_EnvironmentBase *env, J9Object* object);

	virtual bool preObjectStore(J9VMThread *vmThread, J9Object *destObject, fj9object_t *destAddress, J9Object *value, bool isVolatile=false);
	virtual bool preObjectStore(J9VMThread *vmThread, J9Object *destClass, J9Object **destAddress, J9Object *value, bool isVolatile=false);
	virtual bool preObjectStore(J9VMThread *vmThread, J9Object **destAddress, J9Object *value, bool isVolatile=false);

	virtual bool preBatchObjectStore(J9VMThread *vmThread, J9Object *destObject, bool isVolatile=false);
	virtual bool preBatchObjectStore(J9VMThread *vmThread, J9Class *destClass, bool isVolatile=false);

	virtual I_32 backwardReferenceArrayCopyIndex(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, I_32 srcIndex, I_32 destIndex, I_32 lengthInSlots);
	virtual I_32 forwardReferenceArrayCopyIndex(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, I_32 srcIndex, I_32 destIndex, I_32 lengthInSlots);

	bool preObjectStoreImpl(J9VMThread *vmThread, J9Object *destObject, fj9object_t *destAddress, J9Object *value, bool isVolatile);
	bool preObjectStoreImpl(J9VMThread *vmThread, J9Object **destAddress, J9Object *value, bool isVolatile);

	void rememberObjectToRescan(MM_EnvironmentBase *env, J9Object *object);

	virtual J9Object* referenceGet(J9VMThread *vmThread, J9Object *refObject);
	virtual void referenceReprocess(J9VMThread *vmThread, J9Object *refObject);
	virtual void forcedToFinalizableObject(J9VMThread* vmThread, J9Object *object);
	virtual void jniDeleteGlobalReference(J9VMThread *vmThread, J9Object *reference);

	virtual mm_j9object_t readObjectFromInternalVMSlotImpl(J9VMThread *vmThread, j9object_t *srcAddress, bool isVolatile=false);
	virtual void storeObjectToInternalVMSlotImpl(J9VMThread *vmThread, j9object_t *destAddress, mm_j9object_t value, bool isVolatile=false);

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

	MM_StandardSATBAccessBarrier(MM_EnvironmentBase *env) :
		MM_StandardAccessBarrier(env)
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* STANDARDSATBACCESSBARRIER_HPP_ */
