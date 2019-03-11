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
 * @ingroup GC_Modron_Standard
 */

#if !defined(INCREMENTALOVERFLOW_HPP_)
#define INCREMENTALOVERFLOW_HPP_

#include "EnvironmentBase.hpp"
#include "Metronome.hpp"
#include "WorkPacketOverflow.hpp"

class MM_Packet;
class MM_HeapRegionDescriptorRealtime;

/**
 * @todo Provide class documentation
 */
class MM_IncrementalOverflow : public MM_WorkPacketOverflow
{
/* Data members & types */
public:
protected:
private:
	MM_GCExtensionsBase *_extensions;
	MM_HeapRegionDescriptorRealtime *_overflowList;
	bool _overflowThisGCCycle; /**< set to signify an overflow happened sometime during GC cycle */
	
/* Methods */
public:
	static MM_IncrementalOverflow *newInstance(MM_EnvironmentBase *env, MM_WorkPackets *workPackets);
	virtual void reset(MM_EnvironmentBase *env);
	
	virtual bool isEmpty()
	{
		return (_overflowList == NULL);
	}
	
	bool isOverflowThisGCCycle() const { return _overflowThisGCCycle; }
	void resetOverflowThisGCCycle() { _overflowThisGCCycle = false; }
	
	virtual void emptyToOverflow(MM_EnvironmentBase *env, MM_Packet *packet, MM_OverflowType type);
	virtual void fillFromOverflow(MM_EnvironmentBase *env, MM_Packet *packet);
	virtual void overflowItem(MM_EnvironmentBase *env, void *item, MM_OverflowType type);

	/**
	 * Create a IncrementalOverflow object.
	 */	
	MM_IncrementalOverflow(MM_EnvironmentBase *env, MM_WorkPackets *workPackets) :
		MM_WorkPacketOverflow(env, workPackets),
		_overflowList(NULL),
		_overflowThisGCCycle(false)
	{
		_typeId = __FUNCTION__;
	};
	
protected:
	bool initialize(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);
private:
	void overflowItemInternal(MM_EnvironmentBase *env, void *item, MM_OverflowType type);
	void push(MM_EnvironmentBase *env, MM_HeapRegionDescriptorRealtime *region);
	void pushNoLock(MM_EnvironmentBase *env, MM_HeapRegionDescriptorRealtime *region);
	MM_HeapRegionDescriptorRealtime *pop(MM_EnvironmentBase *env);
	void pushLocal(MM_EnvironmentBase *env, MM_HeapRegionDescriptorRealtime *region, MM_OverflowType type);
	void flushLocal(MM_EnvironmentBase *env, MM_OverflowType type);
};

#endif /* INCREMENTALOVERFLOW_HPP_ */
