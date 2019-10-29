/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef J9_METHOD_INCL
#define J9_METHOD_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_METHOD_CONNECTOR
#define J9_METHOD_CONNECTOR
namespace J9 { class Method; }
namespace J9 { typedef J9::Method MethodConnector; }
#endif

#include "compile/OMRMethod.hpp"

#include "codegen/RecognizedMethods.hpp"
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "infra/Flags.hpp"

class TR_ResolvedMethod;
class TR_FrontEnd;
class TR_OpaqueMethodBlock;
namespace TR { class Compilation; }

extern "C" {
struct J9UTF8;
struct J9Class;
struct J9Method;
struct J9ROMClass;
struct J9ROMMethod;
}


namespace J9
{

class Method : public OMR::MethodConnector
   {
   public:

   Method(Type t = J9) :
      OMR::MethodConnector(t) {}

   Method(TR_FrontEnd *trvm, TR_Memory *m, J9Class *aClazz, uintptr_t cpIndex);

   Method(TR_FrontEnd *trvm, TR_Memory *m, TR_OpaqueMethodBlock *aMethod);

   static bool isBigDecimalNameAndSignature(J9UTF8 *name, J9UTF8 *signature);
   static bool isBigDecimalMethod(J9ROMMethod *romMethod, J9ROMClass *romClass);
   static bool isBigDecimalMethod(J9UTF8 *className, J9UTF8 *name, J9UTF8 *signature);
   static bool isBigDecimalMethod(J9Method *j9Method);
   bool isBigDecimalMethod(TR::Compilation *comp = NULL);
   static bool isBigDecimalConvertersNameAndSignature(J9UTF8 *name, J9UTF8 *signature);
   static bool isBigDecimalConvertersMethod(J9ROMMethod *romMethod, J9ROMClass *romClass);
   static bool isBigDecimalConvertersMethod(J9UTF8 *className, J9UTF8 *name, J9UTF8 *signature);
   static bool isBigDecimalConvertersMethod(J9Method *j9Method);
   bool isBigDecimalConvertersMethod(TR::Compilation *comp = NULL);

   static bool isUnsafeGetPutWithObjectArg(TR::RecognizedMethod rm);
   static bool isUnsafeGetPutBoolean(TR::RecognizedMethod rm);
   static bool isUnsafePut(TR::RecognizedMethod rm);
   static bool isVolatileUnsafe(TR::RecognizedMethod rm);
   static TR::DataType unsafeDataTypeForArray(TR::RecognizedMethod rm);
   static TR::DataType unsafeDataTypeForObject(TR::RecognizedMethod rm);
   static bool isVarHandleOperationMethod(TR::RecognizedMethod rm);
   virtual bool isVarHandleAccessMethod(TR::Compilation * = NULL);

   virtual bool isUnsafeWithObjectArg( TR::Compilation * comp = NULL);
   virtual bool isUnsafeCAS(TR::Compilation * = NULL);
   virtual uint32_t numberOfExplicitParameters();
   virtual TR::DataType parmType(uint32_t parmNumber); // returns the type of the parmNumber'th parameter (0-based)

   virtual TR::ILOpCodes directCallOpCode();
   virtual TR::ILOpCodes indirectCallOpCode();

   virtual TR::DataType returnType();
   virtual uint32_t returnTypeWidth();
   virtual bool returnTypeIsUnsigned();
   virtual TR::ILOpCodes returnOpCode();

   virtual const char *signature(TR_Memory *, TR_AllocationKind = heapAlloc); // this actually returns the class, method, and signature!
   virtual uint16_t classNameLength();
   virtual uint16_t nameLength();
   virtual uint16_t signatureLength();
   virtual char *classNameChars(); // returns the utf8 of the class that this method is in.
   virtual char *nameChars(); // returns the utf8 of the method name
   virtual char *signatureChars(); // returns the utf8 of the signature

   void setSignature(char *newSignature, int32_t newSignatureLength, TR_Memory *); // JSR292
   virtual void setArchetypeSpecimen(bool b = true);

   virtual bool isConstructor();
   virtual bool isFinalInObject();
   virtual TR_MethodParameterIterator* getParameterIterator(TR::Compilation &comp, TR_ResolvedMethod *r);

   virtual bool isArchetypeSpecimen(){ return _flags.testAny(ArchetypeSpecimen); }

protected:
   friend class TR_Debug;
   friend class TR_DebugExt;
   uintptr_t    j9returnType();
   void         parseSignature(TR_Memory *);

   uintptr_t    _paramElements;
   uintptr_t    _paramSlots;

public:
   J9UTF8 *     _signature;
   J9UTF8 *     _name;
   J9UTF8 *     _className;

protected:
   uint8_t *    _argTypes;
   char *       _fullSignature;
   flags32_t    _flags;

   enum Flags
      {
      ArchetypeSpecimen = 0x00000001, // An "instance" of an archetype method, where the varargs portion of the signature has been expanded into zero or more args

      dummyLastEnum
      };
   };

}

#endif
