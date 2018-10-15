/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#ifndef J9_AHEADOFTIMECOMPILE_HPP
#define J9_AHEADOFTIMECOMPILE_HPP

#ifndef J9_AHEADOFTIMECOMPILE_CONNECTOR
#define J9_AHEADOFTIMECOMPILE_CONNECTOR
namespace J9 { class AheadOfTimeCompile; }
namespace J9 { typedef J9::AheadOfTimeCompile AheadOfTimeCompileConnector; }
#endif // J9_AHEADOFTIMECOMPILE_CONNECTOR

#include "codegen/OMRAheadOfTimeCompile.hpp"
#include "runtime/RelocationRecord.hpp"

#include <stdint.h>
#include "env/jittypes.h"

namespace TR { class Compilation; }

namespace J9
{

class OMR_EXTENSIBLE AheadOfTimeCompile : public OMR::AheadOfTimeCompileConnector
   {
   public:
   static const size_t SIZEPOINTER = sizeof(uintptrj_t);

   AheadOfTimeCompile(uint32_t *headerSizeMap, TR::Compilation * c) :
      OMR::AheadOfTimeCompileConnector(headerSizeMap, c)
      {
      }

   uint8_t* emitClassChainOffset(uint8_t* cursor, TR_OpaqueClassBlock* classToRemember);
   uintptr_t findCorrectInlinedSiteIndex(void *constantPool, uintptr_t currentInlinedSiteIndex);

   void dumpRelocationData();
   uint8_t* dumpRelocationHeaderData(uint8_t *cursor, bool isVerbose);

   uint8_t *initializeCommonAOTRelocationHeader(TR::IteratedExternalRelocation *relocation, TR_RelocationRecord *reloRecord);

   static void interceptAOTRelocation(TR::ExternalRelocation *relocation);

   /**
    * Return true if an ExternalRelocation of kind TR_ClassAddress is expected
    * to contain a pointer to TR_RelocationRecordInformation.
    */
   static bool classAddressUsesReloRecordInfo() { return false; }
   };

}

#endif // TR_J9AHEADOFTIMECOMPILE_HPP
