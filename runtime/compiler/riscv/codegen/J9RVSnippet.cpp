/*******************************************************************************
 * Copyright (c) 2019, 2022 IBM Corp. and others
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

#include "j9.h"

#include "codegen/CodeGenerator.hpp"
#include "codegen/J9RVSnippet.hpp"

TR::RVMonitorEnterSnippet::RVMonitorEnterSnippet(
   TR::CodeGenerator *codeGen,
   TR::Node *monitorNode,
   int32_t lwOffset,
   TR::LabelSymbol *incLabel,
   TR::LabelSymbol *callLabel,
   TR::LabelSymbol *restartLabel)
   : _incLabel(incLabel),
     _lwOffset(lwOffset),
     TR::RVHelperCallSnippet(codeGen, monitorNode, callLabel, monitorNode->getSymbolReference(), restartLabel)
   {
   TR_UNIMPLEMENTED();
   }

uint8_t *
TR::RVMonitorEnterSnippet::emitSnippetBody()
   {
   TR_UNIMPLEMENTED();

   uint8_t *buffer = cg()->getBinaryBufferCursor();
   return buffer;
   }

void
TR::RVMonitorEnterSnippet::print(TR::FILE *pOutFile, TR_Debug *debug)
   {
   TR_UNIMPLEMENTED();
   }

uint32_t TR::RVMonitorEnterSnippet::getLength(int32_t estimatedSnippetStart)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }

int32_t TR::RVMonitorEnterSnippet::setEstimatedCodeLocation(int32_t estimatedSnippetStart)
   {
   TR_UNIMPLEMENTED();
   return estimatedSnippetStart;
   }


TR::RVMonitorExitSnippet::RVMonitorExitSnippet(
   TR::CodeGenerator *codeGen,
   TR::Node *monitorNode,
   int32_t lwOffset,
   TR::LabelSymbol *decLabel,
   TR::LabelSymbol *callLabel,
   TR::LabelSymbol *restartLabel)
   : _decLabel(decLabel),
     _lwOffset(lwOffset),
     TR::RVHelperCallSnippet(codeGen, monitorNode, callLabel, monitorNode->getSymbolReference(), restartLabel)
   {
   TR_UNIMPLEMENTED();
   }

uint8_t *
TR::RVMonitorExitSnippet::emitSnippetBody()
   {
   TR_UNIMPLEMENTED();

   uint8_t *buffer = cg()->getBinaryBufferCursor();
   return buffer;
   }

void
TR::RVMonitorExitSnippet::print(TR::FILE *pOutFile, TR_Debug *debug)
   {
   TR_UNIMPLEMENTED();
   }

uint32_t
TR::RVMonitorExitSnippet::getLength(int32_t estimatedSnippetStart)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }

int32_t TR::RVMonitorExitSnippet::setEstimatedCodeLocation(int32_t estimatedSnippetStart)
   {
   TR_UNIMPLEMENTED();
   return estimatedSnippetStart;
   }
