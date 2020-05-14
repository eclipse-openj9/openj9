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

#ifndef J9PPCSNIPPET_INCL
#define J9PPCSNIPPET_INCL

#include "p/codegen/InstOpCode.hpp"
#include "codegen/Snippet.hpp"
#include "p/codegen/PPCHelperCallSnippet.hpp"
#include "env/IO.hpp"
#include "j9cfg.h"

#define LOCK_THREAD_PTR_MASK                              (~OBJECT_HEADER_LOCK_BITS_MASK)
#define LOCK_LAST_RECURSION_BIT_NUMBER                    leadingZeroes(OBJECT_HEADER_LOCK_LAST_RECURSION_BIT)

namespace TR {

class PPCReadMonitorSnippet : public TR::PPCHelperCallSnippet
   {
   TR::SymbolReference *_monitorEnterHelper;
   TR::LabelSymbol      *_recurCheckLabel;
   TR::InstOpCode::Mnemonic       _loadOpCode;
   int32_t             _loadOffset;
   TR::Register        *_objectClassReg;

   public:

   PPCReadMonitorSnippet(TR::CodeGenerator   *codeGen,
                         TR::Node            *monitorEnterNode,
                         TR::Node            *monitorExitNode,
                         TR::LabelSymbol      *recurCheckLabel,
                         TR::LabelSymbol      *monExitCallLabel,
                         TR::LabelSymbol      *restartLabel,
                         TR::InstOpCode::Mnemonic      loadOpCode,
                         int32_t            loadOffset,
                         TR::Register        *objectClassReg);

   virtual Kind getKind() { return IsReadMonitor; }

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   virtual void print(TR::FILE *, TR_Debug*);

   virtual int32_t setEstimatedCodeLocation(int32_t p);

   TR::SymbolReference *getMonitorEnterHelper() { return _monitorEnterHelper; };

   TR::LabelSymbol *getRecurCheckLabel() { return _recurCheckLabel; };

   TR::InstOpCode::Mnemonic getLoadOpCode() { return _loadOpCode; }

   int32_t getLoadOffset() { return _loadOffset; }
   };

class PPCAllocPrefetchSnippet : public TR::Snippet
   {

   public:

   PPCAllocPrefetchSnippet(TR::CodeGenerator   *codeGen,
                           TR::Node            *node,
                           TR::LabelSymbol      *callLabel);

   virtual Kind getKind() { return IsAllocPrefetch; }

   virtual uint8_t *emitSnippetBody();
   virtual void print(TR::FILE *, TR_Debug*);

   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   };

class PPCNonZeroAllocPrefetchSnippet : public TR::Snippet
   {

   public:

   PPCNonZeroAllocPrefetchSnippet(TR::CodeGenerator   *codeGen,
                                  TR::Node            *node,
                                  TR::LabelSymbol      *callLabel);

   virtual Kind getKind() { return IsNonZeroAllocPrefetch; }

   virtual uint8_t *emitSnippetBody();
   virtual void print(TR::FILE *, TR_Debug*);

   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   };

uint32_t getCCPreLoadedCodeSize();
void createCCPreLoadedCode(uint8_t *CCPreLoadedCodeBase, uint8_t *CCPreLoadedCodeTop, void **CCPreLoadedCodeTable, TR::CodeGenerator *cg);

}

#endif // J9PPCSNIPPET_INCL
