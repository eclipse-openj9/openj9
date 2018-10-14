/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
#if !defined(OSCACHETEST_HPP_INCLUDED)
#define OSCACHETEST_HPP_INCLUDED

#include "OSCache.hpp"
#include "j9.h"
#include "main.h"
#include <string.h>

#define CACHE_NAME "OSCacheUnitTest"
#define CACHE_NAME1 "OSCacheUnitTest1"

class Init : public SH_OSCache::SH_OSCacheInitializer
{
public:
	void *operator new(size_t t, void* i) { return i; }
	virtual void init(char* data, U_32 len, I_32 minAOT, I_32 maxAOT, I_32 minJIT, I_32 maxJIT, U_32 readWriteLen, U_32 sotfMaxBytes)
	{
		/* Copy some dummy data into the cache - use it to test reading the data back later */
		memcpy(data, "abcde", 5);	
	}
};

#endif /* OSCACHETEST_HPP_INCLUDED */
