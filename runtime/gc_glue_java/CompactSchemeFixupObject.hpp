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
 
#ifndef COMPACTSCHEMEOBJECTFIXUP_HPP_
#define COMPACTSCHEMEOBJECTFIXUP_HPP_

#include "j9cfg.h"
#include "objectdescription.h"

#include "CompactScheme.hpp"
#include "GCExtensions.hpp"

class MM_CompactSchemeFixupObject {
public:
protected:
private:
	OMR_VM *_omrVM;
	MM_GCExtensions *_extensions;
	MM_CompactScheme *_compactScheme;
public:

	/**
	 * Perform fixup for a single object
	 * @param env[in] the current thread
	 * @param objectPtr pointer to object for fixup
	 */
	void fixupObject(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr);

	static void verifyForwardingPtr(omrobjectptr_t objectPtr, omrobjectptr_t forwardingPtr);

	MM_CompactSchemeFixupObject(MM_EnvironmentBase* env, MM_CompactScheme *compactScheme) :
		_omrVM(env->getOmrVM()),
		_extensions(MM_GCExtensions::getExtensions(env)),
		_compactScheme(compactScheme)
	{}

protected:
private:
	/**
	 * Perform fixup for a mixed object
	 * @param objectPtr pointer to object for fixup
	 */
	void fixupMixedObject(omrobjectptr_t objectPtr);

	/**
	 * Perform fixup for an array object
	 * @param objectPtr pointer to object for fixup
	 */
	void fixupArrayObject(omrobjectptr_t objectPtr);

	/**
	 * Called whenever a ownable synchronizer object is fixed up during compact. Places the object on the thread-specific buffer of gc work thread.
	 * @param env -- current thread environment
	 * @param object -- The object of type or subclass of java.util.concurrent.locks.AbstractOwnableSynchronizer.
	 */
	MMINLINE void addOwnableSynchronizerObjectInList(MM_EnvironmentBase *env, omrobjectptr_t objectPtr);
};

#endif /* COMPACTSCHEMEOBJECTFIXUP_HPP_ */
