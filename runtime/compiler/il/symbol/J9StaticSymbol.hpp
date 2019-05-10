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

#ifndef J9_STATICSYMBOL_INCL
#define J9_STATICSYMBOL_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_STATICSYMBOL_CONNECTOR
#define J9_STATICSYMBOL_CONNECTOR
namespace J9 { class StaticSymbol; }
namespace J9 { typedef J9::StaticSymbol StaticSymbolConnector; }
#endif

#include "il/Symbol.hpp"
#include "il/symbol/OMRStaticSymbol.hpp"

#include <stdint.h>
#include "il/DataTypes.hpp"

namespace TR { class StaticSymbol; }

namespace J9
{

/**
 * A symbol with an address
 */
class OMR_EXTENSIBLE StaticSymbol : public OMR::StaticSymbolConnector
   {

protected:

   StaticSymbol(TR::DataType d) :
      OMR::StaticSymbolConnector(d) { }

   StaticSymbol(TR::DataType d, void * address) :
      OMR::StaticSymbolConnector(d, address) { }

   StaticSymbol(TR::DataType d, uint32_t s) :
      OMR::StaticSymbolConnector(d, s) { }

   /* ------- TR_CallSiteTableEntrySymbol --------- */
public:

   void makeCallSiteTableEntry(int32_t callSiteIndex);
   int32_t getCallSiteIndex();

private:

   int32_t _callSiteIndex;

   /* ------- TR_ConstantDynamicSymbol --------- */
public:

   /** \brief
    *     Populate the class signature fields and primitive flag of this constant dynamic symbol.
    *
    *  \param classSignature
    *     The class signature string of the constant dynamic. For primitive this is the signature of the corresponding autobox class.
    *
    *  \param classSignatureLength
    *     The length of the class signature string of the constant dynamic.
    *
    *  \param isPrimitive
    *     Determines whether the constant dynamic is primitive type.
    *
    *  \return
    *     Fields of this static symbol are populated.
    */
   void makeConstantDynamic(char * classSignature, int32_t classSignatureLength, bool isPrimitive);

   /** \brief
    *     Retrieves the class signature string for this constant dynamic static symbol.
    *
    *  \param classSignatureLength
    *     The length of the class signature string of the constant dynamic. Modified in place.
    *
    *  \return
    *     The class signature string for this constant dynamic static symbol.
    */
   char * getConstantDynamicClassSignature(int32_t & classSignatureLength);

   /** \brief
    *     Retrieves whether the underlying constant dynamic is of primitive type.
    *
    *  \return
    *     <c>true</c> if the underlying constant dynamic is primitive; <c>false</c> otherwise.
    */
   bool isConstantDynamicPrimitive();

private:

   char * _classSignature;
   int32_t _classSignatureLength;
   bool _isPrimitive;

   /* ------ TR_RecognizedStaticSymbol ---------- */
public:

   template <typename AllocatorType>
   static TR::StaticSymbol * createRecognized(AllocatorType m, TR::DataType d, TR::Symbol::RecognizedField f);

   /**
    * If this symbol has a valid recognized field, return it. Otherwise, return
    * TR::Symbol::UnknownField
    */
   TR::Symbol::RecognizedField getRecognizedField();

private:

   void makeRecognized(TR::Symbol::RecognizedField f)
      {
      _recognizedField = f;
      _flags.set(RecognizedStatic);
      }

   TR::Symbol::RecognizedField _recognizedField;

   /* ------- TR_MethodTypeTableEntrySymbol ------ */
public:
   template <typename AllocatorType>
   static TR::StaticSymbol * createMethodTypeTableEntry(AllocatorType m, int32_t methodTypeIndex);

   int32_t getMethodTypeIndex();

private:
   void makeMethodTypeTableEntry(int32_t methodTypeIndex);
      

   int32_t _methodTypeIndex;

   };

}

#endif

