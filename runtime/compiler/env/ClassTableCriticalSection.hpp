/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#ifndef TR_CLASSTABLECRITICALSECTION_INCL
#define TR_CLASSTABLECRITICALSECTION_INCL

#include "codegen/FrontEnd.hpp"
#include "env/VMJ9.h"

namespace TR
{

/**
 * @brief The TR::ClassTableCriticalSection class wraps acquireClassTableMutex
 * with RAII functionality to help automate the lifetime of the Class Table Mutex.
 * Implicit in the call to acquireClassTableMutex is the acquisition of VMAccess.
 */
class ClassTableCriticalSection
   {
   public:

   /**
    * @brief Declare the beginning of a critical section, constructing the
    * TR::ClassTableCriticalSection object and acquiring the Class Table Mutex.
    * @param fe The TR_FrontEnd object used to acquire the Class Table Mutex
    * @param locked An optional parameter to prevent reacquiring the Class Table Mutex if
    * it has already been acquired
    */
   ClassTableCriticalSection(TR_FrontEnd *fe, bool locked = false)
      : _locked(locked),
        _acquiredVMAccess(false),
        _fe(fe)
      {
      if (!locked)
         {
         TR_J9VMBase *fej9 = (TR_J9VMBase *)_fe;
         _acquiredVMAccess = fej9->acquireClassTableMutex();
         }
      }

   /**
    * @brief Automatically notify the end of the critical section, destroying the
    * TR::ClassTableCriticalSection object and releasing the Class Table Mutex.
    */
   ~ClassTableCriticalSection()
      {
      if (!_locked)
         {
         TR_J9VMBase *fej9 = (TR_J9VMBase *)_fe;
         fej9->releaseClassTableMutex(_acquiredVMAccess);
         }
      }

   /**
    * @brief Used to determine if VMAccess was acquired.
    */
   bool acquiredVMAccess() const { return _acquiredVMAccess; }


   private:

   bool                    _locked;
   bool                    _acquiredVMAccess;
   TR_FrontEnd            *_fe;
   };

}

#endif // TR_CLASSTABLECRITICALSECTION_INCL
