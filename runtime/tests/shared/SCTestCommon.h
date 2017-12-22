/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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

#if !defined(SCTESTCOMMON_H_INCLUDED)
#define SCTESTCOMMON_H_INCLUDED

#include "j9port.h"
#include "CacheMap.hpp"
#include "CompositeCacheImpl.hpp"
#include "CacheLifecycleManager.hpp"
#include "sharedconsts.h"
#include "shcflags.h"
#include "main.h"
#include "j9.h"

#ifdef __cplusplus
extern "C" {
#endif

/* It is necessary to allocate at least (2 * OS PageSize) amount of cache memory during unit testing,
 * If cache size is less than (2 * OS PageSize), SH_CompositeCacheImpl::startup() will change it to at least (2 * OS PageSize),
 * which can cause unpredictable behavior.
 */
#define CACHE_SIZE (16*1024*1024)

/* In JAZZ Design 56452, cache is marked full when free block bytes goes below CC_MIN_SPACE_BEFORE_CACHE_FULL.
 * This causes AttachedDataMinMaxTests and AOTDataMinMaxTests to fail on Windows, as now the cache gets full
 * before it is expected to be.
 * By increasing the cache size by 2K on Windows, cache gets full as expected.
 */
#if defined(WIN32)
#define SMALL_CACHE_SIZE (10*1024)
#else
#define SMALL_CACHE_SIZE (2*4*1024)
#endif /* defined(WIN32) */
#define NUM_DATA_OBJECTS 6
#define JIT_PROFILE_OBJECT_BASE 0
#define JIT_PROFILE_OBJECT_LIMIT (NUM_DATA_OBJECTS/2)
#define JIT_HINT_OBJECT_BASE JIT_PROFILE_OBJECT_LIMIT
#define JIT_HINT_OBJECT_LIMIT NUM_DATA_OBJECTS

struct minmaxStruct {
	/* Minimum/reserved space for storing data */
	I_32 min;
	/* Maximum space that can be used for storing data */
	I_32 max;  // maximum space
	/* At which index(0..NUM_DATA_OBJECTS-1)that above min/max settings will cause fail(J9SHR_RESOURCE_STORE_FULL)
	 * while storing data. Making assumption that, in each iteration
	 * we store data = (Initial free size in cache) / (num. of iterations)
	*/
	IDATA failingIndex;
};


#define SCPRINTF(err) \
do { \
	j9tty_printf(PORTLIB,"\n\t"); \
	if(err) { \
		j9tty_printf(PORTLIB,"ERROR: %s",__FILE__); \
	} else { \
		j9tty_printf(PORTLIB,"INFO: %s",__FILE__); \
	} \
	j9tty_printf(PORTLIB,"(%d)",__LINE__); \
	j9tty_printf(PORTLIB," %s ",testName); \
}while (0)


#define ERRPRINTF(args) \
do { \
    SCPRINTF(true); \
	j9tty_printf(PORTLIB,args); \
}while (0)

#define ERRPRINTF1(args,p1) \
do { \
	SCPRINTF(true); \
	j9tty_printf(PORTLIB,args,p1); \
}while (0)

#define ERRPRINTF2(args,p1,p2) \
do { \
	SCPRINTF(true); \
	j9tty_printf(PORTLIB,args,p1,p2); \
}while (0)

#define ERRPRINTF3(args,p1,p2,p3) \
do { \
	SCPRINTF(true); \
	j9tty_printf(PORTLIB,args,p1,p2,p3); \
}while (0)

#define ERRPRINTF4(args,p1,p2,p3,p4) \
do { \
	SCPRINTF(true); \
	j9tty_printf(PORTLIB,args,p1,p2,p3,p4); \
}while (0)

#define ERRPRINTF5(args,p1,p2,p3,p4,p5) \
do { \
	SCPRINTF(true); \
	j9tty_printf(PORTLIB,args,p1,p2,p3,p4,p5); \
}while (0)

#define INFOPRINTF(args) \
do { \
	SCPRINTF(false); \
	j9tty_printf(PORTLIB,args); \
}while (0)

#define INFOPRINTF1(args,p1) \
do { \
	SCPRINTF(false); \
	j9tty_printf(PORTLIB,args,p1); \
}while (0)

#define INFOPRINTF2(args,p1,p2) \
do { \
	SCPRINTF(false); \
	j9tty_printf(PORTLIB,args,p1,p2); \
}while (0)

#define INFOPRINTF3(args,p1,p2,p3) \
do { \
	SCPRINTF(false); \
	j9tty_printf(PORTLIB,args,p1,p2,p3); \
}while (0)

#define INFOPRINTF4(args,p1,p2,p3,p4) \
do { \
	SCPRINTF(false); \
	j9tty_printf(PORTLIB,args,p1,p2,p3,p4); \
}while (0)

#define INFOPRINTF5(args,p1,p2,p3,p4,p5) \
do { \
	SCPRINTF(false); \
	j9tty_printf(PORTLIB,args,p1,p2,p3,p4,p5); \
}while (0)

#define INFOPRINTF6(args,p1,p2,p3,p4,p5,p6) \
do { \
	SCPRINTF(false); \
	j9tty_printf(PORTLIB,args,p1,p2,p3,p4,p5,p6); \
}while (0)


void SetRomClassName(J9JavaVM* vm, J9ROMClass * rc, const char * name);

#ifdef __cplusplus
}
#endif
#endif /* !defined(SCTESTCOMMON_H_INCLUDED) */
