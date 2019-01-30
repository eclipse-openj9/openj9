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

#ifndef TR_SYMBOLREFERENCE_INCL
#define TR_SYMBOLREFERENCE_INCL

#include "il/J9SymbolReference.hpp"

#include <stdint.h>
#include "env/KnownObjectTable.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "env/jittypes.h"
#include "infra/Annotations.hpp"

class mcount_t;
namespace TR { class Symbol; }

namespace TR
{

class OMR_EXTENSIBLE SymbolReference : public J9::SymbolReferenceConnector
   {

public:

   SymbolReference(TR::SymbolReferenceTable * symRefTab) :
      J9::SymbolReferenceConnector(symRefTab) {}

   SymbolReference(TR::SymbolReferenceTable * symRefTab,
                   TR::Symbol * symbol,
                   intptrj_t offset = 0) :
      J9::SymbolReferenceConnector(symRefTab,
                                        symbol,
                                        offset) {}

   SymbolReference(TR::SymbolReferenceTable * symRefTab,
                   int32_t refNumber,
                   TR::Symbol *ps,
                   intptrj_t offset = 0) :
      J9::SymbolReferenceConnector(symRefTab,
                                        refNumber,
                                        ps,
                                        offset) {}

   SymbolReference(TR::SymbolReferenceTable *symRefTab,
                   TR::SymbolReferenceTable::CommonNonhelperSymbol number,
                   TR::Symbol *ps,
                   intptrj_t offset = 0) :
      J9::SymbolReferenceConnector(symRefTab,
                                        number,
                                        ps,
                                        offset) {}

   SymbolReference(TR::SymbolReferenceTable *symRefTab,
                   TR::Symbol *sym,
                   mcount_t owningMethodIndex,
                   int32_t cpIndex,
                   int32_t unresolvedIndex = 0,
                   TR::KnownObjectTable::Index knownObjectIndex = TR::KnownObjectTable::UNKNOWN) :
      J9::SymbolReferenceConnector(symRefTab,
                                        sym,
                                        owningMethodIndex,
                                        cpIndex,
                                        unresolvedIndex,
                                        knownObjectIndex) {}

   SymbolReference(TR::SymbolReferenceTable *symRefTab,
                   TR::SymbolReference& sr,
                   intptrj_t offset,
                   TR::KnownObjectTable::Index knownObjectIndex = TR::KnownObjectTable::UNKNOWN) :
      J9::SymbolReferenceConnector(symRefTab,
                                        sr,
                                        offset,
                                        knownObjectIndex) {}

protected:

   SymbolReference(TR::SymbolReferenceTable * symRefTab,
                   TR::Symbol *          symbol,
                   intptrj_t              offset,
                   const char *           name) :
      J9::SymbolReferenceConnector(symRefTab,
                                        symbol,
                                        offset,
                                        name) {}

   };

}

#endif
