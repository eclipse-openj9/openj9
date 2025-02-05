/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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

#ifndef CLASSLOADERTABLE_INCL
#define CLASSLOADERTABLE_INCL

#include "env/TRMemory.hpp"
#include "infra/vector.hpp"

#define CLASSLOADERTABLE_SIZE 2053

struct J9ClassLoader;
class TR_J9SharedCache;
struct TR_ClassLoaderInfo;
namespace TR { class CompilationInfo; }

class TR_PersistentClassLoaderTable
   {
public:

   TR_PERSISTENT_ALLOC(TR_Memory::PersistentCHTable)
   TR_PersistentClassLoaderTable(TR_PersistentMemory *persistentMemory);

   void associateClassLoaderWithClass(J9VMThread *vmThread, void *loader, TR_OpaqueClassBlock *clazz);
   void *lookupClassChainAssociatedWithClassLoader(void *loader) const;
   void *lookupClassLoaderAssociatedWithClassChain(void *chain) const;
#if defined(J9VM_OPT_JITSERVER)
   // JIT client needs to associate each class loader with the name of its first loaded class
   // in order to support AOT method serialization (and caching at JITServer) and deserialization.
   std::pair<void *, void *>// loader, chain
   lookupClassLoaderAndChainAssociatedWithClassName(const uint8_t *data, size_t length) const;
   void *lookupClassLoaderAssociatedWithClassName(const uint8_t *data, size_t length) const;
   const J9UTF8 *lookupClassNameAssociatedWithClassLoader(void *loader) const;
#endif /* defined(J9VM_OPT_JITSERVER) */
   void removeClassLoader(J9VMThread *vmThread, void *loader);

   TR_J9SharedCache *getSharedCache() const { return _sharedCache; }
   void setSharedCache(TR_J9SharedCache *sharedCache) { _sharedCache = sharedCache; }

   /**
    * \brief Remember that \p loader is permanent.
    *
    * It will be included in the result of later calls to getPermanentLoaders().
    *
    * \param vmThread the J9VMThread of the current thread
    * \param loader the permanent class loader
    */
   void addPermanentLoader(J9VMThread *vmThread, J9ClassLoader *loader);

   /**
    * \brief Populate \p dest with the class loaders that are known to be permanent.
    *
    * The class loader pointers will be produced in the same order in which
    * they were added.
    *
    * \param vmThread the J9VMThread of the current thread
    * \param[out] dest the resulting vector of class loader pointers
    */
   void getPermanentLoaders(
      J9VMThread *vmThread, TR::vector<J9ClassLoader*, TR::Region&> &dest) const;

#if defined(J9VM_OPT_JITSERVER)
   /**
    * \brief Get the number of permanent loaders that have been sent to the server.
    *
    * Loaders are expected to be sent in the order in which they were added.
    *
    * The caller must hold the sequencing monitor.
    *
    * \param compInfo the compilation info object
    * \return the number of loaders
    */
   size_t numPermanentLoadersSentToServer(TR::CompilationInfo *compInfo) const;

   /**
    * \brief Add \p n to the number of permanent loaders sent to the server.
    *
    * The caller commits to send the next \p n loaders beyond those that were
    * previously sent.
    *
    * The caller must hold the sequencing monitor.
    *
    * \param n the number of newly sent permanent loaders
    * \param compInfo the compilation info object
    */
   void addPermanentLoadersSentToServer(size_t n, TR::CompilationInfo *compInfo);

   /**
    * \brief Reset the number of permanent loaders sent to the server to zero.
    *
    * This will cause a future compilation to send the loaders again, which is
    * useful after connecting to a new server.
    *
    * The caller must hold the sequencing monitor.
    *
    * \param compInfo the compilation info object
    */
   void resetPermanentLoadersSentToServer(TR::CompilationInfo *compInfo);
#endif

private:

#if defined(J9VM_OPT_JITSERVER)
   void assertSequencingMonitorHeld(TR::CompilationInfo *compInfo) const;
#endif

   friend class TR_Debug;

   TR_PersistentMemory *const _persistentMemory;
   TR_J9SharedCache *_sharedCache;

   typedef TR::typed_allocator<J9ClassLoader*, TR::PersistentAllocator&> ClassLoaderPtrAlloc;
   typedef std::vector<J9ClassLoader*, ClassLoaderPtrAlloc> ClassLoaderPtrVec;
   ClassLoaderPtrVec _permanentLoaders; // protected by the class table mutex

#if defined(J9VM_OPT_JITSERVER)
   size_t _numPermanentLoadersSentToServer; // protected by the sequencing monitor
#endif

   TR_ClassLoaderInfo *_loaderTable[CLASSLOADERTABLE_SIZE];
   TR_ClassLoaderInfo *_chainTable[CLASSLOADERTABLE_SIZE];
#if defined(J9VM_OPT_JITSERVER)
   TR_ClassLoaderInfo *_nameTable[CLASSLOADERTABLE_SIZE];
#endif /* defined(J9VM_OPT_JITSERVER) */
   };

#endif
