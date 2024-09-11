/*******************************************************************************
 * Copyright IBM Corp. and others 2017
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/



#ifndef J9_STATICSYMBOL_INLINES_INCL
#define J9_STATICSYMBOL_INLINES_INCL

/**
 * If OMRStaticInlines gets created, be sure to include here
 */

#include <stddef.h>
#include "env/TRMemory.hpp"
#include "il/J9StaticSymbol.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Symbol.hpp"


inline void
J9::StaticSymbol::makeMethodTypeTableEntry(int32_t methodTypeIndex)
   {
   TR_ASSERT(self()->getDataType() == TR::Address, "MethodTypeTableEntries have historically had TR::Address as data type");
   _methodTypeIndex = methodTypeIndex;
   self()->setMethodTypeTableEntry();

   // Needed so that the relo infra does not think this is a static field address
   self()->setNotDataAddress();
   }

inline void
J9::StaticSymbol::makeCallSiteTableEntry(int32_t callSiteIndex)
   {
   TR_ASSERT(self()->getDataType() == TR::Address, "CallSiteTableEntries have historically had TR::Address as data type");
   _callSiteIndex = callSiteIndex;
   self()->setCallSiteTableEntry();

   // Needed so that the relo infra does not think this is a static field address
   self()->setNotDataAddress();
   }


inline int32_t
J9::StaticSymbol::getCallSiteIndex()
   {
   TR_ASSERT(self()->isCallSiteTableEntry(), "Must have called makeCallSiteTableEntry to have a valid callSiteIndex!");
   return _callSiteIndex;
   }

inline void
J9::StaticSymbol::makeConstantDynamic(char * classSignature, int32_t classSignatureLength, bool isPrimitive)
   {
   TR_ASSERT(self()->getDataType() == TR::Address, "ConstantDynamic should have TR::Address as data type");
   _classSignature = classSignature;
   _classSignatureLength = classSignatureLength;
   _isPrimitive = isPrimitive;
   }

inline char *
J9::StaticSymbol::getConstantDynamicClassSignature(int32_t & classSignatureLength)
   {
   classSignatureLength = _classSignatureLength;
   return _classSignature;
   }

inline bool
J9::StaticSymbol::isConstantDynamicPrimitive()
   {
   return _isPrimitive;
   }

inline TR::Symbol::RecognizedField
J9::StaticSymbol::getRecognizedField()
   {
   if (self()->isRecognizedStatic())
      return _recognizedField;
   else
      return TR::Symbol::UnknownField;
   }


inline int32_t
J9::StaticSymbol::getMethodTypeIndex()
   {
   TR_ASSERT(self()->isMethodTypeTableEntry(), "Must have called makeMethodTypeTableEntry() to have a valid MethodTypeIndex()");
   return _methodTypeIndex;
   }

#endif
