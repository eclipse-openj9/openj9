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

#ifndef ARTIFACT_MANAGER_HPP
#define ARTIFACT_MANAGER_HPP

// This will include less cruft, but may not be the right thing to do
// If so, may be more appropriate to include j9.h instead.

#include "env/TRMemory.hpp"
#include "infra/CriticalSection.hpp"

namespace TR { class CodeCache; }
namespace TR { class Monitor; }
struct J9JavaVM;
struct J9VMThread;
struct J9JITExceptionTable;
struct J9PortLibrary;
struct J9AVLTree;
struct J9JITHashTable;

/**
   @class TR_VMExclusiveAccess
   @brief This class is used to acquire and release exclusive VM access in the manner of the RAII idiom.

   The main use of this class is expected to be instantiation on the stack at the beginning of the scope requiring VM access.
   VM access will then be freed when the instances goes out of scope through auto destruction.
*/
class TR_VMExclusiveAccess {
public:

   /**
      @brief Acquires exclusive VM access if necessary.

      Exclusive VM access is required if we have a current thread.
      @param vm Used to access VM functions to acquire the current thread and acquire exclusive VM access.
   */
   TR_VMExclusiveAccess(J9JavaVM *vm);

   /**
      @brief Releases VM access if appropriate.

      Exclusive VM access will have been acquired if we have a current thread.
   */
   ~TR_VMExclusiveAccess();
private:
   /**
      @brief Used to access internal VM functions.
   */
   J9JavaVM *_vm;

   /**
      @brief Used to determine if we require exclusive VM access
   */
   J9VMThread *_currentThread;
};

/**
   @class TR_TranslationArtifactManager
   @brief Manages JIT access to VM JIT artifacts.

   This class is intended to be use as the centralized manager of the JIT artifact AVL tree and individual hash tables for each code cache.
   This class has uses a TR::Monitor to synchronise access to the individual artifacts and uses exclusive VM access to add a hash table for each code cache.
*/

class TR_TranslationArtifactManager
   {
public:
   TR_PERSISTENT_ALLOC(TR_Memory::ArtifactManager);

   class CriticalSection : public OMR::CriticalSection
      {
   public:
      CriticalSection() : OMR::CriticalSection(getGlobalArtifactManager()->_monitor)
         { }
      CriticalSection(TR_TranslationArtifactManager *mgr) : OMR::CriticalSection(mgr->_monitor)
         { }
      };

   /**
   @brief Adds a J9JITHashTable to the artifact AVL tree to reference artifacts contained in the code cache.

   @param codeCache The new code cache that requires a new hash table in the AVL tree.
   */
   bool addCodeCache(TR::CodeCache *codeCache);

   /**
   @brief Removes a J9JITHashTable from the artifact AVL tree to reference artifacts contained in the code cache.

   @param codeCache The code cache that should be removed from the AVL tree.
   */
   bool removeCodeCache(TR::CodeCache *codeCache);

   /**
   @brief For a given method's J9JITExceptionTable, finds the appropriate hashtable in the AVL tree and inserts the exception table.

   Note, insertArtifact does not check to verify that an artifact's given range is not already occupied by an existing artifact.  This is because artifact ranges represent the virtual memory actually occupied by compiled code.
   Thus, if two artifact ranges overlap, then two pieces of compiled code co-exist on top of each other, which is inherently incorrect.  If the possibility occurs that two artifacts may legitimately have range values that overlap, checks will need to be added.

   @param compiledMethod The J9JITExceptionTable representing the newly compiled method to insert into the artifacts.
   @return Returns true if successful, and false otherwise.
   */
   bool insertArtifact(J9JITExceptionTable *compiledMethod);

   /**
   @brief Attempts to find a registered artifact for a given artifact's startPC.

   @param pc The PC for which we require the JIT artifact.
   @return If an artifact for a given startPC is successfully found, returns that artifact, returns NULL otherwise.
   */
   const J9JITExceptionTable *retrieveArtifact (uintptr_t pc) const;

   /**
   @brief Determines if the JIT artifacts contain a reference to a particular J9JITExceptionTable.

   Note: Just because the JIT artifacts return an artifact for a particular startPC does not mean that it is the same artifact being searched for.
   The method could have been recompiled, and then subsequently unloaded and another method compiled to the same location before the recompilation-based reclamation could occur.
   This will result in the JIT artifacts containing a different J9JITExceptionTable for a given startPC.

   @param artifact The artifact for which to search for.
   @return Returns true if the same artifact is found for the artifact's startPC and false otherwise.
   */
   bool containsArtifact(J9JITExceptionTable *artifact) const;

   /**
   @brief Remove a given artifact from the JIT artifacts if it is found therein.

   @param compiledMethod The J9JITExceptionTable representing the method to remove from the JIT artifacts.
   @return Returns true if the artifact is found within, and successfully removed from, the JIT artifacts and false otherwise.
   */
   bool removeArtifact(J9JITExceptionTable *compiledMethod);

   // statics:
   /**
   @brief Initializes the global artifact manager.

   @param translationArtifacts The J9AVLTree to contain the J9JITHashTables for each code cache
   @param vm A pointer the the J9JavaVM used to reference the port library and access the internal VM functions.
   @return Returns true if initialization is successful and false otherwise.
   */
   static bool initializeGlobalArtifactManager(J9AVLTree *translationArtifacts, J9JavaVM *vm);

   /**
   @brief Returns a pointer the the global artifact manager.

   @return Pointer to the global artifact manager.
   */
   static TR_TranslationArtifactManager *getGlobalArtifactManager() { return globalManager; }

protected:
   /**
   @brief Initializes the artifact manager's members.

   The non-cache members are pointers to types instantiated externally, the constructor simply copies the pointers into the objects private members.
   */
   TR_TranslationArtifactManager(J9AVLTree *translationArtifacts, J9JavaVM *vm, TR::Monitor *monitor);

   /**
   @brief Inserts an artifact into the JIT artifacts causing it to represent a given memory range.

   Note this method expects to be called via another method in the artifact manager and thus does not acquire the artifact manager's monitor.

   @param artifact The J9JITExceptionTable which will represent the given memory range.
   @param startPC The beginning of the memory range to represent.
   @param endPC The end of the memory range to represent.
   @return Returns true if the artifact was successfully inserted as representing the given range, false otherwise.
   */
   bool insertRange(J9JITExceptionTable *artifact, uintptr_t startPC, uintptr_t endPC);

   /**
   @brief Removes an artifact from the JIT artifacts causing it to no longer represent a given memory range.

   Note this method expects to be called via another method in the artifact manager and thus does not acquire the artifact manager's monitor.

   @param artifact The J9JITExceptionTable to remove from representing the given memory range.
   @param startPC The beginning of the memory range the artifact represents.
   @param endPC The end of the memory range the artifact represents.
   @return Returns true if the artifact was successfully removed from representing the given range, false otherwise.
   */
   bool removeRange(J9JITExceptionTable *artifact, uintptr_t startPC, uintptr_t endPC);

private:
   /**
   @brief Determines if the current artifactManager query is using the same artifact as the previous query, and if not, searches for and retrieves the new artifact's code cache's hash table.

   Note this method expects to be called via another method in the artifact manager and thus does not acquire the artifact manager's monitor.

   @param artifact The artifact we are currently inquiring about.
   */
   void updateCache(uintptr_t currentPC) const;

   // member data
   J9AVLTree *_translationArtifacts;
   J9JavaVM *_vm;
   J9PortLibrary *_portLibrary;
   TR::Monitor *_monitor;
   mutable uintptr_t _cachedPC;
   mutable J9JITHashTable *_cachedHashTable;
   mutable J9JITExceptionTable *_retrievedArtifactCache;

   // Singleton
   static TR_TranslationArtifactManager *globalManager;
   };

#endif
