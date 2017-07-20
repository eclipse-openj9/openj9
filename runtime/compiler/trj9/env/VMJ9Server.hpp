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
   virtual TR_Method * createMethod(TR_Memory *, TR_OpaqueClassBlock *, int32_t) override;
   };

#endif // VMJ9SERVER_H
