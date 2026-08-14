/*******************************************************************************
 * Copyright IBM Corp. and others 2026
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#if !defined(GC_BASE_JFR_HPP_)
#define GC_BASE_JFR_HPP_

#include "j9.h"
#include "j9cfg.h"

#if defined(J9VM_OPT_JFR)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Register GC-related JFR hooks.
 *
 * This function registers all garbage collection related hooks for JFR event recording:
 * - jfrPublioGCEndHook (corresponding to public OMR hook)
 * - jfrPrivateGCEndHook (corresponding to private OMR hook)
 *
 * @param vm[in] The Java VM
 * @return 0 on success, non-zero on failure
 */
jint
jfrRegisterGCHooks(J9JavaVM *vm);

/**
 * JFR GC Hook for cycle start.
 *
 * This function emits the GC heap summary event before GC starts.
 *
 * @param hook[in] the VM hook interface
 * @param eventNum[in] the event number
 * @param eventData[in] the event data
 * @param userData[in] the registered user data
 */
void
jfrGCCycleStartHook(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);

/**
 * JFR GC Hook corresponding to public OMR hook data.
 *
 * This function calls all garbage collection related JFR event recording functions on the public OMR hook.
 *
 * @param hook[in] the VM hook interface
 * @param eventNum[in] the event number
 * @param eventData[in] the event data
 * @param userData[in] the registered user data
 */
void
jfrPublicGCEndHook(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);

/**
 * JFR GC Hook corresponding to private OMR hook data.
 *
 * This function calls all garbage collection related JFR event recording functions on the public OMR hook.
 *
 * @param hook[in] the VM hook interface
 * @param eventNum[in] the event number
 * @param eventData[in] the event data
 * @param userData[in] the registered user data
 */
void
jfrPrivateGCEndHook(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);

/**
 * Deregister GC-related JFR hooks.
 *
 * This function unregisters all garbage collection related hooks that were
 * previously registered for JFR event recording.
 *
 * @param vm[in] The Java VM
 */
void
jfrDeregisterGCHooks(J9JavaVM *vm);

#ifdef __cplusplus
}
#endif

#endif /* J9VM_OPT_JFR */

#endif /* GC_BASE_JFR_HPP_ */
