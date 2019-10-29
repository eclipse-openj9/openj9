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

#include "compile/Method.hpp"

#include <stddef.h>
#include "j9.h"
#include "j9cfg.h"
#include "jitprotos.h"
#include "jilconsts.h"
#include "util_api.h"
#include "rommeth.h"

#include "codegen/RecognizedMethods.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/j9method.h"
#include "env/jittypes.h"
#include "env/VMAccessCriticalSection.hpp"
#include "env/VMJ9.h"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "infra/Assert.hpp"


static TR::DataType J9ToTRMap[] =
   { TR::NoType, TR::Int8, TR::Int8, TR::Int16, TR::Int16, TR::Float, TR::Int32, TR::Double, TR::Int64, TR::Address, TR::Address };

static bool J9ToIsUnsignedMap[] =
   { false, true, false, true, false, false, false, false, false, false, false };

static U_32 J9ToSizeMap[] =
   { 0, sizeof(UDATA), sizeof(UDATA), sizeof(UDATA), sizeof(UDATA), sizeof(UDATA), sizeof(UDATA), sizeof(U_64), sizeof(U_64), sizeof(UDATA) };

static TR::ILOpCodes J9ToTRDirectCallMap[] =
   { TR::call, TR::icall, TR::icall, TR::icall, TR::icall, TR::fcall, TR::icall, TR::dcall, TR::lcall, TR::acall };

static TR::ILOpCodes J9ToTRIndirectCallMap[] =
   { TR::calli, TR::icalli, TR::icalli, TR::icalli, TR::icalli, TR::fcalli, TR::icalli, TR::dcalli, TR::lcalli, TR::acalli };

static TR::ILOpCodes J9ToTRReturnOpCodeMap[] =
   { TR::Return, TR::ireturn, TR::ireturn, TR::ireturn, TR::ireturn, TR::freturn, TR::ireturn, TR::dreturn, TR::lreturn, TR::areturn };


static J9UTF8 *str2utf8(char *string, int32_t length, TR_Memory *trMemory, TR_AllocationKind allocKind)
   {
   J9UTF8 *utf8 = (J9UTF8 *) trMemory->allocateMemory(length+sizeof(J9UTF8), allocKind); // This allocates more memory than it needs.
   J9UTF8_SET_LENGTH(utf8, length);
   memcpy(J9UTF8_DATA(utf8), string, length);
   return utf8;
   }

static char *utf82str(J9UTF8 *utf8, TR_Memory *trMemory, TR_AllocationKind allocKind)
   {
   uint16_t length = J9UTF8_LENGTH(utf8);
   char *string = (char *) trMemory->allocateMemory(length+1, allocKind);
   memcpy(string, J9UTF8_DATA(utf8), length);
   string[length] = 0;
   return string;
   }

J9::Method::Method(TR_FrontEnd *fe, TR_Memory *trMemory, J9Class *aClazz, uintptr_t cpIndex)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");

   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;

   J9ROMClass * romClass = aClazz->romClass;
   uintptr_t realCPIndex = jitGetRealCPIndex(fej9->vmThread(), romClass, cpIndex);
   J9ROMMethodRef * romRef = &J9ROM_CP_BASE(romClass, J9ROMMethodRef)[realCPIndex];
   J9ROMClassRef * classRef = &J9ROM_CP_BASE(romClass, J9ROMClassRef)[romRef->classRefCPIndex];
   J9ROMNameAndSignature * nameAndSignature = J9ROMMETHODREF_NAMEANDSIGNATURE(romRef);

   _className = J9ROMCLASSREF_NAME(classRef);
   _name = J9ROMNAMEANDSIGNATURE_NAME(nameAndSignature);
   _signature = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSignature);

   parseSignature(trMemory);
   _fullSignature = NULL;
   }

J9::Method::Method(TR_FrontEnd *fe, TR_Memory *trMemory, TR_OpaqueMethodBlock *aMethod)
   {
   J9ROMMethod * romMethod;
   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;

      {
      TR::VMAccessCriticalSection j9method(fej9);
      romMethod = getOriginalROMMethod((J9Method *)aMethod);
      }

   J9ROMClass *romClass = J9_CLASS_FROM_METHOD(((J9Method *)aMethod))->romClass;
   _className = J9ROMCLASS_CLASSNAME(romClass);
   _name = J9ROMMETHOD_NAME(romMethod);
   _signature = J9ROMMETHOD_SIGNATURE(romMethod);

   parseSignature(trMemory);
   _fullSignature = NULL;
   }

TR_MethodParameterIterator *
J9::Method::getParameterIterator(TR::Compilation &comp, TR_ResolvedMethod *r)
   {
   return new (comp.trHeapMemory()) TR_J9MethodParameterIterator(*(self()), comp, r);
   }

void
J9::Method::parseSignature(TR_Memory * trMemory)
   {
   U_8 tempArgTypes[256];
   jitParseSignature(_signature, tempArgTypes, &_paramElements, &_paramSlots);
   _argTypes = (U_8 *) trMemory->allocateHeapMemory((_paramElements + 1) * sizeof(U_8));
   memcpy(_argTypes, tempArgTypes, (_paramElements + 1));
   }

uintptr_t
J9::Method::j9returnType()
   {
   return _argTypes[_paramElements];
   }

uint32_t
J9::Method::numberOfExplicitParameters()
   {
   TR_ASSERT(_paramElements <= 0xFFFFFFFF, "numberOfExplicitParameters() not expected to return 64-bit values\n");
   return (uint32_t) _paramElements;
   }

// Returns the TR type for the return type of the method
//
TR::DataType
J9::Method::returnType()
   {
   return J9ToTRMap[j9returnType()];
   }

bool
J9::Method::returnTypeIsUnsigned()
   {
   return J9ToIsUnsignedMap[j9returnType()];
   }

// Returns the TR type of the parmNumber'th parameter
// NB: 0-based.  ie: (IC)V has parm0=I, parm1=C, etc..
//
TR::DataType
J9::Method::parmType(uint32_t parmNumber)
   {
   return J9ToTRMap[_argTypes[parmNumber]];
   }

// returns the opcode for a direct call version of the method
//
TR::ILOpCodes
J9::Method::directCallOpCode()
   {
   return J9ToTRDirectCallMap[j9returnType()];
   }

// returns the opcode for an indirect call version of the method
//
TR::ILOpCodes
J9::Method::indirectCallOpCode()
   {
   return J9ToTRIndirectCallMap[j9returnType()];
   }

// returns the return opcode for the method
//
TR::ILOpCodes
J9::Method::returnOpCode()
   {
   return J9ToTRReturnOpCodeMap[j9returnType()];
   }

// Returns the width of the return type of the method
//
U_32
J9::Method::returnTypeWidth()
   {
   return J9ToSizeMap[j9returnType()];
   }

// returns the length of the class that this method is in.
//
U_16
J9::Method::classNameLength()
   {
   return J9UTF8_LENGTH(_className);
   }

// returns the length of the method name
//
U_16
J9::Method::nameLength()
   {
   return J9UTF8_LENGTH(_name);
   }

const char *
J9::Method::signature(TR_Memory * trMemory, TR_AllocationKind allocKind)
   {
   if( !_fullSignature )
      {
   char * s = (char *)trMemory->allocateMemory(classNameLength() + nameLength() + signatureLength() + 3, allocKind);
   sprintf(s, "%.*s.%.*s%.*s", classNameLength(), classNameChars(), nameLength(), nameChars(), signatureLength(), signatureChars());

      if ( allocKind == heapAlloc)
        _fullSignature = s;

   return s;
   }
   else
      return _fullSignature;
   }

// returns the length of the method signature
U_16
J9::Method::signatureLength()
   {
   return J9UTF8_LENGTH(_signature);
   }

// returns the char * pointer to the utf8 encoded name of the class the method is in
//
char *
J9::Method::classNameChars()
   {
   return utf8Data(_className);
   }

// returns the char * pointer to the utf8 encoded method name
//
char *
J9::Method::nameChars()
   {
   return utf8Data(_name);
   }

// returns the char * pointer to the utf8 encoded method signature
//
char *
J9::Method::signatureChars()
   {
   return utf8Data(_signature);
   }

bool
J9::Method::isConstructor()
   {
   return nameLength()==6 && !strncmp(nameChars(), "<init>", 6);
   }

bool
J9::Method::isFinalInObject()
   {
   return methodIsFinalInObject(nameLength(), (U_8 *) nameChars(), signatureLength(), (U_8 *) signatureChars()) ? true : false;
   }

void
J9::Method::setSignature(char *newSignature, int32_t newSignatureLength, TR_Memory *trMemory)
   {
   _signature = str2utf8(newSignature, newSignatureLength, trMemory, heapAlloc);
   parseSignature(trMemory);
   _fullSignature = NULL;
   }

void
J9::Method::setArchetypeSpecimen(bool b)
   {
   _flags.set(ArchetypeSpecimen, b);
   }

bool
J9::Method::isUnsafeCAS(TR::Compilation * c)
   {
   TR::RecognizedMethod rm = getRecognizedMethod();
   switch (rm)
      {
      case TR::sun_misc_Unsafe_compareAndSwapInt_jlObjectJII_Z:
      case TR::sun_misc_Unsafe_compareAndSwapLong_jlObjectJJJ_Z:
      case TR::sun_misc_Unsafe_compareAndSwapObject_jlObjectJjlObjectjlObject_Z:
         return true;
      }

   return false;
   }

bool
J9::Method::isUnsafeWithObjectArg(TR::Compilation * c)
   {
   TR::RecognizedMethod rm = getRecognizedMethod();
   switch (rm)
      {
      case TR::sun_misc_Unsafe_putByte_jlObjectJB_V:
      case TR::sun_misc_Unsafe_putBoolean_jlObjectJZ_V:
      case TR::sun_misc_Unsafe_putChar_jlObjectJC_V:
      case TR::sun_misc_Unsafe_putShort_jlObjectJS_V:
      case TR::sun_misc_Unsafe_putInt_jlObjectJI_V:
      case TR::sun_misc_Unsafe_putLong_jlObjectJJ_V:
      case TR::sun_misc_Unsafe_putFloat_jlObjectJF_V:
      case TR::sun_misc_Unsafe_putDouble_jlObjectJD_V:
      case TR::sun_misc_Unsafe_putObject_jlObjectJjlObject_V:
      case TR::sun_misc_Unsafe_getBoolean_jlObjectJ_Z:
      case TR::sun_misc_Unsafe_getByte_jlObjectJ_B:
      case TR::sun_misc_Unsafe_getChar_jlObjectJ_C:
      case TR::sun_misc_Unsafe_getShort_jlObjectJ_S:
      case TR::sun_misc_Unsafe_getInt_jlObjectJ_I:
      case TR::sun_misc_Unsafe_getLong_jlObjectJ_J:
      case TR::sun_misc_Unsafe_getFloat_jlObjectJ_F:
      case TR::sun_misc_Unsafe_getDouble_jlObjectJ_D:
      case TR::sun_misc_Unsafe_getObject_jlObjectJ_jlObject:
      case TR::sun_misc_Unsafe_putByteVolatile_jlObjectJB_V:
      case TR::sun_misc_Unsafe_putBooleanVolatile_jlObjectJZ_V:
      case TR::sun_misc_Unsafe_putCharVolatile_jlObjectJC_V:
      case TR::sun_misc_Unsafe_putShortVolatile_jlObjectJS_V:
      case TR::sun_misc_Unsafe_putIntVolatile_jlObjectJI_V:
      case TR::sun_misc_Unsafe_putLongVolatile_jlObjectJJ_V:
      case TR::sun_misc_Unsafe_putFloatVolatile_jlObjectJF_V:
      case TR::sun_misc_Unsafe_putDoubleVolatile_jlObjectJD_V:
      case TR::sun_misc_Unsafe_putObjectVolatile_jlObjectJjlObject_V:
      case TR::sun_misc_Unsafe_getBooleanVolatile_jlObjectJ_Z:
      case TR::sun_misc_Unsafe_getByteVolatile_jlObjectJ_B:
      case TR::sun_misc_Unsafe_getCharVolatile_jlObjectJ_C:
      case TR::sun_misc_Unsafe_getShortVolatile_jlObjectJ_S:
      case TR::sun_misc_Unsafe_getIntVolatile_jlObjectJ_I:
      case TR::sun_misc_Unsafe_getLongVolatile_jlObjectJ_J:
      case TR::sun_misc_Unsafe_getFloatVolatile_jlObjectJ_F:
      case TR::sun_misc_Unsafe_getDoubleVolatile_jlObjectJ_D:
      case TR::sun_misc_Unsafe_getObjectVolatile_jlObjectJ_jlObject:
      case TR::sun_misc_Unsafe_putByteOrdered_jlObjectJB_V:
      case TR::sun_misc_Unsafe_putBooleanOrdered_jlObjectJZ_V:
      case TR::sun_misc_Unsafe_putCharOrdered_jlObjectJC_V:
      case TR::sun_misc_Unsafe_putShortOrdered_jlObjectJS_V:
      case TR::sun_misc_Unsafe_putIntOrdered_jlObjectJI_V:
      case TR::sun_misc_Unsafe_putLongOrdered_jlObjectJJ_V:
      case TR::sun_misc_Unsafe_putFloatOrdered_jlObjectJF_V:
      case TR::sun_misc_Unsafe_putDoubleOrdered_jlObjectJD_V:
      case TR::sun_misc_Unsafe_putObjectOrdered_jlObjectJjlObject_V:
         return true;
      default:
         return false;
      }

   return false;
   }

bool
J9::Method::isUnsafeGetPutWithObjectArg(TR::RecognizedMethod rm)
   {
   switch (rm)
      {
      case TR::sun_misc_Unsafe_putByte_jlObjectJB_V:
      case TR::sun_misc_Unsafe_putBoolean_jlObjectJZ_V:
      case TR::sun_misc_Unsafe_putChar_jlObjectJC_V:
      case TR::sun_misc_Unsafe_putShort_jlObjectJS_V:
      case TR::sun_misc_Unsafe_putInt_jlObjectJI_V:
      case TR::sun_misc_Unsafe_putLong_jlObjectJJ_V:
      case TR::sun_misc_Unsafe_putFloat_jlObjectJF_V:
      case TR::sun_misc_Unsafe_putDouble_jlObjectJD_V:
      case TR::sun_misc_Unsafe_putObject_jlObjectJjlObject_V:
      case TR::sun_misc_Unsafe_getBoolean_jlObjectJ_Z:
      case TR::sun_misc_Unsafe_getByte_jlObjectJ_B:
      case TR::sun_misc_Unsafe_getChar_jlObjectJ_C:
      case TR::sun_misc_Unsafe_getShort_jlObjectJ_S:
      case TR::sun_misc_Unsafe_getInt_jlObjectJ_I:
      case TR::sun_misc_Unsafe_getLong_jlObjectJ_J:
      case TR::sun_misc_Unsafe_getFloat_jlObjectJ_F:
      case TR::sun_misc_Unsafe_getDouble_jlObjectJ_D:
      case TR::sun_misc_Unsafe_getObject_jlObjectJ_jlObject:
      case TR::sun_misc_Unsafe_putByteVolatile_jlObjectJB_V:
      case TR::sun_misc_Unsafe_putBooleanVolatile_jlObjectJZ_V:
      case TR::sun_misc_Unsafe_putCharVolatile_jlObjectJC_V:
      case TR::sun_misc_Unsafe_putShortVolatile_jlObjectJS_V:
      case TR::sun_misc_Unsafe_putIntVolatile_jlObjectJI_V:
      case TR::sun_misc_Unsafe_putLongVolatile_jlObjectJJ_V:
      case TR::sun_misc_Unsafe_putFloatVolatile_jlObjectJF_V:
      case TR::sun_misc_Unsafe_putDoubleVolatile_jlObjectJD_V:
      case TR::sun_misc_Unsafe_putObjectVolatile_jlObjectJjlObject_V:
      case TR::sun_misc_Unsafe_getBooleanVolatile_jlObjectJ_Z:
      case TR::sun_misc_Unsafe_getByteVolatile_jlObjectJ_B:
      case TR::sun_misc_Unsafe_getCharVolatile_jlObjectJ_C:
      case TR::sun_misc_Unsafe_getShortVolatile_jlObjectJ_S:
      case TR::sun_misc_Unsafe_getIntVolatile_jlObjectJ_I:
      case TR::sun_misc_Unsafe_getLongVolatile_jlObjectJ_J:
      case TR::sun_misc_Unsafe_getFloatVolatile_jlObjectJ_F:
      case TR::sun_misc_Unsafe_getDoubleVolatile_jlObjectJ_D:
      case TR::sun_misc_Unsafe_getObjectVolatile_jlObjectJ_jlObject:
      case TR::sun_misc_Unsafe_putByteOrdered_jlObjectJB_V:
      case TR::sun_misc_Unsafe_putBooleanOrdered_jlObjectJZ_V:
      case TR::sun_misc_Unsafe_putCharOrdered_jlObjectJC_V:
      case TR::sun_misc_Unsafe_putShortOrdered_jlObjectJS_V:
      case TR::sun_misc_Unsafe_putIntOrdered_jlObjectJI_V:
      case TR::sun_misc_Unsafe_putLongOrdered_jlObjectJJ_V:
      case TR::sun_misc_Unsafe_putFloatOrdered_jlObjectJF_V:
      case TR::sun_misc_Unsafe_putDoubleOrdered_jlObjectJD_V:
      case TR::sun_misc_Unsafe_putObjectOrdered_jlObjectJjlObject_V:
         return true;
      default:
         return false;
      }

   return false;
   }

TR::DataType
J9::Method::unsafeDataTypeForObject(TR::RecognizedMethod rm)
   {
   switch (rm)
      {
      case TR::sun_misc_Unsafe_getBoolean_jlObjectJ_Z:
      case TR::sun_misc_Unsafe_putBoolean_jlObjectJZ_V:
      case TR::sun_misc_Unsafe_getByte_jlObjectJ_B:
      case TR::sun_misc_Unsafe_putByte_jlObjectJB_V:
      case TR::sun_misc_Unsafe_getChar_jlObjectJ_C:
      case TR::sun_misc_Unsafe_putChar_jlObjectJC_V:
      case TR::sun_misc_Unsafe_getShort_jlObjectJ_S:
      case TR::sun_misc_Unsafe_putShort_jlObjectJS_V:
      case TR::sun_misc_Unsafe_getInt_jlObjectJ_I:
      case TR::sun_misc_Unsafe_putInt_jlObjectJI_V:
      case TR::sun_misc_Unsafe_getBooleanVolatile_jlObjectJ_Z:
      case TR::sun_misc_Unsafe_putBooleanVolatile_jlObjectJZ_V:
      case TR::sun_misc_Unsafe_getByteVolatile_jlObjectJ_B:
      case TR::sun_misc_Unsafe_putByteVolatile_jlObjectJB_V:
      case TR::sun_misc_Unsafe_getCharVolatile_jlObjectJ_C:
      case TR::sun_misc_Unsafe_putCharVolatile_jlObjectJC_V:
      case TR::sun_misc_Unsafe_getShortVolatile_jlObjectJ_S:
      case TR::sun_misc_Unsafe_putShortVolatile_jlObjectJS_V:
      case TR::sun_misc_Unsafe_getIntVolatile_jlObjectJ_I:
      case TR::sun_misc_Unsafe_putIntVolatile_jlObjectJI_V:
         return TR::Int32;
      case TR::sun_misc_Unsafe_getLong_jlObjectJ_J:
      case TR::sun_misc_Unsafe_putLong_jlObjectJJ_V:
      case TR::sun_misc_Unsafe_getLongVolatile_jlObjectJ_J:
      case TR::sun_misc_Unsafe_putLongVolatile_jlObjectJJ_V:
         return TR::Int64;
      case TR::sun_misc_Unsafe_getFloat_jlObjectJ_F:
      case TR::sun_misc_Unsafe_putFloat_jlObjectJF_V:
      case TR::sun_misc_Unsafe_getFloatVolatile_jlObjectJ_F:
      case TR::sun_misc_Unsafe_putFloatVolatile_jlObjectJF_V:
         return TR::Float;
      case TR::sun_misc_Unsafe_getDouble_jlObjectJ_D:
      case TR::sun_misc_Unsafe_putDouble_jlObjectJD_V:
      case TR::sun_misc_Unsafe_getDoubleVolatile_jlObjectJ_D:
      case TR::sun_misc_Unsafe_putDoubleVolatile_jlObjectJD_V:
         return TR::Double;
      case TR::sun_misc_Unsafe_getObject_jlObjectJ_jlObject:
      case TR::sun_misc_Unsafe_putObject_jlObjectJjlObject_V:
      case TR::sun_misc_Unsafe_getObjectVolatile_jlObjectJ_jlObject:
      case TR::sun_misc_Unsafe_putObjectVolatile_jlObjectJjlObject_V:
         return TR::Address;
      default:
         TR_ASSERT(false, "Method is not supported\n");
      }
   return TR::NoType;
   }

TR::DataType
J9::Method::unsafeDataTypeForArray(TR::RecognizedMethod rm)
   {
   switch (rm)
      {
      case TR::sun_misc_Unsafe_getBoolean_jlObjectJ_Z:
      case TR::sun_misc_Unsafe_putBoolean_jlObjectJZ_V:
      case TR::sun_misc_Unsafe_getByte_jlObjectJ_B:
      case TR::sun_misc_Unsafe_putByte_jlObjectJB_V:
      case TR::sun_misc_Unsafe_getBooleanVolatile_jlObjectJ_Z:
      case TR::sun_misc_Unsafe_putBooleanVolatile_jlObjectJZ_V:
      case TR::sun_misc_Unsafe_getByteVolatile_jlObjectJ_B:
      case TR::sun_misc_Unsafe_putByteVolatile_jlObjectJB_V:
         return TR::Int8;
      case TR::sun_misc_Unsafe_getChar_jlObjectJ_C:
      case TR::sun_misc_Unsafe_putChar_jlObjectJC_V:
      case TR::sun_misc_Unsafe_getShort_jlObjectJ_S:
      case TR::sun_misc_Unsafe_putShort_jlObjectJS_V:
      case TR::sun_misc_Unsafe_getCharVolatile_jlObjectJ_C:
      case TR::sun_misc_Unsafe_putCharVolatile_jlObjectJC_V:
      case TR::sun_misc_Unsafe_getShortVolatile_jlObjectJ_S:
      case TR::sun_misc_Unsafe_putShortVolatile_jlObjectJS_V:
         return TR::Int16;
      case TR::sun_misc_Unsafe_getInt_jlObjectJ_I:
      case TR::sun_misc_Unsafe_putInt_jlObjectJI_V:
      case TR::sun_misc_Unsafe_getIntVolatile_jlObjectJ_I:
      case TR::sun_misc_Unsafe_putIntVolatile_jlObjectJI_V:
         return TR::Int32;
      case TR::sun_misc_Unsafe_getLong_jlObjectJ_J:
      case TR::sun_misc_Unsafe_putLong_jlObjectJJ_V:
      case TR::sun_misc_Unsafe_getLongVolatile_jlObjectJ_J:
      case TR::sun_misc_Unsafe_putLongVolatile_jlObjectJJ_V:
         return TR::Int64;
      case TR::sun_misc_Unsafe_getFloat_jlObjectJ_F:
      case TR::sun_misc_Unsafe_putFloat_jlObjectJF_V:
      case TR::sun_misc_Unsafe_getFloatVolatile_jlObjectJ_F:
      case TR::sun_misc_Unsafe_putFloatVolatile_jlObjectJF_V:
         return TR::Float;
      case TR::sun_misc_Unsafe_getDouble_jlObjectJ_D:
      case TR::sun_misc_Unsafe_putDouble_jlObjectJD_V:
      case TR::sun_misc_Unsafe_getDoubleVolatile_jlObjectJ_D:
      case TR::sun_misc_Unsafe_putDoubleVolatile_jlObjectJD_V:
         return TR::Double;
      case TR::sun_misc_Unsafe_getObject_jlObjectJ_jlObject:
      case TR::sun_misc_Unsafe_putObject_jlObjectJjlObject_V:
      case TR::sun_misc_Unsafe_getObjectVolatile_jlObjectJ_jlObject:
      case TR::sun_misc_Unsafe_putObjectVolatile_jlObjectJjlObject_V:
         return TR::Address;
      default:
         TR_ASSERT(false, "Method is not supported\n");
      }
   return TR::NoType;
   }

bool
J9::Method::isVolatileUnsafe(TR::RecognizedMethod rm)
   {
   switch (rm)
      {
      case TR::sun_misc_Unsafe_getBooleanVolatile_jlObjectJ_Z:
      case TR::sun_misc_Unsafe_getByteVolatile_jlObjectJ_B:
      case TR::sun_misc_Unsafe_getCharVolatile_jlObjectJ_C:
      case TR::sun_misc_Unsafe_getShortVolatile_jlObjectJ_S:
      case TR::sun_misc_Unsafe_getIntVolatile_jlObjectJ_I:
      case TR::sun_misc_Unsafe_getLongVolatile_jlObjectJ_J:
      case TR::sun_misc_Unsafe_getFloatVolatile_jlObjectJ_F:
      case TR::sun_misc_Unsafe_getDoubleVolatile_jlObjectJ_D:
      case TR::sun_misc_Unsafe_getObjectVolatile_jlObjectJ_jlObject:
      case TR::sun_misc_Unsafe_putBooleanVolatile_jlObjectJZ_V:
      case TR::sun_misc_Unsafe_putByteVolatile_jlObjectJB_V:
      case TR::sun_misc_Unsafe_putCharVolatile_jlObjectJC_V:
      case TR::sun_misc_Unsafe_putShortVolatile_jlObjectJS_V:
      case TR::sun_misc_Unsafe_putIntVolatile_jlObjectJI_V:
      case TR::sun_misc_Unsafe_putLongVolatile_jlObjectJJ_V:
      case TR::sun_misc_Unsafe_putFloatVolatile_jlObjectJF_V:
      case TR::sun_misc_Unsafe_putDoubleVolatile_jlObjectJD_V:
      case TR::sun_misc_Unsafe_putObjectVolatile_jlObjectJjlObject_V:
         return true;
      default:
         return false;
      }
   return false;
   }

bool
J9::Method::isUnsafeGetPutBoolean(TR::RecognizedMethod rm)
   {
   switch (rm)
      {
      case TR::sun_misc_Unsafe_getBoolean_jlObjectJ_Z:
      case TR::sun_misc_Unsafe_getBooleanVolatile_jlObjectJ_Z:
      case TR::sun_misc_Unsafe_putBoolean_jlObjectJZ_V:
      case TR::sun_misc_Unsafe_putBooleanVolatile_jlObjectJZ_V:
         return true;
      default:
         break;
      }

   return false;
   }

// Might need to add more unsafe put methods to this list
bool
J9::Method::isUnsafePut(TR::RecognizedMethod rm)
   {
   switch (rm)
      {
      case TR::sun_misc_Unsafe_compareAndSwapInt_jlObjectJII_Z:
      case TR::sun_misc_Unsafe_compareAndSwapLong_jlObjectJJJ_Z:
      case TR::sun_misc_Unsafe_compareAndSwapObject_jlObjectJjlObjectjlObject_Z:
      case TR::sun_misc_Unsafe_getAndAddInt:
      case TR::sun_misc_Unsafe_getAndAddLong:
      case TR::sun_misc_Unsafe_getAndSetInt:
      case TR::sun_misc_Unsafe_getAndSetLong:
      case TR::sun_misc_Unsafe_putAddress_JJ_V:
      case TR::sun_misc_Unsafe_putBooleanOrdered_jlObjectJZ_V:
      case TR::sun_misc_Unsafe_putBooleanVolatile_jlObjectJZ_V:
      case TR::sun_misc_Unsafe_putBoolean_jlObjectJZ_V:
      case TR::sun_misc_Unsafe_putByteOrdered_jlObjectJB_V:
      case TR::sun_misc_Unsafe_putByteVolatile_jlObjectJB_V:
      case TR::sun_misc_Unsafe_putByte_JB_V:
      case TR::sun_misc_Unsafe_putByte_jlObjectJB_V:
      case TR::sun_misc_Unsafe_putCharOrdered_jlObjectJC_V:
      case TR::sun_misc_Unsafe_putCharVolatile_jlObjectJC_V:
      case TR::sun_misc_Unsafe_putChar_JC_V:
      case TR::sun_misc_Unsafe_putChar_jlObjectJC_V:
      case TR::sun_misc_Unsafe_putDoubleOrdered_jlObjectJD_V:
      case TR::sun_misc_Unsafe_putDoubleVolatile_jlObjectJD_V:
      case TR::sun_misc_Unsafe_putDouble_JD_V:
      case TR::sun_misc_Unsafe_putDouble_jlObjectJD_V:
      case TR::sun_misc_Unsafe_putFloatOrdered_jlObjectJF_V:
      case TR::sun_misc_Unsafe_putFloatVolatile_jlObjectJF_V:
      case TR::sun_misc_Unsafe_putFloat_JF_V:
      case TR::sun_misc_Unsafe_putFloat_jlObjectJF_V:
      case TR::sun_misc_Unsafe_putIntOrdered_jlObjectJI_V:
      case TR::sun_misc_Unsafe_putIntVolatile_jlObjectJI_V:
      case TR::sun_misc_Unsafe_putInt_JI_V:
      case TR::sun_misc_Unsafe_putInt_jlObjectII_V:
      case TR::sun_misc_Unsafe_putInt_jlObjectJI_V:
      case TR::sun_misc_Unsafe_putLongOrdered_jlObjectJJ_V:
      case TR::sun_misc_Unsafe_putLongVolatile_jlObjectJJ_V:
      case TR::sun_misc_Unsafe_putLong_JJ_V:
      case TR::sun_misc_Unsafe_putLong_jlObjectJJ_V:
      case TR::sun_misc_Unsafe_putObjectOrdered_jlObjectJjlObject_V:
      case TR::sun_misc_Unsafe_putObjectVolatile_jlObjectJjlObject_V:
      case TR::sun_misc_Unsafe_putObject_jlObjectJjlObject_V:
      case TR::sun_misc_Unsafe_putShortOrdered_jlObjectJS_V:
      case TR::sun_misc_Unsafe_putShortVolatile_jlObjectJS_V:
      case TR::sun_misc_Unsafe_putShort_JS_V:
      case TR::sun_misc_Unsafe_putShort_jlObjectJS_V:
         return true;
      default:
         return false;
      }
   return false;
   }

bool
J9::Method::isVarHandleOperationMethod(TR::RecognizedMethod rm)
   {
   switch (rm)
      {
      case TR::java_lang_invoke_ArrayVarHandle_ArrayVarHandleOperations_OpMethod:
      case TR::java_lang_invoke_StaticFieldVarHandle_StaticFieldVarHandleOperations_OpMethod:
      case TR::java_lang_invoke_InstanceFieldVarHandle_InstanceFieldVarHandleOperations_OpMethod:
      case TR::java_lang_invoke_ByteArrayViewVarHandle_ByteArrayViewVarHandleOperations_OpMethod:
         return true;
      default:
         return false;
      }
   return false;
   }

bool
J9::Method::isVarHandleAccessMethod(TR::Compilation * comp)
   {
   TR::RecognizedMethod rm = getMandatoryRecognizedMethod();
   switch (rm)
      {
      case TR::java_lang_invoke_VarHandle_get:
      case TR::java_lang_invoke_VarHandle_set:
      case TR::java_lang_invoke_VarHandle_getVolatile:
      case TR::java_lang_invoke_VarHandle_setVolatile:
      case TR::java_lang_invoke_VarHandle_getOpaque:
      case TR::java_lang_invoke_VarHandle_setOpaque:
      case TR::java_lang_invoke_VarHandle_getAcquire:
      case TR::java_lang_invoke_VarHandle_setRelease:
      case TR::java_lang_invoke_VarHandle_compareAndSet:
      case TR::java_lang_invoke_VarHandle_compareAndExchange:
      case TR::java_lang_invoke_VarHandle_compareAndExchangeAcquire:
      case TR::java_lang_invoke_VarHandle_compareAndExchangeRelease:
      case TR::java_lang_invoke_VarHandle_weakCompareAndSet:
      case TR::java_lang_invoke_VarHandle_weakCompareAndSetAcquire:
      case TR::java_lang_invoke_VarHandle_weakCompareAndSetRelease:
      case TR::java_lang_invoke_VarHandle_weakCompareAndSetPlain:
      case TR::java_lang_invoke_VarHandle_getAndSet:
      case TR::java_lang_invoke_VarHandle_getAndSetAcquire:
      case TR::java_lang_invoke_VarHandle_getAndSetRelease:
      case TR::java_lang_invoke_VarHandle_getAndAdd:
      case TR::java_lang_invoke_VarHandle_getAndAddAcquire:
      case TR::java_lang_invoke_VarHandle_getAndAddRelease:
      case TR::java_lang_invoke_VarHandle_getAndBitwiseAnd:
      case TR::java_lang_invoke_VarHandle_getAndBitwiseAndAcquire:
      case TR::java_lang_invoke_VarHandle_getAndBitwiseAndRelease:
      case TR::java_lang_invoke_VarHandle_getAndBitwiseOr:
      case TR::java_lang_invoke_VarHandle_getAndBitwiseOrAcquire:
      case TR::java_lang_invoke_VarHandle_getAndBitwiseOrRelease:
      case TR::java_lang_invoke_VarHandle_getAndBitwiseXor:
      case TR::java_lang_invoke_VarHandle_getAndBitwiseXorAcquire:
      case TR::java_lang_invoke_VarHandle_getAndBitwiseXorRelease:
        return true;
      default:
         return false;
      }

   return false;
   }


#define NUMRECBDMETHODS 74
#define BDCLASSLEN 20
#define NUMNAMEFIELDS 2
#define NUMLENFIELDS 2

const char * recognizedBigDecimalMethods [ /* NUMRECBDMETHODS */ ][ NUMNAMEFIELDS ] =
   {
   /* Following BigDecimal methods call DFP stubs which
    * are inlined by codegen/ppc|s390/DFPTreeEvaluator.cpp
    * If there is a DFP stub that doesn't appear listed here
    * is because it's caller is subsumed by other DFP stubs.
    *
    * Layout:  method name, signature
    */
   // DFPIntConstructor
   { "<init>", "(ILjava/math/MathContext;)V" },
   // DFPLongExpConstructor
   { "longConstructor", "(JILjava/math/MathContext;)V" },
   { "charParser", "([CIILjava/math/MathContext;)V" },
   { "bigIntegerConstructor", "(Ljava/math/BigInteger;ILjava/math/MathContext;)V" },
   { "postSetScaleProcessing", "(Ljava/math/BigDecimal;)V"},
   { "valueOf", "(JI)Ljava/math/BigDecimal;" },
   { "valueOf2DFP", "(JILjava/math/BigDecimal;)Ljava/math/BigDecimal;" },
   // DFPScaledAdd
   { "add", "(Ljava/math/BigDecimal;)Ljava/math/BigDecimal;" },
   { "add2DFP", "(Ljava/math/BigDecimal;Ljava/math/BigDecimal;)Ljava/math/BigDecimal;" },
   // DFPAdd
   { "add", "(Ljava/math/BigDecimal;Ljava/math/MathContext;)Ljava/math/BigDecimal;" },
   // DFPScaledSubtract
   { "subtract", "(Ljava/math/BigDecimal;)Ljava/math/BigDecimal;" },
   { "subtract2DFP", "(Ljava/math/BigDecimal;Ljava/math/BigDecimal;)Ljava/math/BigDecimal;" },
   // DFPSubtract
   { "subtract", "(Ljava/math/BigDecimal;Ljava/math/MathContext;)Ljava/math/BigDecimal;" },
   // DFPScaledMultiply
   { "multiply2", "(Ljava/math/BigDecimal;)Ljava/math/BigDecimal;" },
   // DFPMultiply
   { "multiply", "(Ljava/math/BigDecimal;Ljava/math/MathContext;)Ljava/math/BigDecimal;" },
   // DFPScaledDivide
   { "divide", "(Ljava/math/BigDecimal;II)Ljava/math/BigDecimal;" },
   // DFPDivide
   { "divide", "(Ljava/math/BigDecimal;)Ljava/math/BigDecimal;" },
   { "divide", "(Ljava/math/BigDecimal;Ljava/math/MathContext;)Ljava/math/BigDecimal;" },
   // DFPRound
   { "roundDFP", "(II)V" },
   // DFPCompareTo
   { "compareTo2", "(Ljava/math/BigDecimal;)I" },
   // DFPBCDDigits
   { "precision", "()I" },
   { "signum", "()I" },
   { "unscaledValue", "()Ljava/math/BigInteger;" },
   { "toString2", "()Ljava/lang/String;" },
   { "toStringExpDFP", "(I)[C" },
   { "prePaddedStringDFP", "(II)[C" },
   { "DFPToLL", "()V" },
   // DFPExponent
   { "scale", "()I" },
   // DFPSetScale
   { "setScale", "(IZ)Ljava/math/BigDecimal;" },
   { "setScale2", "(IIJLjava/math/BigDecimal;)Ljava/math/BigDecimal;" },
   // DFPHWAvailable
   { "<init>", "(DLjava/math/MathContext;)V"},
   { "multiply", "(Ljava/math/BigDecimal;)Ljava/math/BigDecimal;"},
   { "divide", "(Ljava/math/BigDecimal;Ljava/math/BigDecimal;II)V"},
   { "divide", "(Ljava/math/BigDecimal;Ljava/math/BigDecimal;Ljava/math/MathContext;)V"},
   { "negate", "(Ljava/math/MathContext;)Ljava/math/BigDecimal;"},
   { "byteValueExact", "()B"},
   { "shortValueExact", "()S"},
   { "intValueExact", "()I"},
   { "longValueExact", "()J"},
   { "toBigIntegerExact", "()Ljava/math/BigInteger;"},
   { "toBigInteger", "()Ljava/math/BigInteger;"},
   { "pow", "(ILjava/math/MathContext;)Ljava/math/BigDecimal;"},
   { "movePointLeft", "(I)Ljava/math/BigDecimal;"},
   { "movePointRight", "(I)Ljava/math/BigDecimal;"},
   { "scaleByPowerOfTen", "(I)Ljava/math/BigDecimal;"},
   { "setScale", "(II)Ljava/math/BigDecimal;"},
   { "stripTrailingZeros", "()Ljava/math/BigDecimal;"},
   { "toEngineeringString", "()Ljava/lang/String;"},
   { "toPlainString", "()Ljava/lang/String;"},
   { "round", "(Ljava/math/MathContext;)Ljava/math/BigDecimal;"},
   { "possibleClone", "(Ljava/math/BigDecimal;)Ljava/math/BigDecimal;"},
   // DFPSignificance
   { "precisionDFPHelper", "()I"},
   // Special method for checking hardware
   { "DFPGetHWAvailable", "()Z"},
   // More methods due to tr.r11 refactoring of BigDecimal
   { "DFPAddHelper", "(Ljava/math/BigDecimal;Ljava/math/BigDecimal;Ljava/math/MathContext;)Z" },
   { "DFPAddHelper", "(Ljava/math/BigDecimal;Ljava/math/BigDecimal;)Z" },
   { "DFPBigIntegerConstructorHelper", "(Ljava/math/BigInteger;ILjava/math/MathContext;)Z" },
   { "DFPCharConstructorHelper", "(JIIILjava/math/MathContext;)Z" },
   { "DFPDivideHelper", "(Ljava/math/BigDecimal;Ljava/math/BigDecimal;Ljava/math/BigDecimal;Ljava/math/MathContext;)Z" },
   { "DFPDivideHelper", "(Ljava/math/BigDecimal;Ljava/math/BigDecimal;Ljava/math/BigDecimal;)Z" },
   { "DFPIntConstructorHelper", "(ILjava/math/MathContext;)Z" },
   { "DFPLongConstructorHelper", "(JILjava/math/MathContext;)Z" },
   { "DFPMultiplyHelper", "(Ljava/math/BigDecimal;Ljava/math/BigDecimal;Ljava/math/MathContext;)Z" },
   { "DFPMultiplyHelper", "(Ljava/math/BigDecimal;Ljava/math/BigDecimal;)Z" },
   { "DFPPrecisionHelper", "()I" },
   { "DFPRoundHelper", "(II)V" },
   { "DFPSetScaleHelper", "(Ljava/math/BigDecimal;I)Z" },
   { "DFPSetScaleHelper", "(Ljava/math/BigDecimal;JII)Z" },
   { "DFPSignumHelper", "()I" },
   { "DFPSubtractHelper", "(Ljava/math/BigDecimal;Ljava/math/BigDecimal;Ljava/math/MathContext;)Z" },
   { "DFPSubtractHelper", "(Ljava/math/BigDecimal;Ljava/math/BigDecimal;)Z" },
   { "DFPUnscaledValueHelper", "()Ljava/math/BigInteger;" },
   { "DFPValueOfHelper", "(JILjava/math/BigDecimal;)Ljava/math/BigDecimal;" },
   { "charParser", "([CIILjava/math/MathContext;[C)V" }, // signature has changed from r9 -- now has an extra char array
   { "compareTo", "()V" }, // now calls DFPGetHWAvailable
   };

const int recognizedBigDecimalLengths [ /* NUMRECBDMETHODS */ ][ NUMLENFIELDS ] =
   {
   /* Following BigDecimal methods call DFP stubs which
    * are inlined by codegen/ppc|s390/DFPTreeEvaluator.cpp
    * If there is a DFP stub that doesn't appear listed here
    * is because it's caller is subsumed by other DFP stubs.
    *
    * Layout:  method name length, signature length
    */
   // DFPIntConstructor
   { 6, 27 },
   // DFPLongExpConstructor
   { 15, 28 },
   { 10, 30 },
   { 21, 49 },
   { 22, 25 },
   { 7, 26 },
   { 11, 48 },
   // DFPScaledAdd
   { 3, 46 },
   { 7, 68 },
   // DFPAdd
   { 3, 69 },
   // DFPScaledSubtract
   { 8, 46 },
   { 12, 68 },
   // DFPSubtract
   { 8, 69 },
   // DFPScaledMultiply
   { 9, 46 },
   // DFPMultiply
   { 8, 69 },
   // DFPScaledDivide
   { 6, 48 },
   // DFPDivide
   { 6, 46 },
   { 6, 69 },
   // DFPRound
   { 8, 5 },
   // DFPCompareTo
   { 10, 25 },
   // DFPBCDDigits
   { 9, 3 },
   { 6, 3 },
   { 13, 24 },
   { 9, 20 },
   { 14, 5 },
   { 18, 6 },
   { 7, 3 },
   // DFPExponent
   { 5, 3 },
   // DFPSetScale
   { 8, 26 },
   { 9, 49 },
   // DFPHWAvailable
   { 6, 27},
   { 8, 46},
   { 6, 49},
   { 6, 70},
   { 6, 47},
   { 14, 3},
   { 15, 3},
   { 13, 3},
   { 14, 3},
   { 17, 24},
   { 12, 24},
   { 3, 48},
   { 13, 25},
   { 14, 25},
   { 17, 25},
   { 8, 26},
   { 18, 24},
   { 19, 20},
   { 13, 20},
   { 5, 47},
   { 13, 46},
   // DFPSignificance
   { 18, 3},
   // Special method for checking hardware
   { 17, 3},
   // More methods due to tr.r11 refactoring of BigDecimal
   { 12, 70 },
   { 12, 47 },
   { 30, 49 },
   { 24, 30 },
   { 15, 92 },
   { 15, 69 },
   { 23, 27 },
   { 24, 28 },
   { 17, 70 },
   { 17, 47 },
   { 18, 3 },
   { 14, 5 },
   { 17, 26 },
   { 17, 28 },
   { 15, 3 },
   { 17, 70 },
   { 17, 47 },
   { 22, 24 },
   { 16, 48 },
   { 10, 32 },
   { 9, 3 },
   };

bool
J9::Method::isBigDecimalNameAndSignature(J9UTF8 * name, J9UTF8 * signature)
   {
   TR_ASSERT(NUMRECBDMETHODS == sizeof(recognizedBigDecimalMethods)/sizeof(recognizedBigDecimalMethods[0]), "recognizedBigDecimalMethods array size expected %d; actual %d", NUMRECBDMETHODS, sizeof(recognizedBigDecimalMethods)/sizeof(recognizedBigDecimalMethods[0]));
   TR_ASSERT(NUMRECBDMETHODS == sizeof(recognizedBigDecimalLengths)/sizeof(recognizedBigDecimalLengths[0]), "recognizedBigDecimalLengths array size expected %d; actual %d", NUMRECBDMETHODS, sizeof(recognizedBigDecimalLengths)/sizeof(recognizedBigDecimalLengths[0]));
   for (int i=0; i < NUMRECBDMETHODS; i++)
      {
      if (J9UTF8_LENGTH(name) == recognizedBigDecimalLengths[i][0] &&
          J9UTF8_LENGTH(signature) == recognizedBigDecimalLengths[i][1] &&
          !strncmp(utf8Data(name), recognizedBigDecimalMethods[i][0], recognizedBigDecimalLengths[i][0]) &&
          !strncmp(utf8Data(signature), recognizedBigDecimalMethods[i][1], recognizedBigDecimalLengths[i][1]))
         return true;
      }
   return false;
   }

bool
J9::Method::isBigDecimalMethod(J9ROMMethod * romMethod, J9ROMClass * romClass)
   {
   return TR_J9VMBase::isBigDecimalClass(J9ROMCLASS_CLASSNAME(romClass)) &&
          isBigDecimalNameAndSignature(J9ROMMETHOD_NAME(romMethod), J9ROMMETHOD_SIGNATURE(romMethod));
   }

bool
J9::Method::isBigDecimalMethod(J9Method * j9Method)
   {
   /*J9UTF8 * className;
   J9UTF8 * name;
   J9UTF8 * signature;
   getClassNameSignatureFromMethod(j9Method, className, name, signature);
   return isBigDecimalMethod(className, name, signature);
   */
#if defined(JITSERVER_SUPPORT)
   if (TR::comp()->isOutOfProcessCompilation())
      {
      auto stream = TR::CompilationInfo::getStream();
      stream->write(JITServer::MessageType::ResolvedMethod_isBigDecimalMethod, j9Method);
      return std::get<0>(stream->read<bool>());
      }
#endif /* defined(JITSERVER_SUPPORT) */
   return isBigDecimalMethod(J9_ROM_METHOD_FROM_RAM_METHOD(j9Method), J9_CLASS_FROM_METHOD(j9Method)->romClass);
   }

bool
J9::Method::isBigDecimalMethod(J9UTF8 * className, J9UTF8 * name, J9UTF8 * signature)
   {
   return TR_J9VMBase::isBigDecimalClass(className) && isBigDecimalNameAndSignature(name, signature);
   }

bool
J9::Method::isBigDecimalMethod(TR::Compilation * comp)
   {
   return isBigDecimalMethod(_className, _name, _signature);
   }

typedef struct RecognizedBigDecimalMethods {
   const char * methodName;
   int32_t methodNameLen;
   const char * methodSig;
   int32_t methodSigLen;
} RecognizedBigDecimalExtensionMethods;

#define NUM_RECOGNIZED_BIG_DECIMAL_CONVERTERS_METHODS 2
RecognizedBigDecimalMethods bdConvertersMethods [NUM_RECOGNIZED_BIG_DECIMAL_CONVERTERS_METHODS] =
   {
      {"signedPackedDecimalToBigDecimal", 31, "(B[I)Ljava/math/BigDecimal;", 27},
      {"BigDecimalToSignedPackedDecimal", 31, "(Ljava/math/BigDecimal;)B[", 26}
   };

bool
J9::Method::isBigDecimalConvertersNameAndSignature(J9UTF8 * name, J9UTF8 * signature)
   {
   for (int i=0; i < NUM_RECOGNIZED_BIG_DECIMAL_CONVERTERS_METHODS; i++)
      {
      if (J9UTF8_LENGTH(name) == bdConvertersMethods[i].methodNameLen &&
          J9UTF8_LENGTH(signature) == bdConvertersMethods[i].methodSigLen &&
          !strncmp(utf8Data(name), bdConvertersMethods[i].methodName, J9UTF8_LENGTH(name)) &&
          !strncmp(utf8Data(signature), bdConvertersMethods[i].methodSig, J9UTF8_LENGTH(signature)))
         {
         return true;
         }
      }
   return false;
   }

bool
J9::Method::isBigDecimalConvertersMethod(J9ROMMethod * romMethod, J9ROMClass * romClass)
   {
   return TR_J9VMBase::isBigDecimalConvertersClass(J9ROMCLASS_CLASSNAME(romClass)) &&
          isBigDecimalConvertersNameAndSignature(J9ROMMETHOD_NAME(romMethod), J9ROMMETHOD_SIGNATURE(romMethod));
   }

bool
J9::Method::isBigDecimalConvertersMethod(J9Method * j9Method)
   {
#if defined(JITSERVER_SUPPORT)
   if (TR::comp()->isOutOfProcessCompilation())
      {
      auto stream = TR::CompilationInfo::getStream();
      stream->write(JITServer::MessageType::ResolvedMethod_isBigDecimalConvertersMethod, j9Method);
      return std::get<0>(stream->read<bool>());
      }
#endif /* defined(JITSERVER_SUPPORT) */
   return isBigDecimalConvertersMethod(J9_ROM_METHOD_FROM_RAM_METHOD(j9Method), J9_CLASS_FROM_METHOD(j9Method)->romClass);
   }

bool
J9::Method::isBigDecimalConvertersMethod(J9UTF8 * className, J9UTF8 * name, J9UTF8 * signature)
   {
   return TR_J9VMBase::isBigDecimalConvertersClass(className) && isBigDecimalConvertersNameAndSignature(name, signature);
   }

bool
J9::Method::isBigDecimalConvertersMethod(TR::Compilation * comp)
   {
   return isBigDecimalConvertersMethod(_className, _name, _signature);
   }

