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

#ifndef J9_VMACCESSCRITICALSECTION_INCL
#define J9_VMACCESSCRITICALSECTION_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_VMACCESSCRITICALSECTION_CONNECTOR
#define J9_VMACCESSCRITICALSECTION_CONNECTOR
namespace J9 { class VMAccessCriticalSection; }
namespace J9 { typedef J9::VMAccessCriticalSection VMAccessCriticalSectionConnector; }
#endif

#include "env/OMRVMAccessCriticalSection.hpp"
#include "env/CompilerEnv.hpp"
#include "infra/Annotations.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "j9.h"

namespace TR { class Compilation; }

namespace J9
{

class OMR_EXTENSIBLE VMAccessCriticalSection : public OMR::VMAccessCriticalSectionConnector
   {
public:

   VMAccessCriticalSection(
         TR_J9VMBase *fej9,
         VMAccessAcquireProtocol protocol,
         TR::Compilation *comp) :
      OMR::VMAccessCriticalSectionConnector(protocol, comp),
         _fej9(fej9)
      {
      _initializedBySubClass = true;
      switch (protocol)
         {
         case acquireVMAccessIfNeeded:
            _acquiredVMAccess = TR::Compiler->vm.acquireVMAccessIfNeeded(fej9);
            _hasVMAccess = true;
            break;

         case tryToAcquireVMAccess:
            _hasVMAccess = TR::Compiler->vm.tryToAcquireAccess(comp, &_acquiredVMAccess);
            break;
         }
      }


   VMAccessCriticalSection(
         TR::Compilation *comp,
         VMAccessAcquireProtocol protocol) :
      OMR::VMAccessCriticalSectionConnector(protocol, comp),
         _fej9(NULL)
      {
      _initializedBySubClass = true;
      switch (protocol)
         {
         case acquireVMAccessIfNeeded:
            _acquiredVMAccess = TR::Compiler->vm.acquireVMAccessIfNeeded(comp->fej9());
            _hasVMAccess = true;
            break;

         case tryToAcquireVMAccess:
            _hasVMAccess = TR::Compiler->vm.tryToAcquireAccess(comp, &_acquiredVMAccess);
            break;
         }
      }

   ~VMAccessCriticalSection()
      {
      if (!_vmAccessReleased)
         {
         if (_comp)
            {
            switch (_protocol)
               {
               case acquireVMAccessIfNeeded:
                  TR::Compiler->vm.releaseVMAccessIfNeeded(_comp, _acquiredVMAccess);
                  break;

               case tryToAcquireVMAccess:
                  if (_hasVMAccess && _acquiredVMAccess)
                     {
                     TR::Compiler->vm.releaseAccess(_comp);
                     }
                  break;
               }
            }
         else if (_fej9)
            {
            switch (_protocol)
               {
               case acquireVMAccessIfNeeded:
                  TR::Compiler->vm.releaseVMAccessIfNeeded(_fej9, _acquiredVMAccess);
                  break;

               case tryToAcquireVMAccess:
                  if (_hasVMAccess && _acquiredVMAccess)
                     {
                     TR::Compiler->vm.releaseAccess(_fej9);
                     }
                  break;
               }
            }

         _vmAccessReleased = true;
         }
      }

protected:

   TR_J9VMBase *_fej9;
   };

}

#endif
