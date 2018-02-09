/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#include "trj9/env/j9method.h"

#include <stddef.h>
#include "bcnames.h"
#include "fastJNI.h"
#include "j9.h"
#include "j9cfg.h"
#include "jitprotos.h"
#include "jilconsts.h"
#include "util_api.h"
#include "rommeth.h"
#include "codegen/CodeGenerator.hpp"
#include "env/KnownObjectTable.hpp"
#include "compile/AOTClassInfo.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/CompilerEnv.hpp"
#include "env/PersistentCHTable.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/jittypes.h"
#include "env/VMAccessCriticalSection.hpp"
#include "env/VMJ9.h"
#include "exceptions/DataCacheError.hpp"
#include "il/DataTypes.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "infra/SimpleRegex.hpp"
#include "optimizer/Inliner.hpp"
#include "optimizer/PreExistence.hpp"
#include "runtime/MethodMetaData.h"
#include "runtime/RelocationRuntime.hpp"
#include "runtime/Runtime.hpp"
#include "runtime/RuntimeAssumptions.hpp"
#include "trj9/env/VMJ9.h"
#include "trj9/control/CompilationRuntime.hpp"
#include "trj9/control/CompilationThread.hpp"
#include "trj9/ilgen/J9ByteCodeIlGenerator.hpp"
#include "trj9/runtime/IProfiler.hpp"
#include "ras/DebugCounter.hpp"

#if defined(_MSC_VER)
#include <malloc.h>
#endif

#define J9VMBYTECODES

#define JSR292_MethodHandle         "java/lang/invoke/MethodHandle"
#define JSR292_invokeExact          "invokeExact"
#define JSR292_invokeExactSig       "([Ljava/lang/Object;)Ljava/lang/Object;"
#define JSR292_ArgumentMoverHandle  "java/lang/invoke/ArgumentMoverHandle"


static TR::DataType J9ToTRMap[] =
   { TR::NoType, TR::Int8, TR::Int8, TR::Int16, TR::Int16, TR::Float, TR::Int32, TR::Double, TR::Int64, TR::Address, TR::Address };
static bool         J9ToIsUnsignedMap[] =
   { false,     true,    false,   true,     false,    false,    false,    false,     false,    false,      false };
static U_32 J9ToSizeMap[] =
   { 0, sizeof(UDATA), sizeof(UDATA), sizeof(UDATA), sizeof(UDATA), sizeof(UDATA), sizeof(UDATA), sizeof(U_64), sizeof(U_64), sizeof(UDATA) };
static TR::ILOpCodes J9ToTRDirectCallMap[] =
   { TR::call, TR::icall, TR::icall, TR::icall, TR::icall, TR::fcall, TR::icall, TR::dcall, TR::lcall, TR::acall };
static TR::ILOpCodes J9ToTRIndirectCallMap[] =
   { TR::calli, TR::icalli, TR::icalli, TR::icalli, TR::icalli, TR::fcalli, TR::icalli, TR::dcalli, TR::lcalli, TR::acalli };
static TR::ILOpCodes J9ToTRReturnOpCodeMap[] =
   { TR::Return, TR::ireturn, TR::ireturn, TR::ireturn, TR::ireturn, TR::freturn, TR::ireturn, TR::dreturn, TR::lreturn, TR::areturn };

bool storeValidationRecordIfNecessary(TR::Compilation * comp, J9ConstantPool *constantPool, int32_t cpIndex, TR_ExternalRelocationTargetKind reloKind, J9Method *ramMethod, J9Class *definingClass=0);


#if defined(DEBUG_LOCAL_CLASS_OPT)
int fieldAttributeCtr = 0;
int staticAttributeCtr = 0;
int resolvedCtr = 0;
int unresolvedCtr = 0;
#endif

#ifdef DEBUG
   #define INCREMENT_COUNTER(vm, slotName) (vm)->_jitConfig->slotName++
#else
   #define INCREMENT_COUNTER(vm, slotName)
#endif

#ifndef NO_OPT_DETAILS
#undef performTransformation
#define performTransformation(comp, a,b)                \
      (comp->getOption(TR_TraceOptDetails) ? comp->getDebug()->performTransformationImpl(true,  (a), (b)) : true)
#endif

static inline bool utf8Matches(struct J9UTF8 * name1, struct J9UTF8 * name2)
   {
   /* optimize if name1 and name2 are identical */
   if (name1 == name2)
      return true;

   /* check lengths before expensive memcmp */
   if (J9UTF8_LENGTH(name1) == J9UTF8_LENGTH(name2) && memcmp(utf8Data(name1), utf8Data(name2), J9UTF8_LENGTH(name1)) == 0)
      return true;

   return false;
   }

static SYS_FLOAT * orderDouble(TR_Memory * m, SYS_FLOAT * result)
   {
   return result;
   }

// takes a J9FieldType and returns the DataTypes associated
// along with the size.
//
static TR::DataType decodeType(U_32 inType)
   {
   // the J9FieldTypes are bits, so we must case on them.

   switch (inType & (J9FieldTypeMask | J9FieldFlagObject))
      {
      case J9FieldTypeLong:    { return TR::Int64; }
      case J9FieldTypeDouble:  { return TR::Double; }
      case J9FieldTypeByte:    { return TR::Int8; }
      case J9FieldTypeFloat:   { return TR::Float; }
      case J9FieldTypeChar:    { return TR::Int16; }
      case J9FieldTypeBoolean: { return TR::Int8; }
      case J9FieldTypeShort:   { return TR::Int16; }
      case J9FieldTypeInt:     { return TR::Int32; }
      case J9FieldFlagObject:  { return TR::Address; }
      default:
         {
         TR_ASSERT(0,"Bad typeCode in JIL_J9VM decodeType");
         return TR::NoType;
         }
      }
   }

static inline UDATA getFieldType(J9ROMConstantPoolItem * cp, I_32 cpIndex)
   {
   J9ROMFieldRef * ref = (J9ROMFieldRef *) (&cp[cpIndex]);
   J9ROMNameAndSignature * nameAndSignature = J9ROMFIELDREF_NAMEANDSIGNATURE(ref);
   J9UTF8 * data = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSignature);
   UDATA j9FieldType = (UDATA)(J9UTF8_DATA(data)[0]);

   switch (j9FieldType)
      {    // cannot use characters because on s390 the compiler will generate an EBCDIC value
           // internal JVM representation is in ASCII
      case J9_SIGBYTE_BYTE:// byte 'B'
        return (j9FieldType |= J9FieldTypeByte);
      case J9_SIGBYTE_CHAR:// char 'C'
        return (j9FieldType |= 0);
      case J9_SIGBYTE_DOUBLE:// double 'D'
        return (j9FieldType |= (J9FieldTypeDouble | J9FieldSizeDouble ));
      case J9_SIGBYTE_FLOAT:// float 'F'
        return (j9FieldType |= J9FieldTypeFloat);
      case J9_SIGBYTE_INT:// int 'I'
        return (j9FieldType |= J9FieldTypeInt);
      case J9_SIGBYTE_LONG:// long 'J'
        return (j9FieldType |=  (J9FieldTypeLong | J9FieldSizeDouble ));
      case J9_SIGBYTE_SHORT:// short 'S'
        return (j9FieldType |= J9FieldTypeShort);
      case J9_SIGBYTE_BOOLEAN:// boolean 'Z'
        return (j9FieldType |= J9FieldTypeBoolean);
      default:// object
        return (j9FieldType |= J9FieldFlagObject);
      }

   }

bool
TR_J9VMBase::supportsFastNanoTime()
{
   static char *disableInlineNanoTime = feGetEnv("TR_disableInlineNanoTime");
   if (disableInlineNanoTime)
      return false;
   else
      return true;
}

/*
 * VM sets a bit in the j9method->constantPool field to indicate 
 * if the method is breakpointed 
 */
U_32
TR_J9VMBase::offsetOfMethodIsBreakpointedBit()
   {
   return J9_STARTPC_METHOD_BREAKPOINTED;
   }

TR_Method *
TR_J9VMBase::createMethod(TR_Memory * trMemory, TR_OpaqueClassBlock * clazz, int32_t refOffset)
   {
   return new (trMemory->trHeapMemory()) TR_J9Method(this, trMemory, TR::Compiler->cls.convertClassOffsetToClassPtr(clazz), refOffset);
   }

TR_ResolvedMethod *
TR_J9VMBase::createResolvedMethod(TR_Memory * trMemory, TR_OpaqueMethodBlock * aMethod,
                                  TR_ResolvedMethod * owningMethod, TR_OpaqueClassBlock *classForNewInstance)
   {
   return createResolvedMethodWithSignature(trMemory, aMethod, classForNewInstance, NULL, -1, owningMethod);
   }

TR_ResolvedMethod *
TR_J9VMBase::createResolvedMethodWithSignature(TR_Memory * trMemory, TR_OpaqueMethodBlock * aMethod, TR_OpaqueClassBlock *classForNewInstance,
                          char *signature, int32_t signatureLength, TR_ResolvedMethod * owningMethod)
   {
   TR_ResolvedJ9Method *result = NULL;
   if (isAOT_DEPRECATED_DO_NOT_USE())
      {
#if defined(J9VM_INTERP_AOT_COMPILE_SUPPORT)
#if defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))
      if (TR::Options::sharedClassCache())
         {
         result = new (trMemory->trHeapMemory()) TR_ResolvedRelocatableJ9Method(aMethod, this, trMemory, owningMethod);
         }
      else
#endif
         return NULL;
#endif
      }
   else
      {
      result = new (trMemory->trHeapMemory()) TR_ResolvedJ9Method(aMethod, this, trMemory, owningMethod);
      if (classForNewInstance)
         {
         result->setClassForNewInstance((J9Class*)classForNewInstance);
         TR_ASSERT(result->isNewInstanceImplThunk(), "createResolvedMethodWithSignature: if classForNewInstance is given this must be a thunk");
         }
      }

   if (signature)
      result->setSignature(signature, signatureLength, trMemory);
   return result;
   }

static J9UTF8 *str2utf8(char *string, int32_t length, TR_Memory *trMemory, TR_AllocationKind allocKind)
   {
   J9UTF8 *utf8 = (J9UTF8 *) trMemory->allocateMemory(length+sizeof(J9UTF8), allocKind);
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

static bool isMethodInValidLibrary(TR_FrontEnd *fe, TR_ResolvedJ9Method *method)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;
   if (fej9->isClassLibraryMethod(method->getPersistentIdentifier(), true))
      return true;

   // this is a work-around because DAA library is not part of class library yet.
   // TODO: to be cleaned up after DAA library packaged into class library
   if (!strncmp(method->convertToMethod()->classNameChars(), "com/ibm/dataaccess/", 19))
      return true;
   // For WebSphere methods
   if (!strncmp(method->convertToMethod()->classNameChars(), "com/ibm/ws/", 11))
      return true;
   if (!strncmp(method->convertToMethod()->classNameChars(), "com/ibm/gpu/Kernel", 18))
      return true;

#ifdef J9VM_OPT_JAVA_CRYPTO_ACCELERATION
   // For IBMJCE Crypto
   if (!strncmp(method->convertToMethod()->classNameChars(), "com/ibm/jit/Crypto", 18))
      return true;
   if (!strncmp(method->convertToMethod()->classNameChars(), "com/ibm/jit/crypto/JITFullHardwareCrypt", 39))
      return true;
   if (!strncmp(method->convertToMethod()->classNameChars(), "com/ibm/jit/crypto/JITFullHardwareDigest", 40))
      return true;
   if (!strncmp(method->convertToMethod()->classNameChars(), "com/ibm/crypto/provider/P224PrimeField", 38))
      return true;
   if (!strncmp(method->convertToMethod()->classNameChars(), "com/ibm/crypto/provider/P256PrimeField", 38))
      return true;
   if (!strncmp(method->convertToMethod()->classNameChars(), "com/ibm/crypto/provider/P384PrimeField", 38))
      return true;
   if (!strncmp(method->convertToMethod()->classNameChars(), "com/ibm/crypto/provider/AESCryptInHardware", 42))
      return true;
   if (!strncmp(method->convertToMethod()->classNameChars(), "com/ibm/crypto/provider/ByteArrayMarshaller", 43))
      return true;
   if (!strncmp(method->convertToMethod()->classNameChars(), "com/ibm/crypto/provider/ByteArrayUnmarshaller", 45))
      return true;
#endif

   return false;
   }

void
TR_J9MethodBase::parseSignature(TR_Memory * trMemory)
   {
   U_8 tempArgTypes[256];
   jitParseSignature(_signature, tempArgTypes, &_paramElements, &_paramSlots);
   _argTypes = (U_8 *) trMemory->allocateHeapMemory((_paramElements + 1) * sizeof(U_8));
   memcpy(_argTypes, tempArgTypes, (_paramElements + 1));
   }

uintptr_t
TR_J9MethodBase::j9returnType()
   {
   return _argTypes[_paramElements];
   }

uint32_t
TR_J9MethodBase::numberOfExplicitParameters()
   {
   TR_ASSERT(_paramElements <= 0xFFFFFFFF, "numberOfExplicitParameters() not expected to return 64-bit values\n");
   return (uint32_t) _paramElements;
   }

// Returns the TR type for the return type of the method
//
TR::DataType
TR_J9MethodBase::returnType()
   {
   return J9ToTRMap[j9returnType()];
   }

bool
TR_J9MethodBase::returnTypeIsUnsigned()
   {
   return J9ToIsUnsignedMap[j9returnType()];
   }

// Returns the TR type of the parmNumber'th parameter
// NB: 0-based.  ie: (IC)V has parm0=I, parm1=C, etc..
//
TR::DataType
TR_J9MethodBase::parmType(uint32_t parmNumber)
   {
   return J9ToTRMap[_argTypes[parmNumber]];
   }

// returns the opcode for a direct call version of the method
//
TR::ILOpCodes
TR_J9MethodBase::directCallOpCode()
   {
   return J9ToTRDirectCallMap[j9returnType()];
   }

// returns the opcode for an indirect call version of the method
//
TR::ILOpCodes
TR_J9MethodBase::indirectCallOpCode()
   {
   return J9ToTRIndirectCallMap[j9returnType()];
   }

// returns the return opcode for the method
//
TR::ILOpCodes
TR_J9MethodBase::returnOpCode()
   {
   return J9ToTRReturnOpCodeMap[j9returnType()];
   }

// Returns the width of the return type of the method
//
U_32
TR_J9MethodBase::returnTypeWidth()
   {
   return J9ToSizeMap[j9returnType()];
   }

// returns the length of the class that this method is in.
//
U_16
TR_J9MethodBase::classNameLength()
   {
   return J9UTF8_LENGTH(_className);
   }

// returns the length of the method name
//
U_16
TR_J9MethodBase::nameLength()
   {
   return J9UTF8_LENGTH(_name);
   }

const char *
TR_J9MethodBase::signature(TR_Memory * trMemory, TR_AllocationKind allocKind)
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
TR_J9MethodBase::signatureLength()
   {
   return J9UTF8_LENGTH(_signature);
   }

// returns the char * pointer to the utf8 encoded name of the class the method is in
//
char *
TR_J9MethodBase::classNameChars()
   {
   return utf8Data(_className);
   }

// returns the char * pointer to the utf8 encoded method name
//
char *
TR_J9MethodBase::nameChars()
   {
   return utf8Data(_name);
   }

// returns the char * pointer to the utf8 encoded method signature
//
char *
TR_J9MethodBase::signatureChars()
   {
   return utf8Data(_signature);
   }

bool
TR_J9MethodBase::isConstructor()
   {
   return nameLength()==6 && !strncmp(nameChars(), "<init>", 6);
   }

bool
TR_J9MethodBase::isFinalInObject()
   {
   return methodIsFinalInObject(nameLength(), (U_8 *) nameChars(), signatureLength(), (U_8 *) signatureChars()) ? true : false;
   }

TR_ResolvedJ9MethodBase::TR_ResolvedJ9MethodBase(TR_FrontEnd * fe, TR_ResolvedMethod * owner)
   : _fe((TR_J9VMBase *)fe), _owningMethod(owner)
   {
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
TR_J9MethodBase::isBigDecimalNameAndSignature(J9UTF8 * name, J9UTF8 * signature)
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
TR_J9MethodBase::isBigDecimalMethod(J9ROMMethod * romMethod, J9ROMClass * romClass)
   {
   return TR_J9VMBase::isBigDecimalClass(J9ROMCLASS_CLASSNAME(romClass)) &&
          isBigDecimalNameAndSignature(J9ROMMETHOD_GET_NAME(romClass, romMethod), J9ROMMETHOD_GET_SIGNATURE(romClass, romMethod));
   }

bool
TR_J9MethodBase::isBigDecimalMethod(J9Method * j9Method)
   {
   /*J9UTF8 * className;
   J9UTF8 * name;
   J9UTF8 * signature;
   getClassNameSignatureFromMethod(j9Method, className, name, signature);
   return isBigDecimalMethod(className, name, signature);
   */

   return isBigDecimalMethod(J9_ROM_METHOD_FROM_RAM_METHOD(j9Method), J9_CLASS_FROM_METHOD(j9Method)->romClass);
   }

bool
TR_J9MethodBase::isBigDecimalMethod(J9UTF8 * className, J9UTF8 * name, J9UTF8 * signature)
   {
   return TR_J9VMBase::isBigDecimalClass(className) && isBigDecimalNameAndSignature(name, signature);
   }

bool
TR_J9MethodBase::isBigDecimalMethod(TR::Compilation * comp)
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
TR_J9MethodBase::isBigDecimalConvertersNameAndSignature(J9UTF8 * name, J9UTF8 * signature)
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
TR_J9MethodBase::isBigDecimalConvertersMethod(J9ROMMethod * romMethod, J9ROMClass * romClass)
   {
   return TR_J9VMBase::isBigDecimalConvertersClass(J9ROMCLASS_CLASSNAME(romClass)) &&
          isBigDecimalConvertersNameAndSignature(J9ROMMETHOD_GET_NAME(romClass, romMethod), J9ROMMETHOD_GET_SIGNATURE(romClass, romMethod));
   }

bool
TR_J9MethodBase::isBigDecimalConvertersMethod(J9Method * j9Method)
   {
   return isBigDecimalConvertersMethod(J9_ROM_METHOD_FROM_RAM_METHOD(j9Method), J9_CLASS_FROM_METHOD(j9Method)->romClass);
   }

bool
TR_J9MethodBase::isBigDecimalConvertersMethod(J9UTF8 * className, J9UTF8 * name, J9UTF8 * signature)
   {
   return TR_J9VMBase::isBigDecimalConvertersClass(className) && isBigDecimalConvertersNameAndSignature(name, signature);
   }

bool
TR_J9MethodBase::isBigDecimalConvertersMethod(TR::Compilation * comp)
   {
   return isBigDecimalConvertersMethod(_className, _name, _signature);
   }


uintptr_t
TR_J9MethodBase::osrFrameSize(J9Method* j9Method)
   {
   return ::osrFrameSize(j9Method);
   }


J9ROMConstantPoolItem *
TR_ResolvedJ9MethodBase::romLiterals()
   {
   return _romLiterals;
   }

TR_J9VMBase *
TR_ResolvedJ9MethodBase::fej9()
   {
   return (TR_J9VMBase *)_fe;
   }

TR_FrontEnd *
TR_ResolvedJ9MethodBase::fe()
   {
   return _fe;
   }

TR_ResolvedMethod *
TR_ResolvedJ9MethodBase::owningMethod()
   {
   return _owningMethod;
   }

void
TR_ResolvedJ9MethodBase::setOwningMethod(TR_ResolvedMethod *parent)
   {
   _owningMethod=parent;
   }

bool
TR_ResolvedJ9MethodBase::owningMethodDoesntMatter()
   {
   // Returning true here allows us to ignore the owning method, which lets us
   // share symrefs more aggressively and other goodies, but usually ignoring
   // the owning method will confuse inliner and others, so only do so when
   // it's known not to matter.

   static char *aggressiveJSR292Opts = feGetEnv("TR_aggressiveJSR292Opts");
   J9UTF8 *className = J9ROMCLASS_CLASSNAME(((J9Class*)containingClass())->romClass);
   if (aggressiveJSR292Opts && strchr(aggressiveJSR292Opts, '3'))
      {
      if (J9UTF8_LENGTH(className) >= 17 && !strncmp((char*)J9UTF8_DATA(className), "java/lang/invoke/", 17))
         {
         return true;
         }
      else switch (getRecognizedMethod())
         {
         case TR::java_lang_invoke_MethodHandle_invokeExactTargetAddress: // This is just a getter that's practically always inlined
            return true;
         default:
         	break;
         }
      }
   else if (!strncmp((char*)J9UTF8_DATA(className), "java/lang/invoke/ILGenMacros", J9UTF8_LENGTH(className)))
      {
      // ILGen macros are always expanded into other trees, so the owning
      // method of the macro's method symbol doesn't matter.
      //
      return true;
      }

   // Safe default
   //
   return false;
   }

bool
TR_ResolvedJ9MethodBase::canAlwaysShareSymbolDespiteOwningMethod(TR_ResolvedMethod *otherMethod)
   {
   if (!owningMethodDoesntMatter())
      return false;

   TR_ResolvedJ9MethodBase *other = (TR_ResolvedJ9MethodBase*)otherMethod;

   if (other == NULL)
      return false; // possibly unresolved
   if (_methodHandleLocation != other->_methodHandleLocation)
      return false;

   TR_J9MethodBase *MB      = asJ9Method();
   TR_J9MethodBase *otherMB = other->asJ9Method();

   // A couple of fast checks
   if (MB->classNameLength() != otherMB->classNameLength())
      return false;
   if (MB->nameLength() != otherMB->nameLength())
      return false;
   if (MB->signatureLength() != otherMB->signatureLength())
      return false;

   //
   // Now we start to get into the more expensive stuff
   //
   if (strncmp(MB->classNameChars(), otherMB->classNameChars(), MB->classNameLength()))
      return false;
   if (strncmp(MB->nameChars(), otherMB->nameChars(), MB->nameLength()))
      return false;
   if (strncmp(MB->signatureChars(), otherMB->signatureChars(), MB->signatureLength()))
      return false;

   return true;
   }

J9ROMFieldRef *
TR_ResolvedJ9MethodBase::romCPBase()
   {
   return (J9ROMFieldRef *) romLiterals();
   }

char *
TR_ResolvedJ9MethodBase::classSignatureOfFieldOrStatic(I_32 cpIndex, int32_t & len)
   {
   if (cpIndex == -1)
      return 0;

   J9ROMFieldRef * ref = (J9ROMFieldRef *) (&romCPBase()[cpIndex]);
   J9ROMNameAndSignature * nameAndSignature = J9ROMFIELDREF_NAMEANDSIGNATURE(ref);
   J9UTF8 * signatureName = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSignature);
   len = J9UTF8_LENGTH(signatureName);
   return (char *)J9UTF8_DATA(signatureName);
   }

char *
TR_ResolvedJ9MethodBase::classNameOfFieldOrStatic(I_32 cpIndex, int32_t & len)
   {
   if (cpIndex == -1)
      return 0;

   J9ROMFieldRef * ref = (J9ROMFieldRef *) (&romCPBase()[cpIndex]);
   J9UTF8 * declName = J9ROMCLASSREF_NAME((J9ROMClassRef *) (&romCPBase()[ref->classRefCPIndex]));
   len = J9UTF8_LENGTH(declName);
   return (char *)J9UTF8_DATA(declName);
   }

int32_t
TR_ResolvedJ9MethodBase::classCPIndexOfFieldOrStatic(int32_t cpIndex)
   {
   if (cpIndex == -1)
      return -1;

   return ((J9ROMFieldRef *)(&romCPBase()[cpIndex]))->classRefCPIndex;
   }

TR_OpaqueClassBlock *
TR_ResolvedJ9MethodBase::getDeclaringClassFromFieldOrStatic(TR::Compilation *comp, int32_t cpIndex)
   {
   J9Class *cpClass = (J9Class*)getClassFromFieldOrStatic(comp, cpIndex);
   if (!cpClass)
      return NULL;

   J9Class *containingClass = NULL;

      {
      TR::VMAccessCriticalSection getDeclaringClassFromFieldOrStatic(fej9());
      J9VMThread *vmThread = fej9()->vmThread();
      int32_t fieldLen; char *field = fieldNameChars      (cpIndex, fieldLen);
      int32_t sigLen;   char *sig   = fieldSignatureChars (cpIndex, sigLen);
      vmThread->javaVM->internalVMFunctions->instanceFieldOffset(vmThread,
                                                                 cpClass,
                                                                 (U_8*)field,
                                                                 fieldLen,
                                                                 (U_8*)sig,
                                                                 sigLen,
                                                                 &containingClass,
                                                                 NULL,
                                                                 J9_RESOLVE_FLAG_JIT_COMPILE_TIME | J9_RESOLVE_FLAG_NO_THROW_ON_FAIL);
      }

   return (TR_OpaqueClassBlock*)containingClass;
   }

void
TR_J9MethodBase::setSignature(char *newSignature, int32_t newSignatureLength, TR_Memory *trMemory)
   {
   _signature = str2utf8(newSignature, newSignatureLength, trMemory, heapAlloc);
   parseSignature(trMemory);
   _fullSignature = NULL;
   }


void
TR_J9MethodBase::setArchetypeSpecimen(bool b)
   {
   _flags.set(ArchetypeSpecimen, b);
   }

char *
TR_ResolvedJ9Method::localName(U_32 slotNumber, U_32 bcIndex, TR_Memory *trMemory)
   {
   I_32 len;
   return localName(slotNumber, bcIndex, len, trMemory);
   }

char *
TR_ResolvedJ9Method::localName(U_32 slotNumber, U_32 bcIndex, I_32 &len, TR_Memory *trMemory)
   {
   J9MethodDebugInfo *methodDebugInfo = getMethodDebugInfoForROMClass(fej9()->getJ9JITConfig()->javaVM, ramMethod());
   if (!methodDebugInfo)
      return NULL;

   J9VariableInfoWalkState state;
   J9VariableInfoValues *values = variableInfoStartDo(methodDebugInfo, &state);
   while (NULL != values)
      {
      // TODO: callers should find out bcIndex so we don't need to ignore it
      if (values->slotNumber == slotNumber
         //&& variableInfo.startVisibility <= bcIndex && bcIndex <= (variableInfo.startVisibility + variableInfo.visibilityLength)
         ){
         J9UTF8 *varName = values->name;
         len = J9UTF8_LENGTH(varName);
         return (char*)J9UTF8_DATA(varName);
         }
      values = variableInfoNextDo(&state);
      }

   return NULL;
   }

char *
TR_ResolvedJ9MethodBase::fieldOrStaticName(I_32 cpIndex, int32_t & len, TR_Memory * trMemory, TR_AllocationKind kind)
   {
   if (cpIndex == -1)
      return "<internal name>";

   J9ROMFieldRef * ref = (J9ROMFieldRef *) (&romCPBase()[cpIndex]);
   J9ROMNameAndSignature * nameAndSignature = J9ROMFIELDREF_NAMEANDSIGNATURE(ref);
   J9UTF8 * declName = J9ROMCLASSREF_NAME((J9ROMClassRef *) (&romCPBase()[ref->classRefCPIndex]));
   len = J9UTF8_LENGTH(declName) + J9UTF8_LENGTH(J9ROMNAMEANDSIGNATURE_NAME(nameAndSignature)) + J9UTF8_LENGTH(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSignature)) +3;

   char * s = (char *)trMemory->allocateMemory(len, kind);
   sprintf(s, "%.*s.%.*s %.*s",
           J9UTF8_LENGTH(declName), utf8Data(declName),
           J9UTF8_LENGTH(J9ROMNAMEANDSIGNATURE_NAME(nameAndSignature)), utf8Data(J9ROMNAMEANDSIGNATURE_NAME(nameAndSignature)),
           J9UTF8_LENGTH(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSignature)), utf8Data(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSignature)));
   return s;
   }

char *
TR_ResolvedJ9MethodBase::fieldName(I_32 cpIndex, TR_Memory * m, TR_AllocationKind kind)
   {
   int32_t len;
   return fieldName(cpIndex, len, m, kind);
   }

char *
TR_ResolvedJ9MethodBase::staticName(I_32 cpIndex, TR_Memory * m, TR_AllocationKind kind)
   {
   int32_t len;
   return staticName(cpIndex, len, m, kind);
   }

char *
TR_ResolvedJ9MethodBase::fieldName(I_32 cpIndex, int32_t & len, TR_Memory * m, TR_AllocationKind kind)
   {
   if (cpIndex < 0) return "<internal field>";
   return fieldOrStaticName(cpIndex, len, m, kind);
   }

char *
TR_ResolvedJ9MethodBase::staticName(I_32 cpIndex, int32_t & len, TR_Memory * m, TR_AllocationKind kind)
   {
   if (cpIndex < 0) return 0;
   return fieldOrStaticName(cpIndex, len, m, kind);
   }

I_32
TR_ResolvedJ9MethodBase::exceptionData(
   J9ExceptionHandler * eh, I_32 bcOffset, I_32 exceptionNumber, I_32 * startIndex, I_32 * endIndex, I_32 * catchType)
   {
   *startIndex = (I_32) (eh[exceptionNumber].startPC) - bcOffset;
   *endIndex = (I_32) (eh[exceptionNumber].endPC) - bcOffset - 1;
   *catchType = (I_32) (eh[exceptionNumber].exceptionClassIndex);
   return (I_32) (eh[exceptionNumber].handlerPC) - bcOffset;
   }

static const char * const excludeArray[] = {
   "java/lang/reflect/AccessibleObject.invokeV(Ljava/lang/Object;[Ljava/lang/Object;)V",
   "java/lang/reflect/AccessibleObject.invokeI(Ljava/lang/Object;[Ljava/lang/Object;)I",
   "java/lang/reflect/AccessibleObject.invokeJ(Ljava/lang/Object;[Ljava/lang/Object;)J",
   "java/lang/reflect/AccessibleObject.invokeF(Ljava/lang/Object;[Ljava/lang/Object;)F",
   "java/lang/reflect/AccessibleObject.invokeD(Ljava/lang/Object;[Ljava/lang/Object;)D",
   "java/lang/reflect/AccessibleObject.invokeL(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;",
   "java/security/AccessController.doPrivileged(Ljava/security/PrivilegedAction;Ljava/security/AccessControlContext;)Ljava/lang/Object;",
   "java/security/AccessController.doPrivileged(Ljava/security/PrivilegedExceptionAction;Ljava/security/AccessControlContext;)Ljava/lang/Object;"
   "java/security/AccessController.doPrivileged(Ljava/security/PrivilegedAction;Ljava/security/AccessControlContext;[Ljava/security/Permission;)Ljava/lang/Object;",
   "java/security/AccessController.doPrivileged(Ljava/security/PrivilegedExceptionAction;Ljava/security/AccessControlContext;[Ljava/security/Permission;)Ljava/lang/Object;"
};

bool
TR_ResolvedJ9MethodBase::isCompilable(TR_Memory * trMemory)
   {
   if (isNative() && (!isJNINative() || getRecognizedMethod() == TR::sun_misc_Unsafe_ensureClassInitialized))
      return false;

   if (isAbstract())
      return false;

   // now go through the list of methods that are excluded by name only
   //
   const char * methodSig = signature(trMemory, stackAlloc);
   for (int i=0; i <sizeof(excludeArray) / sizeof(*excludeArray); ++i)
      if (!strcmp(excludeArray[i], methodSig))
         return false;

   return true;
   }



bool
TR_ResolvedJ9MethodBase::isInlineable(TR::Compilation *comp)
   {
   if (comp->getOption(TR_FullSpeedDebug) && comp->getOption(TR_EnableOSR))
      {
      if (jitMethodIsBreakpointed(fej9()->vmThread(), (J9Method *) getPersistentIdentifier()))
         return false;
      }

   return true;
   }



static intptrj_t getInitialCountForMethod(TR_ResolvedMethod *m, TR::Compilation *comp)
   {
   TR::Options * options = comp->getOptions()->getCmdLineOptions();

   intptrj_t initialCount = m->hasBackwardBranches() ? options->getInitialBCount() : options->getInitialCount();

#if defined(J9VM_INTERP_AOT_COMPILE_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM)) && !defined(SMALL)
   if (TR::Options::sharedClassCache())
      {
      TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);
      J9Method *method = (J9Method *) m->getPersistentIdentifier();

      if (!compInfo->isRomClassForMethodInSharedCache(method, jitConfig->javaVM))
         {
#if defined(J9ZOS390)
          // Do not change the counts on zos at the moment since the shared cache capacity is higher on this platform
          // and by increasing counts we could end up significantly impacting startup
#else
          bool startupTimeMatters = TR::Options::isQuickstartDetected() || TR::Options::getCmdLineOptions()->getOption(TR_UseLowerMethodCounts);

          if (!startupTimeMatters)
             {
             bool useHigherMethodCounts = false;
             J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);

             if (J9ROMMETHOD_HAS_BACKWARDS_BRANCHES(romMethod))
                {
                if (initialCount == TR_QUICKSTART_INITIAL_BCOUNT)
                  useHigherMethodCounts = true;
                }
             else
                {
                if (initialCount == TR_QUICKSTART_INITIAL_COUNT)
                   useHigherMethodCounts = true;
                }

             if (useHigherMethodCounts)
                {
                J9ROMClass *declaringClazz = J9_CLASS_FROM_METHOD(method)->romClass;
                J9UTF8 * className = J9ROMCLASS_CLASSNAME(declaringClazz);

                if (className->length > 5 &&
                    !strncmp(utf8Data(className), "java/", 5))
                  {
                  initialCount = 10000;
                  }
               else
                  {
                  if (J9ROMMETHOD_HAS_BACKWARDS_BRANCHES(romMethod))
                     initialCount = TR_DEFAULT_INITIAL_BCOUNT;
                  else
                     initialCount = TR_DEFAULT_INITIAL_COUNT;
                  }
               }
           }
#endif
         }
      }
#endif

   return initialCount;
   }





bool
TR_ResolvedJ9MethodBase::isCold(TR::Compilation * comp, bool isIndirectCall, TR::ResolvedMethodSymbol * sym)
   {
   if (comp->getOption(TR_DisableMethodIsCold))
      return false;

   // For methods that are resolved but are still interpreted and have high counts
   // we can assume the method is cold
   // We do this for direct calls and currenly not-overridden virtual calls
   // For overridden virtual calls we may decide at some point to traverse all the
   // existing targets to see if they are all interpreted with high counts
   //
   if (!isInterpreted() || maxBytecodeIndex() <= TRIVIAL_INLINER_MAX_SIZE)
      return false;

   if (isIndirectCall && virtualMethodIsOverridden())
      {
      // Method is polymorphic virtual - walk all the targets to see
      // if all of them are not reached
      // FIXME: implement
      return false;
      }

   TR::RecognizedMethod rm = getRecognizedMethod();
   if ((rm == TR::java_math_BigDecimal_noLLOverflowMul || rm == TR::java_math_BigDecimal_noLLOverflowAdd) )
      {
      return false;
      }

   if (true)
      {
      // these methods are never interpreted, so don't bother
      //
      TR::RecognizedMethod rm = comp->getCurrentMethod()->getRecognizedMethod();
      if (rm == TR::com_ibm_jit_DecimalFormatHelper_formatAsDouble ||
          rm == TR::com_ibm_jit_DecimalFormatHelper_formatAsFloat)
         return false;
      }

   intptrj_t count = getInvocationCount();

   TR::Options * options = comp->getOptions()->getCmdLineOptions();
   intptrj_t initialCount = getInitialCountForMethod(this, comp);

   if (count < 0 || count > initialCount)
      return false;

   // if compiling a BigDecimal method, block isn't cold
   if ((!options->getOption(TR_DisableDFP) && !comp->getOptions()->getAOTCmdLineOptions()->getOption(TR_DisableDFP)) &&
       (
#ifdef TR_TARGET_S390
       TR::Compiler->target.cpu.getS390SupportsDFP() ||
#endif
       TR::Compiler->target.cpu.supportsDecimalFloatingPoint()) && sym != NULL)
      {
      TR::MethodSymbol * methodSymbol = sym->getMethodSymbol();
      switch(methodSymbol->getRecognizedMethod())
         {
         case TR::java_math_BigDecimal_DFPIntConstructor:
         case TR::java_math_BigDecimal_DFPLongConstructor:
         case TR::java_math_BigDecimal_DFPLongExpConstructor:
         case TR::java_math_BigDecimal_DFPAdd:
         case TR::java_math_BigDecimal_DFPSubtract:
         case TR::java_math_BigDecimal_DFPMultiply:
         case TR::java_math_BigDecimal_DFPScaledAdd:
         case TR::java_math_BigDecimal_DFPScaledSubtract:
         case TR::java_math_BigDecimal_DFPScaledMultiply:
         case TR::java_math_BigDecimal_DFPRound:
         case TR::java_math_BigDecimal_DFPSignificance:
         case TR::java_math_BigDecimal_DFPExponent:
         case TR::java_math_BigDecimal_DFPCompareTo:
         case TR::java_math_BigDecimal_DFPBCDDigits:
         case TR::java_math_BigDecimal_DFPUnscaledValue:
         case TR::java_math_BigDecimal_DFPSetScale:
         case TR::java_math_BigDecimal_DFPScaledDivide:
         case TR::java_math_BigDecimal_DFPDivide:
         case TR::java_math_BigDecimal_DFPConvertPackedToDFP:
         case TR::java_math_BigDecimal_DFPConvertDFPToPacked:
            return false;
         default:
            break;
         }
      }

   if (comp->isDLT() || fej9()->compiledAsDLTBefore(this))
      return false;

   // If the method has been invoked no more that 5% of the first compile interval
   //
   if (((float)count)/initialCount >= .95)
      {
      //dumpOptDetails(comp, "callee count %d initial count %d\n", count, initialCount);
      TR_ResolvedMethod *compMethod = comp->getCurrentMethod();
      count = compMethod->getInvocationCount();
      initialCount = getInitialCountForMethod(compMethod, comp);

      if ((count < 0) ||
         ((((float)count)/initialCount) < .50))
         {
         //dumpOptDetails(comp, "caller count %d initial count %d\n", count, initialCount);
         return true;
         }
      }

   // default: assume it's not cold
   return false;
   }

TR::SymbolReferenceTable *
TR_ResolvedJ9MethodBase::_genMethodILForPeeking(TR::ResolvedMethodSymbol *methodSymbol, TR::Compilation *c, bool resetVisitCount,  TR_PrexArgInfo* argInfo)
   {
   if (c->getOption(TR_EnableHCR))
      return 0; // Methods can change after peeking them, so peeking is not safe

   // Check if the size of method being peeked exceeds the limit
   TR_ResolvedJ9Method * j9method = static_cast<TR_ResolvedJ9Method *>(methodSymbol->getResolvedMethod());
   if (_fe->_jitConfig->bcSizeLimit && (j9method->maxBytecodeIndex() > _fe->_jitConfig->bcSizeLimit))
      return 0;

   vcount_t oldVisitCount = c->getVisitCount();
   bool b = c->getNeedsClassLookahead();
   c->setNeedsClassLookahead(false);

   c->setVisitCount(1);

   methodSymbol->setParameterList();

   TR_Array<List<TR::SymbolReference> > * oldAutoSymRefs = methodSymbol->getAutoSymRefs();
   TR_Array<List<TR::SymbolReference> > * oldPendingPushSymRefs = methodSymbol->getPendingPushSymRefs();
   methodSymbol->setAutoSymRefs(0);
   methodSymbol->setPendingPushSymRefs(0);

   TR::SymbolReferenceTable *oldSymRefTab = c->getCurrentSymRefTab();
   TR::SymbolReferenceTable *newSymRefTab =
      new (c->trStackMemory()) TR::SymbolReferenceTable(methodSymbol->getResolvedMethod()->maxBytecodeIndex(), c);

   c->setPeekingSymRefTab(newSymRefTab);

   // Do this so that all intermedate calls to c->getSymRefTab()
   // in codegen.dev go to the new symRefTab
   //
   c->setCurrentSymRefTab(newSymRefTab);

   newSymRefTab->addParameters(methodSymbol);

   TR_ByteCodeInfo bci;

   //incInlineDepth is a part of a hack to make InvariantArgumentPreexistence
   //play nicely if getCurrentInlinedCallArgInfo is provided while peeeking.
   //If we don't provide either dummy (default is set to NULL) or real argInfo we will end up
   //using the wrong argInfo coming from a CALLER rather than the peeking method.
   c->getInlinedCallArgInfoStack().push(argInfo);

   // IL request depends on what kind of method we are; build it then generate IL
   TR::IlGeneratorMethodDetails storage;
   TR::IlGeneratorMethodDetails & details = TR::IlGeneratorMethodDetails::create(storage, this);
   TR::CompileIlGenRequest request(details);
   bool success = methodSymbol->genIL(fej9(), c, newSymRefTab, request);

   c->getInlinedCallArgInfoStack().pop();

   c->setCurrentSymRefTab(oldSymRefTab);

   if (resetVisitCount || (c->getVisitCount() < oldVisitCount))
      c->setVisitCount(oldVisitCount);
   c->setNeedsClassLookahead(b);

   methodSymbol->setAutoSymRefs(oldAutoSymRefs);
   methodSymbol->setPendingPushSymRefs(oldPendingPushSymRefs);

   if (success)
      return newSymRefTab;

   return 0;
   }

TR_ResolvedRelocatableJ9Method::TR_ResolvedRelocatableJ9Method(TR_OpaqueMethodBlock * aMethod, TR_FrontEnd * fe, TR_Memory * trMemory, TR_ResolvedMethod * owner, uint32_t vTableSlot)
   : TR_ResolvedJ9Method(aMethod, fe, trMemory, owner, vTableSlot)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;
   TR::Compilation *comp = TR::comp();
   if (comp && this->TR_ResolvedMethod::getRecognizedMethod() != TR::unknownMethod)
      {
      if (fej9->sharedCache()->rememberClass(containingClass()))
         {
         ((TR_ResolvedRelocatableJ9Method *) owner)->validateArbitraryClass(comp, (J9Class*)containingClass());
         }
      else
         {
         setRecognizedMethod(TR::unknownMethod);
         }
      }


   }

int32_t
TR_ResolvedRelocatableJ9Method::virtualCallSelector(U_32 cpIndex)
   {
   return TR_ResolvedJ9Method::virtualCallSelector(cpIndex);
   //return -1;
   }

bool
TR_ResolvedRelocatableJ9Method::unresolvedFieldAttributes(I_32 cpIndex, TR::DataType * type, bool * volatileP, bool * isFinal, bool * isPrivate)
   {
   U_32 ltype;
   I_32 myVolatile, myFinal, myPrivate;

   return false;
   }

bool
TR_ResolvedRelocatableJ9Method::unresolvedStaticAttributes(I_32 cpIndex, TR::DataType * type, bool * volatileP, bool * isFinal, bool * isPrivate)
   {
   U_32 ltype;
   I_32 myVolatile, myFinal, myPrivate;

   return false;
   }

void
TR_ResolvedRelocatableJ9Method::setAttributeResult(bool isStaticField, bool result, UDATA ltype, I_32 myVolatile, I_32 myFinal, I_32 myPrivate, TR::DataType * type, bool * volatileP, bool * isFinal, bool * isPrivate, void ** fieldOffset)
   {
   if (result)
      {
      *volatileP = myVolatile ? true : false;
      if (isFinal) *isFinal = myFinal ? true : false;
      if (isPrivate) *isPrivate = myPrivate ? true : false;
      }
   else
      {
      *volatileP = true;
      if (fieldOffset)
         {
         if (isStaticField)
            *fieldOffset = (void*) NULL;
         else
            *(U_32*)fieldOffset = (U_32) sizeof(J9Object);
         }
      }

   *type = decodeType(ltype);
   }

char *
TR_ResolvedRelocatableJ9Method::fieldOrStaticNameChars(I_32 cpIndex, int32_t & len)
   {
   len = 0;
   return ""; // TODO: Implement me
   }

TR_Method *   TR_ResolvedRelocatableJ9Method::convertToMethod()              { return this; }

bool TR_ResolvedRelocatableJ9Method::isStatic()            { return methodModifiers() & J9AccStatic ? true : false; }
bool TR_ResolvedRelocatableJ9Method::isNative()            { return methodModifiers() & J9AccNative ? true : false; }
bool TR_ResolvedRelocatableJ9Method::isAbstract()          { return methodModifiers() & J9AccAbstract ? true : false; }
bool TR_ResolvedRelocatableJ9Method::hasBackwardBranches() { return J9ROMMETHOD_HAS_BACKWARDS_BRANCHES(romMethod()) ? true : false; }
bool TR_ResolvedRelocatableJ9Method::isObjectConstructor() { return J9ROMMETHOD_IS_OBJECT_CONSTRUCTOR(romMethod()) ? true : false; }
bool TR_ResolvedRelocatableJ9Method::isNonEmptyObjectConstructor() { return J9ROMMETHOD_IS_NON_EMPTY_OBJECT_CONSTRUCTOR(romMethod()) ? true : false; }
bool TR_ResolvedRelocatableJ9Method::isSynchronized()      { return methodModifiers() & J9AccSynchronized ? true : false; }
bool TR_ResolvedRelocatableJ9Method::isPrivate()           { return methodModifiers() & J9AccPrivate ? true : false; }
bool TR_ResolvedRelocatableJ9Method::isProtected()         { return methodModifiers() & J9AccProtected ? true : false; }
bool TR_ResolvedRelocatableJ9Method::isPublic()            { return methodModifiers() & J9AccPublic ? true : false; }
bool TR_ResolvedRelocatableJ9Method::isStrictFP()          { return methodModifiers() & J9AccStrict ? true : false; }

bool TR_ResolvedRelocatableJ9Method::isFinal()             { return (methodModifiers() & J9AccFinal) || (classModifiers() & J9AccFinal) ? true : false;}

bool
TR_ResolvedRelocatableJ9Method::isInterpreted()
   {
   bool alwaysTreatAsInterpreted = true;
#if defined(TR_TARGET_S390)
   alwaysTreatAsInterpreted = false;
#elif defined(TR_TARGET_X86)

   /*if isInterpreted should be only overridden for JNI methods.
   Otherwise buildDirectCall in X86PrivateLinkage.cpp will generate CALL 0
   for certain jitted methods as startAddressForJittedMethod still returns NULL in AOT
   this is not an issue on z as direct calls are dispatched via snippets

   If one of the options to disable JNI is specified
   this function reverts back to the old behaviour
   */
   if (isJNINative() &&
        !TR::Options::getAOTCmdLineOptions()->getOption(TR_DisableDirectToJNI)  &&
        !TR::Options::getAOTCmdLineOptions()->getOption(TR_DisableDirectToJNIInline) &&
        !TR::Options::getCmdLineOptions()->getOption(TR_DisableDirectToJNI) &&
        !TR::Options::getCmdLineOptions()->getOption(TR_DisableDirectToJNIInline)
        )
      {
      alwaysTreatAsInterpreted = false;
      }
#endif

   if (alwaysTreatAsInterpreted)
      return true;

   return TR_ResolvedJ9Method::isInterpreted();
   }

void *
TR_ResolvedRelocatableJ9Method::startAddressForJittedMethod()
   {
   return NULL;
   }

void *
TR_ResolvedRelocatableJ9Method::startAddressForJNIMethod(TR::Compilation * comp)
   {
#if defined(TR_TARGET_S390)  || defined(TR_TARGET_X86) || defined(TR_TARGET_POWER)
   return TR_ResolvedJ9Method::startAddressForJNIMethod(comp);
#else
   return NULL;
#endif
   }

void *
TR_ResolvedRelocatableJ9Method::startAddressForJITInternalNativeMethod()
   {
   return 0;
   }

void *
TR_ResolvedRelocatableJ9Method::startAddressForInterpreterOfJittedMethod()
   {
   return ((J9Method *)getNonPersistentIdentifier())->extra;
   }

void *
TR_ResolvedRelocatableJ9Method::constantPool()
   {
   return romLiterals();
   }

TR_OpaqueClassBlock *
TR_ResolvedRelocatableJ9Method::getClassFromConstantPool(TR::Compilation *comp, uint32_t cpIndex, bool returnClassForAOT)
   {
      TR_OpaqueClassBlock * resolvedClass = NULL;

      if (returnClassForAOT)
         {
         resolvedClass = TR_ResolvedJ9Method::getClassFromConstantPool(comp, cpIndex);
         if (resolvedClass)
            {
            bool validated = validateClassFromConstantPool(comp, (J9Class *)resolvedClass, cpIndex);
            if (validated)
               {
               return (TR_OpaqueClassBlock*)resolvedClass;
               }
            }
         return 0;
         }

      return resolvedClass;
   }

bool
TR_ResolvedRelocatableJ9Method::validateClassFromConstantPool(TR::Compilation *comp, J9Class *clazz, uint32_t cpIndex,  TR_ExternalRelocationTargetKind reloKind)
   {
   return storeValidationRecordIfNecessary(comp, cp(), cpIndex, reloKind, ramMethod(), clazz);
   }

bool
TR_ResolvedRelocatableJ9Method::validateArbitraryClass(TR::Compilation *comp, J9Class *clazz)
   {
   return storeValidationRecordIfNecessary(comp, cp(), 0, TR_ValidateArbitraryClass, ramMethod(), clazz);
   }



static TR::DataType
cpType2trType(UDATA cpType)
   {
   switch (cpType)
      {
      case J9CPTYPE_UNUSED:
      case J9CPTYPE_UNUSED8:
         return TR::NoType;
      case J9CPTYPE_CLASS:
      case J9CPTYPE_STRING:
      case J9CPTYPE_ANNOTATION_UTF8:
      case J9CPTYPE_METHOD_TYPE:
      case J9CPTYPE_METHODHANDLE:
         return TR::Address;
      case J9CPTYPE_INT:
         return TR::Int32;
      case J9CPTYPE_LONG:
         return TR::Int64;
      case J9CPTYPE_FLOAT:
         return TR::Float;
      case J9CPTYPE_DOUBLE:
         return TR::Double;
      default:
         TR_ASSERT(0, "Unknown CPType %d", (int)cpType);
         return TR::NoType;
      }
   }

void *
TR_ResolvedRelocatableJ9Method::stringConstant(I_32 cpIndex)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");

   return (void *) ((U_8 *)&(((J9RAMStringRef *) romLiterals())[cpIndex].stringObject));
   }

bool
TR_ResolvedRelocatableJ9Method::isUnresolvedString(I_32 cpIndex, bool optimizeForAOT)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");

   if (optimizeForAOT)
      return TR_ResolvedJ9Method::isUnresolvedString(cpIndex);
   return (IDATA) 1;
   }

void *
TR_ResolvedRelocatableJ9Method::methodTypeConstant(I_32 cpIndex)
   {
   TR_ASSERT(false, "should be unreachable");
   return NULL;
   }

bool
TR_ResolvedRelocatableJ9Method::isUnresolvedMethodType(I_32 cpIndex)
   {
   TR_ASSERT(false, "should be unreachable");
   return true;
   }

void *
TR_ResolvedRelocatableJ9Method::methodHandleConstant(I_32 cpIndex)
   {
   TR_ASSERT(false, "should be unreachable");
   return NULL;
   }

bool
TR_ResolvedRelocatableJ9Method::isUnresolvedMethodHandle(I_32 cpIndex)
   {
   TR_ASSERT(false, "should be unreachable");
   return true;
   }

bool
TR_ResolvedRelocatableJ9Method::getUnresolvedFieldInCP(I_32 cpIndex)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
   #if defined(J9VM_INTERP_AOT_COMPILE_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))
      return !J9RAMFIELDREF_IS_RESOLVED(((J9RAMFieldRef*)cp()) + cpIndex);
   #else
      return true;
   #endif
   }

bool storeValidationRecordIfNecessary(TR::Compilation * comp, J9ConstantPool *constantPool, int32_t cpIndex, TR_ExternalRelocationTargetKind reloKind, J9Method *ramMethod, J9Class *definingClass)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) comp->fe();

   bool storeClassInfo = true;
   bool fieldInfoCanBeUsed = false;
   TR_AOTStats *aotStats = ((TR_JitPrivateConfig *)fej9->_jitConfig->privateConfig)->aotStats;
   bool isStatic = false;

   TR::CompilationInfo *compInfo = TR::CompilationInfo::get(fej9->_jitConfig);
   TR::CompilationInfoPerThreadBase *compInfoPerThreadBase = compInfo->getCompInfoForCompOnAppThread();
   TR_RelocationRuntime *reloRuntime;
   if (compInfoPerThreadBase)
      reloRuntime = compInfoPerThreadBase->reloRuntime();
   else
      reloRuntime = compInfo->getCompInfoForThread(fej9->vmThread())->reloRuntime();

   isStatic = (reloKind == TR_ValidateStaticField);

   traceMsg(comp, "storeValidationRecordIfNecessary:\n");
   traceMsg(comp, "\tconstantPool %p cpIndex %d\n", constantPool, cpIndex);
   traceMsg(comp, "\treloKind %d isStatic %d\n", reloKind, isStatic);
   J9UTF8 *methodClassName = J9ROMCLASS_CLASSNAME(J9_CLASS_FROM_METHOD(ramMethod)->romClass);
   traceMsg(comp, "\tmethod %p from class %p %.*s\n", ramMethod, J9_CLASS_FROM_METHOD(ramMethod), J9UTF8_LENGTH(methodClassName), J9UTF8_DATA(methodClassName));
   traceMsg(comp, "\tdefiningClass %p\n", definingClass);

   if (!definingClass)
      {
      definingClass = (J9Class *) reloRuntime->getClassFromCP(fej9->vmThread(), fej9->_jitConfig->javaVM, constantPool, cpIndex, isStatic);
      traceMsg(comp, "\tdefiningClass recomputed from cp as %p\n", definingClass);
      }

   if (!definingClass)
      {
      if (aotStats)
         aotStats->numDefiningClassNotFound++;
      return false;
      }

   J9UTF8 *className = J9ROMCLASS_CLASSNAME(definingClass->romClass);
   traceMsg(comp, "\tdefiningClass name %.*s\n", J9UTF8_LENGTH(className), J9UTF8_DATA(className));

   J9ROMClass *romClass = NULL;
   void *classChain = NULL;

   // all kinds of validations may need to rely on the entire class chain, so make sure we can build one first
   classChain = fej9->sharedCache()->rememberClass(definingClass);
   if (!classChain)
      return false;

   bool inLocalList = false;
   TR::list<TR::AOTClassInfo*>* aotClassInfo = comp->_aotClassInfo;
   if (!aotClassInfo->empty())
      {
      for (auto info = aotClassInfo->begin(); info != aotClassInfo->end(); ++info)
         {
         TR_ASSERT(((*info)->_reloKind == TR_ValidateInstanceField ||
                 (*info)->_reloKind == TR_ValidateStaticField ||
                 (*info)->_reloKind == TR_ValidateClass ||
                 (*info)->_reloKind == TR_ValidateArbitraryClass),
                "TR::AOTClassInfo reloKind is not TR_ValidateInstanceField or TR_ValidateStaticField or TR_ValidateClass!");

         if ((*info)->_reloKind == reloKind)
            {
            if (isStatic)
               inLocalList = (romClass == ((J9Class *)((*info)->_clazz))->romClass);
            else
               inLocalList = (classChain == (*info)->_classChain &&
                              cpIndex == (*info)->_cpIndex &&
                              ramMethod == (J9Method *)(*info)->_method);

            if (inLocalList)
               break;
            }
         }
      }

   if (inLocalList)
      {
      traceMsg(comp, "\tFound in local list, nothing to do\n");
      if (aotStats)
         {
         if (isStatic)
            aotStats->numStaticEntriesAlreadyStoredInLocalList++;
         else
            aotStats->numCHEntriesAlreadyStoredInLocalList++;
         }
      return true;
      }

   TR::AOTClassInfo *classInfo = new (comp->trHeapMemory()) TR::AOTClassInfo(fej9, (TR_OpaqueClassBlock *)definingClass, (void *) classChain, (TR_OpaqueMethodBlock *)ramMethod, cpIndex, reloKind);
   if (classInfo)
      {
      traceMsg(comp, "\tCreated new AOT class info %p\n", classInfo);
      comp->_aotClassInfo->push_front(classInfo);
      if (aotStats)
         {
         if (isStatic)
            aotStats->numNewStaticEntriesInLocalList++;
         else
            aotStats->numNewCHEntriesInLocalList++;
         }

      return true;
      }

   // should only be a native OOM that gets us here...
   return false;
   }

bool
TR_ResolvedRelocatableJ9Method::fieldAttributes(TR::Compilation * comp, int32_t cpIndex, uint32_t * fieldOffset, TR::DataType * type, bool * volatileP, bool * isFinal, bool * isPrivate, bool isStore, bool * unresolvedInCP, bool needAOTValidation)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");

#if defined(DEBUG_LOCAL_CLASS_OPT)
   fieldAttributeCtr++;
#endif

   UDATA ltype;
   I_32 volatileFlag = 0, finalFlag = 0, privateFlag = 0;

   bool result = false;
   bool theFieldIsFromLocalClass = false;
   J9ROMFieldShape * fieldShape = 0;
   J9ConstantPool *constantPool = (J9ConstantPool *)(J9_CP_FROM_METHOD(ramMethod()));

   IDATA offset;
   bool fieldInfoCanBeUsed = false;
   bool aotStats = comp->getOption(TR_EnableAOTStats);
   bool resolveField = true;

      {
      TR::VMAccessCriticalSection fieldAttributes(fej9());
      offset = jitCTResolveInstanceFieldRefWithMethod(_fe->vmThread(), ramMethod(), cpIndex, isStore, &fieldShape);

      if (comp->getOption(TR_DisableAOTInstanceFieldResolution))
         resolveField = false;
      else
         fieldInfoCanBeUsed = !needAOTValidation || storeValidationRecordIfNecessary(comp, constantPool, cpIndex, TR_ValidateInstanceField, ramMethod());
      }

   if (offset == J9JIT_RESOLVE_FAIL_COMPILE)
      {
      comp->failCompilation<TR::CompilationException>("offset == J9JIT_RESOLVE_FAIL_COMPILE");
      }

   if (!fieldInfoCanBeUsed && aotStats)
      ((TR_JitPrivateConfig *)_fe->_jitConfig->privateConfig)->aotStats->numInstanceFieldInfoNotUsed++;

   //*fieldOffset = (U_32)NULL; // No path should currently set fieldOffset to non-NULL for AOT.
   if (!resolveField)
      {
      *fieldOffset = (U_32)NULL;
      fieldInfoCanBeUsed = false;
      }
   if (offset >= 0 &&
       (!(_fe->_jitConfig->runtimeFlags & J9JIT_RUNTIME_RESOLVE) ||
        comp->ilGenRequest().details().isMethodHandleThunk() || // cmvc 195373
        !performTransformation(comp, "Setting as unresolved field attributes cpIndex=%d\n",cpIndex))  && fieldInfoCanBeUsed
       /*fieldIsFromLocalClass((void *)_methodCookie, cpIndex)*/)
      {
      theFieldIsFromLocalClass = true;
      ltype = fieldShape->modifiers;
      volatileFlag = (ltype & J9AccVolatile) ? 1 : 0;
      finalFlag = (ltype & J9AccFinal) ? 1 : 0;
      privateFlag = (ltype & J9AccPrivate) ? 1 : 0;

      if (aotStats)
         ((TR_JitPrivateConfig *)_fe->_jitConfig->privateConfig)->aotStats->numInstanceFieldInfoUsed++;
      if (resolveField) *fieldOffset = offset + sizeof(J9Object);  // add header size
#if defined(DEBUG_LOCAL_CLASS_OPT)
      resolvedCtr++;
#endif
      }
   else
      {
#if defined(J9VM_INTERP_AOT_COMPILE_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))
      ltype = getFieldType((J9ROMConstantPoolItem *)romLiterals(), cpIndex);
#endif

#if defined(DEBUG_LOCAL_CLASS_OPT)
      unresolvedCtr++;
#endif
      }

   if (unresolvedInCP)
      *unresolvedInCP = getUnresolvedFieldInCP(cpIndex);

   setAttributeResult(false,
                      theFieldIsFromLocalClass,
                      ltype,
                      volatileFlag,
                      finalFlag,
                      privateFlag,
                      type,
                      volatileP,
                      isFinal,
                      isPrivate,
                      (void**)fieldOffset);

   return theFieldIsFromLocalClass;
   }

bool
TR_ResolvedRelocatableJ9Method::staticAttributes(TR::Compilation * comp,
                                                 int32_t cpIndex,
                                                 void * * address,
                                                 TR::DataType * type,
                                                 bool * volatileP,
                                                 bool * isFinal,
                                                 bool * isPrivate,
                                                 bool isStore,
                                                 bool * unresolvedInCP,
                                                 bool needAOTValidation)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");

#if defined(DEBUG_LOCAL_CLASS_OPT)
   staticAttributeCtr++;
#endif

   U_32 ltype;
   void *offset;
   I_32 volatileFlag = 0, finalFlag = 0, privateFlag = 0;
   bool result = false;
   bool theFieldIsFromLocalClass = false;
   J9ROMFieldShape * fieldShape = 0;
   J9ConstantPool *constantPool = (J9ConstantPool *)(J9_CP_FROM_METHOD(ramMethod()));

      {
      TR::VMAccessCriticalSection staticAttributes(fej9());
      offset = jitCTResolveStaticFieldRefWithMethod(_fe->vmThread(), ramMethod(), cpIndex, isStore, &fieldShape);
      }

   bool fieldInfoCanBeUsed = false;
   bool aotStats = comp->getOption(TR_EnableAOTStats);

   fieldInfoCanBeUsed = !needAOTValidation || storeValidationRecordIfNecessary(comp, constantPool, cpIndex, TR_ValidateStaticField, ramMethod());

   if (offset == (void *)J9JIT_RESOLVE_FAIL_COMPILE)
      {
      comp->failCompilation<TR::CompilationException>("offset == J9JIT_RESOLVE_FAIL_COMPILE");
      }

   if (offset && fieldInfoCanBeUsed &&
      (!(_fe->_jitConfig->runtimeFlags & J9JIT_RUNTIME_RESOLVE) ||
      comp->ilGenRequest().details().isMethodHandleThunk() || // cmvc 195373
      !performTransformation(comp, "Setting as unresolved static attributes cpIndex=%d\n",cpIndex)) /*&&
      compInfo->isRomClassForMethodInSharedCache(method, jitConfig->javaVM) */  /* &&
      fieldIsFromLocalClass((void *)_methodCookie, cpIndex)*/)
      {
      theFieldIsFromLocalClass = true;
      ltype = fieldShape->modifiers;
      volatileFlag = (ltype & J9AccVolatile) ? 1 : 0;
      finalFlag = (ltype & J9AccFinal) ? 1 : 0;
      privateFlag = (ltype & J9AccPrivate) ? 1 : 0;
      *address = offset;

      if (aotStats)
         ((TR_JitPrivateConfig *)_fe->_jitConfig->privateConfig)->aotStats->numStaticFieldInfoUsed++;
#if defined(DEBUG_LOCAL_CLASS_OPT)
      resolvedCtr++;
#endif
      }
   else
      {
      if (aotStats)
         ((TR_JitPrivateConfig *)_fe->_jitConfig->privateConfig)->aotStats->numStaticFieldInfoNotUsed++;
#if defined(J9VM_INTERP_AOT_COMPILE_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))
      ltype = getFieldType((J9ROMConstantPoolItem *)romLiterals(), cpIndex);
#endif

#if defined(DEBUG_LOCAL_CLASS_OPT)
      unresolvedCtr++;
#endif
      }

   if (unresolvedInCP)
      *unresolvedInCP = !J9RAMSTATICFIELDREF_IS_RESOLVED(((J9RAMStaticFieldRef*)constantPool) + cpIndex);

   setAttributeResult(true, theFieldIsFromLocalClass, ltype, volatileFlag, finalFlag, privateFlag, type, volatileP, isFinal, isPrivate, address);

   return theFieldIsFromLocalClass;
   }

char *
TR_ResolvedRelocatableJ9Method::fieldSignatureChars(int32_t cpIndex, int32_t & len)
   {
   return cpIndex > 0 ? fieldOrStaticSignatureChars(cpIndex, len) : 0;
   }

char *
TR_ResolvedRelocatableJ9Method::staticSignatureChars(int32_t cpIndex, int32_t & len)
   {
   return cpIndex >= 0 ? fieldOrStaticSignatureChars(cpIndex, len) : 0;
   }

TR_OpaqueClassBlock *
TR_ResolvedRelocatableJ9Method::classOfStatic(int32_t cpIndex, bool returnClassForAOT)
   {
   TR_OpaqueClassBlock * resolveClassOfStatic = NULL;

   if (returnClassForAOT)
      {
      resolveClassOfStatic = TR_ResolvedJ9Method::classOfStatic(cpIndex, returnClassForAOT);
      }

   return resolveClassOfStatic;
   }

void
TR_ResolvedRelocatableJ9Method::handleUnresolvedStaticMethodInCP(int32_t cpIndex, bool * unresolvedInCP)
   {
   *unresolvedInCP = getUnresolvedStaticMethodInCP(cpIndex);
   }

void
TR_ResolvedRelocatableJ9Method::handleUnresolvedSpecialMethodInCP(int32_t cpIndex, bool * unresolvedInCP)
   {
   *unresolvedInCP = getUnresolvedSpecialMethodInCP(cpIndex);
   }

void
TR_ResolvedRelocatableJ9Method::handleUnresolvedVirtualMethodInCP(int32_t cpIndex, bool * unresolvedInCP)
   {
   *unresolvedInCP = getUnresolvedVirtualMethodInCP(cpIndex);
   }

bool
TR_ResolvedRelocatableJ9Method::getUnresolvedMethodInCP(TR_OpaqueMethodBlock *aMethod)
   {
   J9Method *ramMethod = (J9Method*)aMethod;
   return !ramMethod || !J9_BYTECODE_START_FROM_RAM_METHOD(ramMethod);
   }

bool
TR_ResolvedRelocatableJ9Method::getUnresolvedStaticMethodInCP(I_32 cpIndex)
   {
   J9Method *method = NULL;
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");

      {
      TR::VMAccessCriticalSection getUnresolvedStaticMethodInCP(fej9());
      method = jitResolveStaticMethodRef(_fe->vmThread(),
                                         (J9ConstantPool *)(J9_CP_FROM_METHOD(ramMethod())),
                                         cpIndex,
                                         J9_RESOLVE_FLAG_JIT_COMPILE_TIME);
      }

   return getUnresolvedMethodInCP((TR_OpaqueMethodBlock*)method);
   }

static bool doResolveAtRuntime(J9Method *method, I_32 cpIndex, TR::Compilation *comp)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   if (!method)
      return true;
   else if (method == J9VMJAVALANGINVOKEMETHODHANDLE_INVOKEWITHARGUMENTSHELPER_METHOD(fej9->getJ9JITConfig()->javaVM))
      {
      // invokeWithArgumentsHelper is a weirdo
      if (fej9->isAOT_DEPRECATED_DO_NOT_USE())
         {
         comp->failCompilation<TR::CompilationException>("invokeWithArgumentsHelper");
         }
      else
         return false; // It is incorrect to try to resolve this
      }
   else if (comp->ilGenRequest().details().isMethodHandleThunk()) // cmvc 195373
      return false;
   else if (fej9->getJ9JITConfig()->runtimeFlags & J9JIT_RUNTIME_RESOLVE)
      return performTransformation(comp, "Setting as unresolved static call cpIndex=%d\n", cpIndex);
   else
      return false;
   }


bool
TR_ResolvedRelocatableJ9Method::getUnresolvedSpecialMethodInCP(I_32 cpIndex)
   {
   J9Method *method = NULL;
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");

      {
      TR::VMAccessCriticalSection getUnresolvedSpecialMethodInCP(fej9());
      method = jitResolveSpecialMethodRef(_fe->vmThread(), (J9ConstantPool *)(J9_CP_FROM_METHOD(ramMethod())), cpIndex, J9_RESOLVE_FLAG_JIT_COMPILE_TIME);
      }

   return getUnresolvedMethodInCP((TR_OpaqueMethodBlock*)method);
   }

bool
TR_ResolvedRelocatableJ9Method::getUnresolvedVirtualMethodInCP(I_32 cpIndex)
   {
/*
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
   J9Method *ramMethod;
   I_32 rc=_fe->_vmFunctionTable->resolveVirtualMethodRef(_fe->vmThread(), (J9ConstantPool *)(J9_CP_FROM_METHOD((((AOTSharedCookie *)_methodCookie)->ramMethod))), cpIndex, J9_RESOLVE_FLAG_JIT_COMPILE_TIME, &ramMethod);
   if (!rc)
      return false;

   return getUnresolvedMethodInCP((TR_OpaqueMethodBlock*)ramMethod);
*/
   return false;
   }

TR_ResolvedMethod *
TR_ResolvedRelocatableJ9Method::createResolvedMethodFromJ9Method(TR::Compilation *comp, I_32 cpIndex, uint32_t vTableSlot, J9Method *j9method, bool * unresolvedInCP, TR_AOTInliningStats *aotStats)
   {
   TR_ResolvedMethod *resolvedMethod = NULL;

#if defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))
   static char *dontInline = feGetEnv("TR_AOTDontInline");
   bool resolveAOTMethods = !comp->getOption(TR_DisableAOTResolveDiffCLMethods);
   bool enableAggressive = comp->getOption(TR_EnableAOTInlineSystemMethod);
   bool isSystemClassLoader = false;

   if (dontInline)
      return NULL;

   if (TR::Options::getCmdLineOptions()->getOption(TR_DisableDFP) ||
       TR::Options::getAOTCmdLineOptions()->getOption(TR_DisableDFP) ||
       (!(TR::Compiler->target.cpu.supportsDecimalFloatingPoint()
#ifdef TR_TARGET_S390
       || TR::Compiler->target.cpu.getS390SupportsDFP()
#endif
         ) ||
          !TR_J9MethodBase::isBigDecimalMethod(j9method)))
      {
      // Check if same classloader
      J9Class *j9clazz = (J9Class *) J9_CLASS_FROM_CP(((J9RAMConstantPoolItem *) J9_CP_FROM_METHOD(((J9Method *)j9method))));
      TR_OpaqueClassBlock *clazzOfInlinedMethod = _fe->convertClassPtrToClassOffset(j9clazz);
      TR_OpaqueClassBlock *clazzOfCompiledMethod = _fe->convertClassPtrToClassOffset(J9_CLASS_FROM_METHOD(ramMethod()));

      if (enableAggressive)
         {
         isSystemClassLoader = ((void*)_fe->vmThread()->javaVM->systemClassLoader->classLoaderObject ==  (void*)_fe->getClassLoader(clazzOfInlinedMethod));
         }

      if (TR::CompilationInfo::get(_fe->_jitConfig)->isRomClassForMethodInSharedCache(j9method, _fe->_jitConfig->javaVM))
         {
         bool sameLoaders = false;
         TR_J9VMBase *fej9 = (TR_J9VMBase *)_fe;
         if (resolveAOTMethods ||
             (sameLoaders = fej9->sameClassLoaders(clazzOfInlinedMethod, clazzOfCompiledMethod)) ||
             isSystemClassLoader)
            {
            resolvedMethod = new (comp->trHeapMemory()) TR_ResolvedRelocatableJ9Method((TR_OpaqueMethodBlock *) j9method, _fe, comp->trMemory(), this, vTableSlot);
            if (aotStats)
               {
               aotStats->numMethodResolvedAtCompile++;
               if (_fe->convertClassPtrToClassOffset(J9_CLASS_FROM_METHOD(ramMethod())) == _fe->convertClassPtrToClassOffset(J9_CLASS_FROM_METHOD(j9method)))
                  aotStats->numMethodInSameClass++;
               else
                  aotStats->numMethodNotInSameClass++;
               }
            }
         else if (aotStats && !sameLoaders)
            {
            aotStats->numMethodFromDiffClassLoader++;
            }
         }
      else if (aotStats &&
               !TR::CompilationInfo::get(_fe->_jitConfig)->isRomClassForMethodInSharedCache(j9method, _fe->_jitConfig->javaVM))
         {
         aotStats->numMethodROMMethodNotInSC++;
         }
      }

#endif

   return resolvedMethod;
   }

#if 0
#endif

U_8 *
TR_ResolvedRelocatableJ9Method::allocateException(uint32_t numBytes, TR::Compilation *comp)
   {
   J9JITExceptionTable *eTbl = NULL;
   uint32_t size = 0;
   bool shouldRetryAllocation;
   eTbl = (J9JITExceptionTable *)_fe->allocateDataCacheRecord(numBytes, comp, true, &shouldRetryAllocation,
                                                              J9_JIT_DCE_EXCEPTION_INFO, &size);
   if (!eTbl)
      {
      if (shouldRetryAllocation)
         {
         // force a retry
         comp->failCompilation<J9::RecoverableDataCacheError>("Failed to allocate exception table");
         }
      comp->failCompilation<J9::DataCacheError>("Failed to allocate exception table");
      }
   memset((uint8_t *)eTbl, 0, size);

   // These get updated in TR_RelocationRuntime::relocateAOTCodeAndData
   eTbl->ramMethod = NULL;
   eTbl->constantPool = NULL;

   return (U_8 *) eTbl;
   }

TR_OpaqueMethodBlock *
TR_ResolvedRelocatableJ9Method::getNonPersistentIdentifier()
   {
   return (TR_OpaqueMethodBlock *)ramMethod();
   }


TR_J9Method::TR_J9Method(TR_FrontEnd * fe, TR_Memory * trMemory, J9Class * aClazz, uintptr_t cpIndex)
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

TR_J9Method::TR_J9Method(TR_FrontEnd * fe, TR_Memory * trMemory, TR_OpaqueMethodBlock * aMethod)
   {
   J9ROMMethod * romMethod;
   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;

      {
      TR::VMAccessCriticalSection j9method(fej9);
      romMethod = getOriginalROMMethod((J9Method *)aMethod);
      }

   J9ROMClass *romClass = J9_CLASS_FROM_METHOD(((J9Method *)aMethod))->romClass;
   _className = J9ROMCLASS_CLASSNAME(romClass);
   _name = J9ROMMETHOD_GET_NAME(romClass, romMethod);
   _signature = J9ROMMETHOD_GET_SIGNATURE(romClass, romMethod);

   parseSignature(trMemory);
   _fullSignature = NULL;
   }

//////////////////////////////
//
//  TR_ResolvedMethod
//
/////////////////////////

static bool supportsFastJNI(TR_FrontEnd *fe)
   {
#if defined(TR_TARGET_S390) || defined(TR_TARGET_X86) || defined(TR_TARGET_POWER)
   return true;
#else
   return false;
#endif
   }

TR_ResolvedJ9Method::TR_ResolvedJ9Method(TR_OpaqueMethodBlock * aMethod, TR_FrontEnd * fe, TR_Memory * trMemory, TR_ResolvedMethod * owner, uint32_t vTableSlot)
   : TR_J9Method(fe, trMemory, aMethod), TR_ResolvedJ9MethodBase(fe, owner), _pendingPushSlots(-1)
   {
   _ramMethod = (J9Method *)aMethod;

      {
      TR::VMAccessCriticalSection resolvedj9method(fej9());
      _romMethod = getOriginalROMMethod(_ramMethod);
      }

   _romLiterals = (J9ROMConstantPoolItem *) ((UDATA)romClassPtr() + sizeof(J9ROMClass));
   _vTableSlot = vTableSlot;
   _j9classForNewInstance = NULL;
   if (supportsFastJNI(fe))
      {
      TR_J9VMBase *j9fe = (TR_J9VMBase *)_fe;
      // a non-NULL target address is returned for both JNI natives and non-JNI natives with FastJNI replacements, NULL otherwise
      // properties will be non-zero IFF the target address points to a FastJNI replacement
      _jniTargetAddress = j9fe->getJ9JITConfig()->javaVM->internalVMFunctions->jniNativeMethodProperties(j9fe->vmThread(), _ramMethod, &_jniProperties);

	  // Check for user specified FastJNI override
	  if (TR::Options::getJniAccelerator() != NULL && TR::SimpleRegex::match(TR::Options::getJniAccelerator(), signature(trMemory)))
         {
		 _jniProperties |= 
             J9_FAST_JNI_RETAIN_VM_ACCESS | 
             J9_FAST_JNI_NOT_GC_POINT |
             J9_FAST_NO_NATIVE_METHOD_FRAME |
             J9_FAST_JNI_NO_EXCEPTION_THROW |
             J9_FAST_JNI_NO_SPECIAL_TEAR_DOWN;
         }
      }
   else
      {
      _jniProperties = 0;
      _jniTargetAddress = NULL;
      }

#define x(a, b, c) a, sizeof(b) - 1, b, (int16_t)strlen(c), c

   struct X
      {
      TR::RecognizedMethod _enum;
      char _nameLen;
      const char * _name;
      int16_t _sigLen;
      const char * _sig;
   };

   static X ArrayListMethods[] =
      {
      {x(TR::java_util_ArrayList_add,  "add",    "(Ljava/lang/Object;)Z")},
      {x(TR::java_util_ArrayList_ensureCapacity,      "ensureCapacity",      "(I)V")},
      {x(TR::java_util_ArrayList_get,      "get",      "(I)Ljava/lang/Object;")},
      {x(TR::java_util_ArrayList_remove, "remove", "(I)Ljava/lang/Object;")},
      {x(TR::java_util_ArrayList_set,    "set", "(ILjava/lang/Object;)Ljava/lang/Object;")},
      {  TR::unknownMethod}
      };

   static X ClassMethods[] =
      {
      {x(TR::java_lang_Class_newInstancePrototype, "newInstancePrototype", "(Ljava/lang/Class;)Ljava/lang/Object;")},
      //{x(TR::java_lang_Class_newInstanceImpl,      "newInstanceImpl",      "()Ljava/lang/Object;")},
      {x(TR::java_lang_Class_newInstance,          "newInstance",          "()Ljava/lang/Object;")},
      {x(TR::java_lang_Class_isArray,              "isArray",              "()Z")},
      {x(TR::java_lang_Class_isPrimitive,          "isPrimitive",          "()Z")},
      {x(TR::java_lang_Class_getComponentType,     "getComponentType",     "()Ljava/lang/Class;")},
      {x(TR::java_lang_Class_getModifiersImpl,     "getModifiersImpl",     "()I")},
      {x(TR::java_lang_Class_isAssignableFrom,     "isAssignableFrom",     "(Ljava/lang/Class;)Z")},
      {x(TR::java_lang_Class_isInstance,           "isInstance",           "(Ljava/lang/Object;)Z")},
      {x(TR::java_lang_Class_isInterface,          "isInterface",          "()Z")},
      {  TR::unknownMethod}
      };

   static X ClassLoaderMethods[] =
      {
      {x(TR::java_lang_ClassLoader_callerClassLoader,     "callerClassLoader",    "()Ljava/lang/ClassLoader;")},
      {x(TR::java_lang_ClassLoader_getCallerClassLoader,  "getCallerClassLoader", "()Ljava/lang/ClassLoader;")},
      {x(TR::java_lang_ClassLoader_getStackClassLoader,   "getStackClassLoader",  "(I)Ljava/lang/ClassLoader;")},
      {  TR::unknownMethod}
      };

   static X DoubleMethods[] =
      {
      {x(TR::java_lang_Double_longBitsToDouble, "longBitsToDouble", "(J)D")},
      {x(TR::java_lang_Double_doubleToLongBits, "doubleToLongBits", "(D)J")},
      {x(TR::java_lang_Double_doubleToRawLongBits, "doubleToRawLongBits", "(D)J")},
      {  TR::java_lang_Double_init,                  6,    "<init>", (int16_t)-1,    "*"},
      {  TR::unknownMethod}
      };

   static X FloatMethods[] =
      {
      {x(TR::java_lang_Float_intBitsToFloat, "intBitsToFloat", "(I)F")},
      {x(TR::java_lang_Float_floatToIntBits, "floatToIntBits", "(F)I")},
      {x(TR::java_lang_Float_floatToRawIntBits, "floatToRawIntBits", "(F)I")},
      {  TR::java_lang_Float_init,                  6,    "<init>", (int16_t)-1,    "*"},
      {  TR::unknownMethod}
      };

   static X HashtableMethods[] =
      {
      {x(TR::java_util_Hashtable_get,        "get",           "(Ljava/lang/Object;)Ljava/lang/Object;")},
      {x(TR::java_util_Hashtable_put,        "put",           "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;")},
      {x(TR::java_util_Hashtable_clone,      "clone",         "()Ljava/lang/Object;")},
      {x(TR::java_util_Hashtable_putAll,     "putAll",        "(Ljava/util/Map;)V")},
      {x(TR::java_util_Hashtable_rehash,     "rehash",        "()V")},
      {x(TR::java_util_Hashtable_remove,     "remove",        "(Ljava/lang/Object;)Ljava/lang/Object;")},
      {x(TR::java_util_Hashtable_contains,   "contains",      "(Ljava/lang/Object;)Z")},
      {x(TR::java_util_Hashtable_getEntry,   "getEntry",      "(Ljava/lang/Object;)Ljava/util/HashMapEntry;")},
      {x(TR::java_util_Hashtable_getEnumeration,   "getEnumeration",      "(I)Ljava/util/Enumeration;")},
      {x(TR::java_util_Hashtable_elements,   "elements",      "()Ljava/util/Enumeration;")},
      {  TR::unknownMethod}
      };

   static X TreeMapMethods[] =
      {
      {x(TR::java_util_TreeMap_rbInsert,     "rbInsert",           "(Ljava/lang/Object;)Ljava/util/TreeMap$Entry;")},
      {  TR::unknownMethod}
      };

   static X TreeMapUnboundedValueIteratorMethods[] =
      {
      {x(TR::java_util_TreeMapUnboundedValueIterator_next,   "next",      "()Ljava/lang/Object;")},
      {  TR::unknownMethod}
      };

   static X HashMapMethods[] =
      {
      {x(TR::java_util_HashMap_rehash,                "rehash",           "()V")},
      {x(TR::java_util_HashMap_analyzeMap,            "analyzeMap",           "()V")},
      {x(TR::java_util_HashMap_calculateCapacity,     "calculateCapacity",           "(I)I")},
      {x(TR::java_util_HashMap_findNullKeyEntry,      "findNullKeyEntry",           "()Ljava/util/HashMap$Entry;")},
      {x(TR::java_util_HashMap_get,                   "get",           "(Ljava/lang/Object;)Ljava/lang/Object;")},
      {x(TR::java_util_HashMap_getNode,               "getNode",       "(ILjava/lang/Object;)Ljava/util/HashMap$Node;")},
      {x(TR::java_util_HashMap_putImpl,               "putImpl",       "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;")},
      {x(TR::java_util_HashMap_findNonNullKeyEntry,   "findNonNullKeyEntry",         "(Ljava/lang/Object;II)Ljava/util/HashMap$Entry;")},
      {x(TR::java_util_HashMap_resize,                "resize",         "()[Ljava/util/HashMap$Node;")},
      {  TR::unknownMethod}
      };

   static X HashMapHashIteratorMethods[] =
      {
      {x(TR::java_util_HashMapHashIterator_nextNode,   "nextNode",      "()Ljava/util/HashMap$Node;")},
      {  TR::unknownMethod}
      };

   static X HashtableHashEnumeratorMethods[] =
      {
      {x(TR::java_util_HashtableHashEnumerator_nextElement,     "nextElement",     "()Ljava/lang/Object;")},
      {x(TR::java_util_HashtableHashEnumerator_hasMoreElements, "hasMoreElements", "()Z")},
      {  TR::unknownMethod}
      };

   static X WriterMethods[] =
      {
      {x(TR::java_io_Writer_write_lStringII, "write", "(Ljava/lang/String;II)V")},
      {x(TR::java_io_Writer_write_I,         "write", "(I)V")},
      {  TR::unknownMethod}
      };

   static X ByteArrayOutputStreamMethods[] =
      {
      {x(TR::java_io_ByteArrayOutputStream_write, "write", "([BII)V")},
      { TR::unknownMethod}
      };

   static X ObjectInputStream_BlockDataInputStreamMethods[] =
      {
      {x(TR::java_io_ObjectInputStream_BlockDataInputStream_read, "read", "([BIIZ)I")},
      {  TR::unknownMethod}
      };

   static X BitsMethods[] =
      {
      {x(TR::java_nio_Bits_copyToByteArray,            "copyToByteArray",            "(JLjava/lang/Object;JJ)V")},
      {x(TR::java_nio_Bits_copyFromByteArray,          "copyFromByteArray",          "(Ljava/lang/Object;JJJ)V")},
      {x(TR::java_nio_Bits_keepAlive,                  "keepAlive",                  "(Ljava/lang/Object;)V")},
      {  TR::unknownMethod}
      };

   static X HeapByteBufferMethods[] =
       {
       {x(TR::java_nio_HeapByteBuffer_put,             "put",                        "(IB)Ljava/nio/ByteBuffer;")},
       {  TR::unknownMethod}
       };

   static X MathMethods[] =
      {
      {x(TR::java_lang_Math_abs_I,            "abs",            "(I)I")},
      {x(TR::java_lang_Math_abs_L,            "abs",            "(J)J")},
      {x(TR::java_lang_Math_abs_F,            "abs",            "(F)F")},
      {x(TR::java_lang_Math_abs_D,            "abs",            "(D)D")},
      {x(TR::java_lang_Math_acos,             "acos",           "(D)D")},
      {x(TR::java_lang_Math_asin,             "asin",           "(D)D")},
      {x(TR::java_lang_Math_atan,             "atan",           "(D)D")},
      {x(TR::java_lang_Math_atan2,            "atan2",          "(DD)D")},
      {x(TR::java_lang_Math_cbrt,             "cbrt",           "(D)D")},
      {x(TR::java_lang_Math_ceil,             "ceil",           "(D)D")},
      {x(TR::java_lang_Math_copySign_F,       "copySign",       "(FF)F")},
      {x(TR::java_lang_Math_copySign_D,       "copySign",       "(DD)D")},
      {x(TR::java_lang_Math_cos,              "cos",            "(D)D")},
      {x(TR::java_lang_Math_cosh,             "cosh",           "(D)D")},
      {x(TR::java_lang_Math_exp,              "exp",            "(D)D")},
      {x(TR::java_lang_Math_expm1,            "expm1",          "(D)D")},
      {x(TR::java_lang_Math_floor,            "floor",          "(D)D")},
      {x(TR::java_lang_Math_hypot,            "hypot",          "(DD)D")},
      {x(TR::java_lang_Math_IEEEremainder,    "IEEEremainder",  "(DD)D")},
      {x(TR::java_lang_Math_log,              "log",            "(D)D")},
      {x(TR::java_lang_Math_log10,            "log10",          "(D)D")},
      {x(TR::java_lang_Math_log1p,            "log1p",          "(D)D")},
      {x(TR::java_lang_Math_max_I,            "max",            "(II)I")},
      {x(TR::java_lang_Math_max_L,            "max",            "(JJ)J")},
      {x(TR::java_lang_Math_max_F,            "max",            "(FF)F")},
      {x(TR::java_lang_Math_max_D,            "max",            "(DD)D")},
      {x(TR::java_lang_Math_min_I,            "min",            "(II)I")},
      {x(TR::java_lang_Math_min_L,            "min",            "(JJ)J")},
      {x(TR::java_lang_Math_min_F,            "min",            "(FF)F")},
      {x(TR::java_lang_Math_min_D,            "min",            "(DD)D")},
      {x(TR::java_lang_Math_nextAfter_F,      "nextAfter",      "(FD)F")},
      {x(TR::java_lang_Math_nextAfter_D,      "nextAfter",      "(DD)D")},
      {x(TR::java_lang_Math_pow,              "pow",            "(DD)D")},
      {x(TR::java_lang_Math_rint,             "rint",           "(D)D")},
      {x(TR::java_lang_Math_round_F,          "round",          "(F)I")},
      {x(TR::java_lang_Math_round_D,          "round",          "(D)J")},
      {x(TR::java_lang_Math_scalb_F,          "scalb",          "(FI)F")},
      {x(TR::java_lang_Math_scalb_D,          "scalb",          "(DI)F")},
      {x(TR::java_lang_Math_sin,              "sin",            "(D)D")},
      {x(TR::java_lang_Math_sinh,             "sinh",           "(D)D")},
      {x(TR::java_lang_Math_sqrt,             "sqrt",           "(D)D")},
      {x(TR::java_lang_Math_tan,              "tan",            "(D)D")},
      {x(TR::java_lang_Math_tanh,             "tanh",           "(D)D")},
      {  TR::unknownMethod}
      };

   static X StrictMathMethods[] =
      {
      {x(TR::java_lang_StrictMath_acos,       "acos",            "(D)D")},
      {x(TR::java_lang_StrictMath_asin,       "asin",            "(D)D")},
      {x(TR::java_lang_StrictMath_atan,       "atan",            "(D)D")},
      {x(TR::java_lang_StrictMath_atan2,      "atan2",           "(DD)D")},
      {x(TR::java_lang_StrictMath_cbrt,       "cbrt",            "(D)D")},
      {x(TR::java_lang_StrictMath_ceil,       "ceil",            "(D)D")},
      {x(TR::java_lang_StrictMath_copySign_F, "copySign",       "(FF)F")},
      {x(TR::java_lang_StrictMath_copySign_D, "copySign",       "(DD)D")},
      {x(TR::java_lang_StrictMath_cos,        "cos",             "(D)D")},
      {x(TR::java_lang_StrictMath_cosh,       "cosh",            "(D)D")},
      {x(TR::java_lang_StrictMath_exp,        "exp",             "(D)D")},
      {x(TR::java_lang_StrictMath_expm1,      "expm1",           "(D)D")},
      {x(TR::java_lang_StrictMath_floor,      "floor",           "(D)D")},
      {x(TR::java_lang_StrictMath_hypot,      "hypot",           "(DD)D")},
      {x(TR::java_lang_StrictMath_IEEEremainder,"IEEEremainder", "(DD)D")},
      {x(TR::java_lang_StrictMath_log,        "log",             "(D)D")},
      {x(TR::java_lang_StrictMath_log10,      "log10",           "(D)D")},
      {x(TR::java_lang_StrictMath_log1p,      "log1p",           "(D)D")},
      {x(TR::java_lang_StrictMath_max_F,      "max",            "(FF)F")},
      {x(TR::java_lang_StrictMath_max_D,      "max",            "(DD)D")},
      {x(TR::java_lang_StrictMath_min_F,      "min",            "(FF)F")},
      {x(TR::java_lang_StrictMath_min_D,      "min",            "(DD)D")},
      {x(TR::java_lang_StrictMath_nextAfter_F,"nextAfter",      "(FD)F")},
      {x(TR::java_lang_StrictMath_nextAfter_D,"nextAfter",      "(DD)D")},
      {x(TR::java_lang_StrictMath_pow,        "pow",             "(DD)D")},
      {x(TR::java_lang_StrictMath_rint,       "rint",            "(D)D")},
      {x(TR::java_lang_StrictMath_round_F,    "round",          "(F)I")},
      {x(TR::java_lang_StrictMath_round_D,    "round",          "(D)J")},
      {x(TR::java_lang_StrictMath_scalb_F,    "scalb",          "(FI)F")},
      {x(TR::java_lang_StrictMath_scalb_D,    "scalb",          "(DI)F")},
      {x(TR::java_lang_StrictMath_sin,        "sin",             "(D)D")},
      {x(TR::java_lang_StrictMath_sinh,       "sinh",            "(D)D")},
      {x(TR::java_lang_StrictMath_sqrt,       "sqrt",            "(D)D")},
      {x(TR::java_lang_StrictMath_tan,        "tan",             "(D)D")},
      {x(TR::java_lang_StrictMath_tanh,       "tan",             "(D)D")},
      {  TR::unknownMethod}
      };

   static X ObjectMethods[] =
      {
      {TR::java_lang_Object_init,                 6,    "<init>", (int16_t)-1,    "*"},
      {x(TR::java_lang_Object_getClass,             "getClass",             "()Ljava/lang/Class;")},
      {x(TR::java_lang_Object_hashCodeImpl,         "hashCode",         "()I")},
      {x(TR::java_lang_Object_clone,                "clone",                "()Ljava/lang/Object;")},
      {x(TR::java_lang_Object_newInstancePrototype, "newInstancePrototype", "(Ljava/lang/Class;)Ljava/lang/Object;")},
      {x(TR::java_lang_Object_getAddressAsPrimitive, "getAddressAsPrimitive", "(Ljava/lang/Object;)I")},
      {  TR::unknownMethod}
      };

   static X XPCryptoMethods[] =
      {
      {x(TR::com_ibm_jit_crypto_JITAESCryptInHardware_isAESSupportedByHardwareImpl, "isAESSupportedByHardwareImpl", "()Z")},
      {x(TR::com_ibm_jit_crypto_JITAESCryptInHardware_doAESInHardware, "doAESInHardware", "([BII[BI[IIZ)Z")},
      {x(TR::com_ibm_jit_crypto_JITAESCryptInHardware_expandAESKeyInHardware, "expandAESKeyInHardware", "([B[II)Z")},
      {  TR::unknownMethod}
      };

   static X ZCryptoMethods[] =
      {
      {x(TR::com_ibm_jit_crypto_JITFullHardwareCrypt_z_km,   "z_km",   "([BII[BI[BI)V")},
      {x(TR::com_ibm_jit_crypto_JITFullHardwareCrypt_z_kmc,  "z_kmc",  "([BII[BI[BI)V")},
      {x(TR::com_ibm_jit_crypto_JITFullHardwareCrypt_z_kmo,  "z_kmo",  "([BII[BI[BI)V")},
      {x(TR::com_ibm_jit_crypto_JITFullHardwareCrypt_z_kmf,  "z_kmf",  "([BII[BI[BI)V")},
      {x(TR::com_ibm_jit_crypto_JITFullHardwareCrypt_z_kmctr,"z_kmctr","([BII[BI[BI[BI)V")},
      {x(TR::com_ibm_jit_crypto_JITFullHardwareCrypt_z_kmgcm,"z_kmgcm","([BII[BI[BII[BI)V")},
      {  TR::unknownMethod}
      };

   static X CryptoDigestMethods[] =
      {
      {x(TR::com_ibm_jit_crypto_JITFullHardwareDigest_z_kimd,"z_kimd", "([BII[BI)V")},
      {x(TR::com_ibm_jit_crypto_JITFullHardwareDigest_z_klmd,"z_klmd", "([BII[BI)V")},
      {x(TR::com_ibm_jit_crypto_JITFullHardwareDigest_z_kmac,"z_kmac", "([BII[BI)V")},
      {  TR::unknownMethod}
      };

   static X CryptoECC224Methods[] =
      {
      {x(TR::com_ibm_crypto_provider_P224PrimeField_multiply     ,"multiply", "([I[I[I)V")},
      {x(TR::com_ibm_crypto_provider_P224PrimeField_addNoMod     ,"addNoMod", "([I[I[I)Z")},
      {x(TR::com_ibm_crypto_provider_P224PrimeField_subNoMod     ,"subNoMod", "([I[I[I)Z")},
      {x(TR::com_ibm_crypto_provider_P224PrimeField_divideHelper ,"divideHelper", "([I[I)V")},
      {x(TR::com_ibm_crypto_provider_P224PrimeField_shiftRight   ,"shiftRight", "([I[II)V")},
      {x(TR::com_ibm_crypto_provider_P224PrimeField_mod          ,"mod", "([I[I)V")},
      {  TR::unknownMethod}
      };

   static X CryptoECC256Methods[] =
      {
      {x(TR::com_ibm_crypto_provider_P256PrimeField_multiply     ,"multiply", "([I[I[I)V")},
      {x(TR::com_ibm_crypto_provider_P256PrimeField_addNoMod     ,"addNoMod", "([I[I[I)Z")},
      {x(TR::com_ibm_crypto_provider_P256PrimeField_subNoMod     ,"subNoMod", "([I[I[I)Z")},
      {x(TR::com_ibm_crypto_provider_P256PrimeField_divideHelper ,"divideHelper", "([I[I)V")},
      {x(TR::com_ibm_crypto_provider_P256PrimeField_shiftRight   ,"shiftRight", "([I[II)V")},
      {x(TR::com_ibm_crypto_provider_P256PrimeField_mod          ,"mod", "([I[I)V")},
      {  TR::unknownMethod}
      };

   static X CryptoECC384Methods[] =
      {
      {x(TR::com_ibm_crypto_provider_P384PrimeField_multiply     ,"multiply", "([I[I[I)V")},
      {x(TR::com_ibm_crypto_provider_P384PrimeField_addNoMod     ,"addNoMod", "([I[I[I)Z")},
      {x(TR::com_ibm_crypto_provider_P384PrimeField_subNoMod     ,"subNoMod", "([I[I[I)Z")},
      {x(TR::com_ibm_crypto_provider_P384PrimeField_divideHelper ,"divideHelper", "([I[I)V")},
      {x(TR::com_ibm_crypto_provider_P384PrimeField_shiftRight   ,"shiftRight", "([I[II)V")},
      {x(TR::com_ibm_crypto_provider_P384PrimeField_mod          ,"mod", "([I[I)V")},
      {  TR::unknownMethod}
      };

   static X CryptoAESMethods[] =
     {
     {x(TR::com_ibm_crypto_provider_AEScryptInHardware_cbcDecrypt ,"cbcDecrypt", "([BI[BII[B[I)V")},
     {x(TR::com_ibm_crypto_provider_AEScryptInHardware_cbcEncrypt ,"cbcEncrypt", "([BI[BII[B[I)V")},
     {  TR::unknownMethod}
     };

   static X DataAccessByteArrayMarshallerMethods[] =
   {
      {x(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeShort , "writeShort" , "(S[BIZ)V")},
      {x(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeShort_, "writeShort_", "(S[BIZ)V")},

      {x(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeShortLength , "writeShort" , "(S[BIZI)V")},
      {x(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeShortLength_, "writeShort_", "(S[BIZI)V")},

      {x(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeInt , "writeInt" , "(I[BIZ)V")},
      {x(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeInt_, "writeInt_", "(I[BIZ)V")},

      {x(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeIntLength , "writeInt" , "(I[BIZI)V")},
      {x(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeIntLength_, "writeInt_", "(I[BIZI)V")},

      {x(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeLong , "writeLong" , "(J[BIZ)V")},
      {x(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeLong_, "writeLong_", "(J[BIZ)V")},

      {x(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeLongLength , "writeLong" , "(J[BIZI)V")},
      {x(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeLongLength_, "writeLong_", "(J[BIZI)V")},

      {x(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeFloat , "writeFloat" , "(F[BIZ)V")},
      {x(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeFloat_, "writeFloat_", "(F[BIZ)V")},

      {x(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeDouble , "writeDouble" , "(D[BIZ)V")},
      {x(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeDouble_, "writeDouble_", "(D[BIZ)V")},

      {TR::unknownMethod}
   };

   static X DataAccessByteArrayUnmarshallerMethods[] =
   {
      {x(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readShort , "readShort" , "([BIZ)S")},
      {x(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readShort_, "readShort_", "([BIZ)S")},

      {x(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readShortLength , "readShort" , "([BIZIZ)S")},
      {x(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readShortLength_, "readShort_", "([BIZIZ)S")},

      {x(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readInt , "readInt" , "([BIZ)I")},
      {x(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readInt_, "readInt_", "([BIZ)I")},

      {x(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readIntLength , "readInt" , "([BIZIZ)I")},
      {x(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readIntLength_, "readInt_", "([BIZIZ)I")},

      {x(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readLong , "readLong" , "([BIZ)J")},
      {x(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readLong_, "readLong_", "([BIZ)J")},

      {x(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readLongLength , "readLong" , "([BIZIZ)J")},
      {x(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readLongLength_, "readLong_", "([BIZIZ)J")},

      {x(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readFloat , "readFloat" , "([BIZ)F")},
      {x(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readFloat_, "readFloat_", "([BIZ)F")},

      {x(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readDouble , "readDouble" , "([BIZ)D")},
      {x(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readDouble_, "readDouble_", "([BIZ)D")},

      { TR::unknownMethod}
   };

   static X DataAccessByteArrayUtilMethods[] =
      {
       {x(TR::com_ibm_dataaccess_ByteArrayUtils_trailingZeros,           "trailingZeros"        ,    "([B)I")},
       {x(TR::com_ibm_dataaccess_ByteArrayUtils_trailingZerosByteAtATime,"trailingZerosByteAtATime", "([BII)I")},
       {x(TR::com_ibm_dataaccess_ByteArrayUtils_trailingZerosQuadWordAtATime_,   "trailingZerosQuadWordAtATime_", "([BI)I")},
       { TR::unknownMethod}
      };

   static X DataAccessDecimalDataMethods[] =
   {
      {x(TR::com_ibm_dataaccess_DecimalData_JITIntrinsicsEnabled, "JITIntrinsicsEnabled", "()Z")},

      {x(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToInteger , "convertPackedDecimalToInteger" , "([BIIZ)I")},
      {x(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToInteger_, "convertPackedDecimalToInteger_", "([BIIZ)I")},
      {x(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToInteger_ByteBuffer , "convertPackedDecimalToInteger" , "(Ljava/nio/ByteBuffer;IIZ)I")},
      {x(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToInteger_ByteBuffer_, "convertPackedDecimalToInteger_", "(Ljava/nio/ByteBuffer;IIZJII)I")},

      {x(TR::com_ibm_dataaccess_DecimalData_convertIntegerToPackedDecimal, "convertIntegerToPackedDecimal" , "(I[BIIZ)V")},
      {x(TR::com_ibm_dataaccess_DecimalData_convertIntegerToPackedDecimal_,"convertIntegerToPackedDecimal_", "(I[BIIZ)V")},
      {x(TR::com_ibm_dataaccess_DecimalData_convertIntegerToPackedDecimal_ByteBuffer , "convertIntegerToPackedDecimal" , "(ILjava/nio/ByteBuffer;IIZ)V")},
      {x(TR::com_ibm_dataaccess_DecimalData_convertIntegerToPackedDecimal_ByteBuffer_, "convertIntegerToPackedDecimal_", "(ILjava/nio/ByteBuffer;IIZJII)V")},

      {x(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToLong , "convertPackedDecimalToLong" , "([BIIZ)J")},
      {x(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToLong_, "convertPackedDecimalToLong_", "([BIIZ)J")},
      {x(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToLong_ByteBuffer , "convertPackedDecimalToLong" , "(Ljava/nio/ByteBuffer;IIZ)J")},
      {x(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToLong_ByteBuffer_, "convertPackedDecimalToLong_", "(Ljava/nio/ByteBuffer;IIZJII)J")},

      {x(TR::com_ibm_dataaccess_DecimalData_convertLongToPackedDecimal , "convertLongToPackedDecimal" , "(J[BIIZ)V")},
      {x(TR::com_ibm_dataaccess_DecimalData_convertLongToPackedDecimal_, "convertLongToPackedDecimal_", "(J[BIIZ)V")},
      {x(TR::com_ibm_dataaccess_DecimalData_convertLongToPackedDecimal_ByteBuffer , "convertLongToPackedDecimal" , "(JLjava/nio/ByteBuffer;IIZ)V")},
      {x(TR::com_ibm_dataaccess_DecimalData_convertLongToPackedDecimal_ByteBuffer_, "convertLongToPackedDecimal_", "(JLjava/nio/ByteBuffer;IIZJII)V")},


      {x(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToInteger , "convertExternalDecimalToInteger" , "([BIIZI)I")},
      {x(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToInteger_, "convertExternalDecimalToInteger_", "([BIIZI)I")},

      {x(TR::com_ibm_dataaccess_DecimalData_convertIntegerToExternalDecimal , "convertIntegerToExternalDecimal" , "(I[BIIZI)V")},
      {x(TR::com_ibm_dataaccess_DecimalData_convertIntegerToExternalDecimal_, "convertIntegerToExternalDecimal_", "(I[BIIZI)V")},

      {x(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToLong , "convertExternalDecimalToLong" , "([BIIZI)J")},
      {x(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToLong_, "convertExternalDecimalToLong_", "([BIIZI)J")},

      {x(TR::com_ibm_dataaccess_DecimalData_convertLongToExternalDecimal , "convertLongToExternalDecimal" , "(J[BIIZI)V")},
      {x(TR::com_ibm_dataaccess_DecimalData_convertLongToExternalDecimal_, "convertLongToExternalDecimal_", "(J[BIIZI)V")},


      {x(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToExternalDecimal , "convertPackedDecimalToExternalDecimal" , "([BI[BIII)V")},
      {x(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToExternalDecimal_, "convertPackedDecimalToExternalDecimal_", "([BI[BIII)V")},

      {x(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToPackedDecimal , "convertExternalDecimalToPackedDecimal" , "([BI[BIII)V")},
      {x(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToPackedDecimal_, "convertExternalDecimalToPackedDecimal_", "([BI[BIII)V")},

      {x(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToUnicodeDecimal , "convertPackedDecimalToUnicodeDecimal" , "([BI[CIII)V")},
      {x(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToUnicodeDecimal_, "convertPackedDecimalToUnicodeDecimal_", "([BI[CIII)V")},

      {x(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToPackedDecimal , "convertUnicodeDecimalToPackedDecimal" , "([CI[BIII)V")},
      {x(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToPackedDecimal_, "convertUnicodeDecimalToPackedDecimal_", "([CI[BIII)V")},


      {x(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToBigInteger, "convertPackedDecimalToBigInteger", "([BIIZ)Ljava/math/BigInteger;")},
      {x(TR::com_ibm_dataaccess_DecimalData_convertBigIntegerToPackedDecimal, "convertBigIntegerToPackedDecimal", "(Ljava/math/BigInteger;[BIIZ)V")},

      {x(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToBigDecimal, "convertPackedDecimalToBigDecimal", "([BIIIZ)Ljava/math/BigDecimal;")},
      {x(TR::com_ibm_dataaccess_DecimalData_convertBigDecimalToPackedDecimal, "convertBigDecimalToPackedDecimal", "(Ljava/math/BigDecimal;[BIIZ)V")},


      {x(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToBigInteger, "convertExternalDecimalToBigInteger", "([BIIZI)Ljava/math/BigInteger;")},
      {x(TR::com_ibm_dataaccess_DecimalData_convertBigIntegerToExternalDecimal, "convertBigIntegerToExternalDecimal", "(Ljava/math/BigInteger;[BIIZI)V")},

      {x(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToBigDecimal, "convertExternalDecimalToBigDecimal", "([BIIIZI)Ljava/math/BigDecimal;")},
      {x(TR::com_ibm_dataaccess_DecimalData_convertBigDecimalToExternalDecimal, "convertBigDecimalToExternalDecimal", "(Ljava/math/BigDecimal;[BIIZI)V")},


      {x(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToInteger, "convertUnicodeDecimalToInteger", "([CIIZI)I")},
      {x(TR::com_ibm_dataaccess_DecimalData_convertIntegerToUnicodeDecimal, "convertIntegerToUnicodeDecimal", "(I[CIIZI)V")},

      {x(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToLong, "convertUnicodeDecimalToLong", "([CIIZI)J")},
      {x(TR::com_ibm_dataaccess_DecimalData_convertLongToUnicodeDecimal, "convertLongToUnicodeDecimal", "(J[CIIZI)V")},

      {x(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToBigInteger, "convertUnicodeDecimalToBigInteger", "([CIIZI)Ljava/math/BigInteger;")},
      {x(TR::com_ibm_dataaccess_DecimalData_convertBigIntegerToUnicodeDecimal, "convertBigIntegerToUnicodeDecimal", "(Ljava/math/BigInteger;[CIIZI)V")},

      {x(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToBigDecimal, "convertUnicodeDecimalToBigDecimal", "([CIIIZI)Ljava/math/BigDecimal;")},
      {x(TR::com_ibm_dataaccess_DecimalData_convertBigDecimalToUnicodeDecimal, "convertBigDecimalToUnicodeDecimal", "(Ljava/math/BigDecimal;[CIIZI)V")},


      {x(TR::com_ibm_dataaccess_DecimalData_translateArray, "translateArray",   "([BII[BI[BII)V")},


      {x(TR::com_ibm_dataaccess_DecimalData_DFPFacilityAvailable,  "DFPFacilityAvailable",        "()Z")},
      {x(TR::com_ibm_dataaccess_DecimalData_DFPUseDFP,             "DFPUseDFP",             "()Z")},
      {x(TR::com_ibm_dataaccess_DecimalData_DFPConvertPackedToDFP, "DFPConvertPackedToDFP", "(Ljava/math/BigDecimal;JIZ)Z")},
      {x(TR::com_ibm_dataaccess_DecimalData_DFPConvertDFPToPacked, "DFPConvertDFPToPacked", "(JZ)J")},
      {x(TR::com_ibm_dataaccess_DecimalData_createZeroBigDecimal, "createZeroBigDecimal", "()Ljava/math/BigDecimal;")},
      {x(TR::com_ibm_dataaccess_DecimalData_getlaside, "getlaside", "(Ljava/math/BigDecimal;)J")},
      {x(TR::com_ibm_dataaccess_DecimalData_setlaside, "setlaside", "(Ljava/math/BigDecimal;J)V")},
      {x(TR::com_ibm_dataaccess_DecimalData_getflags, "getflags", "(Ljava/math/BigDecimal;)I")},
      {x(TR::com_ibm_dataaccess_DecimalData_setflags, "setflags", "(Ljava/math/BigDecimal;I)V")},
      {x(TR::com_ibm_dataaccess_DecimalData_slowSignedPackedToBigDecimal, "slowSignedPackedToBigDecimal", "([BIIIZ)Ljava/math/BigDecimal;")},
      {x(TR::com_ibm_dataaccess_DecimalData_slowBigDecimalToSignedPacked, "slowBigDecimalToSignedPacked", "(Ljava/math/BigDecimal;[BIIZ)V")},

      {TR::unknownMethod}
   };

   static X DataAccessPackedDecimalMethods[] =
   {
      {x(TR::com_ibm_dataaccess_PackedDecimal_checkPackedDecimal , "checkPackedDecimal" , "([BIIZZ)I")},
      {x(TR::com_ibm_dataaccess_PackedDecimal_checkPackedDecimal_, "checkPackedDecimal_", "([BIIZZ)I")},

      {x(TR::com_ibm_dataaccess_PackedDecimal_checkPackedDecimal_2bInlined1, "checkPackedDecimal", "([BII)I"  )},
      {x(TR::com_ibm_dataaccess_PackedDecimal_checkPackedDecimal_2bInlined2, "checkPackedDecimal", "([BIIZ)I" )},

      {x(TR::com_ibm_dataaccess_PackedDecimal_addPackedDecimal , "addPackedDecimal" , "([BII[BII[BIIZ)V")},
      {x(TR::com_ibm_dataaccess_PackedDecimal_addPackedDecimal_, "addPackedDecimal_", "([BII[BII[BIIZ)V")},

      {x(TR::com_ibm_dataaccess_PackedDecimal_subtractPackedDecimal , "subtractPackedDecimal" , "([BII[BII[BIIZ)V")},
      {x(TR::com_ibm_dataaccess_PackedDecimal_subtractPackedDecimal_, "subtractPackedDecimal_", "([BII[BII[BIIZ)V")},

      {x(TR::com_ibm_dataaccess_PackedDecimal_multiplyPackedDecimal , "multiplyPackedDecimal" , "([BII[BII[BIIZ)V")},
      {x(TR::com_ibm_dataaccess_PackedDecimal_multiplyPackedDecimal_, "multiplyPackedDecimal_", "([BII[BII[BIIZ)V")},

      {x(TR::com_ibm_dataaccess_PackedDecimal_dividePackedDecimal , "dividePackedDecimal" , "([BII[BII[BIIZ)V")},
      {x(TR::com_ibm_dataaccess_PackedDecimal_dividePackedDecimal_, "dividePackedDecimal_", "([BII[BII[BIIZ)V")},

      {x(TR::com_ibm_dataaccess_PackedDecimal_remainderPackedDecimal , "remainderPackedDecimal" , "([BII[BII[BIIZ)V")},
      {x(TR::com_ibm_dataaccess_PackedDecimal_remainderPackedDecimal_, "remainderPackedDecimal_", "([BII[BII[BIIZ)V")},


      {x(TR::com_ibm_dataaccess_PackedDecimal_lessThanPackedDecimal , "lessThanPackedDecimal" , "([BII[BII)Z")},
      {x(TR::com_ibm_dataaccess_PackedDecimal_lessThanPackedDecimal_, "lessThanPackedDecimal_", "([BII[BII)Z")},

      {x(TR::com_ibm_dataaccess_PackedDecimal_lessThanOrEqualsPackedDecimal , "lessThanOrEqualsPackedDecimal" , "([BII[BII)Z")},
      {x(TR::com_ibm_dataaccess_PackedDecimal_lessThanOrEqualsPackedDecimal_, "lessThanOrEqualsPackedDecimal_", "([BII[BII)Z")},

      {x(TR::com_ibm_dataaccess_PackedDecimal_greaterThanPackedDecimal , "greaterThanPackedDecimal" , "([BII[BII)Z")},
      {x(TR::com_ibm_dataaccess_PackedDecimal_greaterThanPackedDecimal_, "greaterThanPackedDecimal_", "([BII[BII)Z")},

      {x(TR::com_ibm_dataaccess_PackedDecimal_greaterThanOrEqualsPackedDecimal , "greaterThanOrEqualsPackedDecimal" , "([BII[BII)Z")},
      {x(TR::com_ibm_dataaccess_PackedDecimal_greaterThanOrEqualsPackedDecimal_, "greaterThanOrEqualsPackedDecimal_", "([BII[BII)Z")},

      {x(TR::com_ibm_dataaccess_PackedDecimal_equalsPackedDecimal , "equalsPackedDecimal" , "([BII[BII)Z")},
      {x(TR::com_ibm_dataaccess_PackedDecimal_equalsPackedDecimal_, "equalsPackedDecimal_", "([BII[BII)Z")},

      {x(TR::com_ibm_dataaccess_PackedDecimal_notEqualsPackedDecimal , "notEqualsPackedDecimal" , "([BII[BII)Z")},
      {x(TR::com_ibm_dataaccess_PackedDecimal_notEqualsPackedDecimal_, "notEqualsPackedDecimal_", "([BII[BII)Z")},


      {x(TR::com_ibm_dataaccess_PackedDecimal_shiftLeftPackedDecimal , "shiftLeftPackedDecimal" , "([BII[BIIIZ)V")},
      {x(TR::com_ibm_dataaccess_PackedDecimal_shiftLeftPackedDecimal_, "shiftLeftPackedDecimal_", "([BII[BIIIZ)V")},

      {x(TR::com_ibm_dataaccess_PackedDecimal_shiftRightPackedDecimal , "shiftRightPackedDecimal" , "([BII[BIIIZZ)V")},
      {x(TR::com_ibm_dataaccess_PackedDecimal_shiftRightPackedDecimal_, "shiftRightPackedDecimal_", "([BII[BIIIZZ)V")},

      {x(TR::com_ibm_dataaccess_PackedDecimal_movePackedDecimal, "movePackedDecimal" , "([BII[BIIZ)V")},

      {TR::unknownMethod}
   };

   static X BigDecimalMethods[] =
      {
      {x(TR::java_math_BigDecimal_add,                   "add",                   "(Ljava/math/BigDecimal;)Ljava/math/BigDecimal;")},
      {x(TR::java_math_BigDecimal_subtract,              "subtract",              "(Ljava/math/BigDecimal;)Ljava/math/BigDecimal;")},
      {x(TR::java_math_BigDecimal_multiply,              "multiply",              "(Ljava/math/BigDecimal;)Ljava/math/BigDecimal;")},
      {x(TR::java_math_BigDecimal_clone,                 "clone",                 "()Ljava/math/BigDecimal;")},
      {x(TR::java_math_BigDecimal_possibleClone,         "possibleClone",          "(Ljava/math/BigDecimal;)Ljava/math/BigDecimal;")},
      {x(TR::java_math_BigDecimal_valueOf,               "valueOf",               "(JI)Ljava/math/BigDecimal;")},
      {x(TR::java_math_BigDecimal_valueOf_J,             "valueOf",               "(J)Ljava/math/BigDecimal;")},
      {x(TR::java_math_BigDecimal_setScale,              "setScale",              "(II)Ljava/math/BigDecimal;")},
      {x(TR::java_math_BigDecimal_slowSubMulAddAddMulSetScale, "slowSMAAMSS",     "(Ljava/math/BigDecimal;Ljava/math/BigDecimal;Ljava/math/BigDecimal;Ljava/math/BigDecimal;Ljava/math/BigDecimal;)Ljava/math/BigDecimal;")},
      {x(TR::java_math_BigDecimal_slowSubMulSetScale,    "slowSMSS",              "(Ljava/math/BigDecimal;Ljava/math/BigDecimal;Ljava/math/BigDecimal;)Ljava/math/BigDecimal;")},
      {x(TR::java_math_BigDecimal_slowAddAddMulSetScale, "slowAAMSS",             "(Ljava/math/BigDecimal;Ljava/math/BigDecimal;Ljava/math/BigDecimal;Ljava/math/BigDecimal;)Ljava/math/BigDecimal;")},
      {x(TR::java_math_BigDecimal_slowMulSetScale,       "slowMSS",               "(Ljava/math/BigDecimal;Ljava/math/BigDecimal;)Ljava/math/BigDecimal;")},
      {x(TR::java_math_BigDecimal_subMulAddAddMulSetScale, "SMAAMSS",             "(Ljava/math/BigDecimal;Ljava/math/BigDecimal;Ljava/math/BigDecimal;Ljava/math/BigDecimal;Ljava/math/BigDecimal;IIII)Ljava/math/BigDecimal;")},
      {x(TR::java_math_BigDecimal_subMulSetScale,        "SMSS",                  "(Ljava/math/BigDecimal;Ljava/math/BigDecimal;Ljava/math/BigDecimal;II)Ljava/math/BigDecimal;")},
      {x(TR::java_math_BigDecimal_addAddMulSetScale,     "AAMSS",                 "(Ljava/math/BigDecimal;Ljava/math/BigDecimal;Ljava/math/BigDecimal;Ljava/math/BigDecimal;III)Ljava/math/BigDecimal;")},
      {x(TR::java_math_BigDecimal_mulSetScale,           "MSS",                   "(Ljava/math/BigDecimal;Ljava/math/BigDecimal;I)Ljava/math/BigDecimal;")},
      {x(TR::java_math_BigDecimal_longString1,           "longString1",           "(II)Ljava/lang/String;")},
      {x(TR::java_math_BigDecimal_longString1C,          "longString1",           "(II[C)V")},
      {x(TR::java_math_BigDecimal_longString2,           "longString2",           "(III)Ljava/lang/String;")},
      {x(TR::java_math_BigDecimal_toString,              "toString",              "()Ljava/lang/String;")},
      {x(TR::java_math_BigDecimal_doToString,            "doToString",            "()Ljava/lang/String;")},
      {x(TR::java_math_BigDecimal_noLLOverflowAdd,       "noLLOverflowAdd",       "(JJJ)Z")},
      {x(TR::java_math_BigDecimal_noLLOverflowMul,       "noLLOverflowMul",       "(JJJ)Z")},
      {x(TR::java_math_BigDecimal_longAdd,               "longAdd",               "(Ljava/math/BigDecimal;Ljava/math/BigDecimal;Ljava/math/MathContext;Z)Ljava/math/BigDecimal;")},
      {x(TR::java_math_BigDecimal_slAdd,                  "slAdd",                 "(Ljava/math/BigDecimal;Ljava/math/BigDecimal;Ljava/math/MathContext;Z)Ljava/math/BigDecimal;")},
      {x(TR::java_math_BigDecimal_getLaside,             "getLaside",             "()J")},
      {x(TR::java_math_BigDecimal_DFPPerformHysteresis,  "DFPPerformHysteresis",  "()Z")},
      {x(TR::java_math_BigDecimal_DFPUseDFP,             "DFPUseDFP",             "()Z")},
      {x(TR::java_math_BigDecimal_DFPHWAvailable,        "DFPHWAvailable",        "()Z")},
      {x(TR::java_math_BigDecimal_DFPIntConstructor,     "DFPIntConstructor",     "(IIII)Z")},
      {x(TR::java_math_BigDecimal_DFPLongConstructor,    "DFPLongConstructor",    "(JIII)Z")},
      {x(TR::java_math_BigDecimal_DFPLongExpConstructor, "DFPLongExpConstructor", "(JIIIIZ)Z")},
      {x(TR::java_math_BigDecimal_DFPAdd,                "DFPAdd",                "(JJIII)Z")},
      {x(TR::java_math_BigDecimal_DFPSubtract,           "DFPSubtract",           "(JJIII)Z")},
      {x(TR::java_math_BigDecimal_DFPMultiply,           "DFPMultiply",           "(JJIII)Z")},
      {x(TR::java_math_BigDecimal_DFPDivide,             "DFPDivide",             "(JJZIII)I")},
      {x(TR::java_math_BigDecimal_DFPScaledAdd,          "DFPScaledAdd",          "(JJI)Z")},
      {x(TR::java_math_BigDecimal_DFPScaledSubtract,     "DFPScaledSubtract",     "(JJI)Z")},
      {x(TR::java_math_BigDecimal_DFPScaledMultiply,     "DFPScaledMultiply",     "(JJI)Z")},
      {x(TR::java_math_BigDecimal_DFPScaledDivide,       "DFPScaledDivide",       "(JJIII)I")},
      {x(TR::java_math_BigDecimal_DFPRound,              "DFPRound",              "(JII)Z")},
      {x(TR::java_math_BigDecimal_DFPSetScale,           "DFPSetScale",           "(JIZIZ)I")},
      {x(TR::java_math_BigDecimal_DFPCompareTo,          "DFPCompareTo",          "(JJ)I")},
      {x(TR::java_math_BigDecimal_DFPSignificance,       "DFPSignificance",       "(J)I")},
      {x(TR::java_math_BigDecimal_DFPExponent,           "DFPExponent",           "(J)I")},
      {x(TR::java_math_BigDecimal_DFPBCDDigits,          "DFPBCDDigits",          "(J)J")},
      {x(TR::java_math_BigDecimal_DFPUnscaledValue,      "DFPUnscaledValue",      "(J)J")},
      {x(TR::java_math_BigDecimal_DFPConvertPackedToDFP, "DFPConvertPackedToDFP", "(JIZ)Z")},
      {x(TR::java_math_BigDecimal_DFPConvertDFPToPacked, "DFPConvertDFPToPacked", "(JZ)J")},
      {x(TR::java_math_BigDecimal_DFPGetHWAvailable,     "DFPGetHWAvailable",     "()Z")},
      {x(TR::java_math_BigDecimal_doubleValue,     "doubleValue",     "()D")},
      {x(TR::java_math_BigDecimal_floatValue,     "floatValue",       "()F")},
      {x(TR::java_math_BigDecimal_storeTwoCharsFromInt,  "storeTwoCharsFromInt",  "([CII)V")},
      {    TR::unknownMethod}
      };

   static X BigIntegerMethods[] =
      {
      {x(TR::java_math_BigInteger_add,                   "add",                   "(Ljava/math/BigInteger;)Ljava/math/BigInteger;")},
      {x(TR::java_math_BigInteger_subtract,              "subtract",              "(Ljava/math/BigInteger;)Ljava/math/BigInteger;")},
      {x(TR::java_math_BigInteger_multiply,              "multiply",              "(Ljava/math/BigInteger;)Ljava/math/BigInteger;")},
      {    TR::unknownMethod}
      };

   static X PrefetchMethods[] =
      {
      {x(TR::com_ibm_Compiler_Internal__TR_Prefetch,     "_TR_Prefetch_Load",              "(Ljava/lang/Object;I)V")},
      {x(TR::com_ibm_Compiler_Internal__TR_Prefetch,     "_TR_Prefetch_Load_L1",           "(Ljava/lang/Object;I)V")},
      {x(TR::com_ibm_Compiler_Internal__TR_Prefetch,     "_TR_Prefetch_Load_L2",           "(Ljava/lang/Object;I)V")},
      {x(TR::com_ibm_Compiler_Internal__TR_Prefetch,     "_TR_Prefetch_Load_L3",           "(Ljava/lang/Object;I)V")},
      {x(TR::com_ibm_Compiler_Internal__TR_Prefetch,     "_TR_Prefetch_Store",             "(Ljava/lang/Object;I)V")},
      {x(TR::com_ibm_Compiler_Internal__TR_Prefetch,     "_TR_Prefetch_LoadNTA",           "(Ljava/lang/Object;I)V")},
      {x(TR::com_ibm_Compiler_Internal__TR_Prefetch,     "_TR_Prefetch_StoreNTA",          "(Ljava/lang/Object;I)V")},
      {x(TR::com_ibm_Compiler_Internal__TR_Prefetch,     "_TR_Prefetch_StoreConditional",  "(Ljava/lang/Object;I)V")},
      {x(TR::com_ibm_Compiler_Internal__TR_Prefetch,     "_TR_Release_StoreOnly",          "(Ljava/lang/Object;I)V")},
      {x(TR::com_ibm_Compiler_Internal__TR_Prefetch,     "_TR_Release_All",                "(Ljava/lang/Object;I)V")},
      {x(TR::com_ibm_Compiler_Internal__TR_Prefetch,     "_TR_Prefetch_Load",              "(Ljava/lang/Object;Ljava/lang/String;Ljava/lang/String;)V")},
      {x(TR::com_ibm_Compiler_Internal__TR_Prefetch,     "_TR_Prefetch_Load_L1",           "(Ljava/lang/Object;Ljava/lang/String;Ljava/lang/String;)V")},
      {x(TR::com_ibm_Compiler_Internal__TR_Prefetch,     "_TR_Prefetch_Load_L2",           "(Ljava/lang/Object;Ljava/lang/String;Ljava/lang/String;)V")},
      {x(TR::com_ibm_Compiler_Internal__TR_Prefetch,     "_TR_Prefetch_Load_L3",           "(Ljava/lang/Object;Ljava/lang/String;Ljava/lang/String;)V")},
      {x(TR::com_ibm_Compiler_Internal__TR_Prefetch,     "_TR_Prefetch_Store",             "(Ljava/lang/Object;Ljava/lang/String;Ljava/lang/String;)V")},
      {x(TR::com_ibm_Compiler_Internal__TR_Prefetch,     "_TR_Prefetch_LoadNTA",           "(Ljava/lang/Object;Ljava/lang/String;Ljava/lang/String;)V")},
      {x(TR::com_ibm_Compiler_Internal__TR_Prefetch,     "_TR_Prefetch_StoreNTA",          "(Ljava/lang/Object;Ljava/lang/String;Ljava/lang/String;)V")},
      {x(TR::com_ibm_Compiler_Internal__TR_Prefetch,     "_TR_Prefetch_StoreConditional",  "(Ljava/lang/Object;Ljava/lang/String;Ljava/lang/String;)V")},
      {x(TR::com_ibm_Compiler_Internal__TR_Prefetch,     "_TR_Release_StoreOnly",          "(Ljava/lang/Object;Ljava/lang/String;Ljava/lang/String;)V")},
      {x(TR::com_ibm_Compiler_Internal__TR_Prefetch,     "_TR_Release_All",                "(Ljava/lang/Object;Ljava/lang/String;Ljava/lang/String;)V")},
      {x(TR::com_ibm_Compiler_Internal__TR_Prefetch,     "_TR_Prefetch_Load",              "(Ljava/lang/String;Ljava/lang/String;)V")},
      {x(TR::com_ibm_Compiler_Internal__TR_Prefetch,     "_TR_Prefetch_Load_L1",           "(Ljava/lang/String;Ljava/lang/String;)V")},
      {x(TR::com_ibm_Compiler_Internal__TR_Prefetch,     "_TR_Prefetch_Load_L2",           "(Ljava/lang/String;Ljava/lang/String;)V")},
      {x(TR::com_ibm_Compiler_Internal__TR_Prefetch,     "_TR_Prefetch_Load_L3",           "(Ljava/lang/String;Ljava/lang/String;)V")},
      {x(TR::com_ibm_Compiler_Internal__TR_Prefetch,     "_TR_Prefetch_Store",             "(Ljava/lang/String;Ljava/lang/String;)V")},
      {x(TR::com_ibm_Compiler_Internal__TR_Prefetch,     "_TR_Prefetch_LoadNTA",           "(Ljava/lang/String;Ljava/lang/String;)V")},
      {x(TR::com_ibm_Compiler_Internal__TR_Prefetch,     "_TR_Prefetch_StoreNTA",          "(Ljava/lang/String;Ljava/lang/String;)V")},
      {x(TR::com_ibm_Compiler_Internal__TR_Prefetch,     "_TR_Prefetch_StoreConditional",  "(Ljava/lang/String;Ljava/lang/String;)V")},
      {x(TR::com_ibm_Compiler_Internal__TR_Prefetch,     "_TR_Release_StoreOnly",          "(Ljava/lang/String;Ljava/lang/String;)V")},
      {x(TR::com_ibm_Compiler_Internal__TR_Prefetch,     "_TR_Release_All",                "(Ljava/lang/String;Ljava/lang/String;)V")},
      {    TR::unknownMethod}
      };

   static X OSMemoryMethods[] =
      {
      {x(TR::org_apache_harmony_luni_platform_OSMemory_putByte_JB_V,                  "putByte",    "(JB)V")},
      {x(TR::org_apache_harmony_luni_platform_OSMemory_putShort_JS_V,                 "putShort",   "(JS)V")},
      {x(TR::org_apache_harmony_luni_platform_OSMemory_putInt_JI_V,                   "putInt",     "(JI)V")},
      {x(TR::org_apache_harmony_luni_platform_OSMemory_putLong_JJ_V,                  "putLong",    "(JJ)V")},
      {x(TR::org_apache_harmony_luni_platform_OSMemory_putFloat_JF_V,                 "putFloat",   "(JF)V")},
      {x(TR::org_apache_harmony_luni_platform_OSMemory_putDouble_JD_V,                "putDouble",  "(JD)V")},
      {x(TR::org_apache_harmony_luni_platform_OSMemory_putAddress_JJ_V,               "putAddress", "(JJ)V")},

      {x(TR::org_apache_harmony_luni_platform_OSMemory_getByte_J_B,                   "getByte",    "(J)B")},
      {x(TR::org_apache_harmony_luni_platform_OSMemory_getShort_J_S,                  "getShort",   "(J)S")},
      {x(TR::org_apache_harmony_luni_platform_OSMemory_getInt_J_I,                    "getInt",     "(J)I")},
      {x(TR::org_apache_harmony_luni_platform_OSMemory_getLong_J_J,                   "getLong",    "(J)J")},
      {x(TR::org_apache_harmony_luni_platform_OSMemory_getFloat_J_F,                  "getFloat",   "(J)F")},
      {x(TR::org_apache_harmony_luni_platform_OSMemory_getDouble_J_D,                 "getDouble",  "(J)D")},
      {x(TR::org_apache_harmony_luni_platform_OSMemory_getAddress_J_J,                "getAddress", "(J)J")},
      {  TR::unknownMethod}
      };

   static X otivmMethods[] =
      {
      {  TR::com_ibm_oti_vm_VM_callerClass,      11, "callerClass",  (int16_t)-1, "*"},
      {x(TR::java_lang_ClassLoader_callerClassLoader, "callerClassLoader", "()Ljava/lang/ClassLoader;")},
      {  TR::unknownMethod}
      };

   static X ArraysMethods[] =
      {
      {  TR::java_util_Arrays_fill,      4, "fill",    (int16_t)-1, "*"},
      {  TR::java_util_Arrays_equals,    6, "equals",  (int16_t)-1, "*"},
      {x(TR::java_util_Arrays_copyOf_byte,   "copyOf",     "([BI)[B")},
      {x(TR::java_util_Arrays_copyOf_short,  "copyOf",     "([SI)[S")},
      {x(TR::java_util_Arrays_copyOf_char,   "copyOf",     "([CI)[C")},
      {x(TR::java_util_Arrays_copyOf_boolean,"copyOf",     "([ZI)[Z")},
      {x(TR::java_util_Arrays_copyOf_int,    "copyOf",     "([II)[I")},
      {x(TR::java_util_Arrays_copyOf_long,   "copyOf",     "([JI)[J")},
      {x(TR::java_util_Arrays_copyOf_float,  "copyOf",     "([FI)[F")},
      {x(TR::java_util_Arrays_copyOf_double, "copyOf",     "([DI)[D")},
      {x(TR::java_util_Arrays_copyOf_Object1,"copyOf",     "([Ljava/lang/Object;I)[Ljava/lang/Object;")},
      {x(TR::java_util_Arrays_copyOf_Object2,"copyOf",     "([Ljava/lang/Object;ILjava/lang/Class;)[Ljava/lang/Object;")},

      {x(TR::java_util_Arrays_copyOfRange_byte,   "copyOfRange",     "([BII)[B")},
      {x(TR::java_util_Arrays_copyOfRange_short,  "copyOfRange",     "([SII)[S")},
      {x(TR::java_util_Arrays_copyOfRange_char,   "copyOfRange",     "([CII)[C")},
      {x(TR::java_util_Arrays_copyOfRange_boolean,"copyOfRange",     "([ZII)[Z")},
      {x(TR::java_util_Arrays_copyOfRange_int,    "copyOfRange",     "([III)[I")},
      {x(TR::java_util_Arrays_copyOfRange_long,   "copyOfRange",     "([JII)[J")},
      {x(TR::java_util_Arrays_copyOfRange_float,  "copyOfRange",     "([FII)[F")},
      {x(TR::java_util_Arrays_copyOfRange_double, "copyOfRange",     "([DII)[D")},
      {x(TR::java_util_Arrays_copyOfRange_Object1,"copyOfRange",     "([Ljava/lang/Object;II)[Ljava/lang/Object;")},
      {x(TR::java_util_Arrays_copyOfRange_Object2,"copyOfRange",     "([Ljava/lang/Object;IILjava/lang/Class;)[Ljava/lang/Object;")},
      {  TR::unknownMethod}
      };

   static X StringMethods[] =
      {
      {x(TR::java_lang_String_trim,                "trim",                "()Ljava/lang/String;")},
      {x(TR::java_lang_String_init_String,         "<init>",              "(Ljava/lang/String;)V")},
      {x(TR::java_lang_String_init_String_char,    "<init>",              "(Ljava/lang/String;C)V")},
      {x(TR::java_lang_String_init_int_String_int_String_String, "<init>","(ILjava/lang/String;ILjava/lang/String;Ljava/lang/String;)V")},
      {x(TR::java_lang_String_init_int_int_char_boolean, "<init>",       "(II[CZ)V")},
      {  TR::java_lang_String_init,          6,    "<init>", (int16_t)-1,    "*"},
      {x(TR::java_lang_String_charAt,              "charAt",              "(I)C")},
      {x(TR::java_lang_String_charAtInternal_I,    "charAtInternal",      "(I)C")},
      {x(TR::java_lang_String_charAtInternal_IB,   "charAtInternal",      "(I[B)C")},
      {x(TR::java_lang_String_charAtInternal_IB,   "charAtInternal",      "(I[C)C")},
      {x(TR::java_lang_String_concat,              "concat",              "(Ljava/lang/String;)Ljava/lang/String;")},
      {x(TR::java_lang_String_compressedArrayCopy_BIBII,              "compressedArrayCopy",              "([BI[BII)V")},
      {x(TR::java_lang_String_compressedArrayCopy_BICII,              "compressedArrayCopy",              "([BI[CII)V")},
      {x(TR::java_lang_String_compressedArrayCopy_CIBII,              "compressedArrayCopy",              "([CI[BII)V")},
      {x(TR::java_lang_String_compressedArrayCopy_CICII,              "compressedArrayCopy",              "([CI[CII)V")},
      {x(TR::java_lang_String_decompressedArrayCopy_BIBII,              "decompressedArrayCopy",              "([BI[BII)V")},
      {x(TR::java_lang_String_decompressedArrayCopy_BICII,              "decompressedArrayCopy",              "([BI[CII)V")},
      {x(TR::java_lang_String_decompressedArrayCopy_CIBII,              "decompressedArrayCopy",              "([CI[BII)V")},
      {x(TR::java_lang_String_decompressedArrayCopy_CICII,              "decompressedArrayCopy",              "([CI[CII)V")},
      {x(TR::java_lang_String_equals,              "equals",              "(Ljava/lang/Object;)Z")},
      {x(TR::java_lang_String_indexOf_String,      "indexOf",             "(Ljava/lang/String;)I")},
      {x(TR::java_lang_String_indexOf_String_int,  "indexOf",             "(Ljava/lang/String;I)I")},
      {x(TR::java_lang_String_indexOf_native,      "indexOf",             "(II)I")},
      {x(TR::java_lang_String_indexOf_fast,        "indexOf",             "(Ljava/lang/String;Ljava/lang/String;IIC)I")},
      {x(TR::java_lang_String_isCompressed,        "isCompressed",        "()Z")},
      {x(TR::java_lang_String_length,              "length",              "()I")},
      {x(TR::java_lang_String_lengthInternal,      "lengthInternal",      "()I")},
      {x(TR::java_lang_String_replace,             "replace",             "(CC)Ljava/lang/String;")},
      {x(TR::java_lang_String_hashCode,            "hashCode",            "()I")},
      {x(TR::java_lang_String_hashCodeImplCompressed,  "hashCodeImplCompressed",          "([BII)I")},
      {x(TR::java_lang_String_hashCodeImplCompressed,  "hashCodeImplCompressed",          "([CII)I")},
      {x(TR::java_lang_String_hashCodeImplDecompressed,"hashCodeImplDecompressed",        "([BII)I")},
      {x(TR::java_lang_String_hashCodeImplDecompressed,"hashCodeImplDecompressed",        "([CII)I")},
      {x(TR::java_lang_String_compareTo,           "compareTo",           "(Ljava/lang/String;)I")},
      {x(TR::java_lang_String_lastIndexOf,         "lastIndexOf",         "(Ljava/lang/String;I)I")},
      {x(TR::java_lang_String_toLowerCase,         "toLowerCase",         "(Ljava/util/Locale;)Ljava/lang/String;")},
      {x(TR::java_lang_String_toLowerCaseCore,     "toLowerCaseCore",     "(Ljava/lang/String;)Ljava/lang/String;")},
      {x(TR::java_lang_String_toUpperCase,         "toUpperCase",         "(Ljava/util/Locale;)Ljava/lang/String;")},
      {x(TR::java_lang_String_toUpperCaseCore,     "toUpperCaseCore",     "(Ljava/lang/String;)Ljava/lang/String;")},
      {x(TR::java_lang_String_toCharArray,         "toCharArray",         "(Ljava/util/Locale;)Ljava/lang/String;")},
      {x(TR::java_lang_String_regionMatches,       "regionMatches",       "(ILjava/lang/String;II)Z")},
      {x(TR::java_lang_String_regionMatches_bool,  "regionMatches",       "(ZILjava/lang/String;II)Z")},
      {x(TR::java_lang_String_equalsIgnoreCase,    "equalsIgnoreCase",    "(Ljava/lang/String;)Z")},
      {x(TR::java_lang_String_compareToIgnoreCase, "compareToIgnoreCase", "(Ljava/lang/String;)I")},
      {x(TR::java_lang_String_compress,            "compress",            "([C[BII)I")},
      {x(TR::java_lang_String_compressNoCheck,     "compressNoCheck",     "([C[BII)V")},
      {x(TR::java_lang_String_andOR,               "andOR",               "([CII)I")},
      {x(TR::java_lang_String_toUpperHWOptimizedCompressed,  "toUpperHWOptimizedCompressed",  "(Ljava/lang/String;)Z")},
      {x(TR::java_lang_String_toUpperHWOptimizedDecompressed,"toUpperHWOptimizedDecompressed","(Ljava/lang/String;)Z")},
      {x(TR::java_lang_String_toUpperHWOptimized,  "toUpperHWOptimized","(Ljava/lang/String;)Z")},
      {x(TR::java_lang_String_toLowerHWOptimizedCompressed,  "toLowerHWOptimizedCompressed",  "(Ljava/lang/String;)Z")},
      {x(TR::java_lang_String_toLowerHWOptimizedDecompressed,"toLowerHWOptimizedDecompressed",  "(Ljava/lang/String;)Z")},
      {x(TR::java_lang_String_toLowerHWOptimized,  "toLowerHWOptimized",  "(Ljava/lang/String;)Z")},
      {x(TR::java_lang_String_StrHWAvailable,      "StrHWAvailable", "()Z")},
      {x(TR::java_lang_String_unsafeCharAt,        "unsafeCharAt",        "(I)C")},
      {x(TR::java_lang_String_split_str_int,       "split",               "(Ljava/lang/String;I)[Ljava/lang/String;")},
      {x(TR::java_lang_String_getChars_charArray,  "getChars",            "(II[CI)V")},
      {x(TR::java_lang_String_getChars_byteArray,  "getChars",            "(II[BI)V")},
      {  TR::unknownMethod}
      };

   static X StringBufferMethods[] =
      {
      {x(TR::java_lang_StringBuffer_append,             "append",   "([C)Ljava/lang/StringBuffer;")},
      {x(TR::java_lang_StringBuffer_append,             "append",   "([CII)Ljava/lang/StringBuffer;")},
      {x(TR::java_lang_StringBuffer_capacityInternal,   "capacityInternal",   "()I")},
      {x(TR::java_lang_StringBuffer_ensureCapacityImpl, "ensureCapacityImpl", "(I)V")},
      {x(TR::java_lang_StringBuffer_lengthInternalUnsynchronized,     "lengthInternalUnsynchronized",     "()I")},
      {  TR::unknownMethod}
      };

   static X StringCodingMethods[] =
      {
      {x(TR::java_lang_StringCoding_decode, "decode", "(Ljava/nio/charset/Charset;[BII)[C")},
      {x(TR::java_lang_StringCoding_encode, "encode", "(Ljava/nio/charset/Charset;[CII)[B")},
      {x(TR::java_lang_StringCoding_implEncodeISOArray, "implEncodeISOArray", "([BI[BII)I")},
      {x(TR::java_lang_StringCoding_encode8859_1,       "encode8859_1",       "(B[B)[B")},
      {x(TR::java_lang_StringCoding_encodeASCII,        "encodeASCII",        "(B[B)[B")},
      {x(TR::java_lang_StringCoding_encodeUTF8,         "encodeUTF8",         "(B[B)[B")},
      {  TR::unknownMethod}
      };

   static X StringCoding_StringDecoderMethods[] =
      {
      {x(TR::java_lang_StringCoding_StringDecoder_decode, "decode", "([BII)[C")},
      {  TR::unknownMethod}
      };

   static X StringCoding_StringEncoderMethods[] =
      {
      {x(TR::java_lang_StringCoding_StringEncoder_encode, "encode", "([CII)[B")},
      {  TR::unknownMethod}
      };

    static X NumberFormatMethods[] =
      {
      {x(TR::java_text_NumberFormat_format,             "format",   "(D)Ljava/lang/String;")},
      {  TR::unknownMethod}
      };
   static X StringBuilderMethods[] =
      {
      {x(TR::java_lang_StringBuilder_init,               "<init>",             "()V")},
      {x(TR::java_lang_StringBuilder_init_int,           "<init>",             "(I)V")},
      {x(TR::java_lang_StringBuilder_append_bool,        "append",             "(Z)Ljava/lang/StringBuilder;")},
      {x(TR::java_lang_StringBuilder_append_char,        "append",             "(C)Ljava/lang/StringBuilder;")},
      {x(TR::java_lang_StringBuilder_append_double,      "append",             "(D)Ljava/lang/StringBuilder;")},
      {x(TR::java_lang_StringBuilder_append_float,       "append",             "(F)Ljava/lang/StringBuilder;")},
      {x(TR::java_lang_StringBuilder_append_int,         "append",             "(I)Ljava/lang/StringBuilder;")},
      {x(TR::java_lang_StringBuilder_append_long,        "append",             "(J)Ljava/lang/StringBuilder;")},
      {x(TR::java_lang_StringBuilder_append_String,      "append",             "(Ljava/lang/String;)Ljava/lang/StringBuilder;")},
      {x(TR::java_lang_StringBuilder_append_Object,      "append",             "(Ljava/lang/Object;)Ljava/lang/StringBuilder;")},
      {x(TR::java_lang_StringBuilder_capacityInternal,   "capacityInternal",   "()I")},
      {x(TR::java_lang_StringBuilder_ensureCapacityImpl, "ensureCapacityImpl", "(I)V")},
      {x(TR::java_lang_StringBuilder_lengthInternal,     "lengthInternal",     "()I")},
      {x(TR::java_lang_StringBuilder_toString,           "toString",           "()Ljava/lang/String;")},

      {  TR::unknownMethod}
      };

   static X SystemMethods[] =
      {
      {  TR::java_lang_System_arraycopy,      9, "arraycopy",    (int16_t)-1, "*"},
      {x(TR::java_lang_System_currentTimeMillis, "currentTimeMillis",   "()J")},
      {x(TR::java_lang_System_nanoTime,          "nanoTime",   "()J")},
      {x(TR::java_lang_System_hiresClockImpl,          "hiresClockImpl",   "()J")},
      {x(TR::java_lang_System_identityHashCode,  "identityHashCode",    "(Ljava/lang/Object;)I")},
      {  TR::unknownMethod}
      };

   static X ThreadMethods[] =
      {
      {x(TR::java_lang_Thread_currentThread,     "currentThread",   "()Ljava/lang/Thread;")},
      {  TR::unknownMethod}
      };

   static X ThrowableMethods[] =
      {
      {  TR::java_lang_Throwable_printStackTrace,     15, "printStackTrace",   (int16_t)-1, "*"},
      {x(TR::java_lang_Throwable_fillInStackTrace,    "fillInStackTrace",  "()Ljava/lang/Throwable;")},
      {  TR::unknownMethod}
      };

   static X UnsafeMethods[] =
      {
      {x(TR::sun_misc_Unsafe_putBoolean_jlObjectJZ_V,       "putBoolean", "(Ljava/lang/Object;JZ)V")},
      {x(TR::sun_misc_Unsafe_putByte_jlObjectJB_V,          "putByte",    "(Ljava/lang/Object;JB)V")},
      {x(TR::sun_misc_Unsafe_putChar_jlObjectJC_V,          "putChar",    "(Ljava/lang/Object;JC)V")},
      {x(TR::sun_misc_Unsafe_putShort_jlObjectJS_V,         "putShort",   "(Ljava/lang/Object;JS)V")},
      {x(TR::sun_misc_Unsafe_putInt_jlObjectJI_V,           "putInt",     "(Ljava/lang/Object;JI)V")},
      {x(TR::sun_misc_Unsafe_putLong_jlObjectJJ_V,          "putLong",    "(Ljava/lang/Object;JJ)V")},
      {x(TR::sun_misc_Unsafe_putFloat_jlObjectJF_V,         "putFloat",   "(Ljava/lang/Object;JF)V")},
      {x(TR::sun_misc_Unsafe_putDouble_jlObjectJD_V,        "putDouble",  "(Ljava/lang/Object;JD)V")},
      {x(TR::sun_misc_Unsafe_putObject_jlObjectJjlObject_V, "putObject",  "(Ljava/lang/Object;JLjava/lang/Object;)V")},

      {x(TR::sun_misc_Unsafe_putBooleanVolatile_jlObjectJZ_V,       "putBooleanVolatile", "(Ljava/lang/Object;JZ)V")},
      {x(TR::sun_misc_Unsafe_putByteVolatile_jlObjectJB_V,          "putByteVolatile",    "(Ljava/lang/Object;JB)V")},
      {x(TR::sun_misc_Unsafe_putCharVolatile_jlObjectJC_V,          "putCharVolatile",    "(Ljava/lang/Object;JC)V")},
      {x(TR::sun_misc_Unsafe_putShortVolatile_jlObjectJS_V,         "putShortVolatile",   "(Ljava/lang/Object;JS)V")},
      {x(TR::sun_misc_Unsafe_putIntVolatile_jlObjectJI_V,           "putIntVolatile",     "(Ljava/lang/Object;JI)V")},
      {x(TR::sun_misc_Unsafe_putLongVolatile_jlObjectJJ_V,          "putLongVolatile",    "(Ljava/lang/Object;JJ)V")},
      {x(TR::sun_misc_Unsafe_putFloatVolatile_jlObjectJF_V,         "putFloatVolatile",   "(Ljava/lang/Object;JF)V")},
      {x(TR::sun_misc_Unsafe_putDoubleVolatile_jlObjectJD_V,        "putDoubleVolatile",  "(Ljava/lang/Object;JD)V")},
      {x(TR::sun_misc_Unsafe_putObjectVolatile_jlObjectJjlObject_V, "putObjectVolatile",  "(Ljava/lang/Object;JLjava/lang/Object;)V")},

      {x(TR::sun_misc_Unsafe_putBooleanVolatile_jlObjectJZ_V,       "putBooleanRelease", "(Ljava/lang/Object;JZ)V")},
      {x(TR::sun_misc_Unsafe_putByteVolatile_jlObjectJB_V,          "putByteRelease",    "(Ljava/lang/Object;JB)V")},
      {x(TR::sun_misc_Unsafe_putCharVolatile_jlObjectJC_V,          "putCharRelease",    "(Ljava/lang/Object;JC)V")},
      {x(TR::sun_misc_Unsafe_putShortVolatile_jlObjectJS_V,         "putShortRelease",   "(Ljava/lang/Object;JS)V")},
      {x(TR::sun_misc_Unsafe_putIntVolatile_jlObjectJI_V,           "putIntRelease",     "(Ljava/lang/Object;JI)V")},
      {x(TR::sun_misc_Unsafe_putLongVolatile_jlObjectJJ_V,          "putLongRelease",    "(Ljava/lang/Object;JJ)V")},
      {x(TR::sun_misc_Unsafe_putFloatVolatile_jlObjectJF_V,         "putFloatRelease",   "(Ljava/lang/Object;JF)V")},
      {x(TR::sun_misc_Unsafe_putDoubleVolatile_jlObjectJD_V,        "putDoubleRelease",  "(Ljava/lang/Object;JD)V")},
      {x(TR::sun_misc_Unsafe_putObjectVolatile_jlObjectJjlObject_V, "putObjectRelease",  "(Ljava/lang/Object;JLjava/lang/Object;)V")},

      {x(TR::sun_misc_Unsafe_putInt_jlObjectII_V,           "putInt",     "(Ljava/lang/Object;II)V")},

      {x(TR::sun_misc_Unsafe_getBoolean_jlObjectJ_Z,        "getBoolean", "(Ljava/lang/Object;J)Z")},
      {x(TR::sun_misc_Unsafe_getByte_jlObjectJ_B,           "getByte",    "(Ljava/lang/Object;J)B")},
      {x(TR::sun_misc_Unsafe_getChar_jlObjectJ_C,           "getChar",    "(Ljava/lang/Object;J)C")},
      {x(TR::sun_misc_Unsafe_getShort_jlObjectJ_S,          "getShort",   "(Ljava/lang/Object;J)S")},
      {x(TR::sun_misc_Unsafe_getInt_jlObjectJ_I,            "getInt",     "(Ljava/lang/Object;J)I")},
      {x(TR::sun_misc_Unsafe_getLong_jlObjectJ_J,           "getLong",    "(Ljava/lang/Object;J)J")},
      {x(TR::sun_misc_Unsafe_getFloat_jlObjectJ_F,          "getFloat",   "(Ljava/lang/Object;J)F")},
      {x(TR::sun_misc_Unsafe_getDouble_jlObjectJ_D,         "getDouble",  "(Ljava/lang/Object;J)D")},
      {x(TR::sun_misc_Unsafe_getObject_jlObjectJ_jlObject,  "getObject",  "(Ljava/lang/Object;J)Ljava/lang/Object;")},

      {x(TR::sun_misc_Unsafe_getBooleanVolatile_jlObjectJ_Z,        "getBooleanVolatile", "(Ljava/lang/Object;J)Z")},
      {x(TR::sun_misc_Unsafe_getByteVolatile_jlObjectJ_B,           "getByteVolatile",    "(Ljava/lang/Object;J)B")},
      {x(TR::sun_misc_Unsafe_getCharVolatile_jlObjectJ_C,           "getCharVolatile",    "(Ljava/lang/Object;J)C")},
      {x(TR::sun_misc_Unsafe_getShortVolatile_jlObjectJ_S,          "getShortVolatile",   "(Ljava/lang/Object;J)S")},
      {x(TR::sun_misc_Unsafe_getIntVolatile_jlObjectJ_I,            "getIntVolatile",     "(Ljava/lang/Object;J)I")},
      {x(TR::sun_misc_Unsafe_getLongVolatile_jlObjectJ_J,           "getLongVolatile",    "(Ljava/lang/Object;J)J")},
      {x(TR::sun_misc_Unsafe_getFloatVolatile_jlObjectJ_F,          "getFloatVolatile",   "(Ljava/lang/Object;J)F")},
      {x(TR::sun_misc_Unsafe_getDoubleVolatile_jlObjectJ_D,         "getDoubleVolatile",  "(Ljava/lang/Object;J)D")},
      {x(TR::sun_misc_Unsafe_getObjectVolatile_jlObjectJ_jlObject,  "getObjectVolatile",  "(Ljava/lang/Object;J)Ljava/lang/Object;")},

      {x(TR::sun_misc_Unsafe_getBooleanVolatile_jlObjectJ_Z,        "getBooleanAcquire", "(Ljava/lang/Object;J)Z")},
      {x(TR::sun_misc_Unsafe_getByteVolatile_jlObjectJ_B,           "getByteAcquire",    "(Ljava/lang/Object;J)B")},
      {x(TR::sun_misc_Unsafe_getCharVolatile_jlObjectJ_C,           "getCharAcquire",    "(Ljava/lang/Object;J)C")},
      {x(TR::sun_misc_Unsafe_getShortVolatile_jlObjectJ_S,          "getShortAcquire",   "(Ljava/lang/Object;J)S")},
      {x(TR::sun_misc_Unsafe_getIntVolatile_jlObjectJ_I,            "getIntAcquire",     "(Ljava/lang/Object;J)I")},
      {x(TR::sun_misc_Unsafe_getLongVolatile_jlObjectJ_J,           "getLongAcquire",    "(Ljava/lang/Object;J)J")},
      {x(TR::sun_misc_Unsafe_getFloatVolatile_jlObjectJ_F,          "getFloatAcquire",   "(Ljava/lang/Object;J)F")},
      {x(TR::sun_misc_Unsafe_getDoubleVolatile_jlObjectJ_D,         "getDoubleAcquire",  "(Ljava/lang/Object;J)D")},
      {x(TR::sun_misc_Unsafe_getObjectVolatile_jlObjectJ_jlObject,  "getObjectAcquire",  "(Ljava/lang/Object;J)Ljava/lang/Object;")},

      {x(TR::sun_misc_Unsafe_putByte_JB_V,                  "putByte",    "(JB)V")},
      {x(TR::sun_misc_Unsafe_putShort_JS_V,                 "putShort",   "(JS)V")},
      {x(TR::sun_misc_Unsafe_putChar_JC_V,                  "putChar",    "(JC)V")},
      {x(TR::sun_misc_Unsafe_putInt_JI_V,                   "putInt",     "(JI)V")},
      {x(TR::sun_misc_Unsafe_putLong_JJ_V,                  "putLong",    "(JJ)V")},
      {x(TR::sun_misc_Unsafe_putFloat_JF_V,                 "putFloat",   "(JF)V")},
      {x(TR::sun_misc_Unsafe_putDouble_JD_V,                "putDouble",  "(JD)V")},
      {x(TR::sun_misc_Unsafe_putAddress_JJ_V,               "putAddress", "(JJ)V")},

      {x(TR::sun_misc_Unsafe_getByte_J_B,                   "getByte",    "(J)B")},
      {x(TR::sun_misc_Unsafe_getShort_J_S,                  "getShort",   "(J)S")},
      {x(TR::sun_misc_Unsafe_getChar_J_C,                   "getChar",    "(J)C")},
      {x(TR::sun_misc_Unsafe_getInt_J_I,                    "getInt",     "(J)I")},
      {x(TR::sun_misc_Unsafe_getLong_J_J,                   "getLong",    "(J)J")},
      {x(TR::sun_misc_Unsafe_getFloat_J_F,                  "getFloat",   "(J)F")},
      {x(TR::sun_misc_Unsafe_getDouble_J_D,                 "getDouble",  "(J)D")},
      {x(TR::sun_misc_Unsafe_getAddress_J_J,                "getAddress", "(J)J")},

      {x(TR::sun_misc_Unsafe_compareAndSwapInt_jlObjectJII_Z,                  "compareAndSwapInt",    "(Ljava/lang/Object;JII)Z")},
      {x(TR::sun_misc_Unsafe_compareAndSwapInt_jlObjectJII_Z,                  "compareAndSetInt",    "(Ljava/lang/Object;JII)Z")},
      {x(TR::sun_misc_Unsafe_compareAndSwapLong_jlObjectJJJ_Z,                 "compareAndSwapLong",   "(Ljava/lang/Object;JJJ)Z")},
      {x(TR::sun_misc_Unsafe_compareAndSwapLong_jlObjectJJJ_Z,                 "compareAndSetLong",   "(Ljava/lang/Object;JJJ)Z")},
      {x(TR::sun_misc_Unsafe_compareAndSwapObject_jlObjectJjlObjectjlObject_Z, "compareAndSwapObject", "(Ljava/lang/Object;JLjava/lang/Object;Ljava/lang/Object;)Z")},
      {x(TR::sun_misc_Unsafe_compareAndSwapObject_jlObjectJjlObjectjlObject_Z, "compareAndSetObject", "(Ljava/lang/Object;JLjava/lang/Object;Ljava/lang/Object;)Z")},

      {x(TR::sun_misc_Unsafe_staticFieldBase,               "staticFieldBase",   "(Ljava/lang/reflect/Field;)Ljava/lang/Object")},
      {x(TR::sun_misc_Unsafe_staticFieldOffset,             "staticFieldOffset", "(Ljava/lang/reflect/Field;)J")},
      {x(TR::sun_misc_Unsafe_objectFieldOffset,             "objectFieldOffset", "(Ljava/lang/reflect/Field;)J")},

      {x(TR::sun_misc_Unsafe_putBooleanOrdered_jlObjectJZ_V,       "putOrderedBoolean", "(Ljava/lang/Object;JZ)V")},
      {x(TR::sun_misc_Unsafe_putByteOrdered_jlObjectJB_V,          "putOrderedByte",    "(Ljava/lang/Object;JB)V")},
      {x(TR::sun_misc_Unsafe_putCharOrdered_jlObjectJC_V,          "putOrderedChar",    "(Ljava/lang/Object;JC)V")},
      {x(TR::sun_misc_Unsafe_putShortOrdered_jlObjectJS_V,         "putOrderedShort",   "(Ljava/lang/Object;JS)V")},
      {x(TR::sun_misc_Unsafe_putIntOrdered_jlObjectJI_V,           "putOrderedInt",     "(Ljava/lang/Object;JI)V")},
      {x(TR::sun_misc_Unsafe_putLongOrdered_jlObjectJJ_V,          "putOrderedLong",    "(Ljava/lang/Object;JJ)V")},
      {x(TR::sun_misc_Unsafe_putFloatOrdered_jlObjectJF_V,         "putOrderedFloat",   "(Ljava/lang/Object;JF)V")},
      {x(TR::sun_misc_Unsafe_putDoubleOrdered_jlObjectJD_V,        "putOrderedDouble",  "(Ljava/lang/Object;JD)V")},
      {x(TR::sun_misc_Unsafe_putObjectOrdered_jlObjectJjlObject_V, "putOrderedObject",  "(Ljava/lang/Object;JLjava/lang/Object;)V")},

      {x(TR::sun_misc_Unsafe_monitorEnter_jlObject_V,       "monitorEnter",    "(Ljava/lang/Object;)V")},
      {x(TR::sun_misc_Unsafe_monitorExit_jlObject_V,        "monitorExit",     "(Ljava/lang/Object;)V")},
      {x(TR::sun_misc_Unsafe_tryMonitorEnter_jlObject_Z,    "tryMonitorEnter", "(Ljava/lang/Object;)Z")},

      {x(TR::sun_misc_Unsafe_copyMemory,    "copyMemory", "(Ljava/lang/Object;JLjava/lang/Object;JJ)V")},
      {x(TR::sun_misc_Unsafe_setMemory,     "setMemory",  "(Ljava/lang/Object;JJB)V")},

      {x(TR::sun_misc_Unsafe_loadFence,     "loadFence",  "()V")},
      {x(TR::sun_misc_Unsafe_storeFence,    "storeFence", "()V")},
      {x(TR::sun_misc_Unsafe_fullFence,     "fullFence",  "()V")},

      {x(TR::sun_misc_Unsafe_ensureClassInitialized,     "ensureClassInitialized", "(Ljava/lang/Class;)V")},

      {  TR::unknownMethod}
      };

   static X ArrayMethods[] =
      {
      {x(TR::java_lang_reflect_Array_getLength, "getLength", "(Ljava/lang/Object;)I")},
      {  TR::unknownMethod}
      };

   static X VectorMethods[] =
      {
      {  TR::java_util_Vector_subList,    7, "subList",  (int16_t)-1, "*"},
      {x(TR::java_util_Vector_contains,      "contains",     "(Ljava/lang/Object;)Z")},
      {x(TR::java_util_Vector_addElement,    "addElement",   "(Ljava/lang/Object;)V")},
      {  TR::unknownMethod}
      };

   static X SingleByteConverterMethods[] =
      {
      {x(TR::sun_io_ByteToCharSingleByte_convert,       "convert", "([BII[CII)I")},
      {x(TR::sun_io_CharToByteSingleByte_convert,       "convert", "([CII[BII)I")},
      {x(TR::sun_io_ByteToCharSingleByte_JITintrinsicConvert,       "JITintrinsicConvert", "([Ljava/nio/ByteBuffer;[CI[B)I")},
      {  TR::unknownMethod}
      };

   static X DoubleByteConverterMethods[] =
      {
      {x(TR::sun_io_ByteToCharDBCS_EBCDIC_convert,      "convert", "([BII[CII)I")},
      {  TR::unknownMethod}
      };

   static X ORBVMHelperMethods[] =
      {
      {x(TR::com_ibm_oti_vm_ORBVMHelpers_is32Bit,                             "is32Bit", "()Z")},
      {x(TR::com_ibm_oti_vm_ORBVMHelpers_getNumBitsInReferenceField,          "getNumBitsInReferenceField", "()I")},
      {x(TR::com_ibm_oti_vm_ORBVMHelpers_getNumBytesInReferenceField,         "getNumBytesInReferenceField", "()I")},
      {x(TR::com_ibm_oti_vm_ORBVMHelpers_getNumBitsInDescriptionWord,         "getNumBitsInDescriptionWord", "()I")},
      {x(TR::com_ibm_oti_vm_ORBVMHelpers_getNumBytesInDescriptionWord,        "getNumBytesInDescriptionWord", "()I")},
      {x(TR::com_ibm_oti_vm_ORBVMHelpers_getNumBytesInJ9ObjectHeader,         "getNumBytesInJ9ObjectHeader", "()I")},

      {x(TR::com_ibm_oti_vm_ORBVMHelpers_getJ9ClassFromClass32,               "getJ9ClassFromClass32", "(Ljava/lang/Class;)I")},
      {x(TR::com_ibm_oti_vm_ORBVMHelpers_getInstanceShapeFromJ9Class32,       "getTotalInstanceSizeFromJ9Class32", "(I)I")},
      {x(TR::com_ibm_oti_vm_ORBVMHelpers_getInstanceDescriptionFromJ9Class32, "getInstanceDescriptionFromJ9Class32", "(I)I")},
      {x(TR::com_ibm_oti_vm_ORBVMHelpers_getDescriptionWordFromPtr32,         "getDescriptionWordFromPtr32", "(I)I")},

      {x(TR::com_ibm_oti_vm_ORBVMHelpers_getJ9ClassFromClass64,               "getJ9ClassFromClass64", "(Ljava/lang/Class;)J")},
      {x(TR::com_ibm_oti_vm_ORBVMHelpers_getInstanceShapeFromJ9Class64,       "getTotalInstanceSizeFromJ9Class64", "(J)J")},
      {x(TR::com_ibm_oti_vm_ORBVMHelpers_getInstanceDescriptionFromJ9Class64, "getInstanceDescriptionFromJ9Class64", "(J)J")},
      {x(TR::com_ibm_oti_vm_ORBVMHelpers_getDescriptionWordFromPtr64,         "getDescriptionWordFromPtr64", "(J)J")},

      {  TR::unknownMethod}
      };

   // some of these take a long and return an int
   static X JITHelperMethods[] =
      {
      {x(TR::com_ibm_jit_JITHelpers_is32Bit,                                  "is32Bit", "()Z")},
      {x(TR::com_ibm_jit_JITHelpers_isArray,                                  "isArray", "(Ljava/lang/Object;)Z")},
      {x(TR::com_ibm_jit_JITHelpers_getJ9ClassFromObject64,                   "getJ9ClassFromObject64", "(Ljava/lang/Object;)J")},
      {x(TR::com_ibm_jit_JITHelpers_getJ9ClassFromObject32,                   "getJ9ClassFromObject32", "(Ljava/lang/Object;)I")},
      {x(TR::com_ibm_jit_JITHelpers_getJ9ClassFromClass32,                    "getJ9ClassFromClass32", "(Ljava/lang/Class;)I")},
      {x(TR::com_ibm_jit_JITHelpers_getJ9ClassFromClass64,                    "getJ9ClassFromClass64", "(Ljava/lang/Class;)J")},
      {x(TR::com_ibm_jit_JITHelpers_getBackfillOffsetFromJ9Class32,           "getBackfillOffsetFromJ9Class32", "(I)I")},
      {x(TR::com_ibm_jit_JITHelpers_getBackfillOffsetFromJ9Class64,           "getBackfillOffsetFromJ9Class64", "(J)J")},
      {x(TR::com_ibm_jit_JITHelpers_getRomClassFromJ9Class32,                 "getRomClassFromJ9Class32", "(I)I")},
      {x(TR::com_ibm_jit_JITHelpers_getRomClassFromJ9Class64,                 "getRomClassFromJ9Class64", "(J)J")},
      {x(TR::com_ibm_jit_JITHelpers_getArrayShapeFromRomClass32,              "getArrayShapeFromRomClass32", "(I)I")},
      {x(TR::com_ibm_jit_JITHelpers_getArrayShapeFromRomClass64,              "getArrayShapeFromRomClass64", "(J)I")},
      {x(TR::com_ibm_jit_JITHelpers_getSuperClassesFromJ9Class32,             "getSuperClassesFromJ9Class32", "(I)I")},
      {x(TR::com_ibm_jit_JITHelpers_getSuperClassesFromJ9Class64,             "getSuperClassesFromJ9Class64", "(J)J")},
      {x(TR::com_ibm_jit_JITHelpers_getClassDepthAndFlagsFromJ9Class32,       "getClassDepthAndFlagsFromJ9Class32", "(I)I")},
      {x(TR::com_ibm_jit_JITHelpers_getClassDepthAndFlagsFromJ9Class64,       "getClassDepthAndFlagsFromJ9Class64", "(J)J")},
      {x(TR::com_ibm_jit_JITHelpers_getClassFlagsFromJ9Class32,               "getClassFlagsFromJ9Class32", "(I)I")},
      {x(TR::com_ibm_jit_JITHelpers_getClassFlagsFromJ9Class64,               "getClassFlagsFromJ9Class64", "(J)I")},
      {x(TR::com_ibm_jit_JITHelpers_getModifiersFromRomClass32,               "getModifiersFromRomClass32", "(I)I")},
      {x(TR::com_ibm_jit_JITHelpers_getModifiersFromRomClass64,               "getModifiersFromRomClass64", "(J)I")},
      {x(TR::com_ibm_jit_JITHelpers_getClassFromJ9Class32,                    "getClassFromJ9Class32", "(I)Ljava/lang/Class;")},
      {x(TR::com_ibm_jit_JITHelpers_getClassFromJ9Class64,                    "getClassFromJ9Class64", "(J)Ljava/lang/Class;")},
      {x(TR::com_ibm_jit_JITHelpers_getAddressAsPrimitive32,                  "getAddressAsPrimitive32", "(Ljava/lang/Object;)I")},
      {x(TR::com_ibm_jit_JITHelpers_getAddressAsPrimitive64,                  "getAddressAsPrimitive64", "(Ljava/lang/Object;)J")},
      {x(TR::com_ibm_jit_JITHelpers_hashCodeImpl,                             "hashCodeImpl", "(Ljava/lang/Object;)I")},
      {x(TR::com_ibm_jit_JITHelpers_getSuperclass,                            "getSuperclass", "(Ljava/lang/Class;)Ljava/lang/Class;")},
      {x(TR::com_ibm_jit_JITHelpers_optimizedClone,                           "optimizedClone", "(Ljava/lang/Object;)Ljava/lang/Object;")},
      {x(TR::com_ibm_jit_JITHelpers_getPackedDataSizeFromJ9Class32,           "getPackedDataSizeFromJ9Class32", "(I)I")},
      {x(TR::com_ibm_jit_JITHelpers_getPackedDataSizeFromJ9Class64,           "getPackedDataSizeFromJ9Class64", "(J)J")},
      {x(TR::com_ibm_jit_JITHelpers_getComponentTypeFromJ9Class32,            "getComponentTypeFromJ9Class32", "(I)I")},
      {x(TR::com_ibm_jit_JITHelpers_getComponentTypeFromJ9Class64,            "getComponentTypeFromJ9Class64", "(J)J")},
      {x(TR::com_ibm_jit_JITHelpers_transformedEncodeUTF16Big,                "transformedEncodeUTF16Big",       "(JJI)I")},
      {x(TR::com_ibm_jit_JITHelpers_transformedEncodeUTF16Little,             "transformedEncodeUTF16Little",    "(JJI)I")},
      {x(TR::com_ibm_jit_JITHelpers_getIntFromObject,                         "getIntFromObject", "(Ljava/lang/Object;J)I")},
      {x(TR::com_ibm_jit_JITHelpers_getIntFromObjectVolatile,                 "getIntFromObjectVolatile", "(Ljava/lang/Object;J)I")},
      {x(TR::com_ibm_jit_JITHelpers_getLongFromObject,                        "getLongFromObject", "(Ljava/lang/Object;J)J")},
      {x(TR::com_ibm_jit_JITHelpers_getLongFromObjectVolatile,                "getLongFromObjectVolatile", "(Ljava/lang/Object;J)J")},
      {x(TR::com_ibm_jit_JITHelpers_getObjectFromObject,                      "getObjectFromObject", "(Ljava/lang/Object;J)Ljava/lang/Object;")},
      {x(TR::com_ibm_jit_JITHelpers_getObjectFromObjectVolatile,              "getObjectFromObjectVolatile", "(Ljava/lang/Object;J)Ljava/lang/Object;")},
      {x(TR::com_ibm_jit_JITHelpers_putIntInObject,                           "putIntInObject", "(Ljava/lang/Object;JI)V")},
      {x(TR::com_ibm_jit_JITHelpers_putIntInObjectVolatile,                   "putIntInObjectVolatile", "(Ljava/lang/Object;JI)V")},
      {x(TR::com_ibm_jit_JITHelpers_putLongInObject,                          "putLongInObject", "(Ljava/lang/Object;JJ)V")},
      {x(TR::com_ibm_jit_JITHelpers_putLongInObjectVolatile,                  "putLongInObjectVolatile", "(Ljava/lang/Object;JJ)V")},
      {x(TR::com_ibm_jit_JITHelpers_putObjectInObject,                        "putObjectInObject", "(Ljava/lang/Object;JLjava/lang/Object;)V")},
      {x(TR::com_ibm_jit_JITHelpers_putObjectInObjectVolatile,                "putObjectInObjectVolatile", "(Ljava/lang/Object;JLjava/lang/Object;)V")},
      {x(TR::com_ibm_jit_JITHelpers_compareAndSwapIntInObject,                "compareAndSwapIntInObject", "(Ljava/lang/Object;JII)Z")},
      {x(TR::com_ibm_jit_JITHelpers_compareAndSwapLongInObject,               "compareAndSwapLongInObject", "(Ljava/lang/Object;JJJ)Z")},
      {x(TR::com_ibm_jit_JITHelpers_compareAndSwapObjectInObject,             "compareAndSwapObjectInObject", "(Ljava/lang/Object;JLjava/lang/Object;Ljava/lang/Object;)Z")},
      {x(TR::com_ibm_jit_JITHelpers_getByteFromArray,                         "getByteFromArray", "(Ljava/lang/Object;J)B")},
      {x(TR::com_ibm_jit_JITHelpers_getByteFromArrayByIndex,                  "getByteFromArrayByIndex", "(Ljava/lang/Object;I)B")},
      {x(TR::com_ibm_jit_JITHelpers_getByteFromArrayVolatile,                 "getByteFromArrayVolatile", "(Ljava/lang/Object;J)B")},
      {x(TR::com_ibm_jit_JITHelpers_getCharFromArray,                         "getCharFromArray", "(Ljava/lang/Object;J)C")},
      {x(TR::com_ibm_jit_JITHelpers_getCharFromArrayByIndex,                  "getCharFromArrayByIndex", "(Ljava/lang/Object;I)C")},
      {x(TR::com_ibm_jit_JITHelpers_getCharFromArrayVolatile,                 "getCharFromArrayVolatile", "(Ljava/lang/Object;J)C")},
      {x(TR::com_ibm_jit_JITHelpers_getIntFromArray,                          "getIntFromArray", "(Ljava/lang/Object;J)I")},
      {x(TR::com_ibm_jit_JITHelpers_getIntFromArrayVolatile,                  "getIntFromArrayVolatile", "(Ljava/lang/Object;J)I")},
      {x(TR::com_ibm_jit_JITHelpers_getLongFromArray,                         "getLongFromArray", "(Ljava/lang/Object;J)J")},
      {x(TR::com_ibm_jit_JITHelpers_getLongFromArrayVolatile,                 "getLongFromArrayVolatile", "(Ljava/lang/Object;J)J")},
      {x(TR::com_ibm_jit_JITHelpers_getObjectFromArray,                       "getObjectFromArray", "(Ljava/lang/Object;J)Ljava/lang/Object;")},
      {x(TR::com_ibm_jit_JITHelpers_getObjectFromArrayVolatile,               "getObjectFromArrayVolatile", "(Ljava/lang/Object;J)Ljava/lang/Object;")},
      {x(TR::com_ibm_jit_JITHelpers_putByteInArray,                           "putByteInArray", "(Ljava/lang/Object;JB)V")},
      {x(TR::com_ibm_jit_JITHelpers_putByteInArrayByIndex,                    "putByteInArrayByIndex", "(Ljava/lang/Object;IB)V")},
      {x(TR::com_ibm_jit_JITHelpers_putByteInArrayVolatile,                   "putByteInArrayVolatile", "(Ljava/lang/Object;JB)V")},
      {x(TR::com_ibm_jit_JITHelpers_putCharInArray,                           "putCharInArray", "(Ljava/lang/Object;JC)V")},
      {x(TR::com_ibm_jit_JITHelpers_putCharInArrayByIndex,                    "putCharInArrayByIndex", "(Ljava/lang/Object;IC)V")},
      {x(TR::com_ibm_jit_JITHelpers_putCharInArrayVolatile,                   "putCharInArrayVolatile", "(Ljava/lang/Object;JC)V")},
      {x(TR::com_ibm_jit_JITHelpers_putIntInArray,                            "putIntInArray", "(Ljava/lang/Object;JI)V")},
      {x(TR::com_ibm_jit_JITHelpers_putIntInArrayVolatile,                    "putIntInArrayVolatile", "(Ljava/lang/Object;JI)V")},
      {x(TR::com_ibm_jit_JITHelpers_putLongInArray,                           "putLongInArray", "(Ljava/lang/Object;JJ)V")},
      {x(TR::com_ibm_jit_JITHelpers_putLongInArrayVolatile,                   "putLongInArrayVolatile", "(Ljava/lang/Object;JJ)V")},
      {x(TR::com_ibm_jit_JITHelpers_putObjectInArray,                         "putObjectInArray", "(Ljava/lang/Object;JLjava/lang/Object;)V")},
      {x(TR::com_ibm_jit_JITHelpers_putObjectInArrayVolatile,                 "putObjectInArrayVolatile", "(Ljava/lang/Object;JLjava/lang/Object;)V")},
      {x(TR::com_ibm_jit_JITHelpers_compareAndSwapIntInArray,                 "compareAndSwapIntInArray", "(Ljava/lang/Object;JII)Z")},
      {x(TR::com_ibm_jit_JITHelpers_compareAndSwapLongInArray,                "compareAndSwapLongInArray", "(Ljava/lang/Object;JJJ)Z")},
      {x(TR::com_ibm_jit_JITHelpers_compareAndSwapObjectInArray,              "compareAndSwapObjectInArray", "(Ljava/lang/Object;JLjava/lang/Object;Ljava/lang/Object;)Z")},
      {x(TR::com_ibm_jit_JITHelpers_byteToCharUnsigned,                       "byteToCharUnsigned", "(B)C")},
      {x(TR::com_ibm_jit_JITHelpers_getClassInitializeStatus,                 "getClassInitializeStatus", "(Ljava/lang/Class;)I")},
      {  TR::unknownMethod}
      };

   static X StringUTF16Methods[] =
      {
      { x(TR::java_lang_StringUTF16_getChar,                                  "getChar", "([BI)C")},
      { TR::unknownMethod }
      };

   static X DecimalFormatHelperMethods[] =
      {
      {x(TR::com_ibm_jit_DecimalFormatHelper_formatAsDouble,                  "formatAsDouble", "(Ljava/text/DecimalFormat;Ljava/math/BigDecimal;)Ljava/lang/String;")},
      {x(TR::com_ibm_jit_DecimalFormatHelper_formatAsFloat,                   "formatAsFloat", "(Ljava/text/DecimalFormat;Ljava/math/BigDecimal;)Ljava/lang/String;")},
      {  TR::unknownMethod}
      };

   static X FastPathForCollocatedMethods[] =
      {
      {x(TR::com_ibm_rmi_io_FastPathForCollocated_isVMDeepCopySupported,    "isVMDeepCopySupported", "()Z")},
      {  TR::unknownMethod}
      };

   static X WASMethods[] =
      {
         {x(TR::com_ibm_ws_webcontainer_channel_WCCByteBufferOutputStream_printUnencoded, "printUnencoded", "(Ljava/lang/String;II)V")},
         {TR::unknownMethod}
      };

   static X IntegerMethods[] =
      {
      {x(TR::java_lang_Integer_bitCount,                "bitCount",              "(I)I")},
      {x(TR::java_lang_Integer_highestOneBit,           "highestOneBit",         "(I)I")},
      {x(TR::java_lang_Integer_lowestOneBit,            "lowestOneBit",          "(I)I")},
      {x(TR::java_lang_Integer_numberOfLeadingZeros,    "numberOfLeadingZeros",  "(I)I")},
      {x(TR::java_lang_Integer_numberOfTrailingZeros,   "numberOfTrailingZeros", "(I)I")},
      {x(TR::java_lang_Integer_reverseBytes,            "reverseBytes",          "(I)I")},
      {x(TR::java_lang_Integer_rotateLeft,              "rotateLeft",            "(II)I")},
      {x(TR::java_lang_Integer_rotateRight,             "rotateRight",           "(II)I")},
      {x(TR::java_lang_Integer_valueOf,                 "valueOf",               "(I)Ljava/lang/Integer;")},
      {  TR::java_lang_Integer_init,              6,    "<init>", (int16_t)-1,    "*"},
      {  TR::unknownMethod}
      };

   static X LongMethods[] =
      {
      {x(TR::java_lang_Long_bitCount,                   "bitCount",              "(J)I")},
      {x(TR::java_lang_Long_highestOneBit,              "highestOneBit",         "(J)J")},
      {x(TR::java_lang_Long_lowestOneBit,               "lowestOneBit",          "(J)J")},
      {x(TR::java_lang_Long_numberOfLeadingZeros,       "numberOfLeadingZeros",  "(J)I")},
      {x(TR::java_lang_Long_numberOfTrailingZeros,      "numberOfTrailingZeros", "(J)I")},
      {x(TR::java_lang_Long_reverseBytes,              "reverseBytes",           "(J)J")},
      {x(TR::java_lang_Long_rotateLeft,                 "rotateLeft",            "(JI)J")},
      {x(TR::java_lang_Long_rotateRight,                "rotateRight",           "(JI)J")},
      {  TR::java_lang_Long_init,                  6,    "<init>", (int16_t)-1,    "*"},
      {  TR::unknownMethod}
      };

   static X BooleanMethods[] =
      {
      {  TR::java_lang_Boolean_init,          6,    "<init>", (int16_t)-1,    "*"},
      {  TR::unknownMethod}
      };

   static X CharacterMethods[] =
      {
      {  TR::java_lang_Character_init,          6,    "<init>", (int16_t)-1,    "*"},
      {x(TR::java_lang_Character_isDigit,             "isDigit",              "(I)Z")},
      {x(TR::java_lang_Character_isLetter,            "isLetter",             "(I)Z")},
      {x(TR::java_lang_Character_isUpperCase,         "isUpperCase",          "(I)Z")},
      {x(TR::java_lang_Character_isLowerCase,         "isLowerCase",          "(I)Z")},
      {x(TR::java_lang_Character_isWhitespace,        "isWhitespace",         "(I)Z")},
      {x(TR::java_lang_Character_isAlphabetic,        "isAlphabetic",         "(I)Z")},
      {  TR::unknownMethod}
      };

   static X CRC32Methods[] =
      {
      {x(TR::java_util_zip_CRC32_update,              "update",               "(II)I")},
      {x(TR::java_util_zip_CRC32_updateBytes,         "updateBytes",          "(I[BII)I")},
      {x(TR::java_util_zip_CRC32_updateByteBuffer,    "updateByteBuffer",     "(IJII)I")},
      {  TR::unknownMethod}
      };

   static X ByteMethods[] =
      {
      {  TR::java_lang_Byte_init,          6,    "<init>", (int16_t)-1,    "*"},
      {  TR::unknownMethod}
      };

   static X ShortMethods[] =
      {
      {x(TR::java_lang_Short_reverseBytes,             "reverseBytes",         "(S)S")},
      {  TR::java_lang_Short_init,          6,    "<init>", (int16_t)-1,    "*"},
      {  TR::unknownMethod}
      };

   static X ReflectionMethods[] =
      {
      {x(TR::sun_reflect_Reflection_getCallerClass, "getCallerClass", "(I)Ljava/lang/Class;")},
      {x(TR::sun_reflect_Reflection_getClassAccessFlags, "getClassAccessFlags", "(Ljava/lang/Class;)I")},
      {  TR::unknownMethod}
      };

   static X X10Methods [] =
      {
      {x(TR::x10JITHelpers_speculateIndex, "speculateIndex","(II)I")},
      {x(TR::x10JITHelpers_getCPU, "getCPU", "()I")},
      {x(TR::x10JITHelpers_noBoundsCheck, "noBoundsCheck", "(I)I")},
      {x(TR::x10JITHelpers_noNullCheck, "noNullCheck", "(Ljava/lang/Object;)Ljava/lang/Object;")},
      {x(TR::x10JITHelpers_noCastCheck, "noCastCheck", "(Ljava/lang/Object;)Ljava/lang/Object;")},
      {x(TR::x10JITHelpers_checkLowBounds, "checkLowBound", "(II)I")},
      {x(TR::x10JITHelpers_checkHighBounds, "checkHighBound", "(II)I")},
      { TR::unknownMethod}
      };

   static X SubMapMethods[] =
      {
      {x(TR::java_util_TreeMapSubMap_setLastKey,      "setLastKey",      "()V")},
      {x(TR::java_util_TreeMapSubMap_setFirstKey,     "setFirstKey",     "()V")},
      {  TR::unknownMethod}
      };

   static X VMInternalsMethods[] =
      {
      {x(TR::java_lang_Class_newInstanceImpl,      "newInstanceImpl",      "(Ljava/lang/Class;)Ljava/lang/Object;")},
      {x(TR::java_lang_J9VMInternals_is32Bit,                             "is32Bit", "()Z")},
      {x(TR::java_lang_J9VMInternals_isClassModifierPublic,               "isClassModifierPublic", "(I)Z")},
      {x(TR::java_lang_J9VMInternals_getArrayLengthAsObject,              "getArrayLengthAsObject", "(Ljava/lang/Object;)I")},
      {x(TR::java_lang_J9VMInternals_rawNewInstance,                      "rawNewInstance", "(Ljava/lang/Class;)Ljava/lang/Object;")},
      {x(TR::java_lang_J9VMInternals_rawNewArrayInstance,                 "rawNewArrayInstance", "(ILjava/lang/Class;)Ljava/lang/Object;")},
      {x(TR::java_lang_J9VMInternals_defaultClone,                        "defaultClone", "(Ljava/lang/Object;)Ljava/lang/Object;")},
      {x(TR::java_lang_J9VMInternals_getNumBitsInReferenceField,          "getNumBitsInReferenceField", "()I")},
      {x(TR::java_lang_J9VMInternals_getNumBytesInReferenceField,         "getNumBytesInReferenceField", "()I")},
      {x(TR::java_lang_J9VMInternals_getNumBitsInDescriptionWord,         "getNumBitsInDescriptionWord", "()I")},
      {x(TR::java_lang_J9VMInternals_getNumBytesInDescriptionWord,        "getNumBytesInDescriptionWord", "()I")},
      {x(TR::java_lang_J9VMInternals_getNumBytesInJ9ObjectHeader,         "getNumBytesInJ9ObjectHeader", "()I")},

      {x(TR::java_lang_J9VMInternals_getJ9ClassFromClass32,               "getJ9ClassFromClass32", "(Ljava/lang/Class;)I")},
      {x(TR::java_lang_J9VMInternals_getInstanceShapeFromJ9Class32,       "getTotalInstanceSizeFromJ9Class32", "(I)I")},
      {x(TR::java_lang_J9VMInternals_getInstanceDescriptionFromJ9Class32, "getInstanceDescriptionFromJ9Class32", "(I)I")},
      {x(TR::java_lang_J9VMInternals_getDescriptionWordFromPtr32,         "getDescriptionWordFromPtr32", "(I)I")},

      {x(TR::java_lang_J9VMInternals_getJ9ClassFromClass64,               "getJ9ClassFromClass64", "(Ljava/lang/Class;)J")},
      {x(TR::java_lang_J9VMInternals_getInstanceShapeFromJ9Class64,       "getTotalInstanceSizeFromJ9Class64", "(J)J")},
      {x(TR::java_lang_J9VMInternals_getInstanceDescriptionFromJ9Class64, "getInstanceDescriptionFromJ9Class64", "(J)J")},
      {x(TR::java_lang_J9VMInternals_getDescriptionWordFromPtr64,         "getDescriptionWordFromPtr64", "(J)J")},
      {x(TR::java_lang_J9VMInternals_getSuperclass,                       "getSuperclass", "(Ljava/lang/Class;)Ljava/lang/Class;")},
      {x(TR::java_lang_J9VMInternals_identityHashCode,                    "identityHashCode", "(Ljava/lang/Object;)I")},
      {x(TR::java_lang_J9VMInternals_fastIdentityHashCode,                "fastIdentityHashCode", "(Ljava/lang/Object;)I")},
      {x(TR::java_lang_J9VMInternals_primitiveClone,                      "primitiveClone", "(Ljava/lang/Object;)Ljava/lang/Object;")},
      {  TR::unknownMethod}
      };

   static X ReferenceMethods [] =
      {
      {x(TR::java_lang_ref_Reference_getImpl, "getImpl", "()Ljava/lang/Object;")},
      {x(TR::java_lang_ref_Reference_reachabilityFence, "reachabilityFence", "(Ljava/lang/Object;)V")},
      {TR::unknownMethod}
      };

   static X JavaUtilConcurrentAtomicFencesMethods[] =
      {
      {x(TR::java_util_concurrent_atomic_Fences_postLoadFence,                            "postLoadFence",         "()V")},
      {x(TR::java_util_concurrent_atomic_Fences_preStoreFence,                            "preStoreFence",         "()V")},
      {x(TR::java_util_concurrent_atomic_Fences_postStorePreLoadFence,                    "postStorePreLoadFence", "()V")},
      {x(TR::java_util_concurrent_atomic_Fences_postLoadFence_jlObject,                   "postLoadFence",         "(Ljava/lang/Object;)V")},
      {x(TR::java_util_concurrent_atomic_Fences_preStoreFence_jlObject,                   "preStoreFence",         "(Ljava/lang/Object;)V")},
      {x(TR::java_util_concurrent_atomic_Fences_postStorePreLoadFence_jlObject,           "postStorePreLoadFence", "(Ljava/lang/Object;)V")},
      {x(TR::java_util_concurrent_atomic_Fences_postLoadFence_jlObjectjlrField,           "postLoadFence",         "(Ljava/lang/Object;Ljava/lang/reflect/Field;)V")},
      {x(TR::java_util_concurrent_atomic_Fences_preStoreFence_jlObjectjlrField,           "preStoreFence",         "(Ljava/lang/Object;Ljava/lang/reflect/Field;)V")},
      {x(TR::java_util_concurrent_atomic_Fences_postStorePreLoadFence_jlObjectjlrField,   "postStorePreLoadFence", "(Ljava/lang/Object;Ljava/lang/reflect/Field;)V")},
      {x(TR::java_util_concurrent_atomic_Fences_postLoadFence_jlObjectI,                  "postLoadFence",         "([Ljava/lang/Object;I)V")},
      {x(TR::java_util_concurrent_atomic_Fences_preStoreFence_jlObjectI,                  "preStoreFence",         "([Ljava/lang/Object;I)V")},
      {x(TR::java_util_concurrent_atomic_Fences_postStorePreLoadFence_jlObjectI,          "postStorePreLoadFence", "([Ljava/lang/Object;I)V")},
      {x(TR::java_util_concurrent_atomic_Fences_orderAccesses,                            "orderAccesses",         "(Ljava/lang/Object;)V")},
      {x(TR::java_util_concurrent_atomic_Fences_orderReads,                               "orderReads",            "(Ljava/lang/Object;)V")},
      {x(TR::java_util_concurrent_atomic_Fences_orderWrites,                              "orderWrites",           "(Ljava/lang/Object;)V")},
      {x(TR::java_util_concurrent_atomic_Fences_reachabilityFence,                        "reachabilityFence",     "(Ljava/lang/Object;)V")},
      {  TR::unknownMethod}
      };

   //1421
   static X JavaUtilConcurrentAtomicIntegerMethods[] =
      {
      {x(TR::java_util_concurrent_atomic_AtomicInteger_getAndAdd,          "getAndAdd",          "(I)I")},
      {x(TR::java_util_concurrent_atomic_AtomicInteger_getAndIncrement,    "getAndIncrement",    "()I")},
      {x(TR::java_util_concurrent_atomic_AtomicInteger_getAndDecrement,    "getAndDecrement",    "()I")},
      {x(TR::java_util_concurrent_atomic_AtomicInteger_getAndSet,          "getAndSet",          "(I)I")},
      {x(TR::java_util_concurrent_atomic_AtomicInteger_addAndGet,          "addAndGet",          "(I)I")},
      {x(TR::java_util_concurrent_atomic_AtomicInteger_incrementAndGet,    "incrementAndGet",    "()I")},
      {x(TR::java_util_concurrent_atomic_AtomicInteger_decrementAndGet,    "decrementAndGet",    "()I")},
      {x(TR::java_util_concurrent_atomic_AtomicInteger_weakCompareAndSet,  "weakCompareAndSet",  "(II)Z")},
      {x(TR::java_util_concurrent_atomic_AtomicInteger_lazySet,            "lazySet",            "(I)V")},

      {TR::unknownMethod}
      };

   static X JavaUtilConcurrentAtomicIntegerArrayMethods[] =
      {
/*
      {x(TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndAdd,          "getAndAdd",          "(II)I")},
      {x(TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndIncrement,    "getAndIncrement",    "(I)I")},
      {x(TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndDecrement,    "getAndDecrement",    "(I)I")},
      {x(TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndSet,          "getAndSet",          "(II)I")},
      {x(TR::java_util_concurrent_atomic_AtomicIntegerArray_addAndGet,          "addAndGet",          "(II)I")},
      {x(TR::java_util_concurrent_atomic_AtomicIntegerArray_incrementAndGet,    "incrementAndGet",    "(I)I")},
      {x(TR::java_util_concurrent_atomic_AtomicIntegerArray_decrementAndGet,    "decrementAndGet",    "(I)I")},
      {x(TR::java_util_concurrent_atomic_AtomicIntegerArray_weakCompareAndSet,  "weakCompareAndSet",  "(III)Z")},
      {x(TR::java_util_concurrent_atomic_AtomicIntegerArray_lazySet,            "lazySet",            "(II)V")},
*/
      {TR::unknownMethod}
      };

   static X JavaUtilConcurrentAtomicIntegerFieldUpdaterMethods[] =
      {

      {x(TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_getAndAdd,          "getAndAdd",          "(Ljava/lang/Object;I)I")},
      {x(TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_getAndIncrement,    "getAndIncrement",    "(Ljava/lang/Object;)I")},
      {x(TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_getAndDecrement,    "getAndDecrement",    "(Ljava/lang/Object;)I")},
      {x(TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_getAndSet,          "getAndSet",          "(Ljava/lang/Object;I)I")},
      {x(TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_addAndGet,          "addAndGet",          "(Ljava/lang/Object;I)I")},
      {x(TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_incrementAndGet,    "incrementAndGet",    "(Ljava/lang/Object;)I")},
      {x(TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_decrementAndGet,    "decrementAndGet",    "(Ljava/lang/Object;)I")},
      {x(TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_weakCompareAndSet,  "weakCompareAndSet",  "(Ljava/lang/Object;II)Z")},
      {x(TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_lazySet,            "lazySet",            "(Ljava/lang/Object;I)V")},

      {TR::unknownMethod}
      };

   static X JavaUtilConcurrentAtomicBooleanMethods[] =
      {
      {x(TR::java_util_concurrent_atomic_AtomicBoolean_getAndSet,          "getAndSet",          "(Z)Z")},
      {TR::unknownMethod}
      };

   static X JavaUtilConcurrentAtomicReferenceMethods[] =
      {
/*    These are disabled because the codegens don't recognize them anymore.
 *    The reason codegens stopped recognizing these is because of missing wrtbar support.
 *    {x(TR::java_util_concurrent_atomic_AtomicReference_getAndSet,        "getAndSet",          "(Ljava/lang/Object;)Ljava/lang/Object;")},
      {x(TR::java_util_concurrent_atomic_AtomicReference_weakCompareAndSet,"weakCompareAndSet",  "(Ljava/lang/Object;Ljava/lang/Object;)Z")},
      {x(TR::java_util_concurrent_atomic_AtomicReference_lazySet,          "lazySet",            "(Ljava/lang/Object;)V")},
*/
      {TR::unknownMethod}
      };

   static X JavaUtilConcurrentAtomicReferenceArrayMethods[] =
      {
/*
      {x(TR::java_util_concurrent_atomic_AtomicReferenceArray_getAndSet,        "getAndSet",          "(ILjava/lang/Object;)Ljava/lang/Object;")},
      {x(TR::java_util_concurrent_atomic_AtomicReferenceArray_weakCompareAndSet,"weakCompareAndSet",  "(ILjava/lang/Object;Ljava/lang/Object;)Z")},
      {x(TR::java_util_concurrent_atomic_AtomicReferenceArray_lazySet,          "lazySet",            "(ILjava/lang/Object;)V")},
*/
      {TR::unknownMethod}
      };

   static X JavaUtilConcurrentAtomicReferenceFieldUpdaterMethods[] =
      {

      {x(TR::java_util_concurrent_atomic_AtomicReferenceFieldUpdater_getAndSet,        "getAndSet",          "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;")},
      {x(TR::java_util_concurrent_atomic_AtomicReferenceFieldUpdater_weakCompareAndSet,"weakCompareAndSet",  "(Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;)Z")},
      {x(TR::java_util_concurrent_atomic_AtomicReferenceFieldUpdater_lazySet,          "lazySet",            "(Ljava/lang/Object;Ljava/lang/Object;)V")},

      {TR::unknownMethod}
      };

  static X JavaUtilConcurrentAtomicMarkableReferenceMethods[] =
      {

      {x(TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordCAS,           "doubleWordCAS",          "(Ljava/util/concurrent/atomic/AtomicMarkableReference$ReferenceBooleanPair;Ljava/lang/Object;Ljava/lang/Object;ZZ)Z")},
      {x(TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordCASSupported,  "doubleWordCASSupported", "(Ljava/util/concurrent/atomic/AtomicMarkableReference$ReferenceBooleanPair;)Z")},
      {x(TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordSet,           "doubleWordSet",          "(Ljava/util/concurrent/atomic/AtomicMarkableReference$ReferenceBooleanPair;Ljava/lang/Object;Z)V")},
      {x(TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordSetSupported,  "doubleWordSetSupported", "(Ljava/util/concurrent/atomic/AtomicMarkableReference$ReferenceBooleanPair;)Z")},
      {x(TR::java_util_concurrent_atomic_AtomicMarkableReference_setDoubleWordCASSupported,"setDoubleWordCASSupported", "()Z")},
      {x(TR::java_util_concurrent_atomic_AtomicMarkableReference_setDoubleWordSetSupported,"setDoubleWordSetSupported", "()Z")},
      {x(TR::java_util_concurrent_atomic_AtomicMarkableReference_tmDoubleWordCAS,          "tmDoubleWordCAS",          "(Ljava/util/concurrent/atomic/AtomicMarkableReference$Pair;Ljava/lang/Object;Ljava/lang/Object;ZZ)I")},
      {x(TR::java_util_concurrent_atomic_AtomicMarkableReference_tmDoubleWordSet,          "tmDoubleWordSet",          "(Ljava/util/concurrent/atomic/AtomicMarkableReference$Pair;Ljava/lang/Object;Z)Z")},
      {x(TR::java_util_concurrent_atomic_AtomicMarkableReference_tmEnabled,                "tmEnabled",  "()Z")},

      {TR::unknownMethod}
      };

  static X JavaUtilConcurrentAtomicStampedReferenceMethods[] =
      {
      {x(TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordCAS,           "doubleWordCAS",          "(Ljava/util/concurrent/atomic/AtomicStampedReference$ReferenceIntegerPair;Ljava/lang/Object;Ljava/lang/Object;II)Z")},
      {x(TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordCASSupported,  "doubleWordCASSupported", "(Ljava/util/concurrent/atomic/AtomicStampedReference$ReferenceIntegerPair;)Z")},
      {x(TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordSet,           "doubleWordSet",          "(Ljava/util/concurrent/atomic/AtomicStampedReference$ReferenceIntegerPair;Ljava/lang/Object;I)V")},
      {x(TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordSetSupported,  "doubleWordSetSupported", "(Ljava/util/concurrent/atomic/AtomicStampedReference$ReferenceIntegerPair;)Z")},
      {x(TR::java_util_concurrent_atomic_AtomicStampedReference_setDoubleWordCASSupported,"setDoubleWordCASSupported", "()Z")},
      {x(TR::java_util_concurrent_atomic_AtomicStampedReference_setDoubleWordSetSupported,"setDoubleWordSetSupported", "()Z")},
      {x(TR::java_util_concurrent_atomic_AtomicStampedReference_tmDoubleWordCAS,          "tmDoubleWordCAS",          "(Ljava/util/concurrent/atomic/AtomicStampedReference$Pair;Ljava/lang/Object;Ljava/lang/Object;II)I")},
      {x(TR::java_util_concurrent_atomic_AtomicStampedReference_tmDoubleWordSet,          "tmDoubleWordSet",          "(Ljava/util/concurrent/atomic/AtomicStampedReference$Pair;Ljava/lang/Object;I)Z")},
      {x(TR::java_util_concurrent_atomic_AtomicStampedReference_tmEnabled,                "tmEnabled",  "()Z")},
      {TR::unknownMethod}
      };

   static X JavaUtilConcurrentAtomicLongMethods[] =
      {

      {x(TR::java_util_concurrent_atomic_AtomicLong_addAndGet,             "addAndGet",          "(J)J")},
      {x(TR::java_util_concurrent_atomic_AtomicLong_decrementAndGet,       "decrementAndGet",    "()J")},
      {x(TR::java_util_concurrent_atomic_AtomicLong_getAndAdd,             "getAndAdd",          "(J)J")},
      {x(TR::java_util_concurrent_atomic_AtomicLong_getAndDecrement,       "getAndDecrement",    "()J")},
      {x(TR::java_util_concurrent_atomic_AtomicLong_getAndIncrement,       "getAndIncrement",    "()J")},
      {x(TR::java_util_concurrent_atomic_AtomicLong_getAndSet,             "getAndSet",          "(J)J")},
      {x(TR::java_util_concurrent_atomic_AtomicLong_incrementAndGet,       "incrementAndGet",    "()J")},
      {x(TR::java_util_concurrent_atomic_AtomicLong_weakCompareAndSet,     "weakCompareAndSet",  "(JJ)Z")},
      {x(TR::java_util_concurrent_atomic_AtomicLong_lazySet,               "lazySet",            "(J)V")},

      {TR::unknownMethod}
      };

   static X JavaUtilConcurrentAtomicLongArrayMethods[] =
      {
/*
      {x(TR::java_util_concurrent_atomic_AtomicLongArray_getAndAdd,          "getAndAdd",          "(IJ)J")},
      {x(TR::java_util_concurrent_atomic_AtomicLongArray_getAndIncrement,    "getAndIncrement",    "(I)J")},
      {x(TR::java_util_concurrent_atomic_AtomicLongArray_getAndDecrement,    "getAndDecrement",    "(I)J")},
      {x(TR::java_util_concurrent_atomic_AtomicLongArray_getAndSet,          "getAndSet",          "(IJ)J")},
      {x(TR::java_util_concurrent_atomic_AtomicLongArray_addAndGet,          "addAndGet",          "(IJ)J")},
      {x(TR::java_util_concurrent_atomic_AtomicLongArray_incrementAndGet,    "incrementAndGet",    "(I)J")},
      {x(TR::java_util_concurrent_atomic_AtomicLongArray_decrementAndGet,    "decrementAndGet",    "(I)J")},
      {x(TR::java_util_concurrent_atomic_AtomicLongArray_weakCompareAndSet,  "weakCompareAndSet",  "(IJJ)Z")},
      {x(TR::java_util_concurrent_atomic_AtomicLongArray_lazySet,            "lazySet",            "(IJ)V")},
*/
      {TR::unknownMethod}
      };

   static X JavaUtilConcurrentAtomicLongFieldUpdaterMethods[] =
      {

      {x(TR::java_util_concurrent_atomic_AtomicLongFieldUpdater_getAndAdd,          "getAndAdd",          "(Ljava/lang/Object;J)J")},
      {x(TR::java_util_concurrent_atomic_AtomicLongFieldUpdater_getAndIncrement,    "getAndIncrement",    "(Ljava/lang/Object;)J")},
      {x(TR::java_util_concurrent_atomic_AtomicLongFieldUpdater_getAndDecrement,    "getAndDecrement",    "(Ljava/lang/Object;)J")},
      {x(TR::java_util_concurrent_atomic_AtomicLongFieldUpdater_getAndSet,          "getAndSet",          "(Ljava/lang/Object;J)J")},
      {x(TR::java_util_concurrent_atomic_AtomicLongFieldUpdater_addAndGet,          "addAndGet",          "(Ljava/lang/Object;J)J")},
      {x(TR::java_util_concurrent_atomic_AtomicLongFieldUpdater_incrementAndGet,    "incrementAndGet",    "(Ljava/lang/Object;)J")},
      {x(TR::java_util_concurrent_atomic_AtomicLongFieldUpdater_decrementAndGet,    "decrementAndGet",    "(Ljava/lang/Object;)J")},
      {x(TR::java_util_concurrent_atomic_AtomicLongFieldUpdater_weakCompareAndSet,  "weakCompareAndSet",  "(Ljava/lang/Object;JJ)Z")},
      {x(TR::java_util_concurrent_atomic_AtomicLongFieldUpdater_lazySet,            "lazySet",            "(Ljava/lang/Object;J)V")},

      {TR::unknownMethod}
      };

   static X JavaUtilConcurrentConcurrentHashMapMethods[] =
      {
      {x(TR::java_util_concurrent_ConcurrentHashMap_addCount,    "addCount",     "(JI)V")},
      {x(TR::java_util_concurrent_ConcurrentHashMap_tryPresize,  "tryPresize",   "(I)V")},
      {x(TR::java_util_concurrent_ConcurrentHashMap_transfer,    "transfer",     "([Ljava/util/concurrent/ConcurrentHashMap$Node;[Ljava/util/concurrent/ConcurrentHashMap$Node;)V")},
      {x(TR::java_util_concurrent_ConcurrentHashMap_fullAddCount,"fullAddCount", "(JZ)V")},
      {x(TR::java_util_concurrent_ConcurrentHashMap_helpTransfer,"helpTransfer", "([Ljava/util/concurrent/ConcurrentHashMap$Node;Ljava/util/concurrent/ConcurrentHashMap$Node;)[Ljava/util/concurrent/ConcurrentHashMap$Node;")},
      {x(TR::java_util_concurrent_ConcurrentHashMap_initTable,   "initTable",    "()[Ljava/util/concurrent/ConcurrentHashMap$Node;")},
      {x(TR::java_util_concurrent_ConcurrentHashMap_tabAt,       "tabAt",        "([Ljava/util/concurrent/ConcurrentHashMap$Node;I)Ljava/util/concurrent/ConcurrentHashMap$Node;")},
      {x(TR::java_util_concurrent_ConcurrentHashMap_casTabAt,    "casTabAt",     "([Ljava/util/concurrent/ConcurrentHashMap$Node;ILjava/util/concurrent/ConcurrentHashMap$Node;Ljava/util/concurrent/ConcurrentHashMap$Node;)Z")},
      {x(TR::java_util_concurrent_ConcurrentHashMap_setTabAt,    "setTabAt",     "([Ljava/util/concurrent/ConcurrentHashMap$Node;ILjava/util/concurrent/ConcurrentHashMap$Node;)V")},
      {TR::unknownMethod}
      };

   static X JavaUtilConcurrentConcurrentHashMapTreeBinMethods[] =
      {
      {x(TR::java_util_concurrent_ConcurrentHashMap_TreeBin_lockRoot,      "lockRoot",      "")},
      {x(TR::java_util_concurrent_ConcurrentHashMap_TreeBin_contendedLock, "contendedLock", "")},
      {x(TR::java_util_concurrent_ConcurrentHashMap_TreeBin_find,          "find",          "")},
      {TR::unknownMethod}
      };

   // Transactional Memory
   static X JavaUtilConcurrentConcurrentHashMapSegmentMethods[] =
      {
      {x(TR::java_util_concurrent_ConcurrentHashMap_tmPut,       "tmPut", "(ILjava/util/concurrent/ConcurrentHashMap$HashEntry;)I")},
      {x(TR::java_util_concurrent_ConcurrentHashMap_tmRemove,    "tmRemove", "(Ljava/lang/Object;ILjava/lang/Object;)Ljava/lang/Object;")},
      {x(TR::java_util_concurrent_ConcurrentHashMap_tmEnabled,   "tmEnabled",  "()Z")},

      {TR::unknownMethod}
      };

   // Transactional Memory
   static X JavaUtilConcurrentConcurrentLinkedQueueMethods[] =
      {
      {x(TR::java_util_concurrent_ConcurrentLinkedQueue_tmOffer,     "tmOffer", "(Ljava/util/concurrent/ConcurrentLinkedQueue$Node;)I")},
      {x(TR::java_util_concurrent_ConcurrentLinkedQueue_tmPoll,      "tmPoll",  "()Ljava/lang/Object;")},
      {x(TR::java_util_concurrent_ConcurrentLinkedQueue_tmEnabled,   "tmEnabled",  "()Z")},

      {TR::unknownMethod}
      };

   static X QuadMethods[] =
      {
      {x(TR::com_ibm_Compiler_Internal_Quad_enableQuadOptimization, "enableQuadOptimization", "()Z")},
      {x(TR::com_ibm_Compiler_Internal_Quad_add_ql,                 "add",                    "(Lcom/ibm/Compiler/Internal/Quad;J)Lcom/ibm/Compiler/Internal/Quad;")},
      {x(TR::com_ibm_Compiler_Internal_Quad_add_ll,                 "add",                    "(JJ)Lcom/ibm/Compiler/Internal/Quad;")},
      {x(TR::com_ibm_Compiler_Internal_Quad_sub_ql,                 "sub",                    "(Lcom/ibm/Compiler/Internal/Quad;J)Lcom/ibm/Compiler/Internal/Quad;")},
      {x(TR::com_ibm_Compiler_Internal_Quad_sub_ll,                 "sub",                    "(JJ)Lcom/ibm/Compiler/Internal/Quad;")},
      {x(TR::com_ibm_Compiler_Internal_Quad_mul_ll,                 "mul",                    "(JJ)Lcom/ibm/Compiler/Internal/Quad;")},
      {x(TR::com_ibm_Compiler_Internal_Quad_hi,                     "hi",                     "(Lcom/ibm/Compiler/Internal/Quad;)J")},
      {x(TR::com_ibm_Compiler_Internal_Quad_lo,                     "lo",                     "(Lcom/ibm/Compiler/Internal/Quad;)J")},
      {  TR::unknownMethod}
      };

   static X MethodHandleMethods[] =
      {
      {  TR::java_lang_invoke_MethodHandle_doCustomizationLogic     ,   20, "doCustomizationLogic",       (int16_t)-1, "*"},
      {  TR::java_lang_invoke_MethodHandle_undoCustomizationLogic   ,   22, "undoCustomizationLogic",     (int16_t)-1, "*"},
      {  TR::java_lang_invoke_MethodHandle_invoke                   ,    6, "invoke",                     (int16_t)-1, "*"},
      {  TR::java_lang_invoke_MethodHandle_invoke                   ,   13, "invokeGeneric",              (int16_t)-1, "*"}, // Older name from early versions of the jsr292 spec
      {  TR::java_lang_invoke_MethodHandle_invokeExact              ,   11, "invokeExact",                (int16_t)-1, "*"},
      {  TR::java_lang_invoke_MethodHandle_invokeExactTargetAddress ,   24, "invokeExactTargetAddress",   (int16_t)-1, "*"},
      {x(TR::java_lang_invoke_MethodHandle_invokeWithArgumentsHelper,   "invokeWithArgumentsHelper",  "(Ljava/lang/invoke/MethodHandle;[Ljava/lang/Object;)Ljava/lang/Object;")},
      {x(TR::java_lang_invoke_MethodHandle_asType, "asType", "(Ljava/lang/invoke/MethodHandle;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/MethodHandle;")},
      {x(TR::java_lang_invoke_MethodHandle_asType_instance, "asType", "(Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/MethodHandle;")},
      {  TR::unknownMethod}
      };

   static X VarHandleMethods[] =
      {
      // Recognized method only works for resovled methods
      // Resolved VarHandle access methods are suffixed with _impl in their names
      // A list for unresolved VarHandle access methods is in VarHandleTransformer.cpp,
      // changes in the following list need to be reflected in the other
      {  TR::java_lang_invoke_VarHandle_get                       ,    8, "get_impl",                             (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_set                       ,    8, "set_impl",                             (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_getVolatile               ,   16, "getVolatile_impl",                     (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_setVolatile               ,   16, "setVolatile_impl",                     (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_getOpaque                 ,   14, "getOpaque_impl",                       (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_setOpaque                 ,   14, "setOpaque_impl",                       (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_getAcquire                ,   15, "getAcquire_impl",                      (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_setRelease                ,   15, "setRelease_impl",                      (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_compareAndSet             ,   18, "compareAndSet_impl",                   (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_compareAndExchange        ,   23, "compareAndExchange_impl",              (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_compareAndExchangeAcquire ,   30, "compareAndExchangeAcquire_impl",       (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_compareAndExchangeRelease ,   30, "compareAndExchangeRelease_impl",       (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_weakCompareAndSet         ,   22, "weakCompareAndSet_impl",               (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_weakCompareAndSetAcquire  ,   29, "weakCompareAndSetAcquire_impl",        (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_weakCompareAndSetRelease  ,   29, "weakCompareAndSetRelease_impl",        (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_weakCompareAndSetPlain    ,   27, "weakCompareAndSetPlain_impl",          (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_getAndSet                 ,   14, "getAndSet_impl",                       (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_getAndSetAcquire          ,   21, "getAndSetAcquire_impl",                (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_getAndSetRelease          ,   21, "getAndSetRelease_impl",                (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_getAndAdd                 ,   14, "getAndAdd_impl",                       (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_getAndAddAcquire          ,   21, "getAndAddAcquire_impl",                (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_getAndAddRelease          ,   21, "getAndAddRelease_impl",                (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_getAndBitwiseAnd          ,   21, "getAndBitwiseAnd_impl",                (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_getAndBitwiseAndAcquire   ,   28, "getAndBitwiseAndAcquire_impl",         (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_getAndBitwiseAndRelease   ,   28, "getAndBitwiseAndRelease_impl",         (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_getAndBitwiseOr           ,   20, "getAndBitwiseOr_impl",                 (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_getAndBitwiseOrAcquire    ,   28, "getAndBitwiseAndAcquire_impl",         (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_getAndBitwiseOrRelease    ,   28, "getAndBitwiseAndRelease_impl",         (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_getAndBitwiseXor          ,   20, "getAndBitwiseOr_impl",                 (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_getAndBitwiseXorAcquire   ,   28, "getAndBitwiseAndAcquire_impl",         (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_getAndBitwiseXorRelease   ,   28, "getAndBitwiseAndRelease_impl",         (int16_t)-1, "*"},
      {  TR::unknownMethod}
      };

   static X ILGenMacrosMethods[] =
      {
      {  TR::java_lang_invoke_ILGenMacros_placeholder ,      11, "placeholder",      (int16_t)-1, "*"},
      {  TR::java_lang_invoke_ILGenMacros_numArguments,      12, "numArguments",     (int16_t)-1, "*"},
      {  TR::java_lang_invoke_ILGenMacros_populateArray,     13, "populateArray",    (int16_t)-1, "*"},
      {  TR::java_lang_invoke_ILGenMacros_arrayElements,     13, "arrayElements",    (int16_t)-1, "*"},
      {x(TR::java_lang_invoke_ILGenMacros_arrayLength,           "arrayLength",      "(Ljava/lang/Object;)I")},
      {  TR::java_lang_invoke_ILGenMacros_firstN,             6, "firstN",           (int16_t)-1, "*"},
      {  TR::java_lang_invoke_ILGenMacros_dropFirstN,        10, "dropFirstN",       (int16_t)-1, "*"},
      {  TR::java_lang_invoke_ILGenMacros_getField,           8, "getField",         (int16_t)-1, "*"},
      {x(TR::java_lang_invoke_ILGenMacros_invokeExact_X,         "invokeExact_X",    "(Ljava/lang/invoke/MethodHandle;I)I")},
      {x(TR::java_lang_invoke_ILGenMacros_invokeExactAndFixup,   "invokeExact",      "(Ljava/lang/invoke/MethodHandle;I)I")},
      {x(TR::java_lang_invoke_ILGenMacros_isCustomThunk,         "isCustomThunk",    "()Z")},
      {x(TR::java_lang_invoke_ILGenMacros_isShareableThunk,      "isShareableThunk", "()Z")},
      {  TR::java_lang_invoke_ILGenMacros_lastN,              5, "lastN",            (int16_t)-1, "*"},
      {  TR::java_lang_invoke_ILGenMacros_middleN,            7, "middleN",          (int16_t)-1, "*"},
      {x(TR::java_lang_invoke_ILGenMacros_rawNew,                "rawNew",           "(Ljava/lang/Class;)Ljava/lang/Object;")},
      {x(TR::java_lang_invoke_ILGenMacros_parameterCount,        "parameterCount",   "(Ljava/lang/invoke/MethodHandle;)I")},
      {  TR::java_lang_invoke_ILGenMacros_push,               4, "push",             (int16_t)-1, "*"},
      {  TR::java_lang_invoke_ILGenMacros_pop,                5, "pop_I",            (int16_t)-1, "*"},
      {  TR::java_lang_invoke_ILGenMacros_pop,                5, "pop_J",            (int16_t)-1, "*"},
      {  TR::java_lang_invoke_ILGenMacros_pop,                5, "pop_F",            (int16_t)-1, "*"},
      {  TR::java_lang_invoke_ILGenMacros_pop,                5, "pop_D",            (int16_t)-1, "*"},
      {  TR::java_lang_invoke_ILGenMacros_pop,                5, "pop_L",            (int16_t)-1, "*"},
      {x(TR::java_lang_invoke_ILGenMacros_typeCheck,             "typeCheck",        "(Ljava/lang/invoke/MethodHandle;Ljava/lang/invoke/MethodType;)V")},
      {  TR::unknownMethod}
      };

   static X CollectHandleMethods[] =
      {
      {x(TR::java_lang_invoke_CollectHandle_numArgsToPassThrough,          "numArgsToPassThrough",        "()I")},
      {x(TR::java_lang_invoke_CollectHandle_numArgsToCollect,              "numArgsToCollect",            "()I")},
	  {x(TR::java_lang_invoke_CollectHandle_collectionStart,     	       "collectionStart",             "()I")},
	  {x(TR::java_lang_invoke_CollectHandle_numArgsAfterCollectArray,      "numArgsAfterCollectArray",    "()I")},
      {  TR::unknownMethod}
      };

   static X AsTypeHandleMethods[] =
      {
      {  TR::java_lang_invoke_AsTypeHandle_convertArgs,   11, "convertArgs",     (int16_t)-1, "*"},
      {  TR::unknownMethod}
      };

   static X EncodeMethods[] =
      {
      {x(TR::sun_nio_cs_ISO_8859_1_Encoder_encodeArrayLoop,       "encodeArrayLoop", "(Ljava/nio/CharBuffer;Ljava/nio/ByteBuffer;)Ljava/nio/charset/CoderResult;")},
      {x(TR::sun_nio_cs_ISO_8859_1_Encoder_encodeISOArray,        "encodeISOArray",         "([CI[BII)I")},
      {x(TR::sun_nio_cs_ISO_8859_1_Decoder_decodeISO8859_1,       "decodeISO8859_1",      "([BII[CI)I")},
      {x(TR::sun_nio_cs_US_ASCII_Encoder_encodeASCII,             "encodeASCII",             "([CII[BI)I")},
      {x(TR::sun_nio_cs_US_ASCII_Decoder_decodeASCII,             "decodeASCII",             "([BII[CI)I")},
      {x(TR::sun_nio_cs_ext_SBCS_Encoder_encodeSBCS,              "encodeSBCS",              "([CII[BI[B)I")},
      {x(TR::sun_nio_cs_ext_SBCS_Decoder_decodeSBCS,              "decodeSBCS",           "([BII[CI[C)I")},
      {x(TR::sun_nio_cs_UTF_8_Encoder_encodeUTF_8,                "encodeUTF_8",     "([CII[BI)I")},
      {x(TR::sun_nio_cs_UTF_8_Decoder_decodeUTF_8,                "decodeUTF_8",          "([BII[CI)I")},
      {x(TR::sun_nio_cs_UTF_16_Encoder_encodeUTF16Big,            "encodeUTF16Big",       "([CII[BI)I")},
      {x(TR::sun_nio_cs_UTF_16_Encoder_encodeUTF16Little,         "encodeUTF16Little",    "([CII[BI)I")},
      {  TR::unknownMethod}
      };

   static X IBM1388EncoderMethods[] =
      {
      {x(TR::sun_nio_cs_ext_IBM1388_Encoder_encodeArrayLoop,      "encodeArrayLoop", "(Ljava/nio/CharBuffer;Ljava/nio/ByteBuffer;)Ljava/nio/charset/CoderResult;")},
      {  TR::unknownMethod}
      };

   static X ExplicitCastHandleMethods[] =
      {
      {  TR::java_lang_invoke_ExplicitCastHandle_convertArgs,   11, "convertArgs",     (int16_t)-1, "*"},
      {  TR::unknownMethod}
      };

   static X ArgumentMoverHandleMethods[] =
      {
      {x(TR::java_lang_invoke_ArgumentMoverHandle_permuteArgs,  "permuteArgs",   "(I)I")},
      {  TR::unknownMethod}
      };

   static X PermuteHandleMethods[] =
      {
      {x(TR::java_lang_invoke_PermuteHandle_permuteArgs, "permuteArgs", "(I)I")},
      {  TR::unknownMethod}
      };

   static X GuardWithTestHandleMethods[] =
      {
      {x(TR::java_lang_invoke_GuardWithTestHandle_numGuardArgs,  "numGuardArgs", "()I")},
      {  TR::unknownMethod}
      };

   static X InsertHandleMethods[] =
      {
      {x(TR::java_lang_invoke_InsertHandle_numPrefixArgs,      "numPrefixArgs",     "()I")},
      {x(TR::java_lang_invoke_InsertHandle_numSuffixArgs,      "numSuffixArgs",     "()I")},
      {x(TR::java_lang_invoke_InsertHandle_numValuesToInsert,  "numValuesToInsert", "()I")},
      {  TR::unknownMethod}
      };

   static X DirectHandleMethods[] =
      {
      {x(TR::java_lang_invoke_DirectHandle_isAlreadyCompiled,   "isAlreadyCompiled",  "(J)Z")},
      {x(TR::java_lang_invoke_DirectHandle_compiledEntryPoint,  "compiledEntryPoint", "(J)J")},
      {  TR::unknownMethod}
      };

   static X SpreadHandleMethods[] =
      {
      {x(TR::java_lang_invoke_SpreadHandle_numArgsToPassThrough,  "numArgsToPassThrough",   "()I")},
      {x(TR::java_lang_invoke_SpreadHandle_numArgsToSpread,       "numArgsToSpread",        "()I")},
      {x(TR::java_lang_invoke_SpreadHandle_spreadStart,           "spreadStart",            "()I")},
      {x(TR::java_lang_invoke_SpreadHandle_numArgsAfterSpreadArray,       "numArgsAfterSpreadArray",        "()I")},
      {  TR::java_lang_invoke_SpreadHandle_arrayArg,           8, "arrayArg",  (int16_t)-1, "*"},
      {  TR::unknownMethod}
      };

   static X FoldHandleMethods[] =
      {
      {x(TR::java_lang_invoke_FoldHandle_foldPosition,               "foldPosition",            "()I")},
      {x(TR::java_lang_invoke_FoldHandle_argIndices,                 "argIndices",               "()I")},
      {  TR::java_lang_invoke_FoldHandle_argumentsForCombiner,  20,  "argumentsForCombiner",    (int16_t)-1, "*"},
      {  TR::unknownMethod}
      };

   static X FinallyHandleMethods[]=
      {
      {x(TR::java_lang_invoke_FinallyHandle_numFinallyTargetArgsToPassThrough,    "numFinallyTargetArgsToPassThrough",   "()I")},
      {  TR::unknownMethod}
      };

    static X FilterArgumentsHandleMethods[] =
      {
      {x(TR::java_lang_invoke_FilterArgumentsHandle_numPrefixArgs,         "numPrefixArgs",    "()I")},
      {x(TR::java_lang_invoke_FilterArgumentsHandle_numSuffixArgs,         "numSuffixArgs",    "()I")},
      {x(TR::java_lang_invoke_FilterArgumentsHandle_numArgsToFilter,       "numArgsToFilter",  "()I")},
      {x(TR::java_lang_invoke_FilterArgumentsHandle_filterArguments,       "filterArguments",  "([Ljava/lang/invoke/MethodHandle;I)I")},
      {  TR::unknownMethod}
      };

   static X CatchHandleMethods[] =
      {
      {x(TR::java_lang_invoke_CatchHandle_numCatchTargetArgsToPassThrough,       "numCatchTargetArgsToPassThrough",  "()I")},
      {  TR::unknownMethod}
      };

   static X MethodHandlesMethods[] =
      {
      {x(TR::java_lang_invoke_MethodHandles_getStackClass,   "getStackClass",  "(I)Ljava/lang/Class;")},
      {  TR::unknownMethod}
      };

   static X MutableCallSiteMethods[] =
      {
      {x(TR::java_lang_invoke_MutableCallSite_getTarget,   "getTarget",  "()Ljava/lang/invoke/MethodHandle;")},
      {  TR::unknownMethod}
      };

   static X GregorianCalendarMethods [] =
      {
      {x(TR::java_util_GregorianCalendar_computeFields, "computeFields", "(II)I")},
      {  TR::unknownMethod}
      };

   static X NativeThreadMethods[] =
      {
      {x(TR::sun_nio_ch_NativeThread_current, "current", "()J")},
      {  TR::unknownMethod}
      };


   static X  GPUMethods [] =
      {
      {x(TR::com_ibm_gpu_Kernel_blockIdxX,                   "getBlockIdxX",   "()I")},
      {x(TR::com_ibm_gpu_Kernel_blockIdxY,                   "getBlockIdxY",   "()I")},
      {x(TR::com_ibm_gpu_Kernel_blockIdxZ,                   "getBlockIdxZ",   "()I")},
      {x(TR::com_ibm_gpu_Kernel_blockDimX,                   "getBlockDimX",   "()I")},
      {x(TR::com_ibm_gpu_Kernel_blockDimY,                   "getBlockDimY",   "()I")},
      {x(TR::com_ibm_gpu_Kernel_blockDimZ,                   "getBlockDimZ",   "()I")},
      {x(TR::com_ibm_gpu_Kernel_threadIdxX,                  "getThreadIdxX",  "()I")},
      {x(TR::com_ibm_gpu_Kernel_threadIdxY,                  "getThreadIdxY",  "()I")},
      {x(TR::com_ibm_gpu_Kernel_threadIdxZ,                  "getThreadIdxZ",  "()I")},
      {x(TR::com_ibm_gpu_Kernel_syncThreads,                 "syncThreads",    "()V")},
      {  TR::unknownMethod}
      };


   static X SIMDMethods [] =
      {
      // ---- Integer Type Operations
      {x(TR::com_ibm_dataaccess_SIMD_vectorAddInt,           "vectorAddInt",    "([II[II[II)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorSubInt,           "vectorSubInt",    "([II[II[II)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorMulInt,           "vectorMulInt",    "([II[II[II)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorDivInt,           "vectorDivInt",    "([II[II[II)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorRemInt,           "vectorRemInt",    "([II[II[II)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorNegInt,           "vectorNegInt",    "([II[II)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorSplatsInt,        "vectorSplatsInt", "([III)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorMinInt,           "vectorMinInt",    "([II[II[II)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorMaxInt,           "vectorMaxInt",    "([II[II[II)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorStoreInt,         "vectorCopyInt",   "([II[II)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorStoreInt,         "vectorStoreInt",   "([II[II)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorCmpEqInt,         "vectorCmpEqInt",   "([II[II[II)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorCmpGeInt,         "vectorCmpGeInt",   "([II[II[II)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorCmpGtInt,         "vectorCmpGtInt",   "([II[II[II)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorCmpLeInt,         "vectorCmpLeInt",   "([II[II[II)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorCmpLtInt,         "vectorCmpLtInt",   "([II[II[II)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorCmpAllEqInt,      "vectorCmpAllEqInt",   "([II[II)Z")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorCmpAllGeInt,      "vectorCmpAllGeInt",   "([II[II)Z")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorCmpAllGtInt,      "vectorCmpAllGtInt",   "([II[II)Z")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorCmpAllLeInt,      "vectorCmpAllLeInt",   "([II[II)Z")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorCmpAllLtInt,      "vectorCmpAllLtInt",   "([II[II)Z")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorCmpAnyEqInt,      "vectorCmpAnyEqInt",   "([II[II)Z")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorCmpAnyGeInt,      "vectorCmpAnyGeInt",   "([II[II)Z")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorCmpAnyGtInt,      "vectorCmpAnyGtInt",   "([II[II)Z")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorCmpAnyLeInt,      "vectorCmpAnyLeInt",   "([II[II)Z")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorCmpAnyLtInt,      "vectorCmpAnyLtInt",   "([II[II)Z")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorAndInt,           "vectorAndInt",    "([II[II[II)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorOrInt,           "vectorOrInt",    "([II[II[II)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorXorInt,           "vectorXorInt",    "([II[II[II)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorNotInt,           "vectorNotInt",    "([II[II)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorGetElementInt,    "vectorGetElementInt", "([II)I")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorSetElementInt,    "vectorSetElementInt", "([III)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorMergeHighInt,     "vectorMergeHighInt", "([II[II[II)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorMergeLowInt,      "vectorMergeLowInt",  "([II[II[II)V")},

      // ---- Long Type Operations
      {x(TR::com_ibm_dataaccess_SIMD_vectorAddLong,           "vectorAddLong",    "([JI[JI[JI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorSubLong,           "vectorSubLong",    "([JI[JI[JI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorMulLong,           "vectorMulLong",    "([JI[JI[JI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorDivLong,           "vectorDivLong",    "([JI[JI[JI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorRemLong,           "vectorRemLong",    "([JI[JI[JI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorNegLong,           "vectorNegLong",    "([JI[JI)V")},


      {x(TR::com_ibm_dataaccess_SIMD_vectorStoreByte,         "vectorStoreByte",   "([BI[BI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorStoreChar,         "vectorStoreChar",   "([CI[CI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorStoreShort,        "vectorStoreShort",  "([SI[SI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorStoreLong,         "vectorStoreLong",   "([JI[JI)V")},

      {x(TR::com_ibm_dataaccess_SIMD_vectorSplatsByte,        "vectorSplatsByte",  "([BIB)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorSplatsChar,        "vectorSplatsChar",  "([CIC)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorSplatsShort,       "vectorSplatsShort", "([SIS)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorSplatsLong,        "vectorSplatsLong",  "([JIJ)V")},

      // ---- Float Type Operations
      {x(TR::com_ibm_dataaccess_SIMD_vectorAddFloat,           "vectorAddFloat",    "([FI[FI[FI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorSubFloat,           "vectorSubFloat",    "([FI[FI[FI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorMulFloat,           "vectorMulFloat",    "([FI[FI[FI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorDivFloat,           "vectorDivFloat",    "([FI[FI[FI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorNegFloat,           "vectorNegFloat",    "([FI[FI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorSplatsFloat,        "vectorSplatsFloat", "([FIF)V")},

      // ---- Double Type Operations
      {x(TR::com_ibm_dataaccess_SIMD_vectorAddDouble,           "vectorAddDouble",    "([DI[DI[DI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorMinDouble,           "vectorMinDouble",    "([DI[DI[DI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorMaxDouble,           "vectorMaxDouble",    "([DI[DI[DI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorAddress,             "vectorAddress", "([D)J")},

      {x(TR::com_ibm_dataaccess_SIMD_vectorAddReduceDouble,     "vectorAddReduceDouble", "([DI)D")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorCmpEqDouble,         "vectorCmpEqDouble",   "([JI[DI[DI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorCmpGeDouble,         "vectorCmpGeDouble",   "([JI[DI[DI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorCmpGtDouble,         "vectorCmpGtDouble",   "([JI[DI[DI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorCmpLeDouble,         "vectorCmpLeDouble",   "([JI[DI[DI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorCmpLtDouble,         "vectorCmpLtDouble",   "([JI[DI[DI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorCmpAllEqDouble,      "vectorCmpAllEqDouble",   "([DI[DI)Z")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorCmpAllGeDouble,      "vectorCmpAllGeDouble",   "([DI[DI)Z")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorCmpAllGtDouble,      "vectorCmpAllGtDouble",   "([DI[DI)Z")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorCmpAllLeDouble,      "vectorCmpAllLeDouble",   "([DI[DI)Z")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorCmpAllLtDouble,      "vectorCmpAllLtDouble",   "([DI[DI)Z")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorCmpAnyEqDouble,      "vectorCmpAnyEqDouble",   "([DI[DI)Z")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorCmpAnyGeDouble,      "vectorCmpAnyGeDouble",   "([DI[DI)Z")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorCmpAnyGtDouble,      "vectorCmpAnyGtDouble",   "([DI[DI)Z")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorCmpAnyLeDouble,      "vectorCmpAnyLeDouble",   "([DI[DI)Z")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorCmpAnyLtDouble,      "vectorCmpAnyLtDouble",   "([DI[DI)Z")},

      {x(TR::com_ibm_dataaccess_SIMD_vectorDivDouble,           "vectorDivDouble",    "([DI[DI[DI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorGetElementDouble,    "vectorGetElementDouble", "([DI)D")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorSetElementDouble,    "vectorSetElementDouble", "([DID)V")},

      {x(TR::com_ibm_dataaccess_SIMD_vectorLoadWithStrideDouble, "vectorLoadWithStrideDouble", "([DI[DII)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorLogDouble,            "vectorLogDouble",             "([DI[DI)V")},

      {x(TR::com_ibm_dataaccess_SIMD_vectorMaddDouble,          "vectorMaddDouble",      "([DI[DI[DI[DI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorMergeHighDouble,     "vectorMergeHighDouble", "([DI[DI[DI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorMergeLowDouble,      "vectorMergeLowDouble",  "([DI[DI[DI)V")},

      {x(TR::com_ibm_dataaccess_SIMD_vectorMulDouble,           "vectorMulDouble",    "([DI[DI[DI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorNegDouble,           "vectorNegDouble",    "([DI[DI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorNmsubDouble,         "vectorNmsubDouble",   "([DI[DI[DI[DI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorMsubDouble,          "vectorMsubDouble",   "([DI[DI[DI[DI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorSelDouble,           "vectorSelDouble",   "([DI[DI[DI[JI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorSplatsDouble,        "vectorSplatsDouble", "([DID)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorStoreDouble,         "vectorStoreDouble",  "([DI[DI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorStoreDouble,         "vectorCopyDouble",   "([DI[DI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorSubDouble,           "vectorSubDouble",    "([DI[DI[DI)V")},
      // {x(TR::com_ibm_dataaccess_SIMD_vectorRemDouble,           "vectorRemDouble",    "([DI[DI[DI)V")},
      {x(TR::com_ibm_dataaccess_SIMD_vectorSqrtDouble,           "vectorSqrtDouble",    "([DI[DI)V")},
      {  TR::unknownMethod}
      };

#if defined(ENABLE_SPMD_SIMD)
   static X SPMDKernelBaseMethods [] =
      {
      {x(TR::com_ibm_simt_SPMDKernel_execute,   "execute",   "(II)V")},
      {x(TR::com_ibm_simt_SPMDKernel_kernel,    "kernel",   "()V")},
      {  TR::unknownMethod}
      };
#endif

   static X JavaUtilStreamAbstractPipelineMethods [] =
      {
      {x(TR::java_util_stream_AbstractPipeline_evaluate, "evaluate", "(Ljava/util/stream/TerminalOp;)Ljava/lang/Object;")},
      {  TR::unknownMethod}
      };

   static X JavaUtilStreamIntPipelineMethods [] =
      {
      {x(TR::java_util_stream_IntPipeline_forEach, "forEach", "(Ljava/util/function/IntConsumer;)V")},
      {  TR::unknownMethod}
      };

   static X JavaUtilStreamIntPipelineHeadMethods [] =
      {
      {x(TR::java_util_stream_IntPipelineHead_forEach, "forEach", "(Ljava/util/function/IntConsumer;)V")},
      {  TR::unknownMethod}
      };

   static X MTTenantContext[] =
      {
      {x(TR::com_ibm_tenant_TenantContext_switchTenant, "switchTenant", "(Lcom/ibm/tenant/TenantContext;)V")},
      {x(TR::com_ibm_tenant_TenantContext_attach, "attach", "()V")},
      {x(TR::com_ibm_tenant_TenantContext_detach, "detach", "()V")},
      {x(TR::com_ibm_tenant_InternalTenantContext_setCurrent, "setCurrent", "(Lcom/ibm/tenant/TenantContext;)I")},
      {  TR::unknownMethod}
      };

   static X JavaLangRefSoftReferenceMethods [] =
      {
      {x(TR::java_lang_ref_SoftReference_get, "get", "()Ljava/lang/Object;")},
      {  TR::unknownMethod},
      };

   struct Y { const char * _class; X * _methods; };

   static Y class13[] =
      {
      { "java/nio/Bits", BitsMethods },
      { 0 }
      };

   static Y class14[] =
      {
      { "java/lang/Math", MathMethods },
      { "java/lang/Long", LongMethods },
      { "java/lang/Byte", ByteMethods },
      { "java/io/Writer", WriterMethods },
      { 0 }
      };

   static Y class15[] =
      {
      { "java/lang/Class", ClassMethods },
      { "java/lang/Float", FloatMethods },
      { "sun/misc/Unsafe", UnsafeMethods },
      { "java/lang/Short", ShortMethods },
      { 0 }
      };

   static Y class16[] =
      {
      { "java/lang/Double", DoubleMethods },
      { "java/lang/Object", ObjectMethods },
      { "java/lang/String", StringMethods },
      { "java/lang/System", SystemMethods },
      { "java/lang/Thread", ThreadMethods },
      { "java/util/Vector", VectorMethods },
      { "java/util/Arrays", ArraysMethods },
      {0}
      };

   static Y class17[] =
      {
      { "com/ibm/oti/vm/VM", otivmMethods },
      { "java/lang/Integer", IntegerMethods },
      { "java/lang/Boolean", BooleanMethods },
      { "java/util/TreeMap", TreeMapMethods },
      { "java/util/HashMap", HashMapMethods },
      { 0 }
      };

   static Y class18[] =
      {
      { "com/ibm/gpu/Kernel", GPUMethods },
      { 0 }
      };

   static Y class19[] =
      {
      { "java/util/Hashtable", HashtableMethods },
      { "java/lang/Throwable", ThrowableMethods },
      { "java/lang/Character", CharacterMethods },
      { "java/util/ArrayList", ArrayListMethods },
      { "java/util/zip/CRC32", CRC32Methods     },
      { 0 }
      };

   static Y class20[] =
      {
      { "java/lang/StrictMath", StrictMathMethods },
      { "java/math/BigDecimal", BigDecimalMethods },
      { "java/math/BigInteger", BigIntegerMethods },
      { 0 }
      };

   static Y class21[] =
      {
      { "java/lang/ClassLoader", ClassLoaderMethods },
      { "java/lang/StringUTF16", StringUTF16Methods },
      { 0 }
      };

   static Y class22[] =
      {
      { "java/lang/StringBuffer", StringBufferMethods },
      { "sun/reflect/Reflection", ReflectionMethods },
      { "java/text/NumberFormat", NumberFormatMethods },
      { "com/ibm/jit/JITHelpers", JITHelperMethods},
      { "java/lang/StringCoding", StringCodingMethods },
      { 0 }
      };

   static Y class23[] =
      {
#if defined(ENABLE_SPMD_SIMD)
      { "com/ibm/simt/SPMDKernel", SPMDKernelBaseMethods }, // 23
#endif
      { "x10/runtime/VMInterface", X10Methods },
      { "java/lang/J9VMInternals", VMInternalsMethods },
      { "java/lang/ref/Reference", ReferenceMethods },
      { "java/lang/StringBuilder", StringBuilderMethods },
      { "java/lang/reflect/Array", ArrayMethods},
      { "java/nio/HeapByteBuffer", HeapByteBufferMethods},
      { "sun/nio/ch/NativeThread", NativeThreadMethods},
      { "com/ibm/dataaccess/SIMD", SIMDMethods },
      { 0 }
      };

   static Y class24[] =
      {
      { "java/util/TreeMap$SubMap", SubMapMethods },
      { "sun/nio/cs/UTF_8$Decoder", EncodeMethods },
      { "sun/nio/cs/UTF_8$Encoder", EncodeMethods },
      { "sun/nio/cs/UTF16_Encoder", EncodeMethods },
      { "jdk/internal/misc/Unsafe", UnsafeMethods },
      { 0 }
      };

   static Y class25[] =
      {
      { 0 }
      };

   static Y class27[] =
      {
      { "sun/io/ByteToCharSingleByte", DoubleByteConverterMethods },
      { "com/ibm/oti/vm/ORBVMHelpers", ORBVMHelperMethods },
      { "java/util/GregorianCalendar", GregorianCalendarMethods },
      { "sun/nio/cs/US_ASCII$Encoder", EncodeMethods },
      { "sun/nio/cs/US_ASCII$Decoder", EncodeMethods },
      { "sun/nio/cs/ext/SBCS_Encoder", EncodeMethods },
      { "sun/nio/cs/ext/SBCS_Decoder", EncodeMethods },
      { "java/lang/invoke/FoldHandle", FoldHandleMethods },
      { "java/lang/ref/SoftReference", JavaLangRefSoftReferenceMethods },
      { 0 }
      };

   static Y class28[] =
      {
      { "sun/io/ByteToCharDBCS_EBCDIC", DoubleByteConverterMethods },
      { "java/lang/invoke/ILGenMacros", ILGenMacrosMethods },
      { "java/lang/invoke/CatchHandle", CatchHandleMethods },
      { "com/ibm/tenant/TenantContext", MTTenantContext },
      { "java/util/stream/IntPipeline", JavaUtilStreamIntPipelineMethods },
      { 0 }
      };

   static Y class29[] =
      {
      { "java/lang/invoke/MethodHandle", MethodHandleMethods },
      { "java/lang/invoke/AsTypeHandle", AsTypeHandleMethods },
      { "java/lang/invoke/InsertHandle", InsertHandleMethods },
      { "java/lang/invoke/SpreadHandle", SpreadHandleMethods },
      { "java/lang/invoke/DirectHandle", DirectHandleMethods },
      { "sun/nio/cs/ISO_8859_1$Encoder", EncodeMethods },
      { "sun/nio/cs/ISO_8859_1$Decoder", EncodeMethods },
      { "java/io/ByteArrayOutputStream", ByteArrayOutputStreamMethods },
      { 0 }
      };

   static Y class30[] =
      {
      { "com/ibm/Compiler/Internal/Quad", QuadMethods },
      { "java/lang/invoke/CollectHandle", CollectHandleMethods },
      { "java/lang/invoke/FinallyHandle", FinallyHandleMethods },
      { "java/lang/invoke/PermuteHandle", PermuteHandleMethods },
      { "java/lang/invoke/MethodHandles", MethodHandlesMethods },
      { "com/ibm/dataaccess/DecimalData", DataAccessDecimalDataMethods },
      { "sun/nio/cs/ext/IBM1388$Encoder", IBM1388EncoderMethods },
      { "java/util/HashMap$HashIterator", HashMapHashIteratorMethods },
      { 0 }
      };

   static Y class31[] =
      {
      { "com/ibm/jit/DecimalFormatHelper", DecimalFormatHelperMethods},
      { 0 }
      };
   static Y class32[] =
      {
      { "java/lang/invoke/MutableCallSite", MutableCallSiteMethods },
      { "com/ibm/dataaccess/PackedDecimal", DataAccessPackedDecimalMethods },
      { 0 }
      };

   static Y class33[] =
      {
      { "com/ibm/dataaccess/ByteArrayUtils", DataAccessByteArrayUtilMethods },
      { "java/util/stream/AbstractPipeline", JavaUtilStreamAbstractPipelineMethods },
      { "java/util/stream/IntPipeline$Head", JavaUtilStreamIntPipelineHeadMethods },
      { 0 }
      };

   static Y class34[] =
      {
      { "java/util/Hashtable$HashEnumerator", HashtableHashEnumeratorMethods },
      { "java/util/concurrent/atomic/Fences", JavaUtilConcurrentAtomicFencesMethods },
      { "com/ibm/Compiler/Internal/Prefetch", PrefetchMethods },
      { "java/lang/invoke/VarHandleInternal", VarHandleMethods },
      { 0 }
      };

   static Y class35[] =
      {
      { "java/lang/invoke/ExplicitCastHandle", ExplicitCastHandleMethods },
      { 0 }
      };

   static Y class36[] =
      {
      { "java/lang/invoke/GuardWithTestHandle", GuardWithTestHandleMethods },
      { "java/lang/invoke/ArgumentMoverHandle", ArgumentMoverHandleMethods },
      { "com/ibm/rmi/io/FastPathForCollocated", FastPathForCollocatedMethods },
      { "com/ibm/tenant/InternalTenantContext", MTTenantContext },
      { "java/lang/StringCoding$StringDecoder", StringCoding_StringDecoderMethods },
      { "java/lang/StringCoding$StringEncoder", StringCoding_StringEncoderMethods },
      { 0 }
      };

   //1421
   static Y class38[] =
      {
      { "java/util/concurrent/atomic/AtomicLong", JavaUtilConcurrentAtomicLongMethods },
      { "java/lang/invoke/FilterArgumentsHandle", FilterArgumentsHandleMethods },
      { "com/ibm/dataaccess/ByteArrayMarshaller", DataAccessByteArrayMarshallerMethods },
      { "java/util/concurrent/ConcurrentHashMap", JavaUtilConcurrentConcurrentHashMapMethods},
      { "com/ibm/crypto/provider/P224PrimeField", CryptoECC224Methods},
      { "com/ibm/crypto/provider/P256PrimeField", CryptoECC256Methods},
      { "com/ibm/crypto/provider/P384PrimeField", CryptoECC384Methods},
      { 0 }
      };

   static Y class39[] =
      {
      { "com/ibm/jit/crypto/JITFullHardwareCrypt", ZCryptoMethods },
      { 0 }
      };

   static Y class40[] =
      {
      { "java/util/TreeMap$UnboundedValueIterator", TreeMapUnboundedValueIteratorMethods },
      { "com/ibm/dataaccess/ByteArrayUnmarshaller", DataAccessByteArrayUnmarshallerMethods },
      { "com/ibm/jit/crypto/JITFullHardwareDigest", CryptoDigestMethods },
      { "com/ibm/jit/crypto/JITAESCryptInHardware", XPCryptoMethods },
      { 0 }
      };

   static Y class41[] =
      {
      { "org/apache/harmony/luni/platform/OSMemory", OSMemoryMethods },
      { "java/util/concurrent/atomic/AtomicBoolean", JavaUtilConcurrentAtomicBooleanMethods },
      { "java/util/concurrent/atomic/AtomicInteger", JavaUtilConcurrentAtomicIntegerMethods },
      { 0 }
      };

   static Y class42[] =
      {
      { "com/ibm/crypto/provider/AESCryptInHardware", CryptoAESMethods},
      { "java/util/concurrent/ConcurrentLinkedQueue", JavaUtilConcurrentConcurrentLinkedQueueMethods },
      { 0 }
      };

   static Y class43[] =
      {
      { "java/util/concurrent/atomic/AtomicReference", JavaUtilConcurrentAtomicReferenceMethods },
      { "java/util/concurrent/atomic/AtomicLongArray", JavaUtilConcurrentAtomicLongArrayMethods },
#ifdef J9VM_OPT_JAVA_CRYPTO_ACCELERATION
      { "com/ibm/crypto/provider/ByteArrayMarshaller", DataAccessByteArrayMarshallerMethods },
#endif
      { 0 }
      };

   static Y class45[] =
      {
#ifdef J9VM_OPT_JAVA_CRYPTO_ACCELERATION
      { "com/ibm/crypto/provider/ByteArrayUnmarshaller", DataAccessByteArrayUnmarshallerMethods },
#endif
      { 0 }
      };

   static Y class46[] =
      {
      { "java/util/concurrent/atomic/AtomicIntegerArray", JavaUtilConcurrentAtomicIntegerArrayMethods },
      { "java/util/concurrent/ConcurrentHashMap$Segment", JavaUtilConcurrentConcurrentHashMapSegmentMethods },
      { "java/util/concurrent/ConcurrentHashMap$TreeBin", JavaUtilConcurrentConcurrentHashMapTreeBinMethods },
      { "java/io/ObjectInputStream$BlockDataInputStream", ObjectInputStream_BlockDataInputStreamMethods },
      { 0 }
      };

   static Y class48[] =
      {
      { "java/util/concurrent/atomic/AtomicReferenceArray", JavaUtilConcurrentAtomicReferenceArrayMethods },
      { 0 }
      };

   static Y class50[] =
      {
      { "java/util/concurrent/atomic/AtomicStampedReference", JavaUtilConcurrentAtomicStampedReferenceMethods },
      { "java/util/concurrent/atomic/AtomicLongFieldUpdater", JavaUtilConcurrentAtomicLongFieldUpdaterMethods },
      { 0 }
      };

   static Y class51[] =
      {
      { "java/util/concurrent/atomic/AtomicMarkableReference", JavaUtilConcurrentAtomicMarkableReferenceMethods },
      { 0 }
      };

   static Y class53[] =
      {
      { "java/util/concurrent/atomic/AtomicIntegerFieldUpdater", JavaUtilConcurrentAtomicIntegerFieldUpdaterMethods },
      { 0 }
      };

   static Y class55[] =
      {
      { "java/util/concurrent/atomic/AtomicReferenceFieldUpdater", JavaUtilConcurrentAtomicReferenceFieldUpdaterMethods },
      { 0 }
      };

   static Y * recognizedClasses[] =
      {
      0, 0, 0, class13, class14, class15, class16, class17, class18, class19,
      class20, class21, class22, class23, class24, class25, 0, class27, class28, class29,
      class30, class31, class32, class33, class34, class35, class36, 0, class38, class39,
      class40, class41, class42, class43, 0, class45, class46, 0, class48, 0,
      class50, class51, 0, class53, 0, class55
      };

   const int32_t minRecognizedClassLength = 10;
   const int32_t maxRecognizedClassLength = sizeof(recognizedClasses)/sizeof(recognizedClasses[0]) + minRecognizedClassLength - 1;

   const int32_t minNonUserClassLength = 17;

   const int32_t maxNonUserClassLength = 17;
   static Y * nonUserClasses[] =
      {
      class17
      };

   if (isMethodInValidLibrary(fe, this))
      {
      char *className    = convertToMethod()->classNameChars();
      int   classNameLen = convertToMethod()->classNameLength();
      char *name         = convertToMethod()->nameChars();
      int   nameLen      = convertToMethod()->nameLength();
      char *sig          = convertToMethod()->signatureChars();
      int   sigLen       = convertToMethod()->signatureLength();

      if (classNameLen >= minRecognizedClassLength && classNameLen <= maxRecognizedClassLength)
         {
         Y * cl = recognizedClasses[classNameLen - minRecognizedClassLength];
         if (cl)
            for (; cl->_class; ++cl)
               if (!strncmp(cl->_class, className, classNameLen))
                  {
                  for (X * m =  cl->_methods; m->_enum != TR::unknownMethod; ++m)
                     {
                     if (m->_nameLen == nameLen && (m->_sigLen == sigLen || m->_sigLen == (int16_t)-1) &&
                         !strncmp(m->_name, name, nameLen) &&
                         (m->_sigLen == (int16_t)-1 || !strncmp(m->_sig,  sig,  sigLen)))
                        {

                        if ((classNameLen == 30) && !strncmp(className, "com/ibm/Compiler/Internal/Quad", 30))
                           setQuadClassSeen();

                        setRecognizedMethodInfo(m->_enum);
                        break;
                        }
                     }
                  }
         }

      if (TR_Method::getMandatoryRecognizedMethod() == TR::unknownMethod)
         {
         // Cases where multiple method names all map to the same RecognizedMethod
         //
         if ((classNameLen == 17) && !strncmp(className, "java/util/TreeMap", 17))
            setRecognizedMethodInfo(TR::java_util_TreeMap_all);
         else if ((classNameLen == 17) && !strncmp(className, "java/util/EnumMap", 17))
            {
            if (!strncmp(name, "put", 3))
               setRecognizedMethodInfo(TR::java_util_EnumMap_put);
            else if (!strncmp(name, "typeCheck", 9))
               setRecognizedMethodInfo(TR::java_util_EnumMap_typeCheck);
            else if (!strncmp(name, "<init>", 6))
               setRecognizedMethodInfo(TR::java_util_EnumMap__init_);
            else
               setRecognizedMethodInfo(TR::java_util_EnumMap__nec_);
            }
         else if ((classNameLen == 17) && !strncmp(className, "java/util/HashMap", 17))
             setRecognizedMethodInfo(TR::java_util_HashMap_all);
         else if ((classNameLen == 19) && !strncmp(className, "java/util/ArrayList", 19))
            setRecognizedMethodInfo(TR::java_util_ArrayList_all);
         else if ((classNameLen == 19) && !strncmp(className, "java/util/Hashtable", 19))
            setRecognizedMethodInfo(TR::java_util_Hashtable_all);
         else if ((classNameLen == 38) && !strncmp(className, "java/util/concurrent/ConcurrentHashMap", 38))
            setRecognizedMethodInfo(TR::java_util_concurrent_ConcurrentHashMap_all);
         else if ((classNameLen == 16) && !strncmp(className, "java/util/Vector", 16))
            setRecognizedMethodInfo(TR::java_util_Vector_all);
         else if ((classNameLen == 28) && !strncmp(className, "java/lang/invoke/ILGenMacros", 28))
            {
            if (!strncmp(name, "invokeExact_", 12))
               setRecognizedMethodInfo(TR::java_lang_invoke_ILGenMacros_invokeExact);
            else if (!strncmp(name, "first_", 6))
               setRecognizedMethodInfo(TR::java_lang_invoke_ILGenMacros_first);
            else if (!strncmp(name, "last_", 5))
               setRecognizedMethodInfo(TR::java_lang_invoke_ILGenMacros_last);
            }
         else if ((classNameLen == 29) && !strncmp(className, "java/lang/invoke/MethodHandle", 29))
            {
            if (!strncmp(name, "asType", 6))
               {
               int32_t instanceAsTypeSigLen = strlen("(Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/MethodHandle;");
               if (sigLen == instanceAsTypeSigLen &&
                   !strncmp(sig, "(Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/MethodHandle;", instanceAsTypeSigLen))
                  {
                  setRecognizedMethodInfo(TR::java_lang_invoke_MethodHandle_asType_instance);
                  }
               else
                  {
                  setRecognizedMethodInfo(TR::java_lang_invoke_MethodHandle_asType);
                  }
               }

            if (!strncmp(name, "invokeExactTargetAddress",24))
               setRecognizedMethodInfo(TR::java_lang_invoke_MethodHandle_invokeExactTargetAddress);
            }
         else if ((classNameLen == 29) && !strncmp(className, "java/lang/invoke/DirectHandle", 29))
            {
            if (!strncmp(name, "directCall_", 11))
               setRecognizedMethodInfo(TR::java_lang_invoke_DirectHandle_directCall);
            if (!strncmp(name, "invokeExact_thunkArchetype_", 27))
               setRecognizedMethodInfo(TR::java_lang_invoke_DirectHandle_invokeExact);
            }
         else if ((classNameLen == 32) && !strncmp(className, "java/lang/invoke/InterfaceHandle", 32))
            {
            if (!strncmp(name, "interfaceCall_", 14))
               setRecognizedMethodInfo(TR::java_lang_invoke_InterfaceHandle_interfaceCall);
            if (!strncmp(name, "invokeExact_thunkArchetype_", 27))
               setRecognizedMethodInfo(TR::java_lang_invoke_InterfaceHandle_invokeExact);
            }
         else if ((classNameLen == 30) && !strncmp(className, "java/lang/invoke/VirtualHandle", 30))
            {
            if (!strncmp(name, "virtualCall_", 12))
               setRecognizedMethodInfo(TR::java_lang_invoke_VirtualHandle_virtualCall);
            if (!strncmp(name, "invokeExact_thunkArchetype_", 27))
               setRecognizedMethodInfo(TR::java_lang_invoke_VirtualHandle_invokeExact);
            }
         else if ((classNameLen == 30) && !strncmp(className, "java/lang/invoke/ComputedCalls", 30))
            {
            if (!strncmp(name, "dispatchDirect_", 15))
               setRecognizedMethodInfo(TR::java_lang_invoke_ComputedCalls_dispatchDirect);
            else if (!strncmp(name, "dispatchVirtual_", 16))
               setRecognizedMethodInfo(TR::java_lang_invoke_ComputedCalls_dispatchVirtual);
            else if (!strncmp(name, "dispatchJ9Method_", 17))
               setRecognizedMethodInfo(TR::java_lang_invoke_ComputedCalls_dispatchJ9Method);
            }
         else if ((classNameLen >= 59 + 3 && classNameLen <= 59 + 7) && !strncmp(className, "java/lang/invoke/ArrayVarHandle$ArrayVarHandleOperations$Op", 59))
            {
            setRecognizedMethodInfo(TR::java_lang_invoke_ArrayVarHandle_ArrayVarHandleOperations_OpMethod);
            }
         else if ((classNameLen >= 71 + 3 && classNameLen <= 71 + 7) && !strncmp(className, "java/lang/invoke/StaticFieldVarHandle$StaticFieldVarHandleOperations$Op", 71))
            {
            setRecognizedMethodInfo(TR::java_lang_invoke_StaticFieldVarHandle_StaticFieldVarHandleOperations_OpMethod);
            }
         else if ((classNameLen >= 75 + 3 && classNameLen <= 75 + 7) && !strncmp(className, "java/lang/invoke/InstanceFieldVarHandle$InstanceFieldVarHandleOperations$Op", 75))
            {
            setRecognizedMethodInfo(TR::java_lang_invoke_InstanceFieldVarHandle_InstanceFieldVarHandleOperations_OpMethod);
            }
         else if ((classNameLen >= 75 + 3 && classNameLen <= 75 + 7) && !strncmp(className, "java/lang/invoke/ByteArrayViewVarHandle$ByteArrayViewVarHandleOperations$Op", 75))
            {
            setRecognizedMethodInfo(TR::java_lang_invoke_ByteArrayViewVarHandle_ByteArrayViewVarHandleOperations_OpMethod);
            }
         }
      }
   #if defined(TR_HOST_X86)
      if ( convertToMethod()->getRecognizedMethod() == TR::java_lang_System_nanoTime )
         _jniTargetAddress = NULL;
   #endif
   }

void
TR_ResolvedJ9Method::setRecognizedMethodInfo(TR::RecognizedMethod rm)
   {
   setMandatoryRecognizedMethod(rm);

   bool failBecauseOfHCR = false;

   if (!fej9()->isAOT_DEPRECATED_DO_NOT_USE() &&
        TR::Options::getCmdLineOptions()->getOption(TR_EnableHCR) &&
       !isNative())
      {
      TR_OpaqueClassBlock *clazz = fej9()->getClassOfMethod(getPersistentIdentifier());
      J9JITConfig * jitConfig = fej9()->getJ9JITConfig();
      TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);
      TR_PersistentClassInfo *clazzInfo = compInfo->getPersistentInfo()->getPersistentCHTable()->findClassInfo(clazz);

      if (!clazzInfo)
         {
         failBecauseOfHCR = true;
         }
      else if (clazzInfo->classHasBeenRedefined())
         {
         failBecauseOfHCR = true;
         }
      }

   if (TR::Options::getCmdLineOptions()->getOption(TR_FullSpeedDebug) &&
       ((rm == TR::com_ibm_jit_JITHelpers_hashCodeImpl) ||
        (rm == TR::com_ibm_jit_JITHelpers_getSuperclass) ||
        (rm == TR::com_ibm_jit_DecimalFormatHelper_formatAsDouble) ||
        (rm == TR::com_ibm_jit_DecimalFormatHelper_formatAsFloat)))
      return;

   if ( isMethodInValidLibrary(fej9(),this) &&
        !failBecauseOfHCR) // With HCR, non-native methods can change, so we shouldn't "recognize" them
      {
      /*
       * Currently recognized methods are enabled only on x86. If the compilation object is null we are
       * going to compile this particular method, so let's set disableRecMethods = true conservatively
       *
       */
      TR::Compilation* comp = fej9()->_compInfoPT ? fej9()->_compInfoPT->getCompilation() : NULL;

      bool disableRecMethods = !comp  ? true
         : (fej9()->getSupportsRecognizedMethods() && !comp->getOption(TR_DisableRecognizedMethods)) ? false : true;

      if (disableRecMethods && fej9()->isAOT_DEPRECATED_DO_NOT_USE()) // AOT_JIT_GAP
         {
         switch (rm) {
#if defined(TR_TARGET_X86)
            case TR::java_lang_System_currentTimeMillis: // This needs AOT relocation. For now, only implemented on x86
#endif
            //|| rm == TR::java_lang_J9VMInternals_identityHashCode
            case TR::java_lang_Thread_currentThread:
            case TR::sun_misc_Unsafe_compareAndSwapInt_jlObjectJII_Z:
            case TR::sun_misc_Unsafe_compareAndSwapLong_jlObjectJJJ_Z:
            case TR::sun_misc_Unsafe_compareAndSwapObject_jlObjectJjlObjectjlObject_Z:
            case TR::java_lang_Class_isAssignableFrom: // worked
            case TR::java_lang_ref_Reference_getImpl:
            case TR::java_lang_Object_getClass:
            case TR::sun_misc_Unsafe_copyMemory:
            case TR::sun_misc_Unsafe_setMemory:
            case TR::java_lang_Class_isInstance: // used in TR_J9VM::inlineNativeCall

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

            case TR::sun_misc_Unsafe_monitorEnter_jlObject_V:
            case TR::sun_misc_Unsafe_monitorExit_jlObject_V:

            case TR::sun_misc_Unsafe_putByteOrdered_jlObjectJB_V:
            case TR::sun_misc_Unsafe_putBooleanOrdered_jlObjectJZ_V:
            case TR::sun_misc_Unsafe_putCharOrdered_jlObjectJC_V:
            case TR::sun_misc_Unsafe_putShortOrdered_jlObjectJS_V:
            case TR::sun_misc_Unsafe_putIntOrdered_jlObjectJI_V:
            case TR::sun_misc_Unsafe_putLongOrdered_jlObjectJJ_V:
            case TR::sun_misc_Unsafe_putFloatOrdered_jlObjectJF_V:
            case TR::sun_misc_Unsafe_putDoubleOrdered_jlObjectJD_V:
            case TR::sun_misc_Unsafe_putObjectOrdered_jlObjectJjlObject_V: // used in inliner for TR_InlinerBase::isInlineableJNI and TR_InlinerBase::inlineUnsafeCall

            case TR::sun_misc_Unsafe_getBooleanVolatile_jlObjectJ_Z:
            case TR::sun_misc_Unsafe_getByteVolatile_jlObjectJ_B:
            case TR::sun_misc_Unsafe_getCharVolatile_jlObjectJ_C:
            case TR::sun_misc_Unsafe_getShortVolatile_jlObjectJ_S:
            case TR::sun_misc_Unsafe_getIntVolatile_jlObjectJ_I:
            case TR::sun_misc_Unsafe_getLongVolatile_jlObjectJ_J:
            case TR::sun_misc_Unsafe_getFloatVolatile_jlObjectJ_F:
            case TR::sun_misc_Unsafe_getDoubleVolatile_jlObjectJ_D:
            case TR::sun_misc_Unsafe_getObjectVolatile_jlObjectJ_jlObject:

            case TR::sun_misc_Unsafe_putByte_JB_V:
            case TR::org_apache_harmony_luni_platform_OSMemory_putByte_JB_V:
            case TR::sun_misc_Unsafe_putChar_JC_V:
            case TR::sun_misc_Unsafe_putShort_JS_V:
            case TR::org_apache_harmony_luni_platform_OSMemory_putShort_JS_V:
            case TR::sun_misc_Unsafe_putInt_JI_V:
            case TR::org_apache_harmony_luni_platform_OSMemory_putInt_JI_V:
            case TR::sun_misc_Unsafe_putLong_JJ_V: ///////////////////////////
            case TR::org_apache_harmony_luni_platform_OSMemory_putLong_JJ_V:
            case TR::sun_misc_Unsafe_putFloat_JF_V:
            case TR::org_apache_harmony_luni_platform_OSMemory_putFloat_JF_V:
            case TR::sun_misc_Unsafe_putDouble_JD_V:
            case TR::org_apache_harmony_luni_platform_OSMemory_putDouble_JD_V:
            case TR::sun_misc_Unsafe_putAddress_JJ_V:
            case TR::org_apache_harmony_luni_platform_OSMemory_putAddress_JJ_V:

            case TR::sun_misc_Unsafe_getByte_J_B:
            case TR::org_apache_harmony_luni_platform_OSMemory_getByte_J_B:
            case TR::sun_misc_Unsafe_getChar_J_C:
            case TR::sun_misc_Unsafe_getShort_J_S:
            case TR::org_apache_harmony_luni_platform_OSMemory_getShort_J_S:
            case TR::sun_misc_Unsafe_getInt_J_I:
            case TR::org_apache_harmony_luni_platform_OSMemory_getInt_J_I:
            case TR::sun_misc_Unsafe_getLong_J_J:
            case TR::org_apache_harmony_luni_platform_OSMemory_getLong_J_J:
            case TR::sun_misc_Unsafe_getFloat_J_F:
            case TR::org_apache_harmony_luni_platform_OSMemory_getFloat_J_F:
            case TR::sun_misc_Unsafe_getDouble_J_D:
            case TR::org_apache_harmony_luni_platform_OSMemory_getDouble_J_D:
            case TR::sun_misc_Unsafe_getAddress_J_J:
            case TR::org_apache_harmony_luni_platform_OSMemory_getAddress_J_J:

            // from bool TR::TreeEvaluator::VMinlineCallEvaluator(TR::Node *node, bool isIndirect, TR::CodeGenerator *cg)
            //case TR::sun_misc_Unsafe_copyMemory:

            case TR::sun_misc_Unsafe_loadFence:
            case TR::sun_misc_Unsafe_storeFence:
            case TR::sun_misc_Unsafe_fullFence:

            case TR::sun_misc_Unsafe_ensureClassInitialized:

            case TR::java_lang_Math_sqrt:
            case TR::java_lang_StrictMath_sqrt:
            case TR::java_lang_Math_max_I:
            case TR::java_lang_Math_min_I:
            case TR::java_lang_Math_max_L:
            case TR::java_lang_Math_min_L:
            case TR::java_lang_Math_abs_I:
            case TR::java_lang_Math_abs_L:
            case TR::java_lang_Math_abs_F:
            case TR::java_lang_Math_abs_D:
            case TR::java_lang_Long_reverseBytes:
            case TR::java_lang_Integer_reverseBytes:
            case TR::java_lang_Short_reverseBytes:

            case TR::java_util_concurrent_atomic_Fences_reachabilityFence:
            case TR::java_util_concurrent_atomic_Fences_orderAccesses:
            case TR::java_util_concurrent_atomic_Fences_orderReads:
            case TR::java_util_concurrent_atomic_Fences_orderWrites:

            case TR::java_util_concurrent_atomic_AtomicBoolean_getAndSet:
            case TR::java_util_concurrent_atomic_AtomicInteger_addAndGet:
         //case TR::java_util_concurrent_atomic_AtomicInteger_decrementAndGet: // this crashes the JVM

         /*
         case TR::java_util_concurrent_atomic_AtomicInteger_getAndAdd:
         case TR::java_util_concurrent_atomic_AtomicInteger_getAndDecrement:
         case TR::java_util_concurrent_atomic_AtomicInteger_getAndIncrement:
         case TR::java_util_concurrent_atomic_AtomicInteger_getAndSet:
         case TR::java_util_concurrent_atomic_AtomicInteger_incrementAndGet:
         */


            case TR::java_util_concurrent_atomic_AtomicIntegerArray_addAndGet:
            case TR::java_util_concurrent_atomic_AtomicIntegerArray_decrementAndGet:
            case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndAdd:
            case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndDecrement:
            case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndIncrement:
            case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndSet:
            case TR::java_util_concurrent_atomic_AtomicIntegerArray_incrementAndGet:
            case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_addAndGet:
            case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_decrementAndGet:
            case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_getAndAdd:
            case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_getAndDecrement:
            case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_getAndIncrement:
            case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_getAndSet:
            case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_incrementAndGet:
            case TR::java_util_concurrent_atomic_AtomicLongArray_addAndGet:
            case TR::java_util_concurrent_atomic_AtomicLongArray_decrementAndGet:
            case TR::java_util_concurrent_atomic_AtomicLongArray_getAndAdd:
            case TR::java_util_concurrent_atomic_AtomicLongArray_getAndDecrement:
            case TR::java_util_concurrent_atomic_AtomicLongArray_getAndIncrement:
            case TR::java_util_concurrent_atomic_AtomicLongArray_getAndSet:
            case TR::java_util_concurrent_atomic_AtomicLongArray_incrementAndGet:
            case TR::java_util_concurrent_atomic_AtomicLongFieldUpdater_addAndGet:
            case TR::java_util_concurrent_atomic_AtomicLongFieldUpdater_decrementAndGet:
            case TR::java_util_concurrent_atomic_AtomicLongFieldUpdater_getAndAdd:
            case TR::java_util_concurrent_atomic_AtomicLongFieldUpdater_getAndDecrement:
            case TR::java_util_concurrent_atomic_AtomicLongFieldUpdater_getAndIncrement:
            case TR::java_util_concurrent_atomic_AtomicLongFieldUpdater_getAndSet:
            case TR::java_util_concurrent_atomic_AtomicLongFieldUpdater_incrementAndGet:
            case TR::java_util_concurrent_atomic_AtomicLong_addAndGet:
            case TR::java_util_concurrent_atomic_AtomicLong_decrementAndGet:
            case TR::java_util_concurrent_atomic_AtomicLong_getAndAdd:
            case TR::java_util_concurrent_atomic_AtomicLong_getAndDecrement:
            case TR::java_util_concurrent_atomic_AtomicLong_getAndIncrement:
            case TR::java_util_concurrent_atomic_AtomicLong_getAndSet:
            case TR::java_util_concurrent_atomic_AtomicLong_incrementAndGet:

            // some of the ones below may not work in the second run
            case TR::java_lang_Object_clone:

            case TR::java_lang_System_nanoTime:
            case TR::java_lang_Object_hashCodeImpl:
            case TR::java_lang_String_hashCodeImplCompressed:
            case TR::java_lang_String_hashCodeImplDecompressed:
            case TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordCAS:
            case TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordSet:
            case TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordCASSupported:
            case TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordSetSupported:
            case TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordCAS:
            case TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordSet:
            case TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordCASSupported:
            case TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordSetSupported:
            case TR::sun_nio_ch_NativeThread_current:
            case TR::com_ibm_crypto_provider_AEScryptInHardware_cbcDecrypt:
            case TR::com_ibm_crypto_provider_AEScryptInHardware_cbcEncrypt:
            //case TR::java_lang_Class_isAssignableFrom: done



     //SYM    6746  0.24    TR::java_lang_J9VMInternals_identityHashCode --> used in VPhandlers
     //SYM    5455  0.19    TR::java_lang_J9VMInternals_primitiveClone
     //SYM    4567  0.16    TR::java_lang_Class_isAssignableFrom  DONE
     //SYM    2832  0.10    TR::java_lang_ref_Reference_getImpl   DONE
     //SYM    2812  0.10    TR::java_lang_Class_getModifiersImpl  --> not found
     //SYM    2365  0.08    TR::java_lang_Object_getClass         DONE
     //SYM    2301  0.08    JVM_GetClassAccessFlags_Impl
     //SYM    1680  0.06    TR::java_lang_Class_isInstance
     //SYM    1398  0.05    TR::sun_misc_Unsafe_putObject
     //SYM    1391  0.05    TR::sun_misc_Unsafe_copyMemory__      DONE
     //SYM     936  0.03    TR::sun_misc_Unsafe_putOrderedObject --> not found
     //SYM     627  0.02    Java_sun_reflect_Reflection_getClassAccessFlags
     //SYM     596  0.02    TR::java_lang_Class_isPrimitive
     //SYM     564  0.02    TR::sun_misc_Unsafe_putLong(__complex __complex)
               setRecognizedMethod(rm);
            default:
            	break;
            } // ens case
         }
      else
         {
         setRecognizedMethod(rm);
         }
      }
   }

void
TR_ResolvedJ9Method::setQuadClassSeen()
   {
   TR::Compilation* comp = ( fej9()->_compInfoPT ) ? fej9()->_compInfoPT->getCompilation() : NULL;
   //TODO: 
   //This works but is too drastic because it disable the OSR
   //for the entire compilation. Further work needs to be done
   //to not attempt OSR only when the execution entered the code path
   //that's different from the interpreter.
   if (comp && comp->supportsQuadOptimization())
     comp->setSeenClassPreventingInducedOSR();
   }

J9RAMConstantPoolItem *
TR_ResolvedJ9Method::literals()
   {
   return (J9RAMConstantPoolItem *) J9_CP_FROM_METHOD(ramMethod());
   }

J9Class *
TR_ResolvedJ9Method::constantPoolHdr()
   {
   return J9_CLASS_FROM_CP(literals());
   }

J9ROMClass *
TR_ResolvedJ9Method::romClassPtr()
   {
   return constantPoolHdr()->romClass;
   }

TR_Method * TR_ResolvedJ9Method::convertToMethod()                { return this; }
uint32_t      TR_ResolvedJ9Method::numberOfParameters()           { return TR_J9Method::numberOfExplicitParameters() + (isStatic()? 0 : 1); }
uint32_t      TR_ResolvedJ9Method::numberOfExplicitParameters()   { return TR_J9Method::numberOfExplicitParameters(); }
TR::DataType     TR_ResolvedJ9Method::parmType(uint32_t n)           { return TR_J9Method::parmType(n); }
TR::ILOpCodes  TR_ResolvedJ9Method::directCallOpCode()             { return TR_J9Method::directCallOpCode(); }
TR::ILOpCodes  TR_ResolvedJ9Method::indirectCallOpCode()           { return TR_J9Method::indirectCallOpCode(); }
TR::DataType     TR_ResolvedJ9Method::returnType()                   { return TR_J9Method::returnType(); }
uint32_t      TR_ResolvedJ9Method::returnTypeWidth()              { return TR_J9Method::returnTypeWidth(); }
bool          TR_ResolvedJ9Method::returnTypeIsUnsigned()         { return TR_J9Method::returnTypeIsUnsigned(); }
TR::ILOpCodes  TR_ResolvedJ9Method::returnOpCode()                 { return TR_J9Method::returnOpCode(); }
const char *  TR_ResolvedJ9Method::signature(TR_Memory * m, TR_AllocationKind k) { return TR_J9Method::signature(m, k); }
uint16_t      TR_ResolvedJ9Method::classNameLength()              { return TR_J9Method::classNameLength(); }
uint16_t      TR_ResolvedJ9Method::nameLength()                   { return TR_J9Method::nameLength(); }
uint16_t      TR_ResolvedJ9Method::signatureLength()              { return TR_J9Method::signatureLength(); }
char *        TR_ResolvedJ9Method::classNameChars()               { return TR_J9Method::classNameChars(); }
char *        TR_ResolvedJ9Method::nameChars()                    { return TR_J9Method::nameChars(); }
char *        TR_ResolvedJ9Method::signatureChars()               { return TR_J9Method::signatureChars(); }

intptrj_t
TR_ResolvedJ9Method::getInvocationCount()
   {
   return TR::CompilationInfo::getInvocationCount(ramMethod());
   }

bool
TR_ResolvedJ9Method::setInvocationCount(intptrj_t oldCount, intptrj_t newCount)
   {
   return TR::CompilationInfo::setInvocationCount(ramMethod(), oldCount, newCount);
   }

bool
TR_ResolvedJ9Method::isSameMethod(TR_ResolvedMethod * m2)
   {
   if (isNative())
      return false; // A jitted JNI method doesn't call itself

   TR_ResolvedJ9Method *other = (TR_ResolvedJ9Method *)m2; // TODO: Use something safer in the presence of multiple inheritance

   bool sameRamMethod = ramMethod() == other->ramMethod();
   if (!sameRamMethod)
      return false;

   if (asJ9Method()->isArchetypeSpecimen())
      {
      if (!other->asJ9Method()->isArchetypeSpecimen())
         return false;

      uintptrj_t *thisHandleLocation  = getMethodHandleLocation();
      uintptrj_t *otherHandleLocation = other->getMethodHandleLocation();

      // If these are not MethodHandle thunk archetypes, then we're not sure
      // how to compare them.  Conservatively return false in that case.
      //
      if (!thisHandleLocation)
         return false;
      if (!otherHandleLocation)
         return false;

      bool sameMethodHandle;

         {
         TR::VMAccessCriticalSection isSameMethod(fej9());
         sameMethodHandle = (*thisHandleLocation == *otherHandleLocation);
         }

      if (sameMethodHandle)
         {
         // Same ramMethod, same handle.  This means we're talking about the
         // exact same thunk.
         //
         return true;
         }
      else
         {
         // Different method handle.  Assuming we're talking about a custom thunk,
         // then it will be different thunk.
         //
         return false;
         }
      }

   return true;
   }

uint32_t
TR_ResolvedJ9Method::classModifiers()
   {
   return romClassPtr()->modifiers;
   }

uint32_t
TR_ResolvedJ9Method::classExtraModifiers()
   {
   return romClassPtr()->extraModifiers;
   }

uint32_t
TR_ResolvedJ9Method::methodModifiers()
   {
   return romMethod()->modifiers;
   }

bool
TR_ResolvedJ9Method::isConstructor()
   {
   return nameLength()==6 && !strncmp(nameChars(), "<init>", 6);
   }

bool TR_ResolvedJ9Method::isStatic()            { return methodModifiers() & J9AccStatic ? true : false; }
bool TR_ResolvedJ9Method::isNative()            { return methodModifiers() & J9AccNative ? true : false; }
bool TR_ResolvedJ9Method::isAbstract()          { return methodModifiers() & J9AccAbstract ? true : false; }
bool TR_ResolvedJ9Method::hasBackwardBranches() { return J9ROMMETHOD_HAS_BACKWARDS_BRANCHES(romMethod()) ? true : false; }
bool TR_ResolvedJ9Method::isObjectConstructor() { return J9ROMMETHOD_IS_OBJECT_CONSTRUCTOR(romMethod()) ? true : false; }
bool TR_ResolvedJ9Method::isNonEmptyObjectConstructor() { return J9ROMMETHOD_IS_NON_EMPTY_OBJECT_CONSTRUCTOR(romMethod()) ? true : false; }
bool TR_ResolvedJ9Method::isSynchronized()      { return methodModifiers() & J9AccSynchronized ? true : false; }
bool TR_ResolvedJ9Method::isPrivate()           { return methodModifiers() & J9AccPrivate ? true : false; }
bool TR_ResolvedJ9Method::isProtected()         { return methodModifiers() & J9AccProtected ? true : false; }
bool TR_ResolvedJ9Method::isPublic()            { return methodModifiers() & J9AccPublic ? true : false; }
bool TR_ResolvedJ9Method::isStrictFP()          { return methodModifiers() & J9AccStrict ? true : false; }
bool TR_ResolvedJ9Method::isSubjectToPhaseChange(TR::Compilation *comp)
   {
   if (comp->getOptLevel() >= warm)
      {
      TR_OpaqueClassBlock * clazz = containingClass();
      if (clazz)
         {
         J9Method *methods = (J9Method *) fej9()->getMethods(clazz);
         int numMethods = fej9()->getNumMethods(clazz);
         for (int i = 0; i < numMethods; ++i)
            {
            J9Method *method = &(methods[i]);
            J9UTF8 *name = J9ROMMETHOD_GET_NAME(J9_CLASS_FROM_METHOD(method)->romClass, J9_ROM_METHOD_FROM_RAM_METHOD(method));

            if (J9UTF8_LENGTH(name) == 13)
               {
               char s[15];
               sprintf(s, "%.*s",
                    J9UTF8_LENGTH(name), J9UTF8_DATA(name));
               if (strncmp(s, "specInstance$", 13) == 0)
                  return true;
               }
            }
         }
      }
   return comp->getOptLevel() <= warm &&
          comp->getPersistentInfo()->getJitState() == STARTUP_STATE &&
          isPublic() &&
          (
          strncmp("java/util/AbstractCollection", comp->signature(), 28) == 0 ||
          strncmp("java/util/Hash", comp->signature(), 14) == 0 ||
          strncmp("java/lang/String", comp->signature(), 16) == 0 ||
          strncmp("sun/nio/", comp->signature(), 8) == 0
          );
   }

bool TR_ResolvedJ9Method::isFinal()  { return (methodModifiers() & J9AccFinal) || (classModifiers() & J9AccFinal) ? true : false; }

TR_OpaqueClassBlock *
TR_ResolvedJ9Method::containingClass()
   {
   return _fe->convertClassPtrToClassOffset(constantPoolHdr());
   }

U_16
TR_ResolvedJ9Method::numberOfParameterSlots()
   {
   return _paramSlots + (isStatic()? 0 : 1);
   }

U_16
TR_ResolvedJ9Method::archetypeArgPlaceholderSlot(TR_Memory *mem)
   {
   TR_ASSERT(isArchetypeSpecimen(), "should not be called for non-ArchetypeSpecimen methods");
   // Note that this creates and discards a TR_ResolvedMethod, so it leaks heap memory.
   // TODO: Need a better implementation.  Probably should just re-parse the archetype's signature.
   // Then we won't need the TR_Memory argument anymore either.
   //
   TR_ResolvedMethod *archetype = fej9()->createResolvedMethod(mem, getNonPersistentIdentifier());
   //TR_ASSERT(((TR_ResolvedMethod*)this)->numberOfParameterSlots() == archetype->numberOfParameterSlots(), "not equal %d %d", ((TR_ResolvedMethod*)this)->numberOfParameterSlots(), archetype->numberOfParameterSlots());
   return archetype->numberOfParameterSlots() - 1; // "-1" because the placeholder is a 1-slot type (int)
   }

U_16
TR_ResolvedJ9Method::numberOfTemps()
   {
   return J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod());
   }

U_16
TR_ResolvedJ9Method::numberOfPendingPushes()
   {
   if (_pendingPushSlots < 0)
      _pendingPushSlots = J9_MAX_STACK_FROM_ROM_METHOD(romMethod());
   return _pendingPushSlots;
   }

U_8 *
TR_ResolvedJ9Method::bytecodeStart()
   {
   return J9_BYTECODE_START_FROM_ROM_METHOD(romMethod());
   }

U_32
TR_ResolvedJ9Method::maxBytecodeIndex()
   {
   return (U_32) (J9_BYTECODE_END_FROM_ROM_METHOD(romMethod()) - bytecodeStart());
   }

void *
TR_ResolvedJ9Method::ramConstantPool()
   {
   return literals();
   }

void *
TR_ResolvedJ9Method::constantPool()
   {
   return literals();
   }

J9ClassLoader *
TR_ResolvedJ9Method::getClassLoader()
   {
   return cp()->ramClass->classLoader;
   }

J9ConstantPool *
TR_ResolvedJ9Method::cp()
   {
   return (J9ConstantPool *)literals();
   }

bool
TR_ResolvedJ9Method::isInterpreted()
   {
   if (_fe->tossingCode())
      return true;
   return !(TR::CompilationInfo::isCompiled(ramMethod()));
   }

TR_OpaqueMethodBlock *
TR_ResolvedJ9Method::getNonPersistentIdentifier()
   {
   return getPersistentIdentifier();
   }

TR_OpaqueMethodBlock *
TR_ResolvedJ9Method::getPersistentIdentifier()
   {
   return (TR_OpaqueMethodBlock *) ramMethod();
   }

void *
TR_ResolvedJ9Method::resolvedMethodAddress()
   {
   return (void *)getPersistentIdentifier();
   }

void *
TR_ResolvedJ9Method::startAddressForJittedMethod()
   {
   int8_t * address = (int8_t *)TR::CompilationInfo::getJ9MethodStartPC(ramMethod());

   if (!TR::Compiler->target.cpu.isI386() && !(_fe->_jitConfig->runtimeFlags & J9JIT_TESTMODE))
      {
      address += ((*(int32_t *)(address - 4)) >> 16) & 0xFFFF;
      }

   return address;
   }


bool TR_ResolvedJ9Method::isWarmCallGraphTooBig(uint32_t bcIndex, TR::Compilation *comp)
   {
   /*void * startPC = startAddressForJittedMethod();

   traceMsg(comp, " inside inlinesMethod %p, start addr %p\n", ramMethod(), startPC);

   return findIndexInInlineRange((void *) fej9()->getJITMetaData(startPC), bcIndex, comp);*/
   if (fej9()->getIProfiler() &&
       fej9()->getIProfiler()->isWarmCallGraphTooBig((TR_OpaqueMethodBlock *)ramMethod(), bcIndex, comp))
      return true;
   /*
   void * startPC = startAddressForJittedMethod();
   J9TR_MethodMetaData *methodMetaData = (J9TR_MethodMetaData *)fej9()->getJITMetaData(startPC);
   uint32_t methodSize = (methodMetaData->endPC - methodMetaData->startPC);
   static const char * p = feGetEnv("TR_RejectSizeThreshold");
   static uint32_t defaultSizeThreshold = p ? atoi(p) : 355;

   if (methodSize > defaultSizeThreshold)
      {
      TR_PersistentJittedBodyInfo * bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(startPC);
      if (bodyInfo->getHotness() < warm)
         return true;
      }*/
   return false;
   }

void TR_ResolvedJ9Method::setWarmCallGraphTooBig(uint32_t bcIndex, TR::Compilation *comp)
   {
   if (fej9()->getIProfiler())
      fej9()->getIProfiler()->setWarmCallGraphTooBig((TR_OpaqueMethodBlock *)ramMethod(), bcIndex, comp, true);
   }

void *
TR_ResolvedJ9Method::startAddressForInterpreterOfJittedMethod()
   {
   return TR::CompilationInfo::getJ9MethodStartPC(ramMethod());
   }

void *
TR_ResolvedJ9Method::startAddressForJITInternalNativeMethod()
   {
   return startAddressForJittedMethod();
   }

int32_t
TR_ResolvedJ9Method::virtualCallSelector(U_32 cpIndex)
   {
   return -(int32_t)(vTableSlot(cpIndex) - J9JIT_INTERP_VTABLE_OFFSET);
   }

bool
TR_ResolvedJ9Method::virtualMethodIsOverridden()
   {
   return (UDATA)ramMethod()->constantPool & J9_STARTPC_METHOD_IS_OVERRIDDEN ? true : false;
   }

void
TR_ResolvedJ9Method::setVirtualMethodIsOverridden()
   {
   UDATA *cp = (UDATA*)&(ramMethod()->constantPool);
   *cp |= J9_STARTPC_METHOD_IS_OVERRIDDEN;
   }

void *
TR_ResolvedJ9Method::addressContainingIsOverriddenBit()
   {
   return &ramMethod()->constantPool;
   }

bool
TR_ResolvedJ9Method::isJNINative()
   {
   if (!supportsFastJNI(_fe))
      {
      return (((UDATA)ramMethod()->constantPool) & J9_STARTPC_JNI_NATIVE) != 0;
      }
   return _jniTargetAddress != NULL;
   }

bool
TR_ResolvedJ9Method::isJITInternalNative()
   {
   return isNative() && !isJNINative() && !isInterpreted();
   }

bool
TR_J9MethodBase::isUnsafeCAS(TR::Compilation * c)
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
//TR_ResolvedJ9Method::isUnsafeWithObjectArg(TR::Compilation * c)
TR_J9MethodBase::isUnsafeWithObjectArg(TR::Compilation * c)
   {
   //TR::RecognizedMethod rm = TR_ResolvedMethod::getRecognizedMethod();
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
TR_J9MethodBase::isUnsafeGetPutWithObjectArg(TR::RecognizedMethod rm)
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
TR_J9MethodBase::unsafeDataTypeForObject(TR::RecognizedMethod rm)
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
TR_J9MethodBase::unsafeDataTypeForArray(TR::RecognizedMethod rm)
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
TR_J9MethodBase::isVolatileUnsafe(TR::RecognizedMethod rm)
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

// Might need to add more unsafe put methods to this list
bool
TR_J9MethodBase::isUnsafePut(TR::RecognizedMethod rm)
   {
   switch (rm)
      {
      case TR::sun_misc_Unsafe_putBoolean_jlObjectJZ_V:
      case TR::sun_misc_Unsafe_putByte_jlObjectJB_V:
      case TR::sun_misc_Unsafe_putChar_jlObjectJC_V:
      case TR::sun_misc_Unsafe_putShort_jlObjectJS_V:
      case TR::sun_misc_Unsafe_putInt_jlObjectJI_V:
      case TR::sun_misc_Unsafe_putLong_jlObjectJJ_V:
      case TR::sun_misc_Unsafe_putFloat_jlObjectJF_V:
      case TR::sun_misc_Unsafe_putDouble_jlObjectJD_V:
      case TR::sun_misc_Unsafe_putObject_jlObjectJjlObject_V:
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
TR_J9MethodBase::isVarHandleOperationMethod(TR::RecognizedMethod rm)
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
TR_J9MethodBase::isVarHandleAccessMethod(TR::Compilation * comp)
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


bool
TR_ResolvedJ9Method::methodIsNotzAAPEligible()
   {
   return ((UDATA)ramMethod()->constantPool & J9_STARTPC_NATIVE_REQUIRES_SWITCHING) ? true : false;
   }

U_8 *
TR_ResolvedJ9Method::allocateException(uint32_t numBytes, TR::Compilation *comp)
   {
   J9JITExceptionTable *eTbl;
   uint32_t size = 0;
   bool shouldRetryAllocation;
   eTbl = (J9JITExceptionTable *)_fe->allocateDataCacheRecord(numBytes, comp, false, &shouldRetryAllocation,
                                                              J9_JIT_DCE_EXCEPTION_INFO, &size);
   if (!eTbl)
      {
      if (shouldRetryAllocation)
         {
         // force a retry
         comp->failCompilation<J9::RecoverableDataCacheError>("Failed to allocate exception table");
         }
      comp->failCompilation<J9::DataCacheError>("Failed to allocate exception table");
      }
   memset((uint8_t *)eTbl, 0, size);

   #if defined(J9VM_RAS_EYECATCHERS)
   eTbl->className       = J9ROMCLASS_CLASSNAME(romClassPtr());
   eTbl->methodName      = J9ROMMETHOD_GET_NAME(romClassPtr(), romMethod());// J9ROMMETHOD_NAME(romMethod());
   eTbl->methodSignature = J9ROMMETHOD_GET_SIGNATURE(romClassPtr(), romMethod());   //J9ROMMETHOD_SIGNATURE(romMethod());
   #endif

   J9ConstantPool *cpool;
   if (isNewInstanceImplThunk())
      {
      TR_ASSERT(_j9classForNewInstance, "Must have the class for the newInstance");
      //J9Class *clazz = (J9Class*) ((intptrj_t)_ramMethod->extra & ~J9_STARTPC_NOT_TRANSLATED);
      cpool = J9_CP_FROM_CLASS(_j9classForNewInstance);
      }
   else
      cpool = cp();

   // fill in the reserved slots in the newly allocated table
   eTbl->constantPool = cpool;
   eTbl->ramMethod = _ramMethod;

   return (U_8 *) eTbl;
   }

extern "C" J9Method * getNewInstancePrototype(J9VMThread * context);

bool
TR_ResolvedJ9Method::isNewInstanceImplThunk()
   {
   return (_j9classForNewInstance != NULL);
   //return getNewInstancePrototype(_fe->vmThread()) == ramMethod();
   }

char *
TR_ResolvedJ9Method::fieldOrStaticNameChars(I_32 cpIndex, int32_t & len)
   {
   return cpIndex >= 0 ? utf8Data(J9ROMNAMEANDSIGNATURE_NAME(J9ROMFIELDREF_NAMEANDSIGNATURE(&romCPBase()[cpIndex])), len) : 0;
   }

char *
TR_ResolvedJ9Method::fieldNameChars(I_32 cpIndex, int32_t & len)
   {
   return fieldOrStaticNameChars(cpIndex, len);
   }

char *
TR_ResolvedJ9Method::staticNameChars(I_32 cpIndex, int32_t & len)
   {
   return fieldOrStaticNameChars(cpIndex, len);
   }

char *
TR_ResolvedJ9Method::fieldOrStaticSignatureChars(I_32 cpIndex, int32_t & len)
   {
   return cpIndex >= 0 ? utf8Data(J9ROMNAMEANDSIGNATURE_SIGNATURE(J9ROMFIELDREF_NAMEANDSIGNATURE(&romCPBase()[cpIndex])), len) : 0;
   }

char *
TR_ResolvedJ9Method::fieldSignatureChars(I_32 cpIndex, int32_t & len)
   {
   return cpIndex > 0 ? fieldOrStaticSignatureChars(cpIndex, len) : 0;
   }

char *
TR_ResolvedJ9Method::staticSignatureChars(I_32 cpIndex, int32_t & len)
   {
   return cpIndex >= 0 ? fieldOrStaticSignatureChars(cpIndex, len) : 0;
   }

TR_OpaqueClassBlock *
TR_ResolvedJ9Method::classOfStatic(I_32 cpIndex, bool returnClassForAOT)
   {
   TR::VMAccessCriticalSection classOfStatic(fej9());
   TR_OpaqueClassBlock *result;
   result = _fe->convertClassPtrToClassOffset(cpIndex >= 0 ? jitGetClassOfFieldFromCP(_fe->vmThread(), cp(), cpIndex) : 0);
   return result;
   }

TR_OpaqueClassBlock *
TR_ResolvedJ9Method::classOfMethod()
   {
   if (isNewInstanceImplThunk())
      {
      TR_ASSERT(_j9classForNewInstance, "Must have the class for the newInstance");
      //J9Class * clazz = (J9Class *)((intptrj_t)ramMethod()->extra & ~J9_STARTPC_NOT_TRANSLATED);
      return _fe->convertClassPtrToClassOffset(_j9classForNewInstance);//(TR_OpaqueClassBlock *&)(rc);
      }

   return _fe->convertClassPtrToClassOffset(J9_CLASS_FROM_METHOD(ramMethod()));
   }

uint32_t
TR_ResolvedJ9Method::classCPIndexOfMethod(uint32_t methodCPIndex)
   {
   uint32_t realCPIndex = jitGetRealCPIndex(_fe->vmThread(), romClassPtr(), methodCPIndex);
   uint32_t classIndex = ((J9ROMMethodRef *) cp()->romConstantPool)[realCPIndex].classRefCPIndex;
   return classIndex;
   }

void * &
TR_ResolvedJ9Method::addressOfClassOfMethod()
   {
   if (isNewInstanceImplThunk())
      {
      TR_ASSERT(false, "This should not be called\n");
      return (void*&)_j9classForNewInstance;
      }
   else
      return (void*&)J9_CLASS_FROM_METHOD(ramMethod());
   }

I_32
TR_ResolvedJ9Method::exceptionData(I_32 exceptionNumber, I_32 * startIndex, I_32 * endIndex, I_32 * catchType)
   {
   J9ExceptionHandler * exceptionHandler = J9EXCEPTIONINFO_HANDLERS(J9_EXCEPTION_DATA_FROM_ROM_METHOD(romMethod()));
   return TR_ResolvedJ9MethodBase::exceptionData(exceptionHandler, 0, exceptionNumber, startIndex, endIndex, catchType);
   }

U_32
TR_ResolvedJ9Method::numberOfExceptionHandlers()
   {
   return J9ROMMETHOD_HAS_EXCEPTION_INFO(romMethod()) ? J9_EXCEPTION_DATA_FROM_ROM_METHOD(romMethod())->catchCount : 0;
   }

TR::DataType
TR_ResolvedJ9Method::getLDCType(I_32 cpIndex)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
   UDATA cpType = J9_CP_TYPE(J9ROMCLASS_CPSHAPEDESCRIPTION(cp()->ramClass->romClass), cpIndex);
   return cpType2trType(cpType);
   }

bool
TR_ResolvedJ9Method::isClassConstant(int32_t cpIndex)
   {
   UDATA cpType = J9_CP_TYPE(J9ROMCLASS_CPSHAPEDESCRIPTION(cp()->ramClass->romClass), cpIndex);
   return cpType == J9CPTYPE_CLASS;
   }

bool
TR_ResolvedJ9Method::isStringConstant(int32_t cpIndex)
   {
   UDATA cpType = J9_CP_TYPE(J9ROMCLASS_CPSHAPEDESCRIPTION(cp()->ramClass->romClass), cpIndex);
   return (cpType == J9CPTYPE_STRING) || (cpType == J9CPTYPE_ANNOTATION_UTF8);
   }

bool
TR_ResolvedJ9Method::isMethodTypeConstant(int32_t cpIndex)
   {
   UDATA cpType = J9_CP_TYPE(J9ROMCLASS_CPSHAPEDESCRIPTION(cp()->ramClass->romClass), cpIndex);
   return cpType == J9CPTYPE_METHOD_TYPE;
   }

bool
TR_ResolvedJ9Method::isMethodHandleConstant(int32_t cpIndex)
   {
   UDATA cpType = J9_CP_TYPE(J9ROMCLASS_CPSHAPEDESCRIPTION(cp()->ramClass->romClass), cpIndex);
   return cpType == J9CPTYPE_METHODHANDLE;
   }

SYS_FLOAT *
TR_ResolvedJ9Method::doubleConstant(I_32 cpIndex, TR_Memory * m)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
   return (SYS_FLOAT*)&romLiterals()[cpIndex];
   }

uint64_t
TR_ResolvedJ9Method::longConstant(I_32 cpIndex)
   {
   return *((uint64_t *) & romLiterals()[cpIndex]);
   }

bool
TR_ResolvedJ9Method::isUnresolvedString(I_32 cpIndex, bool optimizeForAOT)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
   return (bool) (((J9RAMStringRef *) literals())[cpIndex].stringObject == 0);
   }

float *
TR_ResolvedJ9Method::floatConstant(I_32 cpIndex)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
   return ((float *) &romLiterals()[cpIndex]);
   }

U_32
TR_ResolvedJ9Method::intConstant(I_32 cpIndex)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
   return *((U_32 *) & romLiterals()[cpIndex]);
   }

void *
TR_ResolvedJ9Method::stringConstant(I_32 cpIndex)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
   return &((J9RAMStringRef *) literals())[cpIndex].stringObject;
   }

void *
TR_ResolvedJ9Method::methodTypeConstant(I_32 cpIndex)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
   J9RAMMethodTypeRef *ramMethodTypeRef = (J9RAMMethodTypeRef *)(literals() + cpIndex);
   return &ramMethodTypeRef->type;
   }

bool
TR_ResolvedJ9Method::isUnresolvedMethodType(I_32 cpIndex)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
   return *(intptrj_t*)methodTypeConstant(cpIndex) == 0;
   }

void *
TR_ResolvedJ9Method::methodHandleConstant(I_32 cpIndex)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
   J9RAMMethodHandleRef *ramMethodHandleRef = (J9RAMMethodHandleRef *)(literals() + cpIndex);
   return &ramMethodHandleRef->methodHandle;
   }

bool
TR_ResolvedJ9Method::isUnresolvedMethodHandle(I_32 cpIndex)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
   return *(intptrj_t*)methodHandleConstant(cpIndex) == 0;
   }

void *
TR_ResolvedJ9Method::callSiteTableEntryAddress(int32_t callSiteIndex)
   {
   J9Class *ramClass = constantPoolHdr();
   return ramClass->callSites + callSiteIndex;
   }

bool
TR_ResolvedJ9Method::isUnresolvedCallSiteTableEntry(int32_t callSiteIndex)
   {
   return *(j9object_t*)callSiteTableEntryAddress(callSiteIndex) == NULL;
   }

void *
TR_ResolvedJ9Method::varHandleMethodTypeTableEntryAddress(int32_t cpIndex)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");

   J9Class *ramClass = constantPoolHdr();
   J9ROMClass *romClass = ramClass->romClass;
   U_16 *varHandleMethodTypeLookupTable = NNSRP_GET(romClass->varHandleMethodTypeLookupTable, U_16*);
   U_16 high = romClass->varHandleMethodTypeCount - 1;
   U_16 methodTypeIndex = high / 2;
   U_16 low = 0;

   /* Lookup MethodType cache location using binary search */
   while (varHandleMethodTypeLookupTable[methodTypeIndex] != cpIndex && high >= low)
      {
      if (varHandleMethodTypeLookupTable[methodTypeIndex] > cpIndex)
         high = methodTypeIndex - 1;
      else
         low = methodTypeIndex + 1;
      methodTypeIndex = (high + low) / 2;
      }

   // Assert when the lookup table is not properly initialized or not ordered
   TR_ASSERT(varHandleMethodTypeLookupTable[methodTypeIndex] == cpIndex, "Could not find the VarHandle callsite MethodType");

   return ramClass->varHandleMethodTypes + methodTypeIndex;
   }

#if defined(J9VM_OPT_REMOVE_CONSTANT_POOL_SPLITTING)
void *
TR_ResolvedJ9Method::methodTypeTableEntryAddress(int32_t cpIndex)
   {
   UDATA methodTypeIndex = (((J9RAMMethodRef*) literals())[cpIndex]).methodIndexAndArgCount;
   methodTypeIndex >>= 8;
   J9Class *ramClass = constantPoolHdr();
   return ramClass->methodTypes + methodTypeIndex;
   }

bool
TR_ResolvedJ9Method::isUnresolvedMethodTypeTableEntry(int32_t cpIndex)
   {
   return *(j9object_t*)methodTypeTableEntryAddress(cpIndex) == NULL;
   }
#endif

TR_OpaqueClassBlock *
TR_ResolvedJ9Method::getClassFromConstantPool(TR::Compilation * comp, uint32_t cpIndex, bool)
   {
   TR::VMAccessCriticalSection getClassFromConstantPool(fej9());
   TR_OpaqueClassBlock *result = 0;
   INCREMENT_COUNTER(_fe, totalClassRefs);
   J9Class * resolvedClass;
   if (cpIndex != -1 &&
       !((_fe->_jitConfig->runtimeFlags & J9JIT_RUNTIME_RESOLVE) &&
         !comp->ilGenRequest().details().isMethodHandleThunk() && // cmvc 195373
         performTransformation(comp, "Setting as unresolved class from CP cpIndex=%d\n",cpIndex) )&&
       (resolvedClass = _fe->_vmFunctionTable->resolveClassRef(_fe->vmThread(), cp(), cpIndex, J9_RESOLVE_FLAG_JIT_COMPILE_TIME)))
      {
      result = _fe->convertClassPtrToClassOffset(resolvedClass);
      }
   else
      {
      INCREMENT_COUNTER(_fe, unresolvedClassRefs);
      }
   return result;
   }

/*
TR_OpaqueClassBlock *
TR_ResolvedJ9Method::getClassFromConstantPoolForCheckcast(TR::Compilation *comp, uint32_t cpIndex)
   {
   //TODO: increment some stat
   //((TR_JitPrivateConfig *)javaVM->jitConfig->privateConfig)->aotStats->numInlinedMethodOverridden++;

   J9Class *resolvedClass= (J9Class*)TR_ResolvedJ9Method::getClassFromConstantPool(comp, cpIndex);
   if (resolvedClass)
      {
      bool validated = validateClassFromConstantPoolForCheckcast(comp, resolvedClass, cpIndex);
      if (validated)
         {
         return (TR_OpaqueClassBlock*)resolvedClass;
         }
      }
   return 0;
   }
*/

bool
TR_ResolvedJ9Method::validateClassFromConstantPool(TR::Compilation *comp, J9Class *clazz, uint32_t cpIndex, TR_ExternalRelocationTargetKind reloKind)
   {
   return true;
   }

bool
TR_ResolvedJ9Method::validateArbitraryClass(TR::Compilation *comp, J9Class *clazz)
   {
   return true;
   }


char *
TR_ResolvedJ9Method::getClassNameFromConstantPool(uint32_t cpIndex, uint32_t &length)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
   return utf8Data(J9ROMCLASSREF_NAME((J9ROMClassRef *) &romLiterals()[cpIndex]), length);
   }

const char *
TR_ResolvedJ9Method::newInstancePrototypeSignature(TR_Memory * m, TR_AllocationKind allocKind)
   {
   int32_t  clen;
   TR_ASSERT(_j9classForNewInstance, "Must have the class for newInstance");
   J9Class * clazz = _j9classForNewInstance; //((J9Class*)((uintptrj_t)(ramMethod()->extra) & ~J9_STARTPC_NOT_TRANSLATED);
   char    * className = fej9()->getClassNameChars(_fe->convertClassPtrToClassOffset(clazz), clen);
   char    * s = (char *)m->allocateMemory(clen+nameLength()+signatureLength()+3, allocKind);
   sprintf(s, "%.*s.%.*s%.*s", clen, className, nameLength(), nameChars(), signatureLength(), signatureChars());
   return s;
   }

void *
TR_ResolvedJ9Method::startAddressForJNIMethod(TR::Compilation * comp)
   {
   // This is a FastJNI method, address is directly callable
   if (_jniProperties)
      return _jniTargetAddress;

   char *address = (char *)TR::CompilationInfo::getJ9MethodExtra(ramMethod());

   if (isInterpreted())
      return (void*)((intptrj_t)address & ~J9_STARTPC_NOT_TRANSLATED);

   return *(void * *)(TR::CompilationInfo::getJ9MethodExtra(ramMethod()) - (TR::Compiler->target.is64Bit() ? 12 : 8));
   }

U_32
TR_ResolvedJ9Method::vTableSlot(U_32 cpIndex)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
   //UDATA vTableSlot = ((J9RAMVirtualMethodRef *)literals())[cpIndex].methodIndexAndArgCount >> 8;
   TR_ASSERT(_vTableSlot, "vTableSlot called for unresolved method");
   return _vTableSlot;
   }

bool
TR_ResolvedJ9Method::isCompilable(TR_Memory * trMemory)
   {
   if (!TR_ResolvedJ9MethodBase::isCompilable(trMemory))
      return false;

   /* Don't compile methods with stripped bytecodes */
   if (J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod()) == 0)
      return false;

   // The following methods are magic and require reaches for arg2 (hence require a stack frame)
   // java/security/AccessController.doPrivileged(Ljava/security/PrivilegedAction;Ljava/security/AccessControlContext;)Ljava/lang/Object;
   // java/security/AccessController.doPrivileged(Ljava/security/PrivilegedExceptionAction;Ljava/security/AccessControlContext;)Ljava/lang/Object;

   /* AOT handles this by having the names in the string compare list */
   J9JavaVM * javaVM = _fe->_jitConfig->javaVM;
   J9JNIMethodID * magic = (J9JNIMethodID *) javaVM->doPrivilegedWithContextMethodID1;
   if (magic && ramMethod() == magic->method)
      return false;

   magic = (J9JNIMethodID *) javaVM->doPrivilegedWithContextMethodID2;
   if (magic && ramMethod() == magic->method)
      return false;

   magic = (J9JNIMethodID *) javaVM->doPrivilegedWithContextPermissionMethodID1;
   if (magic && ramMethod() == magic->method)
      return false;

   magic = (J9JNIMethodID *) javaVM->doPrivilegedWithContextPermissionMethodID2;
   if (magic && ramMethod() == magic->method)
      return false;

   return true;
   }

TR_OpaqueClassBlock *
TR_ResolvedJ9Method::getResolvedInterfaceMethod(I_32 cpIndex, UDATA *pITableIndex)
   {
   TR_OpaqueClassBlock *result;
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");

#if TURN_OFF_INLINING
   return 0;
#else

   INCREMENT_COUNTER(_fe, totalInterfaceMethodRefs);
   INCREMENT_COUNTER(_fe, unresolvedInterfaceMethodRefs);

      {
      TR::VMAccessCriticalSection getResolvedInterfaceMethod(fej9());
      result = (TR_OpaqueClassBlock *)jitGetInterfaceITableIndexFromCP(_fe->vmThread(), cp(), cpIndex, pITableIndex);
      }

   return result;
#endif
   }

U_32
TR_ResolvedJ9Method::getResolvedInterfaceMethodOffset(TR_OpaqueClassBlock * classObject, I_32 cpIndex)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
   // the classObject is the fixed type of the this pointer.  The result of this method is going to be
   // used to call the interface function directly.
   UDATA vTableIndex = 0;
#if TURN_OFF_INLINING
   return 0;
#else
   INCREMENT_COUNTER(_fe, unresolvedInterfaceMethodRefs);
   INCREMENT_COUNTER(_fe, totalInterfaceMethodRefs);

      {
      TR::VMAccessCriticalSection getResolvedInterfaceMethodOffset(fej9());
      vTableIndex = jitGetInterfaceVTableIndexFromCP(_fe->vmThread(), cp(), cpIndex, TR::Compiler->cls.convertClassOffsetToClassPtr(classObject));
      }

   return (J9JIT_INTERP_VTABLE_OFFSET - vTableIndex);
#endif
   }

TR_ResolvedMethod *
TR_ResolvedJ9Method::getResolvedInterfaceMethod(TR::Compilation * comp, TR_OpaqueClassBlock * classObject, I_32 cpIndex)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)_fe;
   J9Method * ramMethod =
      (J9Method *)fej9->getResolvedInterfaceMethod(getPersistentIdentifier(), classObject, cpIndex);

   // If the method ref is unresolved, the bytecodes of the ramMethod will be NULL.
   // IFF resolved, then we can look at the rest of the ref.
   //
   if (ramMethod && J9_BYTECODE_START_FROM_RAM_METHOD(ramMethod))
      {
      TR_AOTInliningStats *aotStats = NULL;
      if (comp->getOption(TR_EnableAOTStats))
         aotStats = & (((TR_JitPrivateConfig *)_fe->_jitConfig->privateConfig)->aotStats->interfaceMethods);
      TR_ResolvedMethod *m = createResolvedMethodFromJ9Method(comp, cpIndex, 0, ramMethod, NULL, aotStats);

      TR_OpaqueClassBlock *c = NULL;
      if (m)
         {
         c = m->classOfMethod();
         if (c && !fej9->isInterfaceClass(c))
            {
            TR::DebugCounter::incStaticDebugCounter(comp, "resources.resolvedMethods/interface");
            TR::DebugCounter::incStaticDebugCounter(comp, "resources.resolvedMethods/interface:#bytes", sizeof(TR_ResolvedJ9Method));
            return m;
         }
      }
      }

   TR::DebugCounter::incStaticDebugCounter(comp, "resources.resolvedMethods/interface/null");
   return 0;
   }

TR_ResolvedMethod *
TR_ResolvedJ9Method::getResolvedStaticMethod(TR::Compilation * comp, I_32 cpIndex, bool * unresolvedInCP)
   {
   TR_ResolvedMethod *resolvedMethod = NULL;

   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");

   INCREMENT_COUNTER(_fe, totalStaticMethodRefs);

#if !TURN_OFF_INLINING
   // See if the constant pool entry is already resolved or not
   //
   J9Method * ramMethod;
   if (unresolvedInCP)
      {
      ramMethod = jitGetJ9MethodUsingIndex(_fe->vmThread(), cp(), cpIndex);
      *unresolvedInCP = !ramMethod || !J9_BYTECODE_START_FROM_RAM_METHOD(ramMethod);
      }

      {
      TR::VMAccessCriticalSection getResolvedStaticMethod(fej9());
      ramMethod = jitResolveStaticMethodRef(_fe->vmThread(), cp(), cpIndex, J9_RESOLVE_FLAG_JIT_COMPILE_TIME);
      }

   bool skipForDebugging = doResolveAtRuntime(ramMethod, cpIndex, comp);
   if (isArchetypeSpecimen())
      {
      // ILGen macros currently must be resolved for correctness, or else they
      // are not recognized and expanded.  If we have unresolved calls, we can't
      // tell whether they're ilgen macros because the recognized-method system
      // only works on resovled methods.
      //
      if (ramMethod)
         skipForDebugging = false;
      else
         {
         comp->failCompilation<TR::ILGenFailure>("Can't compile an archetype specimen with unresolved calls");
         }
      }

   if (ramMethod && !skipForDebugging)
      {
      TR_AOTInliningStats *aotStats = NULL;
      if (comp->getOption(TR_EnableAOTStats))
         aotStats = & (((TR_JitPrivateConfig *)_fe->_jitConfig->privateConfig)->aotStats->staticMethods);
      resolvedMethod = createResolvedMethodFromJ9Method(comp, cpIndex, 0, ramMethod, unresolvedInCP, aotStats);
      if (unresolvedInCP)
         *unresolvedInCP = false;
      }

   if (resolvedMethod == NULL)
      {
      INCREMENT_COUNTER(_fe, unresolvedStaticMethodRefs);
      if (unresolvedInCP)
         handleUnresolvedStaticMethodInCP(cpIndex, unresolvedInCP);
      }

#endif

   return resolvedMethod;
   }

TR_ResolvedMethod *
TR_ResolvedJ9Method::getResolvedSpecialMethod(TR::Compilation * comp, I_32 cpIndex, bool * unresolvedInCP)
   {
   TR_ResolvedMethod *resolvedMethod = NULL;

   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");

#if !TURN_OFF_INLINING

   INCREMENT_COUNTER(_fe, totalSpecialMethodRefs);

   // See if the constant pool entry is already resolved or not
   //
   J9Method * ramMethod;
   if (unresolvedInCP)
      {
      ramMethod = jitGetJ9MethodUsingIndex(_fe->vmThread(), cp(), cpIndex);
      //*unresolvedInCP = !ramMethod || !J9_BYTECODE_START_FROM_RAM_METHOD(ramMethod);
      //we init the CP with a special magic method, which has no bytecodes (hence bytecode start is NULL)
      //i.e. the CP will always contain a method for special and static methods
      *unresolvedInCP = true;
      }

   if (!((_fe->_jitConfig->runtimeFlags & J9JIT_RUNTIME_RESOLVE) &&
         !comp->ilGenRequest().details().isMethodHandleThunk() && // cmvc 195373
         performTransformation(comp, "Setting as unresolved special call cpIndex=%d\n",cpIndex) ))
      {
      TR::VMAccessCriticalSection resolveSpecialMethodRef(fej9());

      ramMethod = jitResolveSpecialMethodRef(_fe->vmThread(), cp(), cpIndex, J9_RESOLVE_FLAG_JIT_COMPILE_TIME);
      if (ramMethod)
         {
         TR_AOTInliningStats *aotStats = NULL;
         if (comp->getOption(TR_EnableAOTStats))
            aotStats = & (((TR_JitPrivateConfig *)_fe->_jitConfig->privateConfig)->aotStats->specialMethods);
         resolvedMethod = createResolvedMethodFromJ9Method(comp, cpIndex, 0, ramMethod, unresolvedInCP, aotStats);
         if (unresolvedInCP)
            *unresolvedInCP = false;
         }
      }

   if (resolvedMethod == NULL)
      {
      INCREMENT_COUNTER(_fe, unresolvedSpecialMethodRefs);
      if (unresolvedInCP)
         handleUnresolvedVirtualMethodInCP(cpIndex, unresolvedInCP);
      }

#endif

   return resolvedMethod;
   }

TR_ResolvedMethod *
TR_ResolvedJ9Method::getResolvedVirtualMethod(TR::Compilation * comp, I_32 cpIndex, bool ignoreRtResolve, bool * unresolvedInCP)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
#if TURN_OFF_INLINING
   return 0;
#else
   INCREMENT_COUNTER(_fe, totalVirtualMethodRefs);

   TR_ResolvedMethod *resolvedMethod = NULL;

   // See if the constant pool entry is already resolved or not
   //
   if (unresolvedInCP)
       *unresolvedInCP = true;

   if (!((_fe->_jitConfig->runtimeFlags & J9JIT_RUNTIME_RESOLVE) &&
         !comp->ilGenRequest().details().isMethodHandleThunk() && // cmvc 195373
         performTransformation(comp, "Setting as unresolved virtual call cpIndex=%d\n",cpIndex) ) || ignoreRtResolve)
      {
      // only call the resolve if unresolved
      J9Method * ramMethod = 0;
      UDATA vTableIndex = (((J9RAMVirtualMethodRef*) literals())[cpIndex]).methodIndexAndArgCount;
      vTableIndex >>= 8;
      if ((J9JIT_INTERP_VTABLE_OFFSET + sizeof(uintptrj_t)) == vTableIndex)
         {
         TR::VMAccessCriticalSection resolveVirtualMethodRef(fej9());
         vTableIndex = _fe->_vmFunctionTable->resolveVirtualMethodRefInto(_fe->vmThread(), cp(), cpIndex, J9_RESOLVE_FLAG_JIT_COMPILE_TIME, &ramMethod, NULL);
         }
      else
         {
         // go fishing for the J9Method...
         uint32_t classIndex = ((J9ROMMethodRef *) cp()->romConstantPool)[cpIndex].classRefCPIndex;
         J9Class * classObject = (((J9RAMClassRef*) literals())[classIndex]).value;
         ramMethod = *(J9Method **)((char *)classObject + vTableIndex);
         if (unresolvedInCP)
            *unresolvedInCP = false;
         }

      if (vTableIndex)
         {
         TR_AOTInliningStats *aotStats = NULL;
         if (comp->getOption(TR_EnableAOTStats))
            aotStats = & (((TR_JitPrivateConfig *)_fe->_jitConfig->privateConfig)->aotStats->virtualMethods);
         resolvedMethod = createResolvedMethodFromJ9Method(comp, cpIndex, vTableIndex, ramMethod, unresolvedInCP, aotStats);
         }
      }

   if (resolvedMethod == NULL)
      {
      TR::DebugCounter::incStaticDebugCounter(comp, "resources.resolvedMethods/virtual/null");
      INCREMENT_COUNTER(_fe, unresolvedVirtualMethodRefs);
      if (unresolvedInCP)
         handleUnresolvedVirtualMethodInCP(cpIndex, unresolvedInCP);
      }
   else
      {
      if (((TR_ResolvedJ9Method*)resolvedMethod)->isVarHandleAccessMethod())
         {
         // VarHandle access methods are resolved to *_impl()V, restore their signatures to obtain function correctness in the Walker
         J9ROMMethodRef *romMethodRef = (J9ROMMethodRef *)(cp()->romConstantPool + cpIndex);
         J9ROMNameAndSignature *nameAndSig = J9ROMFIELDREF_NAMEANDSIGNATURE(romMethodRef);
         int32_t signatureLength;
         char   *signature = utf8Data(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig), signatureLength);
         ((TR_ResolvedJ9Method *)resolvedMethod)->setSignature(signature, signatureLength, comp->trMemory());
         }

      TR::DebugCounter::incStaticDebugCounter(comp, "resources.resolvedMethods/virtual");
      TR::DebugCounter::incStaticDebugCounter(comp, "resources.resolvedMethods/virtual:#bytes", sizeof(TR_ResolvedJ9Method));
      }

   return resolvedMethod;
#endif
   }

TR_ResolvedMethod *
TR_ResolvedJ9Method::createResolvedMethodFromJ9Method(TR::Compilation *comp, I_32 cpIndex, uint32_t vTableSlot, J9Method *j9Method, bool * unresolvedInCP, TR_AOTInliningStats *aotStats)
   {
   TR_ResolvedMethod *m = new (comp->trHeapMemory()) TR_ResolvedJ9Method((TR_OpaqueMethodBlock *) j9Method, _fe, comp->trMemory(), this, vTableSlot);
   return m;
   }

void
TR_ResolvedJ9Method::handleUnresolvedStaticMethodInCP(int32_t cpIndex, bool * unresolvedInCP)
   {
   }

void
TR_ResolvedJ9Method::handleUnresolvedSpecialMethodInCP(int32_t cpIndex, bool * unresolvedInCP)
   {
   }

void
TR_ResolvedJ9Method::handleUnresolvedVirtualMethodInCP(int32_t cpIndex, bool * unresolvedInCP)
   {
   }

TR_ResolvedMethod *
TR_ResolvedJ9Method::getResolvedDynamicMethod(TR::Compilation * comp, I_32 callSiteIndex, bool * unresolvedInCP)
   {
   TR_ASSERT(callSiteIndex != -1, "callSiteIndex shouldn't be -1");

#if TURN_OFF_INLINING
   return 0;
#else
   TR_ResolvedMethod *result = NULL;

   // JSR292: "Dynamic methods" differ from other kinds because the bytecode doesn't
   // contain a CP index.  Rather, it contains an index into the class's call site table,
   // which it a table used only for invokedynamic.

   // Note: invokeDynamic resolves operate by resolving the CallSite table entry.
   // Therefore we always return a TR_ResolvedMethod regardless of whether
   // the CP entry is resolved, even in rtResolve mode.

   // See if the constant pool entry is already resolved or not
      {
      TR::VMAccessCriticalSection getResolvedDynamicMethod(fej9());

      J9Class    *ramClass = constantPoolHdr();
      J9ROMClass *romClass = romClassPtr();

      if (unresolvedInCP)
         {
         // "unresolvedInCP" is a bit of a misnomer here, but we can describe
         // something equivalent by checking the callSites table.
         //
         *unresolvedInCP = (ramClass->callSites[callSiteIndex] == NULL);
         }

      J9SRP                 *namesAndSigs = (J9SRP*)J9ROMCLASS_CALLSITEDATA(romClass);
      J9ROMNameAndSignature *nameAndSig   = NNSRP_GET(namesAndSigs[callSiteIndex], J9ROMNameAndSignature*);
      J9UTF8                *signature    = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig);

      TR_OpaqueMethodBlock *dummyInvokeExact = _fe->getMethodFromName("java/lang/invoke/MethodHandle", "invokeExact", JSR292_invokeExactSig, getNonPersistentIdentifier());
      result = _fe->createResolvedMethodWithSignature(comp->trMemory(), dummyInvokeExact, NULL, utf8Data(signature), J9UTF8_LENGTH(signature), this);
      }

   return result;
#endif
   }

TR_ResolvedMethod *
TR_ResolvedJ9Method::getResolvedHandleMethod(TR::Compilation * comp, I_32 cpIndex, bool * unresolvedInCP)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");

#if TURN_OFF_INLINING
   return 0;
#else
   TR_ResolvedMethod *result = NULL;

   // JSR292: "Handle methods" differ from other kinds because the CP entry is
   // not a MethodRef, but rather a MethodTypeRef.

   // Note: invokeHandle resolves operate by resolving the load of the MethodType
   // from the constant pool, not by resolving the method symbol itself.
   // Therefore we always return a TR_ResolvedMethod regardless of whether
   // the CP entry is resolved, even in rtResolve mode.

   // See if the constant pool entry is already resolved or not
      {
      TR::VMAccessCriticalSection getResolvedHandleMethod(fej9());

#if defined(J9VM_OPT_REMOVE_CONSTANT_POOL_SPLITTING)
      if (unresolvedInCP)
         *unresolvedInCP = isUnresolvedMethodTypeTableEntry(cpIndex);
      TR_OpaqueMethodBlock *dummyInvokeExact = _fe->getMethodFromName("java/lang/invoke/MethodHandle", "invokeExact", JSR292_invokeExactSig, getNonPersistentIdentifier());
      J9ROMMethodRef *romMethodRef = (J9ROMMethodRef *)(cp()->romConstantPool + cpIndex);
      J9ROMNameAndSignature *nameAndSig = J9ROMMETHODREF_NAMEANDSIGNATURE(romMethodRef);
      int32_t signatureLength;
      char   *signature = utf8Data(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig), signatureLength);
#else
      if (unresolvedInCP)
         *unresolvedInCP = isUnresolvedMethodType(cpIndex);
      TR_OpaqueMethodBlock *dummyInvokeExact = _fe->getMethodFromName("java/lang/invoke/MethodHandle", "invokeExact", JSR292_invokeExactSig, getNonPersistentIdentifier());
      J9ROMMethodTypeRef *romMethodTypeRef = (J9ROMMethodTypeRef *)(cp()->romConstantPool + cpIndex);
      int32_t signatureLength;
      char   *signature = utf8Data(J9ROMMETHODTYPEREF_SIGNATURE(romMethodTypeRef), signatureLength);
#endif
      result = _fe->createResolvedMethodWithSignature(comp->trMemory(), dummyInvokeExact, NULL, signature, signatureLength, this);
      }

   return result;
#endif
   }

TR_ResolvedMethod *
TR_ResolvedJ9Method::getResolvedHandleMethodWithSignature(TR::Compilation * comp, I_32 cpIndex, char *signature)
   {
#if TURN_OFF_INLINING
   return 0;
#else
   // TODO:JSR292: Dummy would be unnecessary if we could create a TR_ResolvedJ9Method without a j9method
   TR_OpaqueMethodBlock *dummyInvokeExact = _fe->getMethodFromName("java/lang/invoke/MethodHandle", "invokeExact", JSR292_invokeExactSig, getNonPersistentIdentifier());
   TR_ResolvedMethod    *resolvedMethod   = _fe->createResolvedMethodWithSignature(comp->trMemory(), dummyInvokeExact, NULL, signature, strlen(signature), this);
   return resolvedMethod;
#endif
   }

TR_ResolvedMethod *
TR_ResolvedJ9Method::getResolvedVirtualMethod(TR::Compilation * comp, TR_OpaqueClassBlock * classObject, I_32 virtualCallOffset , bool ignoreRtResolve)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)_fe;
   void * ramMethod = fej9->getResolvedVirtualMethod(classObject, virtualCallOffset, ignoreRtResolve);
   TR_ResolvedMethod *m;
   if (_fe->isAOT_DEPRECATED_DO_NOT_USE())
      {
      m = ramMethod ? new (comp->trHeapMemory()) TR_ResolvedRelocatableJ9Method((TR_OpaqueMethodBlock *) ramMethod, _fe, comp->trMemory(), this) : 0;
      }
   else
      {
      m = ramMethod ? new (comp->trHeapMemory()) TR_ResolvedJ9Method((TR_OpaqueMethodBlock *) ramMethod, _fe, comp->trMemory(), this) : 0;
      }
   return m;
   }

bool
TR_ResolvedJ9Method::fieldsAreSame(I_32 cpIndex1, TR_ResolvedMethod * m2, I_32 cpIndex2, bool &sigSame)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)_fe;
   if (!fej9->sameClassLoaders(classOfMethod(), m2->classOfMethod()))
      return false;

   if (cpIndex1 == -1 || cpIndex2 == -1)
      return false;

   TR_ResolvedJ9Method * method2 = (TR_ResolvedJ9Method *)m2;

   if (cpIndex1 == cpIndex2 && this == method2) // should probably be ramMethod() == method2->ramMethod()
      return true;

   // fetch name and sigs of fieldRefs from constant pool, they refer to the same field if both the field names and
   // declaring class names match
   //
   J9ROMFieldRef * ref1 = &(((J9ROMFieldRef *) romLiterals())[cpIndex1]);
   J9ROMFieldRef * ref2 = &(((J9ROMFieldRef *) method2->romLiterals())[cpIndex2]);

   J9ROMNameAndSignature * nameAndSignature1 = J9ROMFIELDREF_NAMEANDSIGNATURE(ref1);
   J9ROMNameAndSignature * nameAndSignature2 = J9ROMFIELDREF_NAMEANDSIGNATURE(ref2);

   if (utf8Matches(J9ROMNAMEANDSIGNATURE_NAME(nameAndSignature1), J9ROMNAMEANDSIGNATURE_NAME(nameAndSignature2)) &&
       utf8Matches(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSignature1), J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSignature2)))
      {
      J9ROMClassRef * classref1 = &(((J9ROMClassRef *) romLiterals())[ref1->classRefCPIndex]);
      J9ROMClassRef * classref2 = &(((J9ROMClassRef *) method2->romLiterals())[ref2->classRefCPIndex]);
      J9UTF8 * declaringClassName1 = J9ROMCLASSREF_NAME(classref1);
      J9UTF8 * declaringClassName2 = J9ROMCLASSREF_NAME(classref2);

      if (utf8Matches(declaringClassName1, declaringClassName2))
         return true;
      }
   else
      sigSame = false;

   return false;
   }

bool
TR_ResolvedJ9Method::staticsAreSame(I_32 cpIndex1, TR_ResolvedMethod * m2, I_32 cpIndex2, bool &sigSame)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)_fe;
   if (!fej9->sameClassLoaders(classOfMethod(), m2->classOfMethod()))
      return false;

   if (cpIndex1 == -1 || cpIndex2 == -1)
      return false;

   TR_ResolvedJ9Method * method2 = (TR_ResolvedJ9Method *)m2;

   if (cpIndex1 == cpIndex2 && this == method2) // should probably be ramMethod() == method2->ramMethod()
      return true;

   // note addresses are not valid if not resolved (and of course if not loaded)
   //

   J9RAMStaticFieldRef * ramCPBase1 = (J9RAMStaticFieldRef*)literals();
   J9RAMStaticFieldRef * ramCPBase2 = (J9RAMStaticFieldRef*)method2->literals();

   if (J9RAMSTATICFIELDREF_IS_RESOLVED(ramCPBase1 + cpIndex1) && J9RAMSTATICFIELDREF_IS_RESOLVED(ramCPBase2 + cpIndex2))
        {
        UDATA dataAddress1 = (UDATA) J9RAMSTATICFIELDREF_VALUEADDRESS(ramCPBase1 + cpIndex1);
        UDATA dataAddress2 = (UDATA) J9RAMSTATICFIELDREF_VALUEADDRESS(ramCPBase2 + cpIndex2);

        // both fields are resolved -- equality requires identical addresses
        //
        return dataAddress1 == dataAddress2 ? true : false;
        }

   // one or more fields unresolved -- do slower class, name & signature comparison
   //
   J9ROMFieldRef * ref1 = &(((J9ROMFieldRef *) romLiterals())[cpIndex1]);
   J9ROMFieldRef * ref2 = &(((J9ROMFieldRef *) method2->romLiterals())[cpIndex2]);
   J9ROMClassRef * classref1 = &(((J9ROMClassRef *) romLiterals())[ref1->classRefCPIndex]);
   J9ROMClassRef * classref2 = &(((J9ROMClassRef *) method2->romLiterals())[ref2->classRefCPIndex]);

   J9ROMNameAndSignature *nameAndSignature1 = J9ROMFIELDREF_NAMEANDSIGNATURE(ref1);
   J9ROMNameAndSignature *nameAndSignature2 = J9ROMFIELDREF_NAMEANDSIGNATURE(ref2);

   if (utf8Matches(J9ROMNAMEANDSIGNATURE_NAME(nameAndSignature1), J9ROMNAMEANDSIGNATURE_NAME(nameAndSignature2)) &&
       utf8Matches(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSignature1), J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSignature2)))
      {
      if (utf8Matches(J9ROMCLASSREF_NAME(classref1), J9ROMCLASSREF_NAME(classref2)))
          return true;
      }
   else
      sigSame = false;

   return false;
   }

//returns true if this field is resolved, false otherwise
//991124 Note the wrong type is returned by this routine for array of int, type is int, not object (address)

bool
TR_ResolvedJ9Method::fieldAttributes(TR::Compilation * comp, I_32 cpIndex, U_32 * fieldOffset, TR::DataType * type, bool * volatileP, bool * isFinal, bool * isPrivate, bool isStore, bool * unresolvedInCP, bool needAOTValidation)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");

   INCREMENT_COUNTER(_fe, totalInstanceFieldRefs);

   // See if the constant pool entry is already resolved or not
   //
   bool isUnresolvedInCP = !J9RAMFIELDREF_IS_RESOLVED(((J9RAMFieldRef*)cp()) + cpIndex);
   if (unresolvedInCP)
      *unresolvedInCP = isUnresolvedInCP;

   bool isColdOrReducedWarm = (comp->getMethodHotness() < warm) || (comp->getMethodHotness() == warm && comp->getOption(TR_NoOptServer));

   //Instance fields in MethodHandle thunks should be resolved at compile time
   bool doRuntimeResolveForEarlyCompilation = isUnresolvedInCP && isColdOrReducedWarm && !comp->ilGenRequest().details().isMethodHandleThunk();

   IDATA offset;
   J9ROMFieldShape *fieldShape;
   if (!doRuntimeResolveForEarlyCompilation)
      {
      TR::VMAccessCriticalSection resolveForEarlyCompilation(fej9());
      offset = jitCTResolveInstanceFieldRefWithMethod(_fe->vmThread(), ramMethod(), cpIndex, isStore, &fieldShape);
      if (offset == J9JIT_RESOLVE_FAIL_COMPILE)
         {
         comp->failCompilation<TR::CompilationException>("offset == J9JIT_RESOLVE_FAIL_COMPILE");
         }
      }
   else
      {
      offset = -1;
      }

   bool resolved;
   UDATA ltype;

   static char *dontResolveJITField = feGetEnv("TR_JITDontResolveField");

   if (offset >= 0 &&
       !dontResolveJITField &&
       (!(_fe->_jitConfig->runtimeFlags & J9JIT_RUNTIME_RESOLVE) || // TODO: Probably more useful not to mark JSR292-related fields as unresolved
         comp->ilGenRequest().details().isMethodHandleThunk() || // cmvc 195373
        !performTransformation(comp, "Setting as unresolved field attributes cpIndex=%d\n",cpIndex)))
      {
      resolved = true;
      ltype = fieldShape->modifiers;
      //ltype = (((J9RAMFieldRef*) literals())[cpIndex]).flags;
      *volatileP = (ltype & J9AccVolatile) ? true : false;
      *fieldOffset = offset + sizeof(J9Object);  // add header size

      if (isFinal) *isFinal = (ltype & J9AccFinal) ? true : false;
      if (isPrivate) *isPrivate = (ltype & J9AccPrivate) ? true : false;
      }
   else
      {
      resolved = false;
      INCREMENT_COUNTER(_fe, unresolvedInstanceFieldRefs);

         {
         TR::VMAccessCriticalSection getFieldType(fej9());
         ltype = (jitGetFieldType) (cpIndex, ramMethod());     // get callers definition of the field type
         }

      *volatileP = true;                              // assume worst case, necessary?
      *fieldOffset = sizeof(J9Object);
      ltype = ltype << 16;
      if (isFinal) *isFinal = false;
      }

   *type = decodeType(ltype);
   return resolved;
   }

bool
TR_ResolvedJ9Method::staticAttributes(TR::Compilation * comp, I_32 cpIndex, void * * address, TR::DataType * type, bool * volatileP, bool * isFinal, bool * isPrivate, bool isStore, bool * unresolvedInCP, bool needAOTValidation)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");

   INCREMENT_COUNTER(_fe, totalStaticVariableRefs);

   // See if the constant pool entry is already resolved or not
   //
   bool isUnresolvedInCP = !J9RAMSTATICFIELDREF_IS_RESOLVED(((J9RAMStaticFieldRef*)cp()) + cpIndex);
   if (unresolvedInCP)
       *unresolvedInCP = isUnresolvedInCP;

   bool isColdOrReducedWarm = (comp->getMethodHotness() < warm) || (comp->getMethodHotness() == warm && comp->getOption(TR_NoOptServer));
   bool doRuntimeResolveForEarlyCompilation = isUnresolvedInCP && isColdOrReducedWarm;

   void *backingStorage;
   J9ROMFieldShape *fieldShape;
   if (!doRuntimeResolveForEarlyCompilation)
      {
      TR::VMAccessCriticalSection resolveForEarlyCompilation(fej9());
      backingStorage = jitCTResolveStaticFieldRefWithMethod(_fe->vmThread(), ramMethod(), cpIndex, isStore, &fieldShape);
      if (backingStorage == (void *) J9JIT_RESOLVE_FAIL_COMPILE)
         {
         comp->failCompilation<TR::CompilationException>("backingStorage == J9JIT_RESOLVE_FAIL_COMPILE");
         }
      }
   else
      {
      backingStorage = NULL;
      }

   bool resolved;
   UDATA ltype;

   static char *dontResolveJITStaticField = feGetEnv("TR_JITDontResolveStaticField");

   if (backingStorage &&
       !dontResolveJITStaticField &&
       (!(_fe->_jitConfig->runtimeFlags & J9JIT_RUNTIME_RESOLVE) ||
         comp->ilGenRequest().details().isMethodHandleThunk() || // cmvc 195373
        !performTransformation(comp, "Setting as unresolved static attributes cpIndex=%d\n",cpIndex)))
      {
      resolved = true;
      ltype = fieldShape->modifiers;
      *volatileP = (ltype & J9AccVolatile) ? true : false;
      if (isFinal) *isFinal = (ltype & J9AccFinal) ? true : false;
      if (isPrivate) *isPrivate = (ltype & J9AccPrivate) ? true : false;
      *address = backingStorage;
      }
   else
      {
      INCREMENT_COUNTER(_fe, unresolvedStaticVariableRefs);
      resolved = false;
      *volatileP = true;
      if (isFinal) *isFinal = false;

         {
         TR::VMAccessCriticalSection getFieldType(fej9());
         ltype = (jitGetFieldType) (cpIndex, ramMethod()); // fetch caller type info
         }

      ltype = ltype << 16;
      *address = 0;
      }

   *type = decodeType(ltype);
   return resolved;
   }


bool
TR_ResolvedJ9Method::fieldIsFromLocalClass(int32_t cpIndex)
   {
   J9ROMConstantPoolItem *cpItem = (J9ROMConstantPoolItem *)romLiterals();
   J9ROMFieldRef *fieldRef = (J9ROMFieldRef *)(cpItem + cpIndex);
   J9ROMClassRef *romClassRef = (J9ROMClassRef *)(cpItem  + fieldRef->classRefCPIndex);
   J9UTF8 *romClassName = J9ROMCLASS_CLASSNAME(romClassPtr());
   J9UTF8 *romClassRefName = J9ROMCLASSREF_NAME(romClassRef);

   if (J9UTF8_EQUALS(romClassRefName, romClassName))
      return true;
   else
      return false;
   }


TR_OpaqueMethodBlock *
TR_J9VMBase::getResolvedVirtualMethod(TR_OpaqueClassBlock * classObject, I_32 virtualCallOffset, bool ignoreRtResolve)
   {
   // the classObject is the fixed type of the this pointer.  The result of this method is going to be
   // used to call the virtual function directly.
   //
   // virtualCallOffset = -(vTableSlot(vmMethod, cpIndex) - J9JIT_INTERP_VTABLE_OFFSET);
   //
   if (isInterfaceClass(classObject))
      return 0;

   INCREMENT_COUNTER(this, totalVirtualMethodRefs);

   J9Method * ramMethod = *(J9Method **)((char *)TR::Compiler->cls.convertClassOffsetToClassPtr(classObject) + virtualCallOffsetToVTableSlot(virtualCallOffset));

   TR_ASSERT(ramMethod, "getResolvedVirtualMethod should always find a ramMethod in the vtable slot");

   if (ramMethod &&
       (!(_jitConfig->runtimeFlags & J9JIT_RUNTIME_RESOLVE) ||
         ignoreRtResolve) &&
       J9_BYTECODE_START_FROM_RAM_METHOD(ramMethod))
      return (TR_OpaqueMethodBlock* ) ramMethod;

   INCREMENT_COUNTER(this, unresolvedVirtualMethodRefs);
   return 0;
   }

TR_OpaqueMethodBlock *
TR_J9VMBase::getResolvedInterfaceMethod(TR_OpaqueMethodBlock *interfaceMethod, TR_OpaqueClassBlock * classObject, I_32 cpIndex)
   {
   TR::VMAccessCriticalSection getResolvedInterfaceMethod(this);
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");

   // the classObject is the fixed type of the this pointer.  The result of this method is going to be
   // used to call the interface function directly.
   //
   INCREMENT_COUNTER(this, unresolvedInterfaceMethodRefs);
   INCREMENT_COUNTER(this, totalInterfaceMethodRefs);

   J9Method * ramMethod = jitGetInterfaceMethodFromCP(vmThread(),
                                                      (J9ConstantPool *)(J9_CP_FROM_METHOD((J9Method*)interfaceMethod)),
                                                      cpIndex,
                                                      TR::Compiler->cls.convertClassOffsetToClassPtr(classObject));

   return (TR_OpaqueMethodBlock *)ramMethod;
   }

J9UTF8 * getSignatureFromTR_VMMethod(TR_OpaqueMethodBlock *vmMethod)
   {
   TR_ASSERT(0, "I don't think these are used");
   return 0;
   }

J9UTF8 * getNameFromTR_VMMethod(TR_OpaqueMethodBlock *vmMethod)
   {
   TR_ASSERT(0, "I don't think these are used");
   return 0;
   }

J9UTF8 * getClassNameFromTR_VMMethod(TR_OpaqueMethodBlock *vmMethod)
   {
   TR_ASSERT(0, "I don't think these are used");
   return 0;
   }

J9Class * getRAMClassFromTR_ResolvedVMMethod(TR_OpaqueMethodBlock *vmMethod)
   {
   return (J9Class *) ((TR_ResolvedMethod *)vmMethod)->addressOfClassOfMethod();
   }

// these two should be member functions of TR::ResolvedMethodSymbol

bool mightWalkTheStackToGetToUserCode(TR::MethodSymbol * sym)
   {
   return true;
   }

TR_J9MethodParameterIterator::TR_J9MethodParameterIterator(TR_J9MethodBase &j9Method, TR::Compilation& comp, TR_ResolvedMethod *resolvedMethod) :
   TR_MethodParameterIterator(comp),
   _j9Method(j9Method), _sig(j9Method.signatureChars()),
   _resolvedMethod(resolvedMethod)
   {
   TR_ASSERT(*_sig == '(', "Bad signature passed to TR_J9MethodParameterIterator");
   ++_sig;
   _nextIncrBy = 0;
   }

TR::DataType TR_J9MethodParameterIterator::getDataType()
   {
   if (*_sig == 'L' || *_sig == '[')
      {
      _nextIncrBy = 0;
      while (_sig[_nextIncrBy] == '[')
    {
    ++_nextIncrBy;
    }
      if (_sig[_nextIncrBy] != 'L')
    {
    // Primitive array
    ++_nextIncrBy;
    }
      else
    {
    while (_sig[_nextIncrBy++] != ';') ;
    }
      return TR::Aggregate;
      }
   else
      {
      _nextIncrBy = 1;
      if (*_sig == 'Z')
    {
    return TR::Int8;
    }
      else if (*_sig == 'B')
    {
    return TR::Int8;
    }
      else if (*_sig == 'C')
    {
    return TR::Int16;
    }
      else if (*_sig == 'S')
    {
    return TR::Int16;
    }
      else if (*_sig == 'I')
    {
    return TR::Int32;
    }
      else if (*_sig == 'J')
    {
    return TR::Int64;
    }
      else if (*_sig == 'F')
    {
    return TR::Float;
    }
      else if (*_sig == 'D')
    {
    return TR::Double;
    }
      else
    {
    TR_ASSERT(0, "Unknown character in Java signature.");
    return TR::NoType;
    }
      }
   }

TR_OpaqueClassBlock * TR_J9MethodParameterIterator::getOpaqueClass()
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(_comp.fe());
   TR_ASSERT(*_sig == '[' || *_sig == 'L', "Asked for class of incorrect Java parameter.");
   if (_nextIncrBy == 0) getDataType();
   return _resolvedMethod == NULL ? NULL :
      fej9->getClassFromSignature(_sig, _nextIncrBy, _resolvedMethod);
   }

char *  TR_J9MethodParameterIterator::getUnresolvedJavaClassSignature(uint32_t& len)
   {
   len = _nextIncrBy;
   return _sig;
   }

bool TR_J9MethodParameterIterator::isArray()
   {
   return (*_sig == '[');
   }

bool TR_J9MethodParameterIterator::isClass()
   {
   return (*_sig == 'L');
   }

bool TR_J9MethodParameterIterator::atEnd()
   {
   return (*_sig == ')');
   }

void TR_J9MethodParameterIterator::advanceCursor()
   {
   if (_nextIncrBy == 0) getDataType();
   _sig += _nextIncrBy;
   _nextIncrBy = 0;
   return;
   }

//////////////////////////////////////////////////////////
// ILGen
//////////////////////////////////////////////////////////

static J9::MethodHandleThunkDetails *
getMethodHandleThunkDetails(TR_J9ByteCodeIlGenerator *ilgen, TR::Compilation *comp, TR::SymbolReference *macro)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   TR::IlGeneratorMethodDetails & details = ilgen->methodDetails();
   if (details.isMethodHandleThunk())
      {
      return & static_cast<J9::MethodHandleThunkDetails &>(details);
      }
   else if (comp->isPeekingMethod())
      {
      if (comp->getOption(TR_TraceILGen))
         traceMsg(comp, "  Conservatively leave ILGen macro '%s' as a native call for peeking\n", comp->getDebug()->getName(macro));
      }
   else
      {
      if (comp->getOption(TR_TraceILGen))
         traceMsg(comp, "  Conservatively abort compile due to presence of ILGen macro '%s'\n", comp->getDebug()->getName(macro));
      comp->failCompilation<TR::ILGenFailure>("Found a call to an ILGen macro requiring a MethodHandle");
      }

   return NULL;
   }

bool
TR_J9ByteCodeIlGenerator::runFEMacro(TR::SymbolReference *symRef)
   {
   TR::MethodSymbol * symbol = symRef->getSymbol()->castToMethodSymbol();
   int32_t archetypeParmCount = symbol->getMethod()->numberOfExplicitParameters() + (symbol->isStatic() ? 0 : 1);

   // Random handy variables
   bool isVirtual = false;
   bool isInterface = false;
   bool isIndirect = false;
   bool returnFromArchetype = false;
   char *nextHandleSignature = NULL;

   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe();

   switch (symRef->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod())
      {
      case TR::java_lang_invoke_CollectHandle_numArgsToPassThrough:
      case TR::java_lang_invoke_CollectHandle_numArgsToCollect:
      case TR::java_lang_invoke_CollectHandle_collectionStart:
      case TR::java_lang_invoke_CollectHandle_numArgsAfterCollectArray:
         {
         TR_ASSERT(archetypeParmCount == 0, "assertion failure");

         J9::MethodHandleThunkDetails *thunkDetails = getMethodHandleThunkDetails(this, comp(), symRef);
         if (!thunkDetails)
            return false;

         uintptrj_t methodHandle;
         int32_t collectArraySize;
         int32_t collectionStart;
         uintptrj_t arguments;
         int32_t numArguments;

            {
            TR::VMAccessCriticalSection invokeCollectHandleNumArgsToCollect(fej9);
            methodHandle = *thunkDetails->getHandleRef();
            collectArraySize = fej9->getInt32Field(methodHandle, "collectArraySize");
            if ((symRef->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod() ==
            		TR::java_lang_invoke_CollectHandle_collectionStart)
            || (symRef->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod() ==
            		TR::java_lang_invoke_CollectHandle_numArgsAfterCollectArray)
			) {
            	collectionStart = fej9->getInt32Field(methodHandle, "collectPosition");
            }
            arguments = fej9->getReferenceField(fej9->getReferenceField(methodHandle, "type", "Ljava/lang/invoke/MethodType;"), "arguments", "[Ljava/lang/Class;");
            numArguments = (int32_t)fej9->getArrayLengthInElements(arguments);
            }

         switch (symRef->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod())
            {
            case TR::java_lang_invoke_CollectHandle_numArgsToPassThrough:
               loadConstant(TR::iconst, numArguments - collectArraySize);
               break;
            case TR::java_lang_invoke_CollectHandle_numArgsToCollect:
               loadConstant(TR::iconst, collectArraySize);
               break;
            case TR::java_lang_invoke_CollectHandle_collectionStart:
               loadConstant(TR::iconst, collectionStart);
               break;
            case TR::java_lang_invoke_CollectHandle_numArgsAfterCollectArray:
               loadConstant(TR::iconst, numArguments - collectionStart - collectArraySize);
               break;
            default:
            	break;
            }
         return true;
         }
      case TR::java_lang_invoke_AsTypeHandle_convertArgs:
      case TR::java_lang_invoke_ExplicitCastHandle_convertArgs:
         {
         bool isExplicit = symRef->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod() == TR::java_lang_invoke_ExplicitCastHandle_convertArgs;

         // Get source and target signature strings
         //
         char *sourceSig = _methodSymbol->getResolvedMethod()->signatureChars() + 1; // skip parenthesis

         J9::MethodHandleThunkDetails *thunkDetails = getMethodHandleThunkDetails(this, comp(), symRef);
         if (!thunkDetails)
            return false;

         uintptrj_t methodHandle;
         uintptrj_t methodDescriptorRef;
         intptrj_t methodDescriptorLength;
         char *methodDescriptor;

            {
            TR::VMAccessCriticalSection invokeExplicitCastHandleConvertArgs(fej9);
            methodHandle = *thunkDetails->getHandleRef();
            methodDescriptorRef = fej9->getReferenceField(fej9->getReferenceField(fej9->getReferenceField(
               methodHandle,
               "next",             "Ljava/lang/invoke/MethodHandle;"),
               "type",             "Ljava/lang/invoke/MethodType;"),
               "methodDescriptor", "Ljava/lang/String;");
            methodDescriptorLength = fej9->getStringUTF8Length(methodDescriptorRef);
            methodDescriptor = (char*)alloca(methodDescriptorLength+1);
            fej9->getStringUTF8(methodDescriptorRef, methodDescriptor, methodDescriptorLength+1);
            }

         // Create a placeholder to cause argument expressions to be expanded
         //
         TR::Node *placeholder = genNodeAndPopChildren(TR::icall, 1, placeholderWithDummySignature());

         // For each child, call the appropriate convert method
         //
         char *targetSig = methodDescriptor+1; // skip parenthesis
         int firstArgSlot = _methodSymbol->isStatic()? 0 : 1;
         for (
            int32_t argIndex = 0;
            sourceSig[0] != ')';
            argIndex++, sourceSig = nextSignatureArgument(sourceSig), targetSig = nextSignatureArgument(targetSig)
            ){
            TR::Node *argValue = placeholder->getChild(argIndex);

            // Inspect source and target types and decide what to do
            //
            char sourceBuf[2], targetBuf[2];
            char *sourceName = sourceBuf; char *sourceType = sourceBuf;
            char *targetName = targetBuf; char *targetType = targetBuf;
            switch (sourceSig[0])
               {
               case 'L':
               case '[':
                  sourceName = "object";
                  sourceType = "Ljava/lang/Object;";
                  break;
               default:
                  sprintf(sourceBuf, "%c", sourceSig[0]);
                  break;
               }
            switch (targetSig[0])
               {
               case 'L':
               case '[':
                  targetName = "object";
                  targetType = "Ljava/lang/Object;";
                  break;
               default:
                  sprintf(targetBuf, "%c", targetSig[0]);
                  break;
               }

            // Call converter function if types are different
            //
            if (strcmp(sourceType, targetType))
               {
               char methodName[30], methodSignature[50];
               if (sourceType[0] == 'L' && isExplicit)
                  sprintf(methodName, "explicitObject2%s", targetName);
               else
                  sprintf(methodName, "%s2%s", sourceName, targetName);
               sprintf(methodSignature, "(%s)%s", sourceType, targetType);
               TR::SymbolReference *methodSymRef = comp()->getSymRefTab()->methodSymRefFromName(_methodSymbol,
                                                                                                "java/lang/invoke/ConvertHandle$FilterHelpers",
                                                                                                methodName,
                                                                                                methodSignature,
                                                                                                TR::MethodSymbol::Static);
               TR_ASSERT(methodSymRef, "assertion failure");

               argValue = TR::Node::createWithSymRef(methodSymRef->getSymbol()->castToMethodSymbol()->getMethod()->directCallOpCode(),
                  1, 1,
                  argValue,
                  methodSymRef);
               genTreeTop(argValue);
               }

            // Address conversions need a downcast after the call
            //
            if (targetType[0] == 'L')
               {
               uintptrj_t methodHandle;
               uintptrj_t arguments;
               TR_OpaqueClassBlock *parmClass;

                  {
                  TR::VMAccessCriticalSection targetTypeL(fej9);
                  // We've already loaded the handle once, but must reload it because we released VM access in between.
                  methodHandle = *thunkDetails->getHandleRef();
                  arguments = fej9->getReferenceField(fej9->getReferenceField(fej9->getReferenceField(
                     methodHandle,
                     "next",             "Ljava/lang/invoke/MethodHandle;"),
                     "type",             "Ljava/lang/invoke/MethodType;"),
                     "arguments",        "[Ljava/lang/Class;");
                  parmClass = (TR_OpaqueClassBlock*)(intptrj_t)fej9->getInt64Field(fej9->getReferenceElement(arguments, argIndex),
                                                                                   "vmRef" /* should use fej9->getOffsetOfClassFromJavaLangClassField() */);
                  }

               if (fej9->isInterfaceClass(parmClass) && isExplicit)
                  {
                  // explicitCastArguments specifies that we must not checkcast interfaces
                  }
               else
                  {
                  push(argValue);
                  loadClassObject(parmClass);
                  genCheckCast();
                  pop();
                  }
               }

            // If we did anything, replace the placeholder's child with the converted value
            //
            if (argValue != placeholder->getChild(argIndex))
               {
               placeholder->getAndDecChild(argIndex);
               placeholder->setAndIncChild(argIndex, argValue);
               }
            }

         // Correct the placeholder's signature.
         // Arg types match the target type; return type (as for all placeholders) is int.
         //
         placeholder->setSymbolReference(symRefWithArtificialSignature(placeholder->getSymbolReference(),
            "(.*)I", methodDescriptor, 0));

         // Placeholder now contains the conversion calls we want to pass on.
         //
         push(placeholder);
         }
         return true;
      case TR::java_lang_invoke_ILGenMacros_invokeExact_X:
         returnFromArchetype = true;
         // fall through
      case TR::java_lang_invoke_ILGenMacros_invokeExactAndFixup:
         {
         // We have a child claiming to return an int, and a parent claiming to
         // take an int, but the actual type is something else.

         J9::MethodHandleThunkDetails *thunkDetails = getMethodHandleThunkDetails(this, comp(), symRef);
         if (!thunkDetails)
            return false;

         TR::Node *methodHandleExpression = _stack->element(_stack->size() - archetypeParmCount);

         // Grab the proper return type signature string from the methodHandleExpression
         //
         uintptrj_t receiverHandle;
         uintptrj_t methodHandle;
         uintptrj_t methodDescriptorRef;
         intptrj_t methodDescriptorLength;
         char *methodDescriptor;

            {
            TR::VMAccessCriticalSection invokeILGenMacrosInvokeExactAndFixup(fej9);
            receiverHandle = *thunkDetails->getHandleRef();
            methodHandle   = returnFromArchetype? receiverHandle : walkReferenceChain(methodHandleExpression, receiverHandle);
            methodDescriptorRef = fej9->getReferenceField(fej9->getReferenceField(
               methodHandle,
               "type",             "Ljava/lang/invoke/MethodType;"),
               "methodDescriptor", "Ljava/lang/String;");
            methodDescriptorLength = fej9->getStringUTF8Length(methodDescriptorRef);
            methodDescriptor = (char*)alloca(methodDescriptorLength+1);
            fej9->getStringUTF8(methodDescriptorRef, methodDescriptor, methodDescriptorLength+1);
            }

         char *returnType = strchr(methodDescriptor, ')') + 1;
         if (comp()->getOption(TR_TraceILGen))
            traceMsg(comp(), "  invokeExactAndFixup: return type of %p is %s\n", methodHandle, returnType); // Technically methodHandle could be stale here

         // Generate an invokeHandle using a symref with the proper return type
         //
         push(methodHandleExpression); // genInvokeHandle needs this on top of stack
         TR_ResolvedMethod  *invokeExactMacro = symRef->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod();
         TR::SymbolReference *invokeExact = comp()->getSymRefTab()->methodSymRefFromName(_methodSymbol, JSR292_MethodHandle, JSR292_invokeExact, JSR292_invokeExactSig, TR::MethodSymbol::ComputedVirtual);
         TR::SymbolReference *invokeExactWithSig = symRefWithArtificialSignature(
            invokeExact,
            "(.*).?",
            invokeExactMacro->signatureChars(), 1, // skip explicit MethodHandle argument -- the real invokeExact has it as a receiver
            returnType);
         TR::Node *invokeExactCall = genInvokeHandle(invokeExactWithSig);

         // Edit the call opcode
         //
         TR::ILOpCodes callOp;
         switch (returnType[0])
            {
            case 'Z':
            case 'B':
            case 'S':
            case 'C':
            case 'I':
               callOp = TR::icalli;
               break;
            case 'J':
               callOp = TR::lcalli;
               break;
            case 'F':
               callOp = TR::fcalli;
               break;
            case 'D':
               callOp = TR::dcalli;
               break;
            case 'L':
            case '[':
               callOp = TR::acalli;
               break;
            case 'V':
               callOp = TR::calli;
               break;
            default:
               TR_ASSERT(0, "Unknown return type '%s'", returnType);
               callOp = invokeExactCall->getOpCodeValue();
               break;
            }
         TR::Node::recreate(invokeExactCall, callOp);

         if (returnFromArchetype)
            {
            // The archetype will use the right return opcode; nothing to do here.
            }
         else
            {
            // Cap invokeExactCall with a placeholder so parent nodes will edit their own signatures
            //
            if (callOp == TR::calli)
               {
               TR::SymbolReference *placeholder = symRefWithArtificialSignature(placeholderWithDummySignature(), "()I");
               push(genNodeAndPopChildren(TR::icall, 0, placeholder));
               }
            else
               {
               TR::SymbolReference *placeholder = symRefWithArtificialSignature(placeholderWithDummySignature(), "(.?)I", returnType);
               push(genNodeAndPopChildren(TR::icall, 1, placeholder));
               }
            }
         }
         return true;
      case TR::java_lang_invoke_ArgumentMoverHandle_permuteArgs:
         {
         J9::MethodHandleThunkDetails *thunkDetails = getMethodHandleThunkDetails(this, comp(), symRef);
         if (!thunkDetails)
            return false;

         uintptrj_t methodHandle;
         uintptrj_t methodDescriptorRef;
         intptrj_t methodDescriptorLength;

            {
            TR::VMAccessCriticalSection invokeArgumentMoverHandlePermuteArgs(fej9);
            methodHandle = *thunkDetails->getHandleRef();
            methodDescriptorRef = fej9->getReferenceField(fej9->getReferenceField(fej9->getReferenceField(
               methodHandle,
               "next",             "Ljava/lang/invoke/MethodHandle;"),
               "type",             "Ljava/lang/invoke/MethodType;"),
               "methodDescriptor", "Ljava/lang/String;");
            methodDescriptorLength = fej9->getStringUTF8Length(methodDescriptorRef);
            nextHandleSignature = (char*)alloca(methodDescriptorLength+1);
            fej9->getStringUTF8(methodDescriptorRef, nextHandleSignature, methodDescriptorLength+1);
            }

         if (comp()->getOption(TR_TraceILGen))
            traceMsg(comp(), "  permuteArgs: nextHandleSignature is %s\n", nextHandleSignature);
         }
         // FALL THROUGH
      case TR::java_lang_invoke_PermuteHandle_permuteArgs:
         {
         J9::MethodHandleThunkDetails *thunkDetails = getMethodHandleThunkDetails(this, comp(), symRef);
         if (!thunkDetails)
            return false;

         // Grab the permute array.  Note we don't release access until we're done with the array.
         //
         int32_t i;
         int32_t permuteLength;
         TR::Node *originalArgs;
         char * oldSignature;
         char * newSignature;

            {
            TR::VMAccessCriticalSection invokePermuteHandlePermuteArgs(fej9);
            uintptrj_t methodHandle = *thunkDetails->getHandleRef();
            uintptrj_t permuteArray = fej9->getReferenceField(methodHandle, "permute", "[I");

            // Create temporary placeholder to cause argument expressions to be expanded
            //
           originalArgs = genNodeAndPopChildren(TR::icall, 1, placeholderWithDummySignature());

            // Push args while computing the result placeholder's signature
            //
            oldSignature = originalArgs->getSymbolReference()->getSymbol()->getResolvedMethodSymbol()->getResolvedMethod()->signatureChars();
            newSignature = "()I";
            permuteLength = fej9->getArrayLengthInElements(permuteArray);
            if (comp()->getOption(TR_TraceILGen))
               traceMsg(comp(), "  permuteArgs: oldSignature is %s\n", oldSignature);
            for (i=0; i < permuteLength; i++)
               {
               int32_t argIndex = fej9->getInt32Element(permuteArray, i);
               if (argIndex >= 0)
                  {
                  newSignature = artificialSignature(stackAlloc, "(.*.@)I",
                     newSignature, 0,
                     oldSignature, argIndex);
                  push(originalArgs->getChild(argIndex));
                  if (comp()->getOption(TR_TraceILGen))
                     traceMsg(comp(), "  permuteArgs:   %d: incoming argument\n", argIndex);
                  }
               else
                  {
                  newSignature = artificialSignature(stackAlloc, "(.*.@)I",
                     newSignature, 0,
                     nextHandleSignature, i);
                  char *argType = nthSignatureArgument(i, nextHandleSignature+1);
                  char  extraName[10];
                  char *extraSignature;
                  switch (argType[0])
                     {
                     case 'L':
                     case '[':
                        sprintf(extraName, "extra_L");
                        extraSignature = artificialSignature(stackAlloc, "(L" JSR292_ArgumentMoverHandle ";I)Ljava/lang/Object;");
                        break;
                     default:
                        sprintf(extraName, "extra_%c", argType[0]);
                        extraSignature = artificialSignature(stackAlloc, "(L" JSR292_ArgumentMoverHandle ";I).@", nextHandleSignature, i);
                        break;
                     }
                  if (comp()->getOption(TR_TraceILGen))
                     traceMsg(comp(), "  permuteArgs:   %d: call to %s.%s%s\n", argIndex, JSR292_ArgumentMoverHandle, extraName, extraSignature);
                  TR::SymbolReference *extra = comp()->getSymRefTab()->methodSymRefFromName(_methodSymbol, JSR292_ArgumentMoverHandle, extraName, extraSignature, TR::MethodSymbol::Static);
                  loadAuto(TR::Address, 0);
                  loadConstant(TR::iconst, argIndex);
                  genInvokeDirect(extra);
                  }
               }
            if (comp()->getOption(TR_TraceILGen))
               traceMsg(comp(), "  permuteArgs: permuted placeholder signature is %s\n", newSignature);

            // We're done with the permute array
            //
            permuteArray = 0;
            }

         // Push a placeholder with the permuted args as children
         //
         TR::SymbolReference *placeholder = symRefWithArtificialSignature(placeholderWithDummySignature(), newSignature);
         push(genNodeAndPopChildren(TR::icall, permuteLength, placeholder));

         // Clean up the temporary placeholder
         //
         for (i=0; i < originalArgs->getNumChildren(); i++)
            originalArgs->getAndDecChild(i);

         }
         return true;
      case TR::java_lang_invoke_GuardWithTestHandle_numGuardArgs:
         {
         TR_ASSERT(archetypeParmCount == 0, "assertion failure");

         J9::MethodHandleThunkDetails *thunkDetails = getMethodHandleThunkDetails(this, comp(), symRef);
         if (!thunkDetails)
            return false;

         uintptrj_t methodHandle;
         uintptrj_t guardArgs;
         int32_t numGuardArgs;

            {
            TR::VMAccessCriticalSection invokeGuardWithTestHandleNumGuardArgs(fej9);
            methodHandle = *thunkDetails->getHandleRef();
            guardArgs = fej9->getReferenceField(fej9->methodHandle_type(fej9->getReferenceField(methodHandle,
               "guard", "Ljava/lang/invoke/MethodHandle;")),
               "arguments", "[Ljava/lang/Class;");
            numGuardArgs = (int32_t)fej9->getArrayLengthInElements(guardArgs);
            }

         loadConstant(TR::iconst, numGuardArgs);
         return true;
         }
      case TR::java_lang_invoke_InsertHandle_numPrefixArgs:
      case TR::java_lang_invoke_InsertHandle_numSuffixArgs:
      case TR::java_lang_invoke_InsertHandle_numValuesToInsert:
         {
         J9::MethodHandleThunkDetails *thunkDetails = getMethodHandleThunkDetails(this, comp(), symRef);
         if (!thunkDetails)
            return false;

         uintptrj_t methodHandle;
         int32_t insertionIndex;
         uintptrj_t arguments;
         int32_t numArguments;
         uintptrj_t values;
         int32_t numValues;

            {
            TR::VMAccessCriticalSection invokeInsertHandle(fej9);
            methodHandle = *thunkDetails->getHandleRef();
            insertionIndex = fej9->getInt32Field(methodHandle, "insertionIndex");
            arguments = fej9->getReferenceField(fej9->getReferenceField(methodHandle, "type", "Ljava/lang/invoke/MethodType;"), "arguments", "[Ljava/lang/Class;");
            numArguments = (int32_t)fej9->getArrayLengthInElements(arguments);
            values = fej9->getReferenceField(methodHandle, "values", "[Ljava/lang/Object;");
            numValues = (int32_t)fej9->getArrayLengthInElements(values);
            }

         switch (symRef->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod())
            {
            case TR::java_lang_invoke_InsertHandle_numPrefixArgs:
               loadConstant(TR::iconst, insertionIndex);
               break;
            case TR::java_lang_invoke_InsertHandle_numSuffixArgs:
               loadConstant(TR::iconst, numArguments - insertionIndex);
               break;
            case TR::java_lang_invoke_InsertHandle_numValuesToInsert:
               loadConstant(TR::iconst, numValues);
               break;
            default:
            	break;
            }
         }
         return true;
      case TR::java_lang_invoke_InterfaceHandle_interfaceCall:
         isInterface = true;
         // fall through
      case TR::java_lang_invoke_VirtualHandle_virtualCall:
         isVirtual = !isInterface;
         isIndirect = true;
         // fall through
      case TR::java_lang_invoke_DirectHandle_directCall:
         {
         J9::MethodHandleThunkDetails *thunkDetails = getMethodHandleThunkDetails(this, comp(), symRef);
         if (!thunkDetails)
            return false;

         TR_OpaqueMethodBlock *j9method;
         uintptrj_t methodHandle;
         int64_t vmSlot;
         uintptrj_t jlClass;

            {
            TR::VMAccessCriticalSection invokeDirectHandleDirectCall(fej9);
            methodHandle   = *thunkDetails->getHandleRef();
            vmSlot         = fej9->getInt64Field(methodHandle, "vmSlot");


            jlClass = fej9->getReferenceField(methodHandle, "defc", "Ljava/lang/Class;");
            if (isInterface)
                {
                TR_OpaqueClassBlock *clazz = fej9->getClassFromJavaLangClass(jlClass);
                j9method = (TR_OpaqueMethodBlock*)&(((J9Class *)clazz)->ramMethods[vmSlot]);
                }
            else if (isVirtual)
               {
               TR_OpaqueMethodBlock **vtable = (TR_OpaqueMethodBlock**)(((uintptrj_t)fej9->getClassFromJavaLangClass(jlClass)) + J9JIT_INTERP_VTABLE_OFFSET);
               int32_t index = (int32_t)((vmSlot - J9JIT_INTERP_VTABLE_OFFSET) / sizeof(vtable[0]));
               j9method = vtable[index];
               }
            else
               {
               j9method = (TR_OpaqueMethodBlock*)(intptrj_t)vmSlot;
               }
            }

         TR_ASSERT(j9method, "Must have a j9method to generate a custom call");

         TR_ResolvedMethod *method = fej9->createResolvedMethod(comp()->trMemory(), j9method);
         TR::Node *callNode;
         if (isIndirect)
            {
            callNode = genNodeAndPopChildren(method->indirectCallOpCode(), archetypeParmCount+1, symRef, 1);
            TR::Node *vftLoad = TR::Node::createWithSymRef(TR::aloadi, 1, 1, callNode->getArgument(0), symRefTab()->findOrCreateVftSymbolRef());
            callNode->setAndIncChild(0, vftLoad);
            }
         else
            {
            callNode = genNodeAndPopChildren(method->directCallOpCode(), archetypeParmCount, symRef);
            }

         // Note: at this point, the callNode's is a signature-edited version
         // of the ILGen macro's own symref whose signature is "compatible"
         // with the real symref we need to use, in the sense that any
         // differences won't affect argument passing.  There may be some
         // immaterial differences in the precise reference types, and in the
         // sub-integer types.

         // Now we substitute the proper symref
         //
         TR::MethodSymbol::Kinds kind;
         if (isInterface)
            kind = TR::MethodSymbol::Interface;
         else if (isVirtual)
            kind = TR::MethodSymbol::Virtual;
         else if (method->isStatic())
            kind = TR::MethodSymbol::Static;
         else
            kind = TR::MethodSymbol::Special; // TODO: Is this right?  Can I always consider a direct call with a receiver to be special?
         callNode->setSymbolReference(comp()->getSymRefTab()->findOrCreateMethodSymbol(_methodSymbol->getResolvedMethodIndex(), -1, method, kind));

         if (callNode->getDataType() != TR::NoType)
            push(callNode);

         if (isVirtual)
            {
            callNode->getSymbolReference()->setOffset(-(vmSlot - J9JIT_INTERP_VTABLE_OFFSET));
            genTreeTop(genNullCheck(callNode));
            }
         else
            {
            genTreeTop(callNode);
            }
         }
         return true;
      case TR::java_lang_invoke_DirectHandle_isAlreadyCompiled:
      case TR::java_lang_invoke_DirectHandle_compiledEntryPoint:
         {
         // Wow, these are painful.

         TR::SymbolReference *extraField = comp()->getSymRefTab()->findOrCreateJ9MethodExtraFieldSymbolRef(offsetof(struct J9Method, extra));
         TR::Node *j9methodAddress = pop();
         if (TR::Compiler->target.is64Bit())
            j9methodAddress = TR::Node::create(TR::l2a, 1, j9methodAddress);
         else
            j9methodAddress = TR::Node::create(TR::i2a, 1, TR::Node::create(TR::l2i, 1, j9methodAddress));
         TR::Node *extra = TR::Node::createWithSymRef(comp()->il.opCodeForIndirectLoad(extraField->getSymbol()->getDataType()), 1, 1, j9methodAddress, extraField);
         TR::Node *result=0;
         switch (symRef->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod())
            {
            case TR::java_lang_invoke_DirectHandle_isAlreadyCompiled:
               {
               TR::ILOpCodes xcmpeq = TR::Compiler->target.is64Bit()? TR::lcmpeq : TR::icmpeq;
               TR::ILOpCodes xand   = TR::Compiler->target.is64Bit()? TR::land   : TR::iand;
               TR::Node *zero = TR::Compiler->target.is64Bit()? TR::Node::lconst(extra, 0) : TR::Node::iconst(extra, 0);
               TR::Node *mask = TR::Compiler->target.is64Bit()? TR::Node::lconst(extra, J9_STARTPC_NOT_TRANSLATED) : TR::Node::iconst(extra, J9_STARTPC_NOT_TRANSLATED);
               result =
                  TR::Node::create(xcmpeq, 2,
                     TR::Node::create(xand, 2, extra, mask),
                     zero);
               }
               break;
            case TR::java_lang_invoke_DirectHandle_compiledEntryPoint:
               if (TR::Compiler->target.cpu.isI386())
                  {
                  // IA32 does not store jitEntryOffset in the linkageInfo word
                  // so it would be incorrect to try to load it from there.
                  // Luckily, jitEntryOffset is always zero, so we can just ignore it.
                  //
                  result = TR::Node::create(TR::i2l, 1, extra);
                  }
               else
                  {
                  TR::SymbolReference *linkageInfoSymRef = comp()->getSymRefTab()->findOrCreateStartPCLinkageInfoSymbolRef(-4);
                  TR::ILOpCodes x2a = TR::Compiler->target.is64Bit()? TR::l2a : TR::i2a;
                  TR::Node *linkageInfo    = TR::Node::createWithSymRef(TR::iloadi, 1, 1, TR::Node::create(x2a, 1, extra), linkageInfoSymRef);
                  TR::Node *jitEntryOffset = TR::Node::create(TR::ishr,   2, linkageInfo, TR::Node::iconst(extra, 16));
                  if (TR::Compiler->target.is64Bit())
                     result = TR::Node::create(TR::ladd, 2, extra, TR::Node::create(TR::i2l, 1, jitEntryOffset));
                  else
                     result = TR::Node::create(TR::iadd, 2, extra, jitEntryOffset);
                  }
               break;
            default:
            	break;
            }
         push(result);
         }
         return true;
      case TR::java_lang_invoke_SpreadHandle_arrayArg:
         {
         J9::MethodHandleThunkDetails *thunkDetails = getMethodHandleThunkDetails(this, comp(), symRef);
         if (!thunkDetails)
            return false;

         // Expand args and then discard all but the last one
         //
         uintptrj_t methodHandle;
         uintptrj_t arrayClass;
         J9ArrayClass *arrayJ9Class;
         int32_t spreadPosition = -1;
            {
            TR::VMAccessCriticalSection invokeSpreadHandleArrayArg(fej9);
            methodHandle = *thunkDetails->getHandleRef();
            arrayClass   = fej9->getReferenceField(methodHandle, "arrayClass", "Ljava/lang/Class;");
            arrayJ9Class = (J9ArrayClass*)(intptrj_t)fej9->getInt64Field(arrayClass,
                                                                         "vmRef" /* should use fej9->getOffsetOfClassFromJavaLangClassField() */);
            uint32_t spreadPositionOffset = fej9->getInstanceFieldOffset(fej9->getObjectClass(methodHandle), "spreadPosition", "I");
            if (spreadPositionOffset != ~0)
               spreadPosition = fej9->getInt32FieldAt(methodHandle, spreadPositionOffset);
            }

         TR::Node *placeholder = genNodeAndPopChildren(TR::icall, 1, placeholderWithDummySignature());
         if (spreadPosition == -1)
            {
            // TODO: Code for old implementation, delete it when J9 JCL change is delivered
            for (int32_t i = placeholder->getNumChildren()-2; i >= 0; i--)
               placeholder->getAndDecChild(i);
            placeholder->setFirst(placeholder->getLastChild());
            }
         else
            {
            for (int32_t i = 0; i< spreadPosition; i++)
               placeholder->getAndDecChild(i);
            for (int32_t i = placeholder->getNumChildren()-1; i > spreadPosition; i--)
               placeholder->getAndDecChild(i);
            placeholder->setFirst(placeholder->getChild(spreadPosition));
            }
         placeholder->setNumChildren(1);

         // Construct the signature string for the array class
         //
         UDATA arity = arrayJ9Class->arity;
         J9Class *leafClass = arrayJ9Class->leafComponentType;
         int32_t leafClassNameLength;
         char *leafClassNameChars = fej9->getClassNameChars((TR_OpaqueClassBlock*)leafClass, leafClassNameLength); // eww, TR_FrontEnd downcast

         char *arrayClassSignature = (char*)alloca(arity + leafClassNameLength + 3); // 3 = 'L' + ';' + null terminator
         memset(arrayClassSignature, '[', arity);
         if (TR::Compiler->cls.isPrimitiveClass(comp(), (TR_OpaqueClassBlock*)leafClass))
            {
            // In this case, leafClassName is something like "int" rather than "I".
            // For an array like [[[[I, the rom arrayclass will think its name is [I.
            // We just need to add arity-1 brackets to the start.  Since we've already
            // added arity, we move one character to the left to delete the extra bracket.
            //
            int32_t arrayRomClassNameLength;
            char *arrayRomClassNameChars = fej9->getClassNameChars((TR_OpaqueClassBlock*)arrayJ9Class, arrayRomClassNameLength);
            TR_ASSERT(arrayRomClassNameLength == 2, "Every array romclass '%.*s' should be of the form [X where X is a single character", arrayRomClassNameLength, arrayRomClassNameChars);
            sprintf(arrayClassSignature+arity-1, "%.*s", arrayRomClassNameLength, arrayRomClassNameChars);
            }
         else
            {
            sprintf(arrayClassSignature+arity, "L%.*s;", leafClassNameLength, leafClassNameChars);
            }

         if (comp()->getOption(TR_TraceILGen))
            traceMsg(comp(), "  SpreadHandle.arrayArg: Array class name is '%s'\n", arrayClassSignature);

         // Edit placeholder signature to give the right class for the array
         placeholder->setSymbolReference(symRefWithArtificialSignature(placeholder->getSymbolReference(),
            "(.?)I", arrayClassSignature));

         push(placeholder);
         return true;
         }
      case TR::java_lang_invoke_SpreadHandle_numArgsToPassThrough:
      case TR::java_lang_invoke_SpreadHandle_numArgsToSpread:
      case TR::java_lang_invoke_SpreadHandle_numArgsAfterSpreadArray:
      case TR::java_lang_invoke_SpreadHandle_spreadStart:
         {
         TR_ASSERT(archetypeParmCount == 0, "assertion failure");

         J9::MethodHandleThunkDetails *thunkDetails = getMethodHandleThunkDetails(this, comp(), symRef);
         if (!thunkDetails)
            return false;

         uintptrj_t methodHandle;
         uintptrj_t arguments;
         int32_t numArguments;
         uintptrj_t next;
         uintptrj_t nextArguments;
         int32_t numNextArguments;
         int32_t spreadStart;

            {
            TR::VMAccessCriticalSection invokeSpreadHandle(fej9);
            methodHandle = *thunkDetails->getHandleRef();
            arguments = fej9->getReferenceField(fej9->methodHandle_type(methodHandle), "arguments", "[Ljava/lang/Class;");
            numArguments = (int32_t)fej9->getArrayLengthInElements(arguments);
            next         = fej9->getReferenceField(methodHandle, "next", "Ljava/lang/invoke/MethodHandle;");
            nextArguments = fej9->getReferenceField(fej9->methodHandle_type(next), "arguments", "[Ljava/lang/Class;");
            numNextArguments = (int32_t)fej9->getArrayLengthInElements(nextArguments);
            // Guard to protect old code
            if (symRef->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod() == TR::java_lang_invoke_SpreadHandle_spreadStart ||
                symRef->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod() == TR::java_lang_invoke_SpreadHandle_numArgsAfterSpreadArray)
               spreadStart = fej9->getInt32Field(methodHandle, "spreadPosition");
            }

         switch (symRef->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod())
            {
            case TR::java_lang_invoke_SpreadHandle_numArgsToPassThrough:
               loadConstant(TR::iconst, numArguments-1);
               break;
            case TR::java_lang_invoke_SpreadHandle_numArgsToSpread:
               loadConstant(TR::iconst, numNextArguments - (numArguments-1));
               break;
            case TR::java_lang_invoke_SpreadHandle_spreadStart:
               loadConstant(TR::iconst, spreadStart);
               break;
            case TR::java_lang_invoke_SpreadHandle_numArgsAfterSpreadArray:
               loadConstant(TR::iconst, numArguments - 1 - spreadStart);
               break;
            default:
            	break;
            }
         return true;
         }
      case TR::java_lang_invoke_FoldHandle_argIndices:
         {
         TR_ASSERT(archetypeParmCount == 0, "assertion failure"); // The number of arguments for argIndices()
         J9::MethodHandleThunkDetails *thunkDetails = getMethodHandleThunkDetails(this, comp(), symRef);
         if (!thunkDetails)
            return false;

         uintptrj_t methodHandle;
         uintptrj_t argIndices;
            {
            TR::VMAccessCriticalSection invokeFoldHandle(fej9);
            methodHandle = *thunkDetails->getHandleRef();
            argIndices = fej9->getReferenceField(methodHandle, "argumentIndices", "[I");
            int32_t arrayLength = (int32_t)fej9->getArrayLengthInElements(argIndices);
            int32_t foldPosition = fej9->getInt32Field(methodHandle, "foldPosition");
            if (arrayLength != 0)
               {
               // Push the indices in reverse order
               for (int i = arrayLength-1; i>=0; i--)
                  {
                  int32_t index = fej9->getInt32Element(argIndices, i);
                  // Argument index from user is relative to arguments for target handle
                  // Convert it to be relative to arguments of the resulting FoldHandle (i.e. argPlaceholder)
                  if (index > foldPosition)
                     index = index - 1;
                  loadConstant(TR::iconst, index);
                  }
               loadConstant(TR::iconst, arrayLength); // number of arguments
               }
            else
               {
               uintptrj_t combiner         = fej9->getReferenceField(methodHandle, "combiner", "Ljava/lang/invoke/MethodHandle;");
               uintptrj_t combinerArguments = fej9->getReferenceField(fej9->methodHandle_type(combiner), "arguments", "[Ljava/lang/Class;");
               int32_t numArgs = (int32_t)fej9->getArrayLengthInElements(combinerArguments);
               // Push the indices in reverse order
               for (int i=foldPosition+numArgs-1; i>=foldPosition; i--)
                   loadConstant(TR::iconst, i);
               loadConstant(TR::iconst, numArgs); // number of arguments
               }
            }
         return true;
         }
      case TR::java_lang_invoke_FoldHandle_foldPosition:
         {
         TR_ASSERT(archetypeParmCount == 0, "assertion failure"); // The number of arguments for foldPosition()

         J9::MethodHandleThunkDetails *thunkDetails = getMethodHandleThunkDetails(this, comp(), symRef);
         if (!thunkDetails)
            return false;

         uintptrj_t methodHandle;
         int32_t foldPosition;
            {
            TR::VMAccessCriticalSection invokeFoldHandle(fej9);
            methodHandle = *thunkDetails->getHandleRef();
            foldPosition = fej9->getInt32Field(methodHandle, "foldPosition");
            }

         loadConstant(TR::iconst, foldPosition);
         return true;
         }
      case TR::java_lang_invoke_FoldHandle_argumentsForCombiner:
         {
         TR::Node *placeholder = genNodeAndPopChildren(TR::icall, 1, placeholderWithDummySignature());

         char *placeholderSignature = placeholder->getSymbol()->castToMethodSymbol()->getMethod()->signatureChars();
         // The top of the stack should be the number of arguments
         int32_t numCombinerArgs = pop()->getInt();
         // Preserve the arguments of placeholder call so that their ref counts don't temporarily get to 0
         TR::Node *dummyAnchor = TR::Node::create(TR::icall, placeholder->getNumChildren());
         TR::Node *dummyTreeTopNode = TR::Node::create(TR::treetop, 1, dummyAnchor);
         for (int i=0; i<placeholder->getNumChildren(); i++)
             {
             dummyAnchor->setAndIncChild(i,placeholder->getChild(i));
             placeholder->getAndDecChild(i);
             }

         char * newSignature = "()I";
         // The rest of the stack should be the arg positions if there are any
         for (int i=0; i<numCombinerArgs; i++)
             {
             int32_t index = pop()->getInt();
             placeholder->setAndIncChild(i, dummyAnchor->getChild(index));
             newSignature = artificialSignature(stackAlloc, "(.*.@)I",
                                            newSignature, 0,
                                            placeholderSignature, index);
             }
         placeholder->setNumChildren(numCombinerArgs);
         placeholder->setSymbolReference(symRefWithArtificialSignature(placeholder->getSymbolReference(), newSignature));
         dummyTreeTopNode->recursivelyDecReferenceCount();
         push(placeholder);

         return true;
         }
      case TR::java_lang_invoke_FinallyHandle_numFinallyTargetArgsToPassThrough:
         {
         TR_ASSERT(archetypeParmCount == 0, "assertion failure"); // The number of arguments for numFinallyTargetArgsToPassThrough()

         J9::MethodHandleThunkDetails *thunkDetails = getMethodHandleThunkDetails(this, comp(), symRef);
         if (!thunkDetails)
            return false;

         int32_t numArgsPassToFinallyTarget;
         char *methodDescriptor;
            {
            TR::VMAccessCriticalSection invokeFinallyHandle(fej9);
            uintptrj_t methodHandle = *thunkDetails->getHandleRef();
            uintptrj_t finallyTarget = fej9->getReferenceField(methodHandle, "finallyTarget", "Ljava/lang/invoke/MethodHandle;");
            uintptrj_t finallyType = fej9->getReferenceField(finallyTarget, "type", "Ljava/lang/invoke/MethodType;");
            uintptrj_t arguments        = fej9->getReferenceField(finallyType, "arguments", "[Ljava/lang/Class;");
            numArgsPassToFinallyTarget = (int32_t)fej9->getArrayLengthInElements(arguments);

            uintptrj_t methodDescriptorRef = fej9->getReferenceField(finallyType, "methodDescriptor", "Ljava/lang/String;");
            int methodDescriptorLength = fej9->getStringUTF8Length(methodDescriptorRef);
            methodDescriptor = (char*)alloca(methodDescriptorLength+1);
            fej9->getStringUTF8(methodDescriptorRef, methodDescriptor, methodDescriptorLength+1);
            }

         char *returnType = strchr(methodDescriptor, ')') + 1;
         if (returnType[0] == 'V')
            numArgsPassToFinallyTarget -= 1;
         else
            numArgsPassToFinallyTarget -= 2;

         loadConstant(TR::iconst, numArgsPassToFinallyTarget);
         return true;
         }

      case TR::java_lang_invoke_FilterArgumentsHandle_numPrefixArgs:
      case TR::java_lang_invoke_FilterArgumentsHandle_numSuffixArgs:
      case TR::java_lang_invoke_FilterArgumentsHandle_numArgsToFilter:
         {
         TR_ASSERT(archetypeParmCount == 0, "assertion failure");

         J9::MethodHandleThunkDetails *thunkDetails = getMethodHandleThunkDetails(this, comp(), symRef);
         if (!thunkDetails)
            return false;

         uintptrj_t methodHandle;
         uintptrj_t arguments;
         int32_t numArguments;
         int32_t startPos;
         uintptrj_t filters;
         int32_t numFilters;

            {
            TR::VMAccessCriticalSection invokeFilderArgumentsHandle(fej9);
            methodHandle = *thunkDetails->getHandleRef();
            arguments = fej9->getReferenceField(fej9->methodHandle_type(methodHandle), "arguments", "[Ljava/lang/Class;");
            numArguments = (int32_t)fej9->getArrayLengthInElements(arguments);
            startPos     = (int32_t)fej9->getInt32Field(methodHandle, "startPos");
            filters = fej9->getReferenceField(methodHandle, "filters", "[Ljava/lang/invoke/MethodHandle;");
            numFilters = fej9->getArrayLengthInElements(filters);
            }

         switch (symRef->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod())
            {
            case TR::java_lang_invoke_FilterArgumentsHandle_numPrefixArgs:
               loadConstant(TR::iconst, startPos);
               break;
            case TR::java_lang_invoke_FilterArgumentsHandle_numSuffixArgs:
               loadConstant(TR::iconst, numArguments - numFilters - startPos);
               break;
            case TR::java_lang_invoke_FilterArgumentsHandle_numArgsToFilter:
               loadConstant(TR::iconst, numFilters);
               break;
            default:
            	break;
            }
         return true;
         }
      case TR::java_lang_invoke_FilterArgumentsHandle_filterArguments:
         {
         TR_ASSERT(archetypeParmCount == 2, "assertion failure");

         J9::MethodHandleThunkDetails *thunkDetails = getMethodHandleThunkDetails(this, comp(), symRef);
         if (!thunkDetails)
            return false;

         // This is required beyond the scope of the stack memory region
         TR::Node *placeholder = NULL;
         char * newSignature = "()I";

         {
         TR::StackMemoryRegion stackMemoryRegion(*comp()->trMemory());

         int32_t startPos;
         TR::KnownObjectTable::Index *filterIndexList;
         char *nextSignature;

            {
            TR::VMAccessCriticalSection invokeFilterArgumentsHandle(fej9);
            uintptrj_t methodHandle = *thunkDetails->getHandleRef();
            uintptrj_t filters = fej9->getReferenceField(methodHandle, "filters", "[Ljava/lang/invoke/MethodHandle;");
            int32_t numFilters = fej9->getArrayLengthInElements(filters);
            filterIndexList = (TR::KnownObjectTable::Index *) comp()->trMemory()->allocateMemory(sizeof(TR::KnownObjectTable::Index) * numFilters, stackAlloc);
            TR::KnownObjectTable *knot = comp()->getOrCreateKnownObjectTable();
            for (int i = 0; i <numFilters; i++)
               {
               filterIndexList[i] = knot->getIndex(fej9->getReferenceElement(filters, i));
               }

            startPos = (int32_t)fej9->getInt32Field(methodHandle, "startPos");
            uintptrj_t methodDescriptorRef = fej9->getReferenceField(fej9->getReferenceField(fej9->getReferenceField(
               methodHandle,
               "next",             "Ljava/lang/invoke/MethodHandle;"),
               "type",             "Ljava/lang/invoke/MethodType;"),
               "methodDescriptor", "Ljava/lang/String;");
            intptrj_t methodDescriptorLength = fej9->getStringUTF8Length(methodDescriptorRef);
            nextSignature = (char*)alloca(methodDescriptorLength+1);
            fej9->getStringUTF8(methodDescriptorRef, nextSignature, methodDescriptorLength+1);
            }

         placeholder = genNodeAndPopChildren(TR::icall, 1, placeholderWithDummySignature());
         char *placeholderSignature = placeholder->getSymbol()->castToMethodSymbol()->getMethod()->signatureChars();
         TR::Node *filterArray = pop();

         if (comp()->getOption(TR_TraceILGen))
            {
            traceMsg(comp(), "  FilterArgumentsHandle.filterArgument:\n");
            traceMsg(comp(), "    filterArray node is %p\n", filterArray);
            traceMsg(comp(), "    placeholder children: %d sig: %.*s\n", placeholder->getNumChildren(), placeholder->getSymbol()->castToMethodSymbol()->getMethod()->signatureLength(), placeholderSignature);
            }

         int32_t firstFilteredArgIndex = 0;
         for (int32_t childIndex = firstFilteredArgIndex; childIndex < placeholder->getNumChildren(); childIndex++)
            {
            int32_t arrayIndex = childIndex - firstFilteredArgIndex;
            int32_t argumentIndex = arrayIndex + startPos;
            if(filterIndexList[arrayIndex] != 0)
               {
               // First arg: receiver method handle comes from filterArray
               //
               push(filterArray);
               loadConstant(TR::iconst, arrayIndex);
               loadArrayElement(TR::Address);

               // genInvokeHandle wants another copy of the receiver at the top of the stack
               dup();
               TR::Node *receiverHandle = pop();
               if (receiverHandle->getOpCode().hasSymbolReference() && receiverHandle->getSymbolReference()->getSymbol()->isArrayShadowSymbol())
                  {
                  if (thunkDetails->isShareable())
                     {
                     // Can't set known object information for the filters.  That
                     // would have the effect of hard-coding the object identities
                     // into the jitted code, which would make it unshareable.
                     }
                  else
                     {
                     TR::SymbolReference *improvedSymbol = comp()->getSymRefTab()->findOrCreateSymRefWithKnownObject(receiverHandle->getSymbolReference(), filterIndexList[arrayIndex]);
                     receiverHandle->setSymbolReference(improvedSymbol);
                     }
                  }

               // Second arg: incoming unfiltered arg is a child of the placeholder call
               //
               push(placeholder->getChild(childIndex));

               // Manufacture an invokeExact symbol to convert from the
               // placeholder's argument type to the next Handle's argument type.
               //
               TR::SymbolReference *invokeExact = comp()->getSymRefTab()->methodSymRefFromName(_methodSymbol, JSR292_MethodHandle, JSR292_invokeExact, JSR292_invokeExactSig, TR::MethodSymbol::ComputedVirtual);
               TR::SymbolReference *invokeExactWithSig = symRefWithArtificialSignature(invokeExact,
                  "(.@).@",
                  placeholderSignature, childIndex,
                  nextSignature, argumentIndex
                  );

               push(receiverHandle);
               genInvokeHandle(invokeExactWithSig);

               // Replace placeholder child with the filtered child
               //
               placeholder->getAndDecChild(childIndex);
               placeholder->setAndIncChild(childIndex, pop());
               }

            // Add the filtered child's type to the placeholder's new signature
            //
            newSignature = artificialSignature(stackAlloc, "(.*.@)I",
               newSignature, 0,
               nextSignature, argumentIndex);
            }

         } // scope of the stack memory region

         // Edit placeholder signature to describe the argument types after filtering
         //
         placeholder->setSymbolReference(symRefWithArtificialSignature(placeholder->getSymbolReference(), newSignature));

         push(placeholder);
         return true;
         }
      case TR::java_lang_invoke_CatchHandle_numCatchTargetArgsToPassThrough:
         {
         J9::MethodHandleThunkDetails *thunkDetails = getMethodHandleThunkDetails(this, comp(), symRef);
         if (!thunkDetails)
            return false;

         int32_t numCatchArguments;

            {
            TR::VMAccessCriticalSection invokeCatchHandle(fej9);
            uintptrj_t methodHandle   = *thunkDetails->getHandleRef();
            uintptrj_t catchTarget    = fej9->getReferenceField(methodHandle, "catchTarget", "Ljava/lang/invoke/MethodHandle;");
            uintptrj_t catchArguments = fej9->getReferenceField(fej9->getReferenceField(
               catchTarget,
               "type", "Ljava/lang/invoke/MethodType;"),
               "arguments", "[Ljava/lang/Class;");
            numCatchArguments = (int32_t)fej9->getArrayLengthInElements(catchArguments);
            }

         loadConstant(TR::iconst, numCatchArguments-1); // First arg is the exception object
         return true;
         }
      case TR::java_lang_invoke_ILGenMacros_parameterCount:
         {
         J9::MethodHandleThunkDetails *thunkDetails = getMethodHandleThunkDetails(this, comp(), symRef);
         if (!thunkDetails)
            return false;

         int32_t parameterCount;

            {
            TR::VMAccessCriticalSection invokeILGenMacros(fej9);
            uintptrj_t receiverHandle   = *thunkDetails->getHandleRef();
            uintptrj_t methodHandle     = walkReferenceChain(pop(), receiverHandle);
            uintptrj_t arguments        = fej9->getReferenceField(fej9->getReferenceField(
               methodHandle,
               "type", "Ljava/lang/invoke/MethodType;"),
               "arguments", "[Ljava/lang/Class;");
            parameterCount = (int32_t)fej9->getArrayLengthInElements(arguments);
            }

         loadConstant(TR::iconst, parameterCount);
         return true;
         }
      case TR::java_lang_invoke_ILGenMacros_arrayLength:
         {
         J9::MethodHandleThunkDetails *thunkDetails = getMethodHandleThunkDetails(this, comp(), symRef);
         if (!thunkDetails)
            return false;

         int32_t arrayLength;

            {
            TR::VMAccessCriticalSection invokeILGenMacros(fej9);
            uintptrj_t receiverHandle   = *thunkDetails->getHandleRef();
            uintptrj_t array            = walkReferenceChain(pop(), receiverHandle);
            arrayLength = (int32_t)fej9->getArrayLengthInElements(array);
            }

         loadConstant(TR::iconst, arrayLength);
         return true;
         }
      case TR::java_lang_invoke_ILGenMacros_getField:
         {
         J9::MethodHandleThunkDetails *thunkDetails = getMethodHandleThunkDetails(this, comp(), symRef);
         if (!thunkDetails)
            return false;

         TR::Node *fieldLoad = pop();
         TR_ASSERT(fieldLoad->getOpCode().isLoadIndirect(), "Unexpected opcode for ILGenMacros.getField: %s", fieldLoad->getOpCode().getName());
         TR::Node *baseObjectNode = fieldLoad->getFirstChild();
         TR::SymbolReference *fieldSymRef = fieldLoad->getSymbolReference();
         TR::Symbol *fieldSym = fieldSymRef->getSymbol();
         TR_ASSERT(fieldSym->isShadow() && fieldSymRef->getCPIndex() > 0, "ILGenMacros.getField expecting field load; found load of %s", comp()->getDebug()->getName(symRef));
         uintptrj_t fieldOffset = fieldSymRef->getOffset() - sizeof(J9Object); // blah

         int32_t result;

            {
            TR::VMAccessCriticalSection invokeILGenMacros(fej9);
            uintptrj_t receiverHandle   = *thunkDetails->getHandleRef();
            uintptrj_t baseObject       = walkReferenceChain(baseObjectNode, receiverHandle);
            TR_ASSERT(fieldSym->getDataType() == TR::Int32, "ILGenMacros.getField expecting int field; found load of %s", comp()->getDebug()->getName(symRef));
            result = fej9->getInt32FieldAt(baseObject, fieldOffset); // TODO: Handle types other than int32
            }

         loadConstant(TR::iconst, result);
         return true;
         }
      case TR::java_lang_invoke_ILGenMacros_isCustomThunk:
      case TR::java_lang_invoke_ILGenMacros_isShareableThunk:
         {
         J9::MethodHandleThunkDetails *thunkDetails = getMethodHandleThunkDetails(this, comp(), symRef);
         if (!thunkDetails)
            return false;

         bool actuallyCustom = thunkDetails->isCustom();
         bool queryingCustom = (symRef->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod() == TR::java_lang_invoke_ILGenMacros_isCustomThunk);
         loadConstant(TR::iconst, actuallyCustom == queryingCustom);
         return true;
         }
      default:
         return false;
      }
   return false;
   }

uintptrj_t
TR_J9ByteCodeIlGenerator::walkReferenceChain(TR::Node *node, uintptrj_t receiver)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp()->fe());
   uintptrj_t result = 0;
   switch (node->getOpCodeValue())
      {
      case TR::aload:
         TR_ASSERT(node->getSymbolReference()->getCPIndex() == 0, "walkReferenceChain expecting aload of 'this'; found aload of %s", comp()->getDebug()->getName(node->getSymbolReference()));
         result = receiver;
         break;
      case TR::aloadi:
         {
         TR::SymbolReference *symRef = node->getSymbolReference();
         if (symRef->isUnresolved())
            {
            if (comp()->getOption(TR_TraceILGen))
               traceMsg(comp(), "  walkReferenceChain hit unresolved symref %s; aborting\n", symRef->getName(comp()->getDebug()));
            comp()->failCompilation<TR::ILGenFailure>("Symbol reference is unresolved");
            }
         TR::Symbol *sym = symRef->getSymbol();
         TR_ASSERT(sym->isShadow() && symRef->getCPIndex() > 0, "walkReferenceChain expecting field load; found load of %s", comp()->getDebug()->getName(symRef));
         uintptrj_t fieldOffset = symRef->getOffset() - sizeof(J9Object); // blah
         result = fej9->getReferenceFieldAt(walkReferenceChain(node->getFirstChild(), receiver), fieldOffset);
         }
         break;
      default:
         TR_ASSERT(0, "Unexpected opcode in walkReferenceChain: %s", node->getOpCode().getName());
         comp()->failCompilation<TR::ILGenFailure>("Unexpected opcode in walkReferenceChain");
         break;
      }

   if (comp()->getOption(TR_TraceILGen))
      {
      TR_ASSERT(node->getOpCode().hasSymbolReference(), "Can't get here without a symref");
      traceMsg(comp(), "  walkReferenceChain(%s) = %p // %s\n",
         comp()->getDebug()->getName(node),
         (void*)result,
         comp()->getDebug()->getName(node->getSymbolReference()));
      }

   return result;
   }
