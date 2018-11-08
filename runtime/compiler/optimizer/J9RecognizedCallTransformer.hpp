/*******************************************************************************
* Copyright (c) 2017, 2018 IBM Corp. and others
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

#ifndef J9RECOGNIZEDCALLTRANSFORMER_INCL
#define J9RECOGNIZEDCALLTRANSFORMER_INCL

#include "optimizer/OMRRecognizedCallTransformer.hpp"
#include "compile/SymbolReferenceTable.hpp"

namespace J9
{

class RecognizedCallTransformer : public OMR::RecognizedCallTransformer
   {
   public:
   RecognizedCallTransformer(TR::OptimizationManager* manager)
      : OMR::RecognizedCallTransformer(manager)
      {}

   protected:
   virtual bool isInlineable(TR::TreeTop* treetop);
   virtual void transform(TR::TreeTop* treetop);

   private:
   void processIntrinsicFunction(TR::TreeTop* treetop, TR::Node* node, TR::ILOpCodes opcode);
   /** \brief
    *     Transforms java/lang/Class.IsAssignableFrom(Ljava/lang/Class;)Z into a JIT helper call TR_checkAssignable with equivalent
    *     semantics.
    *
    *  \param treetop
    *     The treetop which anchors the call node.
    *
    *  \param node
    *     The call node representing a call to java/lang/Class.IsAssignableFrom(Ljava/lang/Class;)Z which has the following shape:
    *
    *     \code
    *     icall <java/lang/Class.IsAssignableFrom(Ljava/lang/Class;)Z>
    *       <cast class object>
    *       <class object to be checked>
    *     \endcode
    */
   void process_java_lang_Class_IsAssignableFrom(TR::TreeTop* treetop, TR::Node* node);
   /** \brief
    *     Transforms java/lang/StringUTF16.toBytes([CII)[B into a fast allocate and arraycopy sequence with equivalent
    *     semantics.
    *
    *  \param treetop
    *     The treetop which anchors the call node.
    *
    *  \param node
    *     The call node representing a call to java/lang/StringUTF16.toBytes([CII)[B which has the following shape:
    *
    *     \code
    *     acall <java/lang/StringUTF16.toBytes([CII)[B>
    *       <value>
    *       <off>
    *       <len>
    *     \endcode
    */
   void process_java_lang_StringUTF16_toBytes(TR::TreeTop* treetop, TR::Node* node);
   /** \brief
    *     Transforms certain Unsafe atomic helpers into a CodeGen inlined helper with equivalent semantics.
    *
    *  \param treetop
    *     The treetop which anchors the call node.
    *
    *  \param helper
    *     The CodeGen inlined helper being transformed into
    *
    *  \param needsNullCheck
    *     Flag indicating if null check is needed on the first argument of the unsafe call
    */
   void processUnsafeAtomicCall(TR::TreeTop* treetop, TR::SymbolReferenceTable::CommonNonhelperSymbol helper, bool needsNullCheck = false);
   };

}
#endif
