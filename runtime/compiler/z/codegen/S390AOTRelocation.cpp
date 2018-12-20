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

#include "z/codegen/S390AOTRelocation.hpp"

#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "z/codegen/S390Instruction.hpp"
#include "env/VMJ9.h"

void TR::S390PairedRelocation::mapRelocation(TR::CodeGenerator *cg)
   {
   if (cg->comp()->getOption(TR_AOT))
      {
      cg->addExternalRelocation(
         new (cg->trHeapMemory()) TR::ExternalOrderedPair32BitRelocation(
            getSourceInstruction()->getBinaryEncoding(),
            getSource2Instruction()->getBinaryEncoding(),
            getRelocationTarget(),
            getKind(), cg),
         __FILE__, __LINE__, getSourceInstruction()->getNode());
      }
   }

void TR::S390EncodingRelocation::addRelocation(TR::CodeGenerator *cg, uint8_t *cursor, char* file, uintptr_t line, TR::Node* node)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   if (_reloType==TR_ClassAddress)
      {
      AOTcgDiag2(  comp, "TR_ClassAddress cursor=%x symbolReference=%x\n", cursor, _symbolReference);
      if (cg->comp()->getOption(TR_UseSymbolValidationManager))
         {
         TR_OpaqueClassBlock *clazz = (TR_OpaqueClassBlock*)(*((uintptrj_t*)cursor));
         TR_ASSERT_FATAL(clazz, "TR_ClassAddress relocation : cursor = %x, clazz can not be null", cursor);
         cg->addExternalRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation(cursor, 
                                                                           (uint8_t *)clazz, 
                                                                           (uint8_t *) TR::SymbolType::typeClass,
                                                                           TR_SymbolFromManager,
                                                                           cg),
                                                                        file, line, node);

         }
      else
         {
         *((uintptrj_t*)cursor)=fej9->getPersistentClassPointerFromClassPointer((TR_OpaqueClassBlock*)(*((uintptrj_t*)cursor)));
         cg->addExternalRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *) _symbolReference, (uint8_t *)_inlinedSiteIndex, TR_ClassAddress, cg),
                           file, line, node);
         }
      }
   else if (_reloType==TR_RamMethod)
      {
      AOTcgDiag1(  comp, "TR_RamMethod cursor=%x\n", cursor);
      if (cg->comp()->getOption(TR_UseSymbolValidationManager))
         {
         TR::ResolvedMethodSymbol *methodSym = (TR::ResolvedMethodSymbol*) _symbolReference->getSymbol();
         uint8_t * j9Method = (uint8_t *) (reinterpret_cast<intptrj_t>(methodSym->getResolvedMethod()->resolvedMethodAddress()));
         TR_ASSERT_FATAL(j9Method, "TR_RamMethod relocation : cursor = %x, j9Method can not be null", cursor);
         cg->addExternalRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation(cursor, 
                                                                           j9Method, 
                                                                           (uint8_t *) TR::SymbolType::typeMethod,
                                                                           TR_SymbolFromManager,
                                                                           cg),
                                                                        file, line, node);
         }
      else
         {
         cg->addExternalRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation(cursor, NULL, TR_RamMethod, cg), file, line, node);
         }
      }
   else if (_reloType==TR_HelperAddress)
      {
      AOTcgDiag1(  comp, "TR_HelperAddress cursor=%x\n", cursor);
      cg->addExternalRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *) _symbolReference, TR_HelperAddress, cg),
                           file, line, node);
      }
   else if (_reloType==TR_AbsoluteHelperAddress)
      {
      AOTcgDiag1(  comp, "TR_AbsoluteHelperAddress cursor=%x\n", cursor);
      cg->addExternalRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *) _symbolReference, TR_AbsoluteHelperAddress, cg),
                              file, line, node);
      }
   else if (_reloType==TR_ConstantPool)
      {
      AOTcgDiag1(  comp, "TR_ConstantPool cursor=%x\n", cursor);
      if (TR::Compiler->target.is64Bit())
         {
         cg->addExternalRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *) *((uint64_t*) cursor), (uint8_t *)_inlinedSiteIndex, TR_ConstantPool, cg),
                              file, line, node);
         }
      else
         {
         cg->addExternalRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *)(intptr_t) *((uint32_t*) cursor), (uint8_t *)_inlinedSiteIndex, TR_ConstantPool, cg),
                              file, line, node);
         }
      }
   else if (_reloType==TR_MethodObject)
      {
      AOTcgDiag1(  comp, "TR_MethodObject cursor=%x\n", cursor);
      cg->addExternalRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *) _symbolReference, (uint8_t *)_inlinedSiteIndex, TR_MethodObject, cg),
                              file, line, node);
      }
   else if (_reloType==TR_DataAddress)
      {
      AOTcgDiag1(  comp, "TR_DataAddress cursor=%x\n", cursor);
      cg->addExternalRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *) _symbolReference, (uint8_t *)_inlinedSiteIndex, TR_DataAddress, cg),
                              file, line, node);
      }
   else if (_reloType==TR_BodyInfoAddress)
      {
      AOTcgDiag1(  comp, "TR_BodyInfoAddress cursor=%x\n", cursor);
      if (TR::Compiler->target.is64Bit())
         {
         cg->addExternalRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *) *((uint64_t*) cursor), TR_BodyInfoAddress, cg),
                              file, line, node);
         }
      else
         {
         cg->addExternalRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *)(intptr_t) *((uint32_t*) cursor), TR_BodyInfoAddress, cg),
                           file, line, node);
         }
      }
   else if (_reloType==TR_DebugCounter)
      {
      TR::DebugCounterBase *counter = cg->comp()->getCounterFromStaticAddress(_symbolReference);
      if (counter == NULL)
         {
         cg->comp()->failCompilation<TR::CompilationException>("Could not generate relocation for debug counter in TR::S390EncodingRelocation::addRelocation\n");
         }
      TR::DebugCounter::generateRelocation(cg->comp(),
                                           cursor,
                                           node,
                                           counter);
      }
   else
      {
      TR_ASSERT(0,"relocation type [%d] not handled yet", _reloType);
      }
   }
