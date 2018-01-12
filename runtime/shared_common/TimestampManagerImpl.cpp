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

#include "TimestampManagerImpl.hpp"
#include "CacheMap.hpp"
#include "ut_j9shr.h"
#include <string.h>

SH_TimestampManagerImpl*
SH_TimestampManagerImpl::newInstance(J9JavaVM* vm, SH_TimestampManagerImpl* memForConstructor, J9SharedClassConfig* sharedClassConfig)
{
	SH_TimestampManagerImpl* newTSM = (SH_TimestampManagerImpl*)memForConstructor;

	new(newTSM) SH_TimestampManagerImpl();
	newTSM->_sharedClassConfig = sharedClassConfig;

	return newTSM;
}

UDATA
SH_TimestampManagerImpl::getRequiredConstrBytes(void)
{
	return sizeof(SH_TimestampManagerImpl);
}

I_64 
SH_TimestampManagerImpl::checkCPEITimeStamp(J9VMThread* currentThread, ClasspathEntryItem* cpei) 
{
	return localCheckTimeStamp(currentThread, cpei, NULL, 0, NULL);
}

I_64 
SH_TimestampManagerImpl::checkROMClassTimeStamp(J9VMThread* currentThread, const char* className, UDATA classNameLen, ClasspathEntryItem* cpei, ROMClassWrapper* rcWrapper) 
{
	return localCheckTimeStamp(currentThread, cpei, className, classNameLen, rcWrapper);	
}

/* Returns TIMESTAMP_UNCHANGED, TIMESTAMP_DOES_NOT_EXIST, TIMESTAMP_DISAPPEARED or an actual timestamp.
 * Should ONLY be called to timestamp against cpitems held in cache, not locally
 * Note: If classname is added, specific classfile is timestamp checked */
I_64 
SH_TimestampManagerImpl::localCheckTimeStamp(J9VMThread* currentThread, ClasspathEntryItem* cpei, const char* className, UDATA classNameLen, ROMClassWrapper* rcWrapper)
{
	I_64 current = 0;
	I_64 test = cpei->timestamp;
	char pathBuf[SHARE_PATHBUF_SIZE];
	char* pathBufPtr = (char*)pathBuf;
	bool doFreeBuffer = false;

	PORT_ACCESS_FROM_PORT(currentThread->javaVM->portLibrary);
	
	if (cpei->protocol==PROTO_DIR) {
		/* If stack buffer not big enough, doFreeBuffer is set to true */
		SH_CacheMap::createPathString(currentThread, _sharedClassConfig, &pathBufPtr, SHARE_PATHBUF_SIZE, cpei, className, classNameLen, &doFreeBuffer);

		if (className!=NULL) {
			/* Timestamp on class instead */
			test = rcWrapper->timestamp;
		}
		Trc_SHR_TMI_LocalCheckTimestamp_Checking_File(currentThread, pathBufPtr);
	} else {
		SH_CacheMap::createPathString(currentThread, _sharedClassConfig, &pathBufPtr, SHARE_PATHBUF_SIZE, cpei, NULL, 0, &doFreeBuffer);

		Trc_SHR_TMI_LocalCheckTimestamp_Checking_Jar(currentThread, pathBufPtr);
	}
	if (!pathBufPtr) {
		return TIMESTAMP_DOES_NOT_EXIST;
	}
	current = j9file_lastmod(pathBufPtr);
	if (doFreeBuffer) {
		j9mem_free_memory(pathBufPtr);
	}
	if (current == -1) {
		if (test == -1) {
			return TIMESTAMP_DOES_NOT_EXIST;
		} else {
			return TIMESTAMP_DISAPPEARED;
		}
	} else {
		if (current == test) {
			return TIMESTAMP_UNCHANGED;
		} else {
			return current;
		}
	}
}


