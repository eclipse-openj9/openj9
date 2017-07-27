#ifndef VMJ9SERVER_H
#define VMJ9SERVER_H

#include "env/VMJ9.h"

class TR_J9ServerVM: public TR_J9SharedCacheVM
   {
public:
   TR_J9ServerVM(J9JITConfig *jitConfig, TR::CompilationInfo *compInfo, J9VMThread *vmContext)
      :TR_J9SharedCacheVM(jitConfig, compInfo, vmContext)
      {}

   virtual bool isClassLibraryMethod(TR_OpaqueMethodBlock *method, bool vettedForAOT) override;
   virtual bool isClassLibraryClass(TR_OpaqueClassBlock *clazz) override;
   virtual TR_OpaqueClassBlock * getSuperClass(TR_OpaqueClassBlock *classPointer) override;
   virtual TR_Method * createMethod(TR_Memory *, TR_OpaqueClassBlock *, int32_t) override;
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
   };

#endif // VMJ9SERVER_H
