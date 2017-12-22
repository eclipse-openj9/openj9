
/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#if !defined(MARKINGSCHEMEROOTMARKER_HPP_)
#define MARKINGSCHEMEROOTMARKER_HPP_

#include "j9.h"

#include "RootScanner.hpp"

class MM_MarkingScheme;
class MM_MarkingDelegate;

class MM_MarkingSchemeRootMarker : public MM_RootScanner
{
/* Data members & types */
public:
protected:
private:
	MM_MarkingScheme *_markingScheme;
	MM_MarkingDelegate *_markingDelegate;

/* Methods */
public:
	MM_MarkingSchemeRootMarker(MM_EnvironmentBase *env, MM_MarkingScheme *markingScheme, MM_MarkingDelegate *markingDelegate) :
		  MM_RootScanner(env)
		, _markingScheme(markingScheme)
		, _markingDelegate(markingDelegate)
	{
		_typeId = __FUNCTION__;
	};

	virtual void doSlot(omrobjectptr_t *slotPtr);
	virtual void doStackSlot(omrobjectptr_t *slotPtr, void *walkState, const void* stackLocation);
	virtual void doVMThreadSlot(omrobjectptr_t *slotPtr, GC_VMThreadIterator *vmThreadIterator);
	virtual void doClass(J9Class *clazz);
	virtual void doClassLoader(J9ClassLoader *classLoader);
	virtual void doFinalizableObject(omrobjectptr_t object);

protected:
private:
};

#endif /* MARKINGSCHEMEROOTMARKER_HPP_ */
