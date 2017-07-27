/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef MAXCOMPILATIONTHREADS_HPP
#define MAXCOMPILATIONTHREADS_HPP

// Must be less than 8 because in some parts of the code (CHTable) we keep flags on a byte variable
// Also, if this increases past 10, we need to expand the _activeThreadName and _suspendedThreadName
// fields in TR::CompilationInfoPerThread as currently they have 1 char available for the thread number.
//
#define MAX_USABLE_COMP_THREADS 7
#define MAX_DIAGNOSTIC_COMP_THREADS 1
#define MAX_TOTAL_COMP_THREADS (MAX_USABLE_COMP_THREADS + MAX_DIAGNOSTIC_COMP_THREADS)
#if (MAX_TOTAL_COMP_THREADS > 8)
#error "MAX_TOTAL_COMP_THREADS should be less than 8"
#endif

#endif // MAXCOMPILATIONTHREADS_HPP
