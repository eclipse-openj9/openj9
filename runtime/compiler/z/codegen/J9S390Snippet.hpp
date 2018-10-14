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

#ifndef J9S390SNIPPET_INCL
#define J9S390SNIPPET_INCL

#include "codegen/Snippet.hpp"
#include "z/codegen/S390HelperCallSnippet.hpp"
#include "codegen/CodeGenerator.hpp"
#include "env/IO.hpp"

#define LOCK_INC_DEC_VALUE                                OBJECT_HEADER_LOCK_FIRST_RECURSION_BIT
#define LOCK_THREAD_PTR_MASK                              (~OBJECT_HEADER_LOCK_BITS_MASK)
#define LOCK_THREAD_PTR_AND_UPPER_COUNT_BIT_MASK          (LOCK_THREAD_PTR_MASK | OBJECT_HEADER_LOCK_LAST_RECURSION_BIT)
#define LOCK_FIRST_RECURSION_BIT_NUMBER             leadingZeroes(OBJECT_HEADER_LOCK_FIRST_RECURSION_BIT)
#define LOCK_LAST_RECURSION_BIT_NUMBER           leadingZeroes(OBJECT_HEADER_LOCK_LAST_RECURSION_BIT)
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

class S390HeapAllocSnippet : public TR::Snippet
   {
   TR::LabelSymbol      *_restartLabel;
   TR::SymbolReference *_destination;
   uint16_t            _length;
   bool                _isLongBranch;


   public:

   S390HeapAllocSnippet(TR::CodeGenerator   *codeGen,
                        TR::Node            *node,
                        TR::LabelSymbol      *callLabel,
                        TR::SymbolReference *destination,
                        TR::LabelSymbol      *restartLabel);


   virtual Kind getKind() { return IsHeapAlloc; }

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);

   virtual void print(TR::FILE *, TR_Debug *debug);

   TR::LabelSymbol      *getRestartLabel() { return _restartLabel; }
   TR::SymbolReference *getDestination() {return _destination;}

   bool                isLongBranch() {return _isLongBranch;}
   void                setIsLongBranch(bool b) {_isLongBranch = b;}

   uint32_t            getMyLength() {return _length;}
   void                setMyLength(uint32_t l) {_length = (uint16_t)l;}
   };

}

#endif
