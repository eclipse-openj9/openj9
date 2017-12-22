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

#ifndef runtimeinstrumentation_h
#define runtimeinstrumentation_h

#include "j9.h"
#include "j9consts.h"

/**
 * Initialize VM support for JIT runtime instrumentation.  The JIT must call this
 * function during startup (before calling updateJITRuntimeInstrumentationFlags).
 *
 * @param[in] vm The J9JavaVM pointer
 *
 * @return 0 on success
 */
UDATA
initializeJITRuntimeInstrumentation(J9JavaVM *vm);

/**
 * Terminate VM support for JIT runtime instrumentation.  The JIT must call this
 * function during shutdown.
 *
 * @param[in] vm The J9JavaVM pointer
 */
void
shutdownJITRuntimeInstrumentation(J9JavaVM *vm);

/**
 * Request an update to the JIT runtime instrumentation flags on the given J9VMThread.
 * The update will take place asynchronously, at the next async check in the target thread.
 * This call assumes that a valid control block has been installed by the JIT before any
 * of the flag bits are enabled.
 * 
 * This function will return an error if J9_PRIVATE_FLAGS_RI_INITIALIZED
 * is not set in the privateFlags of the target thread.  This will be be set if the call to the port library
 * function j9ri_authorize_current_thread() succeeded when the thread was created.
 * 
 * The following constants represent valid bits in newFlags:
 * 
 * J9_JIT_TOGGLE_RI_ON_TRANSITION
 *		This flag is used to control whether or not the VM calls j9ri_disable
 *		when transitioning from compiled code to the interpreter, and j9ri_enable
 *		when transitioning from the interpreter to compiled code.
 *
 * J9_JIT_TOGGLE_RI_IN_COMPILED_CODE
 *		This flag is used by the compiled code to guard inline sequences to enable and
 *		disable runtime instrumentation.
 * 
 * @param[in] targetThread The J9VMThread to update
 * @param[in] newFlags The new value of the RI flags field
 *
 * @return 0 on success (thread is authorized)
 */
UDATA
updateJITRuntimeInstrumentationFlags(J9VMThread *targetThread, UDATA newFlags);

#endif
