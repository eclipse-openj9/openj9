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

#if !defined(J9SC_HOOK_HELPERS_HPP)
#define J9SC_HOOK_HELPERS_HPP

/* @ddr_namespace: default */
#ifdef __cplusplus
extern "C"
{
#endif

#include "j9user.h"
#include "j9.h"
#include "j9protos.h"
#include "j9consts.h"
#include "jvminit.h"
#include "j9port.h"

#ifdef __cplusplus
}
#endif
#include "ClasspathItem.hpp"

#define HELPER_NAMEBUF_SIZE 256

typedef struct ClassNameFilterData {
	J9ClassLoader* classloader;
	char* classname;
	char buffer[HELPER_NAMEBUF_SIZE];
	UDATA classnameLen;
} ClassNameFilterData;

UDATA translateExtraInfo(void* extraInfo, IDATA* helperID, U_16* cpType, ClasspathItem** cachedCPI);
/**
 * Return the cached ClasspathItem for the bootstrap classloader classPathEntries.
 * 
 * @param currentThread The current thread
 * @param bootstrapCPE 	The current bootstrap classloader classPathEntries
 * @param entryCount 	The number of current bootstrap classloader classPathEntries
 * 
 * @return The cached ClasspathItem for the bootstrap classloader, or null if there is no
 * ClasspathItem for the current list of bootstrap classloader classPathEntries.
 * 
 * @see createClasspath()
 */
ClasspathItem* getBootstrapClasspathItem(J9VMThread* currentThread, J9ClassPathEntry* bootstrapCPE, UDATA entryCount);

UDATA checkForStoreFilter(J9JavaVM* vm, J9ClassLoader* classloader, const char* classname, UDATA classnameLen, J9Pool* filterPool);
void storeClassVerboseIO( J9VMThread* currentThread, ClasspathItem * classpath, I_16 entryIndex, U_16 classnameLength, const U_8 * classnameData, UDATA helperID, BOOLEAN didWeStore);
ClasspathItem *createClasspath(J9VMThread* currentThread, J9ClassPathEntry* classPathEntries, UDATA entryCount, IDATA helperID, U_16 cpType, UDATA infoFound);

#endif /*J9SC_HOOK_HELPERS_HPP*/
