/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#ifndef J9ARM64SNIPPET_INCL
#define J9ARM64SNIPPET_INCL

#include "codegen/Snippet.hpp"
#include "codegen/ARM64HelperCallSnippet.hpp"
#include "j9cfg.h"

namespace TR {

class ARM64MonitorEnterSnippet : public TR::ARM64HelperCallSnippet
   {
   TR::LabelSymbol *_incLabel;
   int32_t _lwOffset;

   public:

   /**
    * @brief Constructor
    */
   ARM64MonitorEnterSnippet(TR::CodeGenerator *codeGen,
                            TR::Node *monitorNode,
                            int32_t lwOffset,
                            TR::LabelSymbol *incLabel,
                            TR::LabelSymbol *callLabel,
                            TR::LabelSymbol *restartLabel);

   /**
    * @brief Answers the Snippet kind
    * @return Snippet kind
    */
   virtual Kind getKind() { return IsMonitorEnter; }

   /**
    * @brief Emits the Snippet body
    * @return instruction cursor
    */
   virtual uint8_t *emitSnippetBody();

   /**
    * @brief Prints the Snippet
    */
   virtual void print(TR::FILE *, TR_Debug *);

   /**
    * @brief Answers the Snippet length
    * @return Snippet length
    */
   virtual uint32_t getLength(int32_t estimatedSnippetStart);

   /**
    * @brief Sets estimated binary location
    * @return estimated binary location
    */
   virtual int32_t setEstimatedCodeLocation(int32_t p);

   /**
    * @brief Answers the incLabel
    * @return incLabel
    */
   TR::LabelSymbol * getIncLabel() { return _incLabel; };

   /**
    * @brief Answers the lock-word offset
    * @return lock-word offset
    */
   int32_t getLockWordOffset() { return _lwOffset; }
   };

class ARM64MonitorExitSnippet : public TR::ARM64HelperCallSnippet
   {
   TR::LabelSymbol *_decLabel;
   int32_t _lwOffset;

   public:

   /**
    * @brief Constructor
    */
   ARM64MonitorExitSnippet(TR::CodeGenerator *codeGen,
                           TR::Node *monitorNode,
                           int32_t lwOffset,
                           TR::LabelSymbol *decLabel,
                           TR::LabelSymbol *callLabel,
                           TR::LabelSymbol *restartLabel);

   /**
    * @brief Answers the Snippet kind
    * @return Snippet kind
    */
   virtual Kind getKind() { return IsMonitorExit; }

   /**
    * @brief Emits the Snippet body
    * @return instruction cursor
    */
   virtual uint8_t *emitSnippetBody();

   /**
    * @brief Prints the Snippet
    */
   virtual void print(TR::FILE *, TR_Debug *);

   /**
    * @brief Answers the Snippet length
    * @return Snippet length
    */
   virtual uint32_t getLength(int32_t estimatedSnippetStart);

   /**
    * @brief Sets estimated binary location
    * @return estimated binary location
    */
   virtual int32_t setEstimatedCodeLocation(int32_t p);

   /**
    * @brief Answers the decLabel
    * @return decLabel
    */
   TR::LabelSymbol * getDecLabel() { return _decLabel; }

   /**
    * @brief Answers the lock-word offset
    * @return lock-word offset
    */
   int32_t getLockWordOffset() { return _lwOffset; }
   };

}

#endif
