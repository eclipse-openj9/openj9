
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

/**
 * @file
 * @ingroup GC_Base
 */

#if !defined(FINALIZELISTMANAGER_HPP_)
#define FINALIZELISTMANAGER_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"

#if defined(J9VM_GC_FINALIZATION)

#include "ObjectAccessBarrier.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"

class MM_GCExtensions;
class GC_CheckFinalizableList;

typedef enum GC_FinalizeJobType {
	FINALIZE_JOB_TYPE_OBJECT = 1,
	FINALIZE_JOB_TYPE_REFERENCE = 2,
	FINALIZE_JOB_TYPE_CLASSLOADER = 4
} GC_FinalizeJobType;
typedef struct GC_FinalizeJob {
	GC_FinalizeJobType type;
	union {
		j9object_t object;
		j9object_t reference;
		J9ClassLoader *classLoader;
	};
} GC_FinalizeJob;

/**
 * Provides facility for the management of the finalizer queue
 * @ingroup GC_Base
 */
class GC_FinalizeListManager : public MM_BaseVirtual
{
/* Data members */
private:
	MM_GCExtensions *_extensions; /**< a cached pointer to the extensions structure */
    omrthread_monitor_t _mutex; /**< mutex used to add / remove jobs from the finalize lists */

    j9object_t _systemFinalizableObjects; /**< head of the linked list of objects allocated by the system classloader that need to be finalized */
    UDATA _systemFinalizableObjectCount; /** count of the system finalizable object  */
    j9object_t _defaultFinalizableObjects; /**< head of the linked list of objects allocated by non system classloaders that need to be finalized */
    UDATA _defaultFinalizableObjectCount; /** count of the default finalizable object  */
    j9object_t _referenceObjects; /**< head of the linked list of reference objects that need to be enqueued */
    UDATA _referenceObjectCount; /** count of the reference object */
    J9ClassLoader *_classLoaders; /**< head of the linked list of unloaded classloaders which have open native libraries  */
    UDATA _classLoaderCount; /** count of the class loaders */
protected:
public:
    
/* Methods */
private:
protected:
    /**
     * Pop the head of the System finalizable list
     *
     * @note Must be called while holding this class' _mutex
     *
     * @return the current head of the list, or NULL if the list is empty
     */
    j9object_t popSystemFinalizableObject();

    /**
     * Pop the head of the default finalizable list
     *
     * @note Must be called while holding this class' _mutex
     *
     * @return the current head of the list, or NULL if the list is empty
     */
    j9object_t popDefaultFinalizableObject();

    /**
     * Pop the head of the reference enqueue list
     *
     * @note Must be called while holding this class' _mutex
     *
     * @return the current head of the list, or NULL if the list is empty
     */
    j9object_t popReferenceObject();

    /**
     * Pop the head of the classloader list
     *
     * @note Must be called while holding this class' _mutex
     *
     * @return the current head of the list, or NULL if the list is empty
     */
    J9ClassLoader *popClassLoader();

public:
	void lock() const;
	void unlock() const;
	
	/**
	 * Gets the number of jobs on the queue.
	 * @return The number of jobs on the queue.
	 */
	virtual UDATA getJobCount() const
	{
		lock();
		UDATA count = _classLoaderCount + _defaultFinalizableObjectCount + _systemFinalizableObjectCount + _referenceObjectCount;
		unlock();
		return count;
	}

	virtual UDATA getSystemCount() {return _systemFinalizableObjectCount;}
	virtual UDATA getDefaultCount() {return _defaultFinalizableObjectCount;}
	MMINLINE UDATA getClassloaderCount() {return _classLoaderCount;}
	MMINLINE UDATA getReferenceCount() {return _referenceObjectCount;}

	static GC_FinalizeListManager	*newInstance(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);
	bool initialize();
	void tearDown();

	/**
	 * Add the list of objects to the system finalizable list
	 *
	 * @param head[in] head of the list to add
	 * @param tail[in] tail of the list to add
	 * @param count[in] number of objects in the list to add
	 */
	virtual void addSystemFinalizableObjects(j9object_t head, j9object_t tail, UDATA objectCount);
	/**
	 * Add the list of objects to the default finalizable list
	 *
	 * @param head[in] head of the list to add
	 * @param tail[in] tail of the list to add
	 * @param count[in] number of objects in the list to add
	 */
	virtual void addDefaultFinalizableObjects(j9object_t head, j9object_t tail, UDATA objectCount);
	/**
	 * Add the list of reference objects to the reference list
	 *
	 * @param head[in] head of the list to add
	 * @param tail[in] tail of the list to add
	 * @param count[in] number of objects in the list to add
	 */
	void addReferenceObjects(j9object_t head, j9object_t tail, UDATA objectCount);
	/**
	 * Add the list of classloaders to the classloader list
	 *
	 * @param head[in] head of the list to add
	 * @param tail[in] tail of the list to add
	 * @param count[in] number of objects in the list to add
	 */
	void addClassLoaders(J9ClassLoader *head, J9ClassLoader *tail, UDATA Count);

	/**
	 * Return the head of the system finalize list and set the list to NULL
	 *
	 * @return the current head of the list, or NULL if the list is empty
	 */
	j9object_t resetSystemFinalizableObjects()
	{
		j9object_t value = _systemFinalizableObjects;
		_systemFinalizableObjects = NULL;
		_systemFinalizableObjectCount = 0;
		return value;
	}
	/**
	 * Return the head of the default finalize list and set the list to NULL
	 *
	 * @return the current head of the list, or NULL if the list is empty
	 */
	j9object_t resetDefaultFinalizableObjects()
	{
		j9object_t value = _defaultFinalizableObjects;
		_defaultFinalizableObjects = NULL;
		_defaultFinalizableObjectCount = 0;
		return value;
	}
	/**
	 * Return the head of the reference enqueue list and set the list to NULL
	 *
	 * @return the current head of the list, or NULL if the list is empty
	 */
	j9object_t resetReferenceObjects()
	{
		j9object_t value = _referenceObjects;
		_referenceObjects = NULL;
		_referenceObjectCount = 0;
		return value;
	}
	/**
	 * Peek the head of the System finalizable list
	 *
	 * @return the current head of the list, or NULL if the list is empty
	 */
	virtual j9object_t peekSystemFinalizableObject()
	{
		return _systemFinalizableObjects;
	}
	/**
	 * Peek the next System finalizable list object
	 *
	 * @param current[in] the current object to get the next from
	 *
	 * @return the next object of the list, or NULL
	 */
	virtual j9object_t peekNextSystemFinalizableObject(j9object_t current)
	{
		MM_ObjectAccessBarrier *barrier = _extensions->accessBarrier;
		return barrier->getFinalizeLink(current);
	}
	/**
	 * Peek the head of the default finalizable list
	 *
	 * @return the current head of the list, or NULL if the list is empty
	 */
	virtual j9object_t peekDefaultFinalizableObject()
	{
		return _defaultFinalizableObjects;
	}
	/**
	 * Peek the next of the default finalizable list
	 *
	 * @param current[in] the current object to get the next from
	 *
	 * @return the next object of the list, or NULL
	 */
	virtual j9object_t peekNextDefaultFinalizableObject(j9object_t current)
	{
		MM_ObjectAccessBarrier *barrier = _extensions->accessBarrier;
		return barrier->getFinalizeLink(current);
	}
	/**
	 * Peek the head of the reference list
	 *
	 * @return the current head of the list, or NULL if the list is empty
	 */
	MMINLINE j9object_t peekReferenceObject()
	{
		return _referenceObjects;
	}
	/**
	 * Peek the next of the reference list
	 *
	 * @param current[in] the current object to get the next from
	 *
	 * @return the next object of the list, or NULL
	 */
	MMINLINE j9object_t peekNextReferenceObject(j9object_t current)
	{
		MM_ObjectAccessBarrier *barrier = _extensions->accessBarrier;
		return barrier->getReferenceLink(current);
	}
	/**
	 * Peek the head of the classloader list
	 *
	 * @return the current head of the list, or NULL if the list is empty
	 */
	MMINLINE J9ClassLoader *peekClassLoader()
	{
		return _classLoaders;
	}
	/**
	 * Peek the next of the classloader list
	 *
	 * @param current[in] the current classloader to get the next from
	 *
	 * @return the next classloader of the list, or NULL
	 */
	MMINLINE J9ClassLoader *peekNextClassLoader(J9ClassLoader *current)
	{
		return current->unloadLink;
	}

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	/**
	 * Pop the first classloader on this off that has a non NULL gcThreadNotification
	 * 
	 * @return classLoader the first class loader on the list with a non NULL gcThreadNotification or NULL
	 */
	J9ClassLoader *popRequiredClassLoaderForForcedClassLoaderUnload();
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

	/**
	 * Determine if any of the lists of objects managed by the receiver are non-empty.
	 * @return true if any of the lists are non-empty, false if they are all empty
	 */
	MMINLINE bool isFinalizableObjectProcessingRequired()
	{
		bool result = false;
		if (NULL != peekSystemFinalizableObject()) {
			result = true;
		} else if (NULL != peekDefaultFinalizableObject()) {
			result = true;
		} else if (NULL != peekReferenceObject()) {
			result = true;
		}
		return result;
	}
	
	/**
	 * Pop the next job to process
	 * 
	 * @note Must be called while holding this class' _mutex
	 *
	 * @return the next job or NULL
	 */
	virtual GC_FinalizeJob *consumeJob(J9VMThread *vmThread, GC_FinalizeJob * job);


	/**
	 * Create a FinalizeListManager object
	 */
	GC_FinalizeListManager(MM_GCExtensions *extensions) :
		_extensions(extensions)
		,_mutex(NULL)
		,_systemFinalizableObjects(NULL)
	    ,_systemFinalizableObjectCount(0)
	    ,_defaultFinalizableObjects(NULL)
	    ,_defaultFinalizableObjectCount(0)
	    ,_referenceObjects(NULL)
	    ,_referenceObjectCount(0)
	    ,_classLoaders(NULL)
	    ,_classLoaderCount(0)
	{
		_typeId = __FUNCTION__;
	};

	/*
	 * Friends
	 */
	friend class GC_CheckFinalizableList; /* Temporary until DDR gccheck is updated to iterate multi-tenant finalizable queues */
};
#endif /* J9VM_GC_FINALIZATION */
#endif /* FINALIZELISTMANAGER_HPP_ */
