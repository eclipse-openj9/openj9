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

#ifndef RUNTIME_FAILURE
#define RUNTIME_FAILURE

#pragma once

#include "compile/CompilationException.hpp"

namespace J9 {

struct RuntimeFailure : public virtual TR::CompilationException
   {
   virtual const char* what() const throw() { return "Runtime failure"; }
   };

struct MetaDataCreationFailure : public virtual RuntimeFailure
   {
   virtual const char* what() const throw() { return "MetaData Creation failure"; }
   };

struct CHTableCommitFailure : public virtual RuntimeFailure
   {
   virtual const char* what() const throw() { return "CH Table Commit failure"; }
   };

class LambdaEnforceScorching : public virtual TR::InsufficientlyAggressiveCompilation
   {
   virtual const char* what() const throw() { return "Lambda Enforce Scorching"; }
   };
}

#endif // RUNTIME_FAILURE

