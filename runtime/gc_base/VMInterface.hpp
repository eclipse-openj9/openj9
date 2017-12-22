
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

/**
 * @file
 * @ingroup GC_Base
 */

#if !defined(VMINTERFACE_HPP_)
#define VMINTERFACE_HPP_

#include "j9.h"
#include "j9cfg.h"

class MM_EnvironmentBase;
class MM_GCExtensions;
struct J9HookInterface;

/**
 * @todo Provide class documentation
 * @ingroup GC_Base
 */
class GC_VMInterface
{
private:
protected:
public:
	static J9HookInterface** getHookInterface(MM_GCExtensions *extensions);

	/**
	 * @ingroup GC_Base
	 * @name Locking VM Structures
	 * When using these methods to lock VM structures, pay attention to possible
	 * issues with VM access. 
	 * @{
	 */
	static void lockClassTable(MM_GCExtensions *extensions);
	static void unlockClassTable(MM_GCExtensions *extensions);
	static void lockClassMemorySegmentList(MM_GCExtensions *extensions);
	static void unlockClassMemorySegmentList(MM_GCExtensions *extensions);
	static void lockClasses(MM_GCExtensions *extensions);
	static void unlockClasses(MM_GCExtensions *extensions);
	static void lockJNIGlobalReferences(MM_GCExtensions *extensions);
	static void unlockJNIGlobalReferences(MM_GCExtensions *extensions);
	static void lockVMThreadList(MM_GCExtensions *extensions);
	static void unlockVMThreadList(MM_GCExtensions *extensions);
	static void lockFinalizeList(MM_GCExtensions *extensions);
	static void unlockFinalizeList(MM_GCExtensions *extensions);
	static void lockClassLoaders(MM_GCExtensions *extensions);
	static void unlockClassLoaders(MM_GCExtensions *extensions);
	/**
	 * @}
	 */
};

#endif /* VMINTERFACE_HPP_ */

