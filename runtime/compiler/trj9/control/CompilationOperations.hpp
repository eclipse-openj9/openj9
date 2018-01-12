/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#ifndef COMPILATIONOPERATIONS_HPP
#define COMPILATIONOPERATIONS_HPP

#pragma once

enum TR_CompilationOperations
   {
   OP_Empty,
   OP_HasAcquiredCompilationMonitor,
   OP_WillReleaseCompilationMonitor,
   OP_WillNotifyCompilationMonitor,
   OP_WillWaitOnCompilationMonitor,
   OP_HasFinishedWaitingOnCompilationMonitor,
   OP_StateChange,
   OP_WillWaitOnSlotMonitorAfterCompMonRelease, // This entry will appear out-of-order
   OP_CompileOnSeparateThreadEnter,
   OP_WillStopCompilationThreads,
   // If adding new entries, please add names as well in CompilationThread.cpp and DebugExt.cpp - OperationNames
   OP_LastValidOperation
   };
#if (OP_LastValidOperation > 255)
#error "cannot have more than 255 operations because we use 8 bits to store then"
#endif

#endif // COMPILATIONOPERATIONS_HPP

