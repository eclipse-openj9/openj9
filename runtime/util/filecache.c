/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

/*
 * Notes: There are two sets of conditional statements that can be enabled:
 * 1. DEBUG: These are standard debug statements.
 * 2. COMMAND_DEBUG: Provides command output for capturing the set of public functions  
 *    called and the arguments passed to them.
 */
  
#define filecache_c
#include "filecache.h"
#include "ut_j9util.h"

static void setup_cache(J9CachedFileHandle* handle, U_8 index, I_64 base, IDATA position, U_8 inuse);
static U_8 find_oldest_cache(J9CachedFileHandle* handle);
static BOOLEAN find_next_oldest_cache(J9CachedFileHandle* handle, U_8 oldest, U_8* pNext);
static void increment_lru(J9CachedFileHandle* handle);
static I_8 find_cache_containing(J9CachedFileHandle* handle, I_64 position);
static I_8 find_possible_contention(J9CachedFileHandle* handle, I_64 position);
static IDATA flush_cache(J9CachedFileHandle* handle, U_8 index);

#if defined(DEBUG)
static BOOLEAN assert_cache_separation(struct J9PortLibrary *portLibrary, J9CachedFileHandle* handle);
#endif

#if defined(COMMAND_DEBUG)
static void print_bytes_as_text(struct J9PortLibrary *portLibrary, const void* buf, const IDATA nbytes);
static char bin2hex(U_8 bin);
#endif

#define LATEST_LRU()              handle->lru_count

#if defined(DEBUG)
#define ASSERT_CACHE_SEPARATION() assert_cache_separation(portLibrary,handle)

static BOOLEAN
assert_cache_separation(struct J9PortLibrary *portLibrary, J9CachedFileHandle* handle)
{
	U_8 i;
	U_8 j;
	I_64 base;
	I_64 compare_to_base;
	BOOLEAN clash = FALSE;
	
	PORT_ACCESS_FROM_PORT(portLibrary);

	/* go through all the caches to get their ranges */
	for (i = 0; i < CACHE_BUFFER_NUM; i++) {
		base = handle->cacheTable[i].cacheBase;
		for (j = i+1; j < CACHE_BUFFER_NUM; j++) {
			compare_to_base = handle->cacheTable[j].cacheBase;
			if ( (base < compare_to_base) && ((base + CACHE_BUFFER_SIZE -1) >= compare_to_base) ) {
				j9tty_printf(PORTLIB, ">>bases %ld", base);
				j9tty_printf(PORTLIB, ">> %ld <<\n", compare_to_base);
				clash = TRUE;
			}				
		}
	}
	return clash;
}
#endif

#if defined(COMMAND_DEBUG)
static UDATA recursion_count = 0;

static char
bin2hex(U_8 bin)
{
	char hexvals[] = {'0', '1', '2', '3', '4', '5', '6', '7',
		              '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

	return hexvals[bin];
}

static void
print_bytes_as_text(struct J9PortLibrary *portLibrary, const void* buf, const IDATA nbytes)
{
	U_32 i;

	PORT_ACCESS_FROM_PORT(portLibrary);

	for (i = 0; i < nbytes; i++) {
		U_8 ch = ((U_8*)buf)[i];
		U_8 hi = (ch & 0xf0) >> 4;
		U_8 lo = ch & 0x0f;
		
		j9tty_printf(PORTLIB, "%c", bin2hex(hi));
		j9tty_printf(PORTLIB, "%c", bin2hex(lo));
	}
}
#endif

static void
setup_cache(J9CachedFileHandle* handle, U_8 index, I_64 base, IDATA position, U_8 inuse)
{
	J9CachedFileCacheDescriptor* pCacheTableEntry = &handle->cacheTable[index];
	increment_lru(handle);
	pCacheTableEntry->lru = LATEST_LRU();
	pCacheTableEntry->cacheBase = base;
	pCacheTableEntry->cachePosition = position;
	pCacheTableEntry->hwm = NO_DATA;
	pCacheTableEntry->cacheInUse = inuse;
	
	handle->inuse = index;
}

static U_8
find_oldest_cache(J9CachedFileHandle* handle)
{
	U_8 i;
	U_8 index;
	LRU lru = LATEST_LRU(); /* set up with the current lru */
	
	/* go through all the caches and find the one with the lowest lru */
	for (i = 0; i < CACHE_BUFFER_NUM; i++) {
		if (handle->cacheTable[i].lru < lru) {
			/* found an older cache */
			lru = handle->cacheTable[i].lru;
			index = i;
		}
	}
	
	return index;
}

static BOOLEAN
find_next_oldest_cache(J9CachedFileHandle* handle, U_8 oldest, U_8* pNext)
{
	U_8 index;
	LRU lru;
	LRU next_lru = -1;
	U_8 next_index = 0;
	U_8 found = 0;

	if (CACHE_BUFFER_NUM > 1) {
			for (index = 0; index < CACHE_BUFFER_NUM; index++) {
			if (handle->cacheTable[index].lru > handle->cacheTable[oldest].lru) {
				/* found an lru younger than the oldest but this might not be the next oldest */
				lru = handle->cacheTable[index].lru;
                                                                  
				if (lru <= next_lru) {
					next_lru = lru;                                                             
					next_index = index;
					found = 1;
				}                                                                                      
			}                                                                                              
		}                                                                                                      
	}                                                                                                              
                                                                                                                      
	*pNext = next_index;                                                                                              
	return found;                                                                                                     
}

static void
increment_lru(J9CachedFileHandle* handle)
{
	U_8 nextCache;

	PORT_ACCESS_FROM_PORT(handle->portLibrary);

	/* increment the lru and handle any lru rollover situations */
	if ( (LRU)(LATEST_LRU() + 1) == 0) {
		/* lru has rolled over */
		LRU lru = 0;

#if defined(DEBUG)
		j9tty_printf(PORTLIB, "lru has rolled %d\n", LATEST_LRU());
#endif

		/* need to reset all the lru numbers in their respective order */
		nextCache = find_oldest_cache(handle);
		handle->cacheTable[nextCache].lru = lru;
#if defined(DEBUG)
		j9tty_printf(PORTLIB, "setting cache %d lru to %d\n", nextCache, lru);
#endif

		while (find_next_oldest_cache(handle, nextCache, &nextCache)) {
			lru++;
			handle->cacheTable[nextCache].lru = lru;
#if defined(DEBUG)
			j9tty_printf(PORTLIB, "setting cache %d lru to %d\n", nextCache, lru);
#endif
		}

		handle->lru_count = lru;
	} else {
		/* no rollover, so just do the increment */
		handle->lru_count++;
#if defined(DEBUG)
		j9tty_printf(PORTLIB, "lru has incremented %d\n", LATEST_LRU());
#endif
	}
}                                                                                                                     


static I_8
find_cache_containing(J9CachedFileHandle* handle, I_64 position)
{
	U_8 index;
	
	/* return the index of the cache that contains position */
	/* this does not mean that the cache has necessarily written to the position as yet */
	/* return -1 if no cache holds the position */
	for (index = 0; index < CACHE_BUFFER_NUM; index++) {
		if (handle->cacheTable[index].cacheInUse == INUSE) {
			I_64 max_range = handle->cacheTable[index].cacheBase + CACHE_BUFFER_SIZE -1;
			if (position >= handle->cacheTable[index].cacheBase && position <= max_range) {
				/* found a cache that holds the position */
				return index;
			}
		}
	}
	return -1;
}

static I_8
find_possible_contention(J9CachedFileHandle* handle, I_64 position)
{
	U_8 index;
	
	/* return the index of a cache that will need to be flushed in order to write to this new position */
	for (index = 0; index < CACHE_BUFFER_NUM; index++) {
		I_64 base = handle->cacheTable[index].cacheBase;
		if ( (position < base) && ((position + CACHE_BUFFER_SIZE -1) >= base) ) {
			/* allocation of a cache starting at this position may result in a cache overwrite */
			return index;
		}
	}
	return -1;
}

static IDATA
flush_cache(J9CachedFileHandle* handle, U_8 index)
{
	PORT_ACCESS_FROM_PORT(handle->portLibrary);

	if (HAS_DATA(index)) {
		/* position the file pointer */
		I_64 seek_rc = j9file_seek(handle->fd, handle->cacheTable[index].cacheBase, EsSeekSet);

		IDATA length = handle->cacheTable[index].hwm + 1;
#if defined(DEBUG)
		j9tty_printf(PORTLIB, ">>FLUSHING from %ld for ", handle->cacheTable[index].cacheBase);
		j9tty_printf(PORTLIB, "%ld<<\n", (I_64)length);
#endif
		handle->cacheTable[index].hwm = -1;
		return j9file_write(handle->fd, handle->cacheTable[index].cache, length);
	}

	return 0;
}

/*
 * cached_file_open: open a file for writing through a caching mechanism.
 *
 * @return fd or -1 on failure
 */
IDATA 
j9cached_file_open(struct J9PortLibrary *portLibrary, const char *path, I_32 flags, I_32 mode)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	/* open the real file */
	UDATA rawResult = j9file_open(path, flags, mode);
	J9CachedFileHandle *handle = NULL;
	BOOLEAN alloc_error = FALSE;

	Trc_Util_j9cached_file_open_Entry(path);
		
#if defined(COMMAND_DEBUG)
	j9tty_printf(PORTLIB, "fo %s %d %d\n", path, flags, mode);
#endif

	/* if the file was successfully opened, create the cache to back it up */
	if(rawResult != -1) {
		I_8 index;
		handle = (J9CachedFileHandle *) j9mem_allocate_memory(sizeof(J9CachedFileHandle), OMRMEM_CATEGORY_VM);
		if(handle) {
			/* got the handle, now initialize the cache(s) */
			memset(handle, 0x00, sizeof(J9CachedFileHandle));
			handle->portLibrary = portLibrary;
			handle->fd = rawResult;
			LATEST_LRU() = -1;
			for (index = 0; index < CACHE_BUFFER_NUM; index++) {
				/* initialize the individual cache(s) */
				handle->cacheTable[index].cache = (char *) j9mem_allocate_memory(CACHE_BUFFER_SIZE, OMRMEM_CATEGORY_VM);
				if (handle->cacheTable[index].cache != NULL) {
					/* created one of the cache(s) */
					setup_cache(handle, index, 0, 0, UNUSED);
#if defined(DEBUG)
					j9tty_printf(PORTLIB, "cache %d address: 0x%x\n", LATEST_LRU(), handle->cacheTable[index].cache);
#endif 
				} else {
					/* an allocation has failed, so set the flag to indicate an error */
					alloc_error = TRUE;
#if defined(DEBUG)
					j9tty_printf(PORTLIB, "cache allocation error\n");
#endif
					break;
				}
			}
			
			if (alloc_error == FALSE) {
				/* set the current cache inuse */
				setup_cache(handle, handle->inuse, 0, 0, INUSE);
			}
		} else {
			/* could not allocate the handle, so return an error */
#if defined(DEBUG)
			j9tty_printf(PORTLIB, "cache handle allocation error\n");
#endif
			j9file_close(rawResult); 
			handle = (J9CachedFileHandle *) -1;
			alloc_error = TRUE;
		}
	}
	
	if (alloc_error == TRUE) {
		/* free up any allocated memory */
		if (handle != (J9CachedFileHandle *) -1) {
			/* got a handle, so free any cached associated with it */
			I_8 index;

			for (index = 0; index < CACHE_BUFFER_NUM; index++) {
				if (handle->cacheTable[index].cache != NULL) {
					j9mem_free_memory(handle->cacheTable[index].cache);
				}
			}
			j9mem_free_memory(handle);
			handle  = (J9CachedFileHandle *) -1;
		}
	}
	
#if defined(DEBUG)
	j9tty_printf(PORTLIB, "cache open <%s> (%x)\n", path, rawResult);
#endif
	Trc_Util_j9cached_file_open_Exit(handle);
	return (IDATA) handle;
}

/*
 * cached_file_close: close a file that was opened by the cached_file_open function.
 *
 * @return 0 on success, negative portable error code on failure
 */
I_32
j9cached_file_close(struct J9PortLibrary *portLibrary, IDATA fd)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	J9CachedFileHandle *handle = (J9CachedFileHandle *) fd;
	IDATA realfd;
	I_8 index;
	IDATA flushRC = 0;
	IDATA closeRC;

	Trc_Util_j9cached_file_close_Entry(handle);
	
#if defined(DEBUG)
	j9tty_printf(PORTLIB, "cache close %x\n", fd);
#endif
#if defined(COMMAND_DEBUG)
	j9tty_printf(PORTLIB, "fc\n");
#endif
	if(!handle) {
		return -1;
	}
	if(fd == J9PORT_TTY_ERR || fd == J9PORT_TTY_OUT) {
		return j9file_close(fd);
	}
	
	for (index = 0; index < CACHE_BUFFER_NUM; index++) {

		if (flushRC == 0) {
			/* write each cache in turn */
			/* no need to ensure that the caches are written the the lru order as */
			/* the caches don't overlap and the flush will seek to the correct file position */
#if defined(DEBUG)
		j9tty_printf(PORTLIB, "requesting flush: cache %d because of close\n", index);
#endif
			flushRC = flush_cache(handle, index);
		}

		
		/* now free the cache */
		j9mem_free_memory(handle->cacheTable[index].cache);
	} 

	/* free the fd and close the file */
	realfd = handle->fd;	
	j9mem_free_memory(handle);

	Trc_Util_j9cached_file_close_Exit();

	closeRC = j9file_close(realfd);

	/* Return the first failure */
	if (flushRC) {
		return (I_32)flushRC;
	} else {
		return (I_32)closeRC;
	}
}

/*
 * cached_file_write: write data to a file that was opened using the cached_file_open function.
 *
 * Return number of bytes written on success, negative portable error code on failure
 */
IDATA
j9cached_file_write(struct J9PortLibrary* portLibrary, IDATA fd, const void* buf, const IDATA nbytes)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	J9CachedFileHandle* handle = (J9CachedFileHandle*) fd;
	J9CachedFileCacheDescriptor* pCurrentCache;
	IDATA free_space;
	IDATA rc;

	Trc_Util_j9cached_file_write_Entry(handle, nbytes);
	
	if(!handle) {
		return J9PORT_ERROR_OPFAILED;
	}
	if(fd == J9PORT_TTY_ERR || fd == J9PORT_TTY_OUT) {
		return j9file_write(fd, buf, nbytes);
	}
		
	pCurrentCache = &handle->cacheTable[handle->inuse];
#if defined(DEBUG)
	j9tty_printf(PORTLIB, "j9cached_file_write %d byte(s)\n", nbytes);
	j9tty_printf(PORTLIB, "file position %ld", pCurrentCache->cacheBase + pCurrentCache->cachePosition);
	j9tty_printf(PORTLIB, " -> %ld\n", pCurrentCache->cacheBase + pCurrentCache->cachePosition + nbytes);
	if (ASSERT_CACHE_SEPARATION() == TRUE) {
		j9tty_printf(PORTLIB, "cache clash detected\n");
	}
#endif
#if defined(COMMAND_DEBUG)
	if (recursion_count == 0) {
		j9tty_printf(PORTLIB, "fw %d ", nbytes);
		print_bytes_as_text(portLibrary, buf, nbytes);
		j9tty_printf(PORTLIB, "\n");
	}
	recursion_count++;
#endif

	/* test for the availability of space in the current cache */
	free_space = CACHE_BUFFER_SIZE - pCurrentCache->cachePosition;
	if (nbytes > free_space) {
		I_8 new_cache;
		
		if (free_space > 0) {
			/* the cache will overflow, but still has some room, so fill this cache up */
			memcpy(pCurrentCache->cache + pCurrentCache->cachePosition, buf, free_space);
			pCurrentCache->cachePosition = CACHE_BUFFER_SIZE; /* this is an invalid cachePosition */
			pCurrentCache->hwm = pCurrentCache->cachePosition -1;
#if defined(DEBUG)
			j9tty_printf(PORTLIB, "1. set hwm on %d to %d (%ld)\n", handle->inuse, pCurrentCache->hwm, pCurrentCache->cacheBase+pCurrentCache->hwm);
#endif
		}
		
		/* now look for another to spill the data into */
		/* first, see if the spill cache is already in use */
		new_cache = find_cache_containing(handle, pCurrentCache->cacheBase + CACHE_BUFFER_SIZE);
		if (new_cache == -1) {
			new_cache = find_oldest_cache(handle);
		}
		
		/* as we are about to reuse a cache, we need to write its data if it has any */
#if defined(DEBUG)
		j9tty_printf(PORTLIB, "requesting flush: cache %d because of cache reuse in write\n", new_cache);
#endif
		rc = flush_cache(handle, new_cache);

		if (rc < 0) {
			/* Flush failed, bail out */
			return rc;
		}

		/* setup the new cache */		
		setup_cache(handle, new_cache, pCurrentCache->cacheBase + CACHE_BUFFER_SIZE, 0, INUSE);

		/* write the remaining data into the new cache */
#if defined(DEBUG)
		j9tty_printf(PORTLIB, "write remaining (%d bytes) data into the cache %d\n", nbytes - free_space, new_cache);
#endif
		rc = j9cached_file_write(portLibrary, fd, (char*)buf + free_space, nbytes - free_space);

		if (rc < 0) {
			return rc;
		}
	} else {
		/* the data will fit in the cache, so write it there */
		memcpy(pCurrentCache->cache + pCurrentCache->cachePosition, buf, nbytes);
		pCurrentCache->cachePosition += nbytes;
		if (pCurrentCache->cachePosition > pCurrentCache->hwm) {
			pCurrentCache->hwm = pCurrentCache->cachePosition -1;
		}
#if defined(DEBUG)
		j9tty_printf(PORTLIB, "3. set hwm on %d to %d (%ld)\n", handle->inuse, pCurrentCache->hwm, pCurrentCache->cacheBase+pCurrentCache->hwm);
#endif
	}
#if defined(COMMAND_DEBUG)
	recursion_count--;
#endif
	
	Trc_Util_j9cached_file_write_Exit(nbytes);
	return nbytes;
}

/*
 * cached_file_seek: perform a file seek on a file that was opened using the cached_file_open function.
 *
 * @return New file position on success. Negative portable error code on failure.
 */
I_64
j9cached_file_seek(struct J9PortLibrary* portLibrary, IDATA fd, I_64 offset, I_32 whence)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	J9CachedFileHandle* handle = (J9CachedFileHandle*) fd;
	I_8 current_cache;
	I_64 filePosition;
	I_64 newPosition;
	I_8 rangeCache = -1;
	I_8 clashCache = -1;
	IDATA rc;
	
	Trc_Util_j9cached_file_seek_Entry(handle);
	
	if(!handle) {
		return J9PORT_ERROR_OPFAILED;
	}
	if(fd == J9PORT_TTY_ERR || fd == J9PORT_TTY_OUT) {
		return j9file_seek(fd, offset, whence);
	}

	/* search for a cache with the requested seek position */
	switch (whence) {
		case EsSeekSet:
			/* Seek relative to file start */
			newPosition = offset;
#if defined(DEBUG)
			j9tty_printf(PORTLIB, "j9cached_file_seek set to %ld\n", offset);
#endif
#if defined(COMMAND_DEBUG)
			j9tty_printf(PORTLIB, "ss %d\n", offset);
#endif
			break; 
		case EsSeekEnd:
			/* Seek relative to file end */
			/* not supported */
			return -1;
			break; 
		case EsSeekCur:
			/* Seek relative to current position */
#if defined(COMMAND_DEBUG)
			j9tty_printf(PORTLIB, "sc %d\n", offset);
#endif
			/* get the current position of the file pointer */
			current_cache = handle->inuse;
			filePosition = handle->cacheTable[current_cache].cacheBase + handle->cacheTable[current_cache].cachePosition;
			newPosition = filePosition + offset;
			if (offset == 0) {
				/* this is just getting positional information and there are no cache changes. */
#if defined(DEBUG)
				j9tty_printf(PORTLIB, "j9cached_file_seek current returning %ld\n", newPosition);
#endif
				Trc_Util_j9cached_file_seek_Exit(newPosition);
				return newPosition;
			}
			break;
		default:
			return -1;
	}
	
	/* there are two potential error situations we need to deal with... */
	/* 1. where we could allocate a cache, but it would clash with an existing one */
	/* 2. where the cache we want is already allocated but we are seeking beyond the current hwm */
	
	/* first find if this position already exists */
	rangeCache = find_cache_containing(handle, newPosition);
	if (rangeCache == -1) {
		/* didn't find this position in any of the caches */
		/* now check that if we start a cache at this position, we wont trample over another cache */
		clashCache = find_possible_contention(handle, newPosition);
		if (clashCache == -1) {
			/* no caches that might get trampled, so just get hold of the oldest cache and use it */
			U_8 cache = find_oldest_cache(handle);
#if defined(DEBUG)
			j9tty_printf(PORTLIB, "flushing cache %d because of having to use the oldest\n", cache);
#endif
			rc = flush_cache(handle, cache);

			if (rc < 0) {
				return rc;
			}
			
			/* setup the new cache */		
			setup_cache(handle, cache, newPosition, 0, INUSE);
		} else {
			/* found one that would get trampled, so we need to flush this cache, then we can reuse it */
#if defined(DEBUG)
			j9tty_printf(PORTLIB, "requesting flush: cache %d because of possible contention\n", clashCache);
#endif
			rc = flush_cache(handle, clashCache);

			if (rc < 0) {
				return rc;
			}
		
			/* reuse the cache */		
			setup_cache(handle, clashCache, newPosition, 0, INUSE);
		}
	} else {
		/* found a cache that holds this position, */

		/* if the seek positions into cached data or immediately after it, that has yet to be written */
		/* we should be able to add to the data in the cache. */
		J9CachedFileCacheDescriptor* pTableEntry = &handle->cacheTable[rangeCache];
		I_64 base = pTableEntry->cacheBase;
		IDATA hwm = pTableEntry->hwm;

		if ( (newPosition >= base) && (newPosition <= (base + hwm + 1)) ) {
#if defined(DEBUG)
			j9tty_printf(PORTLIB, "reuse cache %d without flushing\n", rangeCache);
			j9tty_printf(PORTLIB, "cache base: %d\n", base);
			j9tty_printf(PORTLIB, "cache hwm: %d\n", hwm);
#endif

			/* seek into existing cache data or immediately after it is ok, no need to flush */
			handle->inuse = rangeCache;
			pTableEntry->cachePosition = (IDATA)(newPosition - base);
			increment_lru(handle);
			pTableEntry->lru = LATEST_LRU();

#if defined(DEBUG)
			j9tty_printf(PORTLIB, "cache position %d\n", pTableEntry->cachePosition);
			j9tty_printf(PORTLIB, "seek position set to %ld\n", newPosition);
#endif
		} else {
			/* 4. seek beyond the hwm of an existing cache so need to flush the cache. */
#if defined(DEBUG)
			j9tty_printf(PORTLIB, "requesting flush: cache %d because of seek beyond hwm\n", rangeCache);
			j9tty_printf(PORTLIB, "seek position set to %ld\n", newPosition);
#endif
			rc = flush_cache(handle, rangeCache);

			if (rc < 0) {
				return rc;
			}

			/* setup the cache for reuse */		
			setup_cache(handle, rangeCache, newPosition, 0, INUSE);
		}
	}
	
	Trc_Util_j9cached_file_seek_Exit(newPosition);
	return newPosition;
}

/*
 * cached_file_sync: perform a file sync on a file that was opened using the cached_file_open function.
 *
 * @return 0 on success, negative portable error code on failure.
 */
I_32
j9cached_file_sync(struct J9PortLibrary *portLibrary, IDATA fd)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	J9CachedFileHandle* handle = (J9CachedFileHandle*) fd;
	IDATA lru = LATEST_LRU(); /* set up with the current lru count */
	I_8 inuse = handle->inuse;
	I_8 index;

	Trc_Util_j9cached_file_sync_Entry(handle);
	
#if defined(DEBUG)
	j9tty_printf(PORTLIB, "cache sync 0x%x\n", fd);
#endif
#if defined(COMMAND_DEBUG)
	j9tty_printf(PORTLIB, "fs\n");
#endif

	if(!handle) {
		return J9PORT_ERROR_OPFAILED;
	}
	if(fd == J9PORT_TTY_ERR || fd == J9PORT_TTY_OUT) {
		return j9file_sync(fd);
	}
	
#if defined(DEBUG)
	j9tty_printf(PORTLIB, "sync request\n");
#endif
	
	/* push each unwritten cache out to disk but do not alter their */
	/* hwm or cachePosition */
	for (index = 0; index < CACHE_BUFFER_NUM; index++) {
		if (HAS_DATA(index)) {
			IDATA write_rc;
			/* position the file pointer */
			I_64 seek_rc = j9file_seek(handle->fd, handle->cacheTable[index].cacheBase, EsSeekSet);
			IDATA length = handle->cacheTable[index].hwm + 1;
			
#if defined(DEBUG)
			j9tty_printf(PORTLIB, ">>SYNCING %d from %ld for ", index, handle->cacheTable[index].cacheBase);
			j9tty_printf(PORTLIB, "%ld<<\n", (I_64)length);
#endif
			write_rc = j9file_write(handle->fd, handle->cacheTable[index].cache, length);

			if (write_rc < 0) {
				return (I_32)write_rc;
			}
		}
	}
	
	Trc_Util_j9cached_file_sync_Exit();
	
	/* now issue a file_sync to the fs. */
	return j9file_sync(handle->fd);
}
