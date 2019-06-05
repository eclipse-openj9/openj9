
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

#include "j9.h"
#include "j9cfg.h"
#include "j9port.h"
#include "ModronAssertions.h"

#if defined(J9VM_GC_FINALIZATION)

#include "FinalizeListManager.hpp"
#include "GCExtensions.hpp"
#include "Debug.hpp"
#include "ObjectAccessBarrier.hpp"

/**
 * lock the finalize list manager
 */
void
GC_FinalizeListManager::lock() const
{
	omrthread_monitor_enter(_mutex);
}

/**
 * unlock the finalize list manager
 */
void
GC_FinalizeListManager::unlock() const
{
	omrthread_monitor_exit(_mutex);
}

/**
 * create and initialize with defaults new instance of FinalizeListManager
 * @return Pointer to FinalizeListmanager if initialisation successful, NULL otherwise
 */
GC_FinalizeListManager *
GC_FinalizeListManager::newInstance(MM_EnvironmentBase *env)
{
	GC_FinalizeListManager *manager;
	
	manager = (GC_FinalizeListManager *)env->getForge()->allocate(sizeof(GC_FinalizeListManager), MM_AllocationCategory::FINALIZE, J9_GET_CALLSITE());
	if (manager) {
		new(manager) GC_FinalizeListManager(MM_GCExtensions::getExtensions(env));
		if (!manager->initialize()) {
			manager->kill(env);
			return NULL;
		}
	}
	
	return manager;
}

/**
 * deinitialize the instance of FinalizeListManager and deallocate the memory
 */
void
GC_FinalizeListManager::kill(MM_EnvironmentBase *env)
{
	tearDown();
	env->getForge()->free(this);
} 

/**
 * initialize with default values and create the mutex
 * @return true if initialisation successful, false otherwise.
 */
bool
GC_FinalizeListManager::initialize()
{
	if (omrthread_monitor_init_with_name(&_mutex, 0, "FinalizeListManager")) {
		_mutex = NULL;
		return false;
	}
	
	return true;
}

/**
 * deinitialize FinalizeListManager (destroy the mutex)
 */
void
GC_FinalizeListManager::tearDown()
{
	if(NULL != _mutex) {
		omrthread_monitor_destroy(_mutex);
		_mutex = NULL;
	}
}

void
GC_FinalizeListManager::addSystemFinalizableObjects(j9object_t head, j9object_t tail, UDATA objectCount)
{
	lock();

	_extensions->accessBarrier->setFinalizeLink(tail, _systemFinalizableObjects);
	_systemFinalizableObjects = head;
	_systemFinalizableObjectCount += objectCount;

	unlock();
}

j9object_t
GC_FinalizeListManager::popSystemFinalizableObject()
{
	j9object_t value = _systemFinalizableObjects;

	if (NULL != value) {
		_systemFinalizableObjects = _extensions->accessBarrier->getFinalizeLink(value);
		_systemFinalizableObjectCount -= 1;
	}

	return value;
}

void
GC_FinalizeListManager::addDefaultFinalizableObjects(j9object_t head, j9object_t tail, UDATA objectCount)
{
	lock();

	_extensions->accessBarrier->setFinalizeLink(tail, _defaultFinalizableObjects);
	_defaultFinalizableObjects = head;
	_defaultFinalizableObjectCount += objectCount;

	unlock();
}

j9object_t
GC_FinalizeListManager::popDefaultFinalizableObject()
{
	j9object_t value = _defaultFinalizableObjects;

	if (NULL != value) {
		_defaultFinalizableObjects = _extensions->accessBarrier->getFinalizeLink(value);
		_defaultFinalizableObjectCount -= 1;
	}

	return value;
}

void
GC_FinalizeListManager::addReferenceObjects(j9object_t head, j9object_t tail, UDATA objectCount)
{
	lock();

	_extensions->accessBarrier->setReferenceLink(tail, _referenceObjects);
	_referenceObjects = head;
	_referenceObjectCount += objectCount;

	unlock();
}

j9object_t
GC_FinalizeListManager::popReferenceObject()
{
	j9object_t value = _referenceObjects;

	if (NULL != value) {
		_referenceObjects = _extensions->accessBarrier->getReferenceLink(value);
		_referenceObjectCount -= 1;
	}

	return value;
}

void
GC_FinalizeListManager::addClassLoaders(J9ClassLoader *head, J9ClassLoader *tail, UDATA count)
{
	lock();

	tail->unloadLink = _classLoaders;
	_classLoaders = head;
	_classLoaderCount += count;

	unlock();
}

J9ClassLoader *
GC_FinalizeListManager::popClassLoader()
{
	J9ClassLoader *value = _classLoaders;

	if (NULL != value) {
		_classLoaders = value->unloadLink;
		_classLoaderCount -= 1;
	}

	return value;
}

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
J9ClassLoader *
GC_FinalizeListManager::popRequiredClassLoaderForForcedClassLoaderUnload()
{
	J9ClassLoader *returnValue = NULL;
	J9ClassLoader *classLoader = _classLoaders;
	J9ClassLoader *previousLoader = NULL;
	while (NULL != classLoader) {
		if (NULL != classLoader->gcThreadNotification) {
			returnValue = classLoader;
			if (NULL == previousLoader) {
				_classLoaders = classLoader->unloadLink;
			} else {
				previousLoader->unloadLink = classLoader->unloadLink;

			}
			_classLoaderCount -= 1;
			break;
		}
		previousLoader = classLoader;
		classLoader = classLoader->unloadLink;
	}

	return returnValue;
}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

GC_FinalizeJob *
GC_FinalizeListManager::consumeJob(J9VMThread *vmThread, GC_FinalizeJob * job)
{
	Assert_MM_true(J9_PUBLIC_FLAGS_VM_ACCESS == (vmThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS));
	Assert_MM_true(1 == omrthread_monitor_owned_by_self(_mutex)); /* caller must be holding _mutex */
	
	{
		j9object_t referenceObject = popReferenceObject();
		if (NULL != referenceObject) {
			job->type = FINALIZE_JOB_TYPE_REFERENCE;
			job->reference = referenceObject;

			return job;
		}
	}

	{
		J9ClassLoader *loader = popClassLoader();
		if (NULL != loader) {
			job->type = FINALIZE_JOB_TYPE_CLASSLOADER;
			job->classLoader = loader;

			return job;
		}
	}

	{
		j9object_t defaultObject = popDefaultFinalizableObject();
		if (NULL != defaultObject) {
			job->type = FINALIZE_JOB_TYPE_OBJECT;
			job->object = defaultObject;

			return job;
		}
	}

	{
		j9object_t systemObject = popSystemFinalizableObject();
		if (NULL != systemObject) {
			job->type = FINALIZE_JOB_TYPE_OBJECT;
			job->object = systemObject;

			return job;
		}
	}

	return NULL;
}

#endif /* J9VM_GC_FINALIZATION */
