/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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

#ifndef jitserver_api_h
#define jitserver_api_h

#include "jni.h"

#ifdef __cplusplus
extern "C" {
#endif

struct JITServer; /* Forward declaration */

typedef struct JITServer {
	/**
	 * Starts an instance of JITServer.
	 *
	 * @param jitServer pointer to the JITServer interface
	 *
	 * @returns JITSERVER_OK on success, else negative error code
	 */
	int32_t (* startJITServer)(struct JITServer *);
	/**
	 * Wait for JITServer to terminate.
	 *
	 * @param jitServer pointer to the JITServer interface
	 *
	 * @returns JITSERVER_OK on success, else negative error code
	 */
	int32_t (* waitForJITServerTermination)(struct JITServer *);
	/**
	 * Frees the resources allocated by JITServer_CreateServer.
	 *
	 * @param jitServer double pointer to the JITServer interface. Must not be NULL
	 *
	 * @returns JITSERVER_OK on success, else negative error code
	 *
	 * @note on return *jitServer is set to NULL
	 */
	int32_t (* destroyJITServer)(struct JITServer **);
	JavaVM *jvm;
} JITServer;

/**
 * Creates an instance of JITServer.
 *
 * @param jitServer pointer to the location where the JITServer interface
 *			pointer will be placed
 * @param serverArgs arguments passed to JITServer
 *
 * @returns zero on success, else negative error code
 */
int32_t JNICALL
JITServer_CreateServer(JITServer **jitServer, void *serverArgs);

#ifdef __cplusplus
}
#endif

#endif /* jitserver_api_h */
