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

//  __   ___  __   __   ___  __       ___  ___  __
// |  \ |__  |__) |__) |__  /  `  /\   |  |__  |  \
// |__/ |___ |    |  \ |___ \__, /~~\  |  |___ |__/
//
// This file is now deprecated and its contents are slowly
// being removed. Please do not add any more interfaces here.
//
// For more information, please see design 31411 in compjazz

#ifndef INTERNAL_FUNCTIONS_INCL
#define INTERNAL_FUNCTIONS_INCL

#include <stddef.h>                 // for size_t
#include <stdint.h>                 // for int32_t
#include "env/FilePointerDecl.hpp"  // for FILE
#include "env/TRMemory.hpp"         // for TR_Memory, etc

class TR_FrontEnd;
namespace TR { class Compilation; }


class TR_InternalFunctions
   {
public:
   TR_ALLOC(TR_Memory::InternalFunctionsBase);

   TR_InternalFunctions(TR_FrontEnd * fe, TR_PersistentMemory *persistentMemory, TR_Memory * trMemory, TR::Compilation *comp) :
      _fe(fe),
      _persistentMemory(persistentMemory),
      _trMemory(trMemory),
      _comp(comp)
      { }

   TR_PersistentMemory * persistentMemory();

   virtual void fprintf(TR::FILE *file, const char * format, ...);

   TR_Memory * trMemory();
   bool inDebugExtension() { return false; }

   TR_FrontEnd *fe() { return _fe; }

   protected:

   TR_FrontEnd * _fe;
   TR_PersistentMemory * _persistentMemory;
   TR::Compilation * _comp;
   TR_Memory * _trMemory;
   };

#endif
