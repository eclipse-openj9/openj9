/*******************************************************************************
 * Copyright (c) 2000, 2021 IBM Corp. and others
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

#ifndef J9_Z_AHEADOFTIMECOMPILE_INCL
#define J9_Z_AHEADOFTIMECOMPILE_INCL

#ifndef J9_AHEADOFTIMECOMPILE_CONNECTOR
#define J9_AHEADOFTIMECOMPILE_CONNECTOR
namespace J9 { namespace Z { class AheadOfTimeCompile; } }
namespace J9 { typedef J9::Z::AheadOfTimeCompile AheadOfTimeCompileConnector; }
#endif // J9_AHEADOFTIMECOMPILE_CONNECTOR

#include "compiler/codegen/J9AheadOfTimeCompile.hpp"

#include "compile/SymbolReferenceTable.hpp"
#include "codegen/S390AOTRelocation.hpp"

namespace TR { class CodeGenerator; }

namespace J9
{

namespace Z
{

class OMR_EXTENSIBLE AheadOfTimeCompile : public J9::AheadOfTimeCompile
   {
   public:
     AheadOfTimeCompile(TR::CodeGenerator *cg);

    virtual void     processRelocations();

     /**
      * @brief Initialization of relocation record headers for whom data for the fields are acquired
      *        in a manner that is specific to this platform
      *
      * @param relocation pointer to the iterated external relocation
      * @param reloTarget pointer to the TR_RelocationTarget object
      * @param reloRecord pointer to the associated
      * @param targetKind the TR_ExternalRelocationTargetKind enum value
      */
    void initializePlatformSpecificAOTRelocationHeader(TR::IteratedExternalRelocation *relocation, TR_RelocationTarget *reloTarget, TR_RelocationRecord *reloRecord, uint8_t targetKind);

    TR::list<TR::S390Relocation*>& getRelocationList() {return _relocationList;}

  private:
    TR::list<TR::S390Relocation*>     _relocationList;
    TR::CodeGenerator *_cg;
   };

} // namespace Z

} // namespace J9

#endif


