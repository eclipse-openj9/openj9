
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

#if !defined(GENERATIONALACCESSBARRIERCOMPONENT_HPP_)
#define GENERATIONALACCESSBARRIERCOMPONENT_HPP_


#include "j9.h"
#include "j9cfg.h"	
#include "BaseNonVirtual.hpp"

#if defined(J9VM_GC_GENERATIONAL)

class MM_EnvironmentBase;

/**
 * Generational Component for Access barrier for Modron collector.
 */
 
class MM_GenerationalAccessBarrierComponent : public MM_BaseNonVirtual
{
private:
protected:
	
public:
	bool initialize(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);

	void postObjectStore(J9VMThread *vmThread, J9Object *dstObject, J9Object *srcObject);
	void preBatchObjectStore(J9VMThread *vmThread, J9Object *dstObject);
	
	MM_GenerationalAccessBarrierComponent()
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* J9VM_GC_GENERATIONAL */

#endif /* GENERATIONALACCESSBARRIERCOMPONENT_HPP_ */
