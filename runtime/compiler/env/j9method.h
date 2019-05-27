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

#ifndef j9method_h
#define j9method_h

#include "codegen/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "ilgen/IlGeneratorMethodDetails.hpp"

#define J9ROM_CP_BASE(romClassPtr, x) ((x*) (((U_8 *) (romClassPtr)) + sizeof(J9ROMClass)))

class TR_J9VMBase;

extern "C" {
struct J9UTF8;
struct J9Class;
struct J9Method;
struct J9ConstantPool;
struct J9ClassLoader;
struct J9RAMConstantPoolItem;
struct J9ROMClass;
struct J9ROMMethod;
struct J9ROMConstantPoolItem;
struct J9ROMFieldRef;
struct J9ExceptionHandler;
}

#ifdef __cplusplus
extern "C" J9UTF8 * getSignatureFromTR_VMMethod(TR_OpaqueMethodBlock *vmMethod);
extern "C" J9UTF8 * getNameFromTR_VMMethod(TR_OpaqueMethodBlock *vmMethod);
extern "C" J9UTF8 * getClassNameFromTR_VMMethod(TR_OpaqueMethodBlock *vmMethod);
extern "C" J9Class * getRAMClassFromTR_ResolvedVMMethod(TR_OpaqueMethodBlock *vmMethod);
#endif

inline char *nextSignatureArgument(char *currentArgument)
   {
   char *result = currentArgument;
   while (*result == '[')
      result++;
   if (*result == 'L')
      while (*result != ';')
         result++;
   return result+1;
   }

inline char *nthSignatureArgument(int32_t n, char *currentArgument)
   {
   while (n-- >= 1)
      currentArgument = nextSignatureArgument(currentArgument);
   return currentArgument;
   }

class TR_J9MethodBase;
class TR_J9MethodParameterIterator : public TR_MethodParameterIterator
   {
public:
   TR_ALLOC(TR_Memory::Method)
   virtual TR::DataType         getDataType();
   virtual TR_OpaqueClassBlock * getOpaqueClass();
   virtual char *                getUnresolvedJavaClassSignature(uint32_t&);
   virtual bool                  isArray();
   virtual bool                  isClass();
   virtual bool                  atEnd();
   virtual void                  advanceCursor();
private:
   TR_J9MethodParameterIterator(TR_J9MethodBase &j9Method, TR::Compilation& comp, TR_ResolvedMethod * resolveMethod);
   TR_J9MethodBase &    _j9Method;
   TR_ResolvedMethod *  _resolvedMethod;
   char *               _sig;
   uintptr_t            _nextIncrBy;
   friend class TR_J9MethodBase;
   };

class TR_J9MethodBase : public TR_Method
   {
public:
   TR_ALLOC(TR_Memory::Method)

   static bool                   isBigDecimalNameAndSignature(J9UTF8 *name, J9UTF8 *signature);
   static bool                   isBigDecimalMethod(J9ROMMethod * romMethod, J9ROMClass * romClass);
   static bool                   isBigDecimalMethod(J9UTF8 * className, J9UTF8 * name, J9UTF8 * signature);
   static bool                   isBigDecimalMethod(J9Method * j9Method);
   bool                          isBigDecimalMethod( TR::Compilation * comp = NULL);
   static bool                   isBigDecimalConvertersNameAndSignature(J9UTF8 *name, J9UTF8 *signature);
   static bool                   isBigDecimalConvertersMethod(J9ROMMethod * romMethod, J9ROMClass * romClass);
   static bool                   isBigDecimalConvertersMethod(J9UTF8 * className, J9UTF8 * name, J9UTF8 * signature);
   static bool                   isBigDecimalConvertersMethod(J9Method * j9Method);
   bool                          isBigDecimalConvertersMethod( TR::Compilation * comp = NULL);

   static bool                   isUnsafeGetPutWithObjectArg(TR::RecognizedMethod rm);
   static bool                   isUnsafePut(TR::RecognizedMethod rm);
   static bool                   isVolatileUnsafe(TR::RecognizedMethod rm);
   static TR::DataType           unsafeDataTypeForArray(TR::RecognizedMethod rm);
   static TR::DataType           unsafeDataTypeForObject(TR::RecognizedMethod rm);
   static bool                   isVarHandleOperationMethod(TR::RecognizedMethod rm);
   virtual bool                  isVarHandleAccessMethod(TR::Compilation * = NULL);

   virtual bool                  isUnsafeWithObjectArg( TR::Compilation * comp = NULL);
   virtual bool                  isUnsafeCAS(TR::Compilation * = NULL);
   virtual uint32_t              numberOfExplicitParameters();
   virtual TR::DataType         parmType(uint32_t parmNumber); // returns the type of the parmNumber'th parameter (0-based)

   virtual TR::ILOpCodes         directCallOpCode();
   virtual TR::ILOpCodes         indirectCallOpCode();

   virtual TR::DataType         returnType();
   virtual uint32_t              returnTypeWidth();
   virtual bool                  returnTypeIsUnsigned();
   virtual TR::ILOpCodes         returnOpCode();

   virtual const char *          signature(TR_Memory  *, TR_AllocationKind = heapAlloc); // this actually returns the class, method, and signature!
   virtual uint16_t              classNameLength();
   virtual uint16_t              nameLength();
   virtual uint16_t              signatureLength();
   virtual char *                classNameChars(); // returns the utf8 of the class that this method is in.
   virtual char *                nameChars(); // returns the utf8 of the method name
   virtual char *                signatureChars(); // returns the utf8 of the signature

   void                          setSignature(char *newSignature, int32_t newSignatureLength, TR_Memory *); // JSR292
   virtual void                  setArchetypeSpecimen(bool b = true);

   virtual bool                  isConstructor();
   virtual bool                  isFinalInObject();
   virtual TR_MethodParameterIterator* getParameterIterator( TR::Compilation &comp, TR_ResolvedMethod *r)
      { return new (comp.trHeapMemory()) TR_J9MethodParameterIterator(*this, comp, r); }

   virtual bool isArchetypeSpecimen(){ return _flags.testAny(ArchetypeSpecimen); }

protected:
   friend class TR_Debug;
   friend class TR_DebugExt;
   uintptr_t    j9returnType();
   void         parseSignature(TR_Memory *);


   uintptr_t    _paramElements;
   uintptr_t    _paramSlots;
   J9UTF8 *     _signature;
   J9UTF8 *     _name;
   J9UTF8 *     _className;
   uint8_t *    _argTypes;
   char *       _fullSignature;
   flags32_t    _flags;

   enum Flags
      {
      ArchetypeSpecimen = 0x00000001, // An "instance" of an archetype method, where the varargs portion of the signature has been expanded into zero or more args

      dummyLastEnum
      };
   };


class TR_J9Method : public TR_J9MethodBase
   {
public:
   TR_J9Method(TR_FrontEnd *trvm, TR_Memory *, J9Class * aClazz, uintptr_t cpIndex);
   TR_J9Method(TR_FrontEnd *trvm, TR_Memory *, TR_OpaqueMethodBlock * aMethod);
   };

class TR_ResolvedJ9MethodBase : public TR_ResolvedMethod
   {
public:
   TR_ResolvedJ9MethodBase(TR_FrontEnd *, TR_ResolvedMethod * owningMethod);

   virtual bool                  isCompilable(TR_Memory *);
   virtual bool                  isInlineable( TR::Compilation *);
   virtual bool                  isCold( TR::Compilation *, bool, TR::ResolvedMethodSymbol * sym = NULL);
   virtual TR_J9VMBase *         fej9();
   virtual TR_FrontEnd *         fe();


   J9ROMConstantPoolItem * romLiterals();   // address of 1st CP entry (with type being an entry for array indexing)
   J9ROMFieldRef *         romCPBase();

   virtual TR_ResolvedMethod    *owningMethod();
   virtual void                  setOwningMethod(TR_ResolvedMethod *parent);
   virtual bool                  owningMethodDoesntMatter();
   virtual bool                  canAlwaysShareSymbolDespiteOwningMethod(TR_ResolvedMethod *other);

   virtual char *                classNameOfFieldOrStatic(int32_t cpIndex, int32_t & len);
   virtual char *                classSignatureOfFieldOrStatic(int32_t cpIndex, int32_t & len);
   virtual int32_t               classCPIndexOfFieldOrStatic(int32_t cpIndex);
   virtual TR_OpaqueClassBlock  *getDeclaringClassFromFieldOrStatic( TR::Compilation *comp, int32_t cpIndex);
   virtual char *                fieldName (int32_t cpIndex, TR_Memory *, TR_AllocationKind kind = heapAlloc);
   virtual char *                staticName(int32_t cpIndex, TR_Memory *, TR_AllocationKind kind = heapAlloc);
   virtual char *                localName (uint32_t slotNumber, uint32_t bcIndex, TR_Memory *){ return NULL; }
   virtual char *                fieldName (int32_t cpIndex, int32_t & len, TR_Memory *, TR_AllocationKind kind = heapAlloc);
   virtual char *                staticName(int32_t cpIndex, int32_t & len, TR_Memory *, TR_AllocationKind kind = heapAlloc);
   virtual char *                localName (uint32_t slotNumber, uint32_t bcIndex, int32_t &len, TR_Memory *){ return NULL; }

   virtual void setMethodHandleLocation(uintptrj_t *location)
      {
      TR_ASSERT(convertToMethod()->isArchetypeSpecimen(), "All methods associated with a MethodHandle must be archetype specimens");
      _methodHandleLocation = location;
      }

   virtual uintptrj_t *getMethodHandleLocation()
      {
      TR_ASSERT(convertToMethod()->isArchetypeSpecimen(), "All methods associated with a MethodHandle must be archetype specimens");
      return _methodHandleLocation;
      }

   virtual TR::SymbolReferenceTable *_genMethodILForPeeking(TR::ResolvedMethodSymbol *methodSymbol,  TR::Compilation *compilation, bool resetVisitCount, TR_PrexArgInfo* argInfo);

   protected:

   char *  fieldOrStaticName(int32_t cpIndex, int32_t & len, TR_Memory *, TR_AllocationKind kind = heapAlloc);
   int32_t exceptionData(J9ExceptionHandler *, int32_t, int32_t, int32_t *, int32_t *, int32_t *);
   virtual TR_J9MethodBase *asJ9Method() = 0;

   TR_J9VMBase             *_fe;
   TR_ResolvedMethod       *_owningMethod;
   J9ROMConstantPoolItem   *_romLiterals;
   uintptrj_t              *_methodHandleLocation; // Address of a (uncompressed) reference to a j/l/i/MethodHandle

   };


class TR_ResolvedJ9Method : public TR_J9Method, public TR_ResolvedJ9MethodBase
   {
public:
   TR_ResolvedJ9Method(TR_OpaqueMethodBlock * aMethod, TR_FrontEnd *, TR_Memory *, TR_ResolvedMethod * owningMethod = 0, uint32_t vTableSlot = 0);

   J9Method *              ramMethod() { return _ramMethod; }
   J9ROMMethod *           romMethod() { return _romMethod; }
   J9Class *               constantPoolHdr();
   J9ROMClass *            romClassPtr();
   J9RAMConstantPoolItem * literals();   // address of 1st CP entry (with type being an entry for array indexing)


   uint32_t                methodModifiers();
   uint32_t                classModifiers();
   uint32_t                classExtraModifiers();

   J9ClassLoader *         getClassLoader();

   virtual J9ConstantPool *      cp();
   virtual TR_Method *           convertToMethod();

   virtual uint32_t              numberOfParameters();
   virtual uint32_t              numberOfExplicitParameters();
   virtual TR::DataType         parmType(uint32_t parmNumber); // returns the type of the parmNumber'th parameter (0-based)
   virtual TR::ILOpCodes         directCallOpCode();
   virtual TR::ILOpCodes         indirectCallOpCode();
   virtual TR::DataType         returnType();
   virtual uint32_t              returnTypeWidth();
   virtual bool                  returnTypeIsUnsigned();
   virtual TR::ILOpCodes         returnOpCode();
   virtual const char *          signature(TR_Memory  *, TR_AllocationKind = heapAlloc);
   virtual uint16_t              classNameLength();
   virtual uint16_t              nameLength();
   virtual uint16_t              signatureLength();
   virtual char *                classNameChars(); // returns the utf8 of the class that this method is in.
   virtual char *                nameChars(); // returns the utf8 of the method name
   virtual char *                signatureChars(); // returns the utf8 of the signature

   virtual bool                  isConstructor();
   virtual bool                  isStatic();
   virtual bool                  isAbstract();
   virtual bool                  isNative();
   virtual bool                  isSynchronized();
   virtual bool                  isPrivate();
   virtual bool                  isProtected();
   virtual bool                  isPublic();
   virtual bool                  isFinal();
   virtual bool                  isStrictFP();
   virtual bool                  isInterpreted();
   virtual bool                  isInterpretedForHeuristics();
   virtual bool                  hasBackwardBranches();
   virtual bool                  isObjectConstructor();
   virtual bool                  isNonEmptyObjectConstructor();
   virtual bool                  isSubjectToPhaseChange(TR::Compilation *comp);

   virtual void *                resolvedMethodAddress();
   virtual void *                startAddressForJittedMethod();
   virtual void *                startAddressForJNIMethod( TR::Compilation *);
   virtual void *                startAddressForJITInternalNativeMethod();
   virtual void *                startAddressForInterpreterOfJittedMethod();
   virtual bool                  isWarmCallGraphTooBig(uint32_t bcIndex,  TR::Compilation *);
   virtual void                  setWarmCallGraphTooBig(uint32_t bcIndex,  TR::Compilation *);
   virtual void                  getFaninInfo(uint32_t *count, uint32_t *weight, uint32_t *otherBucketWeight = NULL);
   virtual bool                  getCallerWeight(TR_ResolvedJ9Method *caller, uint32_t *weight, uint32_t pcIndex=~0);


   virtual intptrj_t             getInvocationCount();
   virtual bool                  setInvocationCount(intptrj_t oldCount, intptrj_t newCount);
   virtual bool                  isSameMethod(TR_ResolvedMethod *);

   virtual uint16_t              numberOfParameterSlots();
   virtual uint16_t              archetypeArgPlaceholderSlot(TR_Memory *);
   virtual uint16_t              numberOfTemps();
   virtual uint16_t              numberOfPendingPushes();

   virtual uint8_t *             bytecodeStart();
   virtual uint32_t              maxBytecodeIndex();

   virtual TR_OpaqueClassBlock * containingClass();

   static TR_OpaqueClassBlock *  getClassFromCP(TR_J9VMBase *fej9, J9ConstantPool *cp, TR::Compilation *comp, uint32_t cpIndex);
   static TR_OpaqueClassBlock *  getClassOfStaticFromCP(TR_J9VMBase *fej9, J9ConstantPool *cp, int32_t cpIndex);

   virtual void *                ramConstantPool();
   virtual void *                constantPool();
   virtual TR_OpaqueClassBlock * getClassFromConstantPool( TR::Compilation *, uint32_t cpIndex, bool returnClassForAot=false);
   virtual bool                  validateClassFromConstantPool( TR::Compilation *comp, J9Class *clazz, uint32_t cpIndex, TR_ExternalRelocationTargetKind reloKind = TR_ValidateClass);
   virtual bool                  validateArbitraryClass( TR::Compilation *comp, J9Class *clazz);

   virtual char *                getClassNameFromConstantPool(uint32_t cpIndex, uint32_t &length);
   virtual TR::DataType         getLDCType(int32_t cpIndex);
   virtual bool                  isClassConstant(int32_t cpIndex);
   virtual bool                  isStringConstant(int32_t cpIndex);
   virtual bool                  isMethodTypeConstant(int32_t cpIndex);
   virtual bool                  isMethodHandleConstant(int32_t cpIndex);
   virtual uint32_t              intConstant(int32_t cpIndex);
   virtual uint64_t              longConstant(int32_t cpIndex);
   virtual float *               floatConstant(int32_t cpIndex);
   virtual double *              doubleConstant(int32_t cpIndex, TR_Memory *);
   virtual void *                stringConstant(int32_t cpIndex);
   virtual bool                  isUnresolvedString(int32_t cpIndex, bool optimizeForAOT = false);
   /** \brief
    *     Retrieves the underlying type information for a given constant dynamic.
    *
    *  \param cpIndex
    *     The constant pool index of the constant dynamic.
    *
    *  \return
    *     Opaque pointer to the UTF8 type string.
    */
   virtual void *                getConstantDynamicTypeFromCP(int32_t cpIndex);
   /** \brief
    *     Determines whether the given constant pool entry is constant dynamic.
    *
    *  \param cpIndex
    *     The constant pool index of the constant dynamic.
    *
    *  \return
    *     <c>true</c> if the given constant pool entry type is constant dynamic; <c>false</c> otherwise.
    */
   virtual bool                  isConstantDynamic(int32_t cpIndex);
   /** \brief
    *     Determines whether the given constant dynamic is unresolved.
    *
    *  \param cpIndex
    *     The constant pool index of the constant dynamic.
    *
    *  \return
    *     <c>true</c> if the constant dynamic is unresolved; <c>false</c> otherwise.
    */
   virtual bool                  isUnresolvedConstantDynamic(int32_t cpIndex);
   /** \brief
    *     Retrieve the address of the slot containing the constant dynamic.
    *
    *  \param cpIndex
    *     The constant pool index of the constant dynamic.
    *
    *  \return
    *     Opaque pointer to the slot containing the resolved constant dynamic value.
    */
   virtual void *                dynamicConstant(int32_t cpIndex);
   virtual void *                methodTypeConstant(int32_t cpIndex);
   virtual bool                  isUnresolvedMethodType(int32_t cpIndex);
   virtual void *                methodHandleConstant(int32_t cpIndex);
   virtual bool                  isUnresolvedMethodHandle(int32_t cpIndex);

   virtual bool                  isUnresolvedCallSiteTableEntry(int32_t callSiteIndex);
   virtual void *                callSiteTableEntryAddress(int32_t callSiteIndex);
#if defined(J9VM_OPT_REMOVE_CONSTANT_POOL_SPLITTING)
   virtual bool                  isUnresolvedMethodTypeTableEntry(int32_t cpIndex);
   virtual void *                methodTypeTableEntryAddress(int32_t cpIndex);
#endif
   // J9VM_OPT_REMOVE_CONSTANT_POOL_SPLITTING is always true and is planed to be cleaned up, always assume it's true
   virtual void *                varHandleMethodTypeTableEntryAddress(int32_t cpIndex);

   virtual bool                  fieldsAreSame(int32_t, TR_ResolvedMethod *, int32_t, bool &sigSame);
   virtual bool                  staticsAreSame(int32_t, TR_ResolvedMethod *, int32_t, bool &sigSame);

   virtual bool                  fieldAttributes (TR::Compilation *, int32_t cpIndex, uint32_t * fieldOffset, TR::DataType * type, bool * volatileP, bool * isFinal, bool *isPrivate, bool isStore, bool * unresolvedInCP, bool needsAOTValidation);


   virtual bool                  staticAttributes( TR::Compilation *, int32_t cpIndex, void * *, TR::DataType * type, bool * volatileP, bool * isFinal, bool *isPrivate, bool isStore, bool * unresolvedInCP, bool needsAOValidation);

   virtual char *                fieldNameChars(int32_t cpIndex, int32_t & len);
   virtual char *                staticNameChars(int32_t cpIndex, int32_t & len);
   virtual char *                fieldSignatureChars(int32_t cpIndex, int32_t & len);
   virtual char *                staticSignatureChars(int32_t cpIndex, int32_t & len);
   virtual char *                localName (uint32_t slotNumber, uint32_t bcIndex, TR_Memory *);
   virtual char *                localName (uint32_t slotNumber, uint32_t bcIndex, int32_t &len, TR_Memory *);

   virtual TR_OpaqueClassBlock * classOfStatic(int32_t cpIndex, bool returnClassForAOT = false);
   virtual TR_OpaqueClassBlock * classOfMethod();
   virtual uint32_t              classCPIndexOfMethod(uint32_t methodCPIndex);
   virtual void * & addressOfClassOfMethod();

   virtual uint32_t              vTableSlot(uint32_t);

   virtual bool                  isCompilable(TR_Memory *);

   static TR_OpaqueMethodBlock * getVirtualMethod(TR_J9VMBase *fej9, J9ConstantPool *cp, I_32 cpIndex, UDATA *vTableOffset, bool *unresolvedInCP);
   static TR_OpaqueClassBlock  * getInterfaceITableIndexFromCP(TR_J9VMBase *fej9, J9ConstantPool *cp, int32_t cpIndex, uintptrj_t *pITableIndex);

   virtual TR_ResolvedMethod *   getResolvedStaticMethod ( TR::Compilation *, int32_t cpIndex, bool * unresolvedInCP);
   virtual TR_ResolvedMethod *   getResolvedSpecialMethod( TR::Compilation *, int32_t cpIndex, bool * unresolvedInCP);
   virtual TR_ResolvedMethod *   getResolvedVirtualMethod( TR::Compilation *, int32_t cpIndex, bool ignoreRtResolve, bool * unresolvedInCP);
   virtual TR_ResolvedMethod *   getResolvedPossiblyPrivateVirtualMethod( TR::Compilation *, int32_t cpIndex, bool ignoreRtResolve, bool * unresolvedInCP);
   virtual TR_OpaqueClassBlock * getResolvedInterfaceMethod(int32_t cpIndex, uintptrj_t * pITableIndex);

   virtual TR_ResolvedMethod *   getResolvedDynamicMethod( TR::Compilation *, int32_t cpIndex, bool * unresolvedInCP);
   virtual TR_ResolvedMethod *   getResolvedHandleMethod( TR::Compilation *, int32_t cpIndex, bool * unresolvedInCP);
   virtual TR_ResolvedMethod *   getResolvedHandleMethodWithSignature( TR::Compilation *, int32_t cpIndex, char *signature);

   virtual uint32_t              getResolvedInterfaceMethodOffset(TR_OpaqueClassBlock * classObject, int32_t cpIndex);
   virtual TR_ResolvedMethod *   getResolvedImproperInterfaceMethod(TR::Compilation * comp, I_32 cpIndex);
   virtual TR_ResolvedMethod *   getResolvedInterfaceMethod( TR::Compilation *, TR_OpaqueClassBlock * classObject, int32_t cpIndex);
   virtual TR_ResolvedMethod *   getResolvedVirtualMethod( TR::Compilation *, TR_OpaqueClassBlock * classObject, int32_t virtualCallOffset, bool ignoreRtResolve = true);

   virtual bool                  virtualMethodIsOverridden();
   virtual void                  setVirtualMethodIsOverridden();
   virtual void *                addressContainingIsOverriddenBit();
   virtual int32_t               virtualCallSelector(uint32_t cpIndex);

   virtual int32_t               exceptionData(int32_t exceptionNumber, int32_t * startIndex, int32_t * endIndex, int32_t * catchType);
   virtual uint32_t              numberOfExceptionHandlers();

   virtual bool                  isNewInstanceImplThunk();
   virtual bool                  isJNINative();
   virtual bool                  isJITInternalNative();
   virtual bool                  methodIsNotzAAPEligible();

   uintptrj_t                    getJNIProperties() { return _jniProperties; }
   void *                        getJNITargetAddress() {return _jniTargetAddress; }

   virtual TR_OpaqueMethodBlock *getNonPersistentIdentifier();
   virtual TR_OpaqueMethodBlock *getPersistentIdentifier();
   virtual uint8_t *             allocateException(uint32_t, TR::Compilation*);

   virtual const char *          newInstancePrototypeSignature(TR_Memory * m, TR_AllocationKind = heapAlloc);

   J9ExceptionHandler *          exceptionHandler();
   void                          setClassForNewInstance(J9Class *c) { _j9classForNewInstance = c; }
   bool                          fieldIsFromLocalClass(int32_t cpIndex);

   char *fieldOrStaticNameChars      (int32_t cpIndex, int32_t & len);
   char *fieldOrStaticSignatureChars (int32_t cpIndex, int32_t & len);

   /**
    * @brief Create TR::ParameterSymbols from the signature of a method, and add them
    *        to the ParameterList on the ResolvedMethodSymbol.
    *
    * @param[in] methodSym : the ResolvedMethodSymbol to create the parameter list for
    */
   virtual void makeParameterList(TR::ResolvedMethodSymbol *methodSym);

protected:
   virtual TR_J9MethodBase *asJ9Method(){ return this; }

private:
   virtual TR_ResolvedMethod *createResolvedMethodFromJ9Method( TR::Compilation *comp, int32_t cpIndex, uint32_t vTableSlot, J9Method *j9Method, bool * unresolvedInCP, TR_AOTInliningStats *aotStats);

   virtual void                  handleUnresolvedStaticMethodInCP(int32_t cpIndex, bool * unresolvedInCP);
   virtual void                  handleUnresolvedSpecialMethodInCP(int32_t cpIndex, bool * unresolvedInCP);
   virtual void                  handleUnresolvedVirtualMethodInCP(int32_t cpIndex, bool * unresolvedInCP);

   void setRecognizedMethodInfo(TR::RecognizedMethod rm);

   J9Method *              _ramMethod;
   J9ROMMethod *           _romMethod;
   uint32_t                _vTableSlot;
   J9Class *               _j9classForNewInstance;
   uintptrj_t              _jniProperties;
   void *                  _jniTargetAddress;
   int32_t                 _pendingPushSlots;
   };

#if defined(J9VM_INTERP_AOT_COMPILE_SUPPORT)

class TR_ResolvedRelocatableJ9Method : public TR_ResolvedJ9Method
   {
public:
   TR_ResolvedRelocatableJ9Method(TR_OpaqueMethodBlock * aMethod, TR_FrontEnd *, TR_Memory *, TR_ResolvedMethod * owningMethod = 0, uint32_t vTableSlot = 0);

   virtual TR_Method *           convertToMethod();

   virtual void *                constantPool();

   virtual bool                  isStatic();
   virtual bool                  isAbstract();
   virtual bool                  isNative();
   virtual bool                  isSynchronized();
   virtual bool                  isPrivate();
   virtual bool                  isProtected();
   virtual bool                  isPublic();
   virtual bool                  isFinal();
   virtual bool                  isStrictFP();

   virtual bool                  isInterpreted();
   virtual bool                  isInterpretedForHeuristics();
   virtual bool                  hasBackwardBranches();
   virtual bool                  isObjectConstructor();
   virtual bool                  isNonEmptyObjectConstructor();

   virtual void *                startAddressForJittedMethod();
   virtual void *                startAddressForJNIMethod( TR::Compilation *);
   virtual void *                startAddressForJITInternalNativeMethod();
   virtual void *                startAddressForInterpreterOfJittedMethod();


   virtual TR_OpaqueClassBlock * getClassFromConstantPool( TR::Compilation *, uint32_t cpIndex, bool returnClassToAOT = false);
   virtual bool                  validateClassFromConstantPool( TR::Compilation *comp, J9Class *clazz, uint32_t cpIndex, TR_ExternalRelocationTargetKind reloKind = TR_ValidateClass);
   virtual bool                  validateArbitraryClass( TR::Compilation *comp, J9Class *clazz);

   virtual void *                stringConstant(int32_t cpIndex);
   virtual bool                  isUnresolvedString(int32_t cpIndex, bool optimizeForAOT = false);
   virtual void *                methodTypeConstant(int32_t cpIndex);
   virtual bool                  isUnresolvedMethodType(int32_t cpIndex);
   virtual void *                methodHandleConstant(int32_t cpIndex);
   virtual bool                  isUnresolvedMethodHandle(int32_t cpIndex);

   virtual bool                  fieldAttributes ( TR::Compilation *, int32_t cpIndex, uint32_t * fieldOffset, TR::DataType * type, bool * volatileP, bool * isFinal, bool *isPrivate, bool isStore, bool * unresolvedInCP, bool needsAOTValidation);

   virtual bool                  staticAttributes( TR::Compilation *, int32_t cpIndex, void * *, TR::DataType * type, bool * volatileP, bool * isFinal, bool *isPrivate, bool isStore, bool * unresolvedInCP, bool needsAOTValidation);

   virtual int32_t               virtualCallSelector(uint32_t cpIndex);
   virtual char *                fieldSignatureChars(int32_t cpIndex, int32_t & len);
   virtual char *                staticSignatureChars(int32_t cpIndex, int32_t & len);

   virtual TR_OpaqueClassBlock * classOfStatic(int32_t cpIndex, bool returnClassForAOT = false);

   virtual TR_ResolvedMethod *   getResolvedPossiblyPrivateVirtualMethod( TR::Compilation *, int32_t cpIndex, bool ignoreRtResolve, bool * unresolvedInCP);

   virtual bool                  getUnresolvedFieldInCP(int32_t cpIndex);
   virtual bool                  getUnresolvedStaticMethodInCP(int32_t cpIndex);
   virtual bool                  getUnresolvedSpecialMethodInCP(int32_t cpIndex);
   virtual bool                  getUnresolvedVirtualMethodInCP(int32_t cpIndex);

   virtual TR_ResolvedMethod *   getResolvedImproperInterfaceMethod(TR::Compilation * comp, I_32 cpIndex);

   virtual TR_OpaqueMethodBlock *getNonPersistentIdentifier();
   virtual uint8_t *             allocateException(uint32_t, TR::Compilation*);

   virtual TR_OpaqueClassBlock  *getDeclaringClassFromFieldOrStatic( TR::Compilation *comp, int32_t cpIndex);

private:
   virtual TR_ResolvedMethod *   createResolvedMethodFromJ9Method(TR::Compilation *comp, int32_t cpIndex, uint32_t vTableSlot, J9Method *j9Method, bool * unresolvedInCP, TR_AOTInliningStats *aotStats);

   virtual void                  handleUnresolvedStaticMethodInCP(int32_t cpIndex, bool * unresolvedInCP);
   virtual void                  handleUnresolvedSpecialMethodInCP(int32_t cpIndex, bool * unresolvedInCP);
   virtual void                  handleUnresolvedVirtualMethodInCP(int32_t cpIndex, bool * unresolvedInCP);

   bool                          getUnresolvedMethodInCP(TR_OpaqueMethodBlock *aMethod);

   bool                          unresolvedFieldAttributes (int32_t cpIndex, TR::DataType * type, bool * volatileP, bool * isFinal, bool *isPrivate);
   bool                          unresolvedStaticAttributes(int32_t cpIndex, TR::DataType * type, bool * volatileP, bool * isFinal, bool *isPrivate);
   void                          setAttributeResult(bool, bool, uintptr_t, int32_t, int32_t, int32_t, TR::DataType *, bool *, bool *, bool *, void ** );
   char *                        fieldOrStaticNameChars(int32_t cpIndex, int32_t & len);

   J9ExceptionHandler * exceptionHandler();
   };
#endif

#endif
