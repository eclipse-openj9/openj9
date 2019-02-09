
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

#ifndef REFERENCEOBJECTBUFFER_HPP_
#define REFERENCEOBJECTBUFFER_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "BaseVirtual.hpp"

class MM_EnvironmentBase;
class MM_HeapRegionDescriptor;
class MM_ReferenceObjectList;

/**
 * A per-thread buffer of reachable reference objects.
 * The buffer is periodically flushed to the global list. 
 */
class MM_ReferenceObjectBuffer : public MM_BaseVirtual
{
private:
protected:
	j9object_t _head; /**< the head of the linked list of reference objects */
	j9object_t _tail; /**< the tail of the linked list of reference objects */
	MM_HeapRegionDescriptor* _region;  /**< the region in which all buffered objects are located */
	UDATA _referenceObjectType; /** the type of objects being buffered */
	UDATA _objectCount; /**< the number of buffered objects */
	const UDATA _maxObjectCount; /**< the maximum number of objects permitted before a forced flush */
public:
	
private:
	
	/**
	 * Reset a flushed buffer to the empty state.
	 */
	void reset();

	/**
	 * Determine the type (weak/soft/phantom) of the specified reference object.
	 * @param object[in] the object to examine
	 * @return one of J9AccClassReferenceWeak, J9AccClassReferenceSoft or J9AccClassReferencePhantom
	 */
	UDATA getReferenceObjectType(j9object_t object);
	
protected:
	
	virtual bool initialize(MM_EnvironmentBase *env) = 0;
	virtual void tearDown(MM_EnvironmentBase *env) = 0;

	/**
	 * Flush the contents of the buffer to the appropriate global buffers.
	 * Subclasses must override.
	 * @param env[in] the current thread
	 */
	virtual void flushImpl(MM_EnvironmentBase* env) = 0;
	
public:

	void kill(MM_EnvironmentBase *env);

	/**
	 * Add the specified reference object to the buffer.
	 * @param env[in] the current thread
	 * @param object[in] the object to add
	 * @return true if the addition succeeded, false if the list must be flushed first
	 */
	void add(MM_EnvironmentBase* env, j9object_t object);
	
	/**
	 * Flush the contents of the buffer to the appropriate global buffers.
	 * @param env[in] the current thread
	 */
	void flush(MM_EnvironmentBase* env);

	/**
	 * Determine if the buffer is empty.
	 * @return true if there are no objects in the buffer
	 */
	bool isEmpty() { return NULL == _head; }
	
	/**
	 * Construct a new buffer.
	 * @param maxObjectCount the maximum number of objects permitted before a forced flush 
	 */
	MM_ReferenceObjectBuffer(UDATA maxObjectCount);
};

#endif /* REFERENCEOBJECTBUFFER_HPP_ */
