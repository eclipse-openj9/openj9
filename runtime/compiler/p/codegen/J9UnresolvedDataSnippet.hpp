/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#ifndef J9_PPCUNRESOLVEDDATASNIPPET_INCL
#define J9_PPCUNRESOLVEDDATASNIPPET_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_UNRESOLVEDDATASNIPPET_CONNECTOR
#define J9_UNRESOLVEDDATASNIPPET_CONNECTOR
namespace J9 { namespace Power { class UnresolvedDataSnippet; } }
namespace J9 { typedef J9::Power::UnresolvedDataSnippet UnresolvedDataSnippetConnector; }
#else
#error J9::Power::UnresolvedDataSnippet expected to be a primary connector, but a J9 connector is already defined
#endif

#include "compiler/codegen/J9UnresolvedDataSnippet.hpp"

#include <stdint.h>
#include "codegen/Snippet.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Flags.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class MemoryReference; }
namespace TR { class Node; }
namespace TR { class RealRegister; }
namespace TR { class Symbol; }

namespace J9
{

namespace Power
{

class UnresolvedDataSnippet : public J9::UnresolvedDataSnippet
   {

   TR::MemoryReference  *_memoryReference;
   TR::RealRegister     *_dataRegister;

   public:

   UnresolvedDataSnippet(TR::CodeGenerator *, TR::Node *, TR::SymbolReference *s, bool isStore, bool canCauseGC);

   virtual Kind getKind() { return IsUnresolvedData; }

   TR::Symbol *getDataSymbol() {return getDataSymbolReference()->getSymbol();}

   TR::MemoryReference *getMemoryReference() {return _memoryReference;}
   TR::MemoryReference *setMemoryReference(TR::MemoryReference *mr)
      {return (_memoryReference = mr);}

   TR::RealRegister *getDataRegister() {return _dataRegister;}
   void setDataRegister(TR::RealRegister *r) {_dataRegister = r;}

   bool isSpecialDouble() {return _flags.testAll(TO_MASK32(IsSpecialDouble));}
   void setIsSpecialDouble() {_flags.set(TO_MASK32(IsSpecialDouble));}
   void resetIsSpecialDouble() {_flags.reset(TO_MASK32(IsSpecialDouble));}

   bool inSyncSequence() {return _flags.testAll(TO_MASK32(InSyncSequence));}
   void setInSyncSequence() {_flags.set(TO_MASK32(InSyncSequence));}
   void resetInSyncSequence() {_flags.reset(TO_MASK32(InSyncSequence));}

   bool is32BitLong() {return _flags.testAll(TO_MASK32(Is32BitLong));}
   void setIs32BitLong() {_flags.set(TO_MASK32(Is32BitLong));}
   void resetIs32BitLong() {_flags.reset(TO_MASK32(Is32BitLong));}

   uint8_t *getAddressOfDataReference();

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);


   protected:

   enum
      {
      IsSpecialDouble = J9::UnresolvedDataSnippet::NextSnippetFlag,
      InSyncSequence,
      Is32BitLong,
      NextSnippetFlag
      };

   static_assert((int32_t)NextSnippetFlag <= (int32_t)J9::UnresolvedDataSnippet::MaxSnippetFlag,
      "TR::UnresolvedDataSnippet too many flag bits for flag width");

   };

}

}

#endif
