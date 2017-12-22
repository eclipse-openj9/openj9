
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

#ifndef UNFINALIZEDOBJECTBUFFER_HPP_
#define UNFINALIZEDOBJECTBUFFER_HPP_

#include "j9.h"
#include "j9cfg.h"

#include "BaseVirtual.hpp"

class MM_EnvironmentBase;
class MM_GCExtensions;
class MM_HeapRegionDescriptor;

/**
 * A per-thread buffer of recently allocated unfinalized objects.
 * The buffer is periodically flushed to the global list. 
 */
class MM_UnfinalizedObjectBuffer : public MM_BaseVirtual
{
private:
protected:
	j9object_t _head; /**< the head of the linked list of unfinalized objects */
	j9object_t _tail; /**< the tail of the linked list of unfinalized objects */
	MM_HeapRegionDescriptor* _region;  /**< the region in which all buffered objects are located */
	UDATA _objectCount; /**< the number of buffered objects */
	const UDATA _maxObjectCount; /**< the maximum number of objects permitted before a forced flush */
	MM_GCExtensions * const _extensions; /**< a cached pointer to the extensions structure */
public:
	
private:
	
	/**
	 * Reset a flushed buffer to the empty state.
	 */
	void reset();

protected:

	virtual bool initialize(MM_EnvironmentBase *env) = 0;
	virtual void tearDown(MM_EnvironmentBase *en) = 0;

	/**
	 * Flush the contents of the buffer to the appropriate global buffers.
	 * Subclasses must override.
	 * @param env[in] the current thread
	 */
	virtual void flushImpl(MM_EnvironmentBase* env);
	
public:

	void kill(MM_EnvironmentBase *env);

	/**
	 * Add the specified unfinalized object to the buffer.
	 * @param env[in] the current thread
	 * @param object[in] the object to add
	 */
	void add(MM_EnvironmentBase* env, j9object_t object);
	
	/**
	 * Flush the contents of the buffer to the appropriate global buffers.
	 * @param env[in] the current thread
	 */
	void flush(MM_EnvironmentBase* env);

	/**
	 * Construct a new buffer.
	 * @param extensions[in] the GC extensions
	 * @param maxObjectCount the maximum number of objects permitted before a forced flush 
	 */
	MM_UnfinalizedObjectBuffer(MM_GCExtensions *extensions, UDATA maxObjectCount);
};

#endif /* UNFINALIZEDOBJECTBUFFER_HPP_ */
