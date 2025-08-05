/*******************************************************************************
 * Copyright IBM Corp. and others 2025
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
#include "env/ClassLoaderTable.hpp"
#include "env/ClassTableCriticalSection.hpp"
#include "env/J9ConstProvenanceGraph.hpp"
#include "env/StackMemoryRegion.hpp"
#include "ilgen/J9ByteCode.hpp"
#include "ilgen/J9ByteCodeIterator.hpp"
#include "infra/CriticalSection.hpp"
#include "infra/Monitor.hpp"
#include "infra/MonitorTable.hpp"
#include "infra/TRlist.hpp"
#include "infra/vector.hpp"
#include "runtime/MethodMetaData.h"

#if defined(J9VM_OPT_JITSERVER)
#include "env/j9methodServer.hpp"
#include "env/RepeatRetainedMethodsAnalysis.hpp"
#endif

static J9Class *
definingJ9Class(TR_ResolvedMethod *method)
   {
   return reinterpret_cast<J9Class*>(method->classOfMethod());
   }

class J9::RetainedMethodSet::InliningTable
   {
   public:
   InliningTable(TR::Compilation *comp) : _comp(comp) {}
   virtual TR_ResolvedMethod *inlinedResolvedMethod(uint32_t i) = 0;

   protected:
   TR::Compilation * const _comp;
   };

class J9::RetainedMethodSet::CompInliningTable
   : public J9::RetainedMethodSet::InliningTable
   {
   public:
   CompInliningTable(TR::Compilation *comp) : InliningTable(comp) {}

   virtual TR_ResolvedMethod *inlinedResolvedMethod(uint32_t i)
      {
      TR_ASSERT_FATAL(
         i < numInlinedSites(),
         "out of bounds index %u >= %u",
         i,
         numInlinedSites());

      return _comp->getInlinedResolvedMethod(i);
      }

   private:
   uint32_t numInlinedSites() { return _comp->getNumInlinedCallSites(); }
   };

class J9::RetainedMethodSet::VectorInliningTable
   : public J9::RetainedMethodSet::InliningTable
   {
   public:
   VectorInliningTable(
      TR::Compilation *comp,
      const TR::vector<J9::ResolvedInlinedCallSite, TR::Region&> &vec)
      : InliningTable(comp), _vec(vec)
      {}

   virtual TR_ResolvedMethod *inlinedResolvedMethod(uint32_t i)
      {
      TR_ASSERT_FATAL(
         i < _vec.size(),
         "out of bounds index %u >= %u\n",
         i,
         (uint32_t)_vec.size());

      return _vec[i]._method;
      }

   private:
   const TR::vector<J9::ResolvedInlinedCallSite, TR::Region&> &_vec;
   };

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
   J9::RetainedMethodSet *parent,
   InliningTable *inliningTable)
   : OMR::RetainedMethodSet(comp, method, parent)
   , _loaders(comp->trMemory()->heapMemoryRegion())
   , _anonClasses(comp->trMemory()->heapMemoryRegion())
   , _inliningTable(inliningTable)
   {
   if (comp->getOption(TR_TraceRetainedMethods))
      {
      traceMsg(comp, "RetainedMethodSet %p: created with parent=%p, method=", this, parent);
      traceMethod(comp, method);
      traceMsg(comp, "\n");
      }
   }

J9::RetainedMethodSet *
J9::RetainedMethodSet::create(TR::Compilation *comp, TR_ResolvedMethod *method)
   {
   TR::Region &region = comp->trMemory()->heapMemoryRegion();
   auto *inlTab = new (region) CompInliningTable(comp);
   auto *result = new (region) J9::RetainedMethodSet(comp, method, NULL, inlTab);
   result->init();
   return result;
   }

J9::RetainedMethodSet *
J9::RetainedMethodSet::create(
   TR::Compilation *comp,
   TR_ResolvedMethod *method,
   const TR::vector<J9::ResolvedInlinedCallSite, TR::Region&> &inliningTable)
   {
   TR::Region &region = comp->trMemory()->heapMemoryRegion();
   auto *inlTab = new (region) VectorInliningTable(comp, inliningTable);
   auto *result = new (region) J9::RetainedMethodSet(comp, method, NULL, inlTab);
   result->init();
   return result;
   }

const TR::vector<J9::ResolvedInlinedCallSite, TR::Region&> &
J9::RetainedMethodSet::copyInliningTable(
   TR::Compilation *comp, J9JITExceptionTable *metadata)
   {
   auto &inliningTable = *new (comp->region())
      TR::vector<J9::ResolvedInlinedCallSite, TR::Region&>(comp->region());

   uint32_t n = getNumInlinedCallSites(metadata);
   inliningTable.reserve(n);
   for (uint32_t i = 0; i < n; i++)
      {
      auto *site = (TR_InlinedCallSite *)getInlinedCallSiteArrayElement(metadata, i);
      TR_ResolvedMethod *method = new (comp->trHeapMemory())
         TR_ResolvedJ9Method(site->_methodInfo, comp->fe(), comp->trMemory());

      J9::ResolvedInlinedCallSite resolvedSite = { method, site->_byteCodeInfo };
      inliningTable.push_back(resolvedSite);
      }

   return inliningTable;
   }

J9::RetainedMethodSet *
J9::RetainedMethodSet::createChild(TR_ResolvedMethod *method)
   {
   TR::Region &region = comp()->trMemory()->heapMemoryRegion();
   auto *result = new (region) J9::RetainedMethodSet(
      comp(), method, this, _inliningTable);
   result->init();
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
   if (parent() == NULL)
      {
      bool trace = comp()->getOption(TR_TraceRetainedMethods);

      auto permanentLoaders = comp()->permanentLoaders();
      auto end = permanentLoaders.end();
      for (auto it = permanentLoaders.begin(); it != end; it++)
         {
         J9ClassLoader *loader = *it;
         _loaders.insert(loader);
         if (trace)
            {
            traceMsg(
               comp(),
               "RetainedMethodSet %p: add permanent loader %p\n",
               this,
               loader);
            }
         }
      }

   attestWillRemainLoaded(method());
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

J9::RetainedMethodSet *
J9::RetainedMethodSet::withLinkedCalleeAttested(TR_ByteCodeInfo bci)
   {
   TR_ResolvedMethod *linkedCallee = NULL;

#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
   TR_ResolvedMethod *caller =
      bci.getCallerIndex() == -1
      ? comp()->getMethodBeingCompiled()
      : _inliningTable->inlinedResolvedMethod(bci.getCallerIndex());

   if (caller->isNewInstanceImplThunk())
      {
      // A typical newInstance thunk contains exactly one call, and it doesn't
      // occur in the bytecode. In fact, there will appear to be bytecode, but
      // it won't correspond to the generated IL at all (unless the class for
      // newInstance isn't suitable, e.g. no default constructor).
      return this;
      }

   uint8_t *bcStart = caller->bytecodeStart();
   uint32_t bcSize = caller->maxBytecodeIndex();
   uint32_t bcIndex = bci.getByteCodeIndex();

   TR_J9ByteCode bcOp =
      TR_J9ByteCodeIterator::convertOpCodeToByteCodeEnum(
         loadBc<uint8_t>(caller, bcStart, bcSize, bcIndex));

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

         comp()->constProvenanceGraph()->addEdge(caller, linkedCallee);

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
#endif

   if (linkedCallee == NULL || willRemainLoaded(linkedCallee))
      {
      return this;
      }
   else
      {
      return createChild(linkedCallee);
      }
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
   if (isAnonymousClass(clazz) && !willAnonymousClassRemainLoaded(clazz))
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
      }

   // Scan the class loader graph starting from the loader of clazz.
   J9ClassLoader *loader = getLoader(clazz);
   if (comp()->getOption(TR_TraceRetainedMethods))
      {
      traceMsg(
         comp(),
         "RetainedMethodSet %p: class %p has loader %p\n",
         this,
         clazz,
         loader);
      }

   if (willRemainLoaded(loader))
      {
      return;
      }

   if (comp()->getOption(TR_TraceRetainedMethods))
      {
      traceMsg(comp(), "RetainedMethodSet %p: add loader %p\n", this, loader);
      }

   _loaders.insert(loader);
#if defined(J9VM_OPT_JITSERVER)
   if (comp()->isOutOfProcessCompilation())
      {
      // Scanning the graph of class loaders here would require us to query
      // the client. Just skip it instead and do a less precise analysis. The
      // client will repeat the analysis at/near the end of the compilation,
      // and when it does it will be able to scan the loader graph. The client's
      // results will be compatible with the server's results, i.e. the client's
      // set of bonds will be a subset of the server's, and likewise for the
      // set of keepalives.
      //
      // This does restrict inlining more than strictly necessary when
      // dontInlineUnloadableMethods is enabled. However, that option will not
      // be enabled often. It will only be used in the following cases:
      // - when recompiling after a JIT body has been invalidated by unloading,
      // - when the compilation can't be invalidated (e.g. newInstance thunk), and
      // - when the option has been explicitly set for debugging.
      //
      // So there isn't much point implementing a message for this.
      //
      return;
      }
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
      if (outlivingLoaders == NULL
          || outlivingLoaders == J9CLASSLOADER_OUTLIVING_LOADERS_PERMANENT)
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

         comp()->constProvenanceGraph()->addEdge(loader, outlivingLoader);

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
         queue.push_back(outlivingLoader);
         }
      }
   }

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
   keepalivesAndBonds()->_keepaliveLoaders.insert(
      _loaders.begin(), _loaders.end());
   keepalivesAndBonds()->_keepaliveAnonClasses.insert(
      _anonClasses.begin(), _anonClasses.end());
   }

void
J9::RetainedMethodSet::bond()
   {
   OMR::RetainedMethodSet::bond();
   keepalivesAndBonds()->_bondLoaders.insert(_loaders.begin(), _loaders.end());
   keepalivesAndBonds()->_bondAnonClasses.insert(
      _anonClasses.begin(), _anonClasses.end());
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

void *
J9::RetainedMethodSet::unloadingKey(TR_ResolvedMethod *method)
   {
   J9Class *c = definingJ9Class(method);
   return isAnonymousClass(c) ? (void*)c : (void*)getLoader(c);
   }

J9::RetainedMethodSet::KeepalivesAndBonds *
J9::RetainedMethodSet::createKeepalivesAndBonds()
   {
   return new (comp()->region()) KeepalivesAndBonds(comp()->region());
   }

J9::RetainedMethodSet::KeepalivesAndBonds::KeepalivesAndBonds(TR::Region &heapRegion)
   : OMR::RetainedMethodSet::KeepalivesAndBonds(heapRegion)
   , _keepaliveLoaders(heapRegion)
   , _keepaliveAnonClasses(heapRegion)
   , _bondLoaders(heapRegion)
   , _bondAnonClasses(heapRegion)
   {
   // empty
   }

J9::RetainedMethodSet *
J9::RetainedMethodSet::withKeepalivesAttested()
   {
   J9::RetainedMethodSet *result = createChild(method());
   KeepalivesAndBonds *kb = keepalivesAndBonds();
   result->_loaders.insert(kb->_keepaliveLoaders.begin(), kb->_keepaliveLoaders.end());
   result->_anonClasses.insert(kb->_keepaliveAnonClasses.begin(), kb->_keepaliveAnonClasses.end());

   // NOTE: We don't need to take account of the compilation's keepalive
   // classes here. They haven't been used to justify inlining.

   return result;
   }

J9::RetainedMethodSet *
J9::RetainedMethodSet::withBondsAttested()
   {
   J9::RetainedMethodSet *result = createChild(method());
   KeepalivesAndBonds *kb = keepalivesAndBonds();
   result->_loaders.insert(kb->_bondLoaders.begin(), kb->_bondLoaders.end());
   result->_anonClasses.insert(kb->_bondAnonClasses.begin(), kb->_bondAnonClasses.end());
   return result;
   }

#if defined(J9VM_OPT_JITSERVER)

void
J9::RepeatRetainedMethodsAnalysis::getDataForClient(
   TR::Compilation *comp,
   std::vector<InlinedSiteInfo> &inlinedSiteInfo,
   std::vector<TR_ResolvedMethod*> &keepaliveMethods,
   std::vector<TR_ResolvedMethod*> &bondMethods)
   {
   TR_ASSERT_FATAL(comp->isOutOfProcessCompilation(), "server only");

   inlinedSiteInfo.clear();
   keepaliveMethods.clear();
   bondMethods.clear();

   uint32_t numInlinedSites = comp->getNumInlinedCallSites();
   for (uint32_t i = 0; i < numInlinedSites; i++)
      {
      TR_ResolvedMethod *refinedMethod = comp->getInlinedCallSiteRefinedMethod(i);
      bool generatedKeepalive = comp->didInlinedSiteGenerateKeepalive(i);
      bool generatedBond = comp->didInlinedSiteGenerateBond(i);

      TR_ResolvedMethod *refinedMethodMirror = NULL;
      if (refinedMethod != NULL)
         {
         auto serverMethod = static_cast<TR_ResolvedJ9JITServerMethod*>(refinedMethod);
         refinedMethodMirror = serverMethod->getRemoteMirror();
         }

      if (refinedMethodMirror != NULL || generatedKeepalive || generatedBond)
         {
         InlinedSiteInfo info = {};
         info._inlinedSiteIndex = i;
         info._refinedMethod = refinedMethodMirror;
         info._generatedKeepalive = generatedKeepalive;
         info._generatedBond = generatedBond;
         inlinedSiteInfo.push_back(info);
         }
      }

   TR_ResolvedMethod *keepaliveMethod = NULL;
   auto keepaliveMethodsIter = comp->retainedMethods()->keepaliveMethods();
   while (keepaliveMethodsIter.next(&keepaliveMethod))
      {
      keepaliveMethods.push_back(
         static_cast<TR_ResolvedJ9JITServerMethod*>(keepaliveMethod)->getRemoteMirror());
      }

   TR_ResolvedMethod *bondMethod = NULL;
   auto bondMethodsIter = comp->retainedMethods()->bondMethods();
   while (bondMethodsIter.next(&bondMethod))
      {
      bondMethods.push_back(
         static_cast<TR_ResolvedJ9JITServerMethod*>(bondMethod)->getRemoteMirror());
      }
   }

static void
assertSubset(
   const char *what,
   TR::vector<void*, TR::Region&> clientKeys,
   TR::vector<void*, TR::Region&> serverKeys)
   {
   std::sort(clientKeys.begin(), clientKeys.end());
   std::sort(serverKeys.begin(), serverKeys.end());

   std::less<void*> ptrLt;
   auto sIt = serverKeys.begin();
   for (auto cIt = clientKeys.begin(); cIt != clientKeys.end(); cIt++)
      {
      while (sIt != serverKeys.end() && ptrLt(*sIt, *cIt))
         {
         sIt++;
         }

      bool match = sIt != serverKeys.end() && *sIt == *cIt;
      TR_ASSERT_FATAL(
         match,
         "client %s method key %p has no corresponding server key",
         what,
         *cIt);
      }
   }

// inliningTable must have been allocated in the compilation's heap region.
// Otherwise the resulting RetainedMethodSet will outlive it and end up with a
// dangling reference to it.
OMR::RetainedMethodSet *
J9::RepeatRetainedMethodsAnalysis::analyzeOnClient(
   TR::Compilation *comp,
   const TR::vector<J9::ResolvedInlinedCallSite, TR::Region&> &inliningTable,
   const std::vector<InlinedSiteInfo> &inlinedSiteInfo,
   const std::vector<TR_ResolvedMethod*> &serverKeepaliveMethods,
   const std::vector<TR_ResolvedMethod*> &serverBondMethods)
   {
   TR_ASSERT_FATAL(comp->isRemoteCompilation(), "client only");

   if (!comp->mustTrackRetainedMethods())
      {
      // Use the base implementation (no tracking).
      OMR::RetainedMethodSet *result = new (comp->region())
         OMR::RetainedMethodSet(comp, comp->getMethodBeingCompiled(), NULL);

      return result;
      }

   J9::RetainedMethodSet *root = J9::RetainedMethodSet::create(
      comp, comp->getMethodBeingCompiled(), inliningTable);

   uint32_t n = (uint32_t)inliningTable.size();
   TR::vector<J9::RetainedMethodSet*, TR::Region&> retainedMethods(comp->region());
   retainedMethods.reserve(n);
   retainedMethods.resize(n, NULL);

   auto infoIt = inlinedSiteInfo.begin();
   auto infoItEnd = inlinedSiteInfo.end();

   for (uint32_t siteIndex = 0; siteIndex < n; siteIndex++)
      {
      const InlinedSiteInfo *info = NULL;
      if (infoIt != infoItEnd && infoIt->_inlinedSiteIndex == siteIndex)
         {
         info = &*infoIt++;
         }

      const J9::ResolvedInlinedCallSite &inlinedSite = inliningTable[siteIndex];
      TR_ResolvedMethod *targetMethod = inlinedSite._method;
      TR_ByteCodeInfo bci = inlinedSite._bci;

      int32_t callerIndex = bci.getCallerIndex();
      J9::RetainedMethodSet *surroundingSet =
         callerIndex == -1 ? root : retainedMethods[callerIndex];

      surroundingSet = surroundingSet->withLinkedCalleeAttested(bci);

      J9::RetainedMethodSet *callSiteSet = surroundingSet;
      if (info != NULL
          && info->_refinedMethod != NULL
          && !surroundingSet->willRemainLoaded(info->_refinedMethod))
         {
         TR_ASSERT_FATAL(
            info->_generatedKeepalive,
            "client attempting to generate keepalive for inlined site %u, "
            "whose call site did not generate a corresponding keepalive on "
            "the server",
            siteIndex);

         callSiteSet = surroundingSet->createChild(info->_refinedMethod);
         callSiteSet->keepalive();
         }

      J9::RetainedMethodSet *callTargetSet = callSiteSet;
      if (!callSiteSet->willRemainLoaded(targetMethod))
         {
         TR_ASSERT_FATAL(
            info != NULL && info->_generatedBond,
            "client attempting to generate bond for inlined site %u, "
            "which did not generate a corresponding bond on the server",
            siteIndex);

         callTargetSet = callSiteSet->createChild(targetMethod);
         callTargetSet->bond();
         }

      retainedMethods[siteIndex] = callTargetSet;
      }

   // The inlinedSiteInfo must be sorted by inlined site index,
   // and it must contain no duplicate indices.
   TR_ASSERT_FATAL(
      infoIt == infoItEnd,
      "failed to reach the end of inlinedSiteInfo");

   // Check that the resulting sets of keepalive methods and bond methods
   // correspond properly with what the server had. The actual choice of
   // methods can differ, since if multiple call paths requested (say) bonds
   // for different methods defined by the same loader (or the same anonymous
   // class), then an arbitrary one would be kept and the others discarded.
   // But the loaders (or anonymous classes) for the methods found by this
   // repeat analysis must be a subset of those for the methods found on the
   // server.

   TR::vector<void*, TR::Region&> serverKeepaliveKeys(comp->region());
   for (auto it = serverKeepaliveMethods.begin();
        it != serverKeepaliveMethods.end();
        it++)
      {
      serverKeepaliveKeys.push_back(root->unloadingKey(*it));
      }

   TR::vector<void*, TR::Region&> clientKeepaliveKeys(comp->region());
   TR_ResolvedMethod *m = NULL;
   auto clientKeepaliveMethodsIter = root->keepaliveMethods();
   while (clientKeepaliveMethodsIter.next(&m))
      {
      clientKeepaliveKeys.push_back(root->unloadingKey(m));
      }

   assertSubset("keepalive", clientKeepaliveKeys, serverKeepaliveKeys);

   TR::vector<void*, TR::Region&> serverBondKeys(comp->region());
   for (auto it = serverBondMethods.begin(); it != serverBondMethods.end(); it++)
      {
      serverBondKeys.push_back(root->unloadingKey(*it));
      }

   TR::vector<void*, TR::Region&> clientBondKeys(comp->region());
   auto clientBondMethodsIter = root->bondMethods();
   while (clientBondMethodsIter.next(&m))
      {
      clientBondKeys.push_back(root->unloadingKey(m));
      }

   assertSubset("bond", clientBondKeys, serverBondKeys);

   return root;
   }

OMR::RetainedMethodSet *
J9::RepeatRetainedMethodsAnalysis::analyzeOnClient(
   TR::Compilation *comp,
   J9JITExceptionTable *metadata,
   const std::vector<InlinedSiteInfo> &inlinedSiteInfo,
   const std::vector<TR_ResolvedMethod*> &serverKeepaliveMethods,
   const std::vector<TR_ResolvedMethod*> &serverBondMethods)
   {
   // Copy the inlining table from metadata into a vector usable for the analysis.
   auto &inliningTable = J9::RetainedMethodSet::copyInliningTable(comp, metadata);
   return analyzeOnClient(
      comp,
      inliningTable,
      inlinedSiteInfo,
      serverKeepaliveMethods,
      serverBondMethods);
   }

#endif // defined(J9VM_OPT_JITSERVER)
