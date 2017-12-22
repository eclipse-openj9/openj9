/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
#if !defined(COMPOSITECACHE_H_INCLUDED)
#define COMPOSITECACHE_H_INCLUDED

/* @ddr_namespace: default */
#include "j9.h"
#include "j9protos.h"
#include "ut_j9shr.h"

class SH_CompositeCache
{
public:
	virtual U_16 getJVMID(void) = 0;

	virtual bool isRunningReadOnly(void) = 0;

	virtual UDATA getTotalUsableCacheSize(void) = 0;

	virtual void getMinMaxBytes(U_32 *softmx, I_32 *minAOT, I_32 *maxAOT, I_32 *minJIT, I_32 *maxJIT) = 0;

	virtual UDATA getFreeAvailableBytes(void) = 0;

	virtual U_32 getDebugBytes(void) = 0;

	virtual void setInternCacheHeaderFields(J9SRP** sharedTail, J9SRP** sharedHead, U_32** totalSharedNodes, U_32** totalSharedWeight) = 0;
	
	virtual bool isStarted(void) = 0;

	virtual IDATA restoreFromSnapshot(J9JavaVM* vm, const char* cacheName, bool* cacheExist) = 0;

	virtual I_32 tryAdjustMinMaxSizes(J9VMThread *currentThread, bool isJCLCall = false) = 0;
};
#endif /*COMPOSITECACHE_H_INCLUDED*/
