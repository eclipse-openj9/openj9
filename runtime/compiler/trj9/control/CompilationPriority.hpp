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

#ifndef COMPILATIONPRIORITY_HPP
#define COMPILATIONPRIORITY_HPP

#pragma once

enum CompilationPriority
   {
   CP_MIN                = 0x0001,
   CP_ASYNC_ABOVE_MIN    = 0x0020, // used for low priority request that need an upgrade
   CP_ASYNC_BELOW_NORMAL = 0x0040,
   CP_ASYNC_NORMAL       = 0x0080,
   CP_ASYNC_ABOVE_NORMAL = 0x00C0, // priority for very-hot and scorching requests
   CP_ASYNC_BELOW_MAX    = 0x00FE, // used for relocatble methods from shared cache
   CP_ASYNC_MAX          = 0x00FF, // async requests promoted in the queue
   CP_SYNC_MIN           = 0x0100,
   CP_SYNC_NORMAL        = 0x1000,
   CP_SYNC_BELOW_MAX     = 0x7FFE,
   CP_MAX                = 0x7FFF // request to stop compilation thread
   };

#endif // COMPILATIONPRIORITY_HPP

