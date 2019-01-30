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

#ifndef J9_SYMBOLREFERENCE_INCL
#define J9_SYMBOLREFERENCE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_SYMBOLREFERENCE_CONNECTOR
#define J9_SYMBOLREFERENCE_CONNECTOR
namespace J9 { class SymbolReference; }
namespace J9 { typedef J9::SymbolReference SymbolReferenceConnector; }
#endif

#include "il/OMRSymbolReference.hpp"

#include <stdint.h>
#include "compile/SymbolReferenceTable.hpp"
#include "env/KnownObjectTable.hpp"
#include "env/jittypes.h"
#include "infra/Annotations.hpp"

class mcount_t;
namespace TR { class Symbol; }

namespace J9
{

class OMR_EXTENSIBLE SymbolReference : public OMR::SymbolReferenceConnector
   {

public:

   SymbolReference(TR::SymbolReferenceTable * symRefTab) :
      OMR::SymbolReferenceConnector(symRefTab) {}

   SymbolReference(TR::SymbolReferenceTable * symRefTab,
                   TR::Symbol * symbol,
                   intptrj_t offset = 0) :
      OMR::SymbolReferenceConnector(symRefTab,
                                    symbol,
                                    offset) {}

   SymbolReference(TR::SymbolReferenceTable * symRefTab,
                   int32_t refNumber,
                   TR::Symbol *ps,
                   intptrj_t offset = 0) :
      OMR::SymbolReferenceConnector(symRefTab,
                                    refNumber,
                                    ps,
                                    offset) {}

   SymbolReference(TR::SymbolReferenceTable *symRefTab,
                   TR::SymbolReferenceTable::CommonNonhelperSymbol number,
                   TR::Symbol *ps,
                   intptrj_t offset = 0) :
      OMR::SymbolReferenceConnector(symRefTab,
                                    number,
                                    ps,
                                    offset) {}

   SymbolReference(TR::SymbolReferenceTable *symRefTab,
                   TR::Symbol *sym,
                   mcount_t owningMethodIndex,
                   int32_t cpIndex,
                   int32_t unresolvedIndex = 0,
                   TR::KnownObjectTable::Index knownObjectIndex = TR::KnownObjectTable::UNKNOWN);

   SymbolReference(TR::SymbolReferenceTable *symRefTab,
                   TR::SymbolReference& sr,
                   intptrj_t offset,
                   TR::KnownObjectTable::Index knownObjectIndex = TR::KnownObjectTable::UNKNOWN) :
      OMR::SymbolReferenceConnector(symRefTab,
                                    sr,
                                    offset,
                                    knownObjectIndex) {}

   uint32_t getCPIndexForVM();

   bool isClassArray(TR::Compilation * c);
   bool isClassFinal(TR::Compilation * c);
   bool isClassAbstract(TR::Compilation * c);
   bool isClassInterface(TR::Compilation * c);

   bool isNonArrayFinal(TR::Compilation * c);
   int32_t classDepth(TR::Compilation * c);

   /**
    * Return the signature of the symbol's type if applicable. Note, the
    * signature's storage may have been created on the stack!
    */
   const char *getTypeSignature(int32_t & len, TR_AllocationKind = stackAlloc, bool *isFixed = NULL);

protected:

   SymbolReference(TR::SymbolReferenceTable * symRefTab,
                   TR::Symbol *               symbol,
                   intptrj_t                  offset,
                   const char *               name) :
      OMR::SymbolReferenceConnector(symRefTab,
                                    symbol,
                                    offset,
                                    name) {}

   };


char * prependNumParensToSig(const char *, int32_t & len, int32_t,  TR_AllocationKind = stackAlloc);

}

#endif
