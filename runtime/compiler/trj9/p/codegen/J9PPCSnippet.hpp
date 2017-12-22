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

#ifndef J9PPCSNIPPET_INCL
#define J9PPCSNIPPET_INCL

#include "p/codegen/InstOpCode.hpp"
#include "codegen/Snippet.hpp"
#include "p/codegen/PPCHelperCallSnippet.hpp"
#include "env/IO.hpp"
#include "j9cfg.h"

#define LOCK_INC_DEC_VALUE                                OBJECT_HEADER_LOCK_FIRST_RECURSION_BIT
#define LOCK_THREAD_PTR_MASK                              (~OBJECT_HEADER_LOCK_BITS_MASK)
#define LOCK_THREAD_PTR_AND_UPPER_COUNT_BIT_MASK          (LOCK_THREAD_PTR_MASK | OBJECT_HEADER_LOCK_LAST_RECURSION_BIT)
#define LOCK_FIRST_RECURSION_BIT_NUMBER	          leadingZeroes(OBJECT_HEADER_LOCK_FIRST_RECURSION_BIT)
#define LOCK_LAST_RECURSION_BIT_NUMBER	          leadingZeroes(OBJECT_HEADER_LOCK_LAST_RECURSION_BIT)
#define LOCK_OWNING_NON_INFLATED_COMPLEMENT               (OBJECT_HEADER_LOCK_BITS_MASK & ~OBJECT_HEADER_LOCK_INFLATED)
#define LOCK_RESERVATION_BIT                              OBJECT_HEADER_LOCK_RESERVED
#define LOCK_RES_PRIMITIVE_ENTER_MASK                     (OBJECT_HEADER_LOCK_RECURSION_MASK | OBJECT_HEADER_LOCK_FLC)
#define LOCK_RES_PRIMITIVE_EXIT_MASK                      (OBJECT_HEADER_LOCK_BITS_MASK & ~OBJECT_HEADER_LOCK_RECURSION_MASK)
#define LOCK_RES_NON_PRIMITIVE_ENTER_MASK                 (OBJECT_HEADER_LOCK_RECURSION_MASK & ~OBJECT_HEADER_LOCK_LAST_RECURSION_BIT)
#define LOCK_RES_NON_PRIMITIVE_EXIT_MASK                  (OBJECT_HEADER_LOCK_RECURSION_MASK & ~OBJECT_HEADER_LOCK_FIRST_RECURSION_BIT)
#define LOCK_RES_OWNING_COMPLEMENT                        (OBJECT_HEADER_LOCK_RECURSION_MASK | OBJECT_HEADER_LOCK_FLC)
#define LOCK_RES_PRESERVE_ENTER_COMPLEMENT                (OBJECT_HEADER_LOCK_RECURSION_MASK & ~OBJECT_HEADER_LOCK_LAST_RECURSION_BIT)
#define LOCK_RES_CONTENDED_VALUE                          (OBJECT_HEADER_LOCK_FIRST_RECURSION_BIT|OBJECT_HEADER_LOCK_RESERVED|OBJECT_HEADER_LOCK_FLC)

namespace TR {

class PPCMonitorEnterSnippet : public TR::PPCHelperCallSnippet
   {
   TR::LabelSymbol *_incLabel;
   int32_t         _lwOffset;
   bool            _isReservationPreserving;
   TR::Register     *_objectClassReg;

   public:

   PPCMonitorEnterSnippet(TR::CodeGenerator   *codeGen,
                          TR::Node            *monitorNode,
                          int32_t            lwOffset,
                          bool               isPreserving,
                          TR::LabelSymbol      *incLabel,
                          TR::LabelSymbol      *callLabel,
                          TR::LabelSymbol      *restartLabel,
                          TR::Register        *objectClassReg);

   virtual Kind getKind() { return IsMonitorEnter; }

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   virtual void print(TR::FILE *, TR_Debug *);
   virtual int32_t setEstimatedCodeLocation(int32_t p);

   TR::LabelSymbol * getIncLabel() { return _incLabel; };
   int32_t getLockWordOffset() { return _lwOffset; }
   bool isReservationPreserving() { return _isReservationPreserving; }

   // This is here so that _objectClassReg's real reg can be retrieved for log printing.
   // Can be removed if that reg is exposed by a getter or something.
   // (Need to change the corresponding TR_Debug::print method to use it.)
   friend class TR_Debug;
   };

class PPCMonitorExitSnippet : public TR::PPCHelperCallSnippet
   {
   TR::LabelSymbol *_decLabel;
   TR::LabelSymbol *_restoreAndCallLabel;
   int32_t        _lwOffset;
   bool           _isReservationPreserving;
   bool           _isReadOnly;
   TR::Register    *_objectClassReg;

   public:

   PPCMonitorExitSnippet(TR::CodeGenerator   *codeGen,
                         TR::Node            *monitorNode,
                         int32_t            lwOffset,
                         bool               flag,
                         bool               isPreserving,
                         TR::LabelSymbol      *decLabel,
                         TR::LabelSymbol      *restoreAndCallLabel,
                         TR::LabelSymbol      *callLabel,
                         TR::LabelSymbol      *restartLabel,
                         TR::Register        *objectClassReg);

   virtual Kind getKind() { return IsMonitorExit; }

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   virtual void print(TR::FILE *, TR_Debug *);
   virtual int32_t setEstimatedCodeLocation(int32_t p);

   TR::LabelSymbol * getDecLabel() { return _decLabel; }
   TR::LabelSymbol * getRestoreAndCallLabel() { return _restoreAndCallLabel; }
   int32_t          getLockWordOffset() { return _lwOffset; }
   bool             isReservationPreserving() { return _isReservationPreserving; }

   // This is here so that _objectClassReg's real reg can be retrieved for log printing.
   // Can be removed if that reg is exposed by a getter or something.
   // (Need to change the corresponding TR_Debug::print method to use it.)
   friend class TR_Debug;
   };

class PPCLockReservationEnterSnippet : public TR::PPCHelperCallSnippet
   {
   TR::LabelSymbol *_startLabel;
   int32_t         _lwOffset;
   TR::Register    *_objectClassReg;

   public:

   PPCLockReservationEnterSnippet(TR::CodeGenerator   *cg,
                                  TR::Node            *monitorNode,
                                  int32_t            lwOffset,
                                  TR::LabelSymbol      *startLabel,
                                  TR::LabelSymbol      *enterCallLabel,
                                  TR::LabelSymbol      *restartLabel,
                                  TR::Register        *objectClassReg);

   virtual Kind getKind() { return IsLockReservationEnter; }

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   virtual void print(TR::FILE *, TR_Debug*);
   virtual int32_t setEstimatedCodeLocation(int32_t p);

   TR::LabelSymbol * getStartLabel() { return _startLabel; };
   int32_t getLockWordOffset() { return _lwOffset; }
   };

class PPCLockReservationExitSnippet : public TR::PPCHelperCallSnippet
   {
   TR::LabelSymbol *_startLabel;
   int32_t        _lwOffset;
   TR::Register    *_objectClassReg;

   public:

   PPCLockReservationExitSnippet(TR::CodeGenerator   *codeGen,
                                 TR::Node            *monitorNode,
                                 int32_t            lwOffset,
                                 TR::LabelSymbol      *startLabel,
                                 TR::LabelSymbol      *exitCallLabel,
                                 TR::LabelSymbol      *restartLabel,
                                 TR::Register        *objectClassReg);

   virtual Kind getKind() { return IsLockReservationExit; }

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   virtual void print(TR::FILE *, TR_Debug*);

   virtual int32_t setEstimatedCodeLocation(int32_t p);

   TR::LabelSymbol * getStartLabel() { return _startLabel; }
   int32_t          getLockWordOffset() { return _lwOffset; }
   };

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

class PPCHeapAllocSnippet : public TR::Snippet
   {
   TR::LabelSymbol      *_restartLabel;
   TR::SymbolReference *_destination;
   bool               _insertType;

   public:

   PPCHeapAllocSnippet(TR::CodeGenerator   *codeGen,
                       TR::Node            *node,
                       TR::LabelSymbol      *callLabel,
                       TR::SymbolReference *destination,
                       TR::LabelSymbol      *restartLabel,
                       bool               insertType=false);

   virtual Kind getKind() { return IsHeapAlloc; }
   virtual void print(TR::FILE *, TR_Debug*);

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);

   TR::LabelSymbol      *getRestartLabel() { return _restartLabel; }
   TR::SymbolReference *getDestination() {return _destination;}
   bool                getInsertType() {return _insertType;}
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
