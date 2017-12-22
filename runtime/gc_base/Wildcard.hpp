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

#if !defined(WILDCARD_HPP_)
#define WILDCARD_HPP_

#include "j9.h"
#include "j9cfg.h"

#include "BaseNonVirtual.hpp"

class MM_GCExtensions;

class MM_Wildcard : public MM_BaseNonVirtual
{
/* member data */
public:
	class MM_Wildcard *_next; /**< A pointer which can be used to build a linked list of WildCards */
protected:
private:
	const U_32 _matchFlag; /**< a matchflag argument for wildcardMatch() */
	const char* const _needle; /**< a needle argument for wildcardMatch() */
	const UDATA _needleLength; /**< the length of the needle argument for wildcardMatch() */
	char* const _pattern; /**< a block of memory which contains needle and which must be freed by the receiver */
	
/* member functions */
public:
	static MM_Wildcard *newInstance(MM_GCExtensions *extensions, U_32 matchFlag, const char* needle, UDATA needleLength, char* pattern);
	void kill(MM_GCExtensions *extensions);

	bool match(const char* haystack, UDATA haystackLength);
		
protected:
	bool initialize(MM_GCExtensions *extensions);
	void tearDown(MM_GCExtensions *extensions);

private:
	MM_Wildcard(U_32 matchFlag, const char* needle, UDATA needleLength, char* pattern);
};

#endif /* WILDCARD_HPP_ */
