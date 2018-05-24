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

#ifndef J9_Z_MEMORY_REFERENCE_INCL
#define J9_Z_MEMORY_REFERENCE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_MEMORY_REFERENCE_CONNECTOR
#define J9_MEMORY_REFERENCE_CONNECTOR
   namespace J9 { namespace Z { class MemoryReference; } }
   namespace J9 {typedef J9::Z::MemoryReference MemoryReferenceConnector; }
#else
   #error J9::Z::MemoryReference expected to be a primary connector, but a J9 connector is already defined
#endif

#include "codegen/OMRMemoryReference.hpp"

namespace TR { class Snippet; }


namespace J9
{

namespace Z
{

class OMR_EXTENSIBLE MemoryReference : public OMR::MemoryReferenceConnector
   {
public:
   MemoryReference(TR::CodeGenerator *cg) : OMR::MemoryReferenceConnector(cg) {}

   MemoryReference(TR::Register      *br,
                          int32_t           disp,
                          TR::CodeGenerator *cg,
                          const char *name=NULL ) :
           OMR::MemoryReferenceConnector(br, disp, cg, name) {}

   MemoryReference(TR::Register      *br,
                          int32_t           disp,
                          TR::SymbolReference *symRef,
                          TR::CodeGenerator *cg) :
           OMR::MemoryReferenceConnector(br, disp, symRef, cg) {}

   MemoryReference(TR::Register      *br,
                          TR::Register      *ir,
                          int32_t           disp,
                          TR::CodeGenerator *cg) :
           OMR::MemoryReferenceConnector(br, ir, disp, cg) {}

   MemoryReference(int32_t disp, TR::CodeGenerator *cg, bool isConstantDataSnippet=false) :
            OMR::MemoryReferenceConnector(disp, cg, isConstantDataSnippet) {}

   MemoryReference(TR::Node *rootLoadOrStore, TR::CodeGenerator *cg, bool canUseRX = false, TR_StorageReference *storageReference=NULL) :
            OMR::MemoryReferenceConnector(rootLoadOrStore, cg, canUseRX, storageReference) {}

   MemoryReference(TR::Node *addressTree, bool canUseIndex, TR::CodeGenerator *cg) :
            OMR::MemoryReferenceConnector(addressTree, canUseIndex, cg) {}

   MemoryReference(TR::Node *node, TR::SymbolReference *symRef, TR::CodeGenerator *cg, TR_StorageReference *storageReference=NULL) :
            OMR::MemoryReferenceConnector(node, symRef, cg, storageReference) {}
   MemoryReference(TR::Snippet *s, TR::CodeGenerator *cg, TR::Register* base, TR::Node* node) :
            OMR::MemoryReferenceConnector(s, cg, base, node) {}

   MemoryReference(TR::Snippet *s, TR::Register* indx, int32_t disp, TR::CodeGenerator *cg) :
            OMR::MemoryReferenceConnector(s, indx, disp, cg) {}

   MemoryReference(MemoryReference& mr, int32_t n, TR::CodeGenerator *cg) :
            OMR::MemoryReferenceConnector(mr, n, cg) {}

   void addInstrSpecificRelocation(TR::CodeGenerator* cg, TR::Instruction* instr, int32_t disp, uint8_t * cursor);


   static bool typeNeedsAlignment(TR::Node *node);

   TR::UnresolvedDataSnippet * createUnresolvedDataSnippet(TR::Node * node, TR::CodeGenerator * cg, TR::SymbolReference * symRef, TR::Register * tempReg, bool isStore);
   TR::UnresolvedDataSnippet * createUnresolvedDataSnippetForiaload(TR::Node * node, TR::CodeGenerator * cg, TR::SymbolReference * symRef, TR::Register * tempReg, bool & isStore);
   void createUnresolvedSnippetWithNodeRegister(TR::Node * node, TR::CodeGenerator * cg, TR::SymbolReference * symRef, TR::Register *& writableLiteralPoolRegister);
   void createUnresolvedDataSnippetForBaseNode(TR::CodeGenerator * cg, TR::Register * writableLiteralPoolRegister);

   void createPatchableDataInLitpool(TR::Node * node, TR::CodeGenerator * cg, TR::Register * tempReg, TR::UnresolvedDataSnippet * uds);

   void incRefCountForOpaquePseudoRegister(TR::Node * node, TR::CodeGenerator * cg, TR::Compilation * comp);

   bool symRefHasTemporaryNegativeOffset();

   void setMemRefAndGetUnresolvedData(TR::Snippet *& snippet);
   void tryForceFolding(TR::Node *& rootLoadOrStore, TR::CodeGenerator *& cg, TR_StorageReference *& storageReference, TR::SymbolReference *& symRef, TR::Symbol *& symbol,
                        List<TR::Node>& nodesAlreadyEvaluatedBeforeFoldingList);
   };
}

}

TR::MemoryReference * reuseS390MemRefFromStorageRef(TR::MemoryReference *baseMR, int32_t offset, TR::Node *node, TR_StorageReference *storageReference, TR::CodeGenerator *cg, bool enforceSSLimits=true);
TR::MemoryReference * generateS390MemRefFromStorageRef(TR::Node *node, TR_StorageReference *storageReference, TR::CodeGenerator * cg, bool enforceSSLimits=true, bool isNewTemp=false);
TR::MemoryReference * generateS390RightAlignedMemoryReference(TR::Node *node, TR_StorageReference *storageReference, TR::CodeGenerator * cg, bool enforceSSLimits=true, bool isNewTemp=false);
TR::MemoryReference * generateS390LeftAlignedMemoryReference(TR::Node *node, TR_StorageReference *storageReference, TR::CodeGenerator * cg, int32_t leftMostByte, bool enforceSSLimits=true, bool isNewTemp=false);

TR::MemoryReference * reuseS390LeftAlignedMemoryReference(TR::MemoryReference *baseMR, TR::Node *node, TR_StorageReference *storageReference, TR::CodeGenerator *cg, int32_t leftMostByte, bool enforceSSLimits=true);
TR::MemoryReference * reuseS390RightAlignedMemoryReference(TR::MemoryReference *baseMR, TR::Node *node, TR_StorageReference *storageReference, TR::CodeGenerator *cg, bool enforceSSLimits=true);

#endif
