
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

#if !defined(VGC_INTERFACE_HPP_)
#define VGC_INTERFACE_HPP_

#include "j9.h"
#include "j9cfg.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Initializes the verbose function table.
 */
void initializeVerboseFunctionTable(J9JavaVM *javaVM);
	
/**
 * Definition of Verbose function table.
 */
typedef struct J9MemoryManagerVerboseInterface {
	UDATA (*gcDebugVerboseStartupLogging)(J9JavaVM *javaVM, char* filename, UDATA numFiles, UDATA numCycles);
	void (*gcDebugVerboseShutdownLogging)(J9JavaVM *javaVM, UDATA releaseVerboseStructures);
	void (*gcDumpMemorySizes)(J9JavaVM *javaVM);
	UDATA (*configureVerbosegc)(J9JavaVM *javaVM, int enable, char* filename, UDATA numFiles, UDATA numCycles);
	UDATA (*queryVerbosegc)(J9JavaVM *javaVM);
} J9MemoryManagerVerboseInterface;

#ifdef __cplusplus
} /* extern "C" { */
#endif /* __cplusplus */

#endif /* VGC_INTERFACE_HPP_ */
