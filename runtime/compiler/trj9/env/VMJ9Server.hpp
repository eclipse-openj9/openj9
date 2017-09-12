#ifndef VMJ9SERVER_H
#define VMJ9SERVER_H

#include "env/VMJ9.h"

class TR_J9ServerVM: public TR_J9SharedCacheVM
   {
public:
   TR_J9ServerVM(J9JITConfig *jitConfig, TR::CompilationInfo *compInfo, J9VMThread *vmContext)
      :TR_J9SharedCacheVM(jitConfig, compInfo, vmContext)
      {}

   virtual bool needClassAndMethodPointerRelocations() { return false; }

   virtual bool isClassLibraryMethod(TR_OpaqueMethodBlock *method, bool vettedForAOT) override;
   virtual bool isClassLibraryClass(TR_OpaqueClassBlock *clazz) override;
   virtual TR_OpaqueClassBlock * getSuperClass(TR_OpaqueClassBlock *classPointer) override;
   virtual TR_Method * createMethod(TR_Memory *, TR_OpaqueClassBlock *, int32_t) override;
   virtual TR_ResolvedMethod * createResolvedMethod(TR_Memory * trMemory, TR_OpaqueMethodBlock * aMethod, TR_ResolvedMethod * owningMethod, TR_OpaqueClassBlock *classForNewInstance) override;
   virtual TR_ResolvedMethod * createResolvedMethodWithSignature(TR_Memory * trMemory, TR_OpaqueMethodBlock * aMethod, TR_OpaqueClassBlock *classForNewInstance,
                                                                 char *signature, int32_t signatureLength, TR_ResolvedMethod * owningMethod) override;
   virtual TR_YesNoMaybe isInstanceOf(TR_OpaqueClassBlock * a, TR_OpaqueClassBlock *b, bool objectTypeIsFixed, bool castTypeIsFixed, bool optimizeForAOT) override;
   virtual bool isInterfaceClass(TR_OpaqueClassBlock *clazzPointer) override;
   virtual bool isClassArray(TR_OpaqueClassBlock *) override;
   virtual bool isClassFinal(TR_OpaqueClassBlock *) override;
   virtual bool isAbstractClass(TR_OpaqueClassBlock *clazzPointer) override;
   virtual TR_OpaqueClassBlock * getSystemClassFromClassName(const char * name, int32_t length, bool isVettedForAOT) override;
   virtual bool isMethodEnterTracingEnabled(TR_OpaqueMethodBlock *method) override;
   virtual bool isMethodExitTracingEnabled(TR_OpaqueMethodBlock *method) override;
   virtual TR_OpaqueClassBlock * getClassClassPointer(TR_OpaqueClassBlock *objectClassPointer) override;
   virtual void * getClassLoader(TR_OpaqueClassBlock * classPointer) override;
   virtual TR_OpaqueClassBlock * getClassOfMethod(TR_OpaqueMethodBlock *method) override;
   virtual TR_OpaqueClassBlock * getBaseComponentClass(TR_OpaqueClassBlock * clazz, int32_t & numDims) override;
   virtual TR_OpaqueClassBlock * getLeafComponentClassFromArrayClass(TR_OpaqueClassBlock * arrayClass) override;
   virtual TR_OpaqueClassBlock * getClassFromSignature(const char *sig, int32_t length, TR_OpaqueMethodBlock *method, bool isVettedForAOT) override;
   virtual bool jitFieldsAreSame(TR_ResolvedMethod * method1, I_32 cpIndex1, TR_ResolvedMethod * method2, I_32 cpIndex2, int32_t isStatic) override;
   virtual bool jitStaticsAreSame(TR_ResolvedMethod *method1, I_32 cpIndex1, TR_ResolvedMethod *method2, I_32 cpIndex2) override;
   virtual TR_OpaqueClassBlock * getComponentClassFromArrayClass(TR_OpaqueClassBlock *arrayClass) override;
   virtual bool classHasBeenReplaced(TR_OpaqueClassBlock *) override;
   virtual bool classHasBeenExtended(TR_OpaqueClassBlock *) override;
   virtual bool compiledAsDLTBefore(TR_ResolvedMethod *) override;
   virtual char * getClassNameChars(TR_OpaqueClassBlock *ramClass, int32_t & length) override;
   virtual uintptrj_t getOverflowSafeAllocSize() override;
   virtual bool isThunkArchetype(J9Method * method) override;
   virtual int32_t printTruncatedSignature(char *sigBuf, int32_t bufLen, TR_OpaqueMethodBlock *method) override;
   virtual bool isPrimitiveClass(TR_OpaqueClassBlock * clazz) override;
   virtual bool isClassInitialized(TR_OpaqueClassBlock * clazz) override;
   virtual UDATA getOSRFrameSizeInBytes(TR_OpaqueMethodBlock * method) override;
   virtual int32_t getByteOffsetToLockword(TR_OpaqueClassBlock * clazz) override;
   virtual bool isString(TR_OpaqueClassBlock * clazz) override;
   virtual void * getMethods(TR_OpaqueClassBlock * clazz) override;
   virtual uint32_t getNumMethods(TR_OpaqueClassBlock * clazz) override;
   virtual uint32_t getNumInnerClasses(TR_OpaqueClassBlock * clazz) override;
   virtual bool isPrimitiveArray(TR_OpaqueClassBlock *clazz) override;
   virtual uint32_t getAllocationSize(TR::StaticSymbol *classSym, TR_OpaqueClassBlock *clazz) override;
   virtual TR_OpaqueClassBlock * getObjectClass(uintptrj_t objectPointer) override;
   virtual bool stackWalkerMaySkipFrames(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *clazz) override;
   virtual bool hasFinalFieldsInClass(TR_OpaqueClassBlock *clazz) override;
   virtual const char *sampleSignature(TR_OpaqueMethodBlock * aMethod, char *buf, int32_t bufLen, TR_Memory *memory) override;
   virtual TR_OpaqueClassBlock * getHostClass(TR_OpaqueClassBlock *clazzOffset) override;
   virtual intptrj_t getStringUTF8Length(uintptrj_t objectPointer) override;
   virtual uintptrj_t getPersistentClassPointerFromClassPointer(TR_OpaqueClassBlock *) override;
   virtual bool classInitIsFinished(TR_OpaqueClassBlock *) override;
   virtual TR_OpaqueClassBlock *getClassFromNewArrayType(int32_t arrayType) override;
   virtual bool isCloneable(TR_OpaqueClassBlock *clazzPointer) override;
   virtual bool canAllocateInlineClass(TR_OpaqueClassBlock *clazz) override;
   virtual TR_OpaqueClassBlock * getArrayClassFromComponentClass(TR_OpaqueClassBlock *componentClass) override;
   virtual J9Class * matchRAMclassFromROMclass(J9ROMClass *clazz, TR::Compilation *comp) override;
   virtual int32_t * getCurrentLocalsMapForDLT(TR::Compilation *comp) override;
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
   virtual uintptrj_t getProcessID() override;
   virtual UDATA getIdentityHashSaltPolicy() override;
   virtual UDATA getOffsetOfJLThreadJ9Thread() override;
   virtual bool scanReferenceSlotsInClassForOffset(TR::Compilation *comp, TR_OpaqueClassBlock *clazz, int32_t offset) override;
   virtual int32_t findFirstHotFieldTenuredClassOffset(TR::Compilation *comp, TR_OpaqueClassBlock *clazz) override;
   virtual TR_OpaqueMethodBlock *getResolvedVirtualMethod(TR_OpaqueClassBlock * classObject, int32_t cpIndex, bool ignoreReResolve = true) override;
   };

#endif // VMJ9SERVER_H
