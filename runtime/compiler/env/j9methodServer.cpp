/*******************************************************************************
 * Copyright IBM Corp. and others 2018
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

#include "j9methodServer.hpp"
#include "control/CompilationRuntime.hpp"
#include "control/CompilationThread.hpp"
#include "control/JITServerCompilationThread.hpp"
#include "control/JITServerHelpers.hpp"
#include "control/MethodToBeCompiled.hpp"
#include "env/VMJ9Server.hpp"
#include "exceptions/DataCacheError.hpp"
#include "ilgen/J9ByteCodeIterator.hpp"
#include "net/ServerStream.hpp"

ClientSessionData::ClassInfo &
getJ9ClassInfo(TR::CompilationInfoPerThread *threadCompInfo, J9Class *clazz)
   {
   // This function assumes that you are inside of _romMapMonitor
   // Do not use it otherwise
   auto &classMap = threadCompInfo->getClientData()->getROMClassMap();
   auto it = classMap.find(clazz);
   TR_ASSERT_FATAL(it != classMap.end(),"compThreadID %d, ClientData %p, clazz %p: ClassInfo is not in the class map %p!!\n",
      threadCompInfo->getCompThreadId(), threadCompInfo->getClientData(), clazz, &classMap);
   return it->second;
   }

static J9ROMMethod *
romMethodAtClassIndex(J9ROMClass *romClass, uint64_t methodIndex)
   {
   J9ROMMethod * romMethod = J9ROMCLASS_ROMMETHODS(romClass);
   for (size_t i = methodIndex; i; --i)
      romMethod = nextROMMethod(romMethod);
   return romMethod;
   }

TR_ResolvedJ9JITServerMethod::TR_ResolvedJ9JITServerMethod(TR_OpaqueMethodBlock * aMethod, TR_FrontEnd * fe, TR_Memory * trMemory, TR_ResolvedMethod * owningMethod, uint32_t vTableSlot)
   : TR_ResolvedJ9Method(fe, owningMethod)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;
   TR::CompilationInfo *compInfo = TR::CompilationInfo::get(fej9->getJ9JITConfig());
   TR::CompilationInfoPerThread *threadCompInfo = compInfo->getCompInfoForThread(fej9->vmThread());
   _stream = threadCompInfo->getMethodBeingCompiled()->_stream;

   // Create client side mirror of this object to use for calls involving RAM data
   TR_ResolvedJ9Method* owningMethodMirror = owningMethod ? ((TR_ResolvedJ9JITServerMethod*) owningMethod)->_remoteMirror : NULL;

   // If in AOT mode, will actually create relocatable version of resolved method on the client
   _stream->write(JITServer::MessageType::mirrorResolvedJ9Method, aMethod, owningMethodMirror, vTableSlot, fej9->isAOT_DEPRECATED_DO_NOT_USE());
   auto recv = _stream->read<TR_ResolvedJ9JITServerMethodInfo>();
   auto &methodInfo = std::get<0>(recv);

   unpackMethodInfo(aMethod, fe, trMemory, vTableSlot, threadCompInfo, methodInfo);
   }

TR_ResolvedJ9JITServerMethod::TR_ResolvedJ9JITServerMethod(TR_OpaqueMethodBlock * aMethod, TR_FrontEnd * fe, TR_Memory * trMemory, const TR_ResolvedJ9JITServerMethodInfo &methodInfo, TR_ResolvedMethod * owningMethod, uint32_t vTableSlot)
   : TR_ResolvedJ9Method(fe, owningMethod)
   {
   // Mirror has already been created, its parameters are passed in methodInfo
   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;
   TR::CompilationInfo *compInfo = TR::CompilationInfo::get(fej9->getJ9JITConfig());
   TR::CompilationInfoPerThread *threadCompInfo = compInfo->getCompInfoForThread(fej9->vmThread());
   _stream = threadCompInfo->getMethodBeingCompiled()->_stream;
   unpackMethodInfo(aMethod, fe, trMemory, vTableSlot, threadCompInfo, methodInfo);
   }

J9ROMClass *
TR_ResolvedJ9JITServerMethod::romClassPtr()
   {
   return _romClass;
   }

J9RAMConstantPoolItem *
TR_ResolvedJ9JITServerMethod::literals()
   {
   return _literals;
   }

J9Class *
TR_ResolvedJ9JITServerMethod::constantPoolHdr()
   {
   return _ramClass;
   }

bool
TR_ResolvedJ9JITServerMethod::isJNINative()
   {
   return _isJNINative;
   }

void
TR_ResolvedJ9JITServerMethod::setRecognizedMethodInfo(TR::RecognizedMethod rm)
   {
   TR_ResolvedJ9Method::setRecognizedMethodInfo(rm);
   _stream->write(JITServer::MessageType::ResolvedMethod_setRecognizedMethodInfo, _remoteMirror, rm);
   _stream->read<JITServer::Void>();
   }

J9ClassLoader *
TR_ResolvedJ9JITServerMethod::getClassLoader()
   {
   return _classLoader; // cached version
   }

bool
TR_ResolvedJ9JITServerMethod::staticAttributes(TR::Compilation * comp, I_32 cpIndex, void * * address, TR::DataType * type, bool * volatileP, bool * isFinal, bool * isPrivate, bool isStore, bool * unresolvedInCP, bool needAOTValidation)
   {
   bool isStatic = true;
   TR_J9MethodFieldAttributes attributes;
   if(!getCachedFieldAttributes(cpIndex, attributes, isStatic))
      {
      // make a remote call, if attributes are not cached
      _stream->write(JITServer::MessageType::ResolvedMethod_staticAttributes, _remoteMirror, cpIndex, isStore, needAOTValidation);
      auto recv = _stream->read<TR_J9MethodFieldAttributes>();
      attributes = std::get<0>(recv);

      cacheFieldAttributes(cpIndex, attributes, isStatic);
      }
   else
      {
      TR_ASSERT(validateMethodFieldAttributes(attributes, isStatic, cpIndex, isStore, needAOTValidation), "static attributes from client and cache do not match");
      }

   bool result;
   attributes.setMethodFieldAttributesResult(address, type, volatileP, isFinal, isPrivate, unresolvedInCP, &result);

   return result;
   }

TR_OpaqueClassBlock * 
TR_ResolvedJ9JITServerMethod::definingClassFromCPFieldRef(TR::Compilation *comp, int32_t cpIndex, bool isStatic, TR_OpaqueClassBlock **fromResolvedJ9Method)
   {
   TR::CompilationInfoPerThread *compInfoPT = _fe->_compInfoPT;
      {
      OMR::CriticalSection getRemoteROMClass(compInfoPT->getClientData()->getROMMapMonitor());
      auto &cache = getJ9ClassInfo(compInfoPT, _ramClass)._fieldOrStaticDefiningClassCache;
      auto it = cache.find(cpIndex);
      if (it != cache.end())
         {
         if (fromResolvedJ9Method != NULL)
            *fromResolvedJ9Method = it->second;
         return it->second;
         }
      }
   _stream->write(JITServer::MessageType::ResolvedMethod_definingClassFromCPFieldRef, _remoteMirror, cpIndex, isStatic);
   TR_OpaqueClassBlock *resolvedClass = std::get<0>(_stream->read<TR_OpaqueClassBlock *>());
   // Do not cache if the class is unresolved, because it may become resolved later on
   if (resolvedClass)
      {
      OMR::CriticalSection getRemoteROMClass(compInfoPT->getClientData()->getROMMapMonitor());
      auto &cache = getJ9ClassInfo(compInfoPT, _ramClass)._fieldOrStaticDefiningClassCache;
      cache.insert({cpIndex, resolvedClass});
      }
   if (fromResolvedJ9Method != NULL)
      *fromResolvedJ9Method = resolvedClass;

   return resolvedClass;
   }

TR_OpaqueClassBlock *
TR_ResolvedJ9JITServerMethod::getClassFromConstantPool(TR::Compilation * comp, uint32_t cpIndex, bool returnClassForAOT)
   {
   if (cpIndex == -1)
      return NULL;

   auto compInfoPT = (TR::CompilationInfoPerThreadRemote *) _fe->_compInfoPT;
   bool doRuntimeResolve = compInfoPT->getClientData()->getRtResolve() && !comp->ilGenRequest().details().isMethodHandleThunk();
   if (doRuntimeResolve &&
       performTransformation(comp, "Setting as unresolved class from CP cpIndex=%d\n",cpIndex))
      {
      return NULL;
      }

      {
      // This persistent cache must only be checked when doRuntimeResolve is false,
      // otherwise a non method handle thunk compilation can return cached value, instead of NULL.
      OMR::CriticalSection getRemoteROMClass(compInfoPT->getClientData()->getROMMapMonitor());
      auto &constantClassPoolCache = getJ9ClassInfo(compInfoPT, _ramClass)._constantClassPoolCache;
      auto it = constantClassPoolCache.find(cpIndex);
      if (it != constantClassPoolCache.end())
         return it->second;
      }
   _stream->write(JITServer::MessageType::ResolvedMethod_getClassFromConstantPool, _remoteMirror, cpIndex, returnClassForAOT);
   TR_OpaqueClassBlock *resolvedClass = std::get<0>(_stream->read<TR_OpaqueClassBlock *>());

   if (resolvedClass)
      {
      OMR::CriticalSection getRemoteROMClass(compInfoPT->getClientData()->getROMMapMonitor());
      auto &constantClassPoolCache = getJ9ClassInfo(compInfoPT, _ramClass)._constantClassPoolCache;
      constantClassPoolCache.insert({cpIndex, resolvedClass});
      }
   return resolvedClass;
   }

TR_OpaqueClassBlock *
TR_ResolvedJ9JITServerMethod::getDeclaringClassFromFieldOrStatic(TR::Compilation *comp, int32_t cpIndex)
   {
   TR::CompilationInfoPerThread *compInfoPT = _fe->_compInfoPT;
      {
      OMR::CriticalSection getRemoteROMClass(compInfoPT->getClientData()->getROMMapMonitor());
      auto &cache = getJ9ClassInfo(compInfoPT, _ramClass)._fieldOrStaticDeclaringClassCache;
      auto it = cache.find(cpIndex);
      if (it != cache.end())
         return it->second;
      }
   _stream->write(JITServer::MessageType::ResolvedMethod_getDeclaringClassFromFieldOrStatic, _remoteMirror, cpIndex);
   TR_OpaqueClassBlock *declaringClass = std::get<0>(_stream->read<TR_OpaqueClassBlock *>());
   if (declaringClass)
      {
      OMR::CriticalSection getRemoteROMClass(compInfoPT->getClientData()->getROMMapMonitor());
      auto &cache = getJ9ClassInfo(compInfoPT, _ramClass)._fieldOrStaticDeclaringClassCache;
      cache.insert({cpIndex, declaringClass});
      }
   return declaringClass;
   }

TR_OpaqueClassBlock *
TR_ResolvedJ9JITServerMethod::classOfStatic(I_32 cpIndex, bool returnClassForAOT)
   {
   // if method is unresolved, return right away
   if (cpIndex < 0)
      return NULL;

   auto compInfoPT = static_cast<TR::CompilationInfoPerThreadRemote *>(_fe->_compInfoPT);
      {
      OMR::CriticalSection getRemoteROMClass(compInfoPT->getClientData()->getROMMapMonitor()); 
      auto &classOfStaticCache = getJ9ClassInfo(compInfoPT, _ramClass)._classOfStaticCache;
      auto it = classOfStaticCache.find(cpIndex);
      if (it != classOfStaticCache.end())
         return it->second;
      }

   // check per-compilation cache, which can only contain NULL values
   if (compInfoPT->getCachedNullClassOfStatic((TR_OpaqueClassBlock *) _ramClass, cpIndex))
      return NULL;

   _stream->write(JITServer::MessageType::ResolvedMethod_classOfStatic, _remoteMirror, cpIndex, returnClassForAOT);
   TR_OpaqueClassBlock *classOfStatic = std::get<0>(_stream->read<TR_OpaqueClassBlock *>());
   if (classOfStatic)
      {
      // reacquire monitor and cache, if client returned a valid class
      // if client returned NULL, don't cache, because class might not be fully initialized,
      // so the result may change in the future
      OMR::CriticalSection getRemoteROMClass(compInfoPT->getClientData()->getROMMapMonitor()); 
      auto &classOfStaticCache = getJ9ClassInfo(compInfoPT, _ramClass)._classOfStaticCache;
      classOfStaticCache.insert({cpIndex, classOfStatic});
      }
   else
      {
      // cache NULL in a per-compilation cache, even if it becomes initialized during
      // current compilation, shouldn't have much of an effect on performance.
      compInfoPT->cacheNullClassOfStatic((TR_OpaqueClassBlock *) _ramClass, cpIndex);
      }
   return classOfStatic;
   }

void *
TR_ResolvedJ9JITServerMethod::getConstantDynamicTypeFromCP(I_32 cpIndex)
   {
   TR_ASSERT_FATAL(cpIndex != -1, "ConstantDynamic cpIndex shouldn't be -1");
   _stream->write(JITServer::MessageType::ResolvedMethod_getConstantDynamicTypeFromCP, _remoteMirror, cpIndex);

   auto recv = _stream->read<std::string>();
   auto &retConstantDynamicTypeStr = std::get<0>(recv);
   TR_Memory *trMemory = ((TR::CompilationInfoPerThreadRemote *)_fe->_compInfoPT)->getCompilation()->trMemory();
   J9UTF8 *constantDynamicType = str2utf8(retConstantDynamicTypeStr.data(), retConstantDynamicTypeStr.length(), trMemory, heapAlloc);
   return constantDynamicType;
   }

bool
TR_ResolvedJ9JITServerMethod::isConstantDynamic(I_32 cpIndex)
   {
   TR_ASSERT_FATAL(cpIndex != -1, "ConstantDynamic cpIndex shouldn't be -1");
   UDATA cpType = J9_CP_TYPE(J9ROMCLASS_CPSHAPEDESCRIPTION(_romClass), cpIndex);
   return (J9CPTYPE_CONSTANT_DYNAMIC == cpType);
   }

bool
TR_ResolvedJ9JITServerMethod::isUnresolvedConstantDynamic(I_32 cpIndex)
   {
   TR_ASSERT(cpIndex != -1, "ConstantDynamic cpIndex shouldn't be -1");
   _stream->write(JITServer::MessageType::ResolvedMethod_isUnresolvedConstantDynamic, _remoteMirror, cpIndex);
   return std::get<0>(_stream->read<bool>());
   }

void *
TR_ResolvedJ9JITServerMethod::dynamicConstant(I_32 cpIndex, uintptr_t *obj)
   {
   TR_ASSERT_FATAL(cpIndex != -1, "ConstantDynamic cpIndex shouldn't be -1");

   _stream->write(JITServer::MessageType::ResolvedMethod_dynamicConstant, _remoteMirror, cpIndex);

   auto recv = _stream->read<uintptr_t *, uintptr_t>();
   uintptr_t *objLocation = std::get<0>(recv);
   if (obj)
      {
      *obj = std::get<1>(recv);
      }

   return objLocation;
   }

TR_ResolvedMethod *
TR_ResolvedJ9JITServerMethod::getResolvedPossiblyPrivateVirtualMethod(TR::Compilation * comp, I_32 cpIndex, bool ignoreRtResolve, bool * unresolvedInCP)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
#if TURN_OFF_INLINING
   return 0;
#else
   auto compInfoPT = (TR::CompilationInfoPerThreadRemote *) _fe->_compInfoPT;
   TR_ResolvedMethod *resolvedMethod = NULL;

   // See if the constant pool entry is already resolved or not
   if (unresolvedInCP)
       *unresolvedInCP = true;

   bool shouldCompileTimeResolve = shouldCompileTimeResolveMethod(cpIndex);

   if (!((compInfoPT->getClientData()->getRtResolve()) &&
         !comp->ilGenRequest().details().isMethodHandleThunk() && // cmvc 195373
         performTransformation(comp, "Setting as unresolved virtual call cpIndex=%d\n",cpIndex) ) || ignoreRtResolve || shouldCompileTimeResolve)
      {
      if (compInfoPT->getCachedResolvedMethod(
            compInfoPT->getResolvedMethodKey(TR_ResolvedMethodType::VirtualFromCP, (TR_OpaqueClassBlock *) _ramClass, cpIndex),
            this,
            &resolvedMethod,
            unresolvedInCP))
         {
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
         }

      _stream->write(JITServer::MessageType::ResolvedMethod_getResolvedPossiblyPrivateVirtualMethodAndMirror, (TR_ResolvedMethod *) _remoteMirror, literals(), cpIndex);
      auto recv = _stream->read<J9Method *, UDATA, bool, TR_ResolvedJ9JITServerMethodInfo>();
      J9Method *ramMethod = std::get<0>(recv);
      UDATA vTableIndex = std::get<1>(recv);
      bool isUnresolvedInCP = std::get<2>(recv);

      if (unresolvedInCP)
         *unresolvedInCP = isUnresolvedInCP;

      bool createResolvedMethod = true;

      if (comp->compileRelocatableCode() && ramMethod && comp->getOption(TR_UseSymbolValidationManager))
         {
         if (!comp->getSymbolValidationManager()->addVirtualMethodFromCPRecord((TR_OpaqueMethodBlock *)ramMethod, cp(), cpIndex))
            createResolvedMethod = false;
         }

      if (vTableIndex)
         {
         TR_AOTInliningStats *aotStats = NULL;
         if (comp->getOption(TR_EnableAOTStats))
            aotStats = & (((TR_JitPrivateConfig *)_fe->_jitConfig->privateConfig)->aotStats->virtualMethods);

         TR_ResolvedJ9JITServerMethodInfo &methodInfo = std::get<3>(recv);
         
         // call constructor without making a new query
         if (createResolvedMethod)
            {
            resolvedMethod = createResolvedMethodFromJ9Method(comp, cpIndex, vTableIndex, ramMethod, unresolvedInCP, aotStats, methodInfo);
            compInfoPT->cacheResolvedMethod(compInfoPT->getResolvedMethodKey(TR_ResolvedMethodType::VirtualFromCP, (TR_OpaqueClassBlock *) _ramClass, cpIndex), (TR_OpaqueMethodBlock *) ramMethod, (uint32_t) vTableIndex, methodInfo);
            }
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
TR_ResolvedJ9JITServerMethod::getResolvedVirtualMethod(
   TR::Compilation *comp,
   I_32 cpIndex,
   bool ignoreRtResolve,
   bool *unresolvedInCP)
   {
   TR_ResolvedMethod *method = getResolvedPossiblyPrivateVirtualMethod(
      comp,
      cpIndex,
      ignoreRtResolve,
      unresolvedInCP);

   return (method == NULL || method->isPrivate()) ? NULL : method;
   }

bool
TR_ResolvedJ9JITServerMethod::fieldsAreSame(int32_t cpIndex1, TR_ResolvedMethod *m2, int32_t cpIndex2, bool &sigSame)
   {
   if (TR::comp()->compileRelocatableCode())
      // in AOT, this should always return false, because mainline compares class loaders
      // with fe->sameClassLoaders, which always returns false for AOT compiles
      return false;
   TR_ResolvedJ9JITServerMethod *serverMethod2 = static_cast<TR_ResolvedJ9JITServerMethod*>(m2);
   if (getClassLoader() != serverMethod2->getClassLoader())
      return false;

   if (cpIndex1 == -1 || cpIndex2 == -1)
      return false;

   if (cpIndex1 == cpIndex2 && ramMethod() == serverMethod2->ramMethod())
      return true;
   
   int32_t sig1Len = 0, sig2Len = 0;
   char *signature1 = fieldOrStaticSignatureChars(cpIndex1, sig1Len);
   char *signature2 = serverMethod2->fieldOrStaticSignatureChars(cpIndex2, sig2Len);
   
   int32_t name1Len = 0, name2Len = 0;
   char *name1 = fieldOrStaticNameChars(cpIndex1, name1Len);
   char *name2 = serverMethod2->fieldOrStaticNameChars(cpIndex2, name2Len);

   if (sig1Len == sig2Len && !memcmp(signature1, signature2, sig1Len) &&
       name1Len == name2Len && !memcmp(name1, name2, name1Len))
      {
      int32_t class1Len = 0, class2Len = 0;
      char *declaringClassName1 = classNameOfFieldOrStatic(cpIndex1, class1Len);
      char *declaringClassName2 = serverMethod2->classNameOfFieldOrStatic(cpIndex2, class2Len);

      if (class1Len == class2Len && !memcmp(declaringClassName1, declaringClassName2, class1Len))
          return true;
      }
   else
      {
      sigSame = false;
      }
   return false;
   }

bool
TR_ResolvedJ9JITServerMethod::staticsAreSame(int32_t cpIndex1, TR_ResolvedMethod *m2, int32_t cpIndex2, bool &sigSame)
   {
   if (TR::comp()->compileRelocatableCode())
      // in AOT, this should always return false, because mainline compares class loaders
      // with fe->sameClassLoaders, which always returns false for AOT compiles
      return false;
   TR_ResolvedJ9JITServerMethod *serverMethod2 = static_cast<TR_ResolvedJ9JITServerMethod*>(m2);
   if (getClassLoader() != serverMethod2->getClassLoader())
      return false;

   if (cpIndex1 == -1 || cpIndex2 == -1)
      return false;

   if (cpIndex1 == cpIndex2 && ramMethod() == serverMethod2->ramMethod())
      return true;
   
   // the client version of this method compares the addresses of values first, which we cannot do on the server. 
   // skipping that step affects performance, but does not affect correctness. 
   int32_t sig1Len = 0, sig2Len = 0;
   char *signature1 = fieldOrStaticSignatureChars(cpIndex1, sig1Len);
   char *signature2 = serverMethod2->fieldOrStaticSignatureChars(cpIndex2, sig2Len);
   
   int32_t name1Len = 0, name2Len = 0;
   char *name1 = fieldOrStaticNameChars(cpIndex1, name1Len);
   char *name2 = serverMethod2->fieldOrStaticNameChars(cpIndex2, name2Len);

   if (sig1Len == sig2Len && !memcmp(signature1, signature2, sig1Len) &&
       name1Len == name2Len && !memcmp(name1, name2, name1Len))
      {
      int32_t class1Len = 0, class2Len = 0;
      char *declaringClassName1 = classNameOfFieldOrStatic(cpIndex1, class1Len);
      char *declaringClassName2 = serverMethod2->classNameOfFieldOrStatic(cpIndex2, class2Len);

      if (class1Len == class2Len && !memcmp(declaringClassName1, declaringClassName2, class1Len))
          return true;
      }
   else
      {
      sigSame = false;
      }
   return false;
   }

TR_FieldAttributesCache &
TR_ResolvedJ9JITServerMethod::getAttributesCache(bool isStatic, bool unresolvedInCP)
   {
   // Return a persistent attributes cache for regular JIT compilations
   TR::CompilationInfoPerThread *compInfoPT = _fe->_compInfoPT;
   auto &attributesCache = isStatic ? 
      getJ9ClassInfo(compInfoPT, _ramClass)._staticAttributesCache :
      getJ9ClassInfo(compInfoPT, _ramClass)._fieldAttributesCache;
   return attributesCache;
   }

bool
TR_ResolvedJ9JITServerMethod::getCachedFieldAttributes(int32_t cpIndex, TR_J9MethodFieldAttributes &attributes, bool isStatic)
   {
   auto compInfoPT = static_cast<TR::CompilationInfoPerThreadRemote *>(_fe->_compInfoPT);
      {
      // First, search a global cache
      OMR::CriticalSection getRemoteROMClass(compInfoPT->getClientData()->getROMMapMonitor()); 
      auto &attributesCache = getAttributesCache(isStatic);
      auto it = attributesCache.find(cpIndex);
      if (it != attributesCache.end())
         {
         attributes = it->second;
         return true;
         }
      }

   // If global cache is empty, search local cache
   // Local cache is searched after global, because it only stores
   // unresolved field attributes, so most attributes should be stored globally
   return compInfoPT->getCachedFieldOrStaticAttributes((TR_OpaqueClassBlock *) _ramClass, cpIndex, attributes, isStatic);
   }

void
TR_ResolvedJ9JITServerMethod::cacheFieldAttributes(int32_t cpIndex, const TR_J9MethodFieldAttributes &attributes, bool isStatic)
   {
   // When caching field or static attributes, there are 2 caches where they can go:
   // 1. Persistent cache - stores attributes of all resolved fields. Since the field is resolved,
   // the attributes should not change, so the contents should be valid.
   // 2. Per-compilation cache - stores attributes of unresolved fields, which might become resolved
   // during the current compilation, but assuming they are unresolved until the end of the compilation
   // is still functionally correct and does not seem to affect performance.
   auto compInfoPT = static_cast<TR::CompilationInfoPerThreadRemote *>(_fe->_compInfoPT);
   if (attributes.isUnresolvedInCP())
      {
      // field is unresolved in CP, can later become resolved, can only be cached per resolved method.
      compInfoPT->cacheFieldOrStaticAttributes((TR_OpaqueClassBlock *) _ramClass, cpIndex, attributes, isStatic);
      }
   else
      {
      // field is resolved in CP, can cache globally per RAM class.
      OMR::CriticalSection getRemoteROMClass(compInfoPT->getClientData()->getROMMapMonitor()); 
      auto &attributesCache = getAttributesCache(isStatic);
#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
      TR_ASSERT(canCacheFieldAttributes(cpIndex, attributes, isStatic), "new and cached field attributes are not equal");
#endif
      attributesCache.insert({cpIndex, attributes});
      }
   }

bool
TR_ResolvedJ9JITServerMethod::canCacheFieldAttributes(int32_t cpIndex, const TR_J9MethodFieldAttributes &attributes, bool isStatic)
   {
   auto &attributesCache = getAttributesCache(isStatic);
   auto it = attributesCache.find(cpIndex);
   if (it != attributesCache.end())
      {
      // Attempting to cache attributes when this key is already cached.
      // This case can happen when two threads call `getCachedFieldAttributes`,
      // see that attributes are not cached, and they both make a remote call
      // and call this method.
      // Make sure that attributes they are caching are the same.
      auto cachedAttrs = it->second;
      return attributes == cachedAttrs;
      }
   return true;
   }

bool
TR_ResolvedJ9JITServerMethod::fieldAttributes(TR::Compilation * comp, I_32 cpIndex, U_32 * fieldOffset, TR::DataType * type, bool * volatileP, bool * isFinal, bool * isPrivate, bool isStore, bool * unresolvedInCP, bool needAOTValidation)
   {
   bool isStatic = false;
   TR_J9MethodFieldAttributes attributes;
   if(!getCachedFieldAttributes(cpIndex, attributes, isStatic))
      {
      // make a remote call, if attributes are not cached
      _stream->write(JITServer::MessageType::ResolvedMethod_fieldAttributes, _remoteMirror, cpIndex, isStore, needAOTValidation);
      auto recv = _stream->read<TR_J9MethodFieldAttributes>();
      attributes = std::get<0>(recv);

      cacheFieldAttributes(cpIndex, attributes, isStatic);
      }
   else
      {
      TR_ASSERT(validateMethodFieldAttributes(attributes, isStatic, cpIndex, isStore, needAOTValidation), "field attributes from client and cache do not match");
      }
   
   bool result;
   attributes.setMethodFieldAttributesResult(fieldOffset, type, volatileP, isFinal, isPrivate, unresolvedInCP, &result);
   return result;
   }

static bool doResolveAtRuntime(J9Method *method, I_32 cpIndex, TR::Compilation *comp)
   {
   if (!method)
      return true;
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   auto stream = fej9->_compInfoPT->getMethodBeingCompiled()->_stream;
   if (comp->ilGenRequest().details().isMethodHandleThunk()) // cmvc 195373
      {
      return false;
      }
   else if (fej9->_compInfoPT->getClientData()->getRtResolve())
      {
      return performTransformation(comp, "Setting as unresolved static call cpIndex=%d\n", cpIndex);
      }

   return false;
   }

TR_ResolvedMethod *
TR_ResolvedJ9JITServerMethod::getResolvedStaticMethod(TR::Compilation * comp, I_32 cpIndex, bool * unresolvedInCP)
   {
   // JITServer TODO: Decide whether the counters should be updated on the server or the client
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");

   auto compInfoPT = (TR::CompilationInfoPerThreadRemote *) _fe->_compInfoPT;
   TR_ResolvedMethod *resolvedMethod = NULL;
   if (compInfoPT->getCachedResolvedMethod(compInfoPT->getResolvedMethodKey(TR_ResolvedMethodType::Static, (TR_OpaqueClassBlock *) _ramClass, cpIndex), this, &resolvedMethod, unresolvedInCP))
      {
      if ((resolvedMethod == NULL) && unresolvedInCP)
         handleUnresolvedStaticMethodInCP(cpIndex, unresolvedInCP);
      return resolvedMethod;
      }

   _stream->write(JITServer::MessageType::ResolvedMethod_getResolvedStaticMethodAndMirror, _remoteMirror, cpIndex);
   auto recv = _stream->read<J9Method *, TR_ResolvedJ9JITServerMethodInfo, bool>();
   J9Method * ramMethod = std::get<0>(recv);
   bool isUnresolvedInCP = std::get<2>(recv);
   if (unresolvedInCP)
      *unresolvedInCP = isUnresolvedInCP;

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
      // only works on resovled methods.
      //
      if (ramMethod)
         skipForDebugging = false;
      else
         {
         comp->failCompilation<TR::ILGenFailure>("Can't compile an archetype specimen with unresolved calls");
         }
      }

   auto &methodInfo = std::get<1>(recv);
   if (ramMethod && !skipForDebugging)
      {
      TR_AOTInliningStats *aotStats = NULL;
      if (comp->getOption(TR_EnableAOTStats))
         aotStats = & (((TR_JitPrivateConfig *)_fe->_jitConfig->privateConfig)->aotStats->staticMethods);
      resolvedMethod = createResolvedMethodFromJ9Method(comp, cpIndex, 0, ramMethod, unresolvedInCP, aotStats, methodInfo);
      if (unresolvedInCP)
         *unresolvedInCP = false;
      }

   if (resolvedMethod == NULL)
      {
      if (unresolvedInCP)
         handleUnresolvedStaticMethodInCP(cpIndex, unresolvedInCP);
      }
   else
      {
      compInfoPT->cacheResolvedMethod(compInfoPT->getResolvedMethodKey(TR_ResolvedMethodType::Static, (TR_OpaqueClassBlock *) _ramClass, cpIndex), (TR_OpaqueMethodBlock *) ramMethod, 0, methodInfo);
      }

   return resolvedMethod;
   }

TR_ResolvedMethod *
TR_ResolvedJ9JITServerMethod::getResolvedSpecialMethod(TR::Compilation * comp, I_32 cpIndex, bool * unresolvedInCP)
   {
   // JITServer TODO: Decide whether the counters should be updated on the server or the client
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
   
   auto compInfoPT = (TR::CompilationInfoPerThreadRemote *) _fe->_compInfoPT;
   TR_ResolvedMethod *resolvedMethod = NULL;
   TR_OpaqueClassBlock *clazz = (TR_OpaqueClassBlock *) _ramClass;
   if (compInfoPT->getCachedResolvedMethod(compInfoPT->getResolvedMethodKey(TR_ResolvedMethodType::Special, clazz, cpIndex), this, &resolvedMethod, unresolvedInCP))
      {
      if ((resolvedMethod == NULL) && unresolvedInCP)
         handleUnresolvedSpecialMethodInCP(cpIndex, unresolvedInCP);
      return resolvedMethod;
      }

   // Comments from TR_ResolvedJ9Method::getResolvedSpecialMethod
   // We init the CP with a special magic method, which has no bytecodes (hence bytecode start is NULL)
   // i.e. the CP will always contain a method for special and static methods
   if (unresolvedInCP)
      *unresolvedInCP = true;

   _stream->write(JITServer::MessageType::ResolvedMethod_getResolvedSpecialMethodAndMirror, _remoteMirror, cpIndex);
   auto recv = _stream->read<J9Method *, TR_ResolvedJ9JITServerMethodInfo>();
   J9Method * ramMethod = std::get<0>(recv);
   auto &methodInfo = std::get<1>(recv);

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
         resolvedMethod = createResolvedMethodFromJ9Method(comp, cpIndex, 0, ramMethod, unresolvedInCP, aotStats, methodInfo);
      if (unresolvedInCP)
         *unresolvedInCP = false;
      }

   if (resolvedMethod == NULL)
      {
      if (unresolvedInCP)
         handleUnresolvedSpecialMethodInCP(cpIndex, unresolvedInCP);
      }
   else
      {
      compInfoPT->cacheResolvedMethod(compInfoPT->getResolvedMethodKey(TR_ResolvedMethodType::Special, clazz, cpIndex), (TR_OpaqueMethodBlock *) ramMethod, 0, methodInfo);
      }

   return resolvedMethod;
   }

TR_ResolvedMethod *
TR_ResolvedJ9JITServerMethod::createResolvedMethodFromJ9Method( TR::Compilation *comp, int32_t cpIndex, uint32_t vTableSlot, J9Method *j9Method, bool * unresolvedInCP, TR_AOTInliningStats *aotStats)
   {
   TR_ResolvedMethod *m = new (comp->trHeapMemory()) TR_ResolvedJ9JITServerMethod((TR_OpaqueMethodBlock *) j9Method, _fe, comp->trMemory(), this, vTableSlot);
   if (((TR_ResolvedJ9Method*)m)->isSignaturePolymorphicMethod())
      {
      // Signature polymorphic method's signature varies at different call sites and will be different than its declared signature
      int32_t signatureLength;
      char   *signature = getMethodSignatureFromConstantPool(cpIndex, signatureLength);
      ((TR_ResolvedJ9Method *)m)->setSignature(signature, signatureLength, comp->trMemory());
      }
   return m;
   }

TR_ResolvedMethod *
TR_ResolvedJ9JITServerMethod::createResolvedMethodFromJ9Method( TR::Compilation *comp, int32_t cpIndex, uint32_t vTableSlot, J9Method *j9Method, bool * unresolvedInCP, TR_AOTInliningStats *aotStats, const TR_ResolvedJ9JITServerMethodInfo &methodInfo)
   {
   TR_ResolvedMethod *m = new (comp->trHeapMemory()) TR_ResolvedJ9JITServerMethod((TR_OpaqueMethodBlock *) j9Method, _fe, comp->trMemory(), methodInfo, this, vTableSlot);
   if (((TR_ResolvedJ9Method*)m)->isSignaturePolymorphicMethod())
      {
      // Signature polymorphic method's signature varies at different call sites and will be different than its declared signature
      int32_t signatureLength;
      char   *signature = getMethodSignatureFromConstantPool(cpIndex, signatureLength);
      ((TR_ResolvedJ9Method *)m)->setSignature(signature, signatureLength, comp->trMemory());
      }
   return m;
   }

void *
TR_ResolvedJ9JITServerMethod::startAddressForJittedMethod()
   {
   // return the cached value if we have any
   if (_startAddressForJittedMethod)
      {
      return _startAddressForJittedMethod;
      }
   else // Otherwise ask the client for it
      {
      _stream->write(JITServer::MessageType::ResolvedMethod_startAddressForJittedMethod, _remoteMirror);
      return std::get<0>(_stream->read<void *>());
      }
   }

char *
TR_ResolvedJ9JITServerMethod::localName(U_32 slotNumber, U_32 bcIndex, I_32 &len, TR_Memory *trMemory)
   {
   _stream->write(JITServer::MessageType::ResolvedMethod_localName, _remoteMirror, slotNumber, bcIndex);
   auto recv = _stream->read<std::string>();
   auto &nameString = std::get<0>(recv);
   len = nameString.length();
   char *out = (char *)trMemory->allocateHeapMemory(len);
   memcpy(out, nameString.data(), len);
   return out;
   }

TR_OpaqueClassBlock *
TR_ResolvedJ9JITServerMethod::getResolvedInterfaceMethod(I_32 cpIndex, UDATA *pITableIndex)
   {
   _stream->write(JITServer::MessageType::ResolvedMethod_getResolvedInterfaceMethod_2, _remoteMirror, cpIndex);
   auto recv = _stream->read<TR_OpaqueClassBlock *, UDATA>();
   *pITableIndex = std::get<1>(recv);
   auto result = std::get<0>(recv);

   auto comp = _fe->_compInfoPT->getCompilation();
   if (comp && comp->compileRelocatableCode() && comp->getOption(TR_UseSymbolValidationManager))
      {
      if (!comp->getSymbolValidationManager()->addClassFromITableIndexCPRecord(result, cp(), cpIndex))
         result = NULL;
      }
   return result;
   }

TR_ResolvedMethod *
TR_ResolvedJ9JITServerMethod::getResolvedInterfaceMethod(TR::Compilation * comp, TR_OpaqueClassBlock * classObject, I_32 cpIndex)
   {
   TR_ResolvedMethod *resolvedMethod = NULL;
   auto compInfoPT = (TR::CompilationInfoPerThreadRemote *) _fe->_compInfoPT;
   TR_OpaqueClassBlock *clazz = (TR_OpaqueClassBlock *) _ramClass;
   if (compInfoPT->getCachedResolvedMethod(compInfoPT->getResolvedMethodKey(TR_ResolvedMethodType::Interface, clazz, cpIndex, classObject), this, &resolvedMethod))
      return resolvedMethod;
   
   _stream->write(JITServer::MessageType::ResolvedMethod_getResolvedInterfaceMethodAndMirror_3, getPersistentIdentifier(), classObject, cpIndex, _remoteMirror);
   auto recv = _stream->read<bool, J9Method*, TR_ResolvedJ9JITServerMethodInfo>();
   bool resolved = std::get<0>(recv);
   J9Method *ramMethod = std::get<1>(recv);
   auto &methodInfo = std::get<2>(recv);

   if (comp && comp->getOption(TR_UseSymbolValidationManager))
      {
      if (!comp->getSymbolValidationManager()->addInterfaceMethodFromCPRecord((TR_OpaqueMethodBlock *) ramMethod,
                                                                              (TR_OpaqueClassBlock *) ((TR_J9VM *) _fe)->getClassFromMethodBlock(getPersistentIdentifier()),
                                                                              classObject,
                                                                              cpIndex))
         {
         return NULL;
         }
      }


   // If the method ref is unresolved, the bytecodes of the ramMethod will be NULL.
   // IFF resolved, then we can look at the rest of the ref.
   //
   if (resolved)
      {
      TR_AOTInliningStats *aotStats = NULL;
      if (comp->getOption(TR_EnableAOTStats))
         aotStats = & (((TR_JitPrivateConfig *)_fe->_jitConfig->privateConfig)->aotStats->interfaceMethods);
      TR_ResolvedMethod *m = createResolvedMethodFromJ9Method(comp, cpIndex, 0, ramMethod, NULL, aotStats, methodInfo);

      TR_OpaqueClassBlock *c = NULL;
      if (m)
         {
         c = m->classOfMethod();
         if (c && !_fe->isInterfaceClass(c))
            {
            TR::DebugCounter::incStaticDebugCounter(comp, "resources.resolvedMethods/interface");
            TR::DebugCounter::incStaticDebugCounter(comp, "resources.resolvedMethods/interface:#bytes", sizeof(TR_ResolvedJ9Method));
            resolvedMethod = m;
            }
         }
      }
   // for resolved interface method, need to know cpIndex, as well as classObject
   // to uniquely identify it.
   if (resolvedMethod)
      {
      compInfoPT->cacheResolvedMethod(compInfoPT->getResolvedMethodKey(TR_ResolvedMethodType::Interface, clazz, cpIndex, classObject), (TR_OpaqueMethodBlock *) ramMethod, 0, methodInfo);
      return resolvedMethod;
      }

   TR::DebugCounter::incStaticDebugCounter(comp, "resources.resolvedMethods/interface/null");
   return 0;
   }

U_32
TR_ResolvedJ9JITServerMethod::getResolvedInterfaceMethodOffset(TR_OpaqueClassBlock * classObject, I_32 cpIndex)
   {
   _stream->write(JITServer::MessageType::ResolvedMethod_getResolvedInterfaceMethodOffset, _remoteMirror, classObject, cpIndex);
   auto recv = _stream->read<U_32>();
   return std::get<0>(recv);
   }

/* Only returns non-null if the method is not to be dispatched by itable, i.e.
 * if it is:
 * - private (isPrivate()), using direct dispatch;
 * - a final method of Object (isFinalInObject()), using direct dispatch; or
 * - a non-final method of Object, using virtual dispatch.
 */
TR_ResolvedMethod *
TR_ResolvedJ9JITServerMethod::getResolvedImproperInterfaceMethod(TR::Compilation * comp, I_32 cpIndex)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
#if TURN_OFF_INLINING
   return 0;
#else
   if (!(_fe->_compInfoPT->getClientData()->getRtResolve()))
      {
      // check for cache first
      auto compInfoPT = (TR::CompilationInfoPerThreadRemote *) _fe->_compInfoPT;
      TR_ResolvedMethod * resolvedMethod = NULL;
      if (compInfoPT->getCachedResolvedMethod(compInfoPT->getResolvedMethodKey(TR_ResolvedMethodType::ImproperInterface, (TR_OpaqueClassBlock *) _ramClass, cpIndex), this, &resolvedMethod)) 
         return resolvedMethod;

      // query for resolved method and create its mirror at the same time
      _stream->write(JITServer::MessageType::ResolvedMethod_getResolvedImproperInterfaceMethodAndMirror, _remoteMirror, cpIndex);
      auto recv = _stream->read<J9Method *, TR_ResolvedJ9JITServerMethodInfo, UDATA>();
      auto j9method = std::get<0>(recv);
      auto &methodInfo = std::get<1>(recv);
      auto vtableOffset = std::get<2>(recv);

      if (comp->getOption(TR_UseSymbolValidationManager) && j9method)
         {
         if (!comp->getSymbolValidationManager()->addImproperInterfaceMethodFromCPRecord((TR_OpaqueMethodBlock *)j9method, cp(), cpIndex))
            j9method = NULL;
         }

      compInfoPT->cacheResolvedMethod(compInfoPT->getResolvedMethodKey(TR_ResolvedMethodType::ImproperInterface, (TR_OpaqueClassBlock *) _ramClass, cpIndex),
                                     (TR_OpaqueMethodBlock *) j9method, vtableOffset, methodInfo);
      if (j9method == NULL)
         return NULL;
      else
         return createResolvedMethodFromJ9Method(comp, cpIndex, vtableOffset, j9method, NULL, NULL, methodInfo);
      }

   return NULL;
#endif
   }

void *
TR_ResolvedJ9JITServerMethod::startAddressForJNIMethod(TR::Compilation *comp)
   {
   // For fastJNI methods, we have the address cached
   if (_jniProperties)
      return _jniTargetAddress;
   _stream->write(JITServer::MessageType::ResolvedMethod_startAddressForJNIMethod, _remoteMirror);
   return std::get<0>(_stream->read<void *>());
   }

void *
TR_ResolvedJ9JITServerMethod::startAddressForInterpreterOfJittedMethod()
   {
   _stream->write(JITServer::MessageType::ResolvedMethod_startAddressForInterpreterOfJittedMethod, _remoteMirror);
   return std::get<0>(_stream->read<void *>());
   }

TR_ResolvedMethod *
TR_ResolvedJ9JITServerMethod::getResolvedVirtualMethod(TR::Compilation * comp, TR_OpaqueClassBlock * classObject, I_32 virtualCallOffset, bool ignoreRtResolve)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)_fe;
   if (_fe->_compInfoPT->getClientData()->getRtResolve() && !ignoreRtResolve)
      return NULL;

   auto compInfoPT = (TR::CompilationInfoPerThreadRemote *) _fe->_compInfoPT;
   TR_OpaqueClassBlock *clazz = (TR_OpaqueClassBlock *) _ramClass;
   TR_ResolvedMethod *resolvedMethod = NULL;
   // If method is already cached, return right away
   if (compInfoPT->getCachedResolvedMethod(compInfoPT->getResolvedMethodKey(TR_ResolvedMethodType::VirtualFromOffset, clazz, virtualCallOffset, classObject), this, &resolvedMethod)) 
      return resolvedMethod;

   // Remote call finds RAM method at offset and creates a resolved method at the client
   _stream->write(JITServer::MessageType::ResolvedMethod_getResolvedVirtualMethod, classObject, virtualCallOffset, ignoreRtResolve, (TR_ResolvedJ9Method *) getRemoteMirror());
   auto recv = _stream->read<TR_OpaqueMethodBlock *, TR_ResolvedJ9JITServerMethodInfo>();
   auto ramMethod = std::get<0>(recv);
   auto &methodInfo = std::get<1>(recv);

   // it seems better to override this method for AOT but that's what baseline is doing
   // maybe there is a reason for it
   if (_fe->isAOT_DEPRECATED_DO_NOT_USE())
      {
      if (comp && comp->getOption(TR_UseSymbolValidationManager))
         {
         if (!comp->getSymbolValidationManager()->addVirtualMethodFromOffsetRecord(ramMethod, classObject, virtualCallOffset, ignoreRtResolve))
            return NULL;
         }
      resolvedMethod = ramMethod ? new (comp->trHeapMemory()) TR_ResolvedRelocatableJ9JITServerMethod((TR_OpaqueMethodBlock *) ramMethod, _fe, comp->trMemory(), methodInfo, this) : 0;
      }
   else
      {
      resolvedMethod = ramMethod ? new (comp->trHeapMemory()) TR_ResolvedJ9JITServerMethod((TR_OpaqueMethodBlock *) ramMethod, _fe, comp->trMemory(), methodInfo, this) : 0;
      }
   if (resolvedMethod)
      compInfoPT->cacheResolvedMethod(compInfoPT->getResolvedMethodKey(TR_ResolvedMethodType::VirtualFromOffset, clazz, virtualCallOffset, classObject), ramMethod, 0, methodInfo);
   return resolvedMethod;
   }

void *
TR_ResolvedJ9JITServerMethod::stringConstant(I_32 cpIndex)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
   _stream->write(JITServer::MessageType::ResolvedMethod_stringConstant, _remoteMirror, cpIndex);
   auto recv = _stream->read<void *, bool, bool>();

   auto compInfoPT = static_cast<TR::CompilationInfoPerThreadRemote *>(_fe->_compInfoPT);
   compInfoPT->cacheIsUnresolvedStr((TR_OpaqueClassBlock *) _ramClass, cpIndex, TR_IsUnresolvedString(std::get<1>(recv), std::get<2>(recv)));
   return std::get<0>(recv);
   }

bool
TR_ResolvedJ9JITServerMethod::isUnresolvedString(I_32 cpIndex, bool optimizeForAOT)
   {
   auto compInfoPT = static_cast<TR::CompilationInfoPerThreadRemote *>(_fe->_compInfoPT);
   TR_IsUnresolvedString stringAttrs;
   if (compInfoPT->getCachedIsUnresolvedStr((TR_OpaqueClassBlock *) _ramClass, cpIndex, stringAttrs))
      {
      return optimizeForAOT ? stringAttrs._optimizeForAOTTrueResult : stringAttrs._optimizeForAOTFalseResult;
      }
   else
      {
      _stream->write(JITServer::MessageType::ResolvedMethod_isUnresolvedString, _remoteMirror, cpIndex, optimizeForAOT);
      return std::get<0>(_stream->read<bool>());
      }
   }

bool
TR_ResolvedJ9JITServerMethod::isSubjectToPhaseChange(TR::Compilation *comp)
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
      _stream->write(JITServer::MessageType::ResolvedMethod_isSubjectToPhaseChange, _remoteMirror);
      return std::get<0>(_stream->read<bool>());
      // JITServer TODO: cache the JitState when we create  TR_ResolvedJ9JITServerMethod
      // This may not behave exactly like the non-JITServer due to timing differences
      }
   }

TR_ResolvedMethod *
TR_ResolvedJ9JITServerMethod::getResolvedHandleMethod(TR::Compilation *comp, I_32 cpIndex, bool *unresolvedInCP,
                                                      bool *isInvokeCacheAppendixNull)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
#if TURN_OFF_INLINING
   return 0;
#else
   _stream->write(JITServer::MessageType::ResolvedMethod_getResolvedHandleMethod, _remoteMirror, cpIndex);
   auto recv = _stream->read<TR_OpaqueMethodBlock *, TR_ResolvedJ9JITServerMethodInfo, std::string, bool, bool>();
   auto ramMethod = std::get<0>(recv);
   auto &methodInfo = std::get<1>(recv);
   auto &signature = std::get<2>(recv);
   if (unresolvedInCP)
      *unresolvedInCP = std::get<3>(recv);
   if (isInvokeCacheAppendixNull)
      *isInvokeCacheAppendixNull = std::get<4>(recv);

   return static_cast<TR_J9ServerVM *>(_fe)->createResolvedMethodWithSignature(
      comp->trMemory(), ramMethod, NULL, (char *)signature.data(), signature.length(), this, methodInfo
   );
#endif
   }

void *
TR_ResolvedJ9JITServerMethod::methodTypeTableEntryAddress(int32_t cpIndex)
   {
   _stream->write(JITServer::MessageType::ResolvedMethod_methodTypeTableEntryAddress, _remoteMirror, cpIndex);
   return std::get<0>(_stream->read<void*>());
   }

bool
TR_ResolvedJ9JITServerMethod::isUnresolvedMethodTypeTableEntry(int32_t cpIndex)
   {
   _stream->write(JITServer::MessageType::ResolvedMethod_isUnresolvedMethodTypeTableEntry, _remoteMirror, cpIndex);
   return std::get<0>(_stream->read<bool>());
   }

bool
TR_ResolvedJ9JITServerMethod::isUnresolvedCallSiteTableEntry(int32_t callSiteIndex)
   {
   _stream->write(JITServer::MessageType::ResolvedMethod_isUnresolvedCallSiteTableEntry, _remoteMirror, callSiteIndex);
   return std::get<0>(_stream->read<bool>());
   }

void *
TR_ResolvedJ9JITServerMethod::callSiteTableEntryAddress(int32_t callSiteIndex)
   {
   _stream->write(JITServer::MessageType::ResolvedMethod_callSiteTableEntryAddress, _remoteMirror, callSiteIndex);
   return std::get<0>(_stream->read<void*>());
   }

#if defined(J9VM_OPT_METHOD_HANDLE)
bool
TR_ResolvedJ9JITServerMethod::isUnresolvedVarHandleMethodTypeTableEntry(int32_t cpIndex)
   {
   _stream->write(JITServer::MessageType::ResolvedMethod_isUnresolvedVarHandleMethodTypeTableEntry, _remoteMirror, cpIndex);
   return std::get<0>(_stream->read<bool>());
   }

void *
TR_ResolvedJ9JITServerMethod::varHandleMethodTypeTableEntryAddress(int32_t cpIndex)
   {
   _stream->write(JITServer::MessageType::ResolvedMethod_varHandleMethodTypeTableEntryAddress, _remoteMirror, cpIndex);
   return std::get<0>(_stream->read<void*>());
   }
#endif /* defined(J9VM_OPT_METHOD_HANDLE) */

TR_ResolvedMethod *
TR_ResolvedJ9JITServerMethod::getResolvedDynamicMethod(TR::Compilation *comp, I_32 callSiteIndex, bool *unresolvedInCP,
                                                       bool *isInvokeCacheAppendixNull)
   {
   TR_ASSERT(callSiteIndex != -1, "callSiteIndex shouldn't be -1");

#if TURN_OFF_INLINING
   return 0;
#else
   _stream->write(JITServer::MessageType::ResolvedMethod_getResolvedDynamicMethod, _remoteMirror, callSiteIndex);
   auto recv = _stream->read<TR_OpaqueMethodBlock *, TR_ResolvedJ9JITServerMethodInfo, std::string, bool, bool>();
   auto ramMethod = std::get<0>(recv);
   auto &methodInfo = std::get<1>(recv);
   auto &signature = std::get<2>(recv);
   if (unresolvedInCP)
      *unresolvedInCP = std::get<3>(recv);
   if (isInvokeCacheAppendixNull)
      *isInvokeCacheAppendixNull = std::get<4>(recv);

   return static_cast<TR_J9ServerVM *>(_fe)->createResolvedMethodWithSignature(
      comp->trMemory(), ramMethod, NULL, (char *)signature.data(), signature.size(), this, methodInfo
   );
#endif
   }

bool
TR_ResolvedJ9JITServerMethod::shouldFailSetRecognizedMethodInfoBecauseOfHCR()
   {
   _stream->write(JITServer::MessageType::ResolvedMethod_shouldFailSetRecognizedMethodInfoBecauseOfHCR, _remoteMirror);
   return std::get<0>(_stream->read<bool>());
   }

bool
TR_ResolvedJ9JITServerMethod::isSameMethod(TR_ResolvedMethod * m2)
   {
   if (isNative())
      return false; // A jitted JNI method doesn't call itself

   auto other = static_cast<TR_ResolvedJ9JITServerMethod*>(m2);

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

      _stream->write(JITServer::MessageType::ResolvedMethod_isSameMethod, thisHandleLocation, otherHandleLocation);
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
TR_ResolvedJ9JITServerMethod::isInlineable(TR::Compilation *comp)
   {
   // Reduce number of remote queries by testing the options first
   // This assumes knowledge of how the original is implemented
   if (comp->getOption(TR_FullSpeedDebug) && comp->getOption(TR_EnableOSR))
      {
      _stream->write(JITServer::MessageType::ResolvedMethod_isInlineable, _remoteMirror);
      return std::get<0>(_stream->read<bool>());
      }
   else
      {
      return true;
      }
   }

void
TR_ResolvedJ9JITServerMethod::setWarmCallGraphTooBig(uint32_t bcIndex, TR::Compilation *comp)
   {
   // set the variable in the server IProfiler
   TR_ResolvedJ9Method::setWarmCallGraphTooBig(bcIndex, comp);
   // set it on the client IProfiler too, in case a server crashes, client will still have the correct value
   _stream->write(JITServer::MessageType::ResolvedMethod_setWarmCallGraphTooBig, _remoteMirror, bcIndex);
   _stream->read<JITServer::Void>();
   }

void
TR_ResolvedJ9JITServerMethod::setVirtualMethodIsOverridden()
   {
   _stream->write(JITServer::MessageType::ResolvedMethod_setVirtualMethodIsOverridden, _remoteMirror);
   _stream->read<JITServer::Void>();
   }

bool
TR_ResolvedJ9JITServerMethod::methodIsNotzAAPEligible()
   {
   _stream->write(JITServer::MessageType::ResolvedMethod_methodIsNotzAAPEligible, _remoteMirror);
   return std::get<0>(_stream->read<bool>());
   }
void
TR_ResolvedJ9JITServerMethod::setClassForNewInstance(J9Class *c)
   {
   _j9classForNewInstance = c;
   _stream->write(JITServer::MessageType::ResolvedMethod_setClassForNewInstance, _remoteMirror, c);
   _stream->read<JITServer::Void>();
   }

TR_OpaqueClassBlock *
TR_ResolvedJ9JITServerMethod::classOfMethod()
      {
      if (isNewInstanceImplThunk())
         {
         TR_ASSERT(_j9classForNewInstance, "Must have the class for the newInstance");
         //J9Class * clazz = (J9Class *)((intptr_t)ramMethod()->extra & ~J9_STARTPC_NOT_TRANSLATED);
         return _fe->convertClassPtrToClassOffset(_j9classForNewInstance);//(TR_OpaqueClassBlock *&)(rc);
         }
      return _fe->convertClassPtrToClassOffset(_ramClass);
      }

TR_PersistentJittedBodyInfo *
TR_ResolvedJ9JITServerMethod::getExistingJittedBodyInfo()
   {
   return _bodyInfo; // return cached value
   }

void
TR_ResolvedJ9JITServerMethod::getFaninInfo(uint32_t *count, uint32_t *weight, uint32_t *otherBucketWeight)
   {
   uint32_t i = 0;
   uint32_t w = 0;
   if (otherBucketWeight)
      *otherBucketWeight = 0;

   TR_IPMethodHashTableEntry *entry = _iProfilerMethodEntry;
   if (entry)
      {
      w = entry->_otherBucket.getWeight();
      // Iterate through all the callers and add their weight
      for (TR_IPMethodData* it = &entry->_caller; it; it = it->next)
         {
         w += it->getWeight();
         i++;
         }
      if (otherBucketWeight)
         *otherBucketWeight = entry->_otherBucket.getWeight();
      }
   *weight = w;
   *count = i;
   }

bool
TR_ResolvedJ9JITServerMethod::getCallerWeight(TR_ResolvedJ9Method *caller, uint32_t *weight, uint32_t pcIndex)
   {
   TR_OpaqueMethodBlock *callerMethod = caller->getPersistentIdentifier();
   bool useTuples = (pcIndex != ~0);

   //adjust pcIndex for interface calls (see getSearchPCFromMethodAndBCIndex)
   //otherwise we won't be able to locate a caller-callee-bcIndex triplet
   //even if it is in a TR_IPMethodHashTableEntry
   TR_IProfiler *iProfiler = _fe->getIProfiler();
   if (!iProfiler)
      return false;

   uintptr_t pcAddress = iProfiler->getSearchPCFromMethodAndBCIndex(callerMethod, pcIndex, 0);

   TR_IPMethodHashTableEntry *entry = _iProfilerMethodEntry;

   if(!entry)   // if there are no entries, we have no callers!
      {
      *weight = ~0;
      return false;
      }
   for (TR_IPMethodData* it = &entry->_caller; it; it = it->next)
      {
      if( it->getMethod() == callerMethod && (!useTuples || ((((uintptr_t) it->getPCIndex()) + TR::Compiler->mtd.bytecodeStart(callerMethod)) == pcAddress)))
         {
         *weight = it->getWeight();
         return true;
         }
      }

   *weight = entry->_otherBucket.getWeight();
   return false;
   }

void
TR_ResolvedJ9JITServerMethod::createResolvedMethodMirror(TR_ResolvedJ9JITServerMethodInfo &methodInfo, TR_OpaqueMethodBlock *method, uint32_t vTableSlot, TR_ResolvedMethod *owningMethod, TR_FrontEnd *fe, TR_Memory *trMemory)
   {
   // Create resolved method mirror on the client.
   // Should be called to mirror a call to resolved method constructor,
   // will work when method doesn't have an owning method (compilee method).
   // Resolved method should not be NULL
   TR_ResolvedJ9Method *resolvedMethod = NULL;
   if (!((TR_J9VMBase *) fe)->isAOT_DEPRECATED_DO_NOT_USE())
      resolvedMethod = new (trMemory->trHeapMemory()) TR_ResolvedJ9Method(method, fe, trMemory, owningMethod, vTableSlot);
   else
      resolvedMethod = new (trMemory->trHeapMemory()) TR_ResolvedRelocatableJ9Method(method, fe, trMemory, owningMethod, vTableSlot); 
   if (!resolvedMethod) throw std::bad_alloc();

   packMethodInfo(methodInfo, resolvedMethod, fe);
   }

void
TR_ResolvedJ9JITServerMethod::createResolvedMethodFromJ9MethodMirror(TR_ResolvedJ9JITServerMethodInfo &methodInfo, TR_OpaqueMethodBlock *method, uint32_t vTableSlot, TR_ResolvedMethod *owningMethod, TR_FrontEnd *fe, TR_Memory *trMemory)
   {
   // Create resolved method mirror on the client.
   // Should be called to mirror a call to TR_Resolved(Relocatable)MethodFromJ9Method::createResolvedMethodFromJ9Method.
   // Requires owning method to exist to work properly.
   // Resolved method can be NULL.
   TR_ASSERT(owningMethod, "owning method cannot be NULL");
   TR_ResolvedJ9Method *resolvedMethod = NULL;
   // The simplest solution would be to call owningMethod->createResolvedMethodFromJ9Method(...) here. 
   // Thanks to polymorphism, the correct version of the resolved method would be initialized and everything would be simple.
   // However, createResolvedMethodFromJ9Method is declared protected in TR_ResolvedJ9Method, so we can't call it here.
   // Maybe we should make it public, but that would require changing baseline.
   // For now, I'll have to do with mostly copy-pasting code from that method.
   TR_J9VMBase *fej9 = (TR_J9VMBase *) fe;
   if (!fej9->isAOT_DEPRECATED_DO_NOT_USE())
      {
      resolvedMethod = new (trMemory->trHeapMemory()) TR_ResolvedJ9Method(method, fe, trMemory, owningMethod, vTableSlot);
      if (!resolvedMethod) throw std::bad_alloc();
      }
   else
      {
      // Do the same thing createResolvedMethodFromJ9Method does, but without collecting stats
      TR::Compilation *comp = TR::comp();
#if defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
      bool resolveAOTMethods = !comp->getOption(TR_DisableAOTResolveDiffCLMethods);
      bool enableAggressive = comp->getOption(TR_EnableAOTInlineSystemMethod);
      bool isSystemClassLoader = false;
      J9Method *j9method = (J9Method *) method;

         {
         // Check if same classloader
         J9Class *j9clazz = (J9Class *) J9_CLASS_FROM_CP(((J9RAMConstantPoolItem *) J9_CP_FROM_METHOD(((J9Method *)j9method))));
         TR_OpaqueClassBlock *clazzOfInlinedMethod = fej9->convertClassPtrToClassOffset(j9clazz);
         TR_OpaqueClassBlock *clazzOfCompiledMethod = fej9->convertClassPtrToClassOffset(J9_CLASS_FROM_METHOD(((TR_ResolvedJ9Method *) owningMethod)->ramMethod()));

         if (enableAggressive)
            {
            isSystemClassLoader = ((void*)fej9->vmThread()->javaVM->systemClassLoader->classLoaderObject ==  (void*)fej9->getClassLoader(clazzOfInlinedMethod));
            }

         if (fej9->sharedCache()->isROMClassInSharedCache(J9_CLASS_FROM_METHOD(j9method)->romClass))
            {
            bool sameLoaders = false;
            TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;
            if (resolveAOTMethods ||
                (sameLoaders = fej9->sameClassLoaders(clazzOfInlinedMethod, clazzOfCompiledMethod)) ||
                isSystemClassLoader)
               {
               resolvedMethod = new (comp->trHeapMemory()) TR_ResolvedRelocatableJ9Method((TR_OpaqueMethodBlock *) j9method, fe, comp->trMemory(), owningMethod, vTableSlot);
               if (!resolvedMethod) throw std::bad_alloc();
               }
            }
         }

#endif
      }

   packMethodInfo(methodInfo, resolvedMethod, fe);
   }

void
TR_ResolvedJ9JITServerMethod::packMethodInfo(TR_ResolvedJ9JITServerMethodInfo &methodInfo, TR_ResolvedJ9Method *resolvedMethod, TR_FrontEnd *fe)
   {
   auto &methodInfoStruct = std::get<0>(methodInfo);
   if (!resolvedMethod)
      {
      // resolved method not created, setting remoteMirror to NULL indicates
      // that other fields should be ignored.
      methodInfoStruct.remoteMirror = NULL;
      return;
      }


   TR::Compilation *comp = TR::comp();

   // retrieve all relevant attributes of resolvedMethod and set them in methodInfo
   J9RAMConstantPoolItem *literals = (J9RAMConstantPoolItem *)(J9_CP_FROM_METHOD(resolvedMethod->ramMethod()));
   J9Class *cpHdr = J9_CLASS_FROM_CP(literals);

   J9Method *j9method = resolvedMethod->ramMethod();
  
   // fill-in struct fields 
   methodInfoStruct.remoteMirror = resolvedMethod;
   methodInfoStruct.literals = literals;
   methodInfoStruct.ramClass = cpHdr;
   methodInfoStruct.methodIndex = getMethodIndexUnchecked(j9method);
   methodInfoStruct.jniProperties = resolvedMethod->getJNIProperties();
   methodInfoStruct.jniTargetAddress = resolvedMethod->getJNITargetAddress();
   methodInfoStruct.isInterpreted = resolvedMethod->isInterpreted();
   methodInfoStruct.isJNINative = resolvedMethod->isJNINative();
   methodInfoStruct.isMethodInValidLibrary = resolvedMethod->isMethodInValidLibrary();
   methodInfoStruct.mandatoryRm = resolvedMethod->getMandatoryRecognizedMethod();
   methodInfoStruct.rm = ((TR_ResolvedMethod*)resolvedMethod)->getRecognizedMethod();
   methodInfoStruct.startAddressForJittedMethod = TR::CompilationInfo::isCompiled(resolvedMethod->ramMethod()) ?
                                           resolvedMethod->startAddressForJittedMethod() : NULL;
   methodInfoStruct.virtualMethodIsOverridden = resolvedMethod->virtualMethodIsOverridden();
   methodInfoStruct.addressContainingIsOverriddenBit = resolvedMethod->addressContainingIsOverriddenBit();
   methodInfoStruct.classLoader = resolvedMethod->getClassLoader();
   methodInfoStruct.isLambdaFormGeneratedMethod = static_cast<TR_J9VMBase *>(fe)->isLambdaFormGeneratedMethod(resolvedMethod);
   methodInfoStruct.isForceInline = static_cast<TR_J9VMBase *>(fe)->isForceInline(resolvedMethod);
   methodInfoStruct.isDontInline = static_cast<TR_J9VMBase *>(fe)->isDontInline(resolvedMethod);

   TR_PersistentJittedBodyInfo *bodyInfo = NULL;
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

   // set IP method data string.
   // fanin info is not used at cold opt level, so there is no point sending this information to the server
   JITClientIProfiler *iProfiler = (JITClientIProfiler *)((TR_J9VMBase *) fe)->getIProfiler();
   std::get<3>(methodInfo) = (comp && comp->getOptLevel() >= warm && iProfiler) ? iProfiler->serializeIProfilerMethodEntry(resolvedMethod->getPersistentIdentifier()) : std::string();
   }

void
TR_ResolvedJ9JITServerMethod::unpackMethodInfo(TR_OpaqueMethodBlock * aMethod, TR_FrontEnd * fe, TR_Memory * trMemory, uint32_t vTableSlot, TR::CompilationInfoPerThread *threadCompInfo, const TR_ResolvedJ9JITServerMethodInfo &methodInfo)
   {
   auto &methodInfoStruct = std::get<0>(methodInfo);
   
   
   _ramMethod = (J9Method *)aMethod;

   _remoteMirror = methodInfoStruct.remoteMirror;

   // Cache the constantPool and constantPoolHeader
   _literals = methodInfoStruct.literals;
   _ramClass = methodInfoStruct.ramClass;

   _romClass = threadCompInfo->getAndCacheRemoteROMClass(_ramClass);
   _romMethod = romMethodAtClassIndex(_romClass, methodInfoStruct.methodIndex);
   _romLiterals = (J9ROMConstantPoolItem *) ((UDATA) _romClass + sizeof(J9ROMClass));

   _vTableSlot = vTableSlot;
   _j9classForNewInstance = NULL;

   _jniProperties = methodInfoStruct.jniProperties;
   _jniTargetAddress = methodInfoStruct.jniTargetAddress;

   _isInterpreted = methodInfoStruct.isInterpreted;
   _isJNINative = methodInfoStruct.isJNINative;
   _isMethodInValidLibrary = methodInfoStruct.isMethodInValidLibrary;
   
   TR::RecognizedMethod mandatoryRm = methodInfoStruct.mandatoryRm;
   TR::RecognizedMethod rm = methodInfoStruct.rm;

   _startAddressForJittedMethod = methodInfoStruct.startAddressForJittedMethod;
   _virtualMethodIsOverridden = methodInfoStruct.virtualMethodIsOverridden;
   _addressContainingIsOverriddenBit = methodInfoStruct.addressContainingIsOverriddenBit;
   _classLoader = methodInfoStruct.classLoader;
   _isLambdaFormGeneratedMethod = methodInfoStruct.isLambdaFormGeneratedMethod;
   _isForceInline = methodInfoStruct.isForceInline;
   _isDontInline = methodInfoStruct.isDontInline;

   auto &bodyInfoStr = std::get<1>(methodInfo);
   auto &methodInfoStr = std::get<2>(methodInfo);

   _bodyInfo = J9::Recompilation::persistentJittedBodyInfoFromString(bodyInfoStr, methodInfoStr, trMemory);
 
   // initialization from TR_J9Method constructor
   _className = J9ROMCLASS_CLASSNAME(_romClass);
   _name = J9ROMMETHOD_NAME(_romMethod);
   _signature = J9ROMMETHOD_SIGNATURE(_romMethod);
   parseSignature(trMemory);
   _fullSignature = NULL;
  
   setMandatoryRecognizedMethod(mandatoryRm);
   setRecognizedMethod(rm);

   JITServerIProfiler *iProfiler = (JITServerIProfiler *) ((TR_J9VMBase *) fe)->getIProfiler();
   const std::string &entryStr = std::get<3>(methodInfo);
   const auto serialEntry = (TR_ContiguousIPMethodHashTableEntry*) &entryStr[0];
   _iProfilerMethodEntry = (iProfiler && !entryStr.empty()) ? iProfiler->deserializeMethodEntry(serialEntry, trMemory) : NULL; 
   }

bool
TR_ResolvedJ9JITServerMethod::addValidationRecordForCachedResolvedMethod(const TR_ResolvedMethodKey &key, TR_OpaqueMethodBlock *method)
   {
   // This method should be called whenever this resolved method is fetched from
   // cache during AOT compilation, because
   // if the cached resolved method was created while in heuristic region,
   // SVM record would not be added.
   auto svm = _fe->_compInfoPT->getCompilation()->getSymbolValidationManager();
   int32_t cpIndex = key.cpIndex;
   TR_OpaqueClassBlock *classObject = key.classObject;
   bool added = false;
   J9ConstantPool *cp = (J9ConstantPool *) static_cast<TR_ResolvedJ9Method *>(this)->cp();
   switch (key.type)
      {
      case VirtualFromCP:
         added = svm->addVirtualMethodFromCPRecord(method, cp, cpIndex);
         break;
      case VirtualFromOffset:
         added = svm->addVirtualMethodFromOffsetRecord(method, classObject, key.cpIndex, false);
         break;
      case Interface:
         added = svm->addInterfaceMethodFromCPRecord(
            method,
            (TR_OpaqueClassBlock *) ((TR_J9VM *) _fe)->getClassFromMethodBlock(getPersistentIdentifier()),
            classObject,
            cpIndex);
         break;
      case Static:
         added = svm->addStaticMethodFromCPRecord(method, cp, cpIndex);
         break;
      case Special:
         added = svm->addSpecialMethodFromCPRecord(method, cp, cpIndex);
         break;
      case ImproperInterface:
         added = svm->addImproperInterfaceMethodFromCPRecord(method, cp, cpIndex);
         break;
      default:
         TR_ASSERT(false, "Invalid TR_ResolvedMethodType value");
      }
   return added;
   }

void
TR_ResolvedJ9JITServerMethod::cacheResolvedMethodsCallees(int32_t ttlForUnresolved)
   {
   // 1. Iterate through bytecodes and look for method invokes.
   // If resolved method corresponding to an invoke is not cached, add it
   // to the list of methods that will be sent to the client in one batch.
   auto compInfoPT = (TR::CompilationInfoPerThreadRemote *) _fe->_compInfoPT;
   TR_J9ByteCodeIterator bci(0, this, fej9(), compInfoPT->getCompilation());
   std::vector<int32_t> cpIndices;
   std::vector<TR_ResolvedMethodType> methodTypes;
   for(TR_J9ByteCode bc = bci.first(); bc != J9BCunknown; bc = bci.next())
      {
      // Identify all bytecodes that require a resolved method
      int32_t cpIndex = bci.next2Bytes();
      TR_ResolvedMethodType type = TR_ResolvedMethodType::NoType;
      TR_ResolvedMethod *resolvedMethod;
      switch (bc)
         {
         case J9BCinvokevirtual:
            {
            type = TR_ResolvedMethodType::VirtualFromCP;
            break;
            }
         case J9BCinvokestaticsplit:
            {
            // falling through on purpose
            cpIndex |= J9_STATIC_SPLIT_TABLE_INDEX_FLAG;
            }
         case J9BCinvokestatic:
            {
            type = TR_ResolvedMethodType::Static;
            break;
            }
         case J9BCinvokespecialsplit:
            {
            // falling through on purpose
            cpIndex |= J9_SPECIAL_SPLIT_TABLE_INDEX_FLAG;
            }
         case J9BCinvokespecial:
            {
            type = TR_ResolvedMethodType::Special;
            break;
            }
         case J9BCinvokeinterface:
            {
            type = TR_ResolvedMethodType::ImproperInterface;
            break;
            }
         default:
            {
            // do nothing
            break;
            }
         }

      if (type != TR_ResolvedMethodType::NoType &&
          !compInfoPT->getCachedResolvedMethod(
             compInfoPT->getResolvedMethodKey(type, (TR_OpaqueClassBlock *) _ramClass, cpIndex),
             this,
             &resolvedMethod))
         {
         methodTypes.push_back(type);
         cpIndices.push_back(cpIndex);
         }
      }

   int32_t numMethods = methodTypes.size();
   // If less than 2 methods, it's cheaper to create
   // resolved method normally, because client won't
   // have to deal with vectors
   if (numMethods < 2)
      return;

   // 2. Send a remote query to mirror all uncached resolved methods
   _stream->write(JITServer::MessageType::ResolvedMethod_getMultipleResolvedMethods, (TR_ResolvedJ9Method *) _remoteMirror, methodTypes, cpIndices);
   auto recv = _stream->read<std::vector<TR_OpaqueMethodBlock *>, std::vector<uint32_t>, std::vector<TR_ResolvedJ9JITServerMethodInfo>>();

   // 3. Cache all received resolved methods
   auto &ramMethods = std::get<0>(recv);
   auto &vTableOffsets = std::get<1>(recv);
   auto &methodInfos = std::get<2>(recv);
   TR_ASSERT(numMethods == ramMethods.size(), "Number of received methods does not match the number of requested methods");
   for (int32_t i = 0; i < numMethods; ++i)
      {
      TR_ResolvedMethodType type = methodTypes[i];
      TR_ResolvedMethod *resolvedMethod;
      TR_ResolvedMethodKey key = compInfoPT->getResolvedMethodKey(type, (TR_OpaqueClassBlock *) _ramClass, cpIndices[i]);
      if (!compInfoPT->getCachedResolvedMethod(
             key,
             this,
             &resolvedMethod))
         {
         compInfoPT->cacheResolvedMethod(
            key,
            ramMethods[i],
            vTableOffsets[i],
            methodInfos[i],
            ttlForUnresolved
            );
         }
      }
   }

void
TR_ResolvedJ9JITServerMethod::cacheFields()
   {
   // 1. Iterate through bytecodes and look for loads/stores
   // If the corresponding field or static is not cached, add it
   // to the list of fields that will be sent to the client in one batch.
   auto serverVM = static_cast<TR_J9ServerVM *>(_fe);
   auto compInfoPT = _fe->_compInfoPT;
   TR_J9ByteCodeIterator bci(0, this, _fe, compInfoPT->getCompilation());
   std::vector<int32_t> cpIndices;
   std::vector<uint8_t> isStaticField;
   J9Class *ramClass = constantPoolHdr();
   for(TR_J9ByteCode bc = bci.first(); bc != J9BCunknown; bc = bci.next())
      {
      bool isField = false;
      bool isStatic;
      if (bc == J9BCgetfield || bc == J9BCputfield)
         {
         isField = true;
         isStatic = false;
         }
      else if (bc == J9BCgetstatic || bc == J9BCputstatic)
         {
         isField = true;
         isStatic = true;
         }

      J9Class *declaringClass;
      UDATA field;
      int32_t cpIndex = bci.next2Bytes();
      if (isField && !serverVM->getCachedField(ramClass, cpIndex, &declaringClass, &field))
         {
         cpIndices.push_back(cpIndex);
         isStaticField.push_back(isStatic);
         }
      }

   // If there's just one field, it's faster to get it through regular means,
   // to avoid overhead of vectors
   int32_t numFields = cpIndices.size();
   if (numFields < 2)
      return;

   // 2. Send a message to get info for all fields
   JITServer::ServerStream *stream = compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(
      JITServer::MessageType::VM_getFields,
      getRemoteMirror(),
      cpIndices,
      isStaticField);
   auto recv = stream->read<std::vector<J9Class *>, std::vector<UDATA>>();

   // 3. Cache all received fields
   auto &declaringClasses = std::get<0>(recv);
   auto &fields = std::get<1>(recv);
   TR_ASSERT(numFields == declaringClasses.size(), "Number of received fields does not match the requested number");
   OMR::CriticalSection getRemoteROMClass(compInfoPT->getClientData()->getROMMapMonitor());
   for (int32_t i = 0; i < numFields; ++i)
      {
      serverVM->cacheField(ramClass, cpIndices[i], declaringClasses[i], fields[i]);
      }
   }

int32_t
TR_ResolvedJ9JITServerMethod::collectImplementorsCapped(
   TR_OpaqueClassBlock *topClass,
   int32_t maxCount,
   int32_t cpIndexOrOffset,
   TR_YesNoMaybe useGetResolvedInterfaceMethod,
   TR_ResolvedMethod **implArray)
   {
   auto compInfoPT = static_cast<TR::CompilationInfoPerThreadRemote *>(_fe->_compInfoPT);
   JITServer::ServerStream *stream = compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(
      JITServer::MessageType::ResolvedMethod_getResolvedImplementorMethods,
      topClass,
      maxCount,
      cpIndexOrOffset,
      _remoteMirror,
      useGetResolvedInterfaceMethod);
   auto recv = stream->read<std::vector<TR_ResolvedJ9JITServerMethodInfo>, std::vector<J9Method *>, int32_t>();
   auto &methodInfos = std::get<0>(recv);
   auto &ramMethods = std::get<1>(recv);

   bool isInterface = TR::Compiler->cls.isInterfaceClass(compInfoPT->getCompilation(), topClass);

   // refine if requested by a caller
   if (useGetResolvedInterfaceMethod != TR_maybe)
      isInterface = useGetResolvedInterfaceMethod == TR_yes ? true : false;
   for (int32_t i = 0; i < methodInfos.size(); ++i)
      {
      TR_ResolvedMethodType type = isInterface ?
         TR_ResolvedMethodType::Interface :
         TR_ResolvedMethodType::VirtualFromOffset;

      TR_ResolvedMethod *resolvedMethod;
      TR_ResolvedMethodKey key =
         compInfoPT->getResolvedMethodKey(
            type,
            reinterpret_cast<TR_OpaqueClassBlock *>(_ramClass),
            cpIndexOrOffset,
            reinterpret_cast<TR_OpaqueClassBlock *>(std::get<0>(methodInfos[i]).ramClass));

      bool success;
      if (!(success = compInfoPT->getCachedResolvedMethod(
             key,
             this,
             &resolvedMethod)))
         {
         compInfoPT->cacheResolvedMethod(
            key,
            (TR_OpaqueMethodBlock *) ramMethods[i],
            cpIndexOrOffset,
            methodInfos[i],
            0); // all received methods should be resolved
         success = compInfoPT->getCachedResolvedMethod(key, this, &resolvedMethod);
         }

      TR_ASSERT_FATAL(success && resolvedMethod, "method must be cached and resolved");
      implArray[i] = resolvedMethod;
      }

   return std::get<2>(recv);
   }

bool
TR_ResolvedJ9JITServerMethod::validateMethodFieldAttributes(const TR_J9MethodFieldAttributes &attributes, bool isStatic, int32_t cpIndex, bool isStore, bool needAOTValidation)
   {
   // Validation should not be done against attributes cached per-resolved method,
   // because unresolved attribute might have become resolved during the current compilation
   // so validation would fail, but since it does not affect functional correctness,
   // we'll just keep the cached result until the end of the compilation.
   if (attributes.isUnresolvedInCP()) 
      return true;
   if (!isStatic)
      _stream->write(JITServer::MessageType::ResolvedMethod_fieldAttributes, _remoteMirror, cpIndex, isStore, needAOTValidation);
   else
      _stream->write(JITServer::MessageType::ResolvedMethod_staticAttributes, _remoteMirror, cpIndex, isStore, needAOTValidation);
   auto recv = _stream->read<TR_J9MethodFieldAttributes>();
   auto clientAttributes = std::get<0>(recv);
   bool equal = (attributes == clientAttributes);
   return equal;
   }

U_16
TR_ResolvedJ9JITServerMethod::archetypeArgPlaceholderSlot()
   {
   TR_ASSERT(isArchetypeSpecimen(), "should not be called for non-ArchetypeSpecimen methods");
   TR_OpaqueMethodBlock * aMethod = getNonPersistentIdentifier();
   J9ROMMethod * romMethod = JITServerHelpers::romMethodOfRamMethod((J9Method *)aMethod);
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

bool
TR_ResolvedJ9JITServerMethod::isFieldQType(int32_t cpIndex)
   {
   if (!TR::Compiler->om.areFlattenableValueTypesEnabled() ||
      (-1 == cpIndex))
      return false;

   auto comp = _fe->_compInfoPT->getCompilation();
   int32_t sigLen;
   char *sig = fieldOrStaticSignatureChars(cpIndex, sigLen);
   J9UTF8 *utfWrapper = str2utf8(sig, sigLen, comp->trMemory(), heapAlloc);

   J9VMThread *vmThread = comp->j9VMThread();
   return vmThread->javaVM->internalVMFunctions->isNameOrSignatureQtype(utfWrapper);
   }

bool
TR_ResolvedJ9JITServerMethod::isFieldFlattened(TR::Compilation *comp, int32_t cpIndex, bool isStatic)
   {
   if (!TR::Compiler->om.areFlattenableValueTypesEnabled() ||
      (-1 == cpIndex))
      return false;

   _stream->write(JITServer::MessageType::ResolvedMethod_isFieldFlattened, _remoteMirror, cpIndex, isStatic);
   return std::get<0>(_stream->read<bool>());
   }

TR_ResolvedRelocatableJ9JITServerMethod::TR_ResolvedRelocatableJ9JITServerMethod(TR_OpaqueMethodBlock * aMethod, TR_FrontEnd * fe, TR_Memory * trMemory, TR_ResolvedMethod * owner, uint32_t vTableSlot)
   : TR_ResolvedJ9JITServerMethod(aMethod, fe, trMemory, owner, vTableSlot)
   {
   // NOTE: avoid using this constructor as much as possible.
   // Using may result in multiple remote messages (up to 3?) sent to the client
   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;
   TR::Compilation *comp = fej9->_compInfoPT->getCompilation();
   if (comp && this->TR_ResolvedMethod::getRecognizedMethod() != TR::unknownMethod)
      {
      if (fej9->canRememberClass(containingClass()))
         {
         if (comp->getOption(TR_UseSymbolValidationManager))
            {
            TR::SymbolValidationManager *svm = comp->getSymbolValidationManager();
            SVM_ASSERT_ALREADY_VALIDATED(svm, aMethod);
            SVM_ASSERT_ALREADY_VALIDATED(svm, containingClass());
            }
         else
            {
            ((TR_ResolvedRelocatableJ9JITServerMethod *) owner)->validateArbitraryClass(comp, (J9Class*)containingClass());
            }
         }
      }
   }

TR_ResolvedRelocatableJ9JITServerMethod::TR_ResolvedRelocatableJ9JITServerMethod(TR_OpaqueMethodBlock * aMethod, TR_FrontEnd * fe, TR_Memory * trMemory, const TR_ResolvedJ9JITServerMethodInfo &methodInfo, TR_ResolvedMethod * owner, uint32_t vTableSlot)
   : TR_ResolvedJ9JITServerMethod(aMethod, fe, trMemory, methodInfo, owner, vTableSlot)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;
   TR::Compilation *comp = fej9->_compInfoPT->getCompilation();
   if (comp && this->TR_ResolvedMethod::getRecognizedMethod() != TR::unknownMethod)
      {
      // in the client-side constructor we check if containing class has been remembered,
      // and if it wasn't set recognized method to TR::unknownMethod.
      // However, this constructor is passed methodInfo, which means that client-side mirror
      // has already been created, attempted to remember the containing class and set recognized method
      // to appropriate value, then wrote it to methodInfo. So, we don't need that check on the server.
      if (comp->getOption(TR_UseSymbolValidationManager))
         {
         TR::SymbolValidationManager *svm = comp->getSymbolValidationManager();
         SVM_ASSERT_ALREADY_VALIDATED(svm, aMethod);
         SVM_ASSERT_ALREADY_VALIDATED(svm, containingClass());
         }
      else
         {
         // TODO: this will require more remote messages.
         // For simplicity, leave it like this for now, once testing can be done, optimize it
         ((TR_ResolvedRelocatableJ9JITServerMethod *) owner)->validateArbitraryClass(comp, (J9Class*)containingClass());
         }
      }
   }

void *
TR_ResolvedRelocatableJ9JITServerMethod::constantPool()
   {
   return romLiterals();
   }

bool
TR_ResolvedRelocatableJ9JITServerMethod::isInterpreted()
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
   TR::Compilation *comp = _fe->_compInfoPT->getCompilation();
   if (isJNINative() &&
        !comp->getOption(TR_DisableDirectToJNI)  &&
        !comp->getOption(TR_DisableDirectToJNIInline) &&
        !comp->getOption(TR_DisableDirectToJNI) &&
        !comp->getOption(TR_DisableDirectToJNIInline)
        )
      {
      alwaysTreatAsInterpreted = false;
      }
#endif

   if (alwaysTreatAsInterpreted)
      return true;

   return TR_ResolvedJ9JITServerMethod::isInterpreted();
   }

bool
TR_ResolvedRelocatableJ9JITServerMethod::isInterpretedForHeuristics()
   {
   return TR_ResolvedJ9JITServerMethod::isInterpreted();
   }

TR_OpaqueMethodBlock *
TR_ResolvedRelocatableJ9JITServerMethod::getNonPersistentIdentifier()
   {
   return (TR_OpaqueMethodBlock *)ramMethod();
   }

bool
TR_ResolvedRelocatableJ9JITServerMethod::validateArbitraryClass(TR::Compilation *comp, J9Class *clazz)
   {
   return storeValidationRecordIfNecessary(comp, cp(), 0, TR_ValidateArbitraryClass, ramMethod(), clazz);
   }

bool
TR_ResolvedRelocatableJ9JITServerMethod::storeValidationRecordIfNecessary(TR::Compilation * comp, J9ConstantPool *constantPool, int32_t cpIndex, TR_ExternalRelocationTargetKind reloKind, J9Method *ramMethod, J9Class *definingClass)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) comp->fe();

   bool storeClassInfo = true;
   bool fieldInfoCanBeUsed = false;
   TR_AOTStats *aotStats = ((TR_JitPrivateConfig *)fej9->_jitConfig->privateConfig)->aotStats;
   bool isStatic = (reloKind == TR_ValidateStaticField);

   if (comp->getDebug())
      {
      // guard this code with debug check, to avoid
      // sending extra messages when not tracing
      traceMsg(comp, "storeValidationRecordIfNecessary:\n");
      traceMsg(comp, "\tconstantPool %p cpIndex %d\n", constantPool, cpIndex);
      traceMsg(comp, "\treloKind %d isStatic %d\n", reloKind, isStatic);
      TR_J9ServerVM *serverVM = static_cast<TR_J9ServerVM *>(fej9);

      J9UTF8 *methodClassName =
         J9ROMCLASS_CLASSNAME(
            TR::Compiler->cls.romClassOf(
               serverVM->TR_J9ServerVM::getClassOfMethod(reinterpret_cast<TR_OpaqueMethodBlock *>(ramMethod))));
      traceMsg(comp,
               "\tmethod %p from class %p %.*s\n",
               ramMethod,
               serverVM->TR_J9ServerVM::getClassOfMethod(reinterpret_cast<TR_OpaqueMethodBlock *>(ramMethod)),
               J9UTF8_LENGTH(methodClassName),
               J9UTF8_DATA(methodClassName));
      traceMsg(comp, "\tdefiningClass %p\n", definingClass);
      }

   if (!definingClass)
      {
      definingClass = (J9Class *) TR_ResolvedJ9JITServerMethod::definingClassFromCPFieldRef(comp, cpIndex, isStatic);
      traceMsg(comp, "\tdefiningClass recomputed from cp as %p\n", definingClass);
      }

   if (!definingClass)
      {
      if (aotStats)
         aotStats->numDefiningClassNotFound++;
      return false;
      }

   if (comp->getDebug())
      {
      J9UTF8 *className = J9ROMCLASS_CLASSNAME(TR::Compiler->cls.romClassOf((TR_OpaqueClassBlock *) definingClass));
      traceMsg(comp, "\tdefiningClass name %.*s\n", J9UTF8_LENGTH(className), J9UTF8_DATA(className));
      }

   // all kinds of validations may need to rely on the entire class chain, so make sure we can build one first
   const AOTCacheClassChainRecord *classChainRecord = NULL;
   void *classChain = fej9->sharedCache()->rememberClass(definingClass, &classChainRecord);
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
               inLocalList = (TR::Compiler->cls.romClassOf((TR_OpaqueClassBlock *) definingClass) ==
                              TR::Compiler->cls.romClassOf((TR_OpaqueClassBlock *) ((*info)->_clazz)));
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

   TR::AOTClassInfo *classInfo = new (comp->trHeapMemory()) TR::AOTClassInfo(
      fej9, (TR_OpaqueClassBlock *)definingClass, (void *)classChain,
      (TR_OpaqueMethodBlock *)ramMethod, cpIndex, reloKind, classChainRecord
   );
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

U_8 *
TR_ResolvedRelocatableJ9JITServerMethod::allocateException(uint32_t numBytes, TR::Compilation *comp)
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

   return (U_8 *) eTbl;
   }

TR_ResolvedMethod *
TR_ResolvedRelocatableJ9JITServerMethod::createResolvedMethodFromJ9Method(TR::Compilation *comp, I_32 cpIndex, uint32_t vTableSlot, J9Method *j9method, bool * unresolvedInCP, TR_AOTInliningStats *aotStats) {
   // This method is called when a remote mirror hasn't been created yet, so it needs to be created from here.
   TR_ResolvedMethod *resolvedMethod = NULL;

#if defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
   static char *dontInline = feGetEnv("TR_AOTDontInline");

   if (dontInline)
      return NULL;

   // This message will create a remote mirror.
   // Calling constructor would be simpler, but we would have to make another message to update stats
   _stream->write(JITServer::MessageType::ResolvedRelocatableMethod_createResolvedRelocatableJ9Method, getRemoteMirror(), j9method, cpIndex, vTableSlot);
   auto recv = _stream->read<TR_ResolvedJ9JITServerMethodInfo, bool, bool, bool>();
   auto &methodInfo = std::get<0>(recv);
   // These parameters are only needed to update AOT stats. 
   // Maybe we shouldn't even track them, because another version of this method doesn't.
   bool isRomClassForMethodInSharedCache = std::get<1>(recv);
   bool sameClassLoaders = std::get<2>(recv);
   bool sameClass = std::get<3>(recv);

   if (std::get<0>(methodInfo).remoteMirror)
      {
      // Remote mirror created successfully
      resolvedMethod = new (comp->trHeapMemory()) TR_ResolvedRelocatableJ9JITServerMethod((TR_OpaqueMethodBlock *) j9method, _fe, comp->trMemory(), methodInfo, this, vTableSlot);
      if (aotStats)
         {
         aotStats->numMethodResolvedAtCompile++;
         if (sameClass)
            aotStats->numMethodInSameClass++;
         else
            aotStats->numMethodNotInSameClass++;
         }
      }
   else if (aotStats)
      {
      // Remote mirror not created, update AOT stats
      if (!isRomClassForMethodInSharedCache)
         aotStats->numMethodROMMethodNotInSC++;
      else if (!sameClassLoaders)
         aotStats->numMethodFromDiffClassLoader++;
      }

#endif
   if (resolvedMethod && ((TR_ResolvedJ9Method*)resolvedMethod)->isSignaturePolymorphicMethod())
      {
      // Signature polymorphic method's signature varies at different call sites and will be different than its declared signature
      int32_t signatureLength;
      char   *signature = getMethodSignatureFromConstantPool(cpIndex, signatureLength);
      ((TR_ResolvedJ9Method *)resolvedMethod)->setSignature(signature, signatureLength, comp->trMemory());
      }

   return resolvedMethod;
   }

TR_ResolvedMethod *
TR_ResolvedRelocatableJ9JITServerMethod::createResolvedMethodFromJ9Method( TR::Compilation *comp, int32_t cpIndex, uint32_t vTableSlot, J9Method *j9method, bool * unresolvedInCP, TR_AOTInliningStats *aotStats, const TR_ResolvedJ9JITServerMethodInfo &methodInfo)
   {
   // If this method is called, remote mirror has either already been created or creation failed.
   // In either case, methodInfo contains all parameters required to construct a resolved method or check that it's NULL.
   // The only problem is that we can't update AOT stats from this method.
   TR_ResolvedMethod *resolvedMethod = NULL;
   if (std::get<0>(methodInfo).remoteMirror)
      resolvedMethod = new (comp->trHeapMemory()) TR_ResolvedRelocatableJ9JITServerMethod((TR_OpaqueMethodBlock *) j9method, _fe, comp->trMemory(), methodInfo, this, vTableSlot);

   if (resolvedMethod && ((TR_ResolvedJ9Method*)resolvedMethod)->isSignaturePolymorphicMethod())
      {
      // Signature polymorphic method's signature varies at different call sites and will be different than its declared signature
      int32_t signatureLength;
      char   *signature = getMethodSignatureFromConstantPool(cpIndex, signatureLength);
      ((TR_ResolvedJ9Method *)resolvedMethod)->setSignature(signature, signatureLength, comp->trMemory());
      }

   return resolvedMethod;
   }

TR_ResolvedMethod *
TR_ResolvedRelocatableJ9JITServerMethod::getResolvedImproperInterfaceMethod(
   TR::Compilation * comp,
   I_32 cpIndex)
   {
   return aotMaskResolvedImproperInterfaceMethod(
      comp,
      TR_ResolvedJ9JITServerMethod::getResolvedImproperInterfaceMethod(comp, cpIndex));
   }

void *
TR_ResolvedRelocatableJ9JITServerMethod::startAddressForJittedMethod()
   {
   return NULL;
   }

void *
TR_ResolvedRelocatableJ9JITServerMethod::startAddressForJNIMethod(TR::Compilation * comp)
   {
#if defined(TR_TARGET_S390)  || defined(TR_TARGET_X86) || defined(TR_TARGET_POWER)
   return TR_ResolvedJ9JITServerMethod::startAddressForJNIMethod(comp);
#else
   return NULL;
#endif
   }

void *
TR_ResolvedRelocatableJ9JITServerMethod::startAddressForJITInternalNativeMethod()
   {
   return NULL;
   }

void *
TR_ResolvedRelocatableJ9JITServerMethod::startAddressForInterpreterOfJittedMethod()
   {
   return ((J9Method *)getNonPersistentIdentifier())->extra;
   }

TR_OpaqueClassBlock * 
TR_ResolvedRelocatableJ9JITServerMethod::definingClassFromCPFieldRef(TR::Compilation *comp, int32_t cpIndex, bool isStatic, TR_OpaqueClassBlock **fromResolvedJ9Method)
   {
   TR_OpaqueClassBlock *resolvedClass = TR_ResolvedJ9JITServerMethod::definingClassFromCPFieldRef(comp, cpIndex, isStatic);
   if (fromResolvedJ9Method != NULL)
      *fromResolvedJ9Method = resolvedClass;
   if (resolvedClass)
      {
      bool valid = false;
      if (comp->getOption(TR_UseSymbolValidationManager))
         valid = comp->getSymbolValidationManager()->addDefiningClassFromCPRecord(resolvedClass, cp(), cpIndex, isStatic);
      else
         valid = storeValidationRecordIfNecessary(comp, cp(), cpIndex, isStatic ? TR_ValidateStaticField : TR_ValidateInstanceField, ramMethod());

      if (!valid)
         resolvedClass = NULL;
      }
   return resolvedClass;
   }

TR_OpaqueClassBlock *
TR_ResolvedRelocatableJ9JITServerMethod::getClassFromConstantPool(TR::Compilation *comp, uint32_t cpIndex, bool returnClassForAOT)
   {
   if (returnClassForAOT || comp->getOption(TR_UseSymbolValidationManager))
      {
      TR_OpaqueClassBlock * resolvedClass = TR_ResolvedJ9JITServerMethod::getClassFromConstantPool(comp, cpIndex, returnClassForAOT);
      if (resolvedClass &&
         validateClassFromConstantPool(comp, (J9Class *)resolvedClass, cpIndex))
         {
         return (TR_OpaqueClassBlock*)resolvedClass;
         }
      }
   return NULL;
   }

bool
TR_ResolvedRelocatableJ9JITServerMethod::validateClassFromConstantPool(TR::Compilation *comp, J9Class *clazz, uint32_t cpIndex,  TR_ExternalRelocationTargetKind reloKind)
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
TR_ResolvedRelocatableJ9JITServerMethod::isUnresolvedString(I_32 cpIndex, bool optimizeForAOT)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");

   if (optimizeForAOT)
      return TR_ResolvedJ9JITServerMethod::isUnresolvedString(cpIndex);
   return true;
   }

void *
TR_ResolvedRelocatableJ9JITServerMethod::methodTypeConstant(I_32 cpIndex)
   {
   TR_ASSERT(false, "should be unreachable");
   return NULL;
   }

bool
TR_ResolvedRelocatableJ9JITServerMethod::isUnresolvedMethodType(I_32 cpIndex)
   {
   TR_ASSERT(false, "should be unreachable");
   return true;
   }

void *
TR_ResolvedRelocatableJ9JITServerMethod::methodHandleConstant(I_32 cpIndex)
   {
   TR_ASSERT(false, "should be unreachable");
   return NULL;
   }

bool
TR_ResolvedRelocatableJ9JITServerMethod::isUnresolvedMethodHandle(I_32 cpIndex)
   {
   TR_ASSERT(false, "should be unreachable");
   return true;
   }

TR_OpaqueClassBlock *
TR_ResolvedRelocatableJ9JITServerMethod::getDeclaringClassFromFieldOrStatic(TR::Compilation *comp, int32_t cpIndex)
   {
   TR_OpaqueClassBlock *definingClass = TR_ResolvedJ9JITServerMethod::getDeclaringClassFromFieldOrStatic(comp, cpIndex);
   if (comp->getOption(TR_UseSymbolValidationManager))
      {
      if (!comp->getSymbolValidationManager()->addDeclaringClassFromFieldOrStaticRecord(definingClass, cp(), cpIndex))
         return NULL;
      }
   return definingClass;
   }

TR_OpaqueClassBlock *
TR_ResolvedRelocatableJ9JITServerMethod::classOfStatic(int32_t cpIndex, bool returnClassForAOT)
   {
   TR_OpaqueClassBlock * clazz = TR_ResolvedJ9JITServerMethod::classOfStatic(cpIndex, returnClassForAOT);

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

TR_ResolvedMethod *
TR_ResolvedRelocatableJ9JITServerMethod::getResolvedPossiblyPrivateVirtualMethod(
   TR::Compilation *comp,
   int32_t cpIndex,
   bool ignoreRtResolve,
   bool * unresolvedInCP)
   {
   TR_ResolvedMethod *method =
      TR_ResolvedJ9JITServerMethod::getResolvedPossiblyPrivateVirtualMethod(
         comp,
         cpIndex,
         ignoreRtResolve,
         unresolvedInCP);

   return aotMaskResolvedPossiblyPrivateVirtualMethod(comp, method);
   }

bool
TR_ResolvedRelocatableJ9JITServerMethod::getUnresolvedFieldInCP(I_32 cpIndex)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
#if defined(J9VM_INTERP_AOT_COMPILE_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
   _stream->write(JITServer::MessageType::ResolvedMethod_getUnresolvedFieldInCP, getRemoteMirror(), cpIndex);
   return std::get<0>(_stream->read<bool>());
#else
   return true;
#endif
   }

bool
TR_ResolvedRelocatableJ9JITServerMethod::getUnresolvedStaticMethodInCP(int32_t cpIndex)
   {
   _stream->write(JITServer::MessageType::ResolvedMethod_getUnresolvedStaticMethodInCP, getRemoteMirror(), cpIndex);
   return std::get<0>(_stream->read<bool>());
   }

bool
TR_ResolvedRelocatableJ9JITServerMethod::getUnresolvedSpecialMethodInCP(I_32 cpIndex)
   {
   _stream->write(JITServer::MessageType::ResolvedMethod_getUnresolvedSpecialMethodInCP, getRemoteMirror(), cpIndex);
   return std::get<0>(_stream->read<bool>());
   }

void
TR_ResolvedRelocatableJ9JITServerMethod::handleUnresolvedStaticMethodInCP(int32_t cpIndex, bool * unresolvedInCP)
   {
   *unresolvedInCP = getUnresolvedStaticMethodInCP(cpIndex);
   }

void
TR_ResolvedRelocatableJ9JITServerMethod::handleUnresolvedSpecialMethodInCP(int32_t cpIndex, bool * unresolvedInCP)
   {
   *unresolvedInCP = getUnresolvedSpecialMethodInCP(cpIndex);
   }

void
TR_ResolvedRelocatableJ9JITServerMethod::handleUnresolvedVirtualMethodInCP(int32_t cpIndex, bool * unresolvedInCP)
   {
   *unresolvedInCP = getUnresolvedVirtualMethodInCP(cpIndex);
   }

bool
TR_ResolvedRelocatableJ9JITServerMethod::getUnresolvedVirtualMethodInCP(int32_t cpIndex)
   {
   return false;
   }

UDATA
TR_ResolvedRelocatableJ9JITServerMethod::getFieldType(J9ROMConstantPoolItem * CP, int32_t cpIndex)
   {
   _stream->write(JITServer::MessageType::ResolvedRelocatableMethod_getFieldType, cpIndex, getRemoteMirror());
   auto recv = _stream->read<UDATA>();
   return (std::get<0>(recv));
   }

bool
TR_ResolvedRelocatableJ9JITServerMethod::fieldAttributes(TR::Compilation * comp, int32_t cpIndex, uint32_t * fieldOffset, TR::DataType * type, bool * volatileP, bool * isFinal, bool * isPrivate, bool isStore, bool * unresolvedInCP, bool needAOTValidation)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
   
   // Will search per-resolved method cache and make a remote call if needed, client will call AOT version of the method.
   // We still need to create a validation record, and if it can't be added, change
   // return values appropriately.
   bool isStatic = false;
   J9ConstantPool *constantPool = (J9ConstantPool *)literals();
   bool theFieldIsFromLocalClass = false;
   TR_OpaqueClassBlock *definingClass = NULL;
   TR_J9MethodFieldAttributes attributes;
   if (!getCachedFieldAttributes(cpIndex, attributes, isStatic))
      {
      _stream->write(JITServer::MessageType::ResolvedRelocatableMethod_fieldAttributes, getRemoteMirror(), cpIndex, isStore, needAOTValidation);
      auto recv = _stream->read<TR_J9MethodFieldAttributes>();
      attributes = std::get<0>(recv);
      
      cacheFieldAttributes(cpIndex, attributes, isStatic);
      }
   else
      {
      TR_ASSERT(validateMethodFieldAttributes(attributes, isStatic, cpIndex, isStore, constantPool), "static attributes from client and cache do not match");
      }
   attributes.setMethodFieldAttributesResult(fieldOffset, type, volatileP, isFinal, isPrivate, unresolvedInCP, &theFieldIsFromLocalClass, &definingClass);

   bool fieldInfoCanBeUsed = false;
   bool resolveField = true;

   if (comp->getOption(TR_DisableAOTInstanceFieldResolution))
      {
      resolveField = false;
      }
   else
      {
      if (needAOTValidation)
         {
         if (comp->getOption(TR_UseSymbolValidationManager))
            {
            fieldInfoCanBeUsed = comp->getSymbolValidationManager()->addDefiningClassFromCPRecord(reinterpret_cast<TR_OpaqueClassBlock *> (definingClass), constantPool, cpIndex);
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

   if (!resolveField)
      {
      *fieldOffset = (U_32)NULL;
      fieldInfoCanBeUsed = false;
      }

   if (!fieldInfoCanBeUsed)
      {
      theFieldIsFromLocalClass = false;
      // Either couldn't create validation record 
      // or AOT instance field resolution is disabled.
      if (volatileP) *volatileP = true;
      if (isFinal) *isFinal = false;
      if (isPrivate) *isPrivate = false;
      if (fieldOffset) *(U_32*)fieldOffset = TR::Compiler->om.objectHeaderSizeInBytes();
      }

   return theFieldIsFromLocalClass;
   }

bool
TR_ResolvedRelocatableJ9JITServerMethod::staticAttributes(TR::Compilation * comp,
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

   // Will search per-resolved method cache and make a remote call if needed, client will call AOT version of the method.
   // We still need to create a validation record, and if it can't be added, change
   // return values appropriately.
   bool isStatic = true;
   J9ConstantPool *constantPool = (J9ConstantPool *)literals();
   bool theFieldIsFromLocalClass = false;
   TR_OpaqueClassBlock *definingClass = NULL;
   TR_J9MethodFieldAttributes attributes;
   if(!getCachedFieldAttributes(cpIndex, attributes, isStatic))
      {
      _stream->write(JITServer::MessageType::ResolvedRelocatableMethod_staticAttributes, getRemoteMirror(), cpIndex, isStore, needAOTValidation);
      auto recv = _stream->read<TR_J9MethodFieldAttributes>();
      attributes = std::get<0>(recv);
      
      cacheFieldAttributes(cpIndex, attributes, isStatic);
      }
   else
      {
      TR_ASSERT(validateMethodFieldAttributes(attributes, isStatic, cpIndex, isStore, constantPool), "static attributes from client and cache do not match");
      }
   attributes.setMethodFieldAttributesResult(address, type, volatileP, isFinal, isPrivate, unresolvedInCP, &theFieldIsFromLocalClass, &definingClass);

   bool fieldInfoCanBeUsed = false;
   bool resolveField = true;
   if (comp->getOption(TR_DisableAOTInstanceFieldResolution))
      {
      resolveField = false;
      }
   else
      {
      if (needAOTValidation)
         {
         if (comp->getOption(TR_UseSymbolValidationManager))
            {
            fieldInfoCanBeUsed = comp->getSymbolValidationManager()->addDefiningClassFromCPRecord(reinterpret_cast<TR_OpaqueClassBlock *> (definingClass), constantPool, cpIndex, true);
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

   if (!resolveField)
      {
      *address = (U_32)NULL;
      fieldInfoCanBeUsed = false;
      }

   if (!fieldInfoCanBeUsed)
      {
      theFieldIsFromLocalClass = false;
      // Either couldn't create validation record 
      // or AOT instance field resolution is disabled.
      if (volatileP) *volatileP = true;
      if (isFinal) *isFinal = false;
      if (isPrivate) *isPrivate = false;
      if (address) *address = NULL;
      }

   return theFieldIsFromLocalClass;
   }

TR_FieldAttributesCache &
TR_ResolvedRelocatableJ9JITServerMethod::getAttributesCache(bool isStatic, bool unresolvedInCP)
   {
   // Return persistent attributes cache for AOT compilations
   TR::CompilationInfoPerThread *compInfoPT = _fe->_compInfoPT;
   auto &attributesCache = isStatic ? 
      getJ9ClassInfo(compInfoPT, _ramClass)._staticAttributesCacheAOT :
      getJ9ClassInfo(compInfoPT, _ramClass)._fieldAttributesCacheAOT;
   return attributesCache;
   }

bool
TR_ResolvedRelocatableJ9JITServerMethod::validateMethodFieldAttributes(const TR_J9MethodFieldAttributes &attributes, bool isStatic, int32_t cpIndex, bool isStore, bool needAOTValidation)
   {
   // Validation should not be done against attributes cached per-resolved method,
   // because unresolved attribute might have become resolved during the current compilation
   // so validation would fail, but since it does not affect functional correctness,
   // we'll just keep the cached result until the end of the compilation.
   if (attributes.isUnresolvedInCP()) 
      return true;
   if (!isStatic)
      _stream->write(JITServer::MessageType::ResolvedRelocatableMethod_fieldAttributes, getRemoteMirror(), cpIndex, isStore, needAOTValidation);
   else
      _stream->write(JITServer::MessageType::ResolvedRelocatableMethod_staticAttributes, getRemoteMirror(), cpIndex, isStore, needAOTValidation);
   auto recv = _stream->read<TR_J9MethodFieldAttributes>();
   auto clientAttributes = std::get<0>(recv);
   bool equal = (attributes == clientAttributes);
   return equal;
   }

TR_J9ServerMethod::TR_J9ServerMethod(TR_FrontEnd * fe, TR_Memory * trMemory, J9Class * aClazz, uintptr_t cpIndex)
   : TR_J9Method()
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
   TR_J9ServerVM *fej9 = (TR_J9ServerVM *) fe;
   TR::CompilationInfoPerThread *compInfoPT = fej9->_compInfoPT;
   std::string classNameStr;
   std::string methodNameStr;
   std::string methodSignatureStr;
   bool cached = false;
      {
      // look up parameters for construction of this method in a cache first
      OMR::CriticalSection getRemoteROMClass(compInfoPT->getClientData()->getROMMapMonitor());
      auto &cache = getJ9ClassInfo(compInfoPT, aClazz)._J9MethodNameCache;
      // search the cache for existing method parameters
      auto it = cache.find(cpIndex);
      if (it != cache.end())
         {
         const J9MethodNameAndSignature &params = it->second;
         classNameStr = params._classNameStr;
         methodNameStr = params._methodNameStr;
         methodSignatureStr = params._methodSignatureStr;
         cached = true;
         }
      }

   if (!cached)
      {
      // make a remote call and cache the result
      JITServer::ServerStream *stream = fej9->_compInfoPT->getMethodBeingCompiled()->_stream;
      stream->write(JITServer::MessageType::get_params_to_construct_TR_j9method, aClazz, cpIndex);
      const auto recv = stream->read<std::string, std::string, std::string>();
      classNameStr = std::get<0>(recv);
      methodNameStr = std::get<1>(recv);
      methodSignatureStr = std::get<2>(recv);

      OMR::CriticalSection getRemoteROMClass(compInfoPT->getClientData()->getROMMapMonitor());
      auto &cache = getJ9ClassInfo(compInfoPT, aClazz)._J9MethodNameCache;
      cache.insert({cpIndex, {classNameStr, methodNameStr, methodSignatureStr}});
      }

   _className = str2utf8(classNameStr.data(), classNameStr.length(), trMemory, heapAlloc);
   _name = str2utf8(methodNameStr.data(), methodNameStr.length(), trMemory, heapAlloc);
   _signature = str2utf8(methodSignatureStr.data(), methodSignatureStr.length(), trMemory, heapAlloc);

   parseSignature(trMemory);
   _fullSignature = NULL;
   }
