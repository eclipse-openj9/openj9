
/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "Wildcard.hpp"

#include "util_api.h"

#include "GCExtensions.hpp"

MM_Wildcard::MM_Wildcard(uint32_t matchFlag, const char *needle, uintptr_t needleLength, char *pattern)
	: MM_BaseNonVirtual()
	, _next(NULL)
	, _matchFlag(matchFlag)
	, _needle(needle)
	, _needleLength(needleLength)
	, _pattern(pattern)
{
	_typeId = __FUNCTION__;
}

MM_Wildcard *
MM_Wildcard::newInstance(MM_GCExtensions *extensions, uint32_t matchFlag, const char *needle, uintptr_t needleLength, char *pattern)
{
	MM_Wildcard *wildcard = (MM_Wildcard *)extensions->getForge()->allocate(sizeof(MM_Wildcard), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL != wildcard) {
		new(wildcard) MM_Wildcard(matchFlag, needle, needleLength, pattern);
		if (!wildcard->initialize(extensions)) {
			wildcard->kill(extensions);
			return NULL;
		}
	} else {
		OMRPORT_ACCESS_FROM_OMRVM(extensions->getOmrVM());
		omrmem_free_memory(pattern);
	}
	return wildcard;
}

void
MM_Wildcard::kill(MM_GCExtensions *extensions)
{
	tearDown(extensions);
	extensions->getForge()->free(this);
}

bool
MM_Wildcard::initialize(MM_GCExtensions *extensions)
{
	return true;
}

void
MM_Wildcard::tearDown(MM_GCExtensions *extensions)
{
	OMRPORT_ACCESS_FROM_OMRVM(extensions->getOmrVM());
	omrmem_free_memory(_pattern);
}

bool 
MM_Wildcard::match(const char *haystack, uintptr_t haystackLength)
{
	intptr_t rc = wildcardMatch(_matchFlag, _needle, _needleLength, haystack, haystackLength);
	return rc != FALSE;
}
