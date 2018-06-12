/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "ddrutils.h"


/* old file_write function */
static IDATA (*old_write)(J9PortLibrary *portLibrary, IDATA fd, const void * buf, IDATA nbytes);
static IDATA dbg_write (J9PortLibrary *portLibrary, IDATA fd, const void * buf, IDATA nbytes);
#if defined(J9ZOS390)
static IDATA dbg_write_text (J9PortLibrary *portLibrary, IDATA fd, const char * buf, IDATA nbytes);
#endif

#define DEFAULT_STRING_LEN 8192

/*
 * Prints message to the debug print screen
 */
void
dbgPrint (const char* message, ...)
{
	char buf[DEFAULT_STRING_LEN];
	va_list arg_ptr;

	va_start(arg_ptr, message);

#ifdef WIN32
	_vsnprintf(buf, DEFAULT_STRING_LEN, message, arg_ptr);
#else
	vsnprintf(buf, DEFAULT_STRING_LEN, message, arg_ptr);
#endif

	dbgWriteString(buf);

	va_end(arg_ptr);
}
 

#if defined(J9ZOS390)
/* on z/OS, we redefine the portlib file_write_text as well as file_write so create this wrapper to satisfy the type system */
static IDATA
dbg_write_text(J9PortLibrary *portLibrary, IDATA fd, const char * buf, IDATA nbytes)
{
	return dbg_write(portLibrary, fd, buf, nbytes);
}
#endif /* defined(J9ZOS390) */


static IDATA 
dbg_write(J9PortLibrary *portLibrary, IDATA fd, const void * buf, IDATA nbytes)
{
	if (fd == J9PORT_TTY_OUT || fd == J9PORT_TTY_ERR) {
		dbgPrint("%.*s", nbytes, buf);
		return nbytes;
	} else {
		return old_write(portLibrary, fd, buf, nbytes);
	}
}


/*
 * Find the given pattern in the given range by a full scan, reading a page at a time.
 * Ideally the caller will have established what regions exist in the address space and
 * will only search in this way in active regions.
 *
 * pattern: a pointer to the eyecatcher pattern to search for
 * patternLength: length of pattern
 * patternAlignment: guaranteed minimum alignment of the pattern (must be a power of 2)
 * startSearchFrom: minimum address to search at (useful for multiple occurrences of the pattern)
 * bytesToSearch: maximum number of bytes to search
 * bytesSearched: number of bytes searched is returned through this pointer. 0 indicates that no attempt was made to find the pattern.
 *
 * Returns:
 *    The address of the eyecatcher in TARGET memory space
 *     or NULL if it was not found.
 *
 * NOTES:
 *    Currently, this may fail to find the pattern if patternLength > patternAlignment
 *
 *    This scan simply iterates through memory and may take a *very* long time on a 64bit address
 *    space.
 */
void*
dbgFindPatternInRangeFullScan(U_8* pattern, UDATA patternLength, UDATA patternAlignment, U_8* startSearchFrom, UDATA bytesToSearch, UDATA* bytesSearched)
{
	U_8* page = startSearchFrom;
	UDATA align;

	/* round to a page */
	align = (UDATA)page & 4095;
	page -= align;
	bytesToSearch += align;

	*bytesSearched = 0;

	for (;;) {
		U_8 data[4096];
		UDATA bytesRead = 0;

		dbgReadMemory((UDATA)page, data, sizeof(data), &bytesRead);
		if (bytesRead) { 
			UDATA i;
			if (bytesRead > bytesToSearch) {
				bytesRead = bytesToSearch;
			} 
			*bytesSearched += bytesRead;
			for (i = 0; i < bytesRead - patternLength; i += patternAlignment) {
				if (memcmp(data + i, pattern, patternLength) == 0) {
					/* in case the page started before startSearchFrom */
					if (page + i >= startSearchFrom) {
						return page + i;
					}
				}
			}
		}

		if (bytesToSearch <= 4096) {
			/* we shall have just read the last page */
			break;
		}

		page += 4096;
		bytesToSearch -= 4096;
		/* Increment bytesSearched by the amount requested even if bytesRead is < 4096
		 * This is fairly common - e.g. bytesRead == 0 indicates that there is no committed memory
		 * But even so consider we searched it so increment bytesSearched
		 */
		*bytesSearched += 4096;
	}

	return NULL;
}


/*
 * Find the given pattern in the given address range using whatever optimisations are possible.
 * For searching using dbx, use region information if available and only do a full scan
 * within active regions. Use a full scan of all memory as a last resort. See CMVC 185304
 *
 * pattern: a pointer to the eyecatcher pattern to search for
 * patternLength: length of pattern
 * patternAlignment: guaranteed minimum alignment of the pattern (must be a power of 2)
 * startSearchFrom: minimum address to search at (useful for multiple occurrences of the pattern)
 * bytesToSearch: maximum number of bytes to search
 * bytesSearched: number of bytes searched is returned through this pointer. 0 indicates that no attempt was made to find the pattern.
 *
 * Returns:
 *    The address of the eyecatcher in TARGET memory space
 *     or NULL if it was not found.
 *
 * NOTES:
 *    There is no check here to stop startSearchFrom + bytesToSearch going beyond the end of the
 *    address space. If this happens then different results are possible from the full scan
 *    version of the search (which will wrap round to the beginning of memory and carry on) and
 *    the search within regions (which will stop with the last region)
 *
 */
void*
dbgFindPatternInRange(U_8* pattern, UDATA patternLength, UDATA patternAlignment, U_8* startSearchFrom, UDATA bytesToSearch, UDATA* bytesSearched)
{
	U_8* patternAddress = NULL;
	U_8* lastByteToSearch = startSearchFrom + bytesToSearch - 1;

	*bytesSearched = 0;

	if (dbgSupportsRegions()) {
		U_8*  whereToStartTheSearchWithinTheRegion = NULL;
		U_8*  lastByteToSearchWithinTheRegion = NULL;
		UDATA bytesToSearchWithinTheRegion = 0;
		UDATA bytesSearchedWithinTheRegion = 0;
		dbgRegionIteratorState regionIteratorState; /* initialized by subsequent memset */
		dbgRegion region; /* initialized by subsequent memset */
		IDATA result = 0;

		memset (&regionIteratorState, 0, sizeof(dbgRegionIteratorState));
		memset (&region, 0, sizeof(dbgRegion));

		result = dbgRegionIteratorInit(&regionIteratorState);
		if (DDREXT_REGIONS_SUCCESS != result) {
			dbgWriteString("Unable to create an iterator to iterate through memory regions\n");
			return NULL;
		}
		while (dbgRegionIteratorHasNext(&regionIteratorState)) {
			result = dbgRegionIteratorNext(&regionIteratorState, &region);
			if (DDREXT_REGIONS_SUCCESS != result) {
				dbgWriteString("Error obtaining the next region to search\n");
				return NULL;
			}
			if (startSearchFrom > region.last) {
				/* skip any regions lying wholly before the startSearchFrom address */
				continue;
			}
			/* start the search at the beginning of the region or within it if the overall start address lies within */
			whereToStartTheSearchWithinTheRegion = OMR_MAX(region.start, startSearchFrom);

			/* end the search at the end of the region or before if the overall end address lies within */
			lastByteToSearchWithinTheRegion = OMR_MIN(region.last, lastByteToSearch);

			bytesToSearchWithinTheRegion = (UDATA)(lastByteToSearchWithinTheRegion - whereToStartTheSearchWithinTheRegion + 1);
			bytesSearchedWithinTheRegion = 0;
			patternAddress = dbgFindPatternInRangeFullScan(pattern, patternLength, patternAlignment, whereToStartTheSearchWithinTheRegion, bytesToSearchWithinTheRegion, &bytesSearchedWithinTheRegion);
			if (NULL != patternAddress) {
				/* found the pattern, so consider that the total bytes searched is from the initial start point to where we found it (plus the pattern itself) */
				*bytesSearched = (UDATA)(patternAddress - startSearchFrom) + patternLength;
				return patternAddress;
			} else {
				/* did not find the pattern, so consider that the total bytes searched so far is from the initial start point to where we stopped within the region */
				*bytesSearched = (UDATA)(lastByteToSearchWithinTheRegion - startSearchFrom + 1);
			}
			if (*bytesSearched >= bytesToSearch) {
				return NULL;
			}
		}
		/**
		 *  At this point we have searched beyond the last region and not found the pattern, so
		 *  return a value for bytesSearched that says we searched all the way to the end
		 *  of the area we were asked to search
		 */
		*bytesSearched = (UDATA)(lastByteToSearch - startSearchFrom + 1);
		return NULL;
	} else {
		/* do it the slow way. */
		return dbgFindPatternInRangeFullScan(pattern, patternLength, patternAlignment, startSearchFrom, bytesToSearch, bytesSearched);
	}
}
