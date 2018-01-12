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

#ifndef VERBOSEHANDLERREALTIME_HPP_
#define VERBOSEHANDLERREALTIME_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "mmhook.h"

void verboseHandlerCycleStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);

void verboseHandlerCycleEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);

void verboseHandlerTriggerStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);

void verboseHandlerTriggerEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);

void verboseHandlerIncrementStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);

void verboseHandlerIncrementEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);

void verboseHandlerSyncGCStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);

void verboseHandlerSyncGCEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);

void verboseHandlerMarkEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);

void verboseHandlerSweepEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);

void verboseHandlerClassUnloadingEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);

void verboseHandlerMarkStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);

void verboseHandlerSweepStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);

void verboseHandlerClassUnloadingStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);

void verboseHandlerOutOFMemory(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);

void verboseHandlerUtilTrackerOverflow(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);

void verboseHandlerNonMonotonicTime(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);

#endif /* VERBOSEHANDLERREALTIME_HPP_ */
