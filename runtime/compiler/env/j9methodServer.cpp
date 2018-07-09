#include "rpc/J9Server.hpp"
#include "j9methodServer.hpp"
#include "control/CompilationRuntime.hpp"
#include "control/CompilationThread.hpp"
#include "control/MethodToBeCompiled.hpp"
#include "control/JITaaSCompilationThread.hpp"

ClientSessionData::ClassInfo &
getJ9ClassInfo(TR::CompilationInfoPerThread *threadCompInfo, J9Class *clazz)
   {
   // This function assumes that you are inside of _romMapMonitor
   // Do not use it otherwise
   auto &classMap = threadCompInfo->getClientData()->getROMClassMap();
   auto it = classMap.find(clazz);
   TR_ASSERT(it != classMap.end(), "ClassInfo is not in the class map");
   return it->second;
   }

J9ROMClass *
TR_ResolvedJ9JITaaSServerMethod::romClassFromString(const std::string &romClassStr, TR_PersistentMemory *trMemory)
   {
   auto romClass = (J9ROMClass *)(trMemory->allocatePersistentMemory(romClassStr.size(), TR_Memory::ROMClass));
   if (!romClass)
      throw std::bad_alloc();
   memcpy(romClass, &romClassStr[0], romClassStr.size());
   return romClass;
   }

static J9ROMMethod *
romMethodAtClassIndex(J9ROMClass *romClass, uint64_t methodIndex)
   {
   J9ROMMethod * romMethod = J9ROMCLASS_ROMMETHODS(romClass);
   for (size_t i = methodIndex; i; --i)
      romMethod = nextROMMethod(romMethod);
   return romMethod;
   }

J9ROMClass *
TR_ResolvedJ9JITaaSServerMethod::getRemoteROMClass(J9Class *clazz, JITaaS::J9ServerStream *stream, TR_Memory *trMemory, J9Method **methods, TR_OpaqueClassBlock **baseClass, int32_t *numDims, TR_OpaqueClassBlock **parentClass, PersistentVector<TR_OpaqueClassBlock *> **interfaces)
   {
   stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_getRemoteROMClassAndMethods, clazz);
   auto recv  = stream->read<std::string, J9Method*, TR_OpaqueClassBlock*, int32_t, TR_OpaqueClassBlock *, std::vector<TR_OpaqueClassBlock *>>();
   auto &str  = std::get<0>(recv);
   *methods   = std::get<1>(recv);
   *baseClass = std::get<2>(recv);
   *numDims   = std::get<3>(recv);
   *parentClass = std::get<4>(recv);
   std::vector<TR_OpaqueClassBlock *> tmpInterfaces = std::get<5>(recv); 
   *interfaces = new (PERSISTENT_NEW) PersistentVector<TR_OpaqueClassBlock *>
      (tmpInterfaces.begin(), tmpInterfaces.end(),
       PersistentVector<TR_OpaqueClassBlock *>::allocator_type(TR::Compiler->persistentAllocator()));
   return romClassFromString(str, trMemory->trPersistentMemory());
   }

TR_ResolvedJ9JITaaSServerMethod::TR_ResolvedJ9JITaaSServerMethod(TR_OpaqueMethodBlock * aMethod, TR_FrontEnd * fe, TR_Memory * trMemory, TR_ResolvedMethod * owningMethod, uint32_t vTableSlot)
   : TR_ResolvedJ9Method(fe, owningMethod),
   _fieldAttributesCache(decltype(_fieldAttributesCache)::allocator_type(trMemory->heapMemoryRegion())),
   _staticAttributesCache(decltype(_staticAttributesCache)::allocator_type(trMemory->heapMemoryRegion()))
   {
   TR_J9VMBase *j9fe = (TR_J9VMBase *)fe;
   TR::CompilationInfo *compInfo = TR::CompilationInfo::get(j9fe->getJ9JITConfig());
   TR::CompilationInfoPerThread *threadCompInfo = compInfo->getCompInfoForThread(j9fe->vmThread());
   _stream = threadCompInfo->getMethodBeingCompiled()->_stream;

   // Create client side mirror of this object to use for calls involving RAM data
   TR_ResolvedJ9Method* owningMethodMirror = owningMethod ? ((TR_ResolvedJ9JITaaSServerMethod*) owningMethod)->_remoteMirror : nullptr;
   _stream->write(JITaaS::J9ServerMessageType::mirrorResolvedJ9Method, aMethod, owningMethodMirror, vTableSlot);
   auto recv = _stream->read<TR_ResolvedJ9JITaaSServerMethodInfo>();
   auto &methodInfo = std::get<0>(recv);
   setAttributes(aMethod, fe, trMemory, vTableSlot, threadCompInfo, methodInfo);
   }

TR_ResolvedJ9JITaaSServerMethod::TR_ResolvedJ9JITaaSServerMethod(TR_OpaqueMethodBlock * aMethod, TR_FrontEnd * fe, TR_Memory * trMemory, const TR_ResolvedJ9JITaaSServerMethodInfo &methodInfo, TR_ResolvedMethod * owningMethod, uint32_t vTableSlot)
   : TR_ResolvedJ9Method(fe, owningMethod),
   _fieldAttributesCache(decltype(_fieldAttributesCache)::allocator_type(trMemory->heapMemoryRegion())),
   _staticAttributesCache(decltype(_staticAttributesCache)::allocator_type(trMemory->heapMemoryRegion()))
   {
   // Mirror has already been created, its parameters are passed in methodInfo
   TR_J9VMBase *j9fe = (TR_J9VMBase *)fe;
   TR::CompilationInfo *compInfo = TR::CompilationInfo::get(j9fe->getJ9JITConfig());
   TR::CompilationInfoPerThread *threadCompInfo = compInfo->getCompInfoForThread(j9fe->vmThread());
   _stream = threadCompInfo->getMethodBeingCompiled()->_stream;
   setAttributes(aMethod, fe, trMemory, vTableSlot, threadCompInfo, methodInfo);
   }

J9ROMClass *
TR_ResolvedJ9JITaaSServerMethod::romClassPtr()
   {
   return _romClass;
   }

J9RAMConstantPoolItem *
TR_ResolvedJ9JITaaSServerMethod::literals()
   {
   return _literals;
   }

J9Class *
TR_ResolvedJ9JITaaSServerMethod::constantPoolHdr()
   {
   return _ramClass;
   }

bool
TR_ResolvedJ9JITaaSServerMethod::isJNINative()
   {
   if (supportsFastJNI(_fe))
      return _jniTargetAddress != NULL;
   _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_isJNINative, _remoteMirror);
   return std::get<0>(_stream->read<bool>());
   }


void
TR_ResolvedJ9JITaaSServerMethod::setRecognizedMethodInfo(TR::RecognizedMethod rm)
   {
   TR_ResolvedJ9Method::setRecognizedMethodInfo(rm);
   _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_setRecognizedMethodInfo, _remoteMirror, rm);
   _stream->read<JITaaS::Void>();
   }

J9ClassLoader *
TR_ResolvedJ9JITaaSServerMethod::getClassLoader()
   {
   return _classLoader; // cached version
   }

bool
TR_ResolvedJ9JITaaSServerMethod::staticAttributes(TR::Compilation * comp, I_32 cpIndex, void * * address, TR::DataType * type, bool * volatileP, bool * isFinal, bool * isPrivate, bool isStore, bool * unresolvedInCP, bool needAOTValidation)
   {
   // only make a query if it's not in the cache
   auto gotAttrs = _staticAttributesCache.find(cpIndex); 
   if (gotAttrs == _staticAttributesCache.end())
      {
      _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_staticAttributes, _remoteMirror, cpIndex, isStore, needAOTValidation);
      auto recv = _stream->read<TR_J9MethodFieldAttributes>();
      gotAttrs = _staticAttributesCache.insert({cpIndex, std::get<0>(recv)}).first;
      }
   // access attributes from the cache
   auto attrs = gotAttrs->second;
   *address = attrs.address;
   *type = attrs.type;
   *volatileP = attrs.volatileP;
   if (isFinal) *isFinal = attrs.isFinal;
   if (isPrivate) *isPrivate = attrs.isPrivate;
   if (unresolvedInCP) *unresolvedInCP = attrs.unresolvedInCP;
   return attrs.result;
   }

TR_OpaqueClassBlock *
TR_ResolvedJ9JITaaSServerMethod::getClassFromConstantPool(TR::Compilation * comp, uint32_t cpIndex, bool)
   {
   _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_getClassFromConstantPool, _remoteMirror, cpIndex);
   return std::get<0>(_stream->read<TR_OpaqueClassBlock *>());
   }

TR_OpaqueClassBlock *
TR_ResolvedJ9JITaaSServerMethod::getDeclaringClassFromFieldOrStatic(TR::Compilation *comp, int32_t cpIndex)
   {
   _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_getDeclaringClassFromFieldOrStatic, _remoteMirror, cpIndex);
   return std::get<0>(_stream->read<TR_OpaqueClassBlock *>());
   }

TR_OpaqueClassBlock *
TR_ResolvedJ9JITaaSServerMethod::classOfStatic(I_32 cpIndex, bool returnClassForAOT)
   {
   _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_classOfStatic, _remoteMirror, cpIndex, returnClassForAOT);
   return std::get<0>(_stream->read<TR_OpaqueClassBlock *>());
   }

bool
TR_ResolvedJ9JITaaSServerMethod::isUnresolvedString(I_32 cpIndex, bool optimizeForAOT)
   {
   return true;
   }

TR_ResolvedMethod *
TR_ResolvedJ9JITaaSServerMethod::getResolvedVirtualMethod(TR::Compilation * comp, I_32 cpIndex, bool ignoreRtResolve, bool * unresolvedInCP)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
#if TURN_OFF_INLINING
   return 0;
#else
   INCREMENT_COUNTER(_fe, totalVirtualMethodRefs);

   TR_ResolvedMethod *resolvedMethod = NULL;

   // See if the constant pool entry is already resolved or not
   if (unresolvedInCP)
       *unresolvedInCP = true;

   if (!((_fe->_jitConfig->runtimeFlags & J9JIT_RUNTIME_RESOLVE) &&
         !comp->ilGenRequest().details().isMethodHandleThunk() && // cmvc 195373
         performTransformation(comp, "Setting as unresolved virtual call cpIndex=%d\n",cpIndex) ) || ignoreRtResolve)
      {
      _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_getResolvedVirtualMethodAndMirror, (TR_ResolvedMethod *) _remoteMirror, literals(), cpIndex);
      auto recv = _stream->read<J9Method *, UDATA, bool, TR_ResolvedJ9JITaaSServerMethodInfo>();
      J9Method *ramMethod = std::get<0>(recv);
      UDATA vTableIndex = std::get<1>(recv);

      if (std::get<2>(recv) && unresolvedInCP)
         *unresolvedInCP = false;

      if (vTableIndex)
         {
         TR_AOTInliningStats *aotStats = nullptr;
         if (comp->getOption(TR_EnableAOTStats))
            aotStats = & (((TR_JitPrivateConfig *)_fe->_jitConfig->privateConfig)->aotStats->virtualMethods);

         TR_ResolvedJ9JITaaSServerMethodInfo methodInfo = std::get<3>(recv);
         
         // call constructor without making a new query
         resolvedMethod = createResolvedMethodFromJ9Method(comp, cpIndex, vTableIndex, ramMethod, unresolvedInCP, aotStats, methodInfo);
         }
      }

   if (resolvedMethod == nullptr)
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
         J9ROMConstantPoolItem *cpItem = (J9ROMConstantPoolItem *)romLiterals();
         J9ROMMethodRef *romMethodRef = (J9ROMMethodRef *)(cpItem + cpIndex);
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

bool
TR_ResolvedJ9JITaaSServerMethod::fieldAttributes(TR::Compilation * comp, I_32 cpIndex, U_32 * fieldOffset, TR::DataType * type, bool * volatileP, bool * isFinal, bool * isPrivate, bool isStore, bool * unresolvedInCP, bool needAOTValidation)
   {
   // look up attributes in a cache, make a query only if they are not cached
   auto gotAttrs = _fieldAttributesCache.find(cpIndex); 
   if (gotAttrs == _fieldAttributesCache.end())
      {
      _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_fieldAttributes, _remoteMirror, cpIndex, isStore, needAOTValidation);
      auto recv = _stream->read<TR_J9MethodFieldAttributes>();
      gotAttrs = _fieldAttributesCache.insert({cpIndex, std::get<0>(recv)}).first;
      }
   
   // access attributes from the cache
   auto attrs = gotAttrs->second;
   *fieldOffset = attrs.fieldOffset;
   *type = attrs.type;
   *volatileP = attrs.volatileP;
   if (isFinal) *isFinal = attrs.isFinal;
   if (isPrivate) *isPrivate = attrs.isPrivate;
   if (unresolvedInCP) *unresolvedInCP = attrs.unresolvedInCP;
   return attrs.result;
   }

TR_ResolvedMethod *
TR_ResolvedJ9JITaaSServerMethod::getResolvedStaticMethod(TR::Compilation * comp, I_32 cpIndex, bool * unresolvedInCP)
   {
   // JITaaS TODO: Decide whether the counters should be updated on the server or the client
   TR_ResolvedMethod *resolvedMethod = NULL;
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
   INCREMENT_COUNTER(_fe, totalStaticMethodRefs);

   _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_getResolvedStaticMethodAndMirror, _remoteMirror, cpIndex);
   auto recv = _stream->read<J9Method *, TR_ResolvedJ9JITaaSServerMethodInfo>();
   J9Method * ramMethod = std::get<0>(recv); 

   bool skipForDebugging = false;
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
      auto methodInfo = std::get<1>(recv);
      TR_AOTInliningStats *aotStats = NULL;
      if (comp->getOption(TR_EnableAOTStats))
         aotStats = & (((TR_JitPrivateConfig *)_fe->_jitConfig->privateConfig)->aotStats->staticMethods);
      resolvedMethod = createResolvedMethodFromJ9Method(comp, cpIndex, 0, ramMethod, unresolvedInCP, aotStats, methodInfo);
      if (unresolvedInCP)
         *unresolvedInCP = false;
      }

   if (resolvedMethod == NULL)
      {
      INCREMENT_COUNTER(_fe, unresolvedStaticMethodRefs);
      if (unresolvedInCP)
         handleUnresolvedStaticMethodInCP(cpIndex, unresolvedInCP);
      }

   return resolvedMethod;
   }

TR_ResolvedMethod *
TR_ResolvedJ9JITaaSServerMethod::getResolvedSpecialMethod(TR::Compilation * comp, I_32 cpIndex, bool * unresolvedInCP)
   {
   // JITaaS TODO: Decide whether the counters should be updated on the server or the client
   TR_ResolvedMethod *resolvedMethod = nullptr;
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");

   INCREMENT_COUNTER(_fe, totalSpecialMethodRefs);

   if (unresolvedInCP)
      *unresolvedInCP = true;

   _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_getResolvedSpecialMethodAndMirror, _remoteMirror, cpIndex, unresolvedInCP != nullptr);
   auto recv = _stream->read<J9Method *, bool, bool, TR_ResolvedJ9JITaaSServerMethodInfo>();
   J9Method * ramMethod = std::get<0>(recv);
   bool unresolved = std::get<1>(recv);
   bool tookBranch = std::get<2>(recv);

   if (unresolved && unresolvedInCP)
      *unresolvedInCP = true;

   if (tookBranch && ramMethod)
      {
      auto methodInfo = std::get<3>(recv);
      TR_AOTInliningStats *aotStats = NULL;
      if (comp->getOption(TR_EnableAOTStats))
         aotStats = & (((TR_JitPrivateConfig *)_fe->_jitConfig->privateConfig)->aotStats->specialMethods);
      resolvedMethod = createResolvedMethodFromJ9Method(comp, cpIndex, 0, ramMethod, unresolvedInCP, aotStats, methodInfo);
      if (unresolvedInCP)
         *unresolvedInCP = false;
      }

   if (resolvedMethod == NULL)
      {
      INCREMENT_COUNTER(_fe, unresolvedSpecialMethodRefs);
      if (unresolvedInCP)
         handleUnresolvedVirtualMethodInCP(cpIndex, unresolvedInCP);
      }

   return resolvedMethod;
   }

TR_ResolvedMethod *
TR_ResolvedJ9JITaaSServerMethod::createResolvedMethodFromJ9Method( TR::Compilation *comp, int32_t cpIndex, uint32_t vTableSlot, J9Method *j9Method, bool * unresolvedInCP, TR_AOTInliningStats *aotStats)
   {
   return new (comp->trHeapMemory()) TR_ResolvedJ9JITaaSServerMethod((TR_OpaqueMethodBlock *) j9Method, _fe, comp->trMemory(), this, vTableSlot);
   }

TR_ResolvedMethod *
TR_ResolvedJ9JITaaSServerMethod::createResolvedMethodFromJ9Method( TR::Compilation *comp, int32_t cpIndex, uint32_t vTableSlot, J9Method *j9Method, bool * unresolvedInCP, TR_AOTInliningStats *aotStats, const TR_ResolvedJ9JITaaSServerMethodInfo &methodInfo)
   {
   return new (comp->trHeapMemory()) TR_ResolvedJ9JITaaSServerMethod((TR_OpaqueMethodBlock *) j9Method, _fe, comp->trMemory(), methodInfo, this, vTableSlot);
   }

uint32_t
TR_ResolvedJ9JITaaSServerMethod::classCPIndexOfMethod(uint32_t methodCPIndex)
   {
   _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_classCPIndexOfMethod, _remoteMirror, methodCPIndex);
   return std::get<0>(_stream->read<uint32_t>());
   }

void *
TR_ResolvedJ9JITaaSServerMethod::startAddressForJittedMethod()
   {
   // return the cached value if we have any
   if (_startAddressForJittedMethod)
      {
      return _startAddressForJittedMethod;
      }
   else // Otherwise ask the client for it
      {
      _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_startAddressForJittedMethod, _remoteMirror);
      return std::get<0>(_stream->read<void *>());
      }
   }

char *
TR_ResolvedJ9JITaaSServerMethod::localName(U_32 slotNumber, U_32 bcIndex, I_32 &len, TR_Memory *trMemory)
   {
   _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_localName, _remoteMirror, slotNumber, bcIndex);
   const std::string nameString = std::get<0>(_stream->read<std::string>());
   len = nameString.length();
   char *out = (char*) trMemory->allocateHeapMemory(len);
   memcpy(out, &nameString[0], len);
   return out;
   }

TR_OpaqueClassBlock *
TR_ResolvedJ9JITaaSServerMethod::getResolvedInterfaceMethod(I_32 cpIndex, UDATA *pITableIndex)
   {
   _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_getResolvedInterfaceMethod_2, _remoteMirror, cpIndex);
   auto recv = _stream->read<TR_OpaqueClassBlock *, UDATA>();
   *pITableIndex = std::get<1>(recv);
   return std::get<0>(recv);
   }

TR_ResolvedMethod *
TR_ResolvedJ9JITaaSServerMethod::getResolvedInterfaceMethod(TR::Compilation * comp, TR_OpaqueClassBlock * classObject, I_32 cpIndex)
   {
   _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_getResolvedInterfaceMethod_3, getPersistentIdentifier(), classObject, cpIndex);
   auto recv = _stream->read<bool, J9Method*>();
   bool resolved = std::get<0>(recv);
   J9Method *ramMethod = std::get<1>(recv);

   // If the method ref is unresolved, the bytecodes of the ramMethod will be NULL.
   // IFF resolved, then we can look at the rest of the ref.
   //
   if (resolved)
      {
      TR_AOTInliningStats *aotStats = NULL;
      if (comp->getOption(TR_EnableAOTStats))
         aotStats = & (((TR_JitPrivateConfig *)_fe->_jitConfig->privateConfig)->aotStats->interfaceMethods);
      TR_ResolvedMethod *m = createResolvedMethodFromJ9Method(comp, cpIndex, 0, ramMethod, NULL, aotStats);

      TR_OpaqueClassBlock *c = NULL;
      if (m)
         {
         c = m->classOfMethod();
         if (c && !_fe->isInterfaceClass(c))
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

U_32
TR_ResolvedJ9JITaaSServerMethod::getResolvedInterfaceMethodOffset(TR_OpaqueClassBlock * classObject, I_32 cpIndex)
   {
   _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_getResolvedInterfaceMethodOffset, _remoteMirror, classObject, cpIndex);
   auto recv = _stream->read<U_32>();
   return std::get<0>(recv);
   }

void *
TR_ResolvedJ9JITaaSServerMethod::startAddressForJNIMethod(TR::Compilation *comp)
   {
   // For fastJNI methods, we have the address cached
   if (_jniProperties)
      return _jniTargetAddress;
   _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_startAddressForJNIMethod, _remoteMirror);
   return std::get<0>(_stream->read<void *>());
   }

bool
TR_ResolvedJ9JITaaSServerMethod::getUnresolvedStaticMethodInCP(int32_t cpIndex)
   {
   _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_getUnresolvedStaticMethodInCP, _remoteMirror, cpIndex);
   return std::get<0>(_stream->read<bool>());
   }

void *
TR_ResolvedJ9JITaaSServerMethod::startAddressForInterpreterOfJittedMethod()
   {
   _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_startAddressForInterpreterOfJittedMethod, _remoteMirror);
   return std::get<0>(_stream->read<void *>());
   }

bool
TR_ResolvedJ9JITaaSServerMethod::getUnresolvedSpecialMethodInCP(I_32 cpIndex)
   {
   _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_getUnresolvedSpecialMethodInCP, _remoteMirror, cpIndex);
   return std::get<0>(_stream->read<bool>());
   }

TR_ResolvedMethod *
TR_ResolvedJ9JITaaSServerMethod::getResolvedVirtualMethod(TR::Compilation * comp, TR_OpaqueClassBlock * classObject, I_32 virtualCallOffset , bool ignoreRtResolve)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)_fe;
   // the frontend method performs a RPC
   void * ramMethod = fej9->getResolvedVirtualMethod(classObject, virtualCallOffset, ignoreRtResolve);
   TR_ResolvedMethod *m = ramMethod ? new (comp->trHeapMemory()) TR_ResolvedJ9JITaaSServerMethod((TR_OpaqueMethodBlock *) ramMethod, _fe, comp->trMemory(), this) : 0;
   return m;
   }

bool
TR_ResolvedJ9JITaaSServerMethod::getUnresolvedFieldInCP(I_32 cpIndex)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
#if defined(J9VM_INTERP_AOT_COMPILE_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))
   _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_getUnresolvedFieldInCP, _remoteMirror, cpIndex);
   return std::get<0>(_stream->read<bool>());
#else
      return true;
#endif
   }

bool
TR_ResolvedJ9JITaaSServerMethod::inROMClass(void * address)
   {
   return address >= romClassPtr() && address < ((uint8_t*)romClassPtr()) + romClassPtr()->romSize;
   }

char *
TR_ResolvedJ9JITaaSServerMethod::getRemoteROMString(int32_t &len, void *basePtr, std::initializer_list<size_t> offsets)
   {
   auto offsetFirst = *offsets.begin();
   auto offsetSecond = (offsets.size() == 2) ? *(offsets.begin() + 1) : 0;

   TR_ASSERT(offsetFirst < (1 << 16) && offsetSecond < (1 << 16), "Offsets are larger than 16 bits");
   TR_ASSERT(offsets.size() <= 2, "Number of offsets is greater than 2"); 

   // create a key for hashing into a table of strings
   TR_RemoteROMStringKey key;
   uint32_t offsetKey = (offsetFirst << 16) + offsetSecond;
   key.basePtr = basePtr;
   key.offsets = offsetKey;
   
   std::string *cachedStr = nullptr;
   bool isCached = false;
   TR::CompilationInfoPerThread *threadCompInfo = _fe->_compInfoPT;
   {
      OMR::CriticalSection getRemoteROMClass(threadCompInfo->getClientData()->getROMMapMonitor()); 
      auto &stringsCache = getJ9ClassInfo(threadCompInfo, _ramClass)._remoteROMStringsCache;
      // initialize cache if it hasn't been done yet
      if (!stringsCache)
         {
         stringsCache = new (PERSISTENT_NEW) PersistentUnorderedMap<TR_RemoteROMStringKey, std::string>(PersistentUnorderedMap<TR_RemoteROMStringKey, std::string>::allocator_type(TR::Compiler->persistentAllocator()));
         }
      auto gotStr = stringsCache->find(key);
      if (gotStr != stringsCache->end())
         {
         cachedStr = &(gotStr->second);
         isCached = true;
         }
   }

   // only make a query if a string hasn't been cached
   if (!isCached)
      {
      size_t offsetFromROMClass = (uint8_t*) basePtr - (uint8_t*) romClassPtr();
      std::string offsetsStr((char*) offsets.begin(), offsets.size() * sizeof(size_t));
      
      _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_getRemoteROMString, _remoteMirror, offsetFromROMClass, offsetsStr);
         {
         // reaquire the monitor
         OMR::CriticalSection getRemoteROMClass(threadCompInfo->getClientData()->getROMMapMonitor()); 
         auto &stringsCache = getJ9ClassInfo(threadCompInfo, _ramClass)._remoteROMStringsCache;
         cachedStr = &(stringsCache->insert({key, std::get<0>(_stream->read<std::string>())}).first->second);
         }
      }

   len = cachedStr->length();
   return &(cachedStr->at(0));
   }

// Takes a pointer to some data which is placed statically relative to the rom class,
// as well as a list of offsets to J9SRP fields. The first offset is applied before the first
// SRP is followed.
//
// If at any point while following the chain of SRP pointers we land outside the ROM class,
// then we fall back to getRemoteROMString which follows the same process on the client.
//
// This is a workaround because some data referenced in the ROM constant pool is located outside of
// it, but we cannot easily determine if the reference is outside or not (or even if it is a reference!)
// because the data is untyped.
char *
TR_ResolvedJ9JITaaSServerMethod::getROMString(int32_t &len, void *basePtr, std::initializer_list<size_t> offsets)
   {
   uint8_t *ptr = (uint8_t*) basePtr;
   for (size_t offset : offsets)
      {
      ptr += offset;
      if (!inROMClass(ptr))
         return getRemoteROMString(len, basePtr, offsets);
      ptr = ptr + *(J9SRP*)ptr;
      }
   if (!inROMClass(ptr))
      return getRemoteROMString(len, basePtr, offsets);
   char *data = utf8Data((J9UTF8*) ptr, len);
   return data;
   }

char *
TR_ResolvedJ9JITaaSServerMethod::fieldOrStaticSignatureChars(I_32 cpIndex, int32_t & len)
   {
   if (cpIndex < 0)
      return 0;

   char *signature = getROMString(len, &romCPBase()[cpIndex],
                           {
                           offsetof(J9ROMFieldRef, nameAndSignature),
                           offsetof(J9ROMNameAndSignature, signature)
                           });
   return signature;
   }

char *
TR_ResolvedJ9JITaaSServerMethod::getClassNameFromConstantPool(uint32_t cpIndex, uint32_t &length)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");

   int32_t len;
   char *name = getROMString(len, &romLiterals()[cpIndex],
                           {
                           offsetof(J9ROMClassRef, name),
                           });
   length = len; // this is what happends when you have no type conventions
   return name;
   }

char *
TR_ResolvedJ9JITaaSServerMethod::classNameOfFieldOrStatic(I_32 cpIndex, int32_t & len)
   {
   if (cpIndex == -1)
      return 0;

   J9ROMFieldRef * ref = (J9ROMFieldRef *) (&romCPBase()[cpIndex]);
   TR_ASSERT(inROMClass(ref), "field ref must be in ROM class"); // for now. if this fails make it a remote call
   char *name = getROMString(len, &romCPBase()[ref->classRefCPIndex],
                           {
                           offsetof(J9ROMClassRef, name),
                           });
   return name;
   }

char *
TR_ResolvedJ9JITaaSServerMethod::classSignatureOfFieldOrStatic(I_32 cpIndex, int32_t & len)
   {
   if (cpIndex == -1)
      return 0;

   char *signature = getROMString(len, &romCPBase()[cpIndex],
                           {
                           offsetof(J9ROMFieldRef, nameAndSignature),
                           offsetof(J9ROMNameAndSignature, signature),
                           });
   return signature;
   }

char *
TR_ResolvedJ9JITaaSServerMethod::fieldOrStaticNameChars(I_32 cpIndex, int32_t & len)
   {
   if (cpIndex < 0)
      return 0;

   char *name = getROMString(len, &romCPBase()[cpIndex],
                           {
                           offsetof(J9ROMFieldRef, nameAndSignature),
                           offsetof(J9ROMNameAndSignature, name)
                           });
   return name;
   }

char *
TR_ResolvedJ9JITaaSServerMethod::fieldOrStaticName(I_32 cpIndex, int32_t & len, TR_Memory * trMemory, TR_AllocationKind kind)
   {
   if (cpIndex == -1)
      return "<internal name>";

   J9ROMFieldRef * ref = (J9ROMFieldRef *) (&romCPBase()[cpIndex]);
   J9ROMNameAndSignature * nameAndSignature = J9ROMFIELDREF_NAMEANDSIGNATURE(ref);

   if (inROMClass(nameAndSignature))
      {
      J9UTF8 * declName = J9ROMCLASSREF_NAME((J9ROMClassRef *) (&romCPBase()[ref->classRefCPIndex]));
      J9UTF8 * name = J9ROMNAMEANDSIGNATURE_NAME(nameAndSignature);
      J9UTF8 * signature = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSignature);

      if (inROMClass(declName) && inROMClass(name) && inROMClass(signature))
         {
         len = J9UTF8_LENGTH(declName) + J9UTF8_LENGTH(name) + J9UTF8_LENGTH(signature) +3;
         char * s = (char *)trMemory->allocateMemory(len, kind);
         sprintf(s, "%.*s.%.*s %.*s",
                 J9UTF8_LENGTH(declName), utf8Data(declName),
                 J9UTF8_LENGTH(name), utf8Data(name),
                 J9UTF8_LENGTH(signature), utf8Data(signature));
         return s;
         }
      }

   
   std::string *cachedStr = nullptr;
   bool isCached = false;
   TR::CompilationInfoPerThread *threadCompInfo = _fe->_compInfoPT;
   {
      OMR::CriticalSection getRemoteROMClass(threadCompInfo->getClientData()->getROMMapMonitor()); 
      auto &stringsCache = getJ9ClassInfo(threadCompInfo, _ramClass)._fieldOrStaticNameCache;
      // initialize cache if it hasn't been done yet
      if (!stringsCache)
         {
         stringsCache = new (PERSISTENT_NEW) PersistentUnorderedMap<int32_t, std::string>(PersistentUnorderedMap<TR_RemoteROMStringKey, std::string>::allocator_type(TR::Compiler->persistentAllocator()));
         }
      auto gotStr = stringsCache->find(cpIndex);
      if (gotStr != stringsCache->end())
         {
         cachedStr = &(gotStr->second);
         isCached = true;
         }
   }

   // only make a query if a string hasn't been cached
   if (!isCached)
      {
      _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_fieldOrStaticName, _remoteMirror, cpIndex);
         {
         // reaquire the monitor
         OMR::CriticalSection getRemoteROMClass(threadCompInfo->getClientData()->getROMMapMonitor()); 
         auto &stringsCache = getJ9ClassInfo(threadCompInfo, _ramClass)._fieldOrStaticNameCache;
         cachedStr = &(stringsCache->insert({cpIndex, std::get<0>(_stream->read<std::string>())}).first->second);
         }
      }

   len = cachedStr->length();
   return &(cachedStr->at(0));
   }

void *
TR_ResolvedJ9JITaaSServerMethod::stringConstant(I_32 cpIndex)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
   return (void *) ((U_8 *)&(((J9RAMStringRef *) romLiterals())[cpIndex].stringObject));
   }


bool
TR_ResolvedJ9JITaaSServerMethod::isSubjectToPhaseChange(TR::Compilation *comp)
   {
   bool candidate = comp->getOptLevel() <= warm &&
        // comp->getPersistentInfo()->getJitState() == STARTUP_STATE // This needs to be asked at the server
        isPublic() &&
        (
         strncmp("java/util/AbstractCollection", comp->signature(), 28) == 0 ||
         strncmp("java/util/Hash", comp->signature(), 14) == 0 ||
         strncmp("java/lang/String", comp->signature(), 16) == 0 ||
         strncmp("sun/nio/", comp->signature(), 8) == 0
         );
 
   if (!candidate)
      {
      return false;
      }
   else
      {
      _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_isSubjectToPhaseChange, _remoteMirror);
      return std::get<0>(_stream->read<bool>());
      // JITaaS TODO: cache the JitState when we create  TR_ResolvedJ9JITaaSServerMethod
      // This may not behave exactly like the non-JITaaS due to timing differences
      }
   }

TR_ResolvedMethod *
TR_ResolvedJ9JITaaSServerMethod::getResolvedHandleMethod(TR::Compilation * comp, I_32 cpIndex, bool * unresolvedInCP)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
#if TURN_OFF_INLINING
   return 0;
#else

   _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_getResolvedHandleMethod, _remoteMirror, cpIndex);
   auto recv = _stream->read<TR_OpaqueMethodBlock *, std::string, bool>();
   auto dummyInvokeExact = std::get<0>(recv);
   auto signature = std::get<1>(recv);
   if (unresolvedInCP)
      *unresolvedInCP = std::get<2>(recv);

   return _fe->createResolvedMethodWithSignature(comp->trMemory(), dummyInvokeExact, NULL, &signature[0], signature.length(), this);
#endif
   }

#if defined(J9VM_OPT_REMOVE_CONSTANT_POOL_SPLITTING)
void *
TR_ResolvedJ9JITaaSServerMethod::methodTypeTableEntryAddress(int32_t cpIndex)
   {
   _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_methodTypeTableEntryAddress, _remoteMirror, cpIndex);
   return std::get<0>(_stream->read<void*>());
   }

bool
TR_ResolvedJ9JITaaSServerMethod::isUnresolvedMethodTypeTableEntry(int32_t cpIndex)
   {
   _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_isUnresolvedMethodTypeTableEntry, _remoteMirror, cpIndex);
   return std::get<0>(_stream->read<bool>());
   }
#endif

bool
TR_ResolvedJ9JITaaSServerMethod::isUnresolvedCallSiteTableEntry(int32_t callSiteIndex)
   {
   _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_isUnresolvedCallSiteTableEntry, _remoteMirror, callSiteIndex);
   return std::get<0>(_stream->read<bool>());
   }

void *
TR_ResolvedJ9JITaaSServerMethod::callSiteTableEntryAddress(int32_t callSiteIndex)
   {
   _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_callSiteTableEntryAddress, _remoteMirror, callSiteIndex);
   return std::get<0>(_stream->read<void*>());
   }

TR_ResolvedMethod *
TR_ResolvedJ9JITaaSServerMethod::getResolvedDynamicMethod(TR::Compilation * comp, I_32 callSiteIndex, bool * unresolvedInCP)
   {
   TR_ASSERT(callSiteIndex != -1, "callSiteIndex shouldn't be -1");

#if TURN_OFF_INLINING
   return 0;
#else

   J9Class    *ramClass = constantPoolHdr();
   J9ROMClass *romClass = romClassPtr();
   _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_getResolvedDynamicMethod, callSiteIndex, ramClass, getNonPersistentIdentifier());
   auto recv = _stream->read<TR_OpaqueMethodBlock*, std::string, bool>();
   TR_OpaqueMethodBlock *dummyInvokeExact = std::get<0>(recv);
   std::string signature = std::get<1>(recv);
   if (unresolvedInCP)
      *unresolvedInCP = std::get<2>(recv);
   TR_ResolvedMethod *result = _fe->createResolvedMethodWithSignature(comp->trMemory(), dummyInvokeExact, NULL, &signature[0], signature.size(), this);

   return result;
#endif
   }

bool
TR_ResolvedJ9JITaaSServerMethod::shouldFailSetRecognizedMethodInfoBecauseOfHCR()
   {
   _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_shouldFailSetRecognizedMethodInfoBecauseOfHCR, _remoteMirror);
   return std::get<0>(_stream->read<bool>());
   }

bool
TR_ResolvedJ9JITaaSServerMethod::isSameMethod(TR_ResolvedMethod * m2)
   {
   if (isNative())
      return false; // A jitted JNI method doesn't call itself

   auto other = static_cast<TR_ResolvedJ9JITaaSServerMethod*>(m2);

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

      _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_isSameMethod, thisHandleLocation, otherHandleLocation);
      sameMethodHandle = std::get<0>(_stream->read<bool>());

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

bool
TR_ResolvedJ9JITaaSServerMethod::isInlineable(TR::Compilation *comp)
   {
   // Reduce number of remote queries by testing the options first
   // This assumes knowledge of how the original is implemented
   if (comp->getOption(TR_FullSpeedDebug) && comp->getOption(TR_EnableOSR))
      {
      _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_isInlineable, _remoteMirror);
      return std::get<0>(_stream->read<bool>());
      }
   else
      {
      return true;
      }
   }

bool
TR_ResolvedJ9JITaaSServerMethod::isWarmCallGraphTooBig(uint32_t bcIndex, TR::Compilation *comp)
   {
   _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_isWarmCallGraphTooBig, _remoteMirror, bcIndex);
   return std::get<0>(_stream->read<bool>()); 
   }

void
TR_ResolvedJ9JITaaSServerMethod::setWarmCallGraphTooBig(uint32_t bcIndex, TR::Compilation *comp)
   {
   _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_setWarmCallGraphTooBig, _remoteMirror, bcIndex);
   _stream->read<JITaaS::Void>();
   }

void
TR_ResolvedJ9JITaaSServerMethod::setVirtualMethodIsOverridden()
   {
   _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_setVirtualMethodIsOverridden, _remoteMirror);
   _stream->read<JITaaS::Void>();
   }

bool
TR_ResolvedJ9JITaaSServerMethod::methodIsNotzAAPEligible()
   {
   _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_methodIsNotzAAPEligible, _remoteMirror);
   return std::get<0>(_stream->read<bool>());
   }
void
TR_ResolvedJ9JITaaSServerMethod::setClassForNewInstance(J9Class *c)
   {
   _j9classForNewInstance = c;
   _stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_setClassForNewInstance, _remoteMirror, c);
   _stream->read<JITaaS::Void>();
   }

TR_OpaqueClassBlock *
TR_ResolvedJ9JITaaSServerMethod::classOfMethod()
      {
      if (isNewInstanceImplThunk())
         {
         TR_ASSERT(_j9classForNewInstance, "Must have the class for the newInstance");
         //J9Class * clazz = (J9Class *)((intptrj_t)ramMethod()->extra & ~J9_STARTPC_NOT_TRANSLATED);
         return _fe->convertClassPtrToClassOffset(_j9classForNewInstance);//(TR_OpaqueClassBlock *&)(rc);
         }
      return _fe->convertClassPtrToClassOffset(_ramClass);
      }

TR_PersistentJittedBodyInfo *
TR_ResolvedJ9JITaaSServerMethod::getExistingJittedBodyInfo()
   {
   return _bodyInfo; // return cached value
   }

void
TR_ResolvedJ9JITaaSServerMethod::createResolvedJ9MethodMirror(TR_ResolvedJ9JITaaSServerMethodInfo &methodInfo, TR_OpaqueMethodBlock *method, uint32_t vTableSlot, TR_ResolvedMethod *owningMethod, TR_FrontEnd *fe, TR_Memory *trMemory)
   {
   TR_ResolvedJ9Method *resolvedMethod = new (trMemory->trHeapMemory()) TR_ResolvedJ9Method(method, fe, trMemory, owningMethod, vTableSlot);
   if (!resolvedMethod) throw std::bad_alloc();

   J9RAMConstantPoolItem *literals = (J9RAMConstantPoolItem *)(J9_CP_FROM_METHOD(resolvedMethod->ramMethod()));
   J9Class *cpHdr = J9_CLASS_FROM_CP(literals);

   J9Method *j9method = (J9Method *)method;
  
   // fill-in struct fields 
   auto &methodInfoStruct = std::get<0>(methodInfo);

   methodInfoStruct.remoteMirror = resolvedMethod;
   methodInfoStruct.literals = literals;
   methodInfoStruct.ramClass = cpHdr;
   methodInfoStruct.methodIndex = getMethodIndexUnchecked(j9method);
   methodInfoStruct.jniProperties = resolvedMethod->getJNIProperties();
   methodInfoStruct.jniTargetAddress = resolvedMethod->getJNITargetAddress();
   methodInfoStruct.isInterpreted = resolvedMethod->isInterpreted();
   methodInfoStruct.isMethodInValidLibrary = resolvedMethod->isMethodInValidLibrary();
   methodInfoStruct.mandatoryRm = resolvedMethod->getMandatoryRecognizedMethod();
   methodInfoStruct.rm = ((TR_ResolvedMethod*)resolvedMethod)->getRecognizedMethod();
   methodInfoStruct.startAddressForJittedMethod = TR::CompilationInfo::isCompiled(resolvedMethod->ramMethod()) ?
                                           resolvedMethod->startAddressForJittedMethod() : nullptr;
   methodInfoStruct.virtualMethodIsOverridden = resolvedMethod->virtualMethodIsOverridden();
   methodInfoStruct.addressContainingIsOverriddenBit = resolvedMethod->addressContainingIsOverriddenBit();
   methodInfoStruct.classLoader = resolvedMethod->getClassLoader();

   TR_PersistentJittedBodyInfo *bodyInfo = nullptr;
   // Method may not have been compiled
   if (!resolvedMethod->isInterpreted() && !resolvedMethod->isJITInternalNative())
      {
      bodyInfo = resolvedMethod->getExistingJittedBodyInfo();
      }

   // set string info fields (they cannot be inside of the struct)
   std::string jbi = bodyInfo ? std::string((char*)bodyInfo, sizeof(TR_PersistentJittedBodyInfo)) : std::string();
   std::string methodInfoStr = bodyInfo ? std::string((char*)bodyInfo->getMethodInfo(), sizeof(TR_PersistentMethodInfo)) : std::string();
   std::get<1>(methodInfo) = jbi;
   std::get<2>(methodInfo) = methodInfoStr;
   }

void
TR_ResolvedJ9JITaaSServerMethod::setAttributes(TR_OpaqueMethodBlock * aMethod, TR_FrontEnd * fe, TR_Memory * trMemory, uint32_t vTableSlot, TR::CompilationInfoPerThread *threadCompInfo, const TR_ResolvedJ9JITaaSServerMethodInfo &methodInfo)
   {
   auto methodInfoStruct = std::get<0>(methodInfo);
   
   
   _ramMethod = (J9Method *)aMethod;

   _remoteMirror = methodInfoStruct.remoteMirror;

   // Cache the constantPool and constantPoolHeader
   _literals = methodInfoStruct.literals;
   _ramClass = methodInfoStruct.ramClass;

   _romClass = threadCompInfo->getAndCacheRemoteROMClass(_ramClass, trMemory);
   _romMethod = romMethodAtClassIndex(_romClass, methodInfoStruct.methodIndex);
   _romLiterals = (J9ROMConstantPoolItem *) ((UDATA) _romClass + sizeof(J9ROMClass));

   _vTableSlot = vTableSlot;
   _j9classForNewInstance = nullptr;

   _jniProperties = methodInfoStruct.jniProperties;
   _jniTargetAddress = methodInfoStruct.jniTargetAddress;

   _isInterpreted = methodInfoStruct.isInterpreted;
   _isMethodInValidLibrary = methodInfoStruct.isMethodInValidLibrary;
   
   TR::RecognizedMethod mandatoryRm = methodInfoStruct.mandatoryRm;
   TR::RecognizedMethod rm = methodInfoStruct.rm;

   _startAddressForJittedMethod = methodInfoStruct.startAddressForJittedMethod;
   _virtualMethodIsOverridden = methodInfoStruct.virtualMethodIsOverridden;
   _addressContainingIsOverriddenBit = methodInfoStruct.addressContainingIsOverriddenBit;
   _classLoader = methodInfoStruct.classLoader;

   auto &bodyInfoStr = std::get<1>(methodInfo);
   auto &methodInfoStr = std::get<2>(methodInfo);

   _bodyInfo = J9::Recompilation::persistentJittedBodyInfoFromString(bodyInfoStr, methodInfoStr, trMemory);
 
   // initialization from TR_J9Method constructor
   _className = J9ROMCLASS_CLASSNAME(_romClass);
   _name = J9ROMMETHOD_GET_NAME(_romClass, _romMethod);
   _signature = J9ROMMETHOD_GET_SIGNATURE(_romClass, _romMethod);
   parseSignature(trMemory);
   _fullSignature = nullptr;
  
   setMandatoryRecognizedMethod(mandatoryRm);
   setRecognizedMethod(rm);

   }


