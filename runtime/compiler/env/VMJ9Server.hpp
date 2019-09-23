/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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

#ifndef VMJ9SERVER_H
#define VMJ9SERVER_H

#include "env/VMJ9.h"

/**
 * @class TR_J9ServerVM
 * @brief Class used by JITServer for querying client-side VM information 
 *
 * This class is an extension of the TR_J9VM class which overrides a number 
 * of TR_J9VM's APIs. TR_J9ServerVM is used by JITServer and the overridden 
 * APIs mostly send remote messages to JITClient to query information from 
 * the TR_J9VM on the client. The information is needed for JITServer to 
 * compile code that is compatible with the client-side VM. To minimize the 
 * number of remote messages as a way to optimize JITServer performance, a 
 * lot of client-side TR_J9VM information are cached on JITServer.
 */

class TR_J9ServerVM: public TR_J9VM
   {
public:
   TR_J9ServerVM(J9JITConfig *jitConfig, TR::CompilationInfo *compInfo, J9VMThread *vmContext)
      :TR_J9VM(jitConfig, compInfo, vmContext)
      {}

   virtual bool storeOffsetToArgumentsInVirtualIndirectThunks() override { return true; }
   virtual bool needsContiguousCodeAndDataCacheAllocation() override     { return true; }
   virtual bool supportsEmbeddedHeapBounds() override                    { return false; }
   virtual bool supportsFastNanoTime() override                          { return false; }
   virtual bool helpersNeedRelocation() override                         { return true; }
   virtual bool canDevirtualizeDispatch() override                       { return true; }
   virtual bool needRelocationsForBodyInfoData() override                { return true; }
   virtual bool needRelocationsForPersistentInfoData() override          { return true; }

   virtual void markHotField(TR::Compilation *, TR::SymbolReference *, TR_OpaqueClassBlock *, bool) override { return; }

   virtual bool isClassLibraryMethod(TR_OpaqueMethodBlock *method, bool vettedForAOT) override;
   virtual bool isClassLibraryClass(TR_OpaqueClassBlock *clazz) override;
   virtual TR_OpaqueClassBlock * getSuperClass(TR_OpaqueClassBlock *classPointer) override;
   virtual bool isSameOrSuperClass(J9Class *superClass, J9Class *subClass) override;
   virtual TR::Method * createMethod(TR_Memory *, TR_OpaqueClassBlock *, int32_t) override;
   virtual TR_ResolvedMethod * createResolvedMethod(TR_Memory * trMemory, TR_OpaqueMethodBlock * aMethod, TR_ResolvedMethod * owningMethod, TR_OpaqueClassBlock *classForNewInstance) override;
   virtual TR_ResolvedMethod * createResolvedMethodWithSignature(TR_Memory * trMemory, TR_OpaqueMethodBlock * aMethod, TR_OpaqueClassBlock *classForNewInstance,
                                                                 char *signature, int32_t signatureLength, TR_ResolvedMethod * owningMethod) override;
   virtual TR_YesNoMaybe isInstanceOf(TR_OpaqueClassBlock * a, TR_OpaqueClassBlock *b, bool objectTypeIsFixed, bool castTypeIsFixed = true, bool optimizeForAOT = false) override;
   //virtual bool isInterfaceClass(TR_OpaqueClassBlock *clazzPointer) override;
   //virtual bool isClassFinal(TR_OpaqueClassBlock *) override;
   //virtual bool isAbstractClass(TR_OpaqueClassBlock *clazzPointer) override;
   virtual TR_OpaqueClassBlock * getSystemClassFromClassName(const char * name, int32_t length, bool isVettedForAOT = false) override;
   virtual bool isMethodTracingEnabled(TR_OpaqueMethodBlock *method) override;
   virtual bool canMethodEnterEventBeHooked() override;
   virtual bool canMethodExitEventBeHooked() override;
   virtual TR_OpaqueClassBlock * getClassClassPointer(TR_OpaqueClassBlock *objectClassPointer) override;
   virtual void * getClassLoader(TR_OpaqueClassBlock * classPointer) override;
   virtual TR_OpaqueClassBlock * getClassOfMethod(TR_OpaqueMethodBlock *method) override;
   virtual TR_OpaqueClassBlock * getBaseComponentClass(TR_OpaqueClassBlock * clazz, int32_t & numDims) override;
   virtual TR_OpaqueClassBlock * getLeafComponentClassFromArrayClass(TR_OpaqueClassBlock * arrayClass) override;
   virtual TR_OpaqueClassBlock * getClassFromSignature(const char *sig, int32_t length, TR_ResolvedMethod *method, bool isVettedForAOT) override;
   virtual TR_OpaqueClassBlock * getClassFromSignature(const char *sig, int32_t length, TR_OpaqueMethodBlock *method, bool isVettedForAOT) override;
   virtual void * getSystemClassLoader() override;
   virtual bool jitFieldsAreSame(TR_ResolvedMethod * method1, I_32 cpIndex1, TR_ResolvedMethod * method2, I_32 cpIndex2, int32_t isStatic) override;
   virtual bool jitStaticsAreSame(TR_ResolvedMethod *method1, I_32 cpIndex1, TR_ResolvedMethod *method2, I_32 cpIndex2) override;
   virtual TR_OpaqueClassBlock * getComponentClassFromArrayClass(TR_OpaqueClassBlock *arrayClass) override;
   virtual bool classHasBeenReplaced(TR_OpaqueClassBlock *) override;
   virtual bool classHasBeenExtended(TR_OpaqueClassBlock *) override;
   virtual bool compiledAsDLTBefore(TR_ResolvedMethod *) override;
   //virtual char * getClassNameChars(TR_OpaqueClassBlock *ramClass, int32_t & length) override;
   virtual uintptrj_t getOverflowSafeAllocSize() override;
   virtual bool isThunkArchetype(J9Method * method) override;
   virtual int32_t printTruncatedSignature(char *sigBuf, int32_t bufLen, TR_OpaqueMethodBlock *method) override;
   //virtual bool isPrimitiveClass(TR_OpaqueClassBlock * clazz) override;
   virtual bool isClassInitialized(TR_OpaqueClassBlock * clazz) override;
   virtual UDATA getOSRFrameSizeInBytes(TR_OpaqueMethodBlock * method) override;
   virtual int32_t getByteOffsetToLockword(TR_OpaqueClassBlock * clazz) override;
   virtual bool isString(TR_OpaqueClassBlock * clazz) override;
   virtual void * getMethods(TR_OpaqueClassBlock * clazz) override;
   virtual void getResolvedMethods(TR_Memory * trMemory, TR_OpaqueClassBlock * classPointer, List<TR_ResolvedMethod> * resolvedMethodsInClass) override;
   //virtual uint32_t getNumMethods(TR_OpaqueClassBlock * clazz) override;
   //virtual uint32_t getNumInnerClasses(TR_OpaqueClassBlock * clazz) override;
   virtual bool isPrimitiveArray(TR_OpaqueClassBlock *clazz) override;
   virtual uint32_t getAllocationSize(TR::StaticSymbol *classSym, TR_OpaqueClassBlock *clazz) override;
   virtual TR_OpaqueClassBlock * getObjectClass(uintptrj_t objectPointer) override;
   virtual uintptrj_t getStaticReferenceFieldAtAddress(uintptrj_t fieldAddress) override;

   virtual bool stackWalkerMaySkipFrames(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *clazz) override;
   virtual bool hasFinalFieldsInClass(TR_OpaqueClassBlock *clazz) override;
   virtual const char *sampleSignature(TR_OpaqueMethodBlock * aMethod, char *buf, int32_t bufLen, TR_Memory *memory) override;
   virtual TR_OpaqueClassBlock * getHostClass(TR_OpaqueClassBlock *clazzOffset) override;
   virtual intptrj_t getStringUTF8Length(uintptrj_t objectPointer) override;
   virtual bool classInitIsFinished(TR_OpaqueClassBlock *) override;
   virtual int32_t getNewArrayTypeFromClass(TR_OpaqueClassBlock *clazz) override;
   virtual TR_OpaqueClassBlock *getClassFromNewArrayType(int32_t arrayType) override;
   virtual TR_OpaqueClassBlock * getClassFromNewArrayTypeNonNull(int32_t arrayType) override;
   virtual bool isCloneable(TR_OpaqueClassBlock *clazzPointer) override;
   virtual bool canAllocateInlineClass(TR_OpaqueClassBlock *clazz) override;
   virtual TR_OpaqueClassBlock * getArrayClassFromComponentClass(TR_OpaqueClassBlock *componentClass) override;
   virtual J9Class * matchRAMclassFromROMclass(J9ROMClass *clazz, TR::Compilation *comp) override;
   virtual int32_t * getCurrentLocalsMapForDLT(TR::Compilation *comp) override;
   virtual uintptrj_t getReferenceFieldAt(uintptrj_t objectPointer, uintptrj_t offsetFromHeader) override;
   virtual uintptrj_t getReferenceFieldAtAddress(uintptrj_t fieldAddress) override;
   virtual uintptrj_t getVolatileReferenceFieldAt(uintptrj_t objectPointer, uintptrj_t fieldOffset) override;
   virtual int32_t getInt32FieldAt(uintptrj_t objectPointer, uintptrj_t fieldOffset) override;
   virtual int64_t getInt64FieldAt(uintptrj_t objectPointer, uintptrj_t fieldOffset) override;
   virtual void setInt64FieldAt(uintptrj_t objectPointer, uintptrj_t fieldOffset, int64_t newValue) override;
   virtual bool compareAndSwapInt64FieldAt(uintptrj_t objectPointer, uintptrj_t fieldOffset, int64_t oldValue, int64_t newValue) override;
   virtual intptrj_t getArrayLengthInElements(uintptrj_t objectPointer) override;
   virtual TR_OpaqueClassBlock * getClassFromJavaLangClass(uintptrj_t objectPointer) override;
   virtual UDATA getOffsetOfClassFromJavaLangClassField() override;
   virtual uintptrj_t getConstantPoolFromMethod(TR_OpaqueMethodBlock *method) override;
   virtual uintptrj_t getConstantPoolFromClass(TR_OpaqueClassBlock *clazz) override;
   virtual uintptrj_t getProcessID() override;
   virtual UDATA getIdentityHashSaltPolicy() override;
   virtual UDATA getOffsetOfJLThreadJ9Thread() override;
   virtual bool scanReferenceSlotsInClassForOffset(TR::Compilation *comp, TR_OpaqueClassBlock *clazz, int32_t offset) override;
   virtual int32_t findFirstHotFieldTenuredClassOffset(TR::Compilation *comp, TR_OpaqueClassBlock *clazz) override;
   virtual TR_OpaqueMethodBlock *getResolvedVirtualMethod(TR_OpaqueClassBlock * classObject, int32_t cpIndex, bool ignoreReResolve = true) override;
   virtual TR::CodeCache *getDesignatedCodeCache(TR::Compilation *comp) override;
   virtual uint8_t *allocateCodeMemory(TR::Compilation * comp, uint32_t warmCodeSize, uint32_t coldCodeSize, uint8_t ** coldCode, bool isMethodHeaderNeeded) override;
   virtual bool sameClassLoaders(TR_OpaqueClassBlock *, TR_OpaqueClassBlock *) override;
   virtual bool isUnloadAssumptionRequired(TR_OpaqueClassBlock *, TR_ResolvedMethod *) override;
   virtual void *getJ2IThunk(char *signatureChars, uint32_t signatureLength,  TR::Compilation *comp) override;
   virtual void *setJ2IThunk(char *signatureChars, uint32_t signatureLength, void *thunkptr,  TR::Compilation *comp) override;
   virtual uint32_t getInstanceFieldOffset(TR_OpaqueClassBlock *clazz, char *fieldName, uint32_t fieldLen, char *sig, uint32_t sigLen, UDATA options) override;
   virtual int32_t getJavaLangClassHashCode(TR::Compilation *comp, TR_OpaqueClassBlock *clazz, bool &hashCodeComputed) override;
   virtual bool hasFinalizer(TR_OpaqueClassBlock *clazz) override;
   virtual uintptrj_t getClassDepthAndFlagsValue(TR_OpaqueClassBlock *clazz) override;
   virtual uintptrj_t getClassFlagsValue(TR_OpaqueClassBlock * clazz) override;
   virtual TR_OpaqueMethodBlock *getMethodFromName(char *className, char *methodName, char *signature) override;
   virtual TR_OpaqueMethodBlock *getMethodFromClass(TR_OpaqueClassBlock *methodClass, char *methodName, char *signature, TR_OpaqueClassBlock *callingClass) override;
   virtual bool isClassVisible(TR_OpaqueClassBlock *sourceClass, TR_OpaqueClassBlock *destClass) override;
   virtual void markClassForTenuredAlignment(TR::Compilation *comp, TR_OpaqueClassBlock *clazz, uint32_t alignFromStart) override;
   virtual int32_t * getReferenceSlotsInClass(TR::Compilation *comp, TR_OpaqueClassBlock *clazz) override;
   virtual uint32_t getMethodSize(TR_OpaqueMethodBlock *method) override;
   virtual void * addressOfFirstClassStatic(TR_OpaqueClassBlock *clazz) override;
   virtual void * getStaticFieldAddress(TR_OpaqueClassBlock *clazz, unsigned char *fieldName, uint32_t fieldLen, unsigned char *sig, uint32_t sigLen) override;
   virtual int32_t getInterpreterVTableSlot(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *clazz) override;
   virtual void revertToInterpreted(TR_OpaqueMethodBlock *method) override;
   virtual void * getLocationOfClassLoaderObjectPointer(TR_OpaqueClassBlock *clazz) override;
   virtual bool isOwnableSyncClass(TR_OpaqueClassBlock *clazz) override;
   virtual TR_OpaqueClassBlock * getClassFromMethodBlock(TR_OpaqueMethodBlock *method) override;
   virtual U_8 * fetchMethodExtendedFlagsPointer(J9Method *method) override;
   virtual void * getStaticHookAddress(int32_t event) override;
   virtual bool stringEquals(TR::Compilation *comp, uintptrj_t* stringLocation1, uintptrj_t*stringLocation2, int32_t& result) override;
   virtual bool getStringHashCode(TR::Compilation *comp, uintptrj_t* stringLocation, int32_t& result) override;
   virtual int32_t getLineNumberForMethodAndByteCodeIndex(TR_OpaqueMethodBlock *method, int32_t bcIndex) override;
   virtual TR_OpaqueMethodBlock * getObjectNewInstanceImplMethod() override;
   virtual uintptrj_t getBytecodePC(TR_OpaqueMethodBlock *method, TR_ByteCodeInfo &bcInfo) override;
   virtual bool isClassLoadedBySystemClassLoader(TR_OpaqueClassBlock *clazz) override;
   virtual void setInvokeExactJ2IThunk(void *thunkptr, TR::Compilation *comp) override;
   virtual bool needsInvokeExactJ2IThunk(TR::Node *node,  TR::Compilation *comp) override;
   virtual TR_ResolvedMethod *createMethodHandleArchetypeSpecimen(TR_Memory *, uintptrj_t *methodHandleLocation, TR_ResolvedMethod *owningMethod = 0) override;
   virtual intptrj_t getVFTEntry(TR_OpaqueClassBlock *clazz, int32_t offset) override;
   virtual bool isClassArray(TR_OpaqueClassBlock *klass) override;
   virtual uintptrj_t getFieldOffset(TR::Compilation * comp, TR::SymbolReference* classRef, TR::SymbolReference* fieldRef) override { return 0; } // safe answer
   virtual bool canDereferenceAtCompileTime(TR::SymbolReference *fieldRef,  TR::Compilation *comp) { return false; } // safe answer, might change in the future
   virtual bool instanceOfOrCheckCast(J9Class *instanceClass, J9Class* castClass) override;
   virtual bool transformJlrMethodInvoke(J9Method *callerMethod, J9Class *callerClass) override;
   using TR_J9VM :: isAnonymousClass;
   virtual bool isAnonymousClass(TR_OpaqueClassBlock *j9clazz) override;
   virtual TR_IProfiler *getIProfiler() override;
   virtual TR_StaticFinalData dereferenceStaticFinalAddress(void *staticAddress, TR::DataType addressType) override;
   virtual void reserveTrampolineIfNecessary( TR::Compilation *, TR::SymbolReference *symRef, bool inBinaryEncoding) override;
   virtual intptrj_t methodTrampolineLookup( TR::Compilation *, TR::SymbolReference *symRef, void *callSite) override;
   virtual uintptrj_t getPersistentClassPointerFromClassPointer(TR_OpaqueClassBlock * clazz) override;
   virtual bool isStringCompressionEnabledVM() override;
   virtual TR_OpaqueClassBlock *getClassFromCP(J9ConstantPool *cp) override;
   virtual J9ROMMethod *getROMMethodFromRAMMethod(J9Method *ramMethod) override;
   virtual bool getReportByteCodeInfoAtCatchBlock() override;
   virtual void *getInvokeExactThunkHelperAddress(TR::Compilation *comp, TR::SymbolReference *glueSymRef, TR::DataType dataType) override;

protected:
   void getResolvedMethodsAndMethods(TR_Memory *trMemory, TR_OpaqueClassBlock *classPointer, List<TR_ResolvedMethod> *resolvedMethodsInClass, J9Method **methods, uint32_t *numMethods);
   bool getCachedField(J9Class *ramClass, int32_t cpIndex, J9Class **declaringClass, UDATA *field);
   void cacheField(J9Class *ramClass, int32_t cpIndex, J9Class *declaringClass, UDATA field);
   };


/**
 * @class TR_J9SharedCacheServerVM
 * @brief Class used by JITServer for querying client-side VM information with
 * additional handling for AOT compilation
 *
 * This class is an extension of the TR_J9ServerVM class. This class conceptually
 * does very similar things compared to TR_J9ServerVM except it's used for AOT 
 * compilation.
 */

class TR_J9SharedCacheServerVM: public TR_J9ServerVM
   {
public:
   TR_J9SharedCacheServerVM(J9JITConfig *jitConfig, TR::CompilationInfo *compInfo, J9VMThread *vmContext)
      :TR_J9ServerVM(jitConfig, compInfo, vmContext)
      {}

   // in process of removing this query in favour of more meaningful queries below
   virtual bool       isAOT_DEPRECATED_DO_NOT_USE()  override                  { return true; }

   // replacing calls to isAOT
   virtual bool       canUseSymbolValidationManager() override                 { return true; }
   virtual bool       supportsCodeCacheSnippets() override                     { return false; }
   virtual bool       canRelocateDirectNativeCalls() override                  { return false; }
   virtual bool       needClassAndMethodPointerRelocations() override          { return true; }
   virtual bool       inlinedAllocationsMustBeVerified() override              { return true; }
   virtual bool       helpersNeedRelocation() override                         { return true; }
   virtual bool       supportsEmbeddedHeapBounds() override                    { return false; }
   virtual bool       supportsFastNanoTime() override                          { return false; }
   virtual bool       needRelocationsForStatics() override                     { return true; }
   virtual bool       needRelocationsForBodyInfoData() override                { return true; }
   virtual bool       needRelocationsForPersistentInfoData() override          { return true; }
   virtual bool       forceUnresolvedDispatch() override                       { return true; }
   virtual bool       nopsAlsoProcessedByRelocations() override                { return true; }
   virtual bool       supportsGuardMerging() override                          { return false; }
   virtual bool       canDevirtualizeDispatch() override                       { return false; }
   virtual bool       storeOffsetToArgumentsInVirtualIndirectThunks() override { return true; }
   virtual bool       callTargetsNeedRelocations() override                    { return true; }
   virtual bool       doStringPeepholing() override                            { return false; }
   virtual bool       hardwareProfilingInstructionsNeedRelocation() override   { return true; }
   virtual bool       supportsMethodEntryPadding() override                    { return false; }
   virtual bool       isBenefitInliningCheckIfFinalizeObject() override        { return true; }
   virtual bool       needsContiguousCodeAndDataCacheAllocation() override     { return true; }

   virtual bool shouldDelayAotLoad() override                                  { return true; }
   virtual bool isClassVisible(TR_OpaqueClassBlock * sourceClass, TR_OpaqueClassBlock * destClass) override;
   virtual bool stackWalkerMaySkipFrames(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *methodClass) override;
   virtual bool isMethodTracingEnabled(TR_OpaqueMethodBlock *method) override;
   virtual bool traceableMethodsCanBeInlined() override;
   virtual bool canMethodEnterEventBeHooked() override;
   virtual bool canMethodExitEventBeHooked() override;
   virtual bool methodsCanBeInlinedEvenIfEventHooksEnabled() override;
   virtual int32_t getJavaLangClassHashCode(TR::Compilation * comp, TR_OpaqueClassBlock * clazzPointer, bool &hashCodeComputed) override;
   virtual bool javaLangClassGetModifiersImpl(TR_OpaqueClassBlock * clazzPointer, int32_t &result) override;
   virtual uint32_t getInstanceFieldOffset(TR_OpaqueClassBlock * classPointer, char * fieldName, uint32_t fieldLen, char * sig, uint32_t sigLen, UDATA options) override;
   virtual TR_OpaqueClassBlock * getClassOfMethod(TR_OpaqueMethodBlock *method) override;
   virtual TR_OpaqueClassBlock * getSuperClass(TR_OpaqueClassBlock *classPointer) override;
   virtual void getResolvedMethods(TR_Memory *, TR_OpaqueClassBlock *, List<TR_ResolvedMethod> *) override;
   virtual TR_ResolvedMethod * getResolvedMethodForNameAndSignature(TR_Memory * trMemory, TR_OpaqueClassBlock * classPointer, const char* methodName, const char *signature) override;
   virtual bool isClassLibraryMethod(TR_OpaqueMethodBlock *method, bool vettedForAOT = false) override;
   virtual TR_OpaqueMethodBlock * getMethodFromClass(TR_OpaqueClassBlock *, char *, char *, TR_OpaqueClassBlock * = NULL) override;
   virtual bool supportAllocationInlining(TR::Compilation *comp, TR::Node *node) override;
   virtual TR_YesNoMaybe isInstanceOf(TR_OpaqueClassBlock *instanceClass, TR_OpaqueClassBlock *castClass, bool instanceIsFixed, bool castIsFixed = true, bool optimizeForAOT = false) override;
   virtual TR_OpaqueClassBlock * getClassFromSignature(const char * sig, int32_t length, TR_ResolvedMethod *method, bool isVettedForAOT = false) override;
   virtual TR_OpaqueClassBlock * getClassFromSignature(const char * sig, int32_t length, TR_OpaqueMethodBlock *method, bool isVettedForAOT = false) override;
   virtual TR_OpaqueClassBlock * getSystemClassFromClassName(const char * name, int32_t length, bool isVettedForAOT = false) override;
   virtual TR_OpaqueClassBlock * getProfiledClassFromProfiledInfo(TR_ExtraAddressInfo *profiledInfo) override;
   virtual bool isPublicClass(TR_OpaqueClassBlock *clazz) override;
   virtual bool hasFinalizer(TR_OpaqueClassBlock * classPointer) override;
   virtual uintptrj_t getClassDepthAndFlagsValue(TR_OpaqueClassBlock * classPointer) override;
   virtual uintptrj_t getClassFlagsValue(TR_OpaqueClassBlock * classPointer) override;
   virtual bool isPrimitiveClass(TR_OpaqueClassBlock *clazzPointer) override;
   virtual TR_OpaqueClassBlock * getComponentClassFromArrayClass(TR_OpaqueClassBlock * arrayClass) override;
   virtual TR_OpaqueClassBlock * getArrayClassFromComponentClass(TR_OpaqueClassBlock *componentClass) override;
   virtual TR_OpaqueClassBlock * getLeafComponentClassFromArrayClass(TR_OpaqueClassBlock * arrayClass) override;
   virtual TR_OpaqueClassBlock * getBaseComponentClass(TR_OpaqueClassBlock * clazz, int32_t & numDims) override;
   virtual TR_OpaqueClassBlock * getClassFromNewArrayType(int32_t arrayType) override;
   virtual bool isPrimitiveArray(TR_OpaqueClassBlock *) override;
   virtual bool isReferenceArray(TR_OpaqueClassBlock *) override;
   virtual TR_OpaqueMethodBlock * getInlinedCallSiteMethod(TR_InlinedCallSite *ics) override;
   virtual bool sameClassLoaders(TR_OpaqueClassBlock *, TR_OpaqueClassBlock *) override { return false; }
   virtual bool isUnloadAssumptionRequired(TR_OpaqueClassBlock *, TR_ResolvedMethod *) override { return true; }
   virtual bool classHasBeenExtended(TR_OpaqueClassBlock *) override { return true; }
   virtual bool isGetImplInliningSupported() override { return false; }
   virtual TR_ResolvedMethod * getObjectNewInstanceImplMethod(TR_Memory *) override { return NULL; }
   virtual TR::CodeCache * getResolvedTrampoline(TR::Compilation *, TR::CodeCache* curCache, J9Method * method, bool inBinaryEncoding) override { return 0; }
   virtual intptrj_t methodTrampolineLookup(TR::Compilation *, TR::SymbolReference *symRef, void *callSite) override { TR_ASSERT(0, "methodTrampolineLookup not implemented for AOT");  return 0; }
   virtual TR::CodeCache * getDesignatedCodeCache(TR::Compilation *comp) override;
   virtual void * persistJ2IThunk(void *thunk) override { TR_ASSERT(0, "persistJ2IThunk should not be called on the server"); return NULL; }
   virtual void * persistThunk(char *signatureChars, uint32_t signatureLength, uint8_t *thunkStart, uint32_t totalSize) { TR_ASSERT(0, "persistThunk should not be called on the server"); return NULL; }
   virtual void *findPersistentThunk(char *signatureChars, uint32_t signatureLength) override { TR_ASSERT(0, "findPersistentThunk should not be called on the server"); return NULL; }
   virtual J9Class * getClassForAllocationInlining(TR::Compilation *comp, TR::SymbolReference *classSymRef) override;
   virtual TR_OpaqueMethodBlock *getMethodFromName(char *className, char *methodName, char *signature) override;
   virtual TR_OpaqueMethodBlock *getResolvedVirtualMethod(TR_OpaqueClassBlock * classObject, int32_t cpIndex, bool ignoreReResolve = true) override;
   virtual TR_OpaqueMethodBlock *getResolvedInterfaceMethod(TR_OpaqueMethodBlock *ownerMethod, TR_OpaqueClassBlock * classObject, int32_t cpIndex) override;

protected :
   bool validateClass(TR_OpaqueMethodBlock * method, TR_OpaqueClassBlock* j9class, bool isVettedForAOT);
 
   };

#endif // VMJ9SERVER_H
