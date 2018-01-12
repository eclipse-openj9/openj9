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
#ifndef filecache_h
#define filecache_h

/* @ddr_namespace: default */
#include "j9.h"
#include "j9port.h"

#define CACHE_BUFFER_SIZE 16*1024
#define CACHE_BUFFER_NUM 4
#define INVALID_POSITION -1
#define UNUSED FALSE
#define INUSE TRUE
#ifdef J9ZTPF
#undef NO_DATA
#endif
#define NO_DATA -1
#define HAS_DATA(index) handle->cacheTable[index].hwm >= 0

typedef U_16 LRU;  /* LRU is a typedef to allow the size to be easily changed for LRU rollover testing */  

typedef struct J9CachedFileCacheDescriptor {
	LRU lru;                                                     /* least recently used value */
	BOOLEAN cacheInUse;                                          /* when false, the cache has never been used */ 
	I_64 cacheBase;                                              /* seek position for this cache */
	IDATA cachePosition;                                         /* current write position in this cache */
	IDATA hwm;                                                   /* high water mark for the writes to this cache */
	char* cache;                                                 /* pointer to the cache data */  
} J9CachedFileCacheDescriptor;

typedef struct J9CachedFileHandle {
	struct J9PortLibrary* portLibrary;                           /* pointer to the portlib */
	IDATA fd;                                                    /* file descriptor of the file on disk */
	U_8 inuse;                                                   /* index of the current inuse cache */
	LRU lru_count;                                               /* least recently used count */
	J9CachedFileCacheDescriptor cacheTable[CACHE_BUFFER_NUM];    /* the cache descriptors */ 
} J9CachedFileHandle;

#ifdef __cplusplus
extern "C" {
#endif
	 
IDATA
j9cached_file_write(struct J9PortLibrary *portLibrary, IDATA fd, const void *buf, const IDATA nbytes);

IDATA
j9cached_file_open(struct J9PortLibrary *portLibrary, const char *path, I_32 flags, I_32 mode);

I_32
j9cached_file_close(struct J9PortLibrary *portLibrary, IDATA fd);

I_64
j9cached_file_seek(struct J9PortLibrary *portLibrary, IDATA fd, I_64 offset, I_32 whence);

I_32
j9cached_file_sync(struct J9PortLibrary *portLibrary, IDATA fd);

#ifdef __cplusplus
}
#endif

#endif     /* filecache_h */
