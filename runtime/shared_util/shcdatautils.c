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
#if defined(J9VM_OUT_OF_PROCESS)
#include "j9dbgext.h"
#endif

#include "shcdatautils.h"

/* This is a bunch of utility functions for walking a cache in and out of process */

/* Given a VM, this returns a pointer to the start of the metadata, the first metadata entry and the length
 * This is designed to match the dbg version of this function which is in dbgext.c */
void*
shcReadSharedCacheMetadata(J9JavaVM* vm, UDATA* length, ShcItemHdr** firstEntry)
{
	if (vm->sharedClassConfig) {
		J9SharedClassConfig* config = vm->sharedClassConfig;

		if (config->metadataMemorySegment) {
			J9MemorySegment* metadataMemorySegment = config->metadataMemorySegment;
			UDATA size = (metadataMemorySegment->heapTop - metadataMemorySegment->heapBase);

			if (size > 0) {
				void* metaStart = metadataMemorySegment->heapBase;
			
				if (length) {
					*length = size;
				}
				if (firstEntry) {
					*firstEntry = (ShcItemHdr*)((((U_8*)metaStart) + size) - sizeof(ShcItemHdr)); 
				}
				
				return metaStart;
			}
		}
	}
	return NULL;
}

/**
 * Walks a cache in or out of process.
 * 
 * @see shcSharedClassMetadataEntriesStartDo
 *
 * @param[in] vm  A javavm
 * @param[in] state  A walk state struct
 * 
 * @return The next item in the cache corresponding to the limits specified
 */
ShcItem*
shcSharedClassMetadataEntriesNextDo(J9SharedClassMetadataWalkState* state)
{
	ShcItemHdr* entry = state->entry;
	ShcItem* current = NULL;
	ShcItem* walk;
	ShcItemHdr* nextEntry = entry;
	
	if (entry == NULL) {
		return NULL;
	}

	do {
		UDATA type;

		entry = nextEntry;
		nextEntry = (ShcItemHdr*)CCITEMNEXT(entry);
		if ((UDATA)nextEntry <= (UDATA)state->metaStart) {
			if (state->savedEntry != NULL) {
				state->metaStart = state->savedMetaStart;
				nextEntry = state->savedEntry;
				state->savedEntry = NULL;
				if ((UDATA)nextEntry <= (UDATA)state->metaStart) {
					/* Once we've finished walking the cache, set to NULL */
					nextEntry = NULL;
				}
			} else {
				/* Once we've finished walking the cache, set to NULL */
				nextEntry = NULL;
			}
		}

		walk = (ShcItem*)CCITEM(entry);
		type = J9SHR_READMEM(walk->dataType);

		if (type == TYPE_CACHELET) {
			CacheletWrapper *cw = (CacheletWrapper*)ITEMDATA(walk);
			J9SharedCacheHeader *header = (J9SharedCacheHeader*)CLETDATA(cw);
			UDATA totalBytes = J9SHR_READMEM(header->totalBytes);
			UDATA updatePtr = (UDATA)header + J9SHR_READMEM(header->updateSRP);
			UDATA metadataEnd = (UDATA)header + totalBytes;

			/* skip empty cachelets */
			if ((metadataEnd - sizeof(ShcItemHdr)) > updatePtr) {
				state->savedEntry = nextEntry;
				state->metaStart = (void*)updatePtr;
				nextEntry = (ShcItemHdr *)(metadataEnd - sizeof(ShcItemHdr));
			}
		}

		if ((!state->limitDataType || (type == state->limitDataType)) &&
			(state->includeStale || (!CCITEMSTALE(entry)))
		) {
			current = walk;
			break;
		}
	} while (nextEntry);
	state->entry = nextEntry;

	return current;
}

/**
 * Walks a cache in or out of process.
 * If limitDataType is zero, all entries are returned. To limit to a specific data type, set limitDataType to one of the values in shcdatatypes.h
 * If includeStale is zero, stale entries are included in the walk. Otherwise, stale entries are skipped.
 *  
 * Note that in the out-of-process version, the metadata area is dbgMalloc'd by this function. 
 * This memory is freed when nextDo reaches the end of the metadata area.
 * 
 * Note also that state->reloc returns the difference in bytes between the dbgMalloc'd memory address and the dump address.
 * To get the dump address, add state->reloc to any address returned. 

 * @param[in] vm  A javavm
 * @param[in] state  A walk state struct
 * @param[in] limitDataType  Non-zero if the data type returned should be limited
 * @param[in] includeStale  Non-zero if stale classes should be included
 * 
 * @return The first item in the cache corresponding to the limits specified
 */
ShcItem*
shcSharedClassMetadataEntriesStartDo(J9JavaVM* vm, J9SharedClassMetadataWalkState* state, UDATA limitDataType, UDATA includeStale)
{
	void* metaStart;
	UDATA length;
	ShcItemHdr* firstEntry;
	
#if defined(J9VM_OUT_OF_PROCESS)
	if (!(metaStart = dbgReadSharedCacheMetadata(vm, &length, &firstEntry))) {
#else
	if (!(metaStart = shcReadSharedCacheMetadata(vm, &length, &firstEntry))) {
#endif
		return NULL;
	}
	
	state->metaStart = metaStart;
	state->savedMetaStart = metaStart;
	state->entry = firstEntry;
	state->savedEntry = NULL;
	state->length = length;
	state->limitDataType = (U_16)limitDataType;
	state->includeStale = includeStale;

	return shcSharedClassMetadataEntriesNextDo(state);
}
