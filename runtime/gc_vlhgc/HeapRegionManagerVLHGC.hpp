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

#if !defined(HEAPREGIONMANAGERVLHGC_HPP)
#define HEAPREGIONMANAGERVLHGC_HPP

#include "HeapRegionManagerTarok.hpp"

class MM_EnvironmentBase;
class MM_MemboryHandle;
class MM_HeapMemorySnapshot;

class MM_HeapRegionManagerVLHGC : public MM_HeapRegionManagerTarok
{
	/*
	 * Function members
	 */
public:
	/**
	 * @see MM_HeapRegionManagerTarok::enableRegionsInTable
	 */
	virtual bool enableRegionsInTable(MM_EnvironmentBase *env, MM_MemoryHandle *handle);

	virtual MM_HeapMemorySnapshot* getHeapMemorySnapshot(MM_GCExtensionsBase *extensions, MM_HeapMemorySnapshot* snapshot, bool gcEnd);

	static MM_HeapRegionManagerVLHGC *newInstance(MM_EnvironmentBase *env, UDATA regionSize, UDATA tableDescriptorSize, MM_RegionDescriptorInitializer regionDescriptorInitializer, MM_RegionDescriptorDestructor regionDescriptorDestructor);
	MM_HeapRegionManagerVLHGC(MM_EnvironmentBase *env, UDATA regionSize, UDATA tableDescriptorSize, MM_RegionDescriptorInitializer regionDescriptorInitializer, MM_RegionDescriptorDestructor regionDescriptorDestructor);

protected:
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);

private:

};

#endif /* HEAPREGIONMANAGERVLHGC_HPP */
