
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

#ifndef COLLECTORLANGUAGEINTERFACEJAVA_HPP_
#define COLLECTORLANGUAGEINTERFACEJAVA_HPP_

#include "j9.h"
#include "CollectorLanguageInterface.hpp"
#include "CompactScheme.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "HeapRegionDescriptorStandard.hpp"
#include "HeapWalker.hpp"
#include "MarkingScheme.hpp"
#include "ScanClassesMode.hpp"

class GC_ObjectScanner;
class GC_VMThreadIterator;
class MM_CompactScheme;
class MM_EnvironmentStandard;
class MM_ForwardedHeader;
class MM_MemorySubSpaceSemiSpace;
class MM_Scavenger;
class MM_ParallelSweepScheme;

/**
 * Class representing a collector language interface.  This implements the API between the OMR
 * functionality and the language being implemented.
 * @ingroup GC_Base
 */
class MM_CollectorLanguageInterfaceImpl : public MM_CollectorLanguageInterface {
private:
	MM_GCExtensions *_extensions;

protected:
public:
private:
protected:
	bool initialize(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);

	MM_CollectorLanguageInterfaceImpl(J9JavaVM *vm)
		: MM_CollectorLanguageInterface()
		,_extensions(MM_GCExtensions::getExtensions(vm))
	{
		_typeId = __FUNCTION__;
	}

public:
	static MM_CollectorLanguageInterfaceImpl *newInstance(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);

	static MM_CollectorLanguageInterfaceImpl *getInterface(MM_CollectorLanguageInterface *cli) { return (MM_CollectorLanguageInterfaceImpl *)cli; }
};

#endif /* COLLECTORLANGUAGEINTERFACEJAVA_HPP_ */

