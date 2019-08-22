/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

#ifndef JITaaS_PERSISTENT_CHTABLE_H
#define JITaaS_PERSISTENT_CHTABLE_H

#include "env/PersistentCHTable.hpp"
#include "env/PersistentCollections.hpp"
#include "env/CHTable.hpp"
#include <unordered_map>
#include <unordered_set>

#define COLLECT_CHTABLE_STATS

#ifdef COLLECT_CHTABLE_STATS
#define CHTABLE_UPDATE_COUNTER(counter, value) counter += value
#else
#define CHTABLE_UPDATE_COUNTER(counter, value)
#endif


class TR_JITaaSServerPersistentCHTable : public TR_PersistentCHTable
   {
public:
   TR_ALLOC(TR_Memory::PersistentCHTable)

   TR_JITaaSServerPersistentCHTable(TR_PersistentMemory *);

   bool isInitialized() { return !getData().empty(); } // needs CHTable mutex in hand
   bool initializeIfNeeded(TR_J9VMBase *fej9);
   void doUpdate(TR_J9VMBase *fej9, const std::string &removeStr, const std::string &modifyStr);

   virtual TR_PersistentClassInfo * findClassInfo(TR_OpaqueClassBlock * classId) override;
   virtual TR_PersistentClassInfo * findClassInfoAfterLocking(TR_OpaqueClassBlock * classId, TR::Compilation *, bool returnClassInfoForAOT = false) override;

#ifdef COLLECT_CHTABLE_STATS
   // Statistical counters
   uint32_t _numUpdates; // aka numCompilations
   uint32_t _numClassesUpdated;
   uint32_t _numClassesRemoved;
   uint32_t _numQueries;
   uint32_t _updateBytes;
   uint32_t _maxUpdateBytes;
#endif

private:
   void commitRemoves(const std::string &data);
   void commitModifications(const std::string &data);

   PersistentUnorderedMap<TR_OpaqueClassBlock*, TR_PersistentClassInfo*> &getData();
   };

class TR_JITaaSClientPersistentCHTable : public TR_PersistentCHTable
   {
public:
   TR_ALLOC(TR_Memory::PersistentCHTable)

   TR_JITaaSClientPersistentCHTable(TR_PersistentMemory *);

   std::pair<std::string, std::string> serializeUpdates();

   virtual TR_PersistentClassInfo * findClassInfo(TR_OpaqueClassBlock * classId) override;
   virtual TR_PersistentClassInfo * findClassInfoAfterLocking(TR_OpaqueClassBlock * classId, TR::Compilation *, bool returnClassInfoForAOT = false) override;

   virtual TR_PersistentClassInfo *classGotLoaded(TR_FrontEnd *, TR_OpaqueClassBlock *classId) override;
   virtual bool classGotInitialized(TR_FrontEnd *fe, TR_PersistentMemory *persistentMemory, TR_OpaqueClassBlock *classId, TR_PersistentClassInfo *clazz) override;
   virtual void classGotRedefined(TR_FrontEnd *vm, TR_OpaqueClassBlock *oldClassId, TR_OpaqueClassBlock *newClassId) override;
   virtual void classGotUnloaded(TR_FrontEnd *fe, TR_OpaqueClassBlock *classId) override;
   virtual void classGotUnloadedPost(TR_FrontEnd *fe, TR_OpaqueClassBlock *classId) override;
   virtual void removeClass(TR_FrontEnd *, TR_OpaqueClassBlock *classId, TR_PersistentClassInfo *info, bool removeInfo) override;
   virtual bool classGotExtended(TR_FrontEnd *vm, TR_PersistentMemory *, TR_OpaqueClassBlock *superClassId, TR_OpaqueClassBlock *subClassId) override;

  
   void markForRemoval(TR_OpaqueClassBlock *clazz);
   void markDirty(TR_OpaqueClassBlock *clazz);
#ifdef COLLECT_CHTABLE_STATS
   uint32_t _numUpdates; // aka numCompilations
   uint32_t _numCommitFailures;
   uint32_t _numClassesUpdated;
   uint32_t _numClassesRemoved;
   uint32_t _updateBytes;
   uint32_t _maxUpdateBytes;
#endif

private:

   std::string serializeRemoves();
   std::string serializeModifications();

   PersistentUnorderedSet<TR_OpaqueClassBlock*> _dirty;
   PersistentUnorderedSet<TR_OpaqueClassBlock*> _remove;
   };

class FlatPersistentClassInfo
   {
public:
   static std::string serializeHierarchy(TR_PersistentClassInfo *orig);
   static std::vector<TR_PersistentClassInfo*> deserializeHierarchy(std::string& data);
   static size_t classSize(TR_PersistentClassInfo *clazz);
   static size_t serializeClass(TR_PersistentClassInfo *clazz, FlatPersistentClassInfo* info);
   static size_t deserializeClassSimple(TR_PersistentClassInfo *clazz, FlatPersistentClassInfo *info);

   TR_OpaqueClassBlock                *_classId;

   union
      {
      uintptrj_t _visitedStatus;
      TR_PersistentClassInfoForFields *_fieldInfo;
      };
   int16_t                             _prexAssumptions;
   uint16_t                            _timeStamp;
   int32_t                             _nameLength;
   flags8_t                            _flags;
   flags8_t                            _shouldNotBeNewlyExtended; // one bit for each possible compilation thread

   uint32_t                            _numSubClasses;
   TR_OpaqueClassBlock                *_subClasses[0];
   };

class TR_JITaaSPersistentClassInfo : public TR_PersistentClassInfo
   {
public:
   TR_PERSISTENT_ALLOC(TR_Memory::PersistentInfo);
   TR_JITaaSPersistentClassInfo(TR_OpaqueClassBlock *id, TR_JITaaSClientPersistentCHTable *chTable);

   // All of these methods mark the classInfo as dirty/removed and call a parent method
   virtual void setInitialized(TR_PersistentMemory *) override;
   virtual void setClassId(TR_OpaqueClassBlock *newClass) override;
   virtual void setFirstSubClass(TR_SubClass *sc) override;
   virtual void setFieldInfo(TR_PersistentClassInfoForFields *i) override;
   virtual TR_SubClass *addSubClass(TR_PersistentClassInfo *subClass) override;
   virtual void removeSubClasses() override;
   virtual void removeASubClass(TR_PersistentClassInfo *subClass) override;
   virtual void removeUnloadedSubClasses() override;
   virtual void setUnloaded() override;
   virtual void incNumPrexAssumptions() override;
   virtual void setReservable(bool v = true) override;
   virtual void setShouldNotBeNewlyExtended(int32_t ID) override;
   virtual void resetShouldNotBeNewlyExtended(int32_t ID) override;
   virtual void clearShouldNotBeNewlyExtended() override;
   virtual void setHasRecognizedAnnotations(bool v = true) override;
   virtual void setAlreadyCheckedForAnnotations(bool v = true) override;
   virtual void setCannotTrustStaticFinal(bool v = true) override;
   virtual void setClassHasBeenRedefined(bool v = true) override;
   virtual void setNameLength(int32_t length) override;
private:
   static TR_JITaaSClientPersistentCHTable *_chTable;
   };

#endif // JITaaS_PERSISTENT_CHTABLE_H
