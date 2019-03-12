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
#include "oti/jitprotos.h"
#include "rpc/J9Server.hpp"
#include "VMJ9Server.hpp"
#include "j9methodServer.hpp"
#include "control/CompilationRuntime.hpp"
#include "control/CompilationThread.hpp"
#include "control/MethodToBeCompiled.hpp"
#include "runtime/CodeCache.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/CodeCacheExceptions.hpp"
#include "control/JITaaSCompilationThread.hpp"
#include "env/PersistentCHTable.hpp"
#include "exceptions/AOTFailure.hpp"

void
TR_J9ServerVM::getResolvedMethodsAndMethods(TR_Memory *trMemory, TR_OpaqueClassBlock *classPointer, List<TR_ResolvedMethod> *resolvedMethodsInClass, J9Method **methods, uint32_t *numMethods)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getResolvedMethodsAndMirror, classPointer);
   auto recv = stream->read<J9Method *, std::vector<TR_ResolvedJ9JITaaSServerMethodInfo>>();
   auto methodsInClass = std::get<0>(recv);
   auto &methodsInfo = std::get<1>(recv);
   if (methods)
      *methods = methodsInClass;
   if (numMethods)
      *numMethods = methodsInfo.size();
   for (int i = 0; i < methodsInfo.size(); ++i)
      {
      // create resolved methods, using information from mirrors
      auto resolvedMethod = new (trMemory->trHeapMemory()) TR_ResolvedJ9JITaaSServerMethod((TR_OpaqueMethodBlock *) &(methodsInClass[i]), this, trMemory, methodsInfo[i], 0);
      resolvedMethodsInClass->add(resolvedMethod);
      }
   }

bool
TR_J9ServerVM::isClassLibraryMethod(TR_OpaqueMethodBlock *method, bool vettedForAOT)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_isClassLibraryMethod, method, vettedForAOT);
   return std::get<0>(stream->read<bool>());
   }

bool
TR_J9ServerVM::isClassLibraryClass(TR_OpaqueClassBlock *clazz)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_isClassLibraryClass, clazz);
   return std::get<0>(stream->read<bool>());
   }

TR_OpaqueClassBlock *
TR_J9ServerVM::getSuperClass(TR_OpaqueClassBlock *clazz)
   {
   TR_OpaqueClassBlock *parentClass = nullptr;

   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   JITaaSHelpers::getAndCacheRAMClassInfo((J9Class *)clazz, _compInfoPT->getClientData(), stream, JITaaSHelpers::CLASSINFO_PARENT_CLASS, (void *)&parentClass);
   return parentClass;
   }

TR_Method *
TR_J9ServerVM::createMethod(TR_Memory * trMemory, TR_OpaqueClassBlock * clazz, int32_t refOffset)
   {
   return new (trMemory->trHeapMemory()) TR_J9Method(this, trMemory, TR::Compiler->cls.convertClassOffsetToClassPtr(clazz), refOffset, true);
   }

TR_ResolvedMethod *
TR_J9ServerVM::createResolvedMethod(TR_Memory * trMemory, TR_OpaqueMethodBlock * aMethod,
                                  TR_ResolvedMethod * owningMethod, TR_OpaqueClassBlock *classForNewInstance)
   {
   return createResolvedMethodWithSignature(trMemory, aMethod, classForNewInstance, nullptr, -1, owningMethod);
   }

TR_ResolvedMethod *
TR_J9ServerVM::createResolvedMethodWithSignature(TR_Memory * trMemory, TR_OpaqueMethodBlock * aMethod, TR_OpaqueClassBlock *classForNewInstance,
                          char *signature, int32_t signatureLength, TR_ResolvedMethod * owningMethod)
   {
   TR_ResolvedJ9Method *result = NULL;
   if (isAOT_DEPRECATED_DO_NOT_USE())
      {
#if defined(J9VM_INTERP_AOT_COMPILE_SUPPORT)
      result = new (trMemory->trHeapMemory()) TR_ResolvedRelocatableJ9JITaaSServerMethod(aMethod, this, trMemory, owningMethod);
      TR::Compilation *comp = _compInfoPT->getCompilation();
      if (comp && comp->getOption(TR_UseSymbolValidationManager))
         {
         TR::SymbolValidationManager *svm = comp->getSymbolValidationManager();
         if (!svm->isAlreadyValidated(result->containingClass()))
            return NULL;
         }
#endif
      }
   else
      {
      result = new (trMemory->trHeapMemory()) TR_ResolvedJ9JITaaSServerMethod(aMethod, this, trMemory, owningMethod);
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

TR_YesNoMaybe
TR_J9ServerVM::isInstanceOf(TR_OpaqueClassBlock *a, TR_OpaqueClassBlock *b, bool objectTypeIsFixed, bool castTypeIsFixed, bool optimizeForAOT)
   { 
   // use instanceOfOrCheckCast to get the result, because
   // their logic is mostly the same, remote call might be made from there
   J9Class * objectClass   = (J9Class *) a;
   J9Class * castTypeClass = (J9Class *) b;
   bool objectClassIsInstanceOfCastTypeClass = instanceOfOrCheckCast(objectClass, castTypeClass);

   if (objectClassIsInstanceOfCastTypeClass)
      return castTypeIsFixed ? TR_yes : TR_maybe;
   else if (objectTypeIsFixed && !objectClassIsInstanceOfCastTypeClass)
      return TR_no;
   else if (!isInterfaceClass(b) && !isInterfaceClass(a) &&
         !objectClassIsInstanceOfCastTypeClass &&
         !instanceOfOrCheckCast(castTypeClass, objectClass))
      return TR_no;
   return TR_maybe;
   }
/*
bool
TR_J9ServerVM::isInterfaceClass(TR_OpaqueClassBlock *clazzPointer)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_isInterfaceClass, clazzPointer);
   return std::get<0>(stream->read<bool>());
   }

bool
TR_J9ServerVM::isClassFinal(TR_OpaqueClassBlock *clazzPointer)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_isClassFinal, clazzPointer);
   return std::get<0>(stream->read<bool>());
   }

bool
TR_J9ServerVM::isAbstractClass(TR_OpaqueClassBlock *clazzPointer)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_isAbstractClass, clazzPointer);
   return std::get<0>(stream->read<bool>());
   }
*/
TR_OpaqueClassBlock *
TR_J9ServerVM::getSystemClassFromClassName(const char * name, int32_t length, bool isVettedForAOT)
   {
   // check the cache first
   std::string className(name, length);
   PersistentUnorderedMap<std::string, TR_OpaqueClassBlock*> & systemClassByNameMap = _compInfoPT->getClientData()->getSystemClassByNameMap();
   {
   OMR::CriticalSection getSystemClassCS(_compInfoPT->getClientData()->getSystemClassMapMonitor());
   auto it = systemClassByNameMap.find(className);
   if (it != systemClassByNameMap.end())
      return it->second;
   }
   // classname not found; ask the client and cache the answer
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getSystemClassFromClassName, className, isVettedForAOT);
   TR_OpaqueClassBlock * clazz = std::get<0>(stream->read<TR_OpaqueClassBlock *>());
   if (clazz)
      {
      OMR::CriticalSection getSystemClassCS(_compInfoPT->getClientData()->getSystemClassMapMonitor());
      systemClassByNameMap.insert(std::make_pair(className, clazz));
      }
   else
      {
      // Class with given name does not exist yet, but it could be
      // loaded in the future, thus we should not cache NULL pointers.
      }
   return clazz;
   }

bool
TR_J9ServerVM::isMethodTracingEnabled(TR_OpaqueMethodBlock *method)
   {
      {
      OMR::CriticalSection getRemoteROMClass(_compInfoPT->getClientData()->getROMMapMonitor());
      auto it = _compInfoPT->getClientData()->getJ9MethodMap().find((J9Method*) method);
      if (it != _compInfoPT->getClientData()->getJ9MethodMap().end())
         {
         return it->second._isMethodTracingEnabled;
         }
      }

   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_isMethodTracingEnabled, method);
   return std::get<0>(stream->read<bool>());
   }

bool
TR_J9ServerVM::canMethodEnterEventBeHooked()
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   auto *vmInfo = _compInfoPT->getClientData()->getOrCacheVMInfo(stream);
   return vmInfo->_canMethodEnterEventBeHooked;
   }

bool
TR_J9ServerVM::canMethodExitEventBeHooked()
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   auto *vmInfo = _compInfoPT->getClientData()->getOrCacheVMInfo(stream);
   return vmInfo->_canMethodExitEventBeHooked;
   }

TR_OpaqueClassBlock *
TR_J9ServerVM::getClassClassPointer(TR_OpaqueClassBlock *objectClassPointer)
   {
   TR_OpaqueClassBlock *javaLangClass = _compInfoPT->getClientData()->getJavaLangClassPtr();
   if (!javaLangClass)
      {
      // If value is not cached, fetch it from client and cache it
      JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
      stream->write(JITaaS::J9ServerMessageType::VM_getClassClassPointer, objectClassPointer);
      javaLangClass = std::get<0>(stream->read<TR_OpaqueClassBlock *>());
      _compInfoPT->getClientData()->setJavaLangClassPtr(javaLangClass); // cache value for later
      }
   return javaLangClass;
   }

void *
TR_J9ServerVM::getClassLoader(TR_OpaqueClassBlock * classPointer)
   {
   TR_ASSERT(false, "The server vm should not call getClassLoader.");
   return nullptr;
   }

TR_OpaqueClassBlock *
TR_J9ServerVM::getClassOfMethod(TR_OpaqueMethodBlock *method)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getClassOfMethod, method);
   return std::get<0>(stream->read<TR_OpaqueClassBlock *>());
   }

TR_OpaqueClassBlock *
TR_J9ServerVM::getBaseComponentClass(TR_OpaqueClassBlock * clazz, int32_t & numDims)
   {
   TR_OpaqueClassBlock *baseComponentClass = nullptr;
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   JITaaSHelpers::getAndCacheRAMClassInfo((J9Class *)clazz, _compInfoPT->getClientData(), stream, JITaaSHelpers::CLASSINFO_NUMBER_DIMENSIONS, &numDims, JITaaSHelpers::CLASSINFO_BASE_COMPONENT_CLASS, (void *)&baseComponentClass);

  return baseComponentClass;
   }

TR_OpaqueClassBlock *
TR_J9ServerVM::getLeafComponentClassFromArrayClass(TR_OpaqueClassBlock * arrayClass)
   {
   TR_OpaqueClassBlock *leafComponentClass = nullptr;
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   JITaaSHelpers::getAndCacheRAMClassInfo((J9Class *)arrayClass, _compInfoPT->getClientData(), stream, JITaaSHelpers::CLASSINFO_LEAF_COMPONENT_CLASS, (void *)&leafComponentClass);
   return leafComponentClass;
   }

TR_OpaqueClassBlock *
TR_J9ServerVM::getClassFromSignature(const char *sig, int32_t length, TR_ResolvedMethod *method, bool isVettedForAOT)
   {
   TR_OpaqueClassBlock * clazz = nullptr;
   J9ClassLoader * cl = ((TR_ResolvedJ9Method *)method)->getClassLoader();
   // If the classloader is the systemClassLoader, which cannot be unloaded,
   // then look into the global, 'per client', cache
   void * systemClassLoader = getSystemClassLoader();
   if (cl == (J9ClassLoader*)systemClassLoader)
      {
      PersistentUnorderedMap<std::string, TR_OpaqueClassBlock*> & systemClassByNameMap = _compInfoPT->getClientData()->getSystemClassByNameMap();
      {
      OMR::CriticalSection classFromSigCS(_compInfoPT->getClientData()->getSystemClassMapMonitor());
      auto it = systemClassByNameMap.find(std::string(sig, length));
      if (it != systemClassByNameMap.end())
         return it->second;
      }
      // classname not found; ask the client and cache the answer  
      clazz = getClassFromSignature(sig, length, (TR_OpaqueMethodBlock *)method->getPersistentIdentifier(), isVettedForAOT);
      if (clazz)
         {
         OMR::CriticalSection classFromSigCS(_compInfoPT->getClientData()->getSystemClassMapMonitor());
         systemClassByNameMap.insert(std::make_pair(std::string(sig, length), clazz));
         }
      else
         {
         // Class with given name does not exist yet, but it could be
         // loaded in the future, thus we should not cache NULL pointers.
         // Note: many times we get in here due to Ljava/lang/String$StringCompressionFlag;
         // In theory we could consider this a special case and watch the CHTable updates
         // for a class load event for Ljava/lang/String$StringCompressionFlag, but it may not
         // be worth the trouble.
         //static unsigned long errorsSystem = 0;
         //printf("ErrorSystem %lu for cl=%p\tclassName=%.*s\n", ++errorsSystem, cl, length, sig);
         }
      }
   else // look into the 'per compilation' cache
      {
      ClassLoaderStringPair key = {cl, std::string(sig, length)};
      auto it = _compInfoPT->getCustomClassByNameMap().find(key);

      // If not found in the cache, ask the client
      if (it == _compInfoPT->getCustomClassByNameMap().end())
         {
         clazz = getClassFromSignature(sig, length, (TR_OpaqueMethodBlock *)method->getPersistentIdentifier(), isVettedForAOT);
         if (clazz)
            {
            // Put into the HT for next time
            _compInfoPT->getCustomClassByNameMap()[key] = clazz;
            }
         }
      else
         {
         clazz = it->second;
         }
      }
   return clazz;
   }


TR_OpaqueClassBlock *
TR_J9ServerVM::getClassFromSignature(const char *sig, int32_t length, TR_OpaqueMethodBlock *method, bool isVettedForAOT)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   std::string str(sig, length);
   stream->write(JITaaS::J9ServerMessageType::VM_getClassFromSignature, str, method, isVettedForAOT);
   return std::get<0>(stream->read<TR_OpaqueClassBlock *>());
   }

void *
TR_J9ServerVM::getSystemClassLoader()
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   auto *vmInfo = _compInfoPT->getClientData()->getOrCacheVMInfo(stream);
   return vmInfo->_systemClassLoader;
   }

bool
TR_J9ServerVM::jitFieldsAreSame(TR_ResolvedMethod * method1, I_32 cpIndex1, TR_ResolvedMethod * method2, I_32 cpIndex2, int32_t isStatic)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;

   // Pass pointers to client mirrors of the ResolvedMethod objects instead of local objects
   TR_ResolvedJ9JITaaSServerMethod *serverMethod1 = static_cast<TR_ResolvedJ9JITaaSServerMethod*>(method1);
   TR_ResolvedJ9JITaaSServerMethod *serverMethod2 = static_cast<TR_ResolvedJ9JITaaSServerMethod*>(method2);
   TR_ResolvedMethod *clientMethod1 = serverMethod1->getRemoteMirror();
   TR_ResolvedMethod *clientMethod2 = serverMethod2->getRemoteMirror();

   bool result = false;

   bool sigSame = true;
   if (serverMethod1->fieldsAreSame(cpIndex1, serverMethod2, cpIndex2, sigSame))
      result = true;
   else
      {
      if (sigSame)
         {
         // if name and signature comparison is inconclusive, make a remote call 
         stream->write(JITaaS::J9ServerMessageType::VM_jitFieldsAreSame, clientMethod1, cpIndex1, clientMethod2, cpIndex2, isStatic);
         result = std::get<0>(stream->read<bool>());
         }
      }
   return result;
   }

bool
TR_J9ServerVM::jitStaticsAreSame(TR_ResolvedMethod *method1, I_32 cpIndex1, TR_ResolvedMethod *method2, I_32 cpIndex2)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;

   // Pass pointers to client mirrors of the ResolvedMethod objects instead of local objects
   TR_ResolvedJ9JITaaSServerMethod *serverMethod1 = static_cast<TR_ResolvedJ9JITaaSServerMethod*>(method1);
   TR_ResolvedJ9JITaaSServerMethod *serverMethod2 = static_cast<TR_ResolvedJ9JITaaSServerMethod*>(method2);
   TR_ResolvedMethod *clientMethod1 = serverMethod1->getRemoteMirror();
   TR_ResolvedMethod *clientMethod2 = serverMethod2->getRemoteMirror();
   
   bool result = false;

   bool sigSame = true;
   if (serverMethod1->staticsAreSame(cpIndex1, serverMethod2, cpIndex2, sigSame))
      result = true;
   else
      {
      if (sigSame)
         {
         // if name and signature comparison is inconclusive, make a remote call 
         stream->write(JITaaS::J9ServerMessageType::VM_jitStaticsAreSame, clientMethod1, cpIndex1, clientMethod2, cpIndex2);
         result = std::get<0>(stream->read<bool>());
         }
      }
   return result;
   }

TR_OpaqueClassBlock *
TR_J9ServerVM::getComponentClassFromArrayClass(TR_OpaqueClassBlock *arrayClass)
   {
   TR_OpaqueClassBlock *componentClass = nullptr;
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   JITaaSHelpers::getAndCacheRAMClassInfo((J9Class *)arrayClass, _compInfoPT->getClientData(), stream, JITaaSHelpers::CLASSINFO_COMPONENT_CLASS, (void *)&componentClass);
   return componentClass;
   }

bool
TR_J9ServerVM::classHasBeenReplaced(TR_OpaqueClassBlock *clazz)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_classHasBeenReplaced, clazz);
   return std::get<0>(stream->read<bool>());
   }

bool
TR_J9ServerVM::classHasBeenExtended(TR_OpaqueClassBlock *clazz)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_classHasBeenExtended, clazz);
   return std::get<0>(stream->read<bool>());
   }

bool
TR_J9ServerVM::compiledAsDLTBefore(TR_ResolvedMethod *method)
   {
#if defined(J9VM_JIT_DYNAMIC_LOOP_TRANSFER)
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   auto mirror = static_cast<TR_ResolvedJ9JITaaSServerMethod *>(method)->getRemoteMirror();
   stream->write(JITaaS::J9ServerMessageType::VM_compiledAsDLTBefore, static_cast<TR_ResolvedMethod *>(mirror));
   return std::get<0>(stream->read<bool>());
#else
   return 0;
#endif
   }


/*
char *
TR_J9ServerVM::getClassNameChars(TR_OpaqueClassBlock * ramClass, int32_t & length)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getClassNameChars, ramClass);
   const std::string className = std::get<0>(stream->read<std::string>());
   char * classNameChars = (char*) _compInfoPT->getCompilation()->trMemory()->allocateMemory(className.length(), heapAlloc);
   memcpy(classNameChars, &className[0], className.length());
   length = className.length();
   return classNameChars;
   }
*/
uintptrj_t
TR_J9ServerVM::getOverflowSafeAllocSize()
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   auto *vmInfo = _compInfoPT->getClientData()->getOrCacheVMInfo(stream);
   return static_cast<uintptrj_t>(vmInfo->_overflowSafeAllocSize);
   }

bool
TR_J9ServerVM::isThunkArchetype(J9Method * method)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_isThunkArchetype, method);
   return std::get<0>(stream->read<bool>());
   }

static J9UTF8 *str2utf8(char *string, int32_t length, TR_Memory *trMemory, TR_AllocationKind allocKind)
   {
   J9UTF8 *utf8 = (J9UTF8 *) trMemory->allocateMemory(length, allocKind);
   J9UTF8_SET_LENGTH(utf8, length);
   memcpy(J9UTF8_DATA(utf8), string, length);
   return utf8;
   }

int32_t
TR_J9ServerVM::printTruncatedSignature(char *sigBuf, int32_t bufLen, TR_OpaqueMethodBlock *method)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_printTruncatedSignature, method);
   auto recv = stream->read<std::string, std::string, std::string>();
   const std::string classNameStr = std::get<0>(recv);
   const std::string nameStr = std::get<1>(recv);
   const std::string signatureStr = std::get<2>(recv);
   TR_Memory *trMemory = _compInfoPT->getCompilation()->trMemory();
   J9UTF8 * className = str2utf8((char*)&classNameStr[0], classNameStr.length(), trMemory, heapAlloc);
   J9UTF8 * name = str2utf8((char*)&nameStr[0], nameStr.length(), trMemory, heapAlloc);
   J9UTF8 * signature = str2utf8((char*)&signatureStr[0], signatureStr.length(), trMemory, heapAlloc);
   return TR_J9VMBase::printTruncatedSignature(sigBuf, bufLen, className, name, signature);
   }
/*
bool
TR_J9ServerVM::isPrimitiveClass(TR_OpaqueClassBlock * clazz)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_isPrimitiveClass, clazz);
   return std::get<0>(stream->read<bool>());
   }
*/
bool
TR_J9ServerVM::isClassInitialized(TR_OpaqueClassBlock * clazz)
   {
   bool isClassInitialized = false;
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   JITaaSHelpers::getAndCacheRAMClassInfo((J9Class *)clazz, _compInfoPT->getClientData(), stream, JITaaSHelpers::CLASSINFO_CLASS_INITIALIZED, (void *)&isClassInitialized);

   if (!isClassInitialized)
      {
      JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
      stream->write(JITaaS::J9ServerMessageType::VM_isClassInitialized, clazz);
      isClassInitialized = std::get<0>(stream->read<bool>());
      if (isClassInitialized)
         {
         OMR::CriticalSection getRemoteROMClass(_compInfoPT->getClientData()->getROMMapMonitor());
         auto it = _compInfoPT->getClientData()->getROMClassMap().find((J9Class*) clazz);
         if (it != _compInfoPT->getClientData()->getROMClassMap().end())
            {
            it->second.classInitialized = isClassInitialized;
            }
         }
      }
   return isClassInitialized;
   }

UDATA
TR_J9ServerVM::getOSRFrameSizeInBytes(TR_OpaqueMethodBlock * method)
   {
      {
      ClientSessionData *clientSessionData = _compInfoPT->getClientData();
      OMR::CriticalSection getRemoteROMClass(clientSessionData->getROMMapMonitor());
      auto it = clientSessionData->getJ9MethodMap().find((J9Method*) method);
      if (it != clientSessionData->getJ9MethodMap().end())
         {
         return osrFrameSizeRomMethod(it->second._romMethod);
         }
      }
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getOSRFrameSizeInBytes, method);
   return std::get<0>(stream->read<UDATA>());
   }

int32_t
TR_J9ServerVM::getByteOffsetToLockword(TR_OpaqueClassBlock * clazz)
   {
#if defined (J9VM_THR_LOCK_NURSERY)
   uint32_t byteOffsetToLockword = 0;
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   JITaaSHelpers::getAndCacheRAMClassInfo((J9Class *)clazz, _compInfoPT->getClientData(), stream, JITaaSHelpers::CLASSINFO_BYTE_OFFSET_TO_LOCKWORD, (void *)&byteOffsetToLockword);
   return byteOffsetToLockword;
#else
   return TMP_OFFSETOF_J9OBJECT_MONITOR;
#endif
   }

bool
TR_J9ServerVM::isString(TR_OpaqueClassBlock * clazz)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_isString1, clazz);
   return std::get<0>(stream->read<bool>());
   }

void *
TR_J9ServerVM::getMethods(TR_OpaqueClassBlock * clazz)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getMethods, clazz);
   return std::get<0>(stream->read<void *>());
   }

void
TR_J9ServerVM::getResolvedMethods(TR_Memory * trMemory, TR_OpaqueClassBlock * classPointer, List<TR_ResolvedMethod> * resolvedMethodsInClass)
   {
   getResolvedMethodsAndMethods(trMemory, classPointer, resolvedMethodsInClass, NULL, NULL);
   }


/*uint32_t
TR_J9ServerVM::getNumMethods(TR_OpaqueClassBlock * clazz)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getNumMethods, clazz);
   return std::get<0>(stream->read<uint32_t>());
   }

uint32_t
TR_J9ServerVM::getNumInnerClasses(TR_OpaqueClassBlock * clazz)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getNumInnerClasses, clazz);
   return std::get<0>(stream->read<uint32_t>());
   }*/
bool
TR_J9ServerVM::isPrimitiveArray(TR_OpaqueClassBlock *clazz)
   {
   uint32_t modifiers = 0;
   TR_OpaqueClassBlock * componentClass = nullptr;
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   JITaaSHelpers::getAndCacheRAMClassInfo((J9Class *)clazz, _compInfoPT->getClientData(), stream, JITaaSHelpers::CLASSINFO_ROMCLASS_MODIFIERS, (void *)&modifiers, JITaaSHelpers::CLASSINFO_COMPONENT_CLASS, (void *)&componentClass);

   if (!J9_ARE_ALL_BITS_SET(modifiers, J9AccClassArray))
      return false;

   JITaaSHelpers::getAndCacheRAMClassInfo((J9Class *)componentClass, _compInfoPT->getClientData(), stream, JITaaSHelpers::CLASSINFO_ROMCLASS_MODIFIERS, (void *)&modifiers);
   return J9_ARE_ALL_BITS_SET(modifiers, J9AccClassInternalPrimitiveType) ? true : false;

   }

uint32_t
TR_J9ServerVM::getAllocationSize(TR::StaticSymbol *classSym, TR_OpaqueClassBlock *clazz)
   {
   uintptrj_t totalInstanceSize = 0;
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   JITaaSHelpers::getAndCacheRAMClassInfo((J9Class *)clazz, _compInfoPT->getClientData(), stream, JITaaSHelpers::CLASSINFO_TOTAL_INSTANCE_SIZE, (void *)&totalInstanceSize);

   uint32_t objectSize = sizeof(J9Object) + (uint32_t)totalInstanceSize;
   return ((objectSize >= J9_GC_MINIMUM_OBJECT_SIZE) ? objectSize : J9_GC_MINIMUM_OBJECT_SIZE);
   }

TR_OpaqueClassBlock *
TR_J9ServerVM::getObjectClass(uintptrj_t objectPointer)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getObjectClass, objectPointer);
   return std::get<0>(stream->read<TR_OpaqueClassBlock *>());
   }

uintptrj_t
TR_J9ServerVM::getStaticReferenceFieldAtAddress(uintptrj_t fieldAddress)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getStaticReferenceFieldAtAddress, fieldAddress);
   return std::get<0>(stream->read<uintptrj_t>());
   }

bool
TR_J9ServerVM::stackWalkerMaySkipFrames(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *clazz)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_stackWalkerMaySkipFrames, method, clazz);
   return std::get<0>(stream->read<bool>());
   }

bool
TR_J9ServerVM::hasFinalFieldsInClass(TR_OpaqueClassBlock *clazz)
   {
   bool classHasFinalFields = false;
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   JITaaSHelpers::getAndCacheRAMClassInfo((J9Class *)clazz, _compInfoPT->getClientData(), stream, JITaaSHelpers::CLASSINFO_CLASS_HAS_FINAL_FIELDS, (void *)&classHasFinalFields);

   return classHasFinalFields;
   }

const char *
TR_J9ServerVM::sampleSignature(TR_OpaqueMethodBlock * aMethod, char *buf, int32_t bufLen, TR_Memory *trMemory_unused)
   {
   // the passed in TR_Memory is possibly null.
   // in the superclass it would be null if it was not needed, but here we always need it.
   // so we just get it out of the compilation.
   TR_Memory *trMemory = _compInfoPT->getCompilation()->trMemory();
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getClassNameSignatureFromMethod, (J9Method*) aMethod);
   auto recv = stream->read<std::string, std::string, std::string>();
   const std::string str_className = std::get<0>(recv);
   const std::string str_name = std::get<1>(recv);
   const std::string str_signature = std::get<2>(recv);
   J9UTF8 * className = str2utf8((char*)&str_className[0], str_className.length(), trMemory, heapAlloc);
   J9UTF8 * name = str2utf8((char*)&str_name[0], str_name.length(), trMemory, heapAlloc);
   J9UTF8 * signature = str2utf8((char*)&str_signature[0], str_signature.length(), trMemory, heapAlloc);

   int32_t len = J9UTF8_LENGTH(className)+J9UTF8_LENGTH(name)+J9UTF8_LENGTH(signature)+3;
   char * s = len <= bufLen ? buf : (trMemory ? (char*)trMemory->allocateHeapMemory(len) : NULL);
   if (s)
      sprintf(s, "%.*s.%.*s%.*s", J9UTF8_LENGTH(className), utf8Data(className), J9UTF8_LENGTH(name), utf8Data(name), J9UTF8_LENGTH(signature), utf8Data(signature));
   return s;
   }

TR_OpaqueClassBlock *
TR_J9ServerVM::getHostClass(TR_OpaqueClassBlock *clazz)
   {
   TR_OpaqueClassBlock *hostClass = nullptr;
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   JITaaSHelpers::getAndCacheRAMClassInfo((J9Class *)clazz, _compInfoPT->getClientData(), stream, JITaaSHelpers::CLASSINFO_HOST_CLASS, (void *)&hostClass);

   return hostClass;
   }

intptrj_t
TR_J9ServerVM::getStringUTF8Length(uintptrj_t objectPointer)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getStringUTF8Length, objectPointer);
   return std::get<0>(stream->read<intptrj_t>());
   }
/*
uintptrj_t
TR_J9ServerVM::getPersistentClassPointerFromClassPointer(TR_OpaqueClassBlock *clazz)
   {
   J9Class *j9clazz = TR::Compiler->cls.convertClassOffsetToClassPtr(clazz);
   return reinterpret_cast<uintptrj_t>(TR::compInfoPT->getAndCacheRemoteROMClass(j9clazz));
   }
*/
bool
TR_J9ServerVM::classInitIsFinished(TR_OpaqueClassBlock *clazz)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_classInitIsFinished, clazz);
   return std::get<0>(stream->read<bool>());
   }

int32_t
TR_J9ServerVM::getNewArrayTypeFromClass(TR_OpaqueClassBlock *clazz)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getNewArrayTypeFromClass, clazz);
   return std::get<0>(stream->read<int32_t>());
   }

TR_OpaqueClassBlock *
TR_J9ServerVM::getClassFromNewArrayType(int32_t arrayType)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getClassFromNewArrayType, arrayType);
   return std::get<0>(stream->read<TR_OpaqueClassBlock *>());
   }

bool
TR_J9ServerVM::isCloneable(TR_OpaqueClassBlock *clazzPointer)
   {
   uintptrj_t classDepthAndFlags = 0;
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   JITaaSHelpers::getAndCacheRAMClassInfo((J9Class *)clazzPointer, _compInfoPT->getClientData(), stream, JITaaSHelpers::CLASSINFO_CLASS_DEPTH_AND_FLAGS, (void *)&classDepthAndFlags);
   return ((classDepthAndFlags & J9AccClassCloneable) != 0);
   }

bool
TR_J9ServerVM::canAllocateInlineClass(TR_OpaqueClassBlock *clazz)
   {
   uint32_t modifiers = 0;
   bool isClassInitialized = false;
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   JITaaSHelpers::getAndCacheRAMClassInfo((J9Class *)clazz, _compInfoPT->getClientData(), stream, JITaaSHelpers::CLASSINFO_CLASS_INITIALIZED, (void *)&isClassInitialized, JITaaSHelpers::CLASSINFO_ROMCLASS_MODIFIERS, (void *)&modifiers);

  if (!isClassInitialized)
     {
     JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
     stream->write(JITaaS::J9ServerMessageType::VM_isClassInitialized, clazz);
     isClassInitialized = std::get<0>(stream->read<bool>());
     if (isClassInitialized)
        {
        OMR::CriticalSection getRemoteROMClass(_compInfoPT->getClientData()->getROMMapMonitor());
        auto it = _compInfoPT->getClientData()->getROMClassMap().find((J9Class*) clazz);
        if (it != _compInfoPT->getClientData()->getROMClassMap().end())
           {
           it->second.classInitialized = isClassInitialized;
           }
        }
     }

   if (isClassInitialized)
      return ((modifiers & (J9AccAbstract | J9AccInterface ))?false:true);

   return false;
   }

TR_OpaqueClassBlock *
TR_J9ServerVM::getArrayClassFromComponentClass(TR_OpaqueClassBlock *componentClass)
   {
   TR_OpaqueClassBlock *arrayClass = nullptr;
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   JITaaSHelpers::getAndCacheRAMClassInfo((J9Class *)componentClass, _compInfoPT->getClientData(), stream, JITaaSHelpers::CLASSINFO_ARRAY_CLASS, (void *)&arrayClass);
   return arrayClass;
   }

J9Class *
TR_J9ServerVM::matchRAMclassFromROMclass(J9ROMClass *clazz, TR::Compilation *comp)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_matchRAMclassFromROMclass, clazz);
   return std::get<0>(stream->read<J9Class *>());
   }

int32_t *
TR_J9ServerVM::getCurrentLocalsMapForDLT(TR::Compilation *comp)
   {
   int32_t *currentBundles = NULL;
#if defined(J9VM_JIT_DYNAMIC_LOOP_TRANSFER)
   TR_ResolvedJ9JITaaSServerMethod *currentMethod = (TR_ResolvedJ9JITaaSServerMethod *)(comp->getCurrentMethod());
   int32_t numBundles = currentMethod->numberOfTemps() + currentMethod->numberOfParameterSlots();
   numBundles = (numBundles+31)/32;
   currentBundles = (int32_t *)comp->trMemory()->allocateHeapMemory(numBundles * sizeof(int32_t));
   jitConfig->javaVM->localMapFunction(_portLibrary, currentMethod->romClassPtr(), currentMethod->romMethod(), comp->getDltBcIndex(), (U_32 *)currentBundles, NULL, NULL, NULL);
#endif
   return currentBundles;
   }

uintptrj_t
TR_J9ServerVM::getReferenceFieldAtAddress(uintptrj_t fieldAddress)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getReferenceFieldAtAddress, fieldAddress);
   return std::get<0>(stream->read<uintptrj_t>());
   }

uintptrj_t
TR_J9ServerVM::getVolatileReferenceFieldAt(uintptrj_t objectPointer, uintptrj_t fieldOffset)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getVolatileReferenceFieldAt, objectPointer, fieldOffset);
   return std::get<0>(stream->read<uintptrj_t>());
   }

int32_t
TR_J9ServerVM::getInt32FieldAt(uintptrj_t objectPointer, uintptrj_t fieldOffset)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getInt32FieldAt, objectPointer, fieldOffset);
   return std::get<0>(stream->read<int32_t>());
   }

int64_t
TR_J9ServerVM::getInt64FieldAt(uintptrj_t objectPointer, uintptrj_t fieldOffset)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getInt64FieldAt, objectPointer, fieldOffset);
   return std::get<0>(stream->read<int64_t>());
   }

void
TR_J9ServerVM::setInt64FieldAt(uintptrj_t objectPointer, uintptrj_t fieldOffset, int64_t newValue)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_setInt64FieldAt, objectPointer, fieldOffset, newValue);
   stream->read<JITaaS::Void>();
   }

bool
TR_J9ServerVM::compareAndSwapInt64FieldAt(uintptrj_t objectPointer, uintptrj_t fieldOffset, int64_t oldValue, int64_t newValue)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_compareAndSwapInt64FieldAt, objectPointer, fieldOffset, oldValue, newValue);
   return std::get<0>(stream->read<bool>());
   }

intptrj_t
TR_J9ServerVM::getArrayLengthInElements(uintptrj_t objectPointer)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getArrayLengthInElements, objectPointer);
   return std::get<0>(stream->read<intptrj_t>());
   }

TR_OpaqueClassBlock *
TR_J9ServerVM::getClassFromJavaLangClass(uintptrj_t objectPointer)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getClassFromJavaLangClass, objectPointer);
   return std::get<0>(stream->read<TR_OpaqueClassBlock *>());
   }

UDATA
TR_J9ServerVM::getOffsetOfClassFromJavaLangClassField()
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getOffsetOfClassFromJavaLangClassField, JITaaS::Void());
   return std::get<0>(stream->read<UDATA>());
   }

uintptrj_t
TR_J9ServerVM::getConstantPoolFromMethod(TR_OpaqueMethodBlock *method)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getConstantPoolFromMethod, method);
   return std::get<0>(stream->read<uintptrj_t>());
   }

uintptrj_t
TR_J9ServerVM::getConstantPoolFromClass(TR_OpaqueClassBlock *clazz)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getConstantPoolFromClass, clazz);
   return std::get<0>(stream->read<uintptrj_t>());
   }

uintptrj_t
TR_J9ServerVM::getProcessID()
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   auto *vmInfo = _compInfoPT->getClientData()->getOrCacheVMInfo(stream);
   return vmInfo->_processID;
   }

UDATA
TR_J9ServerVM::getIdentityHashSaltPolicy()
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getIdentityHashSaltPolicy, JITaaS::Void());
   return std::get<0>(stream->read<UDATA>());
   }

UDATA
TR_J9ServerVM::getOffsetOfJLThreadJ9Thread()
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getOffsetOfJLThreadJ9Thread, JITaaS::Void());
   return std::get<0>(stream->read<UDATA>());
   }

bool
TR_J9ServerVM::scanReferenceSlotsInClassForOffset(TR::Compilation *comp, TR_OpaqueClassBlock *clazz, int32_t offset)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_scanReferenceSlotsInClassForOffset, clazz, offset);
   return std::get<0>(stream->read<bool>());
   }

int32_t
TR_J9ServerVM::findFirstHotFieldTenuredClassOffset(TR::Compilation *comp, TR_OpaqueClassBlock *clazz)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_findFirstHotFieldTenuredClassOffset, clazz);
   return std::get<0>(stream->read<int32_t>());
   }

TR_OpaqueMethodBlock *
TR_J9ServerVM::getResolvedVirtualMethod(TR_OpaqueClassBlock * classObject, I_32 virtualCallOffset, bool ignoreRtResolve)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getResolvedVirtualMethod, classObject, virtualCallOffset, ignoreRtResolve);
   return std::get<0>(stream->read<TR_OpaqueMethodBlock *>());
   }

TR::CodeCache *
TR_J9ServerVM::getDesignatedCodeCache(TR::Compilation *comp)
   {
   TR::CodeCache * codeCache = TR_J9VMBase::getDesignatedCodeCache(comp);
   
   if (codeCache)
      {
      // memorize the allocation pointers
      comp->setRelocatableMethodCodeStart((uint32_t *)codeCache->getWarmCodeAlloc());
      }
   return codeCache;
   }

uint8_t *
TR_J9ServerVM::allocateCodeMemory(TR::Compilation * comp, uint32_t warmCodeSize, uint32_t coldCodeSize, uint8_t ** coldCode, bool isMethodHeaderNeeded)
   {
   uint8_t *warmCode = TR_J9VM::allocateCodeMemory(comp, warmCodeSize, coldCodeSize, coldCode, isMethodHeaderNeeded);
   // JITaaS FIXME: why is this code needed? Shouldn't this be done when reserving?
   if (!comp->getRelocatableMethodCodeStart())
      comp->setRelocatableMethodCodeStart(warmCode - sizeof(OMR::CodeCacheMethodHeader));
   return warmCode;
   }

bool
TR_J9ServerVM::sameClassLoaders(TR_OpaqueClassBlock * class1, TR_OpaqueClassBlock * class2)
   {
   void *class1Loader = nullptr;
   void *class2Loader = nullptr;
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   JITaaSHelpers::getAndCacheRAMClassInfo((J9Class *)class1, _compInfoPT->getClientData(), stream, JITaaSHelpers::CLASSINFO_CLASS_LOADER, (void *)&class1Loader);
   JITaaSHelpers::getAndCacheRAMClassInfo((J9Class *)class2, _compInfoPT->getClientData(), stream, JITaaSHelpers::CLASSINFO_CLASS_LOADER, (void *)&class2Loader);

    return (class1Loader == class2Loader);
   }

bool
TR_J9ServerVM::isUnloadAssumptionRequired(TR_OpaqueClassBlock *clazz, TR_ResolvedMethod *methodBeingCompiled)
   {
   TR_OpaqueClassBlock * classOfMethod = static_cast<TR_ResolvedJ9JITaaSServerMethod *>(methodBeingCompiled)->classOfMethod();
   uint32_t extraModifiers = 0;
   void *classLoader = nullptr;
   void *classOfMethodLoader = nullptr;
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;

   if (clazz == classOfMethod)
      return false;

   JITaaSHelpers::getAndCacheRAMClassInfo((J9Class *)clazz, _compInfoPT->getClientData(), stream, JITaaSHelpers::CLASSINFO_CLASS_LOADER, (void *)&classLoader,JITaaSHelpers::CLASSINFO_ROMCLASS_EXTRAMODIFIERS, (void *)&extraModifiers);

   if (!J9_ARE_ALL_BITS_SET(extraModifiers, J9AccClassAnonClass))
      {
      if(classLoader == getSystemClassLoader())
         {
         return false;
         }
      else
         {
         JITaaSHelpers::getAndCacheRAMClassInfo((J9Class *)classOfMethod, _compInfoPT->getClientData(), stream, JITaaSHelpers::CLASSINFO_CLASS_LOADER, (void *)&classOfMethodLoader);
         return (classLoader != classOfMethodLoader);
         }
      }
   else
      {
      return true;
      }
   }

uint32_t
TR_J9ServerVM::getInstanceFieldOffset(TR_OpaqueClassBlock *clazz, char *fieldName, uint32_t fieldLen, char *sig, uint32_t sigLen, UDATA options)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getInstanceFieldOffset, clazz, std::string(fieldName, fieldLen), std::string(sig, sigLen), options);
   return std::get<0>(stream->read<uint32_t>());
   }

int32_t
TR_J9ServerVM::getJavaLangClassHashCode(TR::Compilation *comp, TR_OpaqueClassBlock *clazz, bool &hashCodeComputed)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getJavaLangClassHashCode, clazz);
   auto recv = stream->read<int32_t, bool>();
   hashCodeComputed = std::get<1>(recv);
   return std::get<0>(recv);
   }

bool
TR_J9ServerVM::hasFinalizer(TR_OpaqueClassBlock *clazz)
   {
   uintptrj_t classDepthAndFlags = 0;
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   JITaaSHelpers::getAndCacheRAMClassInfo((J9Class *)clazz, _compInfoPT->getClientData(), stream, JITaaSHelpers::CLASSINFO_CLASS_DEPTH_AND_FLAGS, (void *)&classDepthAndFlags);

   return ((classDepthAndFlags & (J9AccClassFinalizeNeeded | J9AccClassOwnableSynchronizer)) != 0);
   }

uintptrj_t
TR_J9ServerVM::getClassDepthAndFlagsValue(TR_OpaqueClassBlock *clazz)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getClassDepthAndFlagsValue, clazz);
   return std::get<0>(stream->read<uintptrj_t>());
   }

TR_OpaqueMethodBlock *
TR_J9ServerVM::getMethodFromName(char *className, char *methodName, char *signature)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getMethodFromName, std::string(className, strlen(className)),
         std::string(methodName, strlen(methodName)), std::string(signature, strlen(signature)));
   return std::get<0>(stream->read<TR_OpaqueMethodBlock *>());
   }

TR_OpaqueMethodBlock *
TR_J9ServerVM::getMethodFromClass(TR_OpaqueClassBlock *methodClass, char *methodName, char *signature, TR_OpaqueClassBlock *callingClass)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getMethodFromClass, methodClass, std::string(methodName, strlen(methodName)),
         std::string(signature, strlen(signature)), callingClass);
   return std::get<0>(stream->read<TR_OpaqueMethodBlock *>());
   }

bool
TR_J9ServerVM::isClassVisible(TR_OpaqueClassBlock *sourceClass, TR_OpaqueClassBlock *destClass)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_isClassVisible, sourceClass, destClass);
   return std::get<0>(stream->read<bool>());
   }

void *
TR_J9ServerVM::setJ2IThunk(char *signatureChars, uint32_t signatureLength, void *thunkptr, TR::Compilation *comp)
   {
   TR_J9VMBase::setJ2IThunk(signatureChars, signatureLength, thunkptr, comp);
   std::string signature(signatureChars, signatureLength);
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_setJ2IThunk, thunkptr, signature);
   stream->read<JITaaS::Void>();
   return thunkptr;
   }

void
TR_J9ServerVM::markClassForTenuredAlignment(TR::Compilation *comp, TR_OpaqueClassBlock *clazz, uint32_t alignFromStart)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_markClassForTenuredAlignment, clazz, alignFromStart);
   stream->read<JITaaS::Void>();
   }

int32_t *
TR_J9ServerVM::getReferenceSlotsInClass(TR::Compilation *comp, TR_OpaqueClassBlock *clazz)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getReferenceSlotsInClass, clazz);
   std::string slotsStr = std::get<0>(stream->read<std::string>());
   if (slotsStr == "")
      return nullptr;
   int32_t *refSlots = (int32_t *)comp->trHeapMemory().allocate(slotsStr.size());
   if (!refSlots)
      throw std::bad_alloc();
   memcpy(refSlots, &slotsStr[0], slotsStr.size());
   return refSlots;
   }

uint32_t
TR_J9ServerVM::getMethodSize(TR_OpaqueMethodBlock *method)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getMethodSize, method);
   return std::get<0>(stream->read<uint32_t>());
   }

void *
TR_J9ServerVM::addressOfFirstClassStatic(TR_OpaqueClassBlock *clazz)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_addressOfFirstClassStatic, clazz);
   return std::get<0>(stream->read<void *>());
   }

void *
TR_J9ServerVM::getStaticFieldAddress(TR_OpaqueClassBlock *clazz, unsigned char *fieldName, uint32_t fieldLen, unsigned char *sig, uint32_t sigLen)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getStaticFieldAddress, clazz, std::string(reinterpret_cast<char*>(fieldName), fieldLen), std::string(reinterpret_cast<char*>(sig), sigLen));
   return std::get<0>(stream->read<void *>());
   }

int32_t
TR_J9ServerVM::getInterpreterVTableSlot(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *clazz)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getInterpreterVTableSlot, method, clazz);
   return std::get<0>(stream->read<int32_t>());
   }

void
TR_J9ServerVM::revertToInterpreted(TR_OpaqueMethodBlock *method)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_revertToInterpreted, method);
   stream->read<JITaaS::Void>();
   }

void *
TR_J9ServerVM::getLocationOfClassLoaderObjectPointer(TR_OpaqueClassBlock *clazz)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getLocationOfClassLoaderObjectPointer, clazz);
   return std::get<0>(stream->read<void *>());
   }

bool
TR_J9ServerVM::isOwnableSyncClass(TR_OpaqueClassBlock *clazz)
   {
   uintptrj_t classDepthAndFlags = 0;
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   JITaaSHelpers::getAndCacheRAMClassInfo((J9Class *)clazz, _compInfoPT->getClientData(), stream, JITaaSHelpers::CLASSINFO_CLASS_DEPTH_AND_FLAGS, (void *)&classDepthAndFlags);

   return ((classDepthAndFlags & J9AccClassOwnableSynchronizer) != 0);
   }

TR_OpaqueClassBlock *
TR_J9ServerVM::getClassFromMethodBlock(TR_OpaqueMethodBlock *method)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getClassFromMethodBlock, method);
   return std::get<0>(stream->read<TR_OpaqueClassBlock *>());
   }

U_8 *
TR_J9ServerVM::fetchMethodExtendedFlagsPointer(J9Method *method)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_fetchMethodExtendedFlagsPointer, method);
   return std::get<0>(stream->read<U_8 *>());
   }

void *
TR_J9ServerVM::getStaticHookAddress(int32_t event)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getStaticHookAddress, event);
   return std::get<0>(stream->read<void *>());
   }

bool
TR_J9ServerVM::stringEquals(TR::Compilation *comp, uintptrj_t* stringLocation1, uintptrj_t*stringLocation2, int32_t& result)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_stringEquals, stringLocation1, stringLocation2);
   auto recv = stream->read<int32_t, bool>();
   result = std::get<0>(recv);
   return std::get<1>(recv);
   }

bool
TR_J9ServerVM::getStringHashCode(TR::Compilation *comp, uintptrj_t* stringLocation, int32_t& result)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getStringHashCode, stringLocation);
   auto recv = stream->read<int32_t, bool>();
   result = std::get<0>(recv);
   return std::get<1>(recv);
   }

int32_t
TR_J9ServerVM::getLineNumberForMethodAndByteCodeIndex(TR_OpaqueMethodBlock *method, int32_t bcIndex)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getLineNumberForMethodAndByteCodeIndex, method, bcIndex);
   return std::get<0>(stream->read<int32_t>());
   }

TR_OpaqueMethodBlock *
TR_J9ServerVM::getObjectNewInstanceImplMethod()
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getObjectNewInstanceImplMethod, JITaaS::Void());
   return std::get<0>(stream->read<TR_OpaqueMethodBlock *>());
   }

uintptrj_t
TR_J9ServerVM::getBytecodePC(TR_OpaqueMethodBlock *method, TR_ByteCodeInfo &bcInfo)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getBytecodePC, method);
   uintptrj_t methodStart = std::get<0>(stream->read<uintptrj_t>());
   return methodStart + (uintptrj_t)(bcInfo.getByteCodeIndex());
   }

bool
TR_J9ServerVM::isClassLoadedBySystemClassLoader(TR_OpaqueClassBlock *clazz)
   {
   void *classLoader = nullptr;
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   JITaaSHelpers::getAndCacheRAMClassInfo((J9Class *)clazz, _compInfoPT->getClientData(), stream, JITaaSHelpers::CLASSINFO_CLASS_LOADER, (void *)&classLoader);

   return (classLoader == getSystemClassLoader());
   }

void
TR_J9ServerVM::setInvokeExactJ2IThunk(void *thunkptr, TR::Compilation *comp)
   {
   TR_J9VMBase::setInvokeExactJ2IThunk(thunkptr, comp);
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_setInvokeExactJ2IThunk, thunkptr);
   stream->read<JITaaS::Void>();
   }

bool
TR_J9ServerVM::needsInvokeExactJ2IThunk(TR::Node *callNode, TR::Compilation *comp)
   {
   TR_ASSERT(callNode->getOpCode().isCall(), "needsInvokeExactJ2IThunk expects call node; found %s", callNode->getOpCode().getName());

   TR::MethodSymbol *methodSymbol = callNode->getSymbol()->castToMethodSymbol();
   TR_Method       *method       = methodSymbol->getMethod();
   if (  methodSymbol->isComputed()
      && (  method->getMandatoryRecognizedMethod() == TR::java_lang_invoke_MethodHandle_invokeExact
         || method->isArchetypeSpecimen()))
      {
      // for JITaaS always need to regenerate a thunk, when it's needed
      return true;
      }
   else
      return false;
   }

TR_ResolvedMethod *
TR_J9ServerVM::createMethodHandleArchetypeSpecimen(TR_Memory *trMemory, uintptrj_t *methodHandleLocation, TR_ResolvedMethod *owningMethod)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_createMethodHandleArchetypeSpecimen, methodHandleLocation);
   auto recv = stream->read<TR_OpaqueMethodBlock*, std::string>();
   TR_OpaqueMethodBlock *archetype = std::get<0>(recv);
   std::string thunkableSignature = std::get<1>(recv);
   if (!archetype)
      return nullptr;

   TR_ResolvedMethod *result = createResolvedMethodWithSignature(trMemory, archetype, NULL, &thunkableSignature[0], thunkableSignature.length(), owningMethod);
   result->convertToMethod()->setArchetypeSpecimen();
   result->setMethodHandleLocation(methodHandleLocation);
   return result;
   }

bool
TR_J9ServerVM::getArrayLengthOfStaticAddress(void *ptr, int32_t &length)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getArrayLengthOfStaticAddress, ptr);
   auto recv = stream->read<bool, int32_t>();
   length = std::get<1>(recv);
   return std::get<0>(recv);
   }

intptrj_t
TR_J9ServerVM::getVFTEntry(TR_OpaqueClassBlock *clazz, int32_t offset)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getVFTEntry, clazz, offset);
   return std::get<0>(stream->read<intptrj_t>());
   }

bool
TR_J9ServerVM::isClassArray(TR_OpaqueClassBlock *klass)
   {
   // If we have the comp, then we can cache the rom class locally
   if (TR::comp())
      {
      return TR_J9VMBase::isClassArray(klass);
      }
   else
      {
      // otherwise, do it remote
      TR_ASSERT(_compInfoPT, "must have either compInfoPT or Compiler");
      JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
      stream->write(JITaaS::J9ServerMessageType::VM_isClassArray, klass);
      return std::get<0>(stream->read<bool>());
      }
   }

bool
TR_J9ServerVM::instanceOfOrCheckCast(J9Class *instanceClass, J9Class* castClass)
   {
   if (instanceClass == castClass)
      return true;

   // When castClass is an ancestor/interface of class instanceClass, can avoid a remote message,
   // since superclasses and interfaces are cached on the server
      {
      OMR::CriticalSection getRemoteROMClass(_compInfoPT->getClientData()->getROMMapMonitor());
      auto it = _compInfoPT->getClientData()->getROMClassMap().find((J9Class*) instanceClass);
      if (it != _compInfoPT->getClientData()->getROMClassMap().end())
         {
         TR_OpaqueClassBlock *instanceClassOffset = (TR_OpaqueClassBlock *) instanceClass;
         TR_OpaqueClassBlock *castClassOffset = (TR_OpaqueClassBlock *) castClass; 

         // this sometimes results in remote messages, but it happens relatively infrequently
         for (TR_OpaqueClassBlock *clazz = getSuperClass(instanceClassOffset); clazz; clazz = getSuperClass(clazz))
            if (clazz == castClassOffset)
               return true;

         // castClass is an interface, check the cached iTable
         if (isInterfaceClass(castClassOffset))
            {
            auto interfaces = it->second.interfaces;
            return std::find(interfaces->begin(), interfaces->end(), castClassOffset) != interfaces->end();
            }

         if (isClassArray(instanceClassOffset) && isClassArray(castClassOffset))
            {
            int instanceNumDims = 0, castNumDims = 0;
            // these are the leaf classes of the arrays, unless the leaf class is primitive.
            // in that case, return values are one-dimensional arrays, and 
            // instanceNumDims is 1 less than the actual number of array dimensions
            instanceClassOffset = getBaseComponentClass(instanceClassOffset, instanceNumDims);
            castClassOffset = getBaseComponentClass(castClassOffset, castNumDims);
                       
            if (instanceNumDims < castNumDims)
               return false;
            if (instanceNumDims != 0 && instanceNumDims == castNumDims)
               return instanceOfOrCheckCast((J9Class *) instanceClassOffset, (J9Class *) castClassOffset);

            // reached when (instanceNumDims > castNumDims), or when one of the arrays contains primitive values.
            // to correctly deal with these cases on the server, need to call
            // getComponentFromArrayClass multiple times, or getLeafComponentTypeFromArrayClass.
            // each call requires a remote message, faster and easier to call instanceOfOrCheckCast on the client
            JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
            stream->write(JITaaS::J9ServerMessageType::VM_instanceOfOrCheckCast, instanceClass, castClass);
            return std::get<0>(stream->read<bool>());
            }
         // instance class is cached, and all of the checks failed, cast is invalid
         return false;
         }
      }

   // instance class is not cached, make a remote call
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_instanceOfOrCheckCast, instanceClass, castClass);
   return std::get<0>(stream->read<bool>());
   }

bool
TR_J9ServerVM::transformJlrMethodInvoke(J9Method *callerMethod, J9Class *callerClass)
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_transformJlrMethodInvoke, callerMethod, callerClass);
   return std::get<0>(stream->read<bool>());
   }

bool
TR_J9ServerVM::isAnonymousClass(TR_OpaqueClassBlock *j9clazz)
   {
   uintptrj_t extraModifiers = 0;
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   JITaaSHelpers::getAndCacheRAMClassInfo((J9Class *)j9clazz, _compInfoPT->getClientData(), stream, JITaaSHelpers::CLASSINFO_ROMCLASS_EXTRAMODIFIERS, (void *)&extraModifiers);

   return (J9_ARE_ALL_BITS_SET(extraModifiers, J9AccClassAnonClass));
   }

TR_IProfiler *
TR_J9ServerVM::getIProfiler()
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   auto * vmInfo = _compInfoPT->getClientData()->getOrCacheVMInfo(stream);
   if (!vmInfo->_isIProfilerEnabled)
      return NULL;

   if (!_iProfiler)
      {
      // This used to use a global variable called 'jitConfig' instead of the local object's '_jitConfig'.  In early out of memory scenarios, the global jitConfig may be NULL.
      _iProfiler = ((TR_JitPrivateConfig*)(_jitConfig->privateConfig))->iProfiler;
      }
   return _iProfiler;
   }

TR_StaticFinalData
TR_J9ServerVM::dereferenceStaticFinalAddress(void *staticAddress, TR::DataType addressType)
   {
   if (!staticAddress)
      return {.dataAddress = 0};

      {
      // check if the value is cached
      OMR::CriticalSection dereferenceStaticFinalAddress(_compInfoPT->getClientData()->getStaticMapMonitor());
      auto &staticMap = _compInfoPT->getClientData()->getStaticFinalDataMap();
      auto it = staticMap.find(staticAddress);
      if (it != staticMap.end())
         return it->second;
      }

   // value at the static address is not cached, ask the client
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_dereferenceStaticAddress, staticAddress, addressType);
   auto data =  std::get<0>(stream->read<TR_StaticFinalData>());
   // cache the result
   OMR::CriticalSection dereferenceStaticFinalAddress(_compInfoPT->getClientData()->getStaticMapMonitor());
   auto &staticMap = _compInfoPT->getClientData()->getStaticFinalDataMap();
   auto it = staticMap.insert({staticAddress, data}).first;
   return it->second;
   }

void
TR_J9ServerVM::reserveTrampolineIfNecessary( TR::Compilation *, TR::SymbolReference *symRef, bool inBinaryEncoding)
{
    // Not necessary in JITaaS server mode
}

intptrj_t
TR_J9ServerVM::methodTrampolineLookup(TR::Compilation *comp, TR::SymbolReference * symRef, void * callSite)
{
    // Not necessary in JITaaS server mode, return the call's PC so that the call will not appear to require a trampoline
    return (intptrj_t)callSite;
}

uintptrj_t
TR_J9ServerVM::getPersistentClassPointerFromClassPointer(TR_OpaqueClassBlock * clazz)
   {
   J9ROMClass *remoteRomClass = NULL;
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   JITaaSHelpers::getAndCacheRAMClassInfo((J9Class *) clazz, _compInfoPT->getClientData(), stream, JITaaSHelpers::CLASSINFO_REMOTE_ROM_CLASS, (void *) &remoteRomClass);
   return (uintptrj_t) remoteRomClass;
   }

TR_OpaqueClassBlock *
TR_J9ServerVM::getClassFromNewArrayTypeNonNull(int32_t arrayType)
   {
   // This query is needed for inline allocation, which requires array class to
   // be non-NULL, but getClassFromNewArrayType returns NULL in AOT mode
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JITaaS::J9ServerMessageType::VM_getClassFromNewArrayTypeNonNull, arrayType);
   auto clazz = std::get<0>(stream->read<TR_OpaqueClassBlock *>());
   TR_ASSERT(clazz, "class must not be NULL");
   return clazz;
   }

bool
TR_J9ServerVM::isStringCompressionEnabledVM()
   {
   JITaaS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   auto *vmInfo = _compInfoPT->getClientData()->getOrCacheVMInfo(stream);
   return vmInfo->_stringCompressionEnabled;
   }

bool
TR_J9SharedCacheServerVM::isClassLibraryMethod(TR_OpaqueMethodBlock *method, bool vettedForAOT)
   {
   TR_ASSERT(vettedForAOT, "The TR_J9SharedCacheServerVM version of this method is expected to be called only from isClassLibraryMethod.\n"
                                          "Please consider whether you call is vetted for AOT!");

   if (getSupportsRecognizedMethods())
      return TR_J9ServerVM::isClassLibraryMethod(method, vettedForAOT);

   return false;
   }

TR_OpaqueMethodBlock *
TR_J9SharedCacheServerVM::getMethodFromClass(TR_OpaqueClassBlock * methodClass, char * methodName, char * signature, TR_OpaqueClassBlock *callingClass)
   {
   TR_OpaqueMethodBlock* omb = TR_J9ServerVM::getMethodFromClass(methodClass, methodName, signature, callingClass);
   if (omb)
      {
      TR::Compilation* comp = _compInfoPT->getCompilation();
      TR_ASSERT(comp, "Should be called only within a compilation");

      if (comp->getOption(TR_UseSymbolValidationManager))
         {
         bool validated = comp->getSymbolValidationManager()->addMethodFromClassAndSignatureRecord(omb, methodClass, callingClass);
         if (!validated)
            omb = NULL;
         }
      else
         {
         if (!((TR_ResolvedRelocatableJ9JITaaSServerMethod*) comp->getCurrentMethod())->validateArbitraryClass(comp, (J9Class*)methodClass))
            omb = NULL;
         if (callingClass && !((TR_ResolvedRelocatableJ9JITaaSServerMethod*) comp->getCurrentMethod())->validateArbitraryClass(comp, (J9Class*)callingClass))
            omb = NULL;
         }
      }

   return omb;
   }

bool
TR_J9SharedCacheServerVM::isPublicClass(TR_OpaqueClassBlock * classPointer)
   {
   TR::Compilation* comp = _compInfoPT->getCompilation();
   TR_ASSERT(comp, "Should be called only within a compilation");

   bool publicClass = TR_J9VM::isPublicClass(classPointer); // TR_J9ServerVM also uses TR_J9VM::isPublicClass
   bool validated = false;

   if (comp->getOption(TR_UseSymbolValidationManager))
      {
      SVM_ASSERT_ALREADY_VALIDATED(comp->getSymbolValidationManager(), classPointer);
      validated = true;
      }
   else
      {
      if (((TR_ResolvedRelocatableJ9JITaaSServerMethod *) comp->getCurrentMethod())->validateArbitraryClass(comp, (J9Class *) classPointer))
         validated = true;
      }

   if (validated)
      return publicClass;
   else
      return true;
   }

TR_OpaqueClassBlock *
TR_J9SharedCacheServerVM::getProfiledClassFromProfiledInfo(TR_ExtraAddressInfo *profiledInfo)
   {
   TR_OpaqueClassBlock * classPointer = (TR_OpaqueClassBlock *)(profiledInfo->_value);
   TR::Compilation* comp = _compInfoPT->getCompilation();
   TR_ASSERT(comp, "Should be called only within a compilation");

   if (comp->getPersistentInfo()->isObsoleteClass((void *)classPointer, comp->fe()))
      return NULL;

   bool validated = false;

   if (comp->getOption(TR_UseSymbolValidationManager))
      {
      validated = comp->getSymbolValidationManager()->addProfiledClassRecord(classPointer);
      }
   else
      {
      if (((TR_ResolvedRelocatableJ9JITaaSServerMethod*) comp->getCurrentMethod())->validateArbitraryClass(comp, (J9Class*)classPointer))
         validated = true;
      }

   if (validated)
      return classPointer;
   else
      return NULL;
   }

TR_YesNoMaybe
TR_J9SharedCacheServerVM::isInstanceOf(TR_OpaqueClassBlock * a, TR_OpaqueClassBlock *b, bool objectTypeIsFixed, bool castTypeIsFixed, bool optimizeForAOT)
   {
   TR::Compilation *comp = _compInfoPT->getCompilation();
   TR_YesNoMaybe isAnInstanceOf = TR_J9ServerVM::isInstanceOf(a, b, objectTypeIsFixed, castTypeIsFixed);
   bool validated = false;

   if (comp && comp->getOption(TR_UseSymbolValidationManager))
      {
      if (isAnInstanceOf != TR_maybe)
         validated = comp->getSymbolValidationManager()->addClassInstanceOfClassRecord(a, b, objectTypeIsFixed, castTypeIsFixed, (isAnInstanceOf == TR_yes));
      }
   else
      {
      validated = optimizeForAOT;
      }

   if (validated)
      return isAnInstanceOf;
   else
      return TR_maybe;
   }

TR_OpaqueClassBlock *
TR_J9SharedCacheServerVM::getSystemClassFromClassName(const char * name, int32_t length, bool isVettedForAOT)
   {
   TR::Compilation *comp = _compInfoPT->getCompilation();
   TR_OpaqueClassBlock *classPointer = TR_J9ServerVM::getSystemClassFromClassName(name, length);
   bool validated = false;

   if (comp && comp->getOption(TR_UseSymbolValidationManager))
      {
      validated = comp->getSymbolValidationManager()->addSystemClassByNameRecord(classPointer);
      }
   else
      {
      if (isVettedForAOT)
         {
         if (((TR_ResolvedRelocatableJ9JITaaSServerMethod *) comp->getCurrentMethod())->validateArbitraryClass(comp, (J9Class *) classPointer))
            validated = true;
         }
      }

   if (validated)
      return classPointer;
   else
      return NULL;
   }

bool
TR_J9SharedCacheServerVM::supportAllocationInlining(TR::Compilation *comp, TR::Node *node)
   {
   if (comp->getOptions()->realTimeGC())
      return false;

   if ((TR::Compiler->target.cpu.isX86() ||
        TR::Compiler->target.cpu.isPower() ||
        TR::Compiler->target.cpu.isZ()) &&
       !comp->getOption(TR_DisableAllocationInlining))
      return true;

   return false;
   }

TR_OpaqueClassBlock *
TR_J9SharedCacheServerVM::getClassFromSignature(const char * sig, int32_t sigLength, TR_ResolvedMethod * method, bool isVettedForAOT)
   {
   TR_ResolvedRelocatableJ9JITaaSServerMethod* resolvedJITaaSMethod = (TR_ResolvedRelocatableJ9JITaaSServerMethod *)method;
   return getClassFromSignature(sig, sigLength, (TR_OpaqueMethodBlock *)resolvedJITaaSMethod->getPersistentIdentifier(), isVettedForAOT);
   }

TR_OpaqueClassBlock *
TR_J9SharedCacheServerVM::getClassFromSignature(const char * sig, int32_t sigLength, TR_OpaqueMethodBlock * method, bool isVettedForAOT)
   {
   TR_OpaqueClassBlock* j9class = TR_J9ServerVM::getClassFromSignature(sig, sigLength, method, true);
   bool validated = false;
   TR::Compilation* comp = _compInfoPT->getCompilation();

   if (j9class)
      {
      if (comp->getOption(TR_UseSymbolValidationManager))
         {
         TR::SymbolValidationManager *svm = comp->getSymbolValidationManager();
         SVM_ASSERT_ALREADY_VALIDATED(svm, method);
         validated = svm->addClassByNameRecord(j9class, getClassFromMethodBlock(method));
         }
      else
         {
         if (isVettedForAOT)
            {
            if (((TR_ResolvedRelocatableJ9JITaaSServerMethod *) comp->getCurrentMethod())->validateArbitraryClass(comp, (J9Class *) j9class))
               validated = true;
            }
         }
      }

   if (validated)
      return j9class;
   else
      return NULL;
   }

bool
TR_J9SharedCacheServerVM::hasFinalizer(TR_OpaqueClassBlock * classPointer)
   {
   TR::Compilation* comp = _compInfoPT->getCompilation();
   TR_ASSERT(comp, "Should be called only within a compilation");

   bool classHasFinalizer = TR_J9ServerVM::hasFinalizer(classPointer);
   bool validated = false;

   if (comp->getOption(TR_UseSymbolValidationManager))
      {
      SVM_ASSERT_ALREADY_VALIDATED(comp->getSymbolValidationManager(), classPointer);
      validated = true;
      }
   else
      {
      if (((TR_ResolvedRelocatableJ9JITaaSServerMethod *) comp->getCurrentMethod())->validateArbitraryClass(comp, (J9Class *) classPointer))
         validated = true;
      }

   if (validated)
      return classHasFinalizer;
   else
      return true;
   }

bool
TR_J9SharedCacheServerVM::isPrimitiveClass(TR_OpaqueClassBlock * classPointer)
   {
   TR::Compilation* comp = _compInfoPT->getCompilation();
   TR_ASSERT(comp, "Should be called only within a compilation");

   bool validated = false;
   bool isPrimClass = TR_J9VMBase::isPrimitiveClass(classPointer); // TR_J9ServerVM also uses TR_J9VMBase::isPrimitiveClass

   if (comp->getOption(TR_UseSymbolValidationManager))
      {
      SVM_ASSERT_ALREADY_VALIDATED(comp->getSymbolValidationManager(), classPointer);
      validated = true;
      }
   else
      {
      if (((TR_ResolvedRelocatableJ9JITaaSServerMethod *) comp->getCurrentMethod())->validateArbitraryClass(comp, (J9Class *) classPointer))
         validated = true;
      }

   if (validated)
      return isPrimClass;
   else
      return false;
   }

bool
TR_J9SharedCacheServerVM::stackWalkerMaySkipFrames(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *methodClass)
   {
   bool skipFrames = TR_J9ServerVM::stackWalkerMaySkipFrames(method, methodClass);
   TR::Compilation *comp = _compInfoPT->getCompilation();
   if (comp && comp->getOption(TR_UseSymbolValidationManager))
      {
      bool recordCreated = comp->getSymbolValidationManager()->addStackWalkerMaySkipFramesRecord(method, methodClass, skipFrames);
      SVM_ASSERT(recordCreated, "Failed to validate addStackWalkerMaySkipFramesRecord");
      }
   return skipFrames;
   }

bool
TR_J9SharedCacheServerVM::isMethodTracingEnabled(TR_OpaqueMethodBlock *method)
   {
   TR::Compilation *comp = _compInfoPT->getCompilation();

   // We want to return the same answer as TR_J9VMBase unless we want to force it to allow tracing
   return TR_J9ServerVM::isMethodTracingEnabled(method) || comp->getOptions()->getOption(TR_EnableAOTMethodEnter) || comp->getOptions()->getOption(TR_EnableAOTMethodExit);
   }

bool
TR_J9SharedCacheServerVM::traceableMethodsCanBeInlined()
   {
   return true;
   }

bool
TR_J9SharedCacheServerVM::canMethodEnterEventBeHooked()
   {
   TR::Compilation *comp = _compInfoPT->getCompilation();
   bool option  = comp->getOptions()->getOption(TR_EnableAOTMethodExit);

   // We want to return the same answer as TR_J9ServerVM unless we want to force it to allow tracing
   return TR_J9ServerVM::canMethodEnterEventBeHooked() || option;
   }

bool
TR_J9SharedCacheServerVM::canMethodExitEventBeHooked()
   {
   TR::Compilation *comp = _compInfoPT->getCompilation();
   bool option  = comp->getOptions()->getOption(TR_EnableAOTMethodExit);
   // We want to return the same answer as TR_J9ServerVM unless we want to force it to allow tracing
   return TR_J9ServerVM::canMethodExitEventBeHooked() || option;
   }

bool
TR_J9SharedCacheServerVM::methodsCanBeInlinedEvenIfEventHooksEnabled()
   {
   return true;
   }

int32_t
TR_J9SharedCacheServerVM::getJavaLangClassHashCode(TR::Compilation * comp, TR_OpaqueClassBlock * clazzPointer, bool &hashCodeComputed)
   {
   hashCodeComputed = false;
   return 0;
   }

bool
TR_J9SharedCacheServerVM::javaLangClassGetModifiersImpl(TR_OpaqueClassBlock * clazzPointer, int32_t &result)
   {
   return false;
   }

uint32_t
TR_J9SharedCacheServerVM::getInstanceFieldOffset(TR_OpaqueClassBlock * classPointer, char * fieldName, uint32_t fieldLen,
                                    char * sig, uint32_t sigLen, UDATA options)
   {
   TR::Compilation* comp = _compInfoPT->getCompilation();
   TR_ASSERT(comp, "Should be called only within a compilation");

   bool validated = false;

   if (comp->getOption(TR_UseSymbolValidationManager))
      {
      SVM_ASSERT_ALREADY_VALIDATED(comp->getSymbolValidationManager(), classPointer);
      validated = true;
      }
   else
      {
      validated = ((TR_ResolvedRelocatableJ9JITaaSServerMethod*) comp->getCurrentMethod())->validateArbitraryClass(comp, (J9Class*)classPointer);
      }

   if (validated)
      return TR_J9ServerVM::getInstanceFieldOffset (classPointer, fieldName, fieldLen, sig, sigLen, options);

   return ~0;
   }

TR_OpaqueClassBlock *
TR_J9SharedCacheServerVM::getSuperClass(TR_OpaqueClassBlock * classPointer)
   {
   TR::Compilation* comp = _compInfoPT->getCompilation();
   TR_ASSERT(comp, "Should be called only within a compilation");

   bool validated = false;
   TR_OpaqueClassBlock *superClass = TR_J9ServerVM::getSuperClass(classPointer);

   if (comp->getOption(TR_UseSymbolValidationManager))
      {
      validated = comp->getSymbolValidationManager()->addSuperClassFromClassRecord(superClass, classPointer);
      }
   else
      {
      validated = ((TR_ResolvedRelocatableJ9JITaaSServerMethod *) comp->getCurrentMethod())->validateArbitraryClass(comp, (J9Class *) classPointer);
      }

   if (validated)
      return superClass;

   return NULL;
   }

void
TR_J9SharedCacheServerVM::getResolvedMethods(TR_Memory *trMemory, TR_OpaqueClassBlock * classPointer, List<TR_ResolvedMethod> * resolvedMethodsInClass)
   {
   TR::Compilation* comp = _compInfoPT->getCompilation();
   TR_ASSERT(comp, "Should be called only within a compilation");

   bool validated = false;

   if (comp->getOption(TR_UseSymbolValidationManager))
      {
      SVM_ASSERT_ALREADY_VALIDATED(comp->getSymbolValidationManager(), classPointer);
      validated = true;
      }
   else
      {
      validated = ((TR_ResolvedRelocatableJ9JITaaSServerMethod *) comp->getCurrentMethod())->validateArbitraryClass(comp, (J9Class *) classPointer);
      }

   if (validated)
      {
      J9Method *resolvedMethods;
      uint32_t numMethods;
      TR_J9ServerVM::getResolvedMethodsAndMethods(trMemory, classPointer, resolvedMethodsInClass, &resolvedMethods, &numMethods);
      if (comp->getOption(TR_UseSymbolValidationManager))
         {
         uint32_t indexIntoArray;
         for (indexIntoArray = 0; indexIntoArray < numMethods; indexIntoArray++)
            {
            comp->getSymbolValidationManager()->addMethodFromClassRecord((TR_OpaqueMethodBlock *) &(resolvedMethods[indexIntoArray]),
                                                                         classPointer,
                                                                         indexIntoArray);
            }
         }

      }
   }


TR_ResolvedMethod *
TR_J9SharedCacheServerVM::getResolvedMethodForNameAndSignature(TR_Memory * trMemory, TR_OpaqueClassBlock * classPointer,
                                                         const char* methodName, const char *signature)
   {
   TR::Compilation* comp = _compInfoPT->getCompilation();
   TR_ASSERT(comp, "Should be called only within a compilation");

   bool validated = false;
   TR_ResolvedMethod *resolvedMethod = TR_J9ServerVM::getResolvedMethodForNameAndSignature(trMemory, classPointer, methodName, signature);

   if (comp->getOption(TR_UseSymbolValidationManager))
      {
      TR_OpaqueMethodBlock *method = (TR_OpaqueMethodBlock *)((TR_ResolvedJ9Method *)resolvedMethod)->ramMethod();
      TR_OpaqueClassBlock *clazz = getClassFromMethodBlock(method);
      SVM_ASSERT_ALREADY_VALIDATED(comp->getSymbolValidationManager(), clazz);
      validated = true;
      }
   else
      {
      validated = ((TR_ResolvedRelocatableJ9JITaaSServerMethod *)comp->getCurrentMethod())->validateArbitraryClass(comp, (J9Class *)classPointer);
      }

   if (validated)
      return resolvedMethod;
   else
      return NULL;
   }

bool
TR_J9SharedCacheServerVM::isPrimitiveArray(TR_OpaqueClassBlock *classPointer)
   {
   TR::Compilation* comp = _compInfoPT->getCompilation();
   TR_ASSERT(comp, "Should be called only within a compilation");

   bool validated = false;
   bool isPrimArray = TR_J9ServerVM::isPrimitiveArray(classPointer);

   if (comp->getOption(TR_UseSymbolValidationManager))
      {
      SVM_ASSERT_ALREADY_VALIDATED(comp->getSymbolValidationManager(), classPointer);
      validated = true;
      }
   else
      {
      if (((TR_ResolvedRelocatableJ9JITaaSServerMethod *) comp->getCurrentMethod())->validateArbitraryClass(comp, (J9Class *) classPointer))
         validated = true;
      }

   if (validated)
      return isPrimArray;
   else
      return false;
   }

bool
TR_J9SharedCacheServerVM::isReferenceArray(TR_OpaqueClassBlock *classPointer)
   {
   TR::Compilation* comp = _compInfoPT->getCompilation();
   TR_ASSERT(comp, "Should be called only within a compilation");

   bool validated = false;
   bool isRefArray = TR_J9ServerVM::isReferenceArray(classPointer);

   if (comp->getOption(TR_UseSymbolValidationManager))
      {
      SVM_ASSERT_ALREADY_VALIDATED(comp->getSymbolValidationManager(), classPointer);
      validated = true;
      }
   else
      {
      if (((TR_ResolvedRelocatableJ9JITaaSServerMethod *) comp->getCurrentMethod())->validateArbitraryClass(comp, (J9Class *) classPointer))
         validated = true;
      }

   if (validated)
      return isRefArray;
   else
      return false;
   }

TR_OpaqueMethodBlock *
TR_J9SharedCacheServerVM::getInlinedCallSiteMethod(TR_InlinedCallSite *ics)
   {
   return (TR_OpaqueMethodBlock *)((TR_AOTMethodInfo *)(ics->_vmMethodInfo))->resolvedMethod->getPersistentIdentifier();
   }

uintptrj_t
TR_J9SharedCacheServerVM::getClassDepthAndFlagsValue(TR_OpaqueClassBlock * classPointer)
   {
   TR::Compilation* comp = _compInfoPT->getCompilation();
   TR_ASSERT(comp, "Should be called only within a compilation");

   bool validated = false;
   uintptrj_t classDepthFlags = TR_J9ServerVM::getClassDepthAndFlagsValue(classPointer);

   if (comp->getOption(TR_UseSymbolValidationManager))
      {
      SVM_ASSERT_ALREADY_VALIDATED(comp->getSymbolValidationManager(), classPointer);
      validated = true;
      }
   else
      {
      if (((TR_ResolvedRelocatableJ9JITaaSServerMethod *) comp->getCurrentMethod())->validateArbitraryClass(comp, (J9Class *) classPointer))
         validated = true;
      }

   if (validated)
      return classDepthFlags;
   else
      return 0;
   }

bool
TR_J9SharedCacheServerVM::isClassVisible(TR_OpaqueClassBlock * sourceClass, TR_OpaqueClassBlock * destClass)
   {
   TR::Compilation *comp = _compInfoPT->getCompilation();
   TR_ASSERT(comp, "Should be called only within a compilation");

   bool validated = false;

   if (comp->getOption(TR_UseSymbolValidationManager))
      {
      TR::SymbolValidationManager *svm = comp->getSymbolValidationManager();
      SVM_ASSERT_ALREADY_VALIDATED(svm, sourceClass);
      SVM_ASSERT_ALREADY_VALIDATED(svm, destClass);
      validated = true;
      }
   else
      {
      validated = ((TR_ResolvedRelocatableJ9JITaaSServerMethod *) comp->getCurrentMethod())->validateArbitraryClass(comp, (J9Class *) sourceClass) &&
                  ((TR_ResolvedRelocatableJ9JITaaSServerMethod *) comp->getCurrentMethod())->validateArbitraryClass(comp, (J9Class *) destClass);
      }

   return (validated ? TR_J9ServerVM::isClassVisible(sourceClass, destClass) : false);
   }

TR_OpaqueClassBlock *
TR_J9SharedCacheServerVM::getClassOfMethod(TR_OpaqueMethodBlock *method)
   {
   TR::Compilation *comp = _compInfoPT->getCompilation();
   TR_ASSERT(comp, "Should be called only within a compilation");

   TR_OpaqueClassBlock *classPointer = TR_J9ServerVM::getClassOfMethod(method);

   bool validated = false;

   if (comp->getOption(TR_UseSymbolValidationManager))
      {
      SVM_ASSERT_ALREADY_VALIDATED(comp->getSymbolValidationManager(), classPointer);
      validated = true;
      }
   else
      {
      validated = ((TR_ResolvedRelocatableJ9JITaaSServerMethod *) comp->getCurrentMethod())->validateArbitraryClass(comp, (J9Class *) classPointer);
      }

   return (validated ? classPointer : NULL);
   }

J9Class *
TR_J9SharedCacheServerVM::getClassForAllocationInlining(TR::Compilation *comp, TR::SymbolReference *classSymRef)
   {
   bool returnClassForAOT = true;
   if (!classSymRef->isUnresolved())
      return TR_J9VM::getClassForAllocationInlining(comp, classSymRef);
   else
      return (J9Class *) classSymRef->getOwningMethod(comp)->getClassFromConstantPool(comp, classSymRef->getCPIndex(), returnClassForAOT);
   }

// Multiple codeCache support
TR::CodeCache *
TR_J9SharedCacheServerVM::getDesignatedCodeCache(TR::Compilation *comp)
   {
   int32_t numReserved = 0;
   int32_t compThreadID = comp ? comp->getCompThreadID() : -1;
   bool hadClassUnloadMonitor = false;
   bool hadVMAccess = releaseClassUnloadMonitorAndAcquireVMaccessIfNeeded(comp, &hadClassUnloadMonitor);
   TR::CodeCache * codeCache = TR::CodeCacheManager::instance()->reserveCodeCache(true, 0, compThreadID, &numReserved);
   acquireClassUnloadMonitorAndReleaseVMAccessIfNeeded(comp, hadVMAccess, hadClassUnloadMonitor);
   // For AOT we need some alignment
   if (codeCache)
      {
      codeCache->alignWarmCodeAlloc(_jitConfig->codeCacheAlignment - 1);

      // For AOT we must install the beginning of the code cache
      comp->setRelocatableMethodCodeStart((uint32_t *)codeCache->getWarmCodeAlloc());
      }
   else
      {
      // If this is a temporary condition due to all code caches being reserved for the moment
      // we should retry this compilation
      if (!(jitConfig->runtimeFlags & J9JIT_CODE_CACHE_FULL) &&
          (numReserved > 0) &&
          comp)
         {
         comp->failCompilation<TR::RecoverableCodeCacheError>("Cannot reserve code cache");
         }
      }

   return codeCache;
   }

TR_OpaqueClassBlock *
TR_J9SharedCacheServerVM::getComponentClassFromArrayClass(TR_OpaqueClassBlock * arrayClass)
   {
   TR::Compilation* comp = _compInfoPT->getCompilation();
   TR_ASSERT(comp, "Should be called only within a compilation");

   bool validated = false;
   TR_OpaqueClassBlock *componentClass = TR_J9ServerVM::getComponentClassFromArrayClass(arrayClass);

   if (comp->getOption(TR_UseSymbolValidationManager))
      {
      SVM_ASSERT_ALREADY_VALIDATED(comp->getSymbolValidationManager(), componentClass);
      validated = true;
      }
   else
      {
      if (((TR_ResolvedRelocatableJ9JITaaSServerMethod *) comp->getCurrentMethod())->validateArbitraryClass(comp, (J9Class *) arrayClass))
         validated = true;
      }

   if (validated)
      return componentClass;
   else
      return NULL;
   }

TR_OpaqueClassBlock *
TR_J9SharedCacheServerVM::getArrayClassFromComponentClass(TR_OpaqueClassBlock * componentClass)
   {
   TR::Compilation* comp = _compInfoPT->getCompilation();
   TR_ASSERT(comp, "Should be called only within a compilation");

   bool validated = false;
   TR_OpaqueClassBlock *arrayClass = TR_J9ServerVM::getArrayClassFromComponentClass(componentClass);

   if (comp->getOption(TR_UseSymbolValidationManager))
      {
      validated = comp->getSymbolValidationManager()->addArrayClassFromComponentClassRecord(arrayClass, componentClass);
      }
   else
      {
      if (((TR_ResolvedRelocatableJ9JITaaSServerMethod *) comp->getCurrentMethod())->validateArbitraryClass(comp, (J9Class *) componentClass))
         validated = true;
      }

   if (validated)
      return arrayClass;
   else
      return NULL;
   }

TR_OpaqueClassBlock *
TR_J9SharedCacheServerVM::getLeafComponentClassFromArrayClass(TR_OpaqueClassBlock * arrayClass)
   {
   TR::Compilation* comp = _compInfoPT->getCompilation();
   TR_ASSERT(comp, "Should be called only within a compilation");

   bool validated = false;
   TR_OpaqueClassBlock *leafComponent = TR_J9ServerVM::getLeafComponentClassFromArrayClass(arrayClass);

   if (comp->getOption(TR_UseSymbolValidationManager))
      {
      SVM_ASSERT_ALREADY_VALIDATED(comp->getSymbolValidationManager(), leafComponent);
      validated = true;
      }
   else
      {
      if (((TR_ResolvedRelocatableJ9JITaaSServerMethod *) comp->getCurrentMethod())->validateArbitraryClass(comp, (J9Class *) arrayClass))
         validated = true;
      }

   if (validated)
      return leafComponent;
   else
      return NULL;
   }

TR_OpaqueClassBlock *
TR_J9SharedCacheServerVM::getBaseComponentClass(TR_OpaqueClassBlock * classPointer, int32_t & numDims)
   {
   TR::Compilation* comp = _compInfoPT->getCompilation();
   TR_ASSERT(comp, "Should be called only within a compilation");

   bool validated = false;
   TR_OpaqueClassBlock *baseComponent = TR_J9ServerVM::getBaseComponentClass(classPointer, numDims);

   if (comp->getOption(TR_UseSymbolValidationManager))
      {
      SVM_ASSERT_ALREADY_VALIDATED(comp->getSymbolValidationManager(), baseComponent);
      validated = true;
      }
   else
      {
      if (((TR_ResolvedRelocatableJ9JITaaSServerMethod *) comp->getCurrentMethod())->validateArbitraryClass(comp, (J9Class *) classPointer))
         validated = true;
      }

   if (validated)
      return baseComponent;
   else
      return classPointer;  // not sure about this return value, but it's what we used to return before we got "smart"
   }

TR_OpaqueClassBlock *
TR_J9SharedCacheServerVM::getClassFromNewArrayType(int32_t arrayType)
   {
   TR::Compilation *comp = _compInfoPT->getCompilation();
   if (comp && comp->getOption(TR_UseSymbolValidationManager))
      return TR_J9ServerVM::getClassFromNewArrayType(arrayType);
   return NULL;
   }
