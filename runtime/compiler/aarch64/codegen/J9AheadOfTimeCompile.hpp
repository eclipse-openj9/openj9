/*******************************************************************************
 * Copyright IBM Corp. and others 2019
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

#ifndef J9_ARM64_AHEADOFTIMECOMPILE_INCL
#define J9_ARM64_AHEADOFTIMECOMPILE_INCL

#ifndef J9_AHEADOFTIMECOMPILE_CONNECTOR
#define J9_AHEADOFTIMECOMPILE_CONNECTOR
namespace J9 { namespace ARM64 { class AheadOfTimeCompile; } }
namespace J9 { typedef J9::ARM64::AheadOfTimeCompile AheadOfTimeCompileConnector; }
#endif // J9_AHEADOFTIMECOMPILE_CONNECTOR

#include "compiler/codegen/J9AheadOfTimeCompile.hpp"

#include "codegen/ARM64AOTRelocation.hpp"
#include "il/SymbolReference.hpp"

namespace TR { class CodeGenerator; }

namespace J9
{

namespace ARM64
{

class OMR_EXTENSIBLE AheadOfTimeCompile : public J9::AheadOfTimeCompile
   {
   public:

   /**
    * @brief Constructor
    */
   AheadOfTimeCompile(TR::CodeGenerator *cg);

   /**
    * @brief Processes relocations
    */
   virtual void processRelocations();


   /**
    * @brief Refer to J9::AheadOfTimeCompile::initializePlatformSpecificAOTRelocationHeader
    */
   bool initializePlatformSpecificAOTRelocationHeader(TR::IteratedExternalRelocation *relocation, TR_RelocationTarget *reloTarget, TR_RelocationRecord *reloRecord, uint8_t targetKind);

   static bool classAddressUsesReloRecordInfo() { return false; }

   private:
   TR::CodeGenerator *_cg;
   };

} // ARM64

} // J9

#endif
