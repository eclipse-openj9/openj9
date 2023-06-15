/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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

#include "env/j9method.h"

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
#include "compile/Method.hpp"
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
#include "exceptions/AOTFailure.hpp"
#include "il/DataTypes.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "infra/SimpleRegex.hpp"
#include "optimizer/Inliner.hpp"
#include "optimizer/PreExistence.hpp"
#include "runtime/J9Runtime.hpp"
#include "runtime/MethodMetaData.h"
#include "runtime/RelocationRuntime.hpp"
#include "runtime/RuntimeAssumptions.hpp"
#include "env/VMJ9.h"
#include "control/CompilationRuntime.hpp"
#include "control/CompilationThread.hpp"
#include "ilgen/J9ByteCodeIlGenerator.hpp"
#include "runtime/IProfiler.hpp"
#include "ras/DebugCounter.hpp"
#include "env/JSR292Methods.h"
#include "control/MethodToBeCompiled.hpp"


#if defined(_MSC_VER)
#include <malloc.h>
#endif

#define J9VMBYTECODES


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


#if defined(DEBUG_LOCAL_CLASS_OPT)
int fieldAttributeCtr = 0;
int staticAttributeCtr = 0;
int resolvedCtr = 0;
int unresolvedCtr = 0;
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

UDATA getFieldType(J9ROMConstantPoolItem * cp, I_32 cpIndex)
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

TR::Method *
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
TR_J9VMBase::createResolvedMethodWithVTableSlot(TR_Memory * trMemory, uint32_t vTableSlot, TR_OpaqueMethodBlock * aMethod,
                                  TR_ResolvedMethod * owningMethod, TR_OpaqueClassBlock *classForNewInstance)
   {
   return createResolvedMethodWithSignature(trMemory, aMethod, classForNewInstance, NULL, -1, owningMethod, vTableSlot);
   }

TR_ResolvedMethod *
TR_J9VMBase::createResolvedMethodWithSignature(TR_Memory * trMemory, TR_OpaqueMethodBlock * aMethod, TR_OpaqueClassBlock *classForNewInstance,
                          char *signature, int32_t signatureLength, TR_ResolvedMethod * owningMethod, uint32_t vTableSlot)
   {
   TR_ResolvedJ9Method *result = NULL;
   if (isAOT_DEPRECATED_DO_NOT_USE())
      {
#if defined(J9VM_INTERP_AOT_COMPILE_SUPPORT)
#if defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
      if (TR::Options::sharedClassCache())
         {
         result = new (trMemory->trHeapMemory()) TR_ResolvedRelocatableJ9Method(aMethod, this, trMemory, owningMethod, vTableSlot);
         TR::Compilation *comp = TR::comp();
         if (comp && comp->getOption(TR_UseSymbolValidationManager))
            {
            TR::SymbolValidationManager *svm = comp->getSymbolValidationManager();
            if (!svm->isAlreadyValidated(result->containingClass()))
               return NULL;
            }
         }
      else
#endif
         return NULL;
#endif
      }
   else
      {
      result = new (trMemory->trHeapMemory()) TR_ResolvedJ9Method(aMethod, this, trMemory, owningMethod, vTableSlot);
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

bool TR_ResolvedJ9Method::isMethodInValidLibrary()
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)_fe;
   if (fej9->isClassLibraryMethod(getPersistentIdentifier(), true))
      return true;

   // this is a work-around because DAA library is not part of class library yet.
   // TODO: to be cleaned up after DAA library packaged into class library
   if (!strncmp(this->convertToMethod()->classNameChars(), "com/ibm/dataaccess/", 19))
      return true;
   // For WebSphere methods
   if (!strncmp(this->convertToMethod()->classNameChars(), "com/ibm/ws/", 11))
      return true;
   if (!strncmp(this->convertToMethod()->classNameChars(), "com/ibm/gpu/Kernel", 18))
      return true;

   if (!strncmp(this->convertToMethod()->classNameChars(), "jdk/incubator/vector/", 21))
      return true;

#ifdef J9VM_OPT_JAVA_CRYPTO_ACCELERATION
   // For IBMJCE Crypto
   if (!strncmp(this->convertToMethod()->classNameChars(), "com/ibm/jit/Crypto", 18))
      return true;
   if (!strncmp(this->convertToMethod()->classNameChars(), "com/ibm/jit/crypto/JITFullHardwareCrypt", 39))
      return true;
   if (!strncmp(this->convertToMethod()->classNameChars(), "com/ibm/jit/crypto/JITFullHardwareDigest", 40))
      return true;
   if (!strncmp(this->convertToMethod()->classNameChars(), "com/ibm/crypto/provider/P224PrimeField", 38))
      return true;
   if (!strncmp(this->convertToMethod()->classNameChars(), "com/ibm/crypto/provider/P256PrimeField", 38))
      return true;
   if (!strncmp(this->convertToMethod()->classNameChars(), "com/ibm/crypto/provider/P384PrimeField", 38))
      return true;
   if (!strncmp(this->convertToMethod()->classNameChars(), "com/ibm/crypto/provider/AESCryptInHardware", 42))
      return true;
   if (!strncmp(this->convertToMethod()->classNameChars(), "com/ibm/crypto/provider/ByteArrayMarshaller", 43))
      return true;
   if (!strncmp(this->convertToMethod()->classNameChars(), "com/ibm/crypto/provider/ByteArrayUnmarshaller", 45))
      return true;
#endif

   return false;
   }

void
TR_J9MethodBase::parseSignature(TR_Memory * trMemory)
   {
   U_8 tempArgTypes[512];
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
TR_ResolvedJ9Method::owningMethodDoesntMatter()
   {

   // Returning true here allows us to ignore the owning method, which lets us
   // share symrefs more aggressively and other goodies, but usually ignoring
   // the owning method will confuse inliner and others, so only do so when
   // it's known not to matter.

   static char *aggressiveJSR292Opts = feGetEnv("TR_aggressiveJSR292Opts");
   J9UTF8 *className = J9ROMCLASS_CLASSNAME(romClassPtr());
   if (aggressiveJSR292Opts && strchr(aggressiveJSR292Opts, '3'))
      {
      if (J9UTF8_LENGTH(className) >= 17 && !strncmp((char*)J9UTF8_DATA(className), "java/lang/invoke/", 17))
         {
         return true;
         }
      else switch (TR_ResolvedJ9MethodBase::getRecognizedMethod())
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
                                                                 J9_LOOK_NO_JAVA);
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
   "java/security/AccessController.doPrivileged(Ljava/security/PrivilegedExceptionAction;Ljava/security/AccessControlContext;)Ljava/lang/Object;",
   "java/security/AccessController.doPrivileged(Ljava/security/PrivilegedAction;Ljava/security/AccessControlContext;[Ljava/security/Permission;)Ljava/lang/Object;",
   "java/security/AccessController.doPrivileged(Ljava/security/PrivilegedExceptionAction;Ljava/security/AccessControlContext;[Ljava/security/Permission;)Ljava/lang/Object;",
   "java/lang/NullPointerException.fillInStackTrace()Ljava/lang/Throwable;",
#if (17 <= JAVA_SPEC_VERSION) && (JAVA_SPEC_VERSION <= 18)
   "jdk/internal/loader/NativeLibraries.load(Ljdk/internal/loader/NativeLibraries$NativeLibraryImpl;Ljava/lang/String;ZZZ)Z",
#elif JAVA_SPEC_VERSION >= 15 /* (17 <= JAVA_SPEC_VERSION) && (JAVA_SPEC_VERSION <= 18) */
   "jdk/internal/loader/NativeLibraries.load(Ljdk/internal/loader/NativeLibraries$NativeLibraryImpl;Ljava/lang/String;ZZ)Z",
#endif /* (17 <= JAVA_SPEC_VERSION) && (JAVA_SPEC_VERSION <= 18) */
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



static intptr_t getInitialCountForMethod(TR_ResolvedMethod *rm, TR::Compilation *comp)
   {
   TR_ResolvedJ9Method *m = static_cast<TR_ResolvedJ9Method *>(rm);
   TR::Options * options = comp->getOptions();

   intptr_t initialCount = m->hasBackwardBranches() ? options->getInitialBCount() : options->getInitialCount();

#if defined(J9VM_INTERP_AOT_COMPILE_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
   if (TR::Options::sharedClassCache())
      {
      J9ROMClass *romClass = m->romClassPtr();
      J9ROMMethod *romMethod = m->romMethod();

      if (!comp->fej9()->sharedCache()->isROMClassInSharedCache(romClass))
         {
#if defined(J9ZOS390)
          // Do not change the counts on zos at the moment since the shared cache capacity is higher on this platform
          // and by increasing counts we could end up significantly impacting startup
#else
          bool startupTimeMatters = TR::Options::isQuickstartDetected() || comp->getOption(TR_UseLowerMethodCounts);

          if (!startupTimeMatters)
             {
             bool useHigherMethodCounts = false;

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
                J9UTF8 * className = J9ROMCLASS_CLASSNAME(romClass);

                if (J9UTF8_LENGTH(className) > 5 &&
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
   // We do this for direct calls and currently not-overridden virtual calls
   // For overridden virtual calls we may decide at some point to traverse all the
   // existing targets to see if they are all interpreted with high counts
   //
   if (!isInterpretedForHeuristics() || maxBytecodeIndex() <= TRIVIAL_INLINER_MAX_SIZE)
      return false;

   if (isIndirectCall && virtualMethodIsOverridden())
      {
      // Method is polymorphic virtual - walk all the targets to see
      // if all of them are not reached
      // FIXME: implement
      return false;
      }

   TR::RecognizedMethod rm = getRecognizedMethod();
   switch (rm)
      {
      case TR::java_math_BigDecimal_noLLOverflowMul:
      case TR::java_math_BigDecimal_noLLOverflowAdd:
      case TR::com_ibm_jit_DecimalFormatHelper_formatAsDouble:
      case TR::com_ibm_jit_DecimalFormatHelper_formatAsFloat:
      case TR::java_lang_invoke_MethodHandle_invokeExact:
      return false;

      default:
         break;
      }

   if (convertToMethod()->isArchetypeSpecimen())
      return false;

   intptr_t count = getInvocationCount();

   intptr_t initialCount = getInitialCountForMethod(this, comp);

   if (count < 0 || count > initialCount)
      return false;

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

   // Do this so that all intermediate calls to c->getSymRefTab()
   // in codegen.dev go to the new symRefTab
   //
   c->setCurrentSymRefTab(newSymRefTab);

   newSymRefTab->addParameters(methodSymbol);

   TR_ByteCodeInfo bci;

   //incInlineDepth is a part of a hack to make InvariantArgumentPreexistence
   //play nicely if getCurrentInlinedCallArgInfo is provided while peeking.
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
         if (comp->getOption(TR_UseSymbolValidationManager))
            {
            TR::SymbolValidationManager *svm = comp->getSymbolValidationManager();
            SVM_ASSERT_ALREADY_VALIDATED(svm, aMethod);
            SVM_ASSERT_ALREADY_VALIDATED(svm, containingClass());
            }
         else if (owner)
            {
            // JITServer: in baseline, if owner doesn't exist then comp doesn't exist, so thi case is not possible
            // but in JITClient comp is initialized before creating resolved method for compilee, so need this guard.
            ((TR_ResolvedRelocatableJ9Method *) owner)->validateArbitraryClass(comp, (J9Class*)containingClass());
            }
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
            *(U_32*)fieldOffset = (U_32) TR::Compiler->om.objectHeaderSizeInBytes();
         }
      }

   *type = decodeType(ltype);
   }


TR::Method *   TR_ResolvedRelocatableJ9Method::convertToMethod()              { return this; }

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
   TR::Compilation *comp = TR::comp();
   if (comp && comp->compileRelocatableCode() && comp->getOption(TR_UseSymbolValidationManager))
      alwaysTreatAsInterpreted = true;
   else
      alwaysTreatAsInterpreted = false;
#elif defined(TR_TARGET_X86)

   /*if isInterpreted should be only overridden for JNI methods.
   Otherwise buildDirectCall in X86PrivateLinkage.cpp will generate CALL 0
   for certain jitted methods as startAddressForJittedMethod still returns NULL in AOT
   this is not an issue on z as direct calls are dispatched via snippets.
   For Z under AOT while using SVM, we do not need to treat all calls unresolved as it was case
   for oldAOT. So alwaysTreatAsInterpreted is set to true in AOT using SVM so that call snippet
   can be generated using correct j9method address.

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

bool
TR_ResolvedRelocatableJ9Method::isInterpretedForHeuristics()
   {
   return TR_ResolvedJ9Method::isInterpreted();
   }

bool
TR_ResolvedRelocatableJ9Method::isCompilable(TR_Memory * trMemory)
   {
   TR::CompilationInfo *compInfo = TR::CompilationInfo::get(fej9()->_jitConfig);

   if (compInfo->isMethodIneligibleForAot(ramMethod()))
      return false;

   return TR_ResolvedJ9Method::isCompilable(trMemory);
   }

void *
TR_ResolvedRelocatableJ9Method::startAddressForJittedMethod()
   {
   return NULL;
   }

void *
TR_ResolvedRelocatableJ9Method::startAddressForJNIMethod(TR::Compilation * comp)
   {
#if defined(TR_TARGET_S390)  || defined(TR_TARGET_X86) || defined(TR_TARGET_POWER) || defined(TR_TARGET_ARM64)
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

   if (returnClassForAOT || comp->getOption(TR_UseSymbolValidationManager))
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
   if (comp->getOption(TR_UseSymbolValidationManager))
      {
      return comp->getSymbolValidationManager()->addClassFromCPRecord(reinterpret_cast<TR_OpaqueClassBlock *>(clazz), cp(), cpIndex);
      }
   else
      {
      return storeValidationRecordIfNecessary(comp, cp(), cpIndex, reloKind, ramMethod(), clazz);
      }
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
      case J9CPTYPE_CONSTANT_DYNAMIC:
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

TR_ResolvedMethod *
TR_ResolvedRelocatableJ9Method::getResolvedPossiblyPrivateVirtualMethod(
   TR::Compilation *comp,
   int32_t cpIndex,
   bool ignoreRtResolve,
   bool * unresolvedInCP)
   {
   TR_ResolvedMethod *method =
      TR_ResolvedJ9Method::getResolvedPossiblyPrivateVirtualMethod(
         comp,
         cpIndex,
         ignoreRtResolve,
         unresolvedInCP);

   return aotMaskResolvedPossiblyPrivateVirtualMethod(comp, method);
   }

TR_ResolvedMethod *
TR_ResolvedJ9Method::aotMaskResolvedPossiblyPrivateVirtualMethod(
   TR::Compilation *comp, TR_ResolvedMethod *method)
   {
   if (method == NULL
       || !method->isPrivate()
       || comp->fej9()->isResolvedDirectDispatchGuaranteed(comp))
      return method;

   // Leave private invokevirtual unresolved. If we resolve it, we may not
   // necessarily have resolved dispatch in codegen, and the generated code
   // could attempt to resolve the wrong kind of constant pool entry.
   return NULL;
   }

bool
TR_ResolvedRelocatableJ9Method::getUnresolvedFieldInCP(I_32 cpIndex)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
   #if defined(J9VM_INTERP_AOT_COMPILE_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
      return !J9RAMFIELDREF_IS_RESOLVED(((J9RAMFieldRef*)cp()) + cpIndex);
   #else
      return true;
   #endif
   }

bool
TR_ResolvedRelocatableJ9Method::storeValidationRecordIfNecessary(TR::Compilation * comp, J9ConstantPool *constantPool, int32_t cpIndex, TR_ExternalRelocationTargetKind reloKind, J9Method *ramMethod, J9Class *definingClass)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) comp->fe();

   bool storeClassInfo = true;
   bool fieldInfoCanBeUsed = false;
   TR_AOTStats *aotStats = ((TR_JitPrivateConfig *)fej9->_jitConfig->privateConfig)->aotStats;
   bool isStatic = false;

   TR::CompilationInfo *compInfo = TR::CompilationInfo::get(fej9->_jitConfig);

   isStatic = (reloKind == TR_ValidateStaticField);

   traceMsg(comp, "storeValidationRecordIfNecessary:\n");
   traceMsg(comp, "\tconstantPool %p cpIndex %d\n", constantPool, cpIndex);
   traceMsg(comp, "\treloKind %d isStatic %d\n", reloKind, isStatic);
   J9UTF8 *methodClassName = J9ROMCLASS_CLASSNAME(J9_CLASS_FROM_METHOD(ramMethod)->romClass);
   traceMsg(comp, "\tmethod %p from class %p %.*s\n", ramMethod, J9_CLASS_FROM_METHOD(ramMethod), J9UTF8_LENGTH(methodClassName), J9UTF8_DATA(methodClassName));
   traceMsg(comp, "\tdefiningClass %p\n", definingClass);

   if (!definingClass)
      {
      definingClass = (J9Class *) TR_ResolvedJ9Method::definingClassFromCPFieldRef(comp, constantPool, cpIndex, isStatic);
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
               inLocalList = (definingClass->romClass == ((J9Class *)((*info)->_clazz))->romClass);
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
         {
         if (needAOTValidation)
            {
            if (comp->getOption(TR_UseSymbolValidationManager))
               {
               TR_OpaqueClassBlock *clazz = TR_ResolvedJ9Method::definingClassFromCPFieldRef(comp, constantPool, cpIndex, false);

               fieldInfoCanBeUsed = comp->getSymbolValidationManager()->addDefiningClassFromCPRecord(clazz, constantPool, cpIndex);
               }
            else
               {
               fieldInfoCanBeUsed = storeValidationRecordIfNecessary(comp, constantPool, cpIndex, TR_ValidateInstanceField, ramMethod());
               }
            }
         else
            {
            fieldInfoCanBeUsed = true;
            }
         }
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
      if (resolveField) *fieldOffset = offset + TR::Compiler->om.objectHeaderSizeInBytes();  // add header size
#if defined(DEBUG_LOCAL_CLASS_OPT)
      resolvedCtr++;
#endif
      }
   else
      {
#if defined(J9VM_INTERP_AOT_COMPILE_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
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

   if (needAOTValidation)
      {
      if (comp->getOption(TR_UseSymbolValidationManager))
         {
         TR_OpaqueClassBlock *clazz = TR_ResolvedJ9Method::definingClassFromCPFieldRef(comp, constantPool, cpIndex, true);

         fieldInfoCanBeUsed = comp->getSymbolValidationManager()->addDefiningClassFromCPRecord(clazz, constantPool, cpIndex, true);
         }
      else
         {
         fieldInfoCanBeUsed = storeValidationRecordIfNecessary(comp, constantPool, cpIndex, TR_ValidateStaticField, ramMethod());
         }
      }
   else
      {
      fieldInfoCanBeUsed = true;
      }

   if (offset == (void *)J9JIT_RESOLVE_FAIL_COMPILE)
      {
      comp->failCompilation<TR::CompilationException>("offset == J9JIT_RESOLVE_FAIL_COMPILE");
      }

   if (offset && fieldInfoCanBeUsed &&
      (!(_fe->_jitConfig->runtimeFlags & J9JIT_RUNTIME_RESOLVE) ||
      comp->ilGenRequest().details().isMethodHandleThunk() || // cmvc 195373
      !performTransformation(comp, "Setting as unresolved static attributes cpIndex=%d\n",cpIndex)))
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
#if defined(J9VM_INTERP_AOT_COMPILE_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
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

TR_OpaqueClassBlock *
TR_ResolvedRelocatableJ9Method::definingClassFromCPFieldRef(
   TR::Compilation *comp,
   I_32 cpIndex,
   bool isStatic,
   TR_OpaqueClassBlock** fromResolvedJ9Method)
   {
   TR_OpaqueClassBlock *clazz = TR_ResolvedJ9Method::definingClassFromCPFieldRef(comp, cp(), cpIndex, isStatic);
   if (fromResolvedJ9Method != NULL) {
      *fromResolvedJ9Method = clazz;
   }

   bool valid = false;
   if (comp->getOption(TR_UseSymbolValidationManager))
      valid = comp->getSymbolValidationManager()->addDefiningClassFromCPRecord(clazz , cp(), cpIndex, isStatic);
   else
      valid = storeValidationRecordIfNecessary(comp, cp(), cpIndex, isStatic ? TR_ValidateStaticField : TR_ValidateInstanceField, ramMethod());

   if (!valid)
      clazz = NULL;

   return clazz;
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
   TR_OpaqueClassBlock * clazz = TR_ResolvedJ9Method::classOfStatic(cpIndex, returnClassForAOT);

   TR::Compilation *comp = TR::comp();
   bool validated = false;

   if (comp && comp->getOption(TR_UseSymbolValidationManager))
      {
      validated = comp->getSymbolValidationManager()->addStaticClassFromCPRecord(clazz, cp(), cpIndex);
      }
   else
      {
      validated = returnClassForAOT;
      }

   if (validated)
      return clazz;
   else
      return NULL;
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
   else if (comp->ilGenRequest().details().isMethodHandleThunk()) // cmvc 195373
      return false;
   else if (fej9->getJ9JITConfig()->runtimeFlags & J9JIT_RUNTIME_RESOLVE)
      return performTransformation(comp, "Setting as unresolved static call cpIndex=%d\n", cpIndex);

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
TR_ResolvedRelocatableJ9Method::getResolvedImproperInterfaceMethod(
   TR::Compilation * comp,
   I_32 cpIndex)
   {
   return aotMaskResolvedImproperInterfaceMethod(
      comp, TR_ResolvedJ9Method::getResolvedImproperInterfaceMethod(comp, cpIndex));
   }

TR_ResolvedMethod *
TR_ResolvedJ9Method::aotMaskResolvedImproperInterfaceMethod(
   TR::Compilation *comp, TR_ResolvedMethod *method)
   {
   if (method == NULL)
      return NULL;

   bool resolvedDispatch = false;
   if (method->isPrivate() || method->convertToMethod()->isFinalInObject())
      resolvedDispatch = comp->fej9()->isResolvedDirectDispatchGuaranteed(comp);
   else
      resolvedDispatch = comp->fej9()->isResolvedVirtualDispatchGuaranteed(comp);

   if (resolvedDispatch)
      return method;

   // Leave this method unresolved. If we resolve it, we may not necessarily
   // have resolved dispatch in codegen, and the generated code could attempt
   // to resolve the wrong kind of constant pool entry.
   return NULL;
   }

TR_ResolvedMethod *
TR_ResolvedRelocatableJ9Method::createResolvedMethodFromJ9Method(TR::Compilation *comp, I_32 cpIndex, uint32_t vTableSlot, J9Method *j9method, bool * unresolvedInCP, TR_AOTInliningStats *aotStats)
   {
   TR_ResolvedMethod *resolvedMethod = NULL;

#if defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
   static char *dontInline = feGetEnv("TR_AOTDontInline");
   bool resolveAOTMethods = !comp->getOption(TR_DisableAOTResolveDiffCLMethods);
   bool enableAggressive = comp->getOption(TR_EnableAOTInlineSystemMethod);
   bool isSystemClassLoader = false;

   if (dontInline)
      return NULL;

      {
      // Check if same classloader
      J9Class *j9clazz = (J9Class *) J9_CLASS_FROM_CP(((J9RAMConstantPoolItem *) J9_CP_FROM_METHOD(((J9Method *)j9method))));
      TR_OpaqueClassBlock *clazzOfInlinedMethod = _fe->convertClassPtrToClassOffset(j9clazz);
      TR_OpaqueClassBlock *clazzOfCompiledMethod = _fe->convertClassPtrToClassOffset(J9_CLASS_FROM_METHOD(ramMethod()));

      if (enableAggressive)
         {
         isSystemClassLoader = ((void*)_fe->vmThread()->javaVM->systemClassLoader->classLoaderObject ==  (void*)_fe->getClassLoader(clazzOfInlinedMethod));
         }

      bool methodInSCC = _fe->sharedCache()->isROMClassInSharedCache(J9_CLASS_FROM_METHOD(j9method)->romClass);
      if (methodInSCC)
         {
         bool sameLoaders = false;
         TR_J9VMBase *fej9 = (TR_J9VMBase *)_fe;
         if (resolveAOTMethods ||
             (sameLoaders = fej9->sameClassLoaders(clazzOfInlinedMethod, clazzOfCompiledMethod)) ||
             isSystemClassLoader)
            {
            resolvedMethod = new (comp->trHeapMemory()) TR_ResolvedRelocatableJ9Method((TR_OpaqueMethodBlock *) j9method, _fe, comp->trMemory(), this, vTableSlot);
            if (comp->getOption(TR_UseSymbolValidationManager))
               {
               TR::SymbolValidationManager *svm = comp->getSymbolValidationManager();
               if (!svm->isAlreadyValidated(resolvedMethod->containingClass()))
                  {
                  return NULL;
                  }
               }
            else if (aotStats)
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
               !methodInSCC)
         {
         aotStats->numMethodROMMethodNotInSC++;
         }
      }

#endif

   if (resolvedMethod && ((TR_ResolvedJ9Method*)resolvedMethod)->isSignaturePolymorphicMethod())
      {
      // Signature polymorphic method's signature varies at different call sites and will be different than its declared signature
      J9ROMMethodRef *romMethodRef = (J9ROMMethodRef *)(cp()->romConstantPool + cpIndex);
      J9ROMNameAndSignature *nameAndSig = J9ROMMETHODREF_NAMEANDSIGNATURE(romMethodRef);
      int32_t signatureLength;
      char   *signature = utf8Data(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig), signatureLength);
      ((TR_ResolvedJ9Method *)resolvedMethod)->setSignature(signature, signatureLength, comp->trMemory());
      }

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
   eTbl = (J9JITExceptionTable *)_fe->allocateDataCacheRecord(numBytes, comp, _fe->needsContiguousCodeAndDataCacheAllocation(), &shouldRetryAllocation,
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

   // This exception table doesn't get initialized until relocation
   eTbl->flags |= JIT_METADATA_NOT_INITIALIZED;

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
   _name = J9ROMMETHOD_NAME(romMethod);
   _signature = J9ROMMETHOD_SIGNATURE(romMethod);

   parseSignature(trMemory);
   _fullSignature = NULL;
   }

#if defined(J9VM_OPT_JITSERVER)
// used by JITServer
TR_J9Method::TR_J9Method()
   {
   }
#endif /* defined(J9VM_OPT_JITSERVER) */

//////////////////////////////
//
//  TR_ResolvedMethod
//
/////////////////////////

static bool supportsFastJNI(TR_FrontEnd *fe)
   {
#if defined(TR_TARGET_S390) || defined(TR_TARGET_X86) || defined(TR_TARGET_POWER) || defined(TR_TARGET_ARM64)
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

      // check for user specified FastJNI override
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

   construct();
   }

#if defined(J9VM_OPT_JITSERVER)
// Protected constructor to be used by JITServer
// Had to reorder arguments to prevent ambiguity with above constructor
TR_ResolvedJ9Method::TR_ResolvedJ9Method(TR_FrontEnd * fe, TR_ResolvedMethod * owner)
   : TR_J9Method(), TR_ResolvedJ9MethodBase(fe, owner), _pendingPushSlots(-1)
   {
   }
#endif /* defined(J9VM_OPT_JITSERVER) */

void TR_ResolvedJ9Method::construct()
   {
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
      {x(TR::java_util_ArrayList_add,      "add",     "(Ljava/lang/Object;)Z")},
      {x(TR::java_util_ArrayList_ensureCapacity,      "ensureCapacity",      "(I)V")},
      {x(TR::java_util_ArrayList_get,      "get",     "(I)Ljava/lang/Object;")},
      {x(TR::java_util_ArrayList_remove,   "remove",  "(I)Ljava/lang/Object;")},
      {x(TR::java_util_ArrayList_set,      "set",     "(ILjava/lang/Object;)Ljava/lang/Object;")},
      {  TR::unknownMethod}
      };

   static X ClassMethods[] =
      {
      {x(TR::java_lang_Class_newInstancePrototype, "newInstancePrototype", "(Ljava/lang/Class;)Ljava/lang/Object;")},
      //{x(TR::java_lang_Class_newInstanceImpl,      "newInstanceImpl",      "()Ljava/lang/Object;")},
      {x(TR::java_lang_Class_newInstance,          "newInstance",          "()Ljava/lang/Object;")},
      {x(TR::java_lang_Class_isArray,              "isArray",              "()Z")},
      {x(TR::java_lang_Class_isPrimitive,          "isPrimitive",          "()Z")},
      {x(TR::java_lang_Class_isValue,              "isValue",              "()Z")},
      {x(TR::java_lang_Class_isPrimitiveClass,     "isPrimitiveClass",     "()Z")},
      {x(TR::java_lang_Class_isIdentity,           "isIdentity",           "()Z")},
      {x(TR::java_lang_Class_getComponentType,     "getComponentType",     "()Ljava/lang/Class;")},
      {x(TR::java_lang_Class_getModifiersImpl,     "getModifiersImpl",     "()I")},
      {x(TR::java_lang_Class_isAssignableFrom,     "isAssignableFrom",     "(Ljava/lang/Class;)Z")},
      {x(TR::java_lang_Class_isInstance,           "isInstance",           "(Ljava/lang/Object;)Z")},
      {x(TR::java_lang_Class_isInterface,          "isInterface",          "()Z")},
      {x(TR::java_lang_Class_cast,                 "cast",                 "(Ljava/lang/Object;)Ljava/lang/Object;")},
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
      {x(TR::java_util_HashMap_prepareArray,          "prepareArray",   "([Ljava/lang/Object;)[Ljava/lang/Object;")},
      {x(TR::java_util_HashMap_keysToArray,           "keysToArray",    "([Ljava/lang/Object;)[Ljava/lang/Object;")},
      {x(TR::java_util_HashMap_valuesToArray,         "valuesToArray",  "([Ljava/lang/Object;)[Ljava/lang/Object;")},
      {  TR::unknownMethod}
      };

   static X HashMapHashIteratorMethods[] =
      {
      {x(TR::java_util_HashMapHashIterator_nextNode,   "nextNode",      "()Ljava/util/HashMap$Node;")},
      {x(TR::java_util_HashMapHashIterator_init,       "<init>",            "(Ljava/util/HashMap;)V")},
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
      {x(TR::java_nio_Bits_byteOrder,                  "byteOrder",                  "()Ljava/nio/ByteOrder;")},
      {x(TR::java_nio_Bits_getCharB,                   "getCharB",                   "(Ljava/nio/ByteBuffer;I)C")},
      {x(TR::java_nio_Bits_getCharL,                   "getCharL",                   "(Ljava/nio/ByteBuffer;I)C")},
      {x(TR::java_nio_Bits_getShortB,                  "getShortB",                  "(Ljava/nio/ByteBuffer;I)S")},
      {x(TR::java_nio_Bits_getShortL,                  "getShortL",                  "(Ljava/nio/ByteBuffer;I)S")},
      {x(TR::java_nio_Bits_getIntB,                    "getIntB",                    "(Ljava/nio/ByteBuffer;I)I")},
      {x(TR::java_nio_Bits_getIntL,                    "getIntL",                    "(Ljava/nio/ByteBuffer;I)I")},
      {x(TR::java_nio_Bits_getLongB,                   "getLongB",                   "(Ljava/nio/ByteBuffer;I)J")},
      {x(TR::java_nio_Bits_getLongL,                   "getLongL",                   "(Ljava/nio/ByteBuffer;I)J")},
      {  TR::unknownMethod}
      };

   static X HeapByteBufferMethods[] =
      {
      {x(TR::java_nio_HeapByteBuffer__get,            "_get",                       "(I)B")},
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
      {x(TR::java_lang_Math_fma_D,            "fma",            "(DDD)D")},
      {x(TR::java_lang_Math_fma_F,            "fma",            "(FFF)F")},
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
      {x(TR::java_lang_StrictMath_fma_D,      "fma",            "(DDD)D")},
      {x(TR::java_lang_StrictMath_fma_F,      "fma",            "(FFF)F")},
      {  TR::unknownMethod}
      };

   static X ObjectMethods[] =
      {
      {TR::java_lang_Object_init,                 6,    "<init>", (int16_t)-1,    "*"},
      {x(TR::java_lang_Object_getClass,             "getClass",             "()Ljava/lang/Class;")},
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
      {x(TR::java_lang_String_encodeASCII,         "encodeASCII",        "(B[B)[B")},
      {x(TR::java_lang_String_equals,              "equals",              "(Ljava/lang/Object;)Z")},
      {x(TR::java_lang_String_indexOf_String,      "indexOf",             "(Ljava/lang/String;)I")},
      {x(TR::java_lang_String_indexOf_String_int,  "indexOf",             "(Ljava/lang/String;I)I")},
      {x(TR::java_lang_String_indexOf_char,        "indexOf",             "(I)I")},
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
      {x(TR::java_lang_String_toCharArray,         "toCharArray",         "()[C")},
      {x(TR::java_lang_String_regionMatches,       "regionMatches",       "(ILjava/lang/String;II)Z")},
      {x(TR::java_lang_String_regionMatches_bool,  "regionMatches",       "(ZILjava/lang/String;II)Z")},
      {  TR::java_lang_String_regionMatchesInternal, 21, "regionMatchesInternal", (int16_t)-1, "*"},
      {x(TR::java_lang_String_equalsIgnoreCase,    "equalsIgnoreCase",    "(Ljava/lang/String;)Z")},
      {x(TR::java_lang_String_compareToIgnoreCase, "compareToIgnoreCase", "(Ljava/lang/String;)I")},
      {x(TR::java_lang_String_compress,            "compress",            "([C[BII)I")},
      {x(TR::java_lang_String_compressNoCheck,     "compressNoCheck",     "([C[BII)V")},
      {x(TR::java_lang_String_andOR,               "andOR",               "([CII)I")},
      {x(TR::java_lang_String_unsafeCharAt,        "unsafeCharAt",        "(I)C")},
      {x(TR::java_lang_String_split_str_int,       "split",               "(Ljava/lang/String;I)[Ljava/lang/String;")},
      {x(TR::java_lang_String_getChars_charArray,  "getChars",            "(II[CI)V")},
      {x(TR::java_lang_String_getChars_byteArray,  "getChars",            "(II[BI)V")},
      {x(TR::java_lang_String_checkIndex,          "checkIndex",          "(II)V")},
      {x(TR::java_lang_String_coder,               "coder",               "()B")},
      {x(TR::java_lang_String_decodeUTF8_UTF16,    "decodeUTF8_UTF16",    "([BII[BIZ)I")},
      {x(TR::java_lang_String_isLatin1,            "isLatin1",            "()Z")},
      {x(TR::java_lang_String_startsWith,          "startsWith",          "(Ljava/lang/String;I)Z")},
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
      {x(TR::java_lang_StringCoding_encodeUTF8,         "encodeUTF8",         "(B[BZ)[B")},
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
      {x(TR::sun_misc_Unsafe_putObject_jlObjectJjlObject_V, "putReference",  "(Ljava/lang/Object;JLjava/lang/Object;)V")},

      {x(TR::sun_misc_Unsafe_putBooleanVolatile_jlObjectJZ_V,       "putBooleanVolatile", "(Ljava/lang/Object;JZ)V")},
      {x(TR::sun_misc_Unsafe_putByteVolatile_jlObjectJB_V,          "putByteVolatile",    "(Ljava/lang/Object;JB)V")},
      {x(TR::sun_misc_Unsafe_putCharVolatile_jlObjectJC_V,          "putCharVolatile",    "(Ljava/lang/Object;JC)V")},
      {x(TR::sun_misc_Unsafe_putShortVolatile_jlObjectJS_V,         "putShortVolatile",   "(Ljava/lang/Object;JS)V")},
      {x(TR::sun_misc_Unsafe_putIntVolatile_jlObjectJI_V,           "putIntVolatile",     "(Ljava/lang/Object;JI)V")},
      {x(TR::sun_misc_Unsafe_putLongVolatile_jlObjectJJ_V,          "putLongVolatile",    "(Ljava/lang/Object;JJ)V")},
      {x(TR::sun_misc_Unsafe_putFloatVolatile_jlObjectJF_V,         "putFloatVolatile",   "(Ljava/lang/Object;JF)V")},
      {x(TR::sun_misc_Unsafe_putDoubleVolatile_jlObjectJD_V,        "putDoubleVolatile",  "(Ljava/lang/Object;JD)V")},
      {x(TR::sun_misc_Unsafe_putObjectVolatile_jlObjectJjlObject_V, "putObjectVolatile",  "(Ljava/lang/Object;JLjava/lang/Object;)V")},
      {x(TR::sun_misc_Unsafe_putObjectVolatile_jlObjectJjlObject_V, "putReferenceVolatile",  "(Ljava/lang/Object;JLjava/lang/Object;)V")},

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
      {x(TR::sun_misc_Unsafe_getObject_jlObjectJ_jlObject,  "getReference",  "(Ljava/lang/Object;J)Ljava/lang/Object;")},

      {x(TR::sun_misc_Unsafe_getBooleanVolatile_jlObjectJ_Z,        "getBooleanVolatile", "(Ljava/lang/Object;J)Z")},
      {x(TR::sun_misc_Unsafe_getByteVolatile_jlObjectJ_B,           "getByteVolatile",    "(Ljava/lang/Object;J)B")},
      {x(TR::sun_misc_Unsafe_getCharVolatile_jlObjectJ_C,           "getCharVolatile",    "(Ljava/lang/Object;J)C")},
      {x(TR::sun_misc_Unsafe_getShortVolatile_jlObjectJ_S,          "getShortVolatile",   "(Ljava/lang/Object;J)S")},
      {x(TR::sun_misc_Unsafe_getIntVolatile_jlObjectJ_I,            "getIntVolatile",     "(Ljava/lang/Object;J)I")},
      {x(TR::sun_misc_Unsafe_getLongVolatile_jlObjectJ_J,           "getLongVolatile",    "(Ljava/lang/Object;J)J")},
      {x(TR::sun_misc_Unsafe_getFloatVolatile_jlObjectJ_F,          "getFloatVolatile",   "(Ljava/lang/Object;J)F")},
      {x(TR::sun_misc_Unsafe_getDoubleVolatile_jlObjectJ_D,         "getDoubleVolatile",  "(Ljava/lang/Object;J)D")},
      {x(TR::sun_misc_Unsafe_getObjectVolatile_jlObjectJ_jlObject,  "getObjectVolatile",  "(Ljava/lang/Object;J)Ljava/lang/Object;")},
      {x(TR::sun_misc_Unsafe_getObjectVolatile_jlObjectJ_jlObject,  "getReferenceVolatile",  "(Ljava/lang/Object;J)Ljava/lang/Object;")},

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
      {x(TR::sun_misc_Unsafe_compareAndSwapObject_jlObjectJjlObjectjlObject_Z, "compareAndSetReference", "(Ljava/lang/Object;JLjava/lang/Object;Ljava/lang/Object;)Z")},

      {x(TR::sun_misc_Unsafe_compareAndExchangeInt_jlObjectJII_Z,                  "compareAndExchangeInt",    "(Ljava/lang/Object;JII)I")},
      {x(TR::sun_misc_Unsafe_compareAndExchangeLong_jlObjectJJJ_Z,                 "compareAndExchangeLong",   "(Ljava/lang/Object;JJJ)J")},
      {x(TR::sun_misc_Unsafe_compareAndExchangeObject_jlObjectJjlObjectjlObject_Z, "compareAndExchangeObject", "(Ljava/lang/Object;JLjava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;")},
      {x(TR::sun_misc_Unsafe_compareAndExchangeObject_jlObjectJjlObjectjlObject_Z, "compareAndExchangeReference", "(Ljava/lang/Object;JLjava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;")},

      {x(TR::sun_misc_Unsafe_compareAndExchangeInt_jlObjectJII_Z,                  "compareAndExchangeIntVolatile",    "(Ljava/lang/Object;JII)I")},
      {x(TR::sun_misc_Unsafe_compareAndExchangeLong_jlObjectJJJ_Z,                 "compareAndExchangeLongVolatile",   "(Ljava/lang/Object;JJJ)J")},
      {x(TR::sun_misc_Unsafe_compareAndExchangeObject_jlObjectJjlObjectjlObject_Z, "compareAndExchangeObjectVolatile", "(Ljava/lang/Object;JLjava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;")},

      {x(TR::sun_misc_Unsafe_staticFieldBase,               "staticFieldBase",   "(Ljava/lang/reflect/Field;)Ljava/lang/Object")},
      {x(TR::sun_misc_Unsafe_staticFieldOffset,             "staticFieldOffset", "(Ljava/lang/reflect/Field;)J")},
      {x(TR::sun_misc_Unsafe_objectFieldOffset,             "objectFieldOffset", "(Ljava/lang/reflect/Field;)J")},
      {x(TR::sun_misc_Unsafe_getAndAddInt,                  "getAndAddInt",      "(Ljava/lang/Object;JI)I")},
      {x(TR::sun_misc_Unsafe_getAndSetInt,                  "getAndSetInt",      "(Ljava/lang/Object;JI)I")},
      {x(TR::sun_misc_Unsafe_getAndAddLong,                 "getAndAddLong",     "(Ljava/lang/Object;JJ)J")},
      {x(TR::sun_misc_Unsafe_getAndSetLong,                 "getAndSetLong",     "(Ljava/lang/Object;JJ)J")},

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
      {x(TR::sun_misc_Unsafe_allocateInstance,           "allocateInstance",       "(Ljava/lang/Class;)Ljava/lang/Object;")},
      {x(TR::jdk_internal_misc_Unsafe_copyMemory0,   "copyMemory0", "(Ljava/lang/Object;JLjava/lang/Object;JJ)V")},
      {  TR::unknownMethod}
      };

#if JAVA_SPEC_VERSION >= 15
   static X NativeLibrariesMethods[] =
      {
#if (17 <= JAVA_SPEC_VERSION) && (JAVA_SPEC_VERSION <= 18)
      {x(TR::jdk_internal_loader_NativeLibraries_load, "load", "(Ljdk/internal/loader/NativeLibraries$NativeLibraryImpl;Ljava/lang/String;ZZZ)Z")},
#else /* (17 <= JAVA_SPEC_VERSION) && (JAVA_SPEC_VERSION <= 18) */
      {x(TR::jdk_internal_loader_NativeLibraries_load, "load", "(Ljdk/internal/loader/NativeLibraries$NativeLibraryImpl;Ljava/lang/String;ZZ)Z")},
#endif /* (17 <= JAVA_SPEC_VERSION) && (JAVA_SPEC_VERSION <= 18) */
      {  TR::unknownMethod}
      };
#endif /* JAVA_SPEC_VERSION >= 15 */


   static X PreconditionsMethods[] =
      {
      {x(TR::jdk_internal_util_Preconditions_checkIndex, "checkIndex", "(IILjava/util/function/BiFunction;)I")},
      {  TR::unknownMethod}
      };


   static X VectorSupportMethods[] =
      {
      {x(TR::jdk_internal_vm_vector_VectorSupport_load, "load", "(Ljava/lang/Class;Ljava/lang/Class;ILjava/lang/Object;JLjava/lang/Object;JLjdk/internal/vm/vector/VectorSupport$VectorSpecies;Ljdk/internal/vm/vector/VectorSupport$LoadOperation;)Ljdk/internal/vm/vector/VectorSupport$VectorPayload;")},
      {x(TR::jdk_internal_vm_vector_VectorSupport_binaryOp, "binaryOp", "(ILjava/lang/Class;Ljava/lang/Class;Ljava/lang/Class;ILjdk/internal/vm/vector/VectorSupport$VectorPayload;Ljdk/internal/vm/vector/VectorSupport$VectorPayload;Ljdk/internal/vm/vector/VectorSupport$VectorMask;Ljdk/internal/vm/vector/VectorSupport$BinaryOperation;)Ljdk/internal/vm/vector/VectorSupport$VectorPayload;" )},
      {x(TR::jdk_internal_vm_vector_VectorSupport_blend, "blend", "(Ljava/lang/Class;Ljava/lang/Class;Ljava/lang/Class;ILjdk/internal/vm/vector/VectorSupport$Vector;Ljdk/internal/vm/vector/VectorSupport$Vector;Ljdk/internal/vm/vector/VectorSupport$VectorMask;Ljdk/internal/vm/vector/VectorSupport$VectorBlendOp;)Ljdk/internal/vm/vector/VectorSupport$Vector;")},
      {x(TR::jdk_internal_vm_vector_VectorSupport_broadcastInt, "broadcastInt", "(ILjava/lang/Class;Ljava/lang/Class;Ljava/lang/Class;ILjdk/internal/vm/vector/VectorSupport$Vector;ILjdk/internal/vm/vector/VectorSupport$VectorMask;Ljdk/internal/vm/vector/VectorSupport$VectorBroadcastIntOp;)Ljdk/internal/vm/vector/VectorSupport$Vector;")},
      {x(TR::jdk_internal_vm_vector_VectorSupport_compare, "compare", "(ILjava/lang/Class;Ljava/lang/Class;Ljava/lang/Class;ILjdk/internal/vm/vector/VectorSupport$Vector;Ljdk/internal/vm/vector/VectorSupport$Vector;Ljdk/internal/vm/vector/VectorSupport$VectorMask;Ljdk/internal/vm/vector/VectorSupport$VectorCompareOp;)Ljdk/internal/vm/vector/VectorSupport$VectorMask;")},
      {x(TR::jdk_internal_vm_vector_VectorSupport_compressExpandOp, "compressExpandOp", "(ILjava/lang/Class;Ljava/lang/Class;Ljava/lang/Class;ILjdk/internal/vm/vector/VectorSupport$Vector;Ljdk/internal/vm/vector/VectorSupport$VectorMask;Ljdk/internal/vm/vector/VectorSupport$CompressExpandOperation;)Ljdk/internal/vm/vector/VectorSupport$VectorPayload;")},
      {x(TR::jdk_internal_vm_vector_VectorSupport_convert, "convert", "(ILjava/lang/Class;Ljava/lang/Class;ILjava/lang/Class;Ljava/lang/Class;ILjdk/internal/vm/vector/VectorSupport$VectorPayload;Ljdk/internal/vm/vector/VectorSupport$VectorSpecies;Ljdk/internal/vm/vector/VectorSupport$VectorConvertOp;)Ljdk/internal/vm/vector/VectorSupport$VectorPayload;")},
      {x(TR::jdk_internal_vm_vector_VectorSupport_fromBitsCoerced, "fromBitsCoerced", "(Ljava/lang/Class;Ljava/lang/Class;IJILjdk/internal/vm/vector/VectorSupport$VectorSpecies;Ljdk/internal/vm/vector/VectorSupport$FromBitsCoercedOperation;)Ljdk/internal/vm/vector/VectorSupport$VectorPayload;")},
      {x(TR::jdk_internal_vm_vector_VectorSupport_maskReductionCoerced, "maskReductionCoerced", "(ILjava/lang/Class;Ljava/lang/Class;ILjdk/internal/vm/vector/VectorSupport$VectorMask;Ljdk/internal/vm/vector/VectorSupport$VectorMaskOp;)J")},
      {x(TR::jdk_internal_vm_vector_VectorSupport_reductionCoerced, "reductionCoerced", "(ILjava/lang/Class;Ljava/lang/Class;Ljava/lang/Class;ILjdk/internal/vm/vector/VectorSupport$Vector;Ljdk/internal/vm/vector/VectorSupport$VectorMask;Ljdk/internal/vm/vector/VectorSupport$ReductionOperation;)J")},
      {x(TR::jdk_internal_vm_vector_VectorSupport_ternaryOp, "ternaryOp", "(ILjava/lang/Class;Ljava/lang/Class;Ljava/lang/Class;ILjdk/internal/vm/vector/VectorSupport$Vector;Ljdk/internal/vm/vector/VectorSupport$Vector;Ljdk/internal/vm/vector/VectorSupport$Vector;Ljdk/internal/vm/vector/VectorSupport$VectorMask;Ljdk/internal/vm/vector/VectorSupport$TernaryOperation;)Ljdk/internal/vm/vector/VectorSupport$Vector;")},
      {x(TR::jdk_internal_vm_vector_VectorSupport_test, "test", "(ILjava/lang/Class;Ljava/lang/Class;ILjdk/internal/vm/vector/VectorSupport$VectorMask;Ljdk/internal/vm/vector/VectorSupport$VectorMask;Ljava/util/function/BiFunction;)Z")},
      {x(TR::jdk_internal_vm_vector_VectorSupport_unaryOp, "unaryOp", "(ILjava/lang/Class;Ljava/lang/Class;Ljava/lang/Class;ILjdk/internal/vm/vector/VectorSupport$Vector;Ljdk/internal/vm/vector/VectorSupport$VectorMask;Ljdk/internal/vm/vector/VectorSupport$UnaryOperation;)Ljdk/internal/vm/vector/VectorSupport$Vector;")},
      {x(TR::jdk_internal_vm_vector_VectorSupport_store, "store", "(Ljava/lang/Class;Ljava/lang/Class;ILjava/lang/Object;JLjdk/internal/vm/vector/VectorSupport$VectorPayload;Ljava/lang/Object;JLjdk/internal/vm/vector/VectorSupport$StoreVectorOperation;)V")},
      {  TR::unknownMethod}
      };

   static X ArrayMethods[] =
      {
      {x(TR::java_lang_reflect_Array_getLength, "getLength", "(Ljava/lang/Object;)I")},
      {  TR::unknownMethod}
      };

   static X MethodMethods[] =
      {
      {  TR::java_lang_reflect_Method_invoke, 6, "invoke", (int16_t)-1, "*"},
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
      {x(TR::com_ibm_jit_JITHelpers_intrinsicIndexOfStringLatin1,             "intrinsicIndexOfStringLatin1", "(Ljava/lang/Object;ILjava/lang/Object;II)I")},
      {x(TR::com_ibm_jit_JITHelpers_intrinsicIndexOfStringUTF16,              "intrinsicIndexOfStringUTF16", "(Ljava/lang/Object;ILjava/lang/Object;II)I")},
      {x(TR::com_ibm_jit_JITHelpers_intrinsicIndexOfLatin1,                   "intrinsicIndexOfLatin1", "(Ljava/lang/Object;BII)I")},
      {x(TR::com_ibm_jit_JITHelpers_intrinsicIndexOfUTF16,                    "intrinsicIndexOfUTF16", "(Ljava/lang/Object;CII)I")},
#ifdef TR_TARGET_32BIT
      {x(TR::com_ibm_jit_JITHelpers_getJ9ClassFromObject32,                   "getJ9ClassFromObject32", "(Ljava/lang/Object;)I")},
      {x(TR::com_ibm_jit_JITHelpers_getJ9ClassFromClass32,                    "getJ9ClassFromClass32", "(Ljava/lang/Class;)I")},
      {x(TR::com_ibm_jit_JITHelpers_getBackfillOffsetFromJ9Class32,           "getBackfillOffsetFromJ9Class32", "(I)I")},
      {x(TR::com_ibm_jit_JITHelpers_getRomClassFromJ9Class32,                 "getRomClassFromJ9Class32", "(I)I")},
      {x(TR::com_ibm_jit_JITHelpers_getArrayShapeFromRomClass32,              "getArrayShapeFromRomClass32", "(I)I")},
      {x(TR::com_ibm_jit_JITHelpers_getSuperClassesFromJ9Class32,             "getSuperClassesFromJ9Class32", "(I)I")},
      {x(TR::com_ibm_jit_JITHelpers_getClassDepthAndFlagsFromJ9Class32,       "getClassDepthAndFlagsFromJ9Class32", "(I)I")},
      {x(TR::com_ibm_jit_JITHelpers_getClassFlagsFromJ9Class32,               "getClassFlagsFromJ9Class32", "(I)I")},
      {x(TR::com_ibm_jit_JITHelpers_getModifiersFromRomClass32,               "getModifiersFromRomClass32", "(I)I")},
      {x(TR::com_ibm_jit_JITHelpers_getClassFromJ9Class32,                    "getClassFromJ9Class32", "(I)Ljava/lang/Class;")},
      {x(TR::com_ibm_jit_JITHelpers_getAddressAsPrimitive32,                  "getAddressAsPrimitive32", "(Ljava/lang/Object;)I")},
#endif
#ifdef TR_TARGET_64BIT
      {x(TR::com_ibm_jit_JITHelpers_getJ9ClassFromObject64,                   "getJ9ClassFromObject64", "(Ljava/lang/Object;)J")},
      {x(TR::com_ibm_jit_JITHelpers_getJ9ClassFromClass64,                    "getJ9ClassFromClass64", "(Ljava/lang/Class;)J")},
      {x(TR::com_ibm_jit_JITHelpers_getBackfillOffsetFromJ9Class64,           "getBackfillOffsetFromJ9Class64", "(J)J")},
      {x(TR::com_ibm_jit_JITHelpers_getRomClassFromJ9Class64,                 "getRomClassFromJ9Class64", "(J)J")},
      {x(TR::com_ibm_jit_JITHelpers_getArrayShapeFromRomClass64,              "getArrayShapeFromRomClass64", "(J)I")},
      {x(TR::com_ibm_jit_JITHelpers_getSuperClassesFromJ9Class64,             "getSuperClassesFromJ9Class64", "(J)J")},
      {x(TR::com_ibm_jit_JITHelpers_getClassDepthAndFlagsFromJ9Class64,       "getClassDepthAndFlagsFromJ9Class64", "(J)J")},
      {x(TR::com_ibm_jit_JITHelpers_getClassFlagsFromJ9Class64,               "getClassFlagsFromJ9Class64", "(J)I")},
      {x(TR::com_ibm_jit_JITHelpers_getModifiersFromRomClass64,               "getModifiersFromRomClass64", "(J)I")},
      {x(TR::com_ibm_jit_JITHelpers_getClassFromJ9Class64,                    "getClassFromJ9Class64", "(J)Ljava/lang/Class;")},
      {x(TR::com_ibm_jit_JITHelpers_getAddressAsPrimitive64,                  "getAddressAsPrimitive64", "(Ljava/lang/Object;)J")},
#endif
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
      {x(TR::com_ibm_jit_JITHelpers_acmplt,                                   "acmplt", "(Ljava/lang/Object;Ljava/lang/Object;)Z")},
      {x(TR::com_ibm_jit_JITHelpers_getClassInitializeStatus,                 "getClassInitializeStatus", "(Ljava/lang/Class;)I")},
      {x(TR::com_ibm_jit_JITHelpers_supportsIntrinsicCaseConversion,          "supportsIntrinsicCaseConversion", "()Z")},
      {x(TR::com_ibm_jit_JITHelpers_toUpperIntrinsicLatin1,                   "toUpperIntrinsicLatin1", "([B[BI)Z")},
      {x(TR::com_ibm_jit_JITHelpers_toUpperIntrinsicLatin1,                   "toUpperIntrinsicLatin1", "([C[CI)Z")},
      {x(TR::com_ibm_jit_JITHelpers_toLowerIntrinsicLatin1,                   "toLowerIntrinsicLatin1", "([B[BI)Z")},
      {x(TR::com_ibm_jit_JITHelpers_toLowerIntrinsicLatin1,                   "toLowerIntrinsicLatin1", "([C[CI)Z")},
      {x(TR::com_ibm_jit_JITHelpers_toUpperIntrinsicUTF16,                    "toUpperIntrinsicUTF16", "([B[BI)Z")},
      {x(TR::com_ibm_jit_JITHelpers_toUpperIntrinsicUTF16,                    "toUpperIntrinsicUTF16", "([C[CI)Z")},
      {x(TR::com_ibm_jit_JITHelpers_toLowerIntrinsicUTF16,                    "toLowerIntrinsicUTF16", "([B[BI)Z")},
      {x(TR::com_ibm_jit_JITHelpers_toLowerIntrinsicUTF16,                    "toLowerIntrinsicUTF16", "([C[CI)Z")},
      {x(TR::com_ibm_jit_JITHelpers_dispatchComputedStaticCall,               "dispatchComputedStaticCall", "()V")},
      {x(TR::com_ibm_jit_JITHelpers_dispatchVirtual,                          "dispatchVirtual", "()V")},
      { TR::unknownMethod}
      };

   static X StringLatin1Methods[] =
      {
      { x(TR::java_lang_StringLatin1_indexOf,                                 "indexOf",       "([BI[BII)I")},
      { x(TR::java_lang_StringLatin1_inflate,                                 "inflate",       "([BI[CII)V")},
      { TR::unknownMethod }
      };

   static X StringUTF16Methods[] =
      {
      { x(TR::java_lang_StringUTF16_charAt,                                   "charAt",             "([BI)C")},
      { x(TR::java_lang_StringUTF16_checkIndex,                               "checkIndex",         "(I[B)V")},
      { x(TR::java_lang_StringUTF16_compareCodePointCI,                       "compareCodePointCI", "(II)I")},
      { x(TR::java_lang_StringUTF16_compareToCIImpl,                          "compareToCIImpl",    "([BII[BII)I")},
      { x(TR::java_lang_StringUTF16_compareValues,                            "compareValues",      "([B[BII)I")},
      { x(TR::java_lang_StringUTF16_getChar,                                  "getChar",            "([BI)C")},
      { x(TR::java_lang_StringUTF16_indexOf,                                  "indexOf",            "([BI[BII)I")},
      { x(TR::java_lang_StringUTF16_length,                                   "length",             "([B)I")},
      { x(TR::java_lang_StringUTF16_newBytesFor,                              "newBytesFor",        "(I)[B")},
      { x(TR::java_lang_StringUTF16_putChar,                                  "putChar",            "([BII)V")},
      { x(TR::java_lang_StringUTF16_toBytes,                                  "toBytes",            "([CII)[B")},
      { x(TR::java_lang_StringUTF16_getChars_Integer,                         "getChars",            "(II[B)I")},
      { x(TR::java_lang_StringUTF16_getChars_Long,                            "getChars",            "(JI[B)I")},
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
      {x(TR::java_lang_Integer_toUnsignedLong,          "toUnsignedLong",         "(I)J")},
      {x(TR::java_lang_Integer_stringSize,              "stringSize",         "(I)I")},
      {x(TR::java_lang_Integer_toString,                "toString",         "(I)Ljava/lang/String;")},
      {x(TR::java_lang_Integer_getChars,                "getChars",        "(II[B)I")},
      {x(TR::java_lang_Integer_getChars_charBuffer,     "getChars",        "(II[C)V")},
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
      {x(TR::java_lang_Long_stringSize,                 "stringSize",            "(J)I") },
      {x(TR::java_lang_Long_toString,                   "toString",            "(J)Ljava/lang/String;") },
      {x(TR::java_lang_Long_getChars,                   "getChars",        "(JI[B)I")},
      {x(TR::java_lang_Long_getChars_charBuffer,        "getChars",        "(JI[C)V")},
      {  TR::unknownMethod}
      };

   static X BooleanMethods[] =
      {
      {  TR::java_lang_Boolean_init,          6,    "<init>", (int16_t)-1,    "*"},
      {  TR::unknownMethod}
      };

   static X ByteOrderMethods[] =
        {
        {x(TR::java_nio_ByteOrder_nativeOrder,          "nativeOrder",          "()Ljava/nio/ByteOrder;")},
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
      {x(TR::java_lang_Character_toLowerCase,         "toLowerCase",          "(I)I")},
      {  TR::unknownMethod}
      };

   static X CRC32Methods[] =
      {
      {x(TR::java_util_zip_CRC32_update,              "update",               "(II)I")},
      {x(TR::java_util_zip_CRC32_updateBytes,         "updateBytes",          "(I[BII)I")},
      {x(TR::java_util_zip_CRC32_updateBytes0,        "updateBytes0",         "(I[BII)I")},
      {x(TR::java_util_zip_CRC32_updateByteBuffer,    "updateByteBuffer",     "(IJII)I")},
      {x(TR::java_util_zip_CRC32_updateByteBuffer0,   "updateByteBuffer0",    "(IJII)I")},
      {  TR::unknownMethod}
      };

   static X CRC32CMethods[] =
      {
      {x(TR::java_util_zip_CRC32C_updateBytes,               "updateBytes",                "(I[BII)I")},
      {x(TR::java_util_zip_CRC32C_updateDirectByteBuffer,    "updateDirectByteBuffer",     "(IJII)I")},
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
      {x(TR::sun_reflect_Reflection_getCallerClass, "getCallerClass", "()Ljava/lang/Class;")},
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
      {x(TR::java_lang_ref_Reference_refersTo, "refersTo", "(Ljava/lang/Object;)Z")},
      {TR::unknownMethod}
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
      {x(TR::java_lang_invoke_MethodHandle_type                     ,   "type",                       "()Ljava/lang/invoke/MethodType;")},
      {x(TR::java_lang_invoke_MethodHandle_asType, "asType", "(Ljava/lang/invoke/MethodHandle;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/MethodHandle;")},
      {x(TR::java_lang_invoke_MethodHandle_asType_instance, "asType", "(Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/MethodHandle;")},
      {  TR::java_lang_invoke_MethodHandle_invokeBasic              ,   11, "invokeBasic",                (int16_t)-1, "*"},
      {  TR::java_lang_invoke_MethodHandle_linkToStatic             ,   12, "linkToStatic",                (int16_t)-1, "*"},
      {  TR::java_lang_invoke_MethodHandle_linkToSpecial            ,   13, "linkToSpecial",                (int16_t)-1, "*"},
      {  TR::java_lang_invoke_MethodHandle_linkToVirtual            ,   13, "linkToVirtual",                (int16_t)-1, "*"},
      {  TR::java_lang_invoke_MethodHandle_linkToInterface          ,   15, "linkToInterface",                (int16_t)-1, "*"},
      {  TR::unknownMethod}
      };

   static X DirectMethodHandleMethods[] =
      {
      {x(TR::java_lang_invoke_DirectMethodHandle_internalMemberName,            "internalMemberName",                 "(Ljava/lang/Object;)Ljava/lang/Object;")},
      {x(TR::java_lang_invoke_DirectMethodHandle_internalMemberNameEnsureInit,  "internalMemberNameEnsureInit",       "(Ljava/lang/Object;)Ljava/lang/Object;")},
      {x(TR::java_lang_invoke_DirectMethodHandle_constructorMethod,             "constructorMethod",       "(Ljava/lang/Object;)Ljava/lang/Object;")},
      {  TR::unknownMethod}
      };

   static X PrimitiveHandleMethods[] =
      {
      {x(TR::java_lang_invoke_PrimitiveHandle_initializeClassIfRequired,  "initializeClassIfRequired",       "()V")},
      {  TR::unknownMethod}
      };

   static X VarHandleMethods[] =
      {
      // Recognized method only works for resolved methods
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
      {  TR::java_lang_invoke_VarHandle_getAndBitwiseOrAcquire    ,   27, "getAndBitwiseOrAcquire_impl",         (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_getAndBitwiseOrRelease    ,   27, "getAndBitwiseOrRelease_impl",         (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_getAndBitwiseXor          ,   21, "getAndBitwiseXor_impl",                 (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_getAndBitwiseXorAcquire   ,   28, "getAndBitwiseXorAcquire_impl",         (int16_t)-1, "*"},
      {  TR::java_lang_invoke_VarHandle_getAndBitwiseXorRelease   ,   28, "getAndBitwiseXorRelease_impl",         (int16_t)-1, "*"},
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
      {x(TR::java_lang_invoke_CollectHandle_allocateArray,                 "allocateArray",               "(Ljava/lang/invoke/CollectHandle;)Ljava/lang/Object;")},
      {x(TR::java_lang_invoke_CollectHandle_numArgsToPassThrough,          "numArgsToPassThrough",        "()I")},
      {x(TR::java_lang_invoke_CollectHandle_numArgsToCollect,              "numArgsToCollect",            "()I")},
      {x(TR::java_lang_invoke_CollectHandle_collectionStart,               "collectionStart",             "()I")},
      {x(TR::java_lang_invoke_CollectHandle_numArgsAfterCollectArray,      "numArgsAfterCollectArray",    "()I")},
      {  TR::java_lang_invoke_CollectHandle_invokeExact,          28,  "invokeExact_thunkArchetype_X",    (int16_t)-1, "*"},
      {  TR::unknownMethod}
      };

   static X InvokersMethods[] =
      {
      {TR::java_lang_invoke_Invokers_checkCustomized,            15,       "checkCustomized",             (int16_t)-1, "*"},
      {TR::java_lang_invoke_Invokers_checkExactType,             14,       "checkExactType",              (int16_t)-1, "*"},
      {TR::java_lang_invoke_Invokers_getCallSiteTarget,          17,       "getCallSiteTarget",           (int16_t)-1, "*"},
      {TR::unknownMethod}
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

   static X BruteArgumentMoverHandleMethods[] =
      {
      {  TR::java_lang_invoke_BruteArgumentMoverHandle_permuteArgs,     11, "permuteArgs",   (int16_t)-1, "*"},
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

   static X MethodHandleImplCountingWrapperMethods[] =
      {
      {x(TR::java_lang_invoke_MethodHandleImpl_CountingWrapper_getTarget,   "getTarget",  "()Ljava/lang/invoke/MethodHandle;")},
      {  TR::unknownMethod}
      };

   static X DelegatingMethodHandleMethods[] =
      {
      {x(TR::java_lang_invoke_DelegatingMethodHandle_getTarget,   "getTarget",  "()Ljava/lang/invoke/MethodHandle;")},
      {  TR::unknownMethod}
      };

   static X DirectHandleMethods[] =
      {
      {x(TR::java_lang_invoke_DirectHandle_isAlreadyCompiled,   "isAlreadyCompiled",  "(J)Z")},
      {x(TR::java_lang_invoke_DirectHandle_compiledEntryPoint,  "compiledEntryPoint", "(J)J")},
      {x(TR::java_lang_invoke_DirectHandle_nullCheckIfRequired,  "nullCheckIfRequired", "(Ljava/lang/Object;)V")},
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
   static X FilterArgumentsWithCombinerHandleMethods[] =
      {
      {x(TR::java_lang_invoke_FilterArgumentsWithCombinerHandle_filterPosition,               "filterPosition",            "()I")},
      {x(TR::java_lang_invoke_FilterArgumentsWithCombinerHandle_argumentIndices,               "argumentIndices",            "()I")},
      {x(TR::java_lang_invoke_FilterArgumentsWithCombinerHandle_numSuffixArgs,      "numSuffixArgs",     "()I")},
      {  TR::java_lang_invoke_FilterArgumentsWithCombinerHandle_argumentsForCombiner,  20,  "argumentsForCombiner",    (int16_t)-1, "*"},
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
      {  TR::java_lang_invoke_FilterArgumentsHandle_invokeExact,  28,  "invokeExact_thunkArchetype_X",    (int16_t)-1, "*"},
      {  TR::unknownMethod}
      };

    static X ConvertHandleFilterHelpersMethods[] =
      {
      {x(TR::java_lang_invoke_ConvertHandleFilterHelpers_object2J,          "object2J",    "(Ljava/lang/Object;)J")},
      {x(TR::java_lang_invoke_ConvertHandleFilterHelpers_number2J,          "number2J",    "(Ljava/lang/Number;)J")},
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

   static X JavaLangInvokeMethodHandleImplMethods [] =
      {
      {x(TR::java_lang_invoke_MethodHandleImpl_isCompileConstant, "isCompileConstant", "(Ljava/lang/Object;)Z")},
      {x(TR::java_lang_invoke_MethodHandleImpl_profileBoolean, "profileBoolean", "(Z[I)Z")},
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

   static X JavaUtilRegexMatcherMethods [] =
      {
      {x(TR::java_util_regex_Matcher_init, "<init>", "(Ljava/util/regex/Pattern;Ljava/lang/CharSequence;)V")},
      {x(TR::java_util_regex_Matcher_usePattern, "usePattern", "(Ljava/util/regex/Pattern;)Ljava/util/regex/Matcher;")},
      {TR::unknownMethod}
      };

   struct Y { const char * _class; X * _methods; };

   /* classXX where XX is the number of characters in the class name */
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
      { "java/util/HashMap", HashMapMethods },
      { 0 }
      };

   static Y class18[] =
      {
      { "com/ibm/gpu/Kernel", GPUMethods },
      { "java/nio/ByteOrder", ByteOrderMethods},
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
      { "java/util/zip/CRC32C", CRC32CMethods     },
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
      { "java/lang/StringLatin1", StringLatin1Methods },
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
      { "java/util/regex/Matcher", JavaUtilRegexMatcherMethods },
      { 0 }
      };

   static Y class24[] =
      {
      { "java/lang/reflect/Method", MethodMethods },
      { "sun/nio/cs/UTF_8$Decoder", EncodeMethods },
      { "sun/nio/cs/UTF_8$Encoder", EncodeMethods },
      { "sun/nio/cs/UTF16_Encoder", EncodeMethods },
      { "jdk/internal/misc/Unsafe", UnsafeMethods },
      { 0 }
      };

   static Y class25[] =
      {
      { "java/lang/invoke/Invokers", InvokersMethods },
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
      { "jdk/internal/reflect/Reflection", ReflectionMethods },
      { "jdk/internal/util/Preconditions", PreconditionsMethods },

      { 0 }
      };
   static Y class32[] =
      {
      { "java/lang/invoke/MutableCallSite", MutableCallSiteMethods },
      { "java/lang/invoke/PrimitiveHandle", PrimitiveHandleMethods },
      { "com/ibm/dataaccess/PackedDecimal", DataAccessPackedDecimalMethods },
      { 0 }
      };

   static Y class33[] =
      {
      { "java/util/stream/AbstractPipeline", JavaUtilStreamAbstractPipelineMethods },
      { "java/util/stream/IntPipeline$Head", JavaUtilStreamIntPipelineHeadMethods },
      { "java/lang/invoke/MethodHandleImpl", JavaLangInvokeMethodHandleImplMethods },
      { 0 }
      };

   static Y class34[] =
      {
      { "java/util/Hashtable$HashEnumerator", HashtableHashEnumeratorMethods },
      { "com/ibm/Compiler/Internal/Prefetch", PrefetchMethods },
      { "java/lang/invoke/VarHandleInternal", VarHandleMethods },
      { 0 }
      };

   static Y class35[] =
      {
      { "java/lang/invoke/ExplicitCastHandle", ExplicitCastHandleMethods },
#if JAVA_SPEC_VERSION >= 15
      { "jdk/internal/loader/NativeLibraries", NativeLibrariesMethods },
#endif /* JAVA_SPEC_VERSION >= 15 */
      { "java/lang/invoke/DirectMethodHandle", DirectMethodHandleMethods },
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
      { "jdk/internal/vm/vector/VectorSupport", VectorSupportMethods },
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
      { "java/lang/invoke/DelegatingMethodHandle", DelegatingMethodHandleMethods },
      { 0 }
      };

   static Y class40[] =
      {
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
      { "java/lang/invoke/BruteArgumentMoverHandle", BruteArgumentMoverHandleMethods },
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

   static Y class44[] =
      {
      { "java/lang/invoke/ConvertHandle$FilterHelpers", ConvertHandleFilterHelpersMethods },
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
      { "java/util/concurrent/ConcurrentHashMap$TreeBin", JavaUtilConcurrentConcurrentHashMapTreeBinMethods },
      { "java/io/ObjectInputStream$BlockDataInputStream", ObjectInputStream_BlockDataInputStreamMethods },
      { 0 }
      };

   static Y class48[] =
      {
      { "java/util/concurrent/atomic/AtomicReferenceArray", JavaUtilConcurrentAtomicReferenceArrayMethods },
      { 0 }
      };

   static Y class49[] =
      {
      { "java/lang/invoke/MethodHandleImpl$CountingWrapper", MethodHandleImplCountingWrapperMethods },
      { 0 }
      };

   static Y class50[] =
      {
      { "java/util/concurrent/atomic/AtomicLongFieldUpdater", JavaUtilConcurrentAtomicLongFieldUpdaterMethods },
      { "java/lang/invoke/FilterArgumentsWithCombinerHandle", FilterArgumentsWithCombinerHandleMethods },
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
      class40, class41, class42, class43, class44, class45, class46, 0, class48, class49,
      class50, 0, 0, class53, 0, class55
      };

   const int32_t minRecognizedClassLength = 10;
   const int32_t maxRecognizedClassLength = sizeof(recognizedClasses)/sizeof(recognizedClasses[0]) + minRecognizedClassLength - 1;

   const int32_t minNonUserClassLength = 17;

   const int32_t maxNonUserClassLength = 17;
   static Y * nonUserClasses[] =
      {
      class17
      };

   if (isMethodInValidLibrary())
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
                        setRecognizedMethodInfo(m->_enum);
                        break;
                        }
                     }
                  }
         }

      if (TR::Method::getMandatoryRecognizedMethod() == TR::unknownMethod)
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
         else if (classNameLen == strlen(JSR292_StaticFieldGetterHandle)
                  && !strncmp(className, JSR292_StaticFieldGetterHandle, classNameLen))
            {
            if (nameLen > 27 && !strncmp(name, "invokeExact_thunkArchetype_", 27))
               setRecognizedMethodInfo(TR::java_lang_invoke_StaticFieldGetterHandle_invokeExact);
            }
         else if (classNameLen == strlen(JSR292_StaticFieldSetterHandle)
                  && !strncmp(className, JSR292_StaticFieldSetterHandle, classNameLen))
            {
            if (nameLen > 27 && !strncmp(name, "invokeExact_thunkArchetype_", 27))
               setRecognizedMethodInfo(TR::java_lang_invoke_StaticFieldSetterHandle_invokeExact);
            }
         else if (classNameLen == strlen(JSR292_FieldGetterHandle)
                  && !strncmp(className, JSR292_FieldGetterHandle, classNameLen))
            {
            if (nameLen > 27 && !strncmp(name, "invokeExact_thunkArchetype_", 27))
               setRecognizedMethodInfo(TR::java_lang_invoke_FieldGetterHandle_invokeExact);
            }
         else if (classNameLen == strlen(JSR292_FieldSetterHandle)
                  && !strncmp(className, JSR292_FieldSetterHandle, classNameLen))
            {
            if (nameLen > 27 && !strncmp(name, "invokeExact_thunkArchetype_", 27))
               setRecognizedMethodInfo(TR::java_lang_invoke_FieldSetterHandle_invokeExact);
            }
         }
      }
   #if defined(TR_HOST_X86)
      switch (convertToMethod()->getRecognizedMethod())
         {
         case TR::java_lang_Class_isAssignableFrom:
         case TR::java_lang_System_nanoTime:
            _jniTargetAddress = NULL;
            break;
         default:
            break;
         }
   #endif
   }

bool
TR_ResolvedJ9Method::shouldFailSetRecognizedMethodInfoBecauseOfHCR()
   {
   TR_OpaqueClassBlock *clazz = fej9()->getClassOfMethod(getPersistentIdentifier());
   J9JITConfig * jitConfig = fej9()->getJ9JITConfig();
   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);
   TR_PersistentClassInfo *clazzInfo = NULL;
   if (compInfo->getPersistentInfo()->getPersistentCHTable())
      {
      clazzInfo = compInfo->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(clazz, fej9(), true);
      }

   if (!clazzInfo)
      return true;
   else if (clazzInfo->classHasBeenRedefined())
      return true;
   return false;
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
      if (shouldFailSetRecognizedMethodInfoBecauseOfHCR())
         failBecauseOfHCR = true;
      }

   if (TR::Options::getCmdLineOptions()->getOption(TR_FullSpeedDebug))
      {
      switch (rm)
         {
         case TR::java_lang_J9VMInternals_getSuperclass:
         case TR::com_ibm_jit_JITHelpers_getSuperclass:
         case TR::com_ibm_jit_DecimalFormatHelper_formatAsDouble:
         case TR::com_ibm_jit_DecimalFormatHelper_formatAsFloat:
            return;
         default:
            break;
         }
      }

   if ( isMethodInValidLibrary() &&
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
            case TR::sun_misc_Unsafe_getAndAddInt:
            case TR::sun_misc_Unsafe_getAndSetInt:
            case TR::sun_misc_Unsafe_getAndAddLong:
            case TR::sun_misc_Unsafe_getAndSetLong:

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
            case TR::java_lang_String_hashCodeImplCompressed:
            case TR::java_lang_String_hashCodeImplDecompressed:
            case TR::java_lang_StringLatin1_inflate:
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

TR::Method * TR_ResolvedJ9Method::convertToMethod()                { return this; }
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

intptr_t
TR_ResolvedJ9Method::getInvocationCount()
   {
   return TR::CompilationInfo::getInvocationCount(ramMethod());
   }

bool
TR_ResolvedJ9Method::setInvocationCount(intptr_t oldCount, intptr_t newCount)
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

      uintptr_t *thisHandleLocation  = getMethodHandleLocation();
      uintptr_t *otherHandleLocation = other->getMethodHandleLocation();

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
   return (nameLength()==6 && !strncmp(nameChars(), "<init>", 6));
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
            J9UTF8 *name = J9ROMMETHOD_NAME(J9_ROM_METHOD_FROM_RAM_METHOD(method));

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
TR_ResolvedJ9Method::archetypeArgPlaceholderSlot()
   {
   TR_ASSERT(isArchetypeSpecimen(), "should not be called for non-ArchetypeSpecimen methods");
   TR_OpaqueMethodBlock * aMethod = getNonPersistentIdentifier();
   J9ROMMethod * romMethod;

      {
      TR::VMAccessCriticalSection j9method(_fe);
      romMethod = getOriginalROMMethod((J9Method *)aMethod);
      }

   J9UTF8 * signature = J9ROMMETHOD_SIGNATURE(romMethod);

   U_8 tempArgTypes[256];
   uintptr_t    paramElements;
   uintptr_t    paramSlots;
   jitParseSignature(signature, tempArgTypes, &paramElements, &paramSlots);
   /*
    * result should be : paramSlot + 1 -1 = paramSlot
    * +1 :thunk archetype are always virtual method and has a receiver
    * -1 :the placeholder is a 1-slot type (int)
    */
   return paramSlots;
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

bool
TR_ResolvedJ9Method::isInterpretedForHeuristics()
   {
   return isInterpreted();
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

   if (!TR::Compiler->target.cpu.isI386() && !(_fe->_jitConfig->runtimeFlags & J9JIT_TOSS_CODE))
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

void
TR_ResolvedJ9Method::getFaninInfo(uint32_t *count, uint32_t *weight, uint32_t *otherBucketWeight)
   {
   TR_IProfiler *profiler = fej9()->getIProfiler();

   if (!profiler)
      return;

   TR_OpaqueMethodBlock *method = getPersistentIdentifier();
   profiler->getFaninInfo(method, count, weight, otherBucketWeight);
   }

bool
TR_ResolvedJ9Method::getCallerWeight(TR_ResolvedJ9Method *caller, uint32_t *weight, uint32_t pcIndex)
   {
   TR_IProfiler *profiler = fej9()->getIProfiler();

   if (!profiler)
      return false;

   return profiler->getCallerWeight(getPersistentIdentifier(), caller->getPersistentIdentifier(), weight, pcIndex);
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

TR_PersistentJittedBodyInfo *
TR_ResolvedJ9Method::getExistingJittedBodyInfo()
   {
   void *methodAddress = startAddressForInterpreterOfJittedMethod();
   return TR::Recompilation::getJittedBodyInfoFromPC(methodAddress);
   }

int32_t
TR_ResolvedJ9Method::virtualCallSelector(U_32 cpIndex)
   {
   return -(int32_t)(vTableSlot(cpIndex) - TR::Compiler->vm.getInterpreterVTableOffset());
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

      default:
         break;
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

bool
TR_J9MethodBase::isUnsafeGetPutBoolean(TR::RecognizedMethod rm)
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
TR_J9MethodBase::isUnsafePut(TR::RecognizedMethod rm)
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
TR_J9MethodBase::isSignaturePolymorphicMethod(TR::Compilation * comp)
   {
   if (isVarHandleAccessMethod(comp)) return true;

   TR::RecognizedMethod rm = getMandatoryRecognizedMethod();
   switch (rm)
      {
      case TR::java_lang_invoke_MethodHandle_invoke:
      case TR::java_lang_invoke_MethodHandle_invokeBasic:
      case TR::java_lang_invoke_MethodHandle_invokeExact:
      case TR::java_lang_invoke_MethodHandle_linkToStatic:
      case TR::java_lang_invoke_MethodHandle_linkToSpecial:
      case TR::java_lang_invoke_MethodHandle_linkToVirtual:
      case TR::java_lang_invoke_MethodHandle_linkToInterface:
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
   eTbl = (J9JITExceptionTable *)_fe->allocateDataCacheRecord(numBytes, comp, _fe->needsContiguousCodeAndDataCacheAllocation(), &shouldRetryAllocation,
                                                              J9_JIT_DCE_EXCEPTION_INFO, &size);
   if (!eTbl)
      {
      if (shouldRetryAllocation)
         {
         comp->failCompilation<J9::RecoverableDataCacheError>("Failed to allocate exception table");
         }
      comp->failCompilation<J9::DataCacheError>("Failed to allocate exception table");
      }
   memset((uint8_t *)eTbl, 0, size);

   eTbl->className       = J9ROMCLASS_CLASSNAME(romClassPtr());
   eTbl->methodName      = J9ROMMETHOD_NAME(romMethod());
   eTbl->methodSignature = J9ROMMETHOD_SIGNATURE(romMethod());

   J9ConstantPool *cpool;
   if (isNewInstanceImplThunk())
      {
      TR_ASSERT(_j9classForNewInstance, "Must have the class for the newInstance");
      //J9Class *clazz = (J9Class*) ((intptr_t)_ramMethod->extra & ~J9_STARTPC_NOT_TRANSLATED);

      // Primitives and arrays don't have constant pool, use the constant pool of java/lang/Class
      if (TR::Compiler->cls.isPrimitiveClass(comp, (TR_OpaqueClassBlock*)_j9classForNewInstance) ||
          TR::Compiler->cls.isClassArray(comp, (TR_OpaqueClassBlock*)_j9classForNewInstance))
         cpool = cp();
      else
         cpool = (J9ConstantPool *) fej9()->getConstantPoolFromClass((TR_OpaqueClassBlock *) _j9classForNewInstance);
      }
   else
      cpool = cp();

   TR_ASSERT(cpool, "Constant pool cannot be null");

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
TR_ResolvedJ9Method::getClassOfStaticFromCP(TR_J9VMBase *fej9, J9ConstantPool *cp, int32_t cpIndex)
   {
   TR::VMAccessCriticalSection classOfStatic(fej9);
   TR_OpaqueClassBlock *result;
   result = fej9->convertClassPtrToClassOffset(cpIndex >= 0 ? jitGetClassOfFieldFromCP(fej9->vmThread(), cp, cpIndex) : 0);
   return result;
   }

TR_OpaqueClassBlock *
TR_ResolvedJ9Method::classOfStatic(I_32 cpIndex, bool returnClassForAOT)
   {
   return getClassOfStaticFromCP(fej9(), cp(), cpIndex);
   }

TR_OpaqueClassBlock *
TR_ResolvedJ9Method::classOfMethod()
   {
   if (isNewInstanceImplThunk())
      {
      TR_ASSERT(_j9classForNewInstance, "Must have the class for the newInstance");
      //J9Class * clazz = (J9Class *)((intptr_t)ramMethod()->extra & ~J9_STARTPC_NOT_TRANSLATED);
      return _fe->convertClassPtrToClassOffset(_j9classForNewInstance);//(TR_OpaqueClassBlock *&)(rc);
      }

   return _fe->convertClassPtrToClassOffset(J9_CLASS_FROM_METHOD(ramMethod()));
   }

uint32_t
TR_ResolvedJ9Method::classCPIndexOfMethod(uint32_t methodCPIndex)
   {
   uint32_t realCPIndex = jitGetRealCPIndex(_fe->vmThread(), romClassPtr(), methodCPIndex);
   uint32_t classIndex = ((J9ROMMethodRef *) romCPBase())[realCPIndex].classRefCPIndex;
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


void
TR_ResolvedJ9Method::setClassForNewInstance(J9Class *c)
   {
   _j9classForNewInstance = c;
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
   UDATA cpType = J9_CP_TYPE(J9ROMCLASS_CPSHAPEDESCRIPTION(romClassPtr()), cpIndex);
   return cpType2trType(cpType);
   }

bool
TR_ResolvedJ9Method::isClassConstant(int32_t cpIndex)
   {
   UDATA cpType = J9_CP_TYPE(J9ROMCLASS_CPSHAPEDESCRIPTION(romClassPtr()), cpIndex);
   return cpType == J9CPTYPE_CLASS;
   }

bool
TR_ResolvedJ9Method::isStringConstant(int32_t cpIndex)
   {
   UDATA cpType = J9_CP_TYPE(J9ROMCLASS_CPSHAPEDESCRIPTION(romClassPtr()), cpIndex);
   return (cpType == J9CPTYPE_STRING) || (cpType == J9CPTYPE_ANNOTATION_UTF8);
   }

bool
TR_ResolvedJ9Method::isMethodTypeConstant(int32_t cpIndex)
   {
   UDATA cpType = J9_CP_TYPE(J9ROMCLASS_CPSHAPEDESCRIPTION(romClassPtr()), cpIndex);
   return cpType == J9CPTYPE_METHOD_TYPE;
   }

bool
TR_ResolvedJ9Method::isMethodHandleConstant(int32_t cpIndex)
   {
   UDATA cpType = J9_CP_TYPE(J9ROMCLASS_CPSHAPEDESCRIPTION(romClassPtr()), cpIndex);
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

void *
TR_ResolvedJ9Method::getConstantDynamicTypeFromCP(I_32 cpIndex)
   {
   return jitGetConstantDynamicTypeFromCP(fej9()->vmThread(), cp(), cpIndex);
   }

bool
TR_ResolvedJ9Method::isConstantDynamic(I_32 cpIndex)
   {
   TR_ASSERT_FATAL(cpIndex != -1, "ConstantDynamic cpIndex shouldn't be -1");
   UDATA cpType = J9_CP_TYPE(J9ROMCLASS_CPSHAPEDESCRIPTION(cp()->ramClass->romClass), cpIndex);
   return (J9CPTYPE_CONSTANT_DYNAMIC == cpType);
   }

// Both value and exception slots are of object pointer type.
// If first slot is non null, the CP entry is resolved to a non-null value.
// Else if second slot is the class object of j/l/Void, the CP entry is resolved to null (0) value.
// We retrieve the Void class object via javaVM->voidReflectClass->classObject,
// which is protected by VMAccessCritical section to ensure vm access.
// Other cases, the CP entry is considered unresolved.
bool
TR_ResolvedJ9Method::isUnresolvedConstantDynamic(I_32 cpIndex)
   {
   TR_ASSERT(cpIndex != -1, "ConstantDynamic cpIndex shouldn't be -1");
   if (((J9RAMConstantDynamicRef *) literals())[cpIndex].value != 0)
      {
      return false;
      }
   if (((J9RAMConstantDynamicRef *) literals())[cpIndex].exception == 0)
      {
      return true;
      }

   TR::VMAccessCriticalSection voidClassObjectCritSec(fej9());
   J9JavaVM * javaVM = fej9()->_jitConfig->javaVM;
   j9object_t voidClassObject = javaVM->voidReflectClass->classObject;
   j9object_t slot2 = ((J9RAMConstantDynamicRef *) literals())[cpIndex].exception;

   return (voidClassObject != slot2);
   }

void *
TR_ResolvedJ9Method::dynamicConstant(I_32 cpIndex, uintptr_t *obj)
   {
   TR_ASSERT_FATAL(cpIndex != -1, "ConstantDynamic cpIndex shouldn't be -1");
   uintptr_t *objLocation = (uintptr_t *)&(((J9RAMConstantDynamicRef *) literals())[cpIndex].value);
   if (obj)
      {
      *obj = *objLocation;
      }
   return objLocation;
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
#if defined(J9VM_OPT_JITSERVER)
   TR_ASSERT(!TR::CompilationInfo::getStream(), "no server");
#endif /* defined(J9VM_OPT_JITSERVER) */
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
   J9RAMMethodTypeRef *ramMethodTypeRef = (J9RAMMethodTypeRef *)(literals() + cpIndex);
   return &ramMethodTypeRef->type;
   }

bool
TR_ResolvedJ9Method::isUnresolvedMethodType(I_32 cpIndex)
   {
#if defined(J9VM_OPT_JITSERVER)
   TR_ASSERT(!TR::CompilationInfo::getStream(), "no server");
#endif /* defined(J9VM_OPT_JITSERVER) */
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
   return *(intptr_t*)methodTypeConstant(cpIndex) == 0;
   }

void *
TR_ResolvedJ9Method::methodHandleConstant(I_32 cpIndex)
   {
#if defined(J9VM_OPT_JITSERVER)
   TR_ASSERT(!TR::CompilationInfo::getStream(), "no server");
#endif /* defined(J9VM_OPT_JITSERVER) */
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
   J9RAMMethodHandleRef *ramMethodHandleRef = (J9RAMMethodHandleRef *)(literals() + cpIndex);
   return &ramMethodHandleRef->methodHandle;
   }

bool
TR_ResolvedJ9Method::isUnresolvedMethodHandle(I_32 cpIndex)
   {
#if defined(J9VM_OPT_JITSERVER)
   TR_ASSERT(!TR::CompilationInfo::getStream(), "no server");
#endif /* defined(J9VM_OPT_JITSERVER) */
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
   return *(intptr_t*)methodHandleConstant(cpIndex) == 0;
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

#if defined(J9VM_OPT_METHOD_HANDLE)
void *
TR_ResolvedJ9Method::varHandleMethodTypeTableEntryAddress(int32_t cpIndex)
   {
#if defined(J9VM_OPT_JITSERVER)
   TR_ASSERT(!TR::CompilationInfo::getStream(), "no server");
#endif /* defined(J9VM_OPT_JITSERVER) */
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

bool
TR_ResolvedJ9Method::isUnresolvedVarHandleMethodTypeTableEntry(int32_t cpIndex)
   {
   return *(j9object_t*)varHandleMethodTypeTableEntryAddress(cpIndex) == NULL;
   }
#endif /* defined(J9VM_OPT_METHOD_HANDLE) */

void *
TR_ResolvedJ9Method::methodTypeTableEntryAddress(int32_t cpIndex)
   {
   J9Class *ramClass = constantPoolHdr();
   UDATA index = (((J9RAMMethodRef*) literals())[cpIndex]).methodIndexAndArgCount >> 8;
#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
   return ramClass->invokeCache + index;
#else
   return ramClass->methodTypes + index;
#endif
   }

bool
TR_ResolvedJ9Method::isUnresolvedMethodTypeTableEntry(int32_t cpIndex)
   {
   return *(j9object_t*)methodTypeTableEntryAddress(cpIndex) == NULL;
   }

TR_OpaqueClassBlock *
TR_ResolvedJ9Method::getClassFromCP(TR_J9VMBase *fej9, J9ConstantPool *cp, TR::Compilation *comp, uint32_t cpIndex)
   {
   TR::VMAccessCriticalSection getClassFromConstantPool(fej9);
   TR_OpaqueClassBlock *result = 0;
   J9Class * resolvedClass;
   if (cpIndex != -1 &&
       !((fej9->_jitConfig->runtimeFlags & J9JIT_RUNTIME_RESOLVE) &&
         !comp->ilGenRequest().details().isMethodHandleThunk() && // cmvc 195373
         performTransformation(comp, "Setting as unresolved class from CP cpIndex=%d\n",cpIndex) )&&
       (resolvedClass = fej9->_vmFunctionTable->resolveClassRef(fej9->vmThread(), cp, cpIndex, J9_RESOLVE_FLAG_JIT_COMPILE_TIME)))
      {
      result = fej9->convertClassPtrToClassOffset(resolvedClass);
      }

   return result;
   }

TR_OpaqueClassBlock *
TR_ResolvedJ9Method::getClassFromConstantPool(TR::Compilation * comp, uint32_t cpIndex, bool)
   {
   return getClassFromCP(fej9(), cp(), comp, cpIndex);
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

char *
TR_ResolvedJ9Method::getMethodSignatureFromConstantPool(I_32 cpIndex, int32_t & len)
   {
   I_32 realCPIndex = jitGetRealCPIndex(_fe->vmThread(), romClassPtr(), cpIndex);
   if (realCPIndex == -1)
      return 0;
   J9ROMMethodRef *romMethodRef = (J9ROMMethodRef *) (&romCPBase()[realCPIndex]);
   J9ROMNameAndSignature *nameAndSig = J9ROMMETHODREF_NAMEANDSIGNATURE(romMethodRef);
   return utf8Data(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig), len);
   }

char *
TR_ResolvedJ9Method::getMethodNameFromConstantPool(int32_t cpIndex, int32_t & len)
   {
   I_32 realCPIndex = jitGetRealCPIndex(_fe->vmThread(), romClassPtr(), cpIndex);
   if (realCPIndex == -1)
      return 0;
   J9ROMMethodRef *romMethodRef = (J9ROMMethodRef *) (&romCPBase()[realCPIndex]);
   J9ROMNameAndSignature *nameAndSig = J9ROMMETHODREF_NAMEANDSIGNATURE(romMethodRef);
   return utf8Data(J9ROMNAMEANDSIGNATURE_NAME(nameAndSig), len);
   }

const char *
TR_ResolvedJ9Method::newInstancePrototypeSignature(TR_Memory * m, TR_AllocationKind allocKind)
   {
   int32_t  clen;
   TR_ASSERT(_j9classForNewInstance, "Must have the class for newInstance");
   J9Class * clazz = _j9classForNewInstance; //((J9Class*)((uintptr_t)(ramMethod()->extra) & ~J9_STARTPC_NOT_TRANSLATED);
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
      return (void*)((intptr_t)address & ~J9_STARTPC_NOT_TRANSLATED);

   return *(void * *)(TR::CompilationInfo::getJ9MethodExtra(ramMethod()) - (comp->target().is64Bit() ? 12 : 8));
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

   magic = (J9JNIMethodID *) javaVM->nativeLibrariesLoadMethodID;
   if (magic && ramMethod() == magic->method) {
      return false;
   }

   return true;
   }

bool
TR_ResolvedJ9Method::isInvokePrivateVTableOffset(UDATA vTableOffset)
   {
   return vTableOffset == J9VTABLE_INVOKE_PRIVATE_OFFSET;
   }

TR_OpaqueMethodBlock *
TR_ResolvedJ9Method::getVirtualMethod(TR_J9VMBase *fej9, J9ConstantPool *cp, I_32 cpIndex, UDATA *vTableOffset, bool *unresolvedInCP)
   {
   J9RAMConstantPoolItem *literals = (J9RAMConstantPoolItem *)cp;
   J9Method * ramMethod = NULL;

   *vTableOffset = (((J9RAMVirtualMethodRef*) literals)[cpIndex]).methodIndexAndArgCount;
   *vTableOffset >>= 8;
   if (J9VTABLE_INITIAL_VIRTUAL_OFFSET == *vTableOffset)
      {
      if (unresolvedInCP)
         *unresolvedInCP = true;
      TR::VMAccessCriticalSection resolveVirtualMethodRef(fej9);
      *vTableOffset = fej9->_vmFunctionTable->resolveVirtualMethodRefInto(fej9->vmThread(), cp, cpIndex, J9_RESOLVE_FLAG_JIT_COMPILE_TIME, &ramMethod, NULL);
      }
   else
      {
      if (unresolvedInCP)
         *unresolvedInCP = false;

      if (!isInvokePrivateVTableOffset(*vTableOffset))
         {
         // go fishing for the J9Method...
         uint32_t classIndex = ((J9ROMMethodRef *) cp->romConstantPool)[cpIndex].classRefCPIndex;
         J9Class * classObject = (((J9RAMClassRef*) literals)[classIndex]).value;
         ramMethod = *(J9Method **)((char *)classObject + *vTableOffset);
         }
      }

   if (isInvokePrivateVTableOffset(*vTableOffset))
      ramMethod = (((J9RAMVirtualMethodRef*) literals)[cpIndex]).method;

   return (TR_OpaqueMethodBlock *)ramMethod;
   }

TR_OpaqueClassBlock *
TR_ResolvedJ9Method::getInterfaceITableIndexFromCP(TR_J9VMBase *fej9, J9ConstantPool *cp, int32_t cpIndex, UDATA *pITableIndex)
   {
   if (cpIndex == -1)
      return NULL;

   TR::VMAccessCriticalSection getResolvedInterfaceMethod(fej9);
   return (TR_OpaqueClassBlock *)jitGetInterfaceITableIndexFromCP(fej9->vmThread(), cp, cpIndex, pITableIndex);
   }

TR_OpaqueClassBlock *
TR_ResolvedJ9Method::getResolvedInterfaceMethod(I_32 cpIndex, UDATA *pITableIndex)
   {
   TR_OpaqueClassBlock *result;
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");

#if TURN_OFF_INLINING
   return 0;
#else

   result = getInterfaceITableIndexFromCP(fej9(), cp(), cpIndex, pITableIndex);

   TR::Compilation *comp = TR::comp();
   if (comp && comp->compileRelocatableCode() && comp->getOption(TR_UseSymbolValidationManager))
      {
      if (!comp->getSymbolValidationManager()->addClassFromITableIndexCPRecord(result, cp(), cpIndex))
         result = NULL;
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
   UDATA vTableOffset = 0;
#if TURN_OFF_INLINING
   return 0;
#else
      {
      TR::VMAccessCriticalSection getResolvedInterfaceMethodOffset(fej9());
      vTableOffset = jitGetInterfaceVTableOffsetFromCP(_fe->vmThread(), cp(), cpIndex, TR::Compiler->cls.convertClassOffsetToClassPtr(classObject));
      }

   return (TR::Compiler->vm.getInterpreterVTableOffset() - vTableOffset);
#endif
   }

/* Only returns non-null if the method is not to be dispatched by itable, i.e.
 * if it is:
 * - private (isPrivate()), using direct dispatch;
 * - a final method of Object (isFinalInObject()), using direct dispatch; or
 * - a non-final method of Object, using virtual dispatch.
 */
TR_ResolvedMethod *
TR_ResolvedJ9Method::getResolvedImproperInterfaceMethod(TR::Compilation * comp, I_32 cpIndex)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
#if TURN_OFF_INLINING
   return 0;
#else
   J9Method *j9method = NULL;
   UDATA vtableOffset = 0;
   if ((_fe->_jitConfig->runtimeFlags & J9JIT_RUNTIME_RESOLVE) == 0)
      {
      TR::VMAccessCriticalSection getResolvedPrivateInterfaceMethodOffset(fej9());
      j9method = jitGetImproperInterfaceMethodFromCP(_fe->vmThread(), cp(), cpIndex, &vtableOffset);
      }

   if (comp->getOption(TR_UseSymbolValidationManager) && j9method)
      {
      if (!comp->getSymbolValidationManager()->addImproperInterfaceMethodFromCPRecord((TR_OpaqueMethodBlock *)j9method, cp(), cpIndex))
         j9method = NULL;
      }

   if (j9method == NULL)
      return NULL;
   else
      return createResolvedMethodFromJ9Method(comp, cpIndex, (uint32_t)vtableOffset, j9method, NULL, NULL);
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

   if (comp->compileRelocatableCode() && comp->getOption(TR_UseSymbolValidationManager) && ramMethod)
      {
      if (!comp->getSymbolValidationManager()->addStaticMethodFromCPRecord((TR_OpaqueMethodBlock *)ramMethod, cp(), cpIndex))
         ramMethod = NULL;
      }

   bool skipForDebugging = doResolveAtRuntime(ramMethod, cpIndex, comp);
   if (isArchetypeSpecimen())
      {
      // ILGen macros currently must be resolved for correctness, or else they
      // are not recognized and expanded.  If we have unresolved calls, we can't
      // tell whether they're ilgen macros because the recognized-method system
      // only works on resolved methods.
      //
      if (ramMethod)
         skipForDebugging = false;
      else
         {
         comp->failCompilation<TR::ILGenFailure>("Can't compile an archetype specimen with unresolved calls");
         }
      }

   // With rtResolve option, INL calls that are required to be compile time
   // resolved are left as unresolved.
   if (shouldCompileTimeResolveMethod(cpIndex))
      skipForDebugging = false;


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

   if (unresolvedInCP != NULL)
      {
      *unresolvedInCP = true;
      }

   if (!((_fe->_jitConfig->runtimeFlags & J9JIT_RUNTIME_RESOLVE) &&
         !comp->ilGenRequest().details().isMethodHandleThunk() && // cmvc 195373
         performTransformation(comp, "Setting as unresolved special call cpIndex=%d\n",cpIndex) ))
      {
      TR::VMAccessCriticalSection resolveSpecialMethodRef(fej9());

      J9Method * ramMethod = jitResolveSpecialMethodRef(_fe->vmThread(), cp(), cpIndex, J9_RESOLVE_FLAG_JIT_COMPILE_TIME);
      if (ramMethod)
         {
         bool createResolvedMethod = true;

         if (comp->getOption(TR_UseSymbolValidationManager))
            {
            if (!comp->getSymbolValidationManager()->addSpecialMethodFromCPRecord((TR_OpaqueMethodBlock *)ramMethod, cp(), cpIndex))
               createResolvedMethod = false;
            }

         TR_AOTInliningStats *aotStats = NULL;
         if (comp->getOption(TR_EnableAOTStats))
            aotStats = & (((TR_JitPrivateConfig *)_fe->_jitConfig->privateConfig)->aotStats->specialMethods);
         if (createResolvedMethod)
            resolvedMethod = createResolvedMethodFromJ9Method(comp, cpIndex, 0, ramMethod, unresolvedInCP, aotStats);
         if (unresolvedInCP)
            *unresolvedInCP = false;
         }
      }

   if (resolvedMethod == NULL)
      {
      if (unresolvedInCP)
         handleUnresolvedSpecialMethodInCP(cpIndex, unresolvedInCP);
      }

#endif

   return resolvedMethod;
   }

bool
TR_ResolvedJ9Method::shouldCompileTimeResolveMethod(I_32 cpIndex)
   {
   int32_t methodNameLength;
   char *methodName = getMethodNameFromConstantPool(cpIndex, methodNameLength);

   I_32 classCPIndex = classCPIndexOfMethod(cpIndex);
   uint32_t classNameLength;
   char *className = getClassNameFromConstantPool(classCPIndex, classNameLength);

   if (classNameLength == strlen(JSR292_MethodHandle) &&
       !strncmp(className, JSR292_MethodHandle, classNameLength))
      {
      // MethodHandle.invokeBasic and MethodHandle.linkTo* methods have to be
      // resolved to be recognized for special handling in tree
      // lowering. Their resolution has no side effect and should always succeed.
      //
      if ((methodNameLength == 11 &&
            !strncmp(methodName, "invokeBasic", methodNameLength)) ||
         (methodNameLength == 12 &&
            !strncmp(methodName, "linkToStatic", methodNameLength)) ||
         (methodNameLength == 13 &&
            (!strncmp(methodName, "linkToSpecial", methodNameLength) ||
             !strncmp(methodName, "linkToVirtual", methodNameLength))) ||
         (methodNameLength == 15 &&
            !strncmp(methodName, "linkToInterface", methodNameLength)))
         return true;
      }

   return false;
   }

TR_ResolvedMethod *
TR_ResolvedJ9Method::getResolvedPossiblyPrivateVirtualMethod(TR::Compilation * comp, I_32 cpIndex, bool ignoreRtResolve, bool * unresolvedInCP)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
#if TURN_OFF_INLINING
   return 0;
#else
   TR_ResolvedMethod *resolvedMethod = NULL;

   bool shouldCompileTimeResolve = shouldCompileTimeResolveMethod(cpIndex);

   // See if the constant pool entry is already resolved or not
   //
   if (unresolvedInCP)
       *unresolvedInCP = true;

   if (!((_fe->_jitConfig->runtimeFlags & J9JIT_RUNTIME_RESOLVE) &&
         !comp->ilGenRequest().details().isMethodHandleThunk() && // cmvc 195373
         performTransformation(comp, "Setting as unresolved virtual call cpIndex=%d\n",cpIndex) ) || ignoreRtResolve || shouldCompileTimeResolve)
      {
      // only call the resolve if unresolved
      UDATA vTableOffset = 0;
      J9Method * ramMethod = (J9Method *)getVirtualMethod(_fe, cp(), cpIndex, &vTableOffset, unresolvedInCP);
      bool createResolvedMethod = true;

      if (comp->compileRelocatableCode() && ramMethod && comp->getOption(TR_UseSymbolValidationManager))
         {
         if (!comp->getSymbolValidationManager()->addVirtualMethodFromCPRecord((TR_OpaqueMethodBlock *)ramMethod, cp(), cpIndex))
            createResolvedMethod = false;
         }

      if (vTableOffset)
         {
         TR_AOTInliningStats *aotStats = NULL;
         if (comp->getOption(TR_EnableAOTStats))
            aotStats = & (((TR_JitPrivateConfig *)_fe->_jitConfig->privateConfig)->aotStats->virtualMethods);
         if (createResolvedMethod)
            resolvedMethod = createResolvedMethodFromJ9Method(comp, cpIndex, vTableOffset, ramMethod, unresolvedInCP, aotStats);
         }

      }

   TR_ASSERT_FATAL(resolvedMethod || !shouldCompileTimeResolve, "Method has to be resolved in %s at cpIndex  %d", signature(comp->trMemory()), cpIndex);

   if (resolvedMethod == NULL)
      {
      TR::DebugCounter::incStaticDebugCounter(comp, "resources.resolvedMethods/virtual/null");
      if (unresolvedInCP)
         handleUnresolvedVirtualMethodInCP(cpIndex, unresolvedInCP);
      }
   else
      {
      TR::DebugCounter::incStaticDebugCounter(comp, "resources.resolvedMethods/virtual");
      TR::DebugCounter::incStaticDebugCounter(comp, "resources.resolvedMethods/virtual:#bytes", sizeof(TR_ResolvedJ9Method));
      }

   return resolvedMethod;
#endif
   }

TR_ResolvedMethod *
TR_ResolvedJ9Method::getResolvedVirtualMethod(
   TR::Compilation * comp,
   I_32 cpIndex,
   bool ignoreRtResolve,
   bool * unresolvedInCP)
   {
   TR_ResolvedMethod *method = getResolvedPossiblyPrivateVirtualMethod(
      comp,
      cpIndex,
      ignoreRtResolve,
      unresolvedInCP);

   return (method == NULL || method->isPrivate()) ? NULL : method;
   }

TR_ResolvedMethod *
TR_ResolvedJ9Method::createResolvedMethodFromJ9Method(TR::Compilation *comp, I_32 cpIndex, uint32_t vTableSlot, J9Method *j9Method, bool * unresolvedInCP, TR_AOTInliningStats *aotStats)
   {
   TR_ResolvedMethod *m = new (comp->trHeapMemory()) TR_ResolvedJ9Method((TR_OpaqueMethodBlock *) j9Method, _fe, comp->trMemory(), this, vTableSlot);

   if (((TR_ResolvedJ9Method*)m)->isSignaturePolymorphicMethod())
      {
      // Signature polymorphic method's signature varies at different call sites and will be different than its declared signature
      J9ROMMethodRef *romMethodRef = (J9ROMMethodRef *)(cp()->romConstantPool + cpIndex);
      J9ROMNameAndSignature *nameAndSig = J9ROMMETHODREF_NAMEANDSIGNATURE(romMethodRef);
      int32_t signatureLength;
      char   *signature = utf8Data(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig), signatureLength);
      ((TR_ResolvedJ9Method *)m)->setSignature(signature, signatureLength, comp->trMemory());
      }

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
TR_ResolvedJ9Method::getResolvedDynamicMethod(TR::Compilation * comp, I_32 callSiteIndex, bool * unresolvedInCP, bool * isInvokeCacheAppendixNull)
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

   J9Class    *ramClass = constantPoolHdr();
   J9ROMClass *romClass = romClassPtr();
   bool isUnresolvedEntry = isUnresolvedCallSiteTableEntry(callSiteIndex);
   if (unresolvedInCP)
      {
      // "unresolvedInCP" is a bit of a misnomer here, but we can describe
      // something equivalent by checking the callSites table.
      //
      *unresolvedInCP = isUnresolvedEntry;
      }

   J9SRP                 *namesAndSigs = (J9SRP*)J9ROMCLASS_CALLSITEDATA(romClass);
   J9ROMNameAndSignature *nameAndSig   = NNSRP_GET(namesAndSigs[callSiteIndex], J9ROMNameAndSignature*);
   J9UTF8                *signature    = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig);

#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
   if (isInvokeCacheAppendixNull)
      *isInvokeCacheAppendixNull = false;

   if (!isUnresolvedEntry)
      {
      TR_OpaqueMethodBlock * targetJ9MethodBlock = NULL;
      uintptr_t * invokeCacheArray = (uintptr_t *) callSiteTableEntryAddress(callSiteIndex);
         {
         TR::VMAccessCriticalSection getResolvedDynamicMethod(fej9());
         targetJ9MethodBlock = fej9()->targetMethodFromMemberName((uintptr_t) fej9()->getReferenceElement(*invokeCacheArray, JSR292_invokeCacheArrayMemberNameIndex)); // this will not work in AOT or JITServer
         // if the callSite table entry is resolved, we can check if the appendix object is null,
         // in which case the appendix object must not be pushed to stack
         uintptr_t appendixObject = (uintptr_t) fej9()->getReferenceElement(*invokeCacheArray, JSR292_invokeCacheArrayAppendixIndex);
         if (isInvokeCacheAppendixNull && !appendixObject) *isInvokeCacheAppendixNull = true;
         }
      result = fej9()->createResolvedMethod(comp->trMemory(), targetJ9MethodBlock, this);
      }
   else
      {
      // Even when invokedynamic is unresolved, we construct a resolved method and rely on
      // the call site table entry to be resolved instead. The resolved method we construct is
      // linkToStatic, which is a VM internal native method that will prepare the call frame for
      // the actual method to be invoked using the last argument we push to stack (memberName object)
      TR_OpaqueMethodBlock *dummyInvoke = _fe->getMethodFromName("java/lang/invoke/MethodHandle", "linkToStatic", "([Ljava/lang/Object;)Ljava/lang/Object;");
      int32_t signatureLength;
      char * linkToStaticSignature = _fe->getSignatureForLinkToStaticForInvokeDynamic(comp, signature, signatureLength);
      result = _fe->createResolvedMethodWithSignature(comp->trMemory(), dummyInvoke, NULL, linkToStaticSignature, signatureLength, this);
      }
#else
   TR_OpaqueMethodBlock *dummyInvokeExact = _fe->getMethodFromName("java/lang/invoke/MethodHandle", "invokeExact", JSR292_invokeExactSig);
   result = _fe->createResolvedMethodWithSignature(comp->trMemory(), dummyInvokeExact, NULL, utf8Data(signature), J9UTF8_LENGTH(signature), this);
#endif /* J9VM_OPT_OPENJDK_METHODHANDLE */
   return result;
#endif /* TURN_OFF_INLINING */
   }

TR_ResolvedMethod *
TR_ResolvedJ9Method::getResolvedHandleMethod(TR::Compilation * comp, I_32 cpIndex, bool * unresolvedInCP, bool * isInvokeCacheAppendixNull)
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

   bool isUnresolvedEntry = isUnresolvedMethodTypeTableEntry(cpIndex);
   if (unresolvedInCP)
      *unresolvedInCP = isUnresolvedEntry;

   J9ROMMethodRef *romMethodRef = (J9ROMMethodRef *)(cp()->romConstantPool + cpIndex);
   J9ROMNameAndSignature *nameAndSig = J9ROMMETHODREF_NAMEANDSIGNATURE(romMethodRef);

#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
   J9UTF8                *signature    = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig);

   if (isInvokeCacheAppendixNull)
      *isInvokeCacheAppendixNull = false;

   if (!isUnresolvedEntry)
      {
      uintptr_t * invokeCacheArray = (uintptr_t *) methodTypeTableEntryAddress(cpIndex);
      TR_OpaqueMethodBlock * targetJ9MethodBlock = NULL;
         {
         TR::VMAccessCriticalSection getResolvedHandleMethod(fej9());
         targetJ9MethodBlock = fej9()->targetMethodFromMemberName((uintptr_t) fej9()->getReferenceElement(*invokeCacheArray, JSR292_invokeCacheArrayMemberNameIndex)); // this will not work in AOT or JITServer
         uintptr_t appendixObject = (uintptr_t) fej9()->getReferenceElement(*invokeCacheArray, JSR292_invokeCacheArrayAppendixIndex);
         if (isInvokeCacheAppendixNull && !appendixObject) *isInvokeCacheAppendixNull = true;
         }
      result = fej9()->createResolvedMethod(comp->trMemory(), targetJ9MethodBlock, this);
      }
   else
      {
      // Even when invokehandle is unresolved, we construct a resolved method and rely on
      // the method type table entry to be resolved instead. The resolved method we construct is
      // linkToStatic, which is a VM internal native method that will prepare the call frame for
      // the actual method to be invoked using the last argument we push to stack (memberName object)
      TR_OpaqueMethodBlock *dummyInvoke = _fe->getMethodFromName("java/lang/invoke/MethodHandle", "linkToStatic", "([Ljava/lang/Object;)Ljava/lang/Object;");
      int32_t signatureLength;
      char * linkToStaticSignature = _fe->getSignatureForLinkToStaticForInvokeHandle(comp, signature, signatureLength);
      result = _fe->createResolvedMethodWithSignature(comp->trMemory(), dummyInvoke, NULL, linkToStaticSignature, signatureLength, this);
      }
#else

   TR_OpaqueMethodBlock *dummyInvokeExact = _fe->getMethodFromName("java/lang/invoke/MethodHandle", "invokeExact", JSR292_invokeExactSig);

   int32_t signatureLength;
   char   *signature = utf8Data(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig), signatureLength);
   result = _fe->createResolvedMethodWithSignature(comp->trMemory(), dummyInvokeExact, NULL, signature, signatureLength, this);
#endif /* J9VM_OPT_OPENJDK_METHODHANDLE */
   return result;
#endif /* TURN_OFF_INLINING */
   }

TR_ResolvedMethod *
TR_ResolvedJ9Method::getResolvedHandleMethodWithSignature(TR::Compilation * comp, I_32 cpIndex, char *signature)
   {
#if TURN_OFF_INLINING
   return 0;
#else
   // TODO:JSR292: Dummy would be unnecessary if we could create a TR_ResolvedJ9Method without a j9method
   TR_OpaqueMethodBlock *dummyInvokeExact = _fe->getMethodFromName("java/lang/invoke/MethodHandle", "invokeExact", JSR292_invokeExactSig);
   TR_ResolvedMethod    *resolvedMethod   = _fe->createResolvedMethodWithSignature(comp->trMemory(), dummyInvokeExact, NULL, signature, strlen(signature), this);
   return resolvedMethod;
#endif
   }

TR_ResolvedMethod *
TR_ResolvedJ9Method::getResolvedVirtualMethod(TR::Compilation * comp, TR_OpaqueClassBlock * classObject, I_32 virtualCallOffset, bool ignoreRtResolve)
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

   // See if the constant pool entry is already resolved or not
   //
   bool isUnresolvedInCP = !J9RAMFIELDREF_IS_RESOLVED(((J9RAMFieldRef*)cp()) + cpIndex);
   if (unresolvedInCP)
      *unresolvedInCP = isUnresolvedInCP;

   bool isColdOrReducedWarm = (comp->getMethodHotness() < warm) || (comp->getMethodHotness() == warm && comp->getOption(TR_NoOptServer));

   //Instance fields in MethodHandle thunks should be resolved at compile time
   bool isMethodHandleThunk = comp->ilGenRequest().details().isMethodHandleThunk() || this->isArchetypeSpecimen();
   bool doRuntimeResolveForEarlyCompilation = isUnresolvedInCP && isColdOrReducedWarm && !isMethodHandleThunk;

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
         isMethodHandleThunk || // cmvc 195373
        !performTransformation(comp, "Setting as unresolved field attributes cpIndex=%d\n",cpIndex)))
      {
      resolved = true;
      ltype = fieldShape->modifiers;
      //ltype = (((J9RAMFieldRef*) literals())[cpIndex]).flags;
      *volatileP = (ltype & J9AccVolatile) ? true : false;
      *fieldOffset = offset + TR::Compiler->om.objectHeaderSizeInBytes();  // add header size

      if (isFinal) *isFinal = (ltype & J9AccFinal) ? true : false;
      if (isPrivate) *isPrivate = (ltype & J9AccPrivate) ? true : false;
      }
   else
      {
      resolved = false;

         {
         TR::VMAccessCriticalSection getFieldType(fej9());
         ltype = (jitGetFieldType) (cpIndex, ramMethod());     // get callers definition of the field type
         }

      *volatileP = true;                              // assume worst case, necessary?
      *fieldOffset = TR::Compiler->om.objectHeaderSizeInBytes();
      ltype = ltype << 16;
      if (isFinal) *isFinal = false;
      if (isPrivate) *isPrivate = false;
      }

   *type = decodeType(ltype);
   return resolved;
   }

bool
TR_ResolvedJ9Method::staticAttributes(TR::Compilation * comp, I_32 cpIndex, void * * address, TR::DataType * type, bool * volatileP, bool * isFinal, bool * isPrivate, bool isStore, bool * unresolvedInCP, bool needAOTValidation)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");

   // See if the constant pool entry is already resolved or not
   //
   bool isUnresolvedInCP = !J9RAMSTATICFIELDREF_IS_RESOLVED(((J9RAMStaticFieldRef*)cp()) + cpIndex);
   if (unresolvedInCP)
       *unresolvedInCP = isUnresolvedInCP;

   bool isColdOrReducedWarm = (comp->getMethodHotness() < warm) || (comp->getMethodHotness() == warm && comp->getOption(TR_NoOptServer));
   bool isMethodHandleThunk = comp->ilGenRequest().details().isMethodHandleThunk() || this->isArchetypeSpecimen();
   bool doRuntimeResolveForEarlyCompilation = isUnresolvedInCP && isColdOrReducedWarm && !isMethodHandleThunk;

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
        isMethodHandleThunk || // cmvc 195373
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
      resolved = false;
      *volatileP = true;
      if (isFinal) *isFinal = false;
      if (isPrivate) *isPrivate = false;

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

TR_OpaqueClassBlock *
TR_ResolvedJ9Method::definingClassFromCPFieldRef(
   TR::Compilation *comp,
   J9ConstantPool *constantPool,
   I_32 cpIndex,
   bool isStatic)
   {
#if defined(J9VM_OPT_JITSERVER)
   TR_ASSERT_FATAL(!comp->isOutOfProcessCompilation(), "Static version of definingClassFromCPFieldRef should not be called in JITServer mode");
#endif
   J9ROMFieldShape *field;
   return definingClassAndFieldShapeFromCPFieldRef(comp, constantPool, cpIndex, isStatic, &field);
   }

TR_OpaqueClassBlock *
TR_ResolvedJ9Method::definingClassAndFieldShapeFromCPFieldRef(TR::Compilation *comp, J9ConstantPool *constantPool, I_32 cpIndex, bool isStatic, J9ROMFieldShape **field)
   {
   J9VMThread *vmThread = comp->j9VMThread();
   J9JavaVM *javaVM = vmThread->javaVM;
   J9JITConfig *jitConfig = javaVM->jitConfig;
   TR_J9VMBase *fe = TR_J9VMBase::get(jitConfig, vmThread);
   TR::VMAccessCriticalSection definingClassAndFieldShapeFromCPFieldRef(fe);

   /* Get the class.  Stop immediately if an exception occurs. */
   J9ROMFieldRef *romFieldRef = (J9ROMFieldRef *)&constantPool->romConstantPool[cpIndex];

   J9Class *resolvedClass = javaVM->internalVMFunctions->resolveClassRef(vmThread, constantPool, romFieldRef->classRefCPIndex, J9_RESOLVE_FLAG_JIT_COMPILE_TIME);

   if (resolvedClass == NULL)
      return NULL;

   J9Class *classFromCP = J9_CLASS_FROM_CP(constantPool);
   J9Class *definingClass = NULL;
   J9ROMNameAndSignature *nameAndSig;
   J9UTF8 *name;
   J9UTF8 *signature;

   nameAndSig = J9ROMFIELDREF_NAMEANDSIGNATURE(romFieldRef);
   name = J9ROMNAMEANDSIGNATURE_NAME(nameAndSig);
   signature = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig);
   if (isStatic)
      {
      void *staticAddress = javaVM->internalVMFunctions->staticFieldAddress(vmThread, resolvedClass, J9UTF8_DATA(name), J9UTF8_LENGTH(name), J9UTF8_DATA(signature), J9UTF8_LENGTH(signature), &definingClass, (UDATA *)field, J9_LOOK_NO_JAVA, classFromCP);
      }
   else
      {
      IDATA fieldOffset = javaVM->internalVMFunctions->instanceFieldOffset(vmThread, resolvedClass, J9UTF8_DATA(name), J9UTF8_LENGTH(name), J9UTF8_DATA(signature), J9UTF8_LENGTH(signature), &definingClass, (UDATA *)field, J9_LOOK_NO_JAVA);
      }

   return (TR_OpaqueClassBlock *)definingClass;
   }

TR_OpaqueClassBlock *
TR_ResolvedJ9Method::definingClassFromCPFieldRef(
   TR::Compilation *comp,
   I_32 cpIndex,
   bool isStatic,
   TR_OpaqueClassBlock** fromResolvedJ9Method)
   {
   auto result = definingClassFromCPFieldRef(comp, cp(), cpIndex, isStatic);
   if (fromResolvedJ9Method != NULL) {
      *fromResolvedJ9Method = result;
   }
   return result;
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


void
TR_ResolvedJ9Method::makeParameterList(TR::ResolvedMethodSymbol *methodSym)
   {
   if (methodSym->getTempIndex() != -1)
      return;

   const char *className    = classNameChars();
   const int   classNameLen = classNameLength();
   const char *sig          = signatureChars();
   const int   sigLen       = signatureLength();
   const char *sigEnd       = sig + sigLen;

   ListAppender<TR::ParameterSymbol> la(&methodSym->getParameterList());
   TR::ParameterSymbol *parmSymbol;
   int32_t slot;
   int32_t ordinal = 0;
   if (methodSym->isStatic())
      {
      slot = 0;
      }
   else
      {
      TR::KnownObjectTable::Index knownObjectIndex = methodSym->getKnownObjectIndexForParm(0);
      parmSymbol = methodSym->comp()->getSymRefTab()->createParameterSymbol(methodSym, 0, TR::Address, knownObjectIndex);
      parmSymbol->setOrdinal(ordinal++);

      int32_t len = classNameLen; // len is passed by reference and changes during the call
      char * s = TR::Compiler->cls.classNameToSignature(className, len, methodSym->comp(), heapAlloc);

      la.add(parmSymbol);
      parmSymbol->setTypeSignature(s, len);

      slot = 1;
      }

   const char *s = sig;
   TR_ASSERT(*s == '(', "Bad signature for method: <%s>", s);
   ++s;

   uint32_t parmSlots = numberOfParameterSlots();
   for (int32_t parmIndex = 0; slot < parmSlots; ++parmIndex)
      {
      TR::DataType type = parmType(parmIndex);
      int32_t size = methodSym->convertTypeToSize(type);
      if (size < 4) type = TR::Int32;

      const char *end = s;

      // Walk past array dims, if any
      while (*end == '[')
         {
         ++end;
         }

      // Walk to the end of the class name, if this is a class name
      if (*end == 'L' || *end == 'Q')
         {
         // Assume the form is L<classname> or Q<classname>; where <classname> is
         // at least 1 char and therefore skip the first 2 chars
         end += 2;
         end = (char *)memchr(end, ';', sigEnd - end);
         TR_ASSERT(end != NULL, "Unexpected NULL, expecting to find a parm of the form L<classname>;");
         }

      // The static_cast<int>(...) is added as a work around for an XLC bug that results in the
      // pointer subtraction below getting converted into a 32-bit signed integer subtraction
      int len = static_cast<int>(end - s) + 1;

      parmSymbol = methodSym->comp()->getSymRefTab()->createParameterSymbol(methodSym, slot, type);
      parmSymbol->setOrdinal(ordinal++);
      parmSymbol->setTypeSignature(s, len);

      s += len;

      la.add(parmSymbol);
      if (type == TR::Int64 || type == TR::Double)
         {
         slot += 2;
         }
      else
         {
         ++slot;
         }
      }

   int32_t lastInterpreterSlot = parmSlots + numberOfTemps();

   if ((methodSym->isSynchronised() || methodSym->getResolvedMethod()->isNonEmptyObjectConstructor()) &&
       methodSym->comp()->getOption(TR_MimicInterpreterFrameShape))
      {
      ++lastInterpreterSlot;
      }

   methodSym->setTempIndex(lastInterpreterSlot, methodSym->comp()->fe());

   methodSym->setFirstJitTempIndex(methodSym->getTempIndex());
   }


TR_OpaqueMethodBlock *
TR_J9VMBase::getResolvedVirtualMethod(TR_OpaqueClassBlock * classObject, I_32 virtualCallOffset, bool ignoreRtResolve)
   {
   // the classObject is the fixed type of the this pointer.  The result of this method is going to be
   // used to call the virtual function directly.
   //
   // virtualCallOffset = -(vTableSlot(vmMethod, cpIndex) - TR::Compiler->vm.getInterpreterVTableOffset());
   //
   if (isInterfaceClass(classObject))
      return 0;

   J9Method * ramMethod = *(J9Method **)((char *)TR::Compiler->cls.convertClassOffsetToClassPtr(classObject) + virtualCallOffsetToVTableSlot(virtualCallOffset));

   TR_ASSERT(ramMethod, "getResolvedVirtualMethod should always find a ramMethod in the vtable slot");

   if (ramMethod &&
       (!(_jitConfig->runtimeFlags & J9JIT_RUNTIME_RESOLVE) ||
         ignoreRtResolve) &&
       J9_BYTECODE_START_FROM_RAM_METHOD(ramMethod))
      return (TR_OpaqueMethodBlock* ) ramMethod;

   return 0;
   }

TR_OpaqueMethodBlock *
TR_J9VMBase::getResolvedInterfaceMethod(J9ConstantPool *ownerCP, TR_OpaqueClassBlock * classObject, int32_t cpIndex)
   {
   TR::VMAccessCriticalSection getResolvedInterfaceMethod(this);
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");

   // the classObject is the fixed type of the this pointer.  The result of this method is going to be
   // used to call the interface function directly.
   //
   J9Method * ramMethod = jitGetInterfaceMethodFromCP(vmThread(),
                                                      ownerCP,
                                                      cpIndex,
                                                      TR::Compiler->cls.convertClassOffsetToClassPtr(classObject));

   return (TR_OpaqueMethodBlock *)ramMethod;
   }

TR_OpaqueMethodBlock *
TR_J9VMBase::getResolvedInterfaceMethod(TR_OpaqueMethodBlock *interfaceMethod, TR_OpaqueClassBlock * classObject, I_32 cpIndex)
   {
   return getResolvedInterfaceMethod((J9ConstantPool *)(J9_CP_FROM_METHOD((J9Method*)interfaceMethod)),
                                                        classObject,
                                                        cpIndex);

   }

TR_OpaqueMethodBlock *
TR_J9SharedCacheVM::getResolvedVirtualMethod(TR_OpaqueClassBlock * classObject, I_32 virtualCallOffset, bool ignoreRtResolve)
   {
   TR_OpaqueMethodBlock *ramMethod = TR_J9VMBase::getResolvedVirtualMethod(classObject, virtualCallOffset, ignoreRtResolve);
   TR::Compilation *comp = TR::comp();
   if (comp && comp->getOption(TR_UseSymbolValidationManager))
      {
      if (!comp->getSymbolValidationManager()->addVirtualMethodFromOffsetRecord(ramMethod, classObject, virtualCallOffset, ignoreRtResolve))
         return NULL;
      }

   return ramMethod;
   }

TR_OpaqueMethodBlock *
TR_J9SharedCacheVM::getResolvedInterfaceMethod(TR_OpaqueMethodBlock *interfaceMethod, TR_OpaqueClassBlock * classObject, I_32 cpIndex)
   {
   TR_OpaqueMethodBlock *ramMethod = TR_J9VMBase::getResolvedInterfaceMethod(interfaceMethod, classObject, cpIndex);
   TR::Compilation *comp = TR::comp();
   if (comp && comp->getOption(TR_UseSymbolValidationManager))
      {
      if (!comp->getSymbolValidationManager()->addInterfaceMethodFromCPRecord(ramMethod,
                                                                              (TR_OpaqueClassBlock *)J9_CLASS_FROM_METHOD((J9Method*)interfaceMethod),
                                                                              classObject,
                                                                              cpIndex))
         {
         return NULL;
         }
      }
   return ramMethod;
   }

TR_OpaqueClassBlock *
TR_ResolvedRelocatableJ9Method::getDeclaringClassFromFieldOrStatic(TR::Compilation *comp, int32_t cpIndex)
   {
   TR_OpaqueClassBlock *definingClass = TR_ResolvedJ9MethodBase::getDeclaringClassFromFieldOrStatic(comp, cpIndex);
   if (comp->getOption(TR_UseSymbolValidationManager))
      {
      if (!comp->getSymbolValidationManager()->addDeclaringClassFromFieldOrStaticRecord(definingClass, cp(), cpIndex))
         return NULL;
      }
   return definingClass;
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
   if (*_sig == 'L' || *_sig == '[' || *_sig == 'Q')
      {
      _nextIncrBy = 0;
      while (_sig[_nextIncrBy] == '[')
         {
         ++_nextIncrBy;
         }

      if (_sig[_nextIncrBy] != 'L' && _sig[_nextIncrBy] != 'Q')
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
   TR_ASSERT(*_sig == '[' || *_sig == 'L' || *_sig == 'Q', "Asked for class of incorrect Java parameter.");
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
   return (*_sig == 'L' || *_sig == 'Q');
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

static TR::DataType typeFromSig(char sig)
   {
   switch (sig)
      {
      case 'L':
      case '[':
      case 'Q':
         return TR::Address;
      case 'I':
      case 'Z':
      case 'B':
      case 'S':
      case 'C':
         return TR::Int32;
      case 'J':
         return TR::Int64;
      case 'F':
         return TR::Float;
      case 'D':
         return TR::Double;
      default:
         break;
      }
   return TR::NoType;
   }

bool
TR_J9ByteCodeIlGenerator::runFEMacro(TR::SymbolReference *symRef)
   {
   TR::MethodSymbol * symbol = symRef->getSymbol()->castToMethodSymbol();
   int32_t archetypeParmCount = symbol->getMethod()->numberOfExplicitParameters() + (symbol->isStatic() ? 0 : 1);

   // Random handy variables
   bool isVirtual = false;
   bool returnFromArchetype = false;
   char *nextHandleSignature = NULL;

   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe();
   TR::RecognizedMethod rm = symRef->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod();

   switch (rm)
      {
      case TR::java_lang_invoke_CollectHandle_allocateArray:
         {
         TR::Node* methodHandleNode = top();

         if (methodHandleNode->getOpCode().hasSymbolReference() &&
             methodHandleNode->getSymbolReference()->hasKnownObjectIndex() &&
             methodHandleNode->isNonNull())
            {
            pop();
            TR::KnownObjectTable *knot = comp()->getKnownObjectTable();
            int32_t collectArraySize = -1;
            int32_t collectPosition = -1;
            TR_OpaqueClassBlock* componentClazz = NULL;
#if defined(J9VM_OPT_JITSERVER)
            if (comp()->isOutOfProcessCompilation())
               {
               auto stream = TR::CompilationInfo::getStream();
               stream->write(JITServer::MessageType::runFEMacro_invokeCollectHandleAllocateArray,
                             methodHandleNode->getSymbolReference()->getKnownObjectIndex());
               auto recv = stream->read<int32_t, int32_t, TR_OpaqueClassBlock *>();
               collectArraySize = std::get<0>(recv);
               collectPosition = std::get<1>(recv);
               componentClazz = std::get<2>(recv);
               }
            else
#endif /* defined(J9VM_OPT_JITSERVER) */
               {
               TR::VMAccessCriticalSection invokeCollectHandleAllocateArray(fej9);
               uintptr_t methodHandle = knot->getPointer(methodHandleNode->getSymbolReference()->getKnownObjectIndex());
               collectArraySize = fej9->getInt32Field(methodHandle, "collectArraySize");
               collectPosition = fej9->getInt32Field(methodHandle, "collectPosition");

               uintptr_t arguments = fej9->getReferenceField(fej9->getReferenceField(fej9->getReferenceField(
                  methodHandle,
                  "next",             "Ljava/lang/invoke/MethodHandle;"),
                  "type",             "Ljava/lang/invoke/MethodType;"),
                  "ptypes",           "[Ljava/lang/Class;");
               componentClazz = fej9->getComponentClassFromArrayClass(fej9->getClassFromJavaLangClass(fej9->getReferenceElement(arguments, collectPosition)));
               }

            loadConstant(TR::iconst, collectArraySize);
            if (fej9->isPrimitiveClass(componentClazz))
               {
               TR_arrayTypeCode typeIndex = fej9->getPrimitiveArrayTypeCode(componentClazz);
               genNewArray(typeIndex);
               }
            else
               {
               loadClassObject(componentClazz);
               genANewArray();
               }
            return true;
            }
         return false;
         }
      case TR::java_lang_invoke_CollectHandle_numArgsToPassThrough:
      case TR::java_lang_invoke_CollectHandle_numArgsToCollect:
      case TR::java_lang_invoke_CollectHandle_collectionStart:
      case TR::java_lang_invoke_CollectHandle_numArgsAfterCollectArray:
         {
         TR_ASSERT(archetypeParmCount == 0, "assertion failure");

         J9::MethodHandleThunkDetails *thunkDetails = getMethodHandleThunkDetails(this, comp(), symRef);
         if (!thunkDetails)
            return false;

         uintptr_t methodHandle;
         int32_t collectArraySize;
         int32_t collectionStart;
         uintptr_t arguments;
         int32_t numArguments;

         bool getCollectPosition = (symRef->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod() ==
                                    TR::java_lang_invoke_CollectHandle_collectionStart)
                                   || (symRef->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod() ==
                                    TR::java_lang_invoke_CollectHandle_numArgsAfterCollectArray);
#if defined(J9VM_OPT_JITSERVER)
         if (comp()->isOutOfProcessCompilation())
            {
            auto stream = TR::CompilationInfo::getStream();
            stream->write(JITServer::MessageType::runFEMacro_invokeCollectHandleNumArgsToCollect,
                          thunkDetails->getHandleRef(), getCollectPosition);
            auto recv = stream->read<int32_t, int32_t, int32_t>();
            collectArraySize = std::get<0>(recv);
            numArguments = std::get<1>(recv);
            collectionStart = std::get<2>(recv);
            }
         else
#endif /* defined(J9VM_OPT_JITSERVER) */
            {
            TR::VMAccessCriticalSection invokeCollectHandleNumArgsToCollect(fej9);
            methodHandle = *thunkDetails->getHandleRef();
            collectArraySize = fej9->getInt32Field(methodHandle, "collectArraySize");
            if (getCollectPosition)
               collectionStart = fej9->getInt32Field(methodHandle, "collectPosition");
            arguments = fej9->getReferenceField(fej9->getReferenceField(methodHandle, "type", "Ljava/lang/invoke/MethodType;"), "ptypes", "[Ljava/lang/Class;");
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

         uintptr_t methodHandle;
         uintptr_t methodDescriptorRef;
         intptr_t methodDescriptorLength;
         char *methodDescriptor;

#if defined(J9VM_OPT_JITSERVER)
         if (comp()->isOutOfProcessCompilation())
            {
            auto stream = TR::CompilationInfo::getStream();
            stream->write(JITServer::MessageType::runFEMacro_invokeExplicitCastHandleConvertArgs, thunkDetails->getHandleRef());
            auto recv = stream->read<std::string>();
            auto &methodDescriptorString = std::get<0>(recv);
            methodDescriptorLength = methodDescriptorString.length();
            methodDescriptor = (char *)alloca(methodDescriptorLength + 1);
            memcpy(methodDescriptor, methodDescriptorString.data(), methodDescriptorLength);
            methodDescriptor[methodDescriptorLength] = 0;
            }
         else
#endif /* defined(J9VM_OPT_JITSERVER) */
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
         if (trace())
            traceMsg(comp(), "sourceSig %s targetSig %.*s\n", sourceSig, methodDescriptorLength, methodDescriptor);

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
               case 'Q':
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
               case 'Q':
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
               if ((sourceType[0] == 'L' || sourceType[0] == 'Q') && isExplicit)
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
            if (targetType[0] == 'L' || targetType[0] == 'Q')
               {
               uintptr_t methodHandle;
               uintptr_t sourceArguments;
               TR_OpaqueClassBlock *sourceParmClass;
               uintptr_t targetArguments;
               TR_OpaqueClassBlock *targetParmClass;
#if defined(J9VM_OPT_JITSERVER)
               if (comp()->isOutOfProcessCompilation())
                  {
                  auto stream = TR::CompilationInfo::getStream();
                  stream->write(JITServer::MessageType::runFEMacro_targetTypeL, thunkDetails->getHandleRef(), argIndex);
                  auto recv = stream->read<TR_OpaqueClassBlock*, TR_OpaqueClassBlock *>();
                  sourceParmClass = std::get<0>(recv);
                  targetParmClass = std::get<1>(recv);
                  }
               else
#endif /* defined(J9VM_OPT_JITSERVER) */
                  {
                  TR::VMAccessCriticalSection targetTypeL(fej9);
                  // We've already loaded the handle once, but must reload it because we released VM access in between.
                  methodHandle = *thunkDetails->getHandleRef();
                  targetArguments = fej9->getReferenceField(fej9->getReferenceField(fej9->getReferenceField(
                     methodHandle,
                     "next",             "Ljava/lang/invoke/MethodHandle;"),
                     "type",             "Ljava/lang/invoke/MethodType;"),
                     "ptypes",           "[Ljava/lang/Class;");
                  targetParmClass = (TR_OpaqueClassBlock*)(intptr_t)fej9->getInt64Field(fej9->getReferenceElement(targetArguments, argIndex),
                                                                                   "vmRef" /* should use fej9->getOffsetOfClassFromJavaLangClassField() */);
                  // Load callsite type and check if two types are compatible
                  sourceArguments = fej9->getReferenceField(fej9->getReferenceField(
                     methodHandle,
                     "type",             "Ljava/lang/invoke/MethodType;"),
                     "ptypes",           "[Ljava/lang/Class;");
                  sourceParmClass = (TR_OpaqueClassBlock*)(intptr_t)fej9->getInt64Field(fej9->getReferenceElement(sourceArguments, argIndex),
                                                                                   "vmRef" /* should use fej9->getOffsetOfClassFromJavaLangClassField() */);
                  }

               if (fej9->isInterfaceClass(targetParmClass) && isExplicit)
                  {
                  // explicitCastArguments specifies that we must not checkcast interfaces
                  }
               else if (sourceParmClass == targetParmClass || fej9->isInstanceOf(sourceParmClass, targetParmClass, false /*objectTypeIsFixed*/, true /*castTypeIsFixed*/) == TR_yes)
                  {
                  traceMsg(comp(), "source type and target type are compatible, omit checkcast for arg %d\n", argIndex);
                  }
               else
                  {
                  push(argValue);
                  loadClassObject(targetParmClass);
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
         uintptr_t receiverHandle;
         uintptr_t methodHandle;
         uintptr_t methodDescriptorRef;
         intptr_t methodDescriptorLength;
         char *methodDescriptor;

#if defined(J9VM_OPT_JITSERVER)
         if (comp()->isOutOfProcessCompilation())
            {
            std::vector<uintptr_t> listOfOffsets;
            if (!returnFromArchetype)
               {
               packReferenceChainOffsets(methodHandleExpression, listOfOffsets);
               }
            auto stream = TR::CompilationInfo::getStream();
            stream->write(JITServer::MessageType::runFEMacro_invokeILGenMacrosInvokeExactAndFixup, thunkDetails->getHandleRef(), listOfOffsets);
            auto recv = stream->read<std::string>();
            auto &methodDescriptorString = std::get<0>(recv);
            methodDescriptorLength = methodDescriptorString.length();
            methodDescriptor = (char *)alloca(methodDescriptorLength + 1);
            memcpy(methodDescriptor, methodDescriptorString.data(), methodDescriptorLength);
            methodDescriptor[methodDescriptorLength] = 0;
            }
         else
#endif /* defined(J9VM_OPT_JITSERVER) */
            {
            TR::VMAccessCriticalSection invokeILGenMacrosInvokeExactAndFixup(fej9);
            receiverHandle = *thunkDetails->getHandleRef();
            methodHandle = returnFromArchetype ? receiverHandle : walkReferenceChain(methodHandleExpression, receiverHandle);
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
         TR_ResolvedMethod  *invokeExactMacro = symRef->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod();
         TR::SymbolReference *invokeExact = comp()->getSymRefTab()->methodSymRefFromName(_methodSymbol, JSR292_MethodHandle, JSR292_invokeExact, JSR292_invokeExactSig, TR::MethodSymbol::ComputedVirtual);
         TR::SymbolReference *invokeExactWithSig = symRefWithArtificialSignature(
            invokeExact,
            "(.*).?",
            invokeExactMacro->signatureChars(), 1, // skip explicit MethodHandle argument -- the real invokeExact has it as a receiver
            returnType);
         TR::Node *invokeExactCall = genILGenMacroInvokeExact(invokeExactWithSig);

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
            case 'Q':
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
      case TR::java_lang_invoke_BruteArgumentMoverHandle_permuteArgs:
         {
         J9::MethodHandleThunkDetails *thunkDetails = getMethodHandleThunkDetails(this, comp(), symRef);
         if (!thunkDetails)
            return false;

         uintptr_t methodHandle;
         uintptr_t methodDescriptorRef;
         intptr_t methodDescriptorLength;

#if defined(J9VM_OPT_JITSERVER)
         if (comp()->isOutOfProcessCompilation())
            {
            auto stream = TR::CompilationInfo::getStream();
            stream->write(JITServer::MessageType::runFEMacro_invokeArgumentMoverHandlePermuteArgs, thunkDetails->getHandleRef());
            auto recv = stream->read<std::string>();
            auto &methodDescString = std::get<0>(recv);
            methodDescriptorLength = methodDescString.size();
            nextHandleSignature = (char *)alloca(methodDescriptorLength + 1);
            memcpy(nextHandleSignature, methodDescString.data(), methodDescriptorLength);
            nextHandleSignature[methodDescriptorLength] = 0;
            }
         else
#endif /* defined(J9VM_OPT_JITSERVER) */
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
         TR::Node* extraArrayNode = NULL;
         TR::Node* extraL[5];
         if (rm == TR::java_lang_invoke_BruteArgumentMoverHandle_permuteArgs)
            {
            extraArrayNode = pop();
            for (int i=4; i>=0; i--)
                extraL[i] = pop();
            }
#if defined(J9VM_OPT_JITSERVER)
         if (comp()->isOutOfProcessCompilation())
            {
            auto stream = TR::CompilationInfo::getStream();
            stream->write(JITServer::MessageType::runFEMacro_invokePermuteHandlePermuteArgs, thunkDetails->getHandleRef());

            // Do the client-side operations
            auto recv = stream->read<int32_t, std::vector<int32_t>>();
            permuteLength = std::get<0>(recv);
            auto &argIndices = std::get<1>(recv);

            // Do the server-side operations
            originalArgs = genNodeAndPopChildren(TR::icall, 1, placeholderWithDummySignature());
            oldSignature = originalArgs->getSymbolReference()->getSymbol()->getResolvedMethodSymbol()->getResolvedMethod()->signatureChars();
            newSignature = "()I";
            if (comp()->getOption(TR_TraceILGen))
               traceMsg(comp(), "  permuteArgs: oldSignature is %s\n", oldSignature);
            for (i=0; i < permuteLength; i++)
               {
               auto argIndex = argIndices[i];
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
                     case 'Q':
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

                  // Get the argument type of next handle
                  TR::DataType dataType = typeFromSig(argType[0]);

                  if (dataType == TR::Address && extraArrayNode)
                     {
                     if (-1 - argIndex < 5)
                        {
                        push(extraL[-1-argIndex]);
                        }
                     else
                        {
                        push(extraArrayNode);
                        loadConstant(TR::iconst, -1 - argIndex);
                        loadArrayElement(TR::Address, comp()->il.opCodeForIndirectArrayLoad(TR::Address), false);
                        }
                     }
                  else
                     {
                     TR::SymbolReference *extra = comp()->getSymRefTab()->methodSymRefFromName(_methodSymbol, JSR292_ArgumentMoverHandle, extraName, extraSignature, TR::MethodSymbol::Static);
                     loadAuto(TR::Address, 0);
                     loadConstant(TR::iconst, argIndex);
                     genInvokeDirect(extra);
                     }
                  }
               }
            if (comp()->getOption(TR_TraceILGen))
               traceMsg(comp(), "  permuteArgs: permuted placeholder signature is %s\n", newSignature);
            }
         else
#endif /* defined(J9VM_OPT_JITSERVER) */
            {
            TR::VMAccessCriticalSection invokePermuteHandlePermuteArgs(fej9);
            uintptr_t methodHandle = *thunkDetails->getHandleRef();

            uintptr_t permuteArray = fej9->getReferenceField(methodHandle, "permute", "[I");

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
                     case 'Q':
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

                  // Get the argument type of next handle
                  TR::DataType dataType = typeFromSig(argType[0]);

                  if (dataType == TR::Address && extraArrayNode)
                     {
                     if (-1 - argIndex < 5)
                        {
                        push(extraL[-1-argIndex]);
                        }
                     else
                        {
                        push(extraArrayNode);
                        loadConstant(TR::iconst, -1 - argIndex);
                        loadArrayElement(TR::Address, comp()->il.opCodeForIndirectArrayLoad(TR::Address), false);
                        }
                     }
                  else
                     {
                     TR::SymbolReference *extra = comp()->getSymRefTab()->methodSymRefFromName(_methodSymbol, JSR292_ArgumentMoverHandle, extraName, extraSignature, TR::MethodSymbol::Static);
                     loadAuto(TR::Address, 0);
                     loadConstant(TR::iconst, argIndex);
                     genInvokeDirect(extra);
                     }
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

         uintptr_t methodHandle;
         uintptr_t guardArgs;
         int32_t numGuardArgs;
#if defined(J9VM_OPT_JITSERVER)
         if (comp()->isOutOfProcessCompilation())
            {
            auto stream = TR::CompilationInfo::getStream();
            stream->write(JITServer::MessageType::runFEMacro_invokeGuardWithTestHandleNumGuardArgs, thunkDetails->getHandleRef());
            numGuardArgs = std::get<0>(stream->read<int32_t>());
            }
         else
#endif /* defined(J9VM_OPT_JITSERVER) */
            {
            TR::VMAccessCriticalSection invokeGuardWithTestHandleNumGuardArgs(fej9);
            methodHandle = *thunkDetails->getHandleRef();
            guardArgs = fej9->getReferenceField(fej9->methodHandle_type(fej9->getReferenceField(methodHandle,
               "guard", "Ljava/lang/invoke/MethodHandle;")),
               "ptypes", "[Ljava/lang/Class;");
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

         uintptr_t methodHandle;
         int32_t insertionIndex;
         uintptr_t arguments;
         int32_t numArguments;
         uintptr_t values;
         int32_t numValues;

#if defined(J9VM_OPT_JITSERVER)
         if (comp()->isOutOfProcessCompilation())
            {
            auto stream = TR::CompilationInfo::getStream();
            stream->write(JITServer::MessageType::runFEMacro_invokeInsertHandle, thunkDetails->getHandleRef());
            auto recv = stream->read<int32_t, int32_t, int32_t>();
            insertionIndex = std::get<0>(recv);
            numArguments = std::get<1>(recv);
            numValues = std::get<2>(recv);
            }
         else
#endif /* defined(J9VM_OPT_JITSERVER) */
            {
            TR::VMAccessCriticalSection invokeInsertHandle(fej9);
            methodHandle = *thunkDetails->getHandleRef();
            insertionIndex = fej9->getInt32Field(methodHandle, "insertionIndex");
            arguments = fej9->getReferenceField(fej9->getReferenceField(methodHandle, "type", "Ljava/lang/invoke/MethodType;"), "ptypes", "[Ljava/lang/Class;");
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
      case TR::java_lang_invoke_VirtualHandle_virtualCall:
         isVirtual = true;
         // fall through
      case TR::java_lang_invoke_DirectHandle_directCall:
         {
         J9::MethodHandleThunkDetails *thunkDetails = getMethodHandleThunkDetails(this, comp(), symRef);
         if (!thunkDetails)
            return false;

         TR_J9VMBase::MethodOfHandle moh = fej9->methodOfDirectOrVirtualHandle(
            thunkDetails->getHandleRef(), isVirtual);

         TR_ASSERT_FATAL(moh.j9method != NULL, "Must have a j9method to generate a custom call");

         TR_ResolvedMethod *method = fej9->createResolvedMethod(comp()->trMemory(), moh.j9method);
         TR::Node *callNode;
         if (isVirtual)
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
         if (isVirtual)
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
            callNode->getSymbolReference()->setOffset(-(moh.vmSlot - TR::Compiler->vm.getInterpreterVTableOffset()));
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
         if (comp()->target().is64Bit())
            j9methodAddress = TR::Node::create(TR::l2a, 1, j9methodAddress);
         else
            j9methodAddress = TR::Node::create(TR::i2a, 1, TR::Node::create(TR::l2i, 1, j9methodAddress));
         TR::Node *extra = TR::Node::createWithSymRef(comp()->il.opCodeForIndirectLoad(extraField->getSymbol()->getDataType()), 1, 1, j9methodAddress, extraField);
         TR::Node *result=0;
         switch (symRef->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod())
            {
            case TR::java_lang_invoke_DirectHandle_isAlreadyCompiled:
               {
               TR::ILOpCodes xcmpeq = comp()->target().is64Bit()? TR::lcmpeq : TR::icmpeq;
               TR::ILOpCodes xand   = comp()->target().is64Bit()? TR::land   : TR::iand;
               TR::Node *zero = comp()->target().is64Bit()? TR::Node::lconst(extra, 0) : TR::Node::iconst(extra, 0);
               TR::Node *mask = comp()->target().is64Bit()? TR::Node::lconst(extra, J9_STARTPC_NOT_TRANSLATED) : TR::Node::iconst(extra, J9_STARTPC_NOT_TRANSLATED);
               result =
                  TR::Node::create(xcmpeq, 2,
                     TR::Node::create(xand, 2, extra, mask),
                     zero);
               }
               break;
            case TR::java_lang_invoke_DirectHandle_compiledEntryPoint:
               if (comp()->target().cpu.isI386())
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
                  TR::ILOpCodes x2a = comp()->target().is64Bit()? TR::l2a : TR::i2a;
                  TR::Node *linkageInfo    = TR::Node::createWithSymRef(TR::iloadi, 1, 1, TR::Node::create(x2a, 1, extra), linkageInfoSymRef);
                  TR::Node *jitEntryOffset = TR::Node::create(TR::ishr,   2, linkageInfo, TR::Node::iconst(extra, 16));
                  if (comp()->target().is64Bit())
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
         uintptr_t methodHandle;
         uintptr_t arrayClass;
         J9ArrayClass *arrayJ9Class;
         J9Class *leafClass;
         UDATA arity;
         int32_t spreadPosition = -1;
         char *leafClassNameChars;
         int32_t leafClassNameLength;
         bool isPrimitiveClass;

#if defined(J9VM_OPT_JITSERVER)
         if (comp()->isOutOfProcessCompilation())
            {
            auto stream = TR::CompilationInfo::getStream();
            stream->write(JITServer::MessageType::runFEMacro_invokeSpreadHandleArrayArg, thunkDetails->getHandleRef());
            auto recv = stream->read<J9ArrayClass *, int32_t, uintptr_t, J9Class *, std::string, bool>();
            arrayJ9Class = std::get<0>(recv);
            spreadPosition = std::get<1>(recv);
            arity = std::get<2>(recv);
            leafClass = std::get<3>(recv);
            auto &leafClassNameStr = std::get<4>(recv);
            leafClassNameLength = leafClassNameStr.length();
            leafClassNameChars = (char *)alloca(leafClassNameLength + 1);
            memcpy(leafClassNameChars, leafClassNameStr.data(), leafClassNameLength);
            leafClassNameChars[leafClassNameLength] = 0;
            isPrimitiveClass = std::get<5>(recv);
            }
         else
#endif /* defined(J9VM_OPT_JITSERVER) */
            {
            TR::VMAccessCriticalSection invokeSpreadHandleArrayArg(fej9);
            methodHandle = *thunkDetails->getHandleRef();
            arrayClass   = fej9->getReferenceField(methodHandle, "arrayClass", "Ljava/lang/Class;");
            arrayJ9Class = (J9ArrayClass*)(intptr_t)fej9->getInt64Field(arrayClass,
                                                                         "vmRef" /* should use fej9->getOffsetOfClassFromJavaLangClassField() */);
            leafClass = (J9Class*)fej9->getLeafComponentClassFromArrayClass((TR_OpaqueClassBlock*)arrayJ9Class);
            arity = arrayJ9Class->arity;
            uint32_t spreadPositionOffset = fej9->getInstanceFieldOffset(fej9->getObjectClass(methodHandle), "spreadPosition", "I");
            if (spreadPositionOffset != ~0)
               spreadPosition = fej9->getInt32FieldAt(methodHandle, spreadPositionOffset);

            leafClassNameChars = fej9->getClassNameChars((TR_OpaqueClassBlock*)leafClass, leafClassNameLength); // eww, TR_FrontEnd downcast
            isPrimitiveClass = TR::Compiler->cls.isPrimitiveClass(comp(), (TR_OpaqueClassBlock *) leafClass);
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
         char *arrayClassSignature = (char*)alloca(arity + leafClassNameLength + 3); // 3 = 'L' + ';' + null terminator
         memset(arrayClassSignature, '[', arity);
         if (isPrimitiveClass)
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

         uintptr_t methodHandle;
         uintptr_t arguments;
         int32_t numArguments;
         uintptr_t next;
         uintptr_t nextArguments;
         int32_t numNextArguments;
         int32_t spreadStart;

#if defined(J9VM_OPT_JITSERVER)
         if (comp()->isOutOfProcessCompilation())
            {
            auto stream = TR::CompilationInfo::getStream();
            bool getSpreadPos = symRef->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod() == TR::java_lang_invoke_SpreadHandle_spreadStart ||
               symRef->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod() == TR::java_lang_invoke_SpreadHandle_numArgsAfterSpreadArray;
            stream->write(JITServer::MessageType::runFEMacro_invokeSpreadHandle, thunkDetails->getHandleRef(), getSpreadPos);
            auto recv = stream->read<int32_t, int32_t, int32_t>();
            numArguments = std::get<0>(recv);
            numNextArguments = std::get<1>(recv);
            spreadStart = std::get<2>(recv);
            }
         else
#endif /* defined(J9VM_OPT_JITSERVER) */
            {
            TR::VMAccessCriticalSection invokeSpreadHandle(fej9);
            methodHandle = *thunkDetails->getHandleRef();
            arguments = fej9->getReferenceField(fej9->methodHandle_type(methodHandle), "ptypes", "[Ljava/lang/Class;");
            numArguments = (int32_t)fej9->getArrayLengthInElements(arguments);
            next         = fej9->getReferenceField(methodHandle, "next", "Ljava/lang/invoke/MethodHandle;");
            nextArguments = fej9->getReferenceField(fej9->methodHandle_type(next), "ptypes", "[Ljava/lang/Class;");
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

         uintptr_t methodHandle;
         uintptr_t argIndices;

#if defined(J9VM_OPT_JITSERVER)
         if (comp()->isOutOfProcessCompilation())
            {
            auto stream = TR::CompilationInfo::getStream();
            stream->write(JITServer::MessageType::runFEMacro_invokeFoldHandle, thunkDetails->getHandleRef());
            auto recv = stream->read<std::vector<int32_t>, int32_t, int32_t>();

            auto &indices = std::get<0>(recv);
            int32_t foldPosition = std::get<1>(recv);
            int32_t numArgs = std::get<2>(recv);
            int32_t arrayLength = indices.size();

            if (arrayLength != 0)
               {
               // Push the indices in reverse order
               for (int i = arrayLength-1; i>=0; i--)
                  {
                  int32_t index = indices[i];
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
               // Push the indices in reverse order
               for (int i=foldPosition+numArgs-1; i>=foldPosition; i--)
                   loadConstant(TR::iconst, i);
               loadConstant(TR::iconst, numArgs); // number of arguments
               }
            }
         else
#endif /* defined(J9VM_OPT_JITSERVER) */
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
               uintptr_t combiner         = fej9->getReferenceField(methodHandle, "combiner", "Ljava/lang/invoke/MethodHandle;");
               uintptr_t combinerArguments = fej9->getReferenceField(fej9->methodHandle_type(combiner), "ptypes", "[Ljava/lang/Class;");
               int32_t numArgs = (int32_t)fej9->getArrayLengthInElements(combinerArguments);
               // Push the indices in reverse order
               for (int i=foldPosition+numArgs-1; i>=foldPosition; i--)
                   loadConstant(TR::iconst, i);
               loadConstant(TR::iconst, numArgs); // number of arguments
               }
            }
         return true;
         }
      case TR::java_lang_invoke_FilterArgumentsWithCombinerHandle_argumentIndices:
         {
         TR_ASSERT(archetypeParmCount == 0, "assertion failure"); // The number of arguments for argumentIndices()
         J9::MethodHandleThunkDetails *thunkDetails = getMethodHandleThunkDetails(this, comp(), symRef);
         if (!thunkDetails)
            return false;

         uintptr_t methodHandle;
         uintptr_t argumentIndices;

#if defined(J9VM_OPT_JITSERVER)
         if (comp()->isOutOfProcessCompilation())
            {
            auto stream = TR::CompilationInfo::getStream();
            stream->write(JITServer::MessageType::runFEMacro_invokeFilterArgumentsWithCombinerHandleArgumentIndices, thunkDetails->getHandleRef());

            auto recv = stream->read<int32_t, std::vector<int32_t>>();
            int32_t arrayLength = std::get<0>(recv);
            auto &argIndices = std::get<1>(recv);
            // Push the indices in reverse order
            for (int i = arrayLength - 1; i >= 0; i--) {
               loadConstant(TR::iconst, argIndices[i]);
            }
            loadConstant(TR::iconst, arrayLength); // number of arguments
            }
         else
#endif /* defined(J9VM_OPT_JITSERVER) */
            {
            TR::VMAccessCriticalSection invokeFilterArgumentsWithCombinerHandle(fej9);
            methodHandle = *thunkDetails->getHandleRef();
            argumentIndices = fej9->getReferenceField(methodHandle, "argumentIndices", "[I");
            int32_t arrayLength = (int32_t)fej9->getArrayLengthInElements(argumentIndices);
            // Push the indices in reverse order
            for (int i = arrayLength - 1; i >= 0; i--) {
               int32_t index = fej9->getInt32Element(argumentIndices, i);
               loadConstant(TR::iconst, index);
            }
            loadConstant(TR::iconst, arrayLength); // number of arguments
            }
         return true;
         }
      case TR::java_lang_invoke_FoldHandle_foldPosition:
         {
         TR_ASSERT(archetypeParmCount == 0, "assertion failure"); // The number of arguments for foldPosition()

         J9::MethodHandleThunkDetails *thunkDetails = getMethodHandleThunkDetails(this, comp(), symRef);
         if (!thunkDetails)
            return false;

         uintptr_t methodHandle;
         int32_t foldPosition;

#if defined(J9VM_OPT_JITSERVER)
         if (comp()->isOutOfProcessCompilation())
            {
            auto stream = TR::CompilationInfo::getStream();
            stream->write(JITServer::MessageType::runFEMacro_invokeFoldHandle2, thunkDetails->getHandleRef());
            foldPosition = std::get<0>(stream->read<int32_t>());
            }
         else
#endif /* defined(J9VM_OPT_JITSERVER) */
            {
            TR::VMAccessCriticalSection invokeFoldHandle(fej9);
            methodHandle = *thunkDetails->getHandleRef();
            foldPosition = fej9->getInt32Field(methodHandle, "foldPosition");
            }

         loadConstant(TR::iconst, foldPosition);
         return true;
         }
      case TR::java_lang_invoke_FilterArgumentsWithCombinerHandle_filterPosition:
         {
         TR_ASSERT(archetypeParmCount == 0, "assertion failure"); // The number of arguments for filterPosition()

         J9::MethodHandleThunkDetails *thunkDetails = getMethodHandleThunkDetails(this, comp(), symRef);
         if (!thunkDetails)
            return false;

         uintptr_t methodHandle;
         int32_t filterPosition;

#if defined(J9VM_OPT_JITSERVER)
         if (comp()->isOutOfProcessCompilation())
            {
            auto stream = TR::CompilationInfo::getStream();
            stream->write(JITServer::MessageType::runFEMacro_invokeFilterArgumentsWithCombinerHandleFilterPosition, thunkDetails->getHandleRef());
            filterPosition = std::get<0>(stream->read<int32_t>());
            }
         else
#endif /* defined(J9VM_OPT_JITSERVER) */
            {
            TR::VMAccessCriticalSection invokeFilterArgumentsWithCombinerHandle(fej9);
            methodHandle = *thunkDetails->getHandleRef();
            filterPosition = fej9->getInt32Field(methodHandle, "filterPosition");
            }

         loadConstant(TR::iconst, filterPosition);
         return true;
         }
      case TR::java_lang_invoke_FoldHandle_argumentsForCombiner:
      case TR::java_lang_invoke_FilterArgumentsWithCombinerHandle_argumentsForCombiner:
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

#if defined(J9VM_OPT_JITSERVER)
         if (comp()->isOutOfProcessCompilation())
            {
            auto stream = TR::CompilationInfo::getStream();
            stream->write(JITServer::MessageType::runFEMacro_invokeFinallyHandle, thunkDetails->getHandleRef());
            auto recv = stream->read<int32_t, std::string>();
            numArgsPassToFinallyTarget = std::get<0>(recv);
            auto &methodDescriptorString = std::get<1>(recv);
            int methodDescriptorLength = methodDescriptorString.size();
            methodDescriptor = (char *)alloca(methodDescriptorLength + 1);
            memcpy(methodDescriptor, methodDescriptorString.data(), methodDescriptorLength);
            methodDescriptor[methodDescriptorLength] = 0;
            }
         else
#endif /* defined(J9VM_OPT_JITSERVER) */
            {
            TR::VMAccessCriticalSection invokeFinallyHandle(fej9);
            uintptr_t methodHandle = *thunkDetails->getHandleRef();
            uintptr_t finallyTarget = fej9->getReferenceField(methodHandle, "finallyTarget", "Ljava/lang/invoke/MethodHandle;");
            uintptr_t finallyType = fej9->getReferenceField(finallyTarget, "type", "Ljava/lang/invoke/MethodType;");
            uintptr_t arguments        = fej9->getReferenceField(finallyType, "ptypes", "[Ljava/lang/Class;");
            numArgsPassToFinallyTarget = (int32_t)fej9->getArrayLengthInElements(arguments);

            uintptr_t methodDescriptorRef = fej9->getReferenceField(finallyType, "methodDescriptor", "Ljava/lang/String;");
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
      case TR::java_lang_invoke_FilterArgumentsWithCombinerHandle_numSuffixArgs:
         {
         TR_ASSERT(archetypeParmCount == 0, "assertion failure");

         J9::MethodHandleThunkDetails *thunkDetails = getMethodHandleThunkDetails(this, comp(), symRef);
         if (!thunkDetails)
            return false;

         uintptr_t methodHandle;
         uintptr_t arguments;
         int32_t numArguments;
         int32_t filterPos;

#if defined(J9VM_OPT_JITSERVER)
         if (comp()->isOutOfProcessCompilation())
            {
            auto stream = TR::CompilationInfo::getStream();
            stream->write(JITServer::MessageType::runFEMacro_invokeFilterArgumentsWithCombinerHandleNumSuffixArgs, thunkDetails->getHandleRef());
            auto recv = stream->read<int32_t, int32_t>();
            numArguments = std::get<0>(recv);
            filterPos = std::get<1>(recv);
            }
         else
#endif /* defined(J9VM_OPT_JITSERVER) */
            {
            TR::VMAccessCriticalSection invokeFilterArgumentsWithCombinerHandle(fej9);
            methodHandle = *thunkDetails->getHandleRef();
            arguments = fej9->getReferenceField(fej9->methodHandle_type(methodHandle), "ptypes", "[Ljava/lang/Class;");
            numArguments = (int32_t)fej9->getArrayLengthInElements(arguments);
            filterPos     = (int32_t)fej9->getInt32Field(methodHandle, "filterPosition");
            }

         loadConstant(TR::iconst, numArguments - (filterPos + 1));
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

         uintptr_t methodHandle;
         uintptr_t arguments;
         int32_t numArguments;
         int32_t startPos;
         uintptr_t filters;
         int32_t numFilters;

#if defined(J9VM_OPT_JITSERVER)
         if (comp()->isOutOfProcessCompilation())
            {
            auto stream = TR::CompilationInfo::getStream();
            stream->write(JITServer::MessageType::runFEMacro_invokeFilterArgumentsHandle2, thunkDetails->getHandleRef());
            auto recv = stream->read<int32_t, int32_t, int32_t>();
            numArguments = std::get<0>(recv);
            startPos = std::get<1>(recv);
            numFilters = std::get<2>(recv);
            }
         else
#endif /* defined(J9VM_OPT_JITSERVER) */
            {
            TR::VMAccessCriticalSection invokeFilterArgumentsHandle2(fej9);
            methodHandle = *thunkDetails->getHandleRef();
            arguments = fej9->getReferenceField(fej9->methodHandle_type(methodHandle), "ptypes", "[Ljava/lang/Class;");
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
         bool *haveFilter = NULL;
         bool knotEnabled = !comp()->getOption(TR_DisableKnownObjectTable);
         char *nextSignature;
         TR::KnownObjectTable *knot = knotEnabled ? comp()->getOrCreateKnownObjectTable() : NULL;
#if defined(J9VM_OPT_JITSERVER)
         if (comp()->isOutOfProcessCompilation())
            {
            auto stream = TR::CompilationInfo::getStream();
            stream->write(JITServer::MessageType::runFEMacro_invokeFilterArgumentsHandle, thunkDetails->getHandleRef(), knotEnabled);
            auto recv = stream->read<int32_t, std::string, std::vector<uint8_t>, std::vector<TR::KnownObjectTable::Index>, std::vector<uintptr_t *>>();
            startPos = std::get<0>(recv);
            auto &nextSigStr = std::get<1>(recv);
            auto &haveFilterList = std::get<2>(recv);
            auto &recvfilterIndexList = std::get<3>(recv);
            auto &recvFilterObjectReferenceLocationList = std::get<4>(recv);

            // copy the next signature
            intptr_t methodDescriptorLength = nextSigStr.size();
            nextSignature = (char *)alloca(methodDescriptorLength + 1);
            memcpy(nextSignature, nextSigStr.data(), methodDescriptorLength);
            nextSignature[methodDescriptorLength] = 0;

            // copy the filters
            int32_t numFilters = haveFilterList.size();
            filterIndexList = knotEnabled ? (TR::KnownObjectTable::Index *)comp()->trMemory()->allocateMemory(
                              sizeof(TR::KnownObjectTable::Index) * numFilters, stackAlloc) : NULL;
            haveFilter = (bool *)comp()->trMemory()->allocateMemory(sizeof(bool) * numFilters, stackAlloc);
            for (int i = 0; i < numFilters; i++)
               {
               haveFilter[i] = haveFilterList[i];
               if (knotEnabled)
                  {
                  filterIndexList[i] = recvfilterIndexList[i];

                  if (recvfilterIndexList[i] != TR::KnownObjectTable::UNKNOWN)
                     knot->updateKnownObjectTableAtServer(recvfilterIndexList[i], recvFilterObjectReferenceLocationList[i]);
                  }
               }
            }
         else
#endif /* defined(J9VM_OPT_JITSERVER) */
            {
            TR::VMAccessCriticalSection invokeFilterArgumentsHandle(fej9);
            uintptr_t methodHandle = *thunkDetails->getHandleRef();
            uintptr_t filters = fej9->getReferenceField(methodHandle, "filters", "[Ljava/lang/invoke/MethodHandle;");

            int32_t numFilters = fej9->getArrayLengthInElements(filters);
            filterIndexList = knotEnabled ? (TR::KnownObjectTable::Index *) comp()->trMemory()->allocateMemory(sizeof(TR::KnownObjectTable::Index) * numFilters, stackAlloc) : NULL;
            haveFilter = (bool *) comp()->trMemory()->allocateMemory(sizeof(bool) * numFilters, stackAlloc);
            for (int i = 0; i <numFilters; i++)
               {
               // copy filters in a list, so that we don't have to use filterIndexList
               // to determine if a filter is null later on
               haveFilter[i] = fej9->getReferenceElement(filters, i) != 0;
               if (knotEnabled)
                  filterIndexList[i] = knot->getOrCreateIndex(fej9->getReferenceElement(filters, i));
               }


            startPos = (int32_t)fej9->getInt32Field(methodHandle, "startPos");
            uintptr_t methodDescriptorRef = fej9->getReferenceField(fej9->getReferenceField(fej9->getReferenceField(
               methodHandle,
               "next",             "Ljava/lang/invoke/MethodHandle;"),
               "type",             "Ljava/lang/invoke/MethodType;"),
               "methodDescriptor", "Ljava/lang/String;");
            intptr_t methodDescriptorLength = fej9->getStringUTF8Length(methodDescriptorRef);
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
            if(haveFilter[arrayIndex])
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
                  if (!knotEnabled || thunkDetails->isShareable())
                     {
                     // First case: If Known Object Table is disabled, can't improve symbol reference.
                     // Second case: Can't set known object information for the filters.  That
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

               genILGenMacroInvokeExact(invokeExactWithSig);

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

#if defined(J9VM_OPT_JITSERVER)
         if (comp()->isOutOfProcessCompilation())
            {
            auto stream = TR::CompilationInfo::getStream();
            stream->write(JITServer::MessageType::runFEMacro_invokeCatchHandle, thunkDetails->getHandleRef());
            numCatchArguments = std::get<0>(stream->read<int32_t>());
            }
         else
#endif /* defined(J9VM_OPT_JITSERVER) */
            {
            TR::VMAccessCriticalSection invokeCatchHandle(fej9);
            uintptr_t methodHandle   = *thunkDetails->getHandleRef();
            uintptr_t catchTarget    = fej9->getReferenceField(methodHandle, "catchTarget", "Ljava/lang/invoke/MethodHandle;");
            uintptr_t catchArguments = fej9->getReferenceField(fej9->getReferenceField(
               catchTarget,
               "type", "Ljava/lang/invoke/MethodType;"),
               "ptypes", "[Ljava/lang/Class;");
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

#if defined(J9VM_OPT_JITSERVER)
         if (comp()->isOutOfProcessCompilation())
            {
            auto stream = TR::CompilationInfo::getStream();
            std::vector<uintptr_t> listOfOffsets;
            packReferenceChainOffsets(pop(), listOfOffsets);
            stream->write(JITServer::MessageType::runFEMacro_invokeILGenMacrosParameterCount, thunkDetails->getHandleRef(), listOfOffsets);
            parameterCount = std::get<0>(stream->read<int32_t>());
            }
         else
#endif /* defined(J9VM_OPT_JITSERVER) */
            {
            TR::VMAccessCriticalSection invokeILGenMacros(fej9);
            uintptr_t receiverHandle   = *thunkDetails->getHandleRef();
            uintptr_t methodHandle     = walkReferenceChain(pop(), receiverHandle);
            uintptr_t arguments        = fej9->getReferenceField(fej9->getReferenceField(
               methodHandle,
               "type", "Ljava/lang/invoke/MethodType;"),
               "ptypes", "[Ljava/lang/Class;");
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

#if defined(J9VM_OPT_JITSERVER)
         if (comp()->isOutOfProcessCompilation())
            {
            auto stream = TR::CompilationInfo::getStream();
            std::vector<uintptr_t> listOfOffsets;
            packReferenceChainOffsets(pop(), listOfOffsets);
            stream->write(JITServer::MessageType::runFEMacro_invokeILGenMacrosArrayLength, thunkDetails->getHandleRef(), listOfOffsets);
            arrayLength = std::get<0>(stream->read<int32_t>());
            }
         else
#endif /* defined(J9VM_OPT_JITSERVER) */
            {
            TR::VMAccessCriticalSection invokeILGenMacros(fej9);
            uintptr_t receiverHandle   = *thunkDetails->getHandleRef();
            uintptr_t array            = walkReferenceChain(pop(), receiverHandle);
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
         uintptr_t fieldOffset = fieldSymRef->getOffset() - TR::Compiler->om.objectHeaderSizeInBytes(); // blah

         int32_t result;

#if defined(J9VM_OPT_JITSERVER)
         if (comp()->isOutOfProcessCompilation())
            {
            auto stream = TR::CompilationInfo::getStream();
            std::vector<uintptr_t> listOfOffsets;
            packReferenceChainOffsets(baseObjectNode, listOfOffsets);
            TR_ASSERT(fieldSym->getDataType() == TR::Int32, "ILGenMacros.getField expecting int field; found load of %s", comp()->getDebug()->getName(symRef));
            stream->write(JITServer::MessageType::runFEMacro_invokeILGenMacrosGetField, thunkDetails->getHandleRef(), fieldOffset, listOfOffsets);
            result = std::get<0>(stream->read<int32_t>());
            }
         else
#endif /* defined(J9VM_OPT_JITSERVER) */
            {
            TR::VMAccessCriticalSection invokeILGenMacros(fej9);
            uintptr_t receiverHandle   = *thunkDetails->getHandleRef();
            uintptr_t baseObject       = walkReferenceChain(baseObjectNode, receiverHandle);
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

uintptr_t
TR_J9ByteCodeIlGenerator::walkReferenceChain(TR::Node *node, uintptr_t receiver)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp()->fe());
#if defined(J9VM_OPT_JITSERVER)
   TR_ASSERT_FATAL(!comp()->isOutOfProcessCompilation(), "walkReferenceChain() should not be called by JITServer because of getReferenceFieldAt() call");
#endif
   uintptr_t result = 0;
   if (node->getOpCode().isLoadDirect() && node->getType() == TR::Address)
      {
      TR_ASSERT(node->getSymbolReference()->getCPIndex() == 0, "walkReferenceChain expecting aload of 'this'; found aload of %s", comp()->getDebug()->getName(node->getSymbolReference()));
      result = receiver;
      }
   else if (node->getOpCode().isLoadIndirect() && node->getType() == TR::Address)
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
      uintptr_t fieldOffset = symRef->getOffset() - TR::Compiler->om.objectHeaderSizeInBytes();
      result = fej9->getReferenceFieldAt(walkReferenceChain(node->getFirstChild(), receiver), fieldOffset);
      }
   else
      {
      TR_ASSERT(0, "Unexpected opcode in walkReferenceChain: %s", node->getOpCode().getName());
      comp()->failCompilation<TR::ILGenFailure>("Unexpected opcode in walkReferenceChain");
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


#if defined(J9VM_OPT_JITSERVER)
void
TR_J9ByteCodeIlGenerator::packReferenceChainOffsets(TR::Node *node, std::vector<uintptr_t>& listOfOffsets)
   {
   if (node->getOpCode().isLoadDirect() && node->getType() == TR::Address)
      {
      TR_ASSERT(node->getSymbolReference()->getCPIndex() == 0, "walkReferenceChain expecting aload of 'this'; found aload of %s", comp()->getDebug()->getName(node->getSymbolReference()));
      return;
      }
   else if (node->getOpCode().isLoadIndirect() && node->getType() == TR::Address)
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
      uintptr_t fieldOffset = symRef->getOffset() - TR::Compiler->om.objectHeaderSizeInBytes();
      packReferenceChainOffsets(node->getFirstChild(), listOfOffsets);
      listOfOffsets.push_back(fieldOffset);
      }
   else
      {
      TR_ASSERT(0, "Unexpected opcode in walkReferenceChain: %s", node->getOpCode().getName());
      comp()->failCompilation<TR::ILGenFailure>("Unexpected opcode in walkReferenceChain");
      }

   if (comp()->getOption(TR_TraceILGen))
      {
      TR_ASSERT(node->getOpCode().hasSymbolReference(), "Can't get here without a symref");
      traceMsg(comp(), "  walkReferenceChain(%s) // %s\n",
         comp()->getDebug()->getName(node),
         comp()->getDebug()->getName(node->getSymbolReference()));
      }
   return;
   }
#endif

bool
TR_ResolvedJ9Method::isFieldQType(int32_t cpIndex)
   {
   if (!TR::Compiler->om.areFlattenableValueTypesEnabled() ||
      (-1 == cpIndex))
      return false;

   J9VMThread *vmThread = fej9()->vmThread();
   J9ROMFieldRef *ref = (J9ROMFieldRef *) (&romCPBase()[cpIndex]);
   J9ROMNameAndSignature *nameAndSignature = J9ROMFIELDREF_NAMEANDSIGNATURE(ref);
   J9UTF8 *signature = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSignature);

   return vmThread->javaVM->internalVMFunctions->isNameOrSignatureQtype(signature);
   }

bool
TR_ResolvedJ9Method::isFieldFlattened(TR::Compilation *comp, int32_t cpIndex, bool isStatic)
   {
   if (!TR::Compiler->om.areFlattenableValueTypesEnabled() ||
      (-1 == cpIndex))
      return false;

   J9VMThread *vmThread = fej9()->vmThread();
   J9ROMFieldShape *fieldShape = NULL;
   TR_OpaqueClassBlock *containingClass = definingClassAndFieldShapeFromCPFieldRef(comp, cp(), cpIndex, isStatic, &fieldShape);

   // No lock is required here. Entries in J9Class::flattenedClassCache are only written during classload.
   // They are effectively read only when being exposed to the JIT.
   return vmThread->javaVM->internalVMFunctions->isFlattenableFieldFlattened(reinterpret_cast<J9Class *>(containingClass), fieldShape);
   }
