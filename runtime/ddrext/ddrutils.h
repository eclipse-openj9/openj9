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

#ifndef _DDR_UTILS_H
#define _DDR_UTILS_H

#include <stdarg.h>

#include "ddr.h"

typedef struct dbgRegionIteratorState {
	/* NOTE: the types are chosen to match the unsigned ints that the dbx region call expects */
	unsigned int totalRegionCount;	/* the total number of regions */
	unsigned int regionIndex;	/* the index of the next region to be enquired upon */
} dbgRegionIteratorState;

typedef struct dbgRegion {
	/**
	 * NOTE: the types are chosen to match the way the values are commonly used within ddrutils.c
	 * Note however that dbx returns uint64_t within the dbx region info structure and these
	 * values are assigned in with the appropriate cast. This sounds like it might lose
	 * precision on 32-bit platforms but we trust that dbx will return only 32-bits' worth
	 * of data on 32-bit platforms.
	 */
	U_8* start;		/* starting address of the region */
	U_8* last;		/* address of the last byte of the region */
	UDATA size;			/* size of the region */
} dbgRegion;

extern void dbgReadMemory (UDATA address, void *structure, UDATA size, UDATA *bytesRead);
extern void dbgWriteString (const char* message);
extern void dbgWrite (const char* message, ...);
extern int dbgGetThreadList(UDATA * threadCount, UDATA ** threadList);
extern int dbgGetRegisters(UDATA tid, UDATA * registerCount, ddrRegister ** registerList);
extern int dbgGetTargetArchitecture(void);
extern void dbgVPrint (const char* message, va_list arg_ptr);
extern void dbgPrint (const char* message, ...);
extern void* dbgFindPatternInRange(U_8* pattern, UDATA patternLength, UDATA patternAlignment, U_8* startSearchFrom, UDATA bytesToSearch, UDATA* bytesSearched);
extern int dbgAliasCommand(char * alias);
extern int dbgAliasInitialize();
extern BOOLEAN dbgSupportsRegions(void);
extern int dbgRegionIteratorInit(dbgRegionIteratorState *state);
extern BOOLEAN dbgRegionIteratorHasNext(dbgRegionIteratorState *state);
extern int dbgRegionIteratorNext(dbgRegionIteratorState *state, dbgRegion *region);


#endif
