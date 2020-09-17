/*******************************************************************************
 * Copyright (c) 2018, 2020 IBM Corp. and others
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

#ifndef JITSERVER_PERSISTENT_CHTABLE_H
#define JITSERVER_PERSISTENT_CHTABLE_H

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

/**
 * @class JITServerPersistentCHTable
 * @brief Class for managing PersistentCHTable data at the server side
 *
 * This class is an extension of the TR_PersistentCHTable class. Since the 
 * JITServer can have multiple JITClients and each JITClient has its own class 
 * hierarchies, the JITServer needs to keep a separate TR_PersistentCHTable
 * cache for each JITClient. This is done by associating caches with client 
 * IDs within the JITServerPersisentCHTable. It overrides the findClassInfo()
 * methods to find the corresponding JITClient and return its cached class 
 * info.
 */

class JITServerPersistentCHTable : public TR_PersistentCHTable
   {
public:
   TR_ALLOC(TR_Memory::PersistentCHTable)

   JITServerPersistentCHTable(TR_PersistentMemory *);
   ~JITServerPersistentCHTable();

   bool isInitialized() { return !_classMap.empty(); } // needs CHTable monitor in hand
   bool initializeCHTable(TR_J9VMBase *fej9, const std::string &rawData);
   void doUpdate(TR_J9VMBase *fej9, const std::string &removeStr, const std::string &modifyStr);

   virtual TR_PersistentClassInfo * findClassInfo(TR_OpaqueClassBlock * classId) override;
   virtual TR_PersistentClassInfo * findClassInfoAfterLocking(TR_OpaqueClassBlock * classId, TR::Compilation *, bool returnClassInfoForAOT = false) override;
   virtual TR_PersistentClassInfo * findClassInfoAfterLocking(TR_OpaqueClassBlock * classId, TR_FrontEnd *, bool returnClassInfoForAOT = false) override;

   TR::Monitor *getCHTableMonitor() { return _chTableMonitor; }

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

   PersistentUnorderedMap<TR_OpaqueClassBlock*, TR_PersistentClassInfo*> _classMap;
   TR::Monitor *_chTableMonitor;
   };

/**
 * @class JITClientPersistentCHTable
 * @brief Class for managing PersistentCHTable data at the client side
 *
 * This class is an extension of the TR_PersistentCHTable class.
 * JITClientPersistentCHTable needs to keep track of what changes have
 * been made to its TR_PersistentCHTable since the last delta update was
 * sent. Other functionalities are deferred to the TR_PersistentCHTable 
 * base class.
 */

class JITClientPersistentCHTable : public TR_PersistentCHTable
   {
public:
   TR_ALLOC(TR_Memory::PersistentCHTable)

   JITClientPersistentCHTable(TR_PersistentMemory *);

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

   /** 
    * @brief Collect entire class hierarchy into a vector
    * @param[out] out : The vector that gets populated with pointers to all existing TR_PersistentClassInfo
    * @return Returns the amount of space needed to serialize the entire hierarchy
    */
   size_t collectEntireHierarchy(std::vector<TR_PersistentClassInfo*> &out) const;
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


/**
 * @class FlatPersistentClassInfo
 * @brief Class for serializing and deserializing CHTable data for JITServerPersistentCHtable and JITClientPersistentCHTable
 *
 * This class is a utilty class for JITServerPersistentCHTable and 
 * JITClientPersistentCHTable. It is a friend class of TR_PersistentClassInfo
 * to make it more convenient to manipulate its fields.
 */

class FlatPersistentClassInfo
   {
public:
   static std::string serializeHierarchy(const JITClientPersistentCHTable *chTable);
   static std::vector<TR_PersistentClassInfo*> deserializeHierarchy(const std::string& data);
   static size_t classSize(TR_PersistentClassInfo *clazz);
   static size_t serializeClass(TR_PersistentClassInfo *clazz, FlatPersistentClassInfo* info);
   static size_t deserializeClassSimple(TR_PersistentClassInfo *clazz, FlatPersistentClassInfo *info);

   TR_OpaqueClassBlock                *_classId;

   union
      {
      uintptr_t _visitedStatus;
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



/**
 * @class TR_JITClientPersistentClassInfo
 * @brief Class for marking TR_PersistentClassInfo as dirty/removed.
 *
 * This class is a extension of the TR_PersistentClassInfo class that is
 * used by JITClient to mark classInfo as dirty/removed.
 * This is how JITClient determines what the delta update to JITServer is
 * everytime. Once the marking operation is completed, the corresponding 
 * parent method is called and original functionality is preserved.
 */

class TR_JITClientPersistentClassInfo : public TR_PersistentClassInfo
   {
public:
   TR_PERSISTENT_ALLOC(TR_Memory::PersistentInfo);
   TR_JITClientPersistentClassInfo(TR_OpaqueClassBlock *id, JITClientPersistentCHTable *chTable);

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
   static JITClientPersistentCHTable *_chTable;
   };

#endif // JITSERVER_PERSISTENT_CHTABLE_H
