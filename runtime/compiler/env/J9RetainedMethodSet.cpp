/*******************************************************************************
 * Copyright IBM Corp. and others 2024
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

#include "env/J9RetainedMethodSet.hpp"

#include "j9nonbuilder.h"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/ClassTableCriticalSection.hpp"
#include "env/StackMemoryRegion.hpp"
#include "ilgen/J9ByteCode.hpp"
#include "ilgen/J9ByteCodeIterator.hpp"
#include "infra/CriticalSection.hpp"
#include "infra/Monitor.hpp"
#include "infra/MonitorTable.hpp"
#include "infra/TRlist.hpp"
#include "infra/vector.hpp"

#if defined(J9VM_OPT_JITSERVER)
#include "env/j9methodServer.hpp"
#include "net/ServerStream.hpp"
#endif

static J9Class *
definingJ9Class(TR_ResolvedMethod *method)
   {
   return reinterpret_cast<J9Class*>(method->classOfMethod());
   }

static void
traceMethod(TR::Compilation *comp, TR_ResolvedMethod *method)
   {
   traceMsg(
      comp,
      "%p %.*s.%.*s%.*s in class %p",
      method->getPersistentIdentifier(),
      method->classNameLength(),
      method->classNameChars(),
      method->nameLength(),
      method->nameChars(),
      method->signatureLength(),
      method->signatureChars(),
      method->containingClass());
   }

J9::RetainedMethodSet::RetainedMethodSet(
   TR::Compilation *comp,
   TR_ResolvedMethod *method,
   J9::RetainedMethodSet *parent)
   : OMR::RetainedMethodSet(comp, method, parent)
   , _loaders(comp->trMemory()->heapMemoryRegion())
   , _anonClasses(comp->trMemory()->heapMemoryRegion())
   {
#if defined(J9VM_OPT_JITSERVER)
   if (comp->isOutOfProcessCompilation())
      {
      _remoteMirror = NULL;
      }
   else if (comp->isRemoteCompilation())
      {
      _scanLog = NULL;
      }
#endif

   if (comp->getOption(TR_TraceRetainedMethods))
      {
      traceMsg(comp, "RetainedMethodSet %p: created with parent=%p, method=", this, parent);
      traceMethod(comp, method);
      traceMsg(comp, "\n");
      }
   }

J9::RetainedMethodSet *
J9::RetainedMethodSet::create(
   TR::Compilation *comp,
   TR_ResolvedMethod *method
#if defined(J9VM_OPT_JITSERVER)
   , ScanLog *scanLog
#endif
   )
   {
   TR::Region &region = comp->trMemory()->heapMemoryRegion();
   auto *result = new (region) J9::RetainedMethodSet(comp, method, NULL);

#if defined(J9VM_OPT_JITSERVER)
   if (comp->isRemoteCompilation())
      {
      result->initWithScanLog(scanLog);
      }
   else
#endif
      {
      result->init();
      }

   return result;
   }

J9::RetainedMethodSet *
J9::RetainedMethodSet::createChild(
   TR_ResolvedMethod *method
#if defined(J9VM_OPT_JITSERVER)
   , ScanLog *scanLog
#endif
   )
   {
   TR::Region &region = comp()->trMemory()->heapMemoryRegion();
   auto *result = new (region) J9::RetainedMethodSet(comp(), method, this);

#if defined(J9VM_OPT_JITSERVER)
   if (comp()->isRemoteCompilation())
      {
      result->initWithScanLog(scanLog);
      }
   else
#endif
      {
      result->init();
      }

   return result;
   }

J9::RetainedMethodSet *
J9::RetainedMethodSet::withKeepalivesAttested()
   {
   J9::RetainedMethodSet *result = createChild(method());
   auto keepalives = keepaliveMethods();
   TR_ResolvedMethod *m = NULL;
   while (keepalives.next(&m))
      {
      result->attestWillRemainLoaded(m);
      }

   return result;
   }

static void
traceNamedLoader(
   TR::Compilation *comp,
   J9::RetainedMethodSet *s,
   const char *name,
   J9ClassLoader *loader)
   {
   traceMsg(comp, "RetainedMethodSet %p: %s=%p\n", s, name, loader);
   }

void
J9::RetainedMethodSet::init()
   {
#if defined(J9VM_OPT_JITSERVER)
   if (comp()->isOutOfProcessCompilation())
      {
      void *remoteParent = parent() == NULL ? NULL : parent()->_remoteMirror;
      void *remoteMethod =
         static_cast<TR_ResolvedJ9JITServerMethod*>(method())->getRemoteMirror();

      JITServer::ServerStream *stream = comp()->getStream();
      stream->write(
         JITServer::MessageType::RetainedMethodSet_createMirror, remoteParent, remoteMethod);

      auto recv = stream->read<void*, J9Class*, std::vector<J9ClassLoader*>>();
      _remoteMirror = std::get<0>(recv);

      if (comp()->getOption(TR_TraceRetainedMethods))
         {
         traceMsg(
            comp(),
            "RetainedMethodSet %p: remote mirror %p\n",
            this,
            _remoteMirror);
         }

      // No need to "log" added items here, since we're on the server.
      J9Class *anonClass = std::get<1>(recv);
      if (anonClass != NULL)
         {
         _anonClasses.insert(anonClass);
         }

      auto &loaders = std::get<2>(recv);
      _loaders.insert(loaders.begin(), loaders.end());

      return;
      }
#endif

   if (parent() == NULL)
      {
      J9JavaVM *vm = comp()->fej9()->vmThread()->javaVM;
      addPermanentLoader(vm->systemClassLoader, "system");
      addPermanentLoader(vm->extensionClassLoader, "extension");
      addPermanentLoader(vm->applicationClassLoader, "application");
      }

   attestWillRemainLoaded(method());
   }

void
J9::RetainedMethodSet::addPermanentLoader(J9ClassLoader *loader, const char *name)
   {
   bool trace = comp()->getOption(TR_TraceRetainedMethods);
   if (loader == NULL)
      {
      if (trace)
         {
         traceMsg(comp(), "RetainedMethodSet %p: no %s loader\n", this, name);
         }
      }
   else
      {
      if (trace)
         {
         traceMsg(comp(), "RetainedMethodSet %p: add %s loader %p\n", this, name, loader);
         }

      _loaders.insert(loader);
#if defined(J9VM_OPT_JITSERVER)
      logAddedLoader(loader);
#endif
      }
   }

template <typename T>
static T loadBc(
   TR_ResolvedMethod *m,
   uint8_t *bcStart,
   uint32_t bcSize,
   uint32_t bcIndex,
   uint32_t offset = 0,
   const char *instrName = "unknown instruction")
   {
   TR_ASSERT_FATAL(
      bcIndex + offset < bcSize && bcIndex + offset + sizeof(T) <= bcSize,
      "bc index %d+%d out of range (%d) for %d bytes in %s within %.*s.%.*s%.*s",
      bcIndex,
      offset,
      bcSize,
      (int32_t)sizeof(T),
      instrName,
      m->classNameLength(),
      m->classNameChars(),
      m->nameLength(),
      m->nameChars(),
      m->signatureLength(),
      m->signatureChars());

   return *(T*)(bcStart + (bcIndex + offset));
   }

void
J9::RetainedMethodSet::attestLinkedCalleeWillRemainLoaded(TR_ByteCodeInfo bci)
   {
#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
   TR_ResolvedMethod *caller =
      bci.getCallerIndex() == -1
      ? comp()->getMethodBeingCompiled()
      : comp()->getInlinedResolvedMethod(bci.getCallerIndex());

   if (caller->isNewInstanceImplThunk())
      {
      // A typical newInstance thunk contains exactly one call, and it doesn't
      // occur in the bytecode. In fact, there will appear to be bytecode, but
      // it won't correspond to the generated IL at all (unless the class for
      // newInstance isn't suitable, e.g. no default constructor).
      return;
      }

   uint8_t *bcStart = caller->bytecodeStart();
   uint32_t bcSize = caller->maxBytecodeIndex();
   uint32_t bcIndex = bci.getByteCodeIndex();

   TR_J9ByteCode bcOp =
      TR_J9ByteCodeIterator::convertOpCodeToByteCodeEnum(
         loadBc<uint8_t>(caller, bcStart, bcSize, bcIndex));

   TR_ResolvedMethod *linkedCallee = NULL;

   switch (bcOp)
      {
      case J9BCinvokevirtual:
      case J9BCinvokeinterface:
      case J9BCinvokespecial:
      case J9BCinvokespecialsplit:
      case J9BCinvokestatic:
      case J9BCinvokestaticsplit:
         // We should almost always have already found the loader that defines
         // the named callee either in the permanent loaders or by following
         // the outliving loaders sets, so there's no need to inspect these
         // call sites.
         break;

      case J9BCinvokedynamic:
      case J9BCinvokehandle:
         {
         // The callee here will usually (or maybe even always) be an anonymous
         // class. Make sure to take note of it.

         if (comp()->compileRelocatableCode())
            {
            break;
            }

         uintptr_t *invokeCacheArray = NULL;
         if (bcOp == J9BCinvokehandle)
            {
            uint16_t cpIndex = loadBc<uint16_t>(
               caller, bcStart, bcSize, bcIndex, 1, "invokehandle");

            if (caller->isUnresolvedMethodTypeTableEntry(cpIndex))
               {
               break;
               }

            invokeCacheArray =
               (uintptr_t*)caller->methodTypeTableEntryAddress(cpIndex);
            }
         else
            {
            uint16_t callSiteIndex = loadBc<uint16_t>(
               caller, bcStart, bcSize, bcIndex, 1, "invokedynamic");

            if (caller->isUnresolvedCallSiteTableEntry(callSiteIndex))
               {
               break;
               }

            invokeCacheArray =
               (uintptr_t*)caller->callSiteTableEntryAddress(callSiteIndex);

            // We get an exception instead of an array if resolution fails.
            if (!comp()->fej9()->isInvokeCacheEntryAnArray(invokeCacheArray))
               {
               break;
               }
            }

         linkedCallee =
            comp()->fej9()->targetMethodFromInvokeCacheArrayMemberNameObj(
               comp(), caller, invokeCacheArray);

         break;
         }

      default:
         TR_ASSERT_FATAL(
            false,
            "unexpected invoke bytecode %d at %d in %.*s.%.*s%.*s",
            bcOp,
            bcIndex,
            caller->classNameLength(),
            caller->classNameChars(),
            caller->nameLength(),
            caller->nameChars(),
            caller->signatureLength(),
            caller->signatureChars());

         break;
      }

   if (linkedCallee != NULL)
      {
      attestWillRemainLoaded(linkedCallee);
      }
#endif
   }

void
J9::RetainedMethodSet::attestWillRemainLoaded(TR_ResolvedMethod *method)
   {
   if (comp()->getOption(TR_TraceRetainedMethods))
      {
      traceMsg(comp(), "RetainedMethodSet %p: attest method ", this);
      traceMethod(comp(), method);
      traceMsg(comp(), "\n");
      }

   scan(definingJ9Class(method));
   }

void
J9::RetainedMethodSet::scan(J9Class *clazz)
   {
   J9ClassLoader *loader = getLoader(clazz);
   bool loaderWillRemain = willRemainLoaded(loader);
   bool isAnonymous = isAnonymousClass(clazz);
   bool anonClassWillRemain = isAnonymous && willAnonymousClassRemainLoaded(clazz);

#if defined(J9VM_OPT_JITSERVER)
   if (comp()->isOutOfProcessCompilation())
      {
      if (loaderWillRemain && (!isAnonymous || anonClassWillRemain))
         {
         return; // save a round trip if nothing will happen
         }

      JITServer::ServerStream *stream = comp()->getStream();
      stream->write(
         JITServer::MessageType::RetainedMethodSet_scan, _remoteMirror, clazz);

      auto recv = stream->read<J9Class*, std::vector<J9ClassLoader*>>();

      // No need to "log" added items here, since we're on the server.
      J9Class *anonClass = std::get<0>(recv);
      if (anonClass != NULL)
         {
         _anonClasses.insert(anonClass);
         }

      auto &loaders = std::get<1>(recv);
      _loaders.insert(loaders.begin(), loaders.end());

      return;
      }
#endif

   if (isAnonymous && !anonClassWillRemain)
      {
      if (comp()->getOption(TR_TraceRetainedMethods))
         {
         traceMsg(
            comp(),
            "RetainedMethodSet %p: add anonymous class %p\n",
            this,
            clazz);
         }

      _anonClasses.insert(clazz);
#if defined(J9VM_OPT_JITSERVER)
      if (comp()->isRemoteCompilation() && _scanLog != NULL)
         {
         TR_ASSERT_FATAL(
            _scanLog->_addedAnonClass == NULL,
            "RetainedMethodSet %p: multiple anonymous classes in the same scan",
            this);

         _scanLog->_addedAnonClass = clazz;
         }
#endif
      }

   // Scan the class loader graph starting from the loader of clazz.

   if (comp()->getOption(TR_TraceRetainedMethods))
      {
      traceMsg(
         comp(),
         "RetainedMethodSet %p: class %p has loader %p\n",
         this,
         clazz,
         loader);
      }

   if (loaderWillRemain)
      {
      return;
      }

   if (comp()->getOption(TR_TraceRetainedMethods))
      {
      traceMsg(comp(), "RetainedMethodSet %p: add loader %p\n", this, loader);
      }

   _loaders.insert(loader);
#if defined(J9VM_OPT_JITSERVER)
   logAddedLoader(loader);
#endif

   TR::StackMemoryRegion stackRegion(*comp()->trMemory());

   TR::vector<J9ClassLoader*, TR::Region&> outlivingLoadersCopy(stackRegion);
   outlivingLoadersCopy.reserve(16);

   TR::list<J9ClassLoader*, TR::Region&> queue(stackRegion);
   queue.push_back(loader);
   while (!queue.empty())
      {
      loader = queue.front();
      queue.pop_front();

      outlivingLoadersCopy.clear();

      // Go ahead and read loader->outlivingLoaders without taking the mutex.
      // The pointer value changes atomically, and in the likely case that we
      // have an empty or singleton set, it can be decoded without locking.
      // It's possible to miss an update this way, if another thread is
      // concurrently adding the first or second entry, but even if we did take
      // the mutex, we just as well might have taken it just before the other
      // thread and missed the update anyway. This is fine because the set is
      // not required to be exhaustive.
      void *outlivingLoaders = loader->outlivingLoaders;
      if (outlivingLoaders == NULL)
         {
         // Empty set.
         }
      else if (((uintptr_t)outlivingLoaders & J9CLASSLOADER_OUTLIVING_LOADERS_SINGLE_TAG) != 0)
         {
         // Singleton set.
         J9ClassLoader *outlivingLoader = reinterpret_cast<J9ClassLoader*>(
            (uintptr_t)outlivingLoaders & ~(uintptr_t)J9CLASSLOADER_OUTLIVING_LOADERS_SINGLE_TAG);

         outlivingLoadersCopy.push_back(outlivingLoader);
         }
      else
         {
         // Hash table. We need the mutex in order to iterate over it.
         TR::ClassTableCriticalSection criticalSection(comp()->fej9());

         // Once loader->outlivingLoaders is set to point to a hash table, it
         // won't be modified again. Instead, the hash table that it points to
         // will be mutated, and only to add new entries.
         void *repeatLoadedOutlivingLoaders = loader->outlivingLoaders;
         TR_ASSERT_FATAL(
            repeatLoadedOutlivingLoaders == outlivingLoaders,
            "unexpected change to outlivingLoaders of J9ClassLoader %p: old %p, new %p",
            loader,
            outlivingLoaders,
            repeatLoadedOutlivingLoaders);

         J9HashTable *table = (J9HashTable*)outlivingLoaders;
         J9HashTableState state;
         J9ClassLoader **entry = (J9ClassLoader**)hashTableStartDo(table, &state);
         while (entry != NULL)
            {
            outlivingLoadersCopy.push_back(*entry);
            entry = (J9ClassLoader**)hashTableNextDo(&state);
            }
         }

      auto copyEnd = outlivingLoadersCopy.end();
      for (auto it = outlivingLoadersCopy.begin(); it != copyEnd; it++)
         {
         J9ClassLoader *outlivingLoader = *it;
         if (comp()->getOption(TR_TraceRetainedMethods))
            {
            traceMsg(
               comp(),
               "RetainedMethodSet %p: loader %p is outlived by loader %p\n",
               this,
               loader,
               outlivingLoader);
            }

         if (willRemainLoaded(outlivingLoader))
            {
            continue;
            }

         if (comp()->getOption(TR_TraceRetainedMethods))
            {
            traceMsg(
               comp(),
               "RetainedMethodSet %p: add loader %p\n",
               this,
               outlivingLoader);
            }

         _loaders.insert(outlivingLoader);
#if defined(J9VM_OPT_JITSERVER)
         logAddedLoader(outlivingLoader);
#endif

         queue.push_back(outlivingLoader);
         }
      }
   }

#if defined(J9VM_OPT_JITSERVER)
void
J9::RetainedMethodSet::scanForClient(J9Class *clazz, ScanLog *scanLog)
   {
   TR_ASSERT_FATAL(
      comp()->isRemoteCompilation(),
      "RetainedMethodSet %p: can only scanForClient on the client",
      this);

   TR_ASSERT_FATAL(
      scanLog != NULL, "RetainedMethodSet %p: scanForClient missing scanLog", this);

   _scanLog = scanLog;
   try
      {
      scan(clazz);
      _scanLog = NULL;
      }
   catch (...)
      {
      _scanLog = NULL;
      throw;
      }
   }
#endif

bool
J9::RetainedMethodSet::willRemainLoaded(TR_ResolvedMethod *method)
   {
   return willRemainLoaded(definingJ9Class(method));
   }

bool
J9::RetainedMethodSet::willRemainLoaded(J9Class *clazz)
   {
   if (isAnonymousClass(clazz))
      {
      return willAnonymousClassRemainLoaded(clazz);
      }
   else
      {
      return willRemainLoaded(getLoader(clazz));
      }
   }

bool
J9::RetainedMethodSet::willRemainLoaded(J9ClassLoader *loader)
   {
   for (J9::RetainedMethodSet *s = this; s != NULL; s = s->parent())
      {
      if (s->_loaders.count(loader) != 0)
         {
         return true;
         }
      }

   return false;
   }

bool
J9::RetainedMethodSet::willAnonymousClassRemainLoaded(J9Class *clazz)
   {
   TR_ASSERT_FATAL(isAnonymousClass(clazz), "not anonymous: %p", clazz);

   for (J9::RetainedMethodSet *s = this; s != NULL; s = s->parent())
      {
      if (s->_anonClasses.count(clazz) != 0)
         {
         return true;
         }
      }

   return false;
   }

void
J9::RetainedMethodSet::keepalive()
   {
   OMR::RetainedMethodSet::keepalive();

#if defined(J9VM_OPT_JITSERVER)
   if (comp()->isOutOfProcessCompilation())
      {
      // Repeat keealive on the client to keep the remote mirrors in sync.
      JITServer::ServerStream *stream = comp()->getStream();
      stream->write(
         JITServer::MessageType::RetainedMethodSet_keepalive, _remoteMirror);

      stream->read<JITServer::Void>();
      }
#endif
   }

void
J9::RetainedMethodSet::bond()
   {
   OMR::RetainedMethodSet::bond();

#if defined(J9VM_OPT_JITSERVER)
   if (comp()->isOutOfProcessCompilation())
      {
      // Repeat bond on the client to keep the remote mirrors in sync.
      JITServer::ServerStream *stream = comp()->getStream();
      stream->write(
         JITServer::MessageType::RetainedMethodSet_bond, _remoteMirror);

      stream->read<JITServer::Void>();
      }
#endif
   }

J9ClassLoader *
J9::RetainedMethodSet::getLoader(J9Class *clazz)
   {
   auto *c = TR::Compiler->cls.convertClassPtrToClassOffset(clazz);
   return (J9ClassLoader*)comp()->fej9()->getClassLoader(c);
   }

bool
J9::RetainedMethodSet::isAnonymousClass(J9Class *clazz)
   {
   auto *c = TR::Compiler->cls.convertClassPtrToClassOffset(clazz);
   return comp()->fej9()->isAnonymousClass(c);
   }

void
J9::RetainedMethodSet::mergeIntoParent()
   {
   // No need to "log" added items here, since the entire mergeIntoParent()
   // operation will be repeated on both server and client (because bond() will
   // be repeated on both server and client), and the sets will be in sync
   // beforehand, so they will remain in sync afterward.
   parent()->_loaders.insert(_loaders.begin(), _loaders.end());
   parent()->_anonClasses.insert(_anonClasses.begin(), _anonClasses.end());
   _loaders.clear();
   _anonClasses.clear();
   }

void *
J9::RetainedMethodSet::unloadingKey(TR_ResolvedMethod *method)
   {
   J9Class *c = definingJ9Class(method);
   return isAnonymousClass(c) ? (void*)c : (void*)getLoader(c);
   }

#if defined(J9VM_OPT_JITSERVER)
void
J9::RetainedMethodSet::initWithScanLog(ScanLog *scanLog)
   {
   TR_ASSERT_FATAL(comp()->isRemoteCompilation(), "client-only path");

   // Usually scanLog is required because we should be creating a remote mirror
   // of a J9::RetainedMethodSet that was created on the server. However, in an
   // AOT load, we need to be able to analyze the inlining table independently
   // of what the server may have been doing.
   TR_ASSERT_FATAL(
      scanLog != NULL
         || comp()->compileRelocatableCode()
         || comp()->isDeserializedAOTMethod(),
      "RetainedMethodSet %p: init missing scanLog on client (non-AOT)",
      this);

   _scanLog = scanLog;
   try
      {
      init();
      _scanLog = NULL;
      }
   catch (...)
      {
      _scanLog = NULL;
      throw;
      }
   }

void
J9::RetainedMethodSet::logAddedLoader(J9ClassLoader *loader)
   {
   if (comp()->isRemoteCompilation() && _scanLog != NULL)
      {
      _scanLog->_addedLoaders.push_back(loader);
      }
   }
#endif
