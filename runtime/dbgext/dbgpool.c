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

#include "j9dbgext.h"
#include "j9protos.h"

#define J9DBGEXTPOOL_PUDDLELIST(base) LOCAL_WSRP_GET((base)->puddleList, J9PoolPuddleList *)
#define J9DBGEXTPOOLPUDDLELIST_NEXTPUDDLE(base) LOCAL_WSRP_GET((base)->nextPuddle, J9PoolPuddle *)
#define J9DBGEXTPOOLPUDDLE_NEXTPUDDLE(base) LOCAL_WSRP_GET((base)->nextPuddle, J9PoolPuddle *)

#define DEFAULT_J9POOL_WALK_PFUNC defaultJ9PoolWalkPFunc
extern J9Pool * dbgRead_J9Pool(void*);
extern J9PoolPuddle * dbgRead_J9PoolPuddle(void*);

#define PARSE_FUNC_PTR(args,pFunc) \
		parseFuncPtr(args,(void (**)(void*,J9PoolWalkData*))(&pFunc))

void defaultJ9PoolWalkPFunc(void *addr, J9PoolWalkData *data)
{
	/* userData contains the segment offset */
	dbgPrint("\t[%d]\t=\t0x%zx\n", data->counter, (UDATA)addr + data->puddle->userData);
}


static void parseFuncPtr(const char *args, void (**ppFunc)(void*,J9PoolWalkData*))
{
	if(ppFunc != NULL) {
		*ppFunc = DEFAULT_J9POOL_WALK_PFUNC;
	}
}


static J9PoolPuddle * 
mapPuddle(J9Pool* head, UDATA addr)
{
	UDATA bytesToBeRead; 
	IDATA segmentOffset;
	J9PoolPuddle *puddle = NULL;

	bytesToBeRead = head->puddleAllocSize;
	puddle = dbgMallocAndRead(bytesToBeRead, (void*)addr);
	if(puddle == NULL) {
		dbgError("\tcould not map J9PoolPuddle at 0x%p (%zu bytes).\n", addr, bytesToBeRead);
		return NULL;
	}
	
	segmentOffset = addr - (UDATA)puddle;
	puddle->userData = segmentOffset;

	/* adjust nextPuddle & friends to be accurate */
	if (puddle->nextPuddle != 0) {
		puddle->nextPuddle += segmentOffset;
	}
	if (puddle->prevPuddle != 0) {
		puddle->prevPuddle += segmentOffset;
	}
	if (puddle->nextAvailablePuddle != 0) {
		puddle->nextAvailablePuddle += segmentOffset;
	}
	if (puddle->prevAvailablePuddle != 0) {
		puddle->prevAvailablePuddle += segmentOffset;
	}

	return puddle;
}

void dbgUnmapPool(J9Pool *pool)
{
	J9PoolPuddleList *puddleList;
	J9PoolPuddle *thisPuddle, *nextPuddle;

	puddleList = J9DBGEXTPOOL_PUDDLELIST(pool);
	thisPuddle = J9DBGEXTPOOLPUDDLELIST_NEXTPUDDLE(puddleList);
	nextPuddle = NULL;
	while(thisPuddle != NULL) {
		nextPuddle = J9DBGEXTPOOLPUDDLE_NEXTPUDDLE(thisPuddle);
		dbgFree(thisPuddle);
		thisPuddle = nextPuddle;
	}
	dbgFree(puddleList);
	dbgFree(pool);
}


J9Pool *dbgMapPool(UDATA addr)
{
	J9Pool *pool = (J9Pool*)dbgRead_J9Pool((void*)addr);
	J9PoolPuddleList *puddleList, *mappedPuddleList;
	J9PoolPuddle *poolNextPuddle, *prevPuddle, *walk;
	IDATA offset;
	UDATA bytesRead;

	puddleList = J9POOL_PUDDLELIST(pool);
	mappedPuddleList = dbgMalloc(sizeof(J9PoolPuddleList), puddleList);
	if (!mappedPuddleList) {
		dbgError("could not allocate temp space for J9PoolPuddleList\n");
		return NULL;
	}

	dbgReadMemory( (UDATA) puddleList, mappedPuddleList, sizeof(J9PoolPuddleList), &bytesRead);
	if (bytesRead != sizeof(J9PoolPuddleList)) {
		dbgError("could not read J9PoolPuddleList at %p\n", puddleList);
		return NULL;
	}

	offset = (UDATA)puddleList - (UDATA)mappedPuddleList;
	if (mappedPuddleList->nextAvailablePuddle) {
		mappedPuddleList->nextAvailablePuddle += offset;
	}
	if (mappedPuddleList->nextPuddle) {
		mappedPuddleList->nextPuddle += offset;
	}

	NNWSRP_SET(pool->puddleList, mappedPuddleList);
	walk = poolNextPuddle = J9DBGEXTPOOLPUDDLELIST_NEXTPUDDLE(mappedPuddleList);
	prevPuddle = NULL;
	
	while(walk != NULL) {
		J9PoolPuddle *mappedPuddle = NULL;
		mappedPuddle = mapPuddle(pool, (UDATA)walk);
		if (mappedPuddle == NULL) {
			dbgFree(mappedPuddleList);
			dbgFree(pool);
			return NULL;	
		}
		if (walk == poolNextPuddle) {
			NNWSRP_SET(mappedPuddleList->nextPuddle, mappedPuddle);
		}
		if (NULL != prevPuddle) {
			NNWSRP_SET(prevPuddle->nextPuddle, mappedPuddle);
		}
		WSRP_SET(mappedPuddle->prevPuddle, prevPuddle);
		walk = J9DBGEXTPOOLPUDDLE_NEXTPUDDLE(mappedPuddle);
		WSRP_SET(mappedPuddle->nextPuddle, NULL);
		prevPuddle = mappedPuddle;
	}

	return pool;
}

/**
 * @return 1 if the puddle contains the given element, 0 otherwise
 */
static UDATA
puddleContainsElement(J9Pool* head, J9PoolPuddle *puddle, void *element)
{
	UDATA puddleBase = (UDATA)LOCAL_WSRP_GET(puddle->firstElementAddress, void*);
	UDATA puddleTop = puddleBase + (head->elementSize * head->elementsPerPuddle);

	if( ((UDATA)element >= puddleBase) && ((UDATA)element < puddleTop) ) {
		  return 1;
	}
	return 0;
}

void walkJ9Pool(UDATA addr, void (*pFunc)(void*,J9PoolWalkData*), J9PoolWalkData*data)
{
	J9Pool *pool;
	
	if (!addr) {
		dbgError("bad or missing J9Pool at 0x%p\n",addr);
		return;
	}
	
	pool = dbgMapPool(addr);
	if (pool != NULL) {
		J9PoolPuddleList *puddleList = J9DBGEXTPOOL_PUDDLELIST(pool);
		void *anElement;
		pool_state aState;
		anElement = pool_startDo(pool, &aState);

		data->head = pool;
		data->puddle = J9DBGEXTPOOLPUDDLELIST_NEXTPUDDLE(puddleList);

		for( data->counter = 0; anElement != NULL; data->counter++ ) {
			while(!puddleContainsElement(data->head, data->puddle, anElement)) {
				data->puddle = J9DBGEXTPOOLPUDDLE_NEXTPUDDLE(data->puddle);
				if(data->puddle == NULL) {
					dbgError("\tcould not locate element 0x%p in J9Pool at 0x%p\n", anElement, addr);
					return;
				}
			}

			(pFunc)(anElement, data);
			anElement = pool_nextDo(&aState);
		}
		
		dbgUnmapPool(pool);
	} else {
		dbgError("could not map J9Pool at 0x%p\n",addr);
	}
}

void dbgext_walkj9pool(const char *args)
{
	UDATA addr = dbgGetExpression(args);
	J9PoolWalkData data;
	void (*pFunc)(void*,J9PoolWalkData*);
	
	PARSE_FUNC_PTR(args,pFunc);
	if(pFunc == DEFAULT_J9POOL_WALK_PFUNC) {
		dbgPrint("J9Pool at 0x%zx\n{\n",addr);
		walkJ9Pool(addr,pFunc,&data);
		dbgPrint("}\n");
	} else {
		walkJ9Pool(addr,pFunc,&data);
	}
}





