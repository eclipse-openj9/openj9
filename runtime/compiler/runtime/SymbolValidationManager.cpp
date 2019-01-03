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

#include <string.h>
#include "env/VMJ9.h"
#include "env/ClassLoaderTable.hpp"
#include "env/PersistentCHTable.hpp"
#include "env/VMAccessCriticalSection.hpp"
#include "exceptions/AOTFailure.hpp"
#include "compile/Compilation.hpp"
#include "control/CompilationRuntime.hpp"
#include "control/CompilationThread.hpp"
#include "runtime/RelocationRuntime.hpp"
#include "runtime/SymbolValidationManager.hpp"

#include "j9protos.h"

static char getPrimitiveChar(J9UTF8 * primitiveClassName)
   {
   const char *name = reinterpret_cast<const char *>(J9UTF8_DATA(primitiveClassName));
   int32_t length = J9UTF8_LENGTH(primitiveClassName);

   if (0 == strncmp(name,"int", length))
      return 'I';
   if (0 == strncmp(name,"boolean", length))
      return 'Z';
   if (0 == strncmp(name,"long", length))
      return 'J';
   if (0 == strncmp(name,"double", length))
      return 'D';
   if (0 == strncmp(name,"float", length))
      return 'F';
   if (0 == strncmp(name,"char", length))
      return 'C';
   if (0 == strncmp(name,"byte", length))
      return 'B';
   if (0 == strncmp(name,"short", length))
      return 'S';

   return '\0';
   }

TR::SymbolValidationManager::SymbolValidationManager(TR::Region &region, TR_ResolvedMethod *compilee)
   : _symbolID(FIRST_ID),
     _heuristicRegion(0),
     _region(region),
     _symbolValidationRecords(_region),
     _symbolToIdMap((SymbolToIdComparator()), _region),
     _idToSymbolTable(_region),
     _seenSymbolsSet((SeenSymbolsComparator()), _region)
   {
   assertionsAreFatal(); // Acknowledge the env var whether or not assertions fail

   TR::Compilation* comp = TR::comp();
   J9VMThread *vmThread = comp->j9VMThread();
   TR_J9VM *fej9 = (TR_J9VM *)TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);

   defineGuaranteedID(compilee->classOfMethod(), TR::SymbolType::typeClass);
   defineGuaranteedID(compilee->getPersistentIdentifier(), TR::SymbolType::typeMethod);

   struct J9Class ** arrayClasses = &fej9->getJ9JITConfig()->javaVM->booleanArrayClass;
   for (int32_t i = 4; i <= 11; i++)
      {
      TR_OpaqueClassBlock *arrayClass = reinterpret_cast<TR_OpaqueClassBlock *>(arrayClasses[i - 4]);
      TR_OpaqueClassBlock *component = fej9->getComponentClassFromArrayClass(arrayClass);
      defineGuaranteedID(arrayClass, TR::SymbolType::typeClass);
      defineGuaranteedID(component, TR::SymbolType::typeClass);
      }
   }

void
TR::SymbolValidationManager::defineGuaranteedID(void *symbol, TR::SymbolType type)
   {
   uint16_t id = getNewSymbolID();
   _symbolToIdMap.insert(std::make_pair(symbol, id));
   setSymbolOfID(id, symbol, type);
   _seenSymbolsSet.insert(symbol);
   }

void
TR::SymbolValidationManager::setSymbolOfID(uint16_t id, void *symbol, TR::SymbolType type)
   {
   if (id >= _idToSymbolTable.size())
      {
      TypedSymbol unused = { NULL, typeOpaque, false };
      _idToSymbolTable.resize(id + 1, unused);
      }

   SVM_ASSERT(!_idToSymbolTable[id]._hasValue, "multiple definitions of ID %d", id);
   _idToSymbolTable[id]._symbol = symbol;
   _idToSymbolTable[id]._type = type;
   _idToSymbolTable[id]._hasValue = true;
   }

uint16_t
TR::SymbolValidationManager::getNewSymbolID()
   {
   SVM_ASSERT_NONFATAL(_symbolID != 0xFFFF, "symbol ID overflow");
   return _symbolID++;
   }

void *
TR::SymbolValidationManager::getSymbolFromID(uint16_t id, TR::SymbolType type)
   {
   TypedSymbol *entry = NULL;
   if (id < _idToSymbolTable.size())
      entry = &_idToSymbolTable[id];

   SVM_ASSERT(entry != NULL && entry->_hasValue, "Unknown ID %d", id);
   SVM_ASSERT(entry->_type == type, "ID has type %d when %d was expected", entry->_type, type);
   return entry->_symbol;
   }

TR_OpaqueClassBlock *
TR::SymbolValidationManager::getClassFromID(uint16_t id)
   {
   return static_cast<TR_OpaqueClassBlock*>(
      getSymbolFromID(id, TR::SymbolType::typeClass));
   }

J9Class *
TR::SymbolValidationManager::getJ9ClassFromID(uint16_t id)
   {
   return static_cast<J9Class*>(
      getSymbolFromID(id, TR::SymbolType::typeClass));
   }

TR_OpaqueMethodBlock *
TR::SymbolValidationManager::getMethodFromID(uint16_t id)
   {
   return static_cast<TR_OpaqueMethodBlock*>(
      getSymbolFromID(id, TR::SymbolType::typeMethod));
   }

J9Method *
TR::SymbolValidationManager::getJ9MethodFromID(uint16_t id)
   {
   return static_cast<J9Method*>(
      getSymbolFromID(id, TR::SymbolType::typeMethod));
   }

uint16_t
TR::SymbolValidationManager::tryGetIDFromSymbol(void *symbol)
   {
   SymbolToIdMap::iterator it = _symbolToIdMap.find(symbol);
   if (it == _symbolToIdMap.end())
      return NO_ID;
   else
      return it->second;
   }

uint16_t
TR::SymbolValidationManager::getIDFromSymbol(void *symbol)
   {
   uint16_t id = tryGetIDFromSymbol(symbol);
   SVM_ASSERT(id != NO_ID, "Unknown symbol %p\n", symbol);
   return id;
   }

TR_OpaqueClassBlock *
TR::SymbolValidationManager::getBaseComponentClass(TR_OpaqueClassBlock *clazz, int32_t &numDims)
   {
   numDims = 0;
   if (!clazz)
      return NULL;

   TR::Compilation* comp = TR::comp();
   J9VMThread *vmThread = comp->j9VMThread();
   TR_J9VM *fej9 = (TR_J9VM *)TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);

   while (fej9->isClassArray(clazz))
      {
      TR_OpaqueClassBlock * componentClass = fej9->getComponentClassFromArrayClass(clazz);
      numDims++;
      clazz = componentClass;
      }

   return clazz;
   }


void *
TR::SymbolValidationManager::storeClassChain(TR_J9VMBase *fej9, TR_OpaqueClassBlock *clazz)
   {
   void *classChain = NULL;
   bool validated = false;

   classChain = fej9->sharedCache()->rememberClass(clazz);
   if (classChain)
      validated = addClassChainRecord(clazz, classChain);

   if (validated)
      return classChain;
   else
      return NULL;
   }

void
TR::SymbolValidationManager::storeRecord(void *symbol, TR::SymbolValidationRecord *record)
   {
   SVM_ASSERT(!inHeuristicRegion(), "Attempted to storeRecord in a heuristic region");
   if (!isAlreadyValidated(symbol))
      {
      _symbolToIdMap.insert(std::make_pair(symbol, getNewSymbolID()));
      }
   _symbolValidationRecords.push_front(record);

   record->printFields();
   traceMsg(TR::comp(), "\tkind=%d\n", record->_kind);
   traceMsg(TR::comp(), "\tid=%d\n", (uint32_t)getIDFromSymbol(symbol));
   traceMsg(TR::comp(), "\n");
   }

bool
TR::SymbolValidationManager::storeClassRecord(TR_OpaqueClassBlock *clazz,
                                              TR::ClassValidationRecord *record,
                                              int32_t arrayDimsToValidate,
                                              bool storeCurrentRecord)
   {
   bool validated = false;

   TR::Compilation* comp = TR::comp();
   J9VMThread *vmThread = comp->j9VMThread();
   TR_J9VMBase *fej9 = TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);

   TR_AOTStats *aotStats = ((TR_JitPrivateConfig *)fej9->_jitConfig->privateConfig)->aotStats;

   if (storeCurrentRecord)
      storeRecord(static_cast<void *>(clazz), record);

   if (fej9->isClassArray(clazz))
      {
      validated = true;
      }
   else
      {
      if (fej9->isPrimitiveClass(clazz))
         {
         validated = true; // primitive types have guaranteed IDs
         }
      else
         {
         record->_classChain = storeClassChain(fej9, clazz);
         validated = (record->_classChain != NULL);
         }

      if (validated)
         {
         int32_t arrayDims = 0;
         TR_OpaqueClassBlock *componentClass = clazz;
         while (validated && componentClass && arrayDims < arrayDimsToValidate)
            {
            TR_OpaqueClassBlock *arrayClass = fej9->getArrayClassFromComponentClass(componentClass);

            validated = addArrayClassFromComponentClassRecord(arrayClass, componentClass);

            componentClass = arrayClass;
            arrayDims++;
            }

         SVM_ASSERT(
            validated && arrayDims == arrayDimsToValidate,
            "Failed to validate class %p",
            clazz);
         }
      }

   /* If we failed to store the class chain
    * then it is because we failed to rememberClass;
    * therefore, the last element in the list
    * will be the one we added in this method and not
    * the ClassChainRecord
    */
   if (!validated)
      _symbolValidationRecords.pop_front();

   return validated;
   }

bool
TR::SymbolValidationManager::storeValidationRecordIfNecessary(void *symbol,
                                                              TR::SymbolValidationRecord *record,
                                                              int32_t arrayDimsToValidate)
   {
   bool existsInList = false;
   bool validated = false;

   for (auto it = _symbolValidationRecords.begin(); it != _symbolValidationRecords.end(); it++)
      {
      if ((*it)->_kind == record->_kind && record->isEqual(*it))
         {
         existsInList = true;
         break;
         }
      }

   if (!existsInList || arrayDimsToValidate)
      {
      if (record->isClassValidationRecord())
         {
         validated = storeClassRecord(static_cast<TR_OpaqueClassBlock *>(symbol),
                                      reinterpret_cast<TR::ClassValidationRecord *>(record),
                                      arrayDimsToValidate,
                                      !existsInList);
         }
      else
         {
         storeRecord(symbol, record);
         validated = true;
         }
      }

   if (existsInList || !validated)
      {
      _region.deallocate(record);
      }

   return (existsInList || validated);
   }

bool
TR::SymbolValidationManager::addClassByNameRecord(TR_OpaqueClassBlock *clazz, TR_OpaqueClassBlock *beholder)
   {
   if (!clazz)
      return false;
   if (inHeuristicRegion())
      return true;

   TR::Compilation* comp = TR::comp();

   SVM_ASSERT_ALREADY_VALIDATED(this, beholder);

   int32_t arrayDims = 0;
   clazz = getBaseComponentClass(clazz, arrayDims);
   char primitiveType = '\0';

   J9VMThread *vmThread = comp->j9VMThread();
   TR_J9VMBase *fej9 = TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);
   if (fej9->isPrimitiveClass(clazz))
      {
      J9ROMClass *romClass = reinterpret_cast<J9ROMClass *>(fej9->getPersistentClassPointerFromClassPointer(clazz));
      J9UTF8 * className = J9ROMCLASS_CLASSNAME(romClass);
      primitiveType = getPrimitiveChar(className);
      }

   SymbolValidationRecord *record = new (_region) ClassByNameRecord(clazz, beholder, primitiveType);
   return storeValidationRecordIfNecessary(static_cast<void *>(clazz), record, arrayDims);
   }

bool
TR::SymbolValidationManager::addProfiledClassRecord(TR_OpaqueClassBlock *clazz)
   {
   if (!clazz)
      return false;
   if (inHeuristicRegion())
      return true;

   int32_t arrayDims = 0;
   clazz = getBaseComponentClass(clazz, arrayDims);
   char primitiveType = '\0';

   TR::Compilation* comp = TR::comp();
   J9VMThread *vmThread = comp->j9VMThread();
   TR_J9VMBase *fej9 = TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);
   if (fej9->isPrimitiveClass(clazz))
      {
      J9ROMClass *romClass = reinterpret_cast<J9ROMClass *>(fej9->getPersistentClassPointerFromClassPointer(clazz));
      J9UTF8 * className = J9ROMCLASS_CLASSNAME(romClass);
      primitiveType = getPrimitiveChar(className);
      }

   SymbolValidationRecord *record = new (_region) ProfiledClassRecord(clazz, primitiveType);
   bool validated = storeValidationRecordIfNecessary(static_cast<void *>(clazz), record, arrayDims);
   return validated;
   }

bool
TR::SymbolValidationManager::addClassFromCPRecord(TR_OpaqueClassBlock *clazz, J9ConstantPool *constantPoolOfBeholder, uint32_t cpIndex)
   {
   if (!clazz)
      return false;
   if (inHeuristicRegion())
      return true;

   int32_t arrayDims = 0;
   clazz = getBaseComponentClass(clazz, arrayDims);

   TR_OpaqueClassBlock *beholder = reinterpret_cast<TR_OpaqueClassBlock *>(J9_CLASS_FROM_CP(constantPoolOfBeholder));
   SVM_ASSERT_ALREADY_VALIDATED(this, beholder);

   SymbolValidationRecord *record = new (_region) ClassFromCPRecord(clazz, beholder, cpIndex);
   return storeValidationRecordIfNecessary(static_cast<void *>(clazz), record, arrayDims);
   }

bool
TR::SymbolValidationManager::addDefiningClassFromCPRecord(TR_OpaqueClassBlock *clazz, J9ConstantPool *constantPoolOfBeholder, uint32_t cpIndex, bool isStatic)
   {
   if (!clazz)
      return false;
   if (inHeuristicRegion())
      return true;

   int32_t arrayDims = 0;
   clazz = getBaseComponentClass(clazz, arrayDims);

   TR_OpaqueClassBlock *beholder = reinterpret_cast<TR_OpaqueClassBlock *>(J9_CLASS_FROM_CP(constantPoolOfBeholder));
   SVM_ASSERT_ALREADY_VALIDATED(this, beholder);

   SymbolValidationRecord *record = new (_region) DefiningClassFromCPRecord(clazz, beholder, cpIndex, isStatic);
   return storeValidationRecordIfNecessary(static_cast<void *>(clazz), record, arrayDims);
   }

bool
TR::SymbolValidationManager::addStaticClassFromCPRecord(TR_OpaqueClassBlock *clazz, J9ConstantPool *constantPoolOfBeholder, uint32_t cpIndex)
   {
   if (!clazz)
      return false;
   if (inHeuristicRegion())
      return true;

   int32_t arrayDims = 0;
   clazz = getBaseComponentClass(clazz, arrayDims);

   TR_OpaqueClassBlock *beholder = reinterpret_cast<TR_OpaqueClassBlock *>(J9_CLASS_FROM_CP(constantPoolOfBeholder));
   SVM_ASSERT_ALREADY_VALIDATED(this, beholder);

   SymbolValidationRecord *record = new (_region) StaticClassFromCPRecord(clazz, beholder, cpIndex);
   return storeValidationRecordIfNecessary(static_cast<void *>(clazz), record, arrayDims);
   }

bool
TR::SymbolValidationManager::addClassFromMethodRecord(TR_OpaqueClassBlock *clazz, TR_OpaqueMethodBlock *method)
   {
   if (!clazz)
      return false;
   if (inHeuristicRegion())
      return true;

   int32_t arrayDims = 0;
   clazz = getBaseComponentClass(clazz, arrayDims);

   SVM_ASSERT_ALREADY_VALIDATED(this, method);

   SymbolValidationRecord *record = new (_region) ClassFromMethodRecord(clazz, method);
   return storeValidationRecordIfNecessary(static_cast<void *>(clazz), record);
   }

bool
TR::SymbolValidationManager::addComponentClassFromArrayClassRecord(TR_OpaqueClassBlock *componentClass, TR_OpaqueClassBlock *arrayClass)
   {
   if (!componentClass)
      return false;
   if (inHeuristicRegion())
      return true;

   SVM_ASSERT_ALREADY_VALIDATED(this, arrayClass);

   SymbolValidationRecord *record = new (_region) ComponentClassFromArrayClassRecord(componentClass, arrayClass);
   return storeValidationRecordIfNecessary(static_cast<void *>(componentClass), record);
   }

bool
TR::SymbolValidationManager::addArrayClassFromComponentClassRecord(TR_OpaqueClassBlock *arrayClass, TR_OpaqueClassBlock *componentClass)
   {
   if (!arrayClass)
      return false;
   if (inHeuristicRegion())
      return true;

   SVM_ASSERT_ALREADY_VALIDATED(this, componentClass);

   SymbolValidationRecord *record = new (_region) ArrayClassFromComponentClassRecord(arrayClass, componentClass);
   return storeValidationRecordIfNecessary(static_cast<void *>(arrayClass), record);
   }

bool
TR::SymbolValidationManager::addSuperClassFromClassRecord(TR_OpaqueClassBlock *superClass, TR_OpaqueClassBlock *childClass)
   {
   if (!superClass)
      return false;
   if (inHeuristicRegion())
      return true;

   int32_t arrayDims = 0;
   superClass = getBaseComponentClass(superClass, arrayDims);

   SVM_ASSERT_ALREADY_VALIDATED(this, childClass);

   SymbolValidationRecord *record = new (_region) SuperClassFromClassRecord(superClass, childClass);
   return storeValidationRecordIfNecessary(static_cast<void *>(superClass), record, arrayDims);
   }

bool
TR::SymbolValidationManager::addClassInstanceOfClassRecord(TR_OpaqueClassBlock *classOne, TR_OpaqueClassBlock *classTwo, bool objectTypeIsFixed, bool castTypeIsFixed, bool isInstanceOf)
   {
   if (inHeuristicRegion())
      return true;

   SVM_ASSERT_ALREADY_VALIDATED(this, classOne);
   SVM_ASSERT_ALREADY_VALIDATED(this, classTwo);

   SymbolValidationRecord *record = new (_region) ClassInstanceOfClassRecord(classOne, classTwo, objectTypeIsFixed, castTypeIsFixed, isInstanceOf);
   return storeValidationRecordIfNecessary(static_cast<void *>(classOne), record);
   }

bool
TR::SymbolValidationManager::addSystemClassByNameRecord(TR_OpaqueClassBlock *systemClass)
   {
   if (!systemClass)
      return false;
   if (inHeuristicRegion())
      return true;

   int32_t arrayDims = 0;
   systemClass = getBaseComponentClass(systemClass, arrayDims);

   SymbolValidationRecord *record = new (_region) SystemClassByNameRecord(systemClass);
   return storeValidationRecordIfNecessary(static_cast<void *>(systemClass), record, arrayDims);
   }

bool
TR::SymbolValidationManager::addClassFromITableIndexCPRecord(TR_OpaqueClassBlock *clazz, J9ConstantPool *constantPoolOfBeholder, int32_t cpIndex)
   {
   if (!clazz)
      return false;
   if (inHeuristicRegion())
      return true;

   TR_OpaqueClassBlock *beholder = reinterpret_cast<TR_OpaqueClassBlock *>(J9_CLASS_FROM_CP(constantPoolOfBeholder));
   SVM_ASSERT_ALREADY_VALIDATED(this, beholder);

   int32_t arrayDims = 0;
   clazz = getBaseComponentClass(clazz, arrayDims);

   SymbolValidationRecord *record = new (_region) ClassFromITableIndexCPRecord(clazz, beholder, cpIndex);
   return storeValidationRecordIfNecessary(static_cast<void *>(clazz), record, arrayDims);
   }

bool
TR::SymbolValidationManager::addDeclaringClassFromFieldOrStaticRecord(TR_OpaqueClassBlock *clazz, J9ConstantPool *constantPoolOfBeholder, int32_t cpIndex)
   {
   if (!clazz)
      return false;
   if (inHeuristicRegion())
      return true;

   TR_OpaqueClassBlock *beholder = reinterpret_cast<TR_OpaqueClassBlock *>(J9_CLASS_FROM_CP(constantPoolOfBeholder));
   SVM_ASSERT_ALREADY_VALIDATED(this, beholder);

   int32_t arrayDims = 0;
   clazz = getBaseComponentClass(clazz, arrayDims);

   SymbolValidationRecord *record = new (_region) DeclaringClassFromFieldOrStaticRecord(clazz, beholder, cpIndex);
   return storeValidationRecordIfNecessary(static_cast<void *>(clazz), record, arrayDims);
   }

bool
TR::SymbolValidationManager::addClassClassRecord(TR_OpaqueClassBlock *classClass, TR_OpaqueClassBlock *objectClass)
   {
   if (!classClass)
      return false;
   if (inHeuristicRegion())
      return true;

   SVM_ASSERT_ALREADY_VALIDATED(this, objectClass);

   SymbolValidationRecord *record = new (_region) ClassClassRecord(classClass, objectClass);
   return storeValidationRecordIfNecessary(static_cast<void *>(classClass), record);
   }

bool
TR::SymbolValidationManager::addConcreteSubClassFromClassRecord(TR_OpaqueClassBlock *childClass, TR_OpaqueClassBlock *superClass)
   {
   if (!superClass)
      return false;
   if (inHeuristicRegion())
      return true;

   int32_t arrayDims = 0;
   childClass = getBaseComponentClass(childClass, arrayDims);

   SVM_ASSERT_ALREADY_VALIDATED(this, superClass);

   SymbolValidationRecord *record = new (_region) ConcreteSubClassFromClassRecord(childClass, superClass);
   return storeValidationRecordIfNecessary(static_cast<void *>(childClass), record, arrayDims);
   }

bool
TR::SymbolValidationManager::addClassChainRecord(TR_OpaqueClassBlock *clazz, void *classChain)
   {
   if (!clazz)
      return false;
   if (inHeuristicRegion())
      return true;

   SymbolValidationRecord *record = new (_region) ClassChainRecord(clazz, classChain);
   return storeValidationRecordIfNecessary(static_cast<void *>(clazz), record);
   }

bool
TR::SymbolValidationManager::addMethodByNameRecord(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *beholder)
   {
   if (!method)
      return false;
   if (inHeuristicRegion())
      return true;

   SVM_ASSERT_ALREADY_VALIDATED(this, beholder);

   SymbolValidationRecord *record = new (_region) MethodByNameRecord(method, beholder);
   bool valid = storeValidationRecordIfNecessary(static_cast<void *>(method), record);

   if (valid)
      {
      J9Class *methodClass = J9_CLASS_FROM_METHOD(reinterpret_cast<J9Method *>(method));
      valid = addClassFromMethodRecord(reinterpret_cast<TR_OpaqueClassBlock *>(methodClass), method);
      }

   return valid;
   }

bool
TR::SymbolValidationManager::addMethodFromClassRecord(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *beholder, uint32_t index)
   {
   if (!method)
      return false;
   if (inHeuristicRegion())
      return true;

   TR::Compilation* comp = TR::comp();

   if (index == static_cast<uint32_t>(-1))
      {
      J9VMThread *vmThread = comp->j9VMThread();
      TR_J9VM *fej9 = (TR_J9VM *)TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);

      J9Method * resolvedMethods = static_cast<J9Method *>(fej9->getMethods(beholder));
      uint32_t numMethods = fej9->getNumMethods(beholder);
      for (index = 0; index < numMethods ; index++)
         {
         if ((TR_OpaqueMethodBlock *) &(resolvedMethods[index]) == method)
            break;
         }

      SVM_ASSERT(
         index < numMethods,
         "Method %p not found in class %p",
         method,
         beholder);
      }

   SVM_ASSERT_ALREADY_VALIDATED(this, beholder);

   SymbolValidationRecord *record = new (_region) MethodFromClassRecord(method, beholder, index);
   return storeValidationRecordIfNecessary(static_cast<void *>(method), record);
   }

bool
TR::SymbolValidationManager::addStaticMethodFromCPRecord(TR_OpaqueMethodBlock *method, J9ConstantPool *cp, int32_t cpIndex)
   {
   if (!method)
      return false;
   if (inHeuristicRegion())
      return true;

   TR_OpaqueClassBlock *beholder = reinterpret_cast<TR_OpaqueClassBlock *>(J9_CLASS_FROM_CP(cp));
   SVM_ASSERT_ALREADY_VALIDATED(this, beholder);

   SymbolValidationRecord *record = new (_region) StaticMethodFromCPRecord(method, beholder, cpIndex);
   bool valid = storeValidationRecordIfNecessary(static_cast<void *>(method), record);

   if (valid)
      {
      J9Class *methodClass = J9_CLASS_FROM_METHOD(reinterpret_cast<J9Method *>(method));
      valid = addClassFromMethodRecord(reinterpret_cast<TR_OpaqueClassBlock *>(methodClass), method);
      }

   return valid;
   }

bool
TR::SymbolValidationManager::addSpecialMethodFromCPRecord(TR_OpaqueMethodBlock *method, J9ConstantPool *cp, int32_t cpIndex)
   {
   if (!method)
      return false;
   if (inHeuristicRegion())
      return true;

   TR_OpaqueClassBlock *beholder = reinterpret_cast<TR_OpaqueClassBlock *>(J9_CLASS_FROM_CP(cp));
   SVM_ASSERT_ALREADY_VALIDATED(this, beholder);

   SymbolValidationRecord *record = new (_region) SpecialMethodFromCPRecord(method, beholder, cpIndex);
   bool valid = storeValidationRecordIfNecessary(static_cast<void *>(method), record);

   if (valid)
      {
      J9Class *methodClass = J9_CLASS_FROM_METHOD(reinterpret_cast<J9Method *>(method));
      valid = addClassFromMethodRecord(reinterpret_cast<TR_OpaqueClassBlock *>(methodClass), method);
      }

   return valid;
   }

bool
TR::SymbolValidationManager::addVirtualMethodFromCPRecord(TR_OpaqueMethodBlock *method, J9ConstantPool *cp, int32_t cpIndex)
   {
   if (!method)
      return false;
   if (inHeuristicRegion())
      return true;

   TR_OpaqueClassBlock *beholder = reinterpret_cast<TR_OpaqueClassBlock *>(J9_CLASS_FROM_CP(cp));
   SVM_ASSERT_ALREADY_VALIDATED(this, beholder);

   SymbolValidationRecord *record = new (_region) VirtualMethodFromCPRecord(method, beholder, cpIndex);
   bool valid = storeValidationRecordIfNecessary(static_cast<void *>(method), record);

   if (valid)
      {
      J9Class *methodClass = J9_CLASS_FROM_METHOD(reinterpret_cast<J9Method *>(method));
      valid = addClassFromMethodRecord(reinterpret_cast<TR_OpaqueClassBlock *>(methodClass), method);
      }

   return valid;
   }

bool
TR::SymbolValidationManager::addVirtualMethodFromOffsetRecord(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *beholder, int32_t virtualCallOffset, bool ignoreRtResolve)
   {
   if (!method)
      return false;
   if (inHeuristicRegion())
      return true;

   SVM_ASSERT_ALREADY_VALIDATED(this, beholder);

   SymbolValidationRecord *record = new (_region) VirtualMethodFromOffsetRecord(method, beholder, virtualCallOffset, ignoreRtResolve);
   bool valid = storeValidationRecordIfNecessary(static_cast<void *>(method), record);

   if (valid)
      {
      J9Class *methodClass = J9_CLASS_FROM_METHOD(reinterpret_cast<J9Method *>(method));
      valid = addClassFromMethodRecord(reinterpret_cast<TR_OpaqueClassBlock *>(methodClass), method);
      }

   return valid;
   }

bool
TR::SymbolValidationManager::addInterfaceMethodFromCPRecord(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *beholder, TR_OpaqueClassBlock *lookup, int32_t cpIndex)
   {
   if (!method)
      return false;
   if (inHeuristicRegion())
      return true;

   SVM_ASSERT_ALREADY_VALIDATED(this, beholder);
   SVM_ASSERT_ALREADY_VALIDATED(this, lookup);

   SymbolValidationRecord *record = new (_region) InterfaceMethodFromCPRecord(method, beholder, lookup, cpIndex);
   bool valid = storeValidationRecordIfNecessary(static_cast<void *>(method), record);

   if (valid)
      {
      J9Class *methodClass = J9_CLASS_FROM_METHOD(reinterpret_cast<J9Method *>(method));
      valid = addClassFromMethodRecord(reinterpret_cast<TR_OpaqueClassBlock *>(methodClass), method);
      }

   return valid;
   }

bool
TR::SymbolValidationManager::addImproperInterfaceMethodFromCPRecord(TR_OpaqueMethodBlock *method, J9ConstantPool *cp, int32_t cpIndex)
   {
   if (!method)
      return false;
   if (inHeuristicRegion())
      return true;

   TR_OpaqueClassBlock *beholder = reinterpret_cast<TR_OpaqueClassBlock *>(J9_CLASS_FROM_CP(cp));
   SVM_ASSERT_ALREADY_VALIDATED(this, beholder);

   SymbolValidationRecord *record = new (_region) ImproperInterfaceMethodFromCPRecord(method, beholder, cpIndex);
   bool valid = storeValidationRecordIfNecessary(static_cast<void *>(method), record);

   if (valid)
      {
      J9Class *methodClass = J9_CLASS_FROM_METHOD(reinterpret_cast<J9Method *>(method));
      valid = addClassFromMethodRecord(reinterpret_cast<TR_OpaqueClassBlock *>(methodClass), method);
      }

   return valid;
   }

bool
TR::SymbolValidationManager::addMethodFromClassAndSignatureRecord(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *methodClass, TR_OpaqueClassBlock *beholder)
   {
   if (!method)
      return false;
   if (inHeuristicRegion())
      return true;

   SVM_ASSERT_ALREADY_VALIDATED(this, methodClass);
   SVM_ASSERT_ALREADY_VALIDATED(this, beholder);

   SymbolValidationRecord *record = new (_region) MethodFromClassAndSigRecord(method, methodClass, beholder);
   return storeValidationRecordIfNecessary(static_cast<void *>(method), record);
   }

bool
TR::SymbolValidationManager::addMethodFromSingleImplementerRecord(TR_OpaqueMethodBlock *method,
                                                                  TR_OpaqueClassBlock *thisClass,
                                                                  int32_t cpIndexOrVftSlot,
                                                                  TR_OpaqueMethodBlock *callerMethod,
                                                                  TR_YesNoMaybe useGetResolvedInterfaceMethod)
   {
   if (!method)
      return false;
   if (inHeuristicRegion())
      return true;

   SVM_ASSERT_ALREADY_VALIDATED(this, thisClass);
   SVM_ASSERT_ALREADY_VALIDATED(this, callerMethod);

   SymbolValidationRecord *record = new (_region) MethodFromSingleImplementer(method, thisClass, cpIndexOrVftSlot, callerMethod, useGetResolvedInterfaceMethod);
   bool valid = storeValidationRecordIfNecessary(static_cast<void *>(method), record);

   if (valid)
      {
      J9Class *methodClass = J9_CLASS_FROM_METHOD(reinterpret_cast<J9Method *>(method));
      valid = addClassFromMethodRecord(reinterpret_cast<TR_OpaqueClassBlock *>(methodClass), method);
      }

   return valid;
   }

bool
TR::SymbolValidationManager::addMethodFromSingleInterfaceImplementerRecord(TR_OpaqueMethodBlock *method,
                                                                           TR_OpaqueClassBlock *thisClass,
                                                                           int32_t cpIndex,
                                                                           TR_OpaqueMethodBlock *callerMethod)
   {
   if (!method)
      return false;
   if (inHeuristicRegion())
      return true;

   SVM_ASSERT_ALREADY_VALIDATED(this, thisClass);
   SVM_ASSERT_ALREADY_VALIDATED(this, callerMethod);

   SymbolValidationRecord *record = new (_region) MethodFromSingleInterfaceImplementer(method, thisClass, cpIndex, callerMethod);
   bool valid = storeValidationRecordIfNecessary(static_cast<void *>(method), record);

   if (valid)
      {
      J9Class *methodClass = J9_CLASS_FROM_METHOD(reinterpret_cast<J9Method *>(method));
      valid = addClassFromMethodRecord(reinterpret_cast<TR_OpaqueClassBlock *>(methodClass), method);
      }

   return valid;
   }

bool
TR::SymbolValidationManager::addMethodFromSingleAbstractImplementerRecord(TR_OpaqueMethodBlock *method,
                                                                          TR_OpaqueClassBlock *thisClass,
                                                                          int32_t vftSlot,
                                                                          TR_OpaqueMethodBlock *callerMethod)
   {
   if (!method)
      return false;
   if (inHeuristicRegion())
      return true;

   SVM_ASSERT_ALREADY_VALIDATED(this, thisClass);
   SVM_ASSERT_ALREADY_VALIDATED(this, callerMethod);

   SymbolValidationRecord *record = new (_region) MethodFromSingleAbstractImplementer(method, thisClass, vftSlot, callerMethod);
   bool valid = storeValidationRecordIfNecessary(static_cast<void *>(method), record);

   if (valid)
      {
      J9Class *methodClass = J9_CLASS_FROM_METHOD(reinterpret_cast<J9Method *>(method));
      valid = addClassFromMethodRecord(reinterpret_cast<TR_OpaqueClassBlock *>(methodClass), method);
      }

   return valid;
   }

bool
TR::SymbolValidationManager::addStackWalkerMaySkipFramesRecord(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *methodClass, bool skipFrames)
   {
   if (!method || !methodClass)
      return false;
   if (inHeuristicRegion())
      return true;

   SVM_ASSERT_ALREADY_VALIDATED(this, method);
   SVM_ASSERT_ALREADY_VALIDATED(this, methodClass);

   SymbolValidationRecord *record = new (_region) StackWalkerMaySkipFramesRecord(method, methodClass, skipFrames);
   return storeValidationRecordIfNecessary(static_cast<void *>(method), record);
   }

bool
TR::SymbolValidationManager::addClassInfoIsInitializedRecord(TR_OpaqueClassBlock *clazz, bool isInitialized)
   {
   if (!clazz)
      return false;
   if (inHeuristicRegion())
      return true;

   SVM_ASSERT_ALREADY_VALIDATED(this, clazz);

   SymbolValidationRecord *record = new (_region) ClassInfoIsInitialized(clazz, isInitialized);
   return storeValidationRecordIfNecessary(static_cast<void *>(clazz), record);
   }



bool
TR::SymbolValidationManager::validateSymbol(uint16_t idToBeValidated, void *validSymbol, TR::SymbolType type)
   {
   bool valid = false;
   TypedSymbol *entry = NULL;
   if (idToBeValidated < _idToSymbolTable.size())
      entry = &_idToSymbolTable[idToBeValidated];

   if (validSymbol)
      {
      if (entry == NULL || !entry->_hasValue)
         {
         if (_seenSymbolsSet.find(validSymbol) == _seenSymbolsSet.end())
            {
            setSymbolOfID(idToBeValidated, validSymbol, type);
            _seenSymbolsSet.insert(validSymbol);
            valid = true;
            }
         }
      else
         {
         valid = (entry->_symbol == validSymbol && entry->_type == type);
         }
      }

   return valid;
   }

bool
TR::SymbolValidationManager::validateSymbol(uint16_t idToBeValidated, TR_OpaqueClassBlock *clazz)
   {
   return validateSymbol(idToBeValidated, clazz, TR::SymbolType::typeClass);
   }

bool
TR::SymbolValidationManager::validateSymbol(uint16_t idToBeValidated, J9Class *clazz)
   {
   return validateSymbol(idToBeValidated, clazz, TR::SymbolType::typeClass);
   }

bool
TR::SymbolValidationManager::validateSymbol(uint16_t idToBeValidated, TR_OpaqueMethodBlock *method)
   {
   return validateSymbol(idToBeValidated, method, TR::SymbolType::typeMethod);
   }

bool
TR::SymbolValidationManager::validateSymbol(uint16_t idToBeValidated, J9Method *method)
   {
   return validateSymbol(idToBeValidated, method, TR::SymbolType::typeMethod);
   }

bool
TR::SymbolValidationManager::validateClassByNameRecord(uint16_t classID, uint16_t beholderID, J9ROMClass *romClass, char primitiveType)
   {
   TR::Compilation* comp = TR::comp();
   J9VMThread *vmThread = comp->j9VMThread();
   TR_J9VM *fej9 = (TR_J9VM *)TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);

   J9Class *beholder = getJ9ClassFromID(beholderID);
   J9ConstantPool *beholderCP = J9_CP_FROM_CLASS(beholder);

   if (primitiveType != '\0')
      {
      getClassFromID(classID); // just to assert that it exists
      return true;
      }

   J9UTF8 * classNameData = J9ROMCLASS_CLASSNAME(romClass);
   char *className = reinterpret_cast<char *>(J9UTF8_DATA(classNameData));
   uint32_t classNameLength = J9UTF8_LENGTH(classNameData);

   TR_OpaqueClassBlock *classByName = fej9->getClassFromSignature(className, classNameLength, beholderCP);

   if (!classByName)
      return false;

   int32_t arrayDims = 0;
   classByName = getBaseComponentClass(classByName, arrayDims);

   return validateSymbol(classID, classByName);
   }

bool
TR::SymbolValidationManager::validateProfiledClassRecord(uint16_t classID, char primitiveType, void *classChainIdentifyingLoader, void *classChainForClassBeingValidated)
   {
   TR::Compilation* comp = TR::comp();

   if (primitiveType != '\0')
      {
      getClassFromID(classID); // just to assert that it exists
      return true;
      }

   J9ClassLoader *classLoader = (J9ClassLoader *) comp->fej9()->sharedCache()->persistentClassLoaderTable()->lookupClassLoaderAssociatedWithClassChain(classChainIdentifyingLoader);

   if (classLoader)
      {
      TR_OpaqueClassBlock *clazz = comp->fej9()->sharedCache()->lookupClassFromChainAndLoader(static_cast<uintptrj_t *>(classChainForClassBeingValidated), classLoader);

      if (clazz)
         {
         int32_t arrayDims = 0;
         clazz = getBaseComponentClass(clazz, arrayDims);

         return validateSymbol(classID, clazz);
         }
      }

   return false;
   }

bool
TR::SymbolValidationManager::validateClassFromCPRecord(uint16_t classID,  uint16_t beholderID, uint32_t cpIndex)
   {
   TR::Compilation* comp = TR::comp();
   J9VMThread *vmThread = comp->j9VMThread();
   TR_J9VM *fej9 = (TR_J9VM *)TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);

   J9Class *beholder = getJ9ClassFromID(beholderID);
   J9ConstantPool *beholderCP = J9_CP_FROM_CLASS(beholder);
   TR_OpaqueClassBlock *classFromCP = TR_ResolvedJ9Method::getClassFromCP(fej9, beholderCP, comp, cpIndex);

   int32_t arrayDims = 0;
   classFromCP = getBaseComponentClass(classFromCP, arrayDims);

   return validateSymbol(classID, classFromCP);
   }

bool
TR::SymbolValidationManager::validateDefiningClassFromCPRecord(uint16_t classID, uint16_t beholderID, uint32_t cpIndex, bool isStatic)
   {
   TR::Compilation* comp = TR::comp();
   J9VMThread *vmThread = comp->j9VMThread();
   TR_J9VM *fej9 = (TR_J9VM *)TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);

   TR::CompilationInfo *compInfo = TR::CompilationInfo::get(fej9->_jitConfig);
   TR::CompilationInfoPerThreadBase *compInfoPerThreadBase = compInfo->getCompInfoForCompOnAppThread();
   TR_RelocationRuntime *reloRuntime;
   if (compInfoPerThreadBase)
      reloRuntime = compInfoPerThreadBase->reloRuntime();
   else
      reloRuntime = compInfo->getCompInfoForThread(fej9->vmThread())->reloRuntime();

   J9Class *beholder = getJ9ClassFromID(beholderID);
   J9ConstantPool *beholderCP = J9_CP_FROM_CLASS(beholder);
   TR_OpaqueClassBlock *classFromCP = reloRuntime->getClassFromCP(fej9->vmThread(), fej9->_jitConfig->javaVM, beholderCP, cpIndex, isStatic);

   int32_t arrayDims = 0;
   classFromCP = getBaseComponentClass(classFromCP, arrayDims);

   return validateSymbol(classID, classFromCP);
   }

bool
TR::SymbolValidationManager::validateStaticClassFromCPRecord(uint16_t classID, uint16_t beholderID, uint32_t cpIndex)
   {
   TR::Compilation* comp = TR::comp();
   J9VMThread *vmThread = comp->j9VMThread();
   TR_J9VM *fej9 = (TR_J9VM *)TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);

   J9Class *beholder = getJ9ClassFromID(beholderID);
   J9ConstantPool *beholderCP = J9_CP_FROM_CLASS(beholder);
   TR_OpaqueClassBlock *classFromCP = TR_ResolvedJ9Method::getClassOfStaticFromCP(fej9, beholderCP, cpIndex);

   int32_t arrayDims = 0;
   classFromCP = getBaseComponentClass(classFromCP, arrayDims);

   return validateSymbol(classID, classFromCP);
   }

bool
TR::SymbolValidationManager::validateClassFromMethodRecord(uint16_t classID, uint16_t methodID)
   {
   J9Method *method = getJ9MethodFromID(methodID);
   TR_OpaqueClassBlock *classFromMethod = reinterpret_cast<TR_OpaqueClassBlock *>(J9_CLASS_FROM_METHOD(method));

   int32_t arrayDims = 0;
   classFromMethod = getBaseComponentClass(classFromMethod, arrayDims);

   return validateSymbol(classID, classFromMethod);
   }

bool
TR::SymbolValidationManager::validateComponentClassFromArrayClassRecord(uint16_t componentClassID, uint16_t arrayClassID)
   {
   TR::Compilation* comp = TR::comp();
   J9VMThread *vmThread = comp->j9VMThread();
   TR_J9VM *fej9 = (TR_J9VM *)TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);

   TR_OpaqueClassBlock *arrayClass = getClassFromID(arrayClassID);
   TR_OpaqueClassBlock *componentClass = fej9->getComponentClassFromArrayClass(arrayClass);

   return validateSymbol(componentClassID, componentClass);
   }

bool
TR::SymbolValidationManager::validateArrayClassFromComponentClassRecord(uint16_t arrayClassID, uint16_t componentClassID)
   {
   TR::Compilation* comp = TR::comp();
   J9VMThread *vmThread = comp->j9VMThread();
   TR_J9VM *fej9 = (TR_J9VM *)TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);

   TR_OpaqueClassBlock *componentClass = getClassFromID(componentClassID);
   TR_OpaqueClassBlock *arrayClass = fej9->getArrayClassFromComponentClass(componentClass);

   return validateSymbol(arrayClassID, arrayClass);
   }

bool
TR::SymbolValidationManager::validateSuperClassFromClassRecord(uint16_t superClassID, uint16_t childClassID)
   {
   TR::Compilation* comp = TR::comp();
   J9VMThread *vmThread = comp->j9VMThread();
   TR_J9VM *fej9 = (TR_J9VM *)TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);

   TR_OpaqueClassBlock *childClass = getClassFromID(childClassID);
   TR_OpaqueClassBlock *superClass = fej9->getSuperClass(childClass);

   int32_t arrayDims = 0;
   superClass = getBaseComponentClass(superClass, arrayDims);

   return validateSymbol(superClassID, superClass);
   }

bool
TR::SymbolValidationManager::validateClassInstanceOfClassRecord(uint16_t classOneID, uint16_t classTwoID, bool objectTypeIsFixed, bool castTypeIsFixed, bool wasInstanceOf)
   {
   TR::Compilation* comp = TR::comp();
   J9VMThread *vmThread = comp->j9VMThread();
   TR_J9VM *fej9 = (TR_J9VM *)TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);

   TR_OpaqueClassBlock *classOne = getClassFromID(classOneID);
   TR_OpaqueClassBlock *classTwo = getClassFromID(classTwoID);

   TR_YesNoMaybe isInstanceOf = fej9->isInstanceOf(classOne, classTwo, objectTypeIsFixed, castTypeIsFixed);

   return (wasInstanceOf == isInstanceOf);
   }

bool
TR::SymbolValidationManager::validateSystemClassByNameRecord(uint16_t systemClassID, J9ROMClass *romClass)
   {
   TR::Compilation* comp = TR::comp();
   J9VMThread *vmThread = comp->j9VMThread();
   TR_J9VM *fej9 = (TR_J9VM *)TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);

   J9UTF8 * className = J9ROMCLASS_CLASSNAME(romClass);
   TR_OpaqueClassBlock *systemClassByName = fej9->getSystemClassFromClassName(reinterpret_cast<const char *>(J9UTF8_DATA(className)),
                                                                              J9UTF8_LENGTH(className));

   int32_t arrayDims = 0;
   systemClassByName = getBaseComponentClass(systemClassByName, arrayDims);

   return validateSymbol(systemClassID, systemClassByName);
   }

bool
TR::SymbolValidationManager::validateClassFromITableIndexCPRecord(uint16_t classID, uint16_t beholderID, uint32_t cpIndex)
   {
   TR::Compilation* comp = TR::comp();
   J9VMThread *vmThread = comp->j9VMThread();
   TR_J9VM *fej9 = (TR_J9VM *)TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);

   J9Class *beholder = getJ9ClassFromID(beholderID);
   J9ConstantPool *beholderCP = J9_CP_FROM_CLASS(beholder);
   uintptr_t pITableIndex;
   TR_OpaqueClassBlock *clazz = TR_ResolvedJ9Method::getInterfaceITableIndexFromCP(fej9, beholderCP, cpIndex, &pITableIndex);

   int32_t arrayDims = 0;
   clazz = getBaseComponentClass(clazz, arrayDims);

   return validateSymbol(classID, clazz);
   }

bool
TR::SymbolValidationManager::validateDeclaringClassFromFieldOrStaticRecord(uint16_t definingClassID, uint16_t beholderID, int32_t cpIndex)
   {
   TR::Compilation* comp = TR::comp();
   J9VMThread *vmThread = comp->j9VMThread();
   TR_J9VM *fej9 = (TR_J9VM *)TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);

   J9Class *beholder = getJ9ClassFromID(beholderID);
   J9ROMClass *beholderRomClass = beholder->romClass;
   J9ConstantPool *beholderCP = J9_CP_FROM_CLASS(beholder);
   J9ROMFieldRef *romCPBase = (J9ROMFieldRef *)((UDATA)beholderRomClass + sizeof(J9ROMClass));

   int32_t classCPIndexOfFieldOrStatic = -1;
   if (cpIndex != -1)
      classCPIndexOfFieldOrStatic = ((J9ROMFieldRef *)(&romCPBase[cpIndex]))->classRefCPIndex;

   J9Class *definingClass = NULL;
   J9Class *cpClass = (J9Class*)TR_ResolvedJ9Method::getClassFromCP(fej9, beholderCP, comp, classCPIndexOfFieldOrStatic);

   if (cpClass)
      {
      TR::VMAccessCriticalSection getDeclaringClassFromFieldOrStatic(fej9);

      int32_t fieldLen;
      char *field = cpIndex >= 0 ? utf8Data(J9ROMNAMEANDSIGNATURE_NAME(J9ROMFIELDREF_NAMEANDSIGNATURE(&romCPBase[cpIndex])), fieldLen) : 0;

      int32_t sigLen;
      char *sig = cpIndex >= 0 ? utf8Data(J9ROMNAMEANDSIGNATURE_SIGNATURE(J9ROMFIELDREF_NAMEANDSIGNATURE(&romCPBase[cpIndex])), sigLen) : 0;

      vmThread->javaVM->internalVMFunctions->instanceFieldOffset(vmThread,
                                                                 cpClass,
                                                                 (U_8*)field,
                                                                 fieldLen,
                                                                 (U_8*)sig,
                                                                 sigLen,
                                                                 &definingClass,
                                                                 NULL,
                                                                 J9_LOOK_NO_JAVA);
      }
   else
      {
      return false;
      }

   TR_OpaqueClassBlock *opaqueDefiningClass = reinterpret_cast<TR_OpaqueClassBlock*>(definingClass);
   int32_t arrayDims = 0;
   opaqueDefiningClass = getBaseComponentClass(opaqueDefiningClass, arrayDims);

   return validateSymbol(definingClassID, opaqueDefiningClass);
   }

bool
TR::SymbolValidationManager::validateClassClassRecord(uint16_t classClassID, uint16_t objectClassID)
   {
   TR::Compilation* comp = TR::comp();
   J9VMThread *vmThread = comp->j9VMThread();
   TR_J9VM *fej9 = (TR_J9VM *)TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);

   TR_OpaqueClassBlock *objectClass = getClassFromID(objectClassID);
   TR_OpaqueClassBlock *classClass = fej9->getClassClassPointer(objectClass);

   return validateSymbol(classClassID, classClass);
   }

bool
TR::SymbolValidationManager::validateConcreteSubClassFromClassRecord(uint16_t childClassID, uint16_t superClassID)
   {
   TR_OpaqueClassBlock *superClass = getClassFromID(superClassID);

   TR_PersistentCHTable * chTable = comp()->getPersistentInfo()->getPersistentCHTable();
   TR_OpaqueClassBlock *childClass = chTable->findSingleConcreteSubClass(superClass, TR::comp(), false);

   int32_t arrayDims = 0;
   childClass = getBaseComponentClass(childClass, arrayDims);

   return validateSymbol(childClassID, childClass);
   }

bool
TR::SymbolValidationManager::validateClassChainRecord(uint16_t classID, void *classChain)
   {
   TR::Compilation* comp = TR::comp();
   TR_OpaqueClassBlock *definingClass = getClassFromID(classID);

   return comp->fej9()->sharedCache()->classMatchesCachedVersion(definingClass, (uintptrj_t *) classChain);
   }

bool
TR::SymbolValidationManager::validateMethodByNameRecord(uint16_t methodID, uint16_t beholderID, J9ROMClass *romClass, J9ROMMethod *romMethod)
   {
   TR::Compilation* comp = TR::comp();
   J9VMThread *vmThread = comp->j9VMThread();
   TR_J9VM *fej9 = reinterpret_cast<TR_J9VM *>(TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread));

   J9Class *beholder = getJ9ClassFromID(beholderID);
   J9ConstantPool *beholderCP = J9_CP_FROM_CLASS(beholder);

   J9UTF8 *methodNameData = J9ROMMETHOD_NAME(romMethod);
   char *methodName = (char *)alloca(J9UTF8_LENGTH(methodNameData)+1);
   strncpy(methodName, reinterpret_cast<const char *>(J9UTF8_DATA(methodNameData)), J9UTF8_LENGTH(methodNameData));
   methodName[J9UTF8_LENGTH(methodNameData)] = '\0';

   J9UTF8 *methodSigData = J9ROMMETHOD_SIGNATURE(romMethod);
   char *methodSig = (char *)alloca(J9UTF8_LENGTH(methodSigData)+1);
   strncpy(methodSig, reinterpret_cast<const char *>(J9UTF8_DATA(methodSigData)), J9UTF8_LENGTH(methodSigData));
   methodSig[J9UTF8_LENGTH(methodSigData)] = '\0';

   J9UTF8 * classNameData = J9ROMCLASS_CLASSNAME(romClass);
   char *className = (char *)alloca(J9UTF8_LENGTH(classNameData)+1);
   strncpy(className, reinterpret_cast<const char *>(J9UTF8_DATA(classNameData)), J9UTF8_LENGTH(classNameData));
   className[J9UTF8_LENGTH(classNameData)] = '\0';

   TR_OpaqueMethodBlock *method = fej9->getMethodFromName(className, methodName, methodSig, beholderCP);

   return validateSymbol(methodID, method);
   }

bool
TR::SymbolValidationManager::validateMethodFromClassRecord(uint16_t methodID, uint16_t beholderID, uint32_t index)
   {
   TR::Compilation* comp = TR::comp();
   J9VMThread *vmThread = comp->j9VMThread();
   TR_J9VMBase *fej9 = TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);

   TR_OpaqueClassBlock *beholder = getClassFromID(beholderID);
   J9Method *method;

      {
      TR::VMAccessCriticalSection getResolvedMethods(fej9); // Prevent HCR
      J9Method *methods = static_cast<J9Method *>(fej9->getMethods(beholder));
      uint32_t numMethods = fej9->getNumMethods(beholder);
      SVM_ASSERT(index < numMethods, "Index is not within the bounds of the ramMethods array");
      method = &(methods[index]);
      }

   return validateSymbol(methodID, method);
   }

bool
TR::SymbolValidationManager::validateStaticMethodFromCPRecord(uint16_t methodID, uint16_t beholderID, int32_t cpIndex)
   {
   TR::Compilation* comp = TR::comp();
   J9VMThread *vmThread = comp->j9VMThread();
   TR_J9VMBase *fej9 = TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);

   J9Class *beholder = getJ9ClassFromID(beholderID);
   J9ConstantPool *beholderCP = J9_CP_FROM_CLASS(beholder);
   J9Method *ramMethod;

      {
      TR::VMAccessCriticalSection getResolvedStaticMethod(fej9);
      ramMethod = jitResolveStaticMethodRef(vmThread, beholderCP, cpIndex, J9_RESOLVE_FLAG_JIT_COMPILE_TIME);
      }

   return validateSymbol(methodID, ramMethod);
   }

bool
TR::SymbolValidationManager::validateSpecialMethodFromCPRecord(uint16_t methodID, uint16_t beholderID, int32_t cpIndex)
   {
   TR::Compilation* comp = TR::comp();
   J9VMThread *vmThread = comp->j9VMThread();
   TR_J9VMBase *fej9 = TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);

   J9Class *beholder = getJ9ClassFromID(beholderID);
   J9ConstantPool *beholderCP = J9_CP_FROM_CLASS(beholder);
   J9Method *ramMethod;

      {
      TR::VMAccessCriticalSection resolveSpecialMethodRef(fej9);
      ramMethod = jitResolveSpecialMethodRef(vmThread, beholderCP, cpIndex, J9_RESOLVE_FLAG_JIT_COMPILE_TIME);
      }

   return validateSymbol(methodID, ramMethod);
   }

bool
TR::SymbolValidationManager::validateVirtualMethodFromCPRecord(uint16_t methodID, uint16_t beholderID, int32_t cpIndex)
   {
   TR::Compilation* comp = TR::comp();
   J9VMThread *vmThread = comp->j9VMThread();
   TR_J9VMBase *fej9 = TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);

   J9Class *beholder = getJ9ClassFromID(beholderID);
   J9ConstantPool *beholderCP = J9_CP_FROM_CLASS(beholder);

   UDATA vTableOffset;
   bool unresolvedInCP;
   TR_OpaqueMethodBlock * ramMethod = TR_ResolvedJ9Method::getVirtualMethod(fej9, beholderCP, cpIndex, &vTableOffset, &unresolvedInCP);

   return validateSymbol(methodID, ramMethod);
   }

bool
TR::SymbolValidationManager::validateVirtualMethodFromOffsetRecord(uint16_t methodID, uint16_t beholderID, int32_t virtualCallOffset, bool ignoreRtResolve)
   {
   TR::Compilation* comp = TR::comp();
   J9VMThread *vmThread = comp->j9VMThread();
   TR_J9VMBase *fej9 = TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);

   TR_OpaqueClassBlock *beholder = getClassFromID(beholderID);

   TR_OpaqueMethodBlock *ramMethod = fej9->getResolvedVirtualMethod(beholder, virtualCallOffset, ignoreRtResolve);

   return validateSymbol(methodID, ramMethod);
   }

bool
TR::SymbolValidationManager::validateInterfaceMethodFromCPRecord(uint16_t methodID, uint16_t beholderID, uint16_t lookupID, int32_t cpIndex)
   {
   TR::Compilation* comp = TR::comp();
   J9VMThread *vmThread = comp->j9VMThread();
   TR_J9VMBase *fej9 = TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);

   TR_OpaqueClassBlock *lookup = getClassFromID(lookupID);

   J9Class *beholder = getJ9ClassFromID(beholderID);
   J9ConstantPool *beholderCP = J9_CP_FROM_CLASS(beholder);

   TR_OpaqueMethodBlock *ramMethod = fej9->getResolvedInterfaceMethod(beholderCP, lookup, cpIndex);

   return validateSymbol(methodID, ramMethod);
   }

bool
TR::SymbolValidationManager::validateImproperInterfaceMethodFromCPRecord(uint16_t methodID, uint16_t beholderID, int32_t cpIndex)
   {
   TR::Compilation* comp = TR::comp();
   J9VMThread *vmThread = comp->j9VMThread();
   TR_J9VMBase *fej9 = TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);

   J9Class *beholder = getJ9ClassFromID(beholderID);
   J9ConstantPool *beholderCP = J9_CP_FROM_CLASS(beholder);
   J9Method *ramMethod;

      {
      TR::VMAccessCriticalSection resolveImproperMethodRef(fej9);
      ramMethod = jitGetImproperInterfaceMethodFromCP(vmThread, beholderCP, cpIndex);
      }

   return validateSymbol(methodID, ramMethod);
   }

bool
TR::SymbolValidationManager::validateMethodFromClassAndSignatureRecord(uint16_t methodID, uint16_t methodClassID, uint16_t beholderID, J9ROMMethod *romMethod)
   {
   TR::Compilation* comp = TR::comp();
   J9VMThread *vmThread = comp->j9VMThread();
   TR_J9VMBase *fej9 = TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);

   TR_OpaqueClassBlock *methodClass = getClassFromID(methodClassID);
   TR_OpaqueClassBlock *beholder = getClassFromID(beholderID);

   J9UTF8 *methodNameData = J9ROMMETHOD_NAME(romMethod);
   char *methodName = (char *)alloca(J9UTF8_LENGTH(methodNameData)+1);
   strncpy(methodName, reinterpret_cast<const char *>(J9UTF8_DATA(methodNameData)), J9UTF8_LENGTH(methodNameData));
   methodName[J9UTF8_LENGTH(methodNameData)] = '\0';

   J9UTF8 *methodSigData = J9ROMMETHOD_SIGNATURE(romMethod);
   char *methodSig = (char *)alloca(J9UTF8_LENGTH(methodSigData)+1);
   strncpy(methodSig, reinterpret_cast<const char *>(J9UTF8_DATA(methodSigData)), J9UTF8_LENGTH(methodSigData));
   methodSig[J9UTF8_LENGTH(methodSigData)] = '\0';

   TR_OpaqueMethodBlock *method = fej9->getMethodFromClass(methodClass, methodName, methodSig, beholder);

   return validateSymbol(methodID, method);
   }

bool
TR::SymbolValidationManager::validateMethodFromSingleImplementerRecord(uint16_t methodID,
                                                                       uint16_t thisClassID,
                                                                       int32_t cpIndexOrVftSlot,
                                                                       uint16_t callerMethodID,
                                                                       TR_YesNoMaybe useGetResolvedInterfaceMethod)
   {
   TR::Compilation* comp = TR::comp();
   J9VMThread *vmThread = comp->j9VMThread();
   TR_J9VMBase *fej9 = TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);
   TR_Memory *trMemory = comp->trMemory();
   TR_PersistentCHTable * chTable = comp->getPersistentInfo()->getPersistentCHTable();

   TR_OpaqueClassBlock *thisClass = getClassFromID(thisClassID);
   TR_OpaqueMethodBlock *callerMethod = getMethodFromID(callerMethodID);

   TR_ResolvedMethod *callerResolvedMethod = fej9->createResolvedMethod(trMemory, callerMethod, NULL);
   TR_ResolvedMethod *calleeResolvedMethod = chTable->findSingleImplementer(thisClass, cpIndexOrVftSlot, callerResolvedMethod, comp, false, useGetResolvedInterfaceMethod, false);

   if (!calleeResolvedMethod)
      return false;

   TR_OpaqueMethodBlock *method = calleeResolvedMethod->getPersistentIdentifier();

   return validateSymbol(methodID, method);
   }

bool
TR::SymbolValidationManager::validateMethodFromSingleInterfaceImplementerRecord(uint16_t methodID,
                                                                                uint16_t thisClassID,
                                                                                int32_t cpIndex,
                                                                                uint16_t callerMethodID)
   {
   TR::Compilation* comp = TR::comp();
   J9VMThread *vmThread = comp->j9VMThread();
   TR_J9VMBase *fej9 = TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);
   TR_Memory *trMemory = comp->trMemory();
   TR_PersistentCHTable * chTable = comp->getPersistentInfo()->getPersistentCHTable();

   TR_OpaqueClassBlock *thisClass = getClassFromID(thisClassID);
   TR_OpaqueMethodBlock *callerMethod = getMethodFromID(callerMethodID);

   TR_ResolvedMethod *callerResolvedMethod = fej9->createResolvedMethod(trMemory, callerMethod, NULL);
   TR_ResolvedMethod *calleeResolvedMethod = chTable->findSingleInterfaceImplementer(thisClass, cpIndex, callerResolvedMethod, comp, false, false);

   if (!calleeResolvedMethod)
      return false;

   TR_OpaqueMethodBlock *method = calleeResolvedMethod->getPersistentIdentifier();

   return validateSymbol(methodID, method);
   }

bool
TR::SymbolValidationManager::validateMethodFromSingleAbstractImplementerRecord(uint16_t methodID,
                                                                               uint16_t thisClassID,
                                                                               int32_t vftSlot,
                                                                               uint16_t callerMethodID)
   {
   TR::Compilation* comp = TR::comp();
   J9VMThread *vmThread = comp->j9VMThread();
   TR_J9VMBase *fej9 = TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);
   TR_Memory *trMemory = comp->trMemory();
   TR_PersistentCHTable * chTable = comp->getPersistentInfo()->getPersistentCHTable();

   TR_OpaqueClassBlock *thisClass = getClassFromID(thisClassID);
   TR_OpaqueMethodBlock *callerMethod = getMethodFromID(callerMethodID);

   TR_ResolvedMethod *callerResolvedMethod = fej9->createResolvedMethod(trMemory, callerMethod, NULL);
   TR_ResolvedMethod *calleeResolvedMethod = chTable->findSingleAbstractImplementer(thisClass, vftSlot, callerResolvedMethod, comp, false, false);

   if (!calleeResolvedMethod)
      return false;

   TR_OpaqueMethodBlock *method = calleeResolvedMethod->getPersistentIdentifier();

   return validateSymbol(methodID, method);
   }

bool
TR::SymbolValidationManager::validateStackWalkerMaySkipFramesRecord(uint16_t methodID, uint16_t methodClassID, bool couldSkipFrames)
   {
   TR::Compilation* comp = TR::comp();
   J9VMThread *vmThread = comp->j9VMThread();
   TR_J9VMBase *fej9 = TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);

   TR_OpaqueMethodBlock *method = getMethodFromID(methodID);
   TR_OpaqueClassBlock *methodClass = getClassFromID(methodClassID);

   bool canSkipFrames = fej9->stackWalkerMaySkipFrames(method, methodClass);

   return (canSkipFrames == couldSkipFrames);
   }

bool
TR::SymbolValidationManager::validateClassInfoIsInitializedRecord(uint16_t classID, bool wasInitialized)
   {
   TR::Compilation* comp = TR::comp();
   TR_OpaqueClassBlock *clazz = getClassFromID(classID);

   bool initialized = false;

   TR_PersistentClassInfo * classInfo =
                          comp->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(clazz, comp, true);

   if (classInfo)
      initialized = classInfo->isInitialized();

   bool valid = (!wasInitialized || initialized);
   return valid;
   }

bool
TR::SymbolValidationManager::assertionsAreFatal()
   {
   // Look for the env var even in debug builds so that it's always acknowledged.
   static const bool fatal = feGetEnv("TR_svmAssertionsAreFatal") != NULL;

#if defined(DEBUG) || defined(PROD_WITH_ASSUMES) || defined(SVM_ASSERTIONS_ARE_FATAL)
   return true;
#else
   return fatal;
#endif
   }

static void printClass(TR_OpaqueClassBlock *clazz)
   {
   J9UTF8 *className = J9ROMCLASS_CLASSNAME(((J9Class *)clazz)->romClass);
   traceMsg(TR::comp(), "\tclassName=%.*s\n", J9UTF8_LENGTH(className), J9UTF8_DATA(className));
   }

void TR::ClassValidationRecord::printFields()
   {
   traceMsg(TR::comp(), "\t_classChain=0x%p\n", _classChain);
   }

void TR::ClassByNameRecord::printFields()
   {
   traceMsg(TR::comp(), "ClassByNameRecord\n");
   TR::ClassValidationRecord::printFields();
   traceMsg(TR::comp(), "\t_class=0x%p\n", _class);
   printClass(_class);
   traceMsg(TR::comp(), "\t_beholder=0x%p\n", _beholder);
   printClass(_beholder);
   traceMsg(TR::comp(), "\t_primitiveType=%c\n", _primitiveType);
   }

void TR::ProfiledClassRecord::printFields()
   {
   traceMsg(TR::comp(), "ProfiledClassRecord\n");
   TR::ClassValidationRecord::printFields();
   traceMsg(TR::comp(), "\t_class=0x%p\n", _class);
   printClass(_class);
   traceMsg(TR::comp(), "\t_primitiveType=%c\n", _primitiveType);
   }

void TR::ClassFromCPRecord::printFields()
   {
   traceMsg(TR::comp(), "ClassFromCPRecord\n");
   TR::ClassValidationRecord::printFields();
   traceMsg(TR::comp(), "\t_class=0x%p\n", _class);
   printClass(_class);
   traceMsg(TR::comp(), "\t_beholder=0x%p\n", _beholder);
   printClass(_beholder);
   traceMsg(TR::comp(), "\t_cpIndex=%d\n", _cpIndex);
   }

void TR::DefiningClassFromCPRecord::printFields()
   {
   traceMsg(TR::comp(), "DefiningClassFromCPRecord\n");
   TR::ClassValidationRecord::printFields();
   traceMsg(TR::comp(), "\t_class=0x%p\n", _class);
   printClass(_class);
   traceMsg(TR::comp(), "\t_beholder=0x%p\n", _beholder);
   printClass(_beholder);
   traceMsg(TR::comp(), "\t_cpIndex=%d\n", _cpIndex);
   traceMsg(TR::comp(), "\t_isStatic=%s\n", (_isStatic ? "true" : "false"));
   }

void TR::StaticClassFromCPRecord::printFields()
   {
   traceMsg(TR::comp(), "StaticClassFromCPRecord\n");
   TR::ClassValidationRecord::printFields();
   traceMsg(TR::comp(), "\t_class=0x%p\n", _class);
   printClass(_class);
   traceMsg(TR::comp(), "\t_beholder=0x%p\n", _beholder);
   printClass(_beholder);
   traceMsg(TR::comp(), "\t_cpIndex=%d\n", _cpIndex);
   }

void TR::ClassFromMethodRecord::printFields()
   {
   traceMsg(TR::comp(), "ClassFromMethodRecord\n");
   TR::ClassValidationRecord::printFields();
   traceMsg(TR::comp(), "\t_class=0x%p\n", _class);
   printClass(_class);
   traceMsg(TR::comp(), "\t_method=0x%p\n", _method);
   }

void TR::ComponentClassFromArrayClassRecord::printFields()
   {
   traceMsg(TR::comp(), "ComponentClassFromArrayClassRecord\n");
   TR::ClassValidationRecord::printFields();
   traceMsg(TR::comp(), "\t_componentClass=0x%p\n", _componentClass);
   printClass(_componentClass);
   traceMsg(TR::comp(), "\t_arrayClass=0x%p\n", _arrayClass);
   printClass(_arrayClass);
   }

void TR::ArrayClassFromComponentClassRecord::printFields()
   {
   traceMsg(TR::comp(), "ArrayClassFromComponentClassRecord\n");
   TR::ClassValidationRecord::printFields();
   traceMsg(TR::comp(), "\t_arrayClass=0x%p\n", _arrayClass);
   printClass(_arrayClass);
   traceMsg(TR::comp(), "\t_componentClass=0x%p\n", _componentClass);
   printClass(_componentClass);
   }

void TR::SuperClassFromClassRecord::printFields()
   {
   traceMsg(TR::comp(), "SuperClassFromClassRecord\n");
   TR::ClassValidationRecord::printFields();
   traceMsg(TR::comp(), "\t_superClass=0x%p\n", _superClass);
   printClass(_superClass);
   traceMsg(TR::comp(), "\t_childClass=0x%p\n", _childClass);
   printClass(_childClass);
   }

void TR::ClassInstanceOfClassRecord::printFields()
   {
   traceMsg(TR::comp(), "ClassInstanceOfClassRecord\n");
   traceMsg(TR::comp(), "\t_classOne=0x%p\n", _classOne);
   printClass(_classOne);
   traceMsg(TR::comp(), "\t_classTwo=0x%p\n", _classTwo);
   printClass(_classTwo);
   traceMsg(TR::comp(), "\t_objectTypeIsFixed=%s\n", _objectTypeIsFixed ? "true" : "false");
   traceMsg(TR::comp(), "\t_castTypeIsFixed=%s\n", _castTypeIsFixed ? "true" : "false");
   traceMsg(TR::comp(), "\t_isInstanceOf=%s\n", _isInstanceOf ? "true" : "false");
   }

void TR::SystemClassByNameRecord::printFields()
   {
   traceMsg(TR::comp(), "SystemClassByNameRecord\n");
   TR::ClassValidationRecord::printFields();
   traceMsg(TR::comp(), "\t_systemClass=0x%p\n", _systemClass);
   printClass(_systemClass);
   }

void TR::ClassFromITableIndexCPRecord::printFields()
   {
   traceMsg(TR::comp(), "ClassFromITableIndexCPRecord\n");
   TR::ClassValidationRecord::printFields();
   traceMsg(TR::comp(), "\t_class=0x%p\n", _class);
   printClass(_class);
   traceMsg(TR::comp(), "\t_beholder=0x%p\n", _beholder);
   printClass(_beholder);
   traceMsg(TR::comp(), "\t_cpIndex=%d\n", _cpIndex);
   }

void TR::DeclaringClassFromFieldOrStaticRecord::printFields()
   {
   traceMsg(TR::comp(), "DeclaringClassFromFieldOrStaticRecord\n");
   TR::ClassValidationRecord::printFields();
   traceMsg(TR::comp(), "\t_class=0x%p\n", _class);
   printClass(_class);
   traceMsg(TR::comp(), "\t_beholder=0x%p\n", _beholder);
   printClass(_beholder);
   traceMsg(TR::comp(), "\t_cpIndex=%d\n", _cpIndex);
   }

void TR::ClassClassRecord::printFields()
   {
   traceMsg(TR::comp(), "ClassClassRecord\n");
   TR::ClassValidationRecord::printFields();
   traceMsg(TR::comp(), "\t_classClass=0x%p\n", _classClass);
   printClass(_classClass);
   traceMsg(TR::comp(), "\t_objectClass=0x%p\n", _objectClass);
   printClass(_objectClass);
   }

void TR::ConcreteSubClassFromClassRecord::printFields()
   {
   traceMsg(TR::comp(), "ConcreteSubClassFromClassRecord\n");
   TR::ClassValidationRecord::printFields();
   traceMsg(TR::comp(), "\t_childClass=0x%p\n", _childClass);
   traceMsg(TR::comp(), "\t_superClass=0x%p\n", _superClass);
   }

void TR::ClassChainRecord::printFields()
   {
   traceMsg(TR::comp(), "ClassChainRecord\n");
   traceMsg(TR::comp(), "\t_class=0x%p\n", _class);
   printClass(_class);
   traceMsg(TR::comp(), "\t_classChain=0x%p\n", _classChain);
   }

void TR::MethodByNameRecord::printFields()
   {
   traceMsg(TR::comp(), "MethodByNameRecord\n");
   traceMsg(TR::comp(), "\t_method=0x%p\n", _method);
   traceMsg(TR::comp(), "\t_beholder=0x%p\n", _beholder);
   printClass(_beholder);
   }

void TR::MethodFromClassRecord::printFields()
   {
   traceMsg(TR::comp(), "MethodFromClassRecord\n");
   traceMsg(TR::comp(), "\t_method=0x%p\n", _method);
   traceMsg(TR::comp(), "\t_beholder=0x%p\n", _beholder);
   printClass(_beholder);
   traceMsg(TR::comp(), "\t_index=%u\n", _index);
   }

void TR::StaticMethodFromCPRecord::printFields()
   {
   traceMsg(TR::comp(), "StaticMethodFromCPRecord\n");
   traceMsg(TR::comp(), "\t_method=0x%p\n", _method);
   traceMsg(TR::comp(), "\t_beholder=0x%p\n", _beholder);
   printClass(_beholder);
   traceMsg(TR::comp(), "\t_cpIndex=%d\n", _cpIndex);
   }

void TR::SpecialMethodFromCPRecord::printFields()
   {
   traceMsg(TR::comp(), "SpecialMethodFromCPRecord\n");
   traceMsg(TR::comp(), "\t_method=0x%p\n", _method);
   traceMsg(TR::comp(), "\t_beholder=0x%p\n", _beholder);
   printClass(_beholder);
   traceMsg(TR::comp(), "\t_cpIndex=%d\n", _cpIndex);
   }

void TR::VirtualMethodFromCPRecord::printFields()
   {
   traceMsg(TR::comp(), "VirtualMethodFromCPRecord\n");
   traceMsg(TR::comp(), "\t_method=0x%p\n", _method);
   traceMsg(TR::comp(), "\t_beholder=0x%p\n", _beholder);
   printClass(_beholder);
   traceMsg(TR::comp(), "\t_cpIndex=%d\n", _cpIndex);
   }

void TR::VirtualMethodFromOffsetRecord::printFields()
   {
   traceMsg(TR::comp(), "VirtualMethodFromOffsetRecord\n");
   traceMsg(TR::comp(), "\t_method=0x%p\n", _method);
   traceMsg(TR::comp(), "\t_beholder=0x%p\n", _beholder);
   printClass(_beholder);
   traceMsg(TR::comp(), "\t_virtualCallOffset=%d\n", _virtualCallOffset);
   traceMsg(TR::comp(), "\t_ignoreRtResolve=%s\n", _ignoreRtResolve ? "true" : "false");
   }

void TR::InterfaceMethodFromCPRecord::printFields()
   {
   traceMsg(TR::comp(), "InterfaceMethodFromCPRecord\n");
   traceMsg(TR::comp(), "\t_method=0x%p\n", _method);
   traceMsg(TR::comp(), "\t_beholder=0x%p\n", _beholder);
   printClass(_beholder);
   traceMsg(TR::comp(), "\t_lookup=0x%p\n", _lookup);
   printClass(_lookup);
   traceMsg(TR::comp(), "\t_cpIndex=%d\n", _cpIndex);
   }

void TR::MethodFromClassAndSigRecord::printFields()
   {
   traceMsg(TR::comp(), "MethodFromClassAndSigRecord\n");
   traceMsg(TR::comp(), "\t_method=0x%p\n", _method);
   traceMsg(TR::comp(), "\t_methodClass=0x%p\n", _methodClass);
   printClass(_methodClass);
   traceMsg(TR::comp(), "\t_beholder=0x%p\n", _beholder);
   printClass(_beholder);
   }

void TR::StackWalkerMaySkipFramesRecord::printFields()
   {
   traceMsg(TR::comp(), "StackWalkerMaySkipFramesRecord\n");
   traceMsg(TR::comp(), "\t_method=0x%p\n", _method);
   traceMsg(TR::comp(), "\t_methodClass=0x%p\n", _methodClass);
   printClass(_methodClass);
   traceMsg(TR::comp(), "\t_skipFrames=%sp\n", _skipFrames ? "true" : "false");
   }

void TR::ClassInfoIsInitialized::printFields()
   {
   traceMsg(TR::comp(), "ClassInfoIsInitialized\n");
   traceMsg(TR::comp(), "\t_class=0x%p\n", _class);
   printClass(_class);
   traceMsg(TR::comp(), "\t_isInitialized=%sp\n", _isInitialized ? "true" : "false");
   }

void TR::MethodFromSingleImplementer::printFields()
   {
   traceMsg(TR::comp(), "MethodFromSingleImplementer\n");
   traceMsg(TR::comp(), "\t_method=0x%p\n", _method);
   traceMsg(TR::comp(), "\t_thisClass=0x%p\n", _thisClass);
   printClass(_thisClass);
   traceMsg(TR::comp(), "\t_cpIndexOrVftSlot=%d\n", _cpIndexOrVftSlot);
   traceMsg(TR::comp(), "\t_callerMethod=0x%p\n", _callerMethod);
   traceMsg(TR::comp(), "\t_useGetResolvedInterfaceMethod=%d\n", _useGetResolvedInterfaceMethod);
   }

void TR::MethodFromSingleInterfaceImplementer::printFields()
   {
   traceMsg(TR::comp(), "MethodFromSingleInterfaceImplementer\n");
   traceMsg(TR::comp(), "\t_method=0x%p\n", _method);
   traceMsg(TR::comp(), "\t_thisClass=0x%p\n", _thisClass);
   printClass(_thisClass);
   traceMsg(TR::comp(), "\t_cpIndex=%d\n", _cpIndex);
   traceMsg(TR::comp(), "\t_callerMethod=0x%p\n", _callerMethod);
   }

void TR::MethodFromSingleAbstractImplementer::printFields()
   {
   traceMsg(TR::comp(), "MethodFromSingleAbstractImplementer\n");
   traceMsg(TR::comp(), "\t_method=0x%p\n", _method);
   traceMsg(TR::comp(), "\t_thisClass=0x%p\n", _thisClass);
   printClass(_thisClass);
   traceMsg(TR::comp(), "\t_vftSlot=%d\n", _vftSlot);
   traceMsg(TR::comp(), "\t_callerMethod=0x%p\n", _callerMethod);
   }

void TR::ImproperInterfaceMethodFromCPRecord::printFields()
   {
   traceMsg(TR::comp(), "ImproperInterfaceMethodFromCPRecord\n");
   traceMsg(TR::comp(), "\t_method=0x%p\n", _method);
   traceMsg(TR::comp(), "\t_beholder=0x%p\n", _beholder);
   printClass(_beholder);
   traceMsg(TR::comp(), "\t_cpIndex=%d\n", _cpIndex);
   }
