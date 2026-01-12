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

#include "compile/Compilation.hpp"
#include "env/alloca_openxl.h"
#include "env/j9fieldsInfo.h"
#include "env/KnownObjectTable.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "env/VMAccessCriticalSection.hpp"
#include "env/VMJ9.h"
#include "infra/Assert.hpp"
#include "ras/Logger.hpp"
#include "j9.h"
#if defined(J9VM_OPT_JITSERVER)
#include "control/CompilationRuntime.hpp"
#endif /* defined(J9VM_OPT_JITSERVER) */


J9::KnownObjectTable::KnownObjectTable(TR::Compilation *comp) :
      OMR::KnownObjectTableConnector(comp),
      _objectInfos(comp->trMemory()),
      _stableArrayRanks(comp->trMemory())
   {
   _objectInfos.add(ObjectInfo()); // Reserve index zero for NULL
   }


TR::KnownObjectTable *
J9::KnownObjectTable::self()
   {
   return static_cast<TR::KnownObjectTable *>(this);
   }

TR::KnownObjectTable::Index
J9::KnownObjectTable::getEndIndex()
   {
   return _objectInfos.size();
   }


bool
J9::KnownObjectTable::isNull(Index index)
   {
   return index == 0;
   }


TR::KnownObjectTable::Index
J9::KnownObjectTable::getOrCreateIndex(uintptr_t objectPointer)
   {
   if (objectPointer == 0)
      return 0; // Special Index value for NULL

   uint32_t nextIndex = self()->getEndIndex();
#if defined(J9VM_OPT_JITSERVER)
   TR_ASSERT_FATAL(!self()->comp()->isOutOfProcessCompilation(), "It is not safe to call getOrCreateIndex() at the server. The object pointer could have become stale at the client.");
#endif /* defined(J9VM_OPT_JITSERVER) */
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(self()->fe());
   TR_ASSERT(fej9->haveAccess(), "Must haveAccess in J9::KnownObjectTable::getOrCreateIndex");

   // Search for existing matching entry
   //
   for (uint32_t i = 1; i < nextIndex; i++)
      if (*(_objectInfos.element(i)._jniReference) == objectPointer)
         return i;

   // No luck -- allocate a new one
   //
   J9VMThread *thread = getJ9VMThreadFromTR_VM(self()->fe());
   TR_ASSERT(thread, "assertion failure");
   _objectInfos.setSize(nextIndex+1);
   _objectInfos[nextIndex]._jniReference = (uintptr_t*)thread->javaVM->internalVMFunctions->j9jni_createLocalRef((JNIEnv*)thread, (j9object_t)objectPointer);
   // Fill out the remaining fields of the entry
   TR_OpaqueClassBlock *objClass = fej9->getObjectClass(objectPointer);
   _objectInfos[nextIndex]._isString = fej9->isString(objClass);
   _objectInfos[nextIndex]._jlClass = fej9->getClassClassPointer(objClass);
   _objectInfos[nextIndex]._isFixedJavaLangClass = (objClass == _objectInfos[nextIndex]._jlClass);
   if (_objectInfos[nextIndex]._isFixedJavaLangClass)
      {
      // When the class happens to be java/lang/Class we change that
      // to the class that the java/lang/Class object represents.
      _objectInfos[nextIndex]._clazz = fej9->getClassFromJavaLangClass(objectPointer);
      }
   else
      {
      _objectInfos[nextIndex]._clazz = objClass;
      }
   return nextIndex;
   }

TR::KnownObjectTable::Index
J9::KnownObjectTable::getOrCreateIndex(uintptr_t objectPointer, bool isArrayWithConstantElements)
   {
   TR::KnownObjectTable::Index index = self()->getOrCreateIndex(objectPointer);
   if (isArrayWithConstantElements)
      {
      self()->addArrayWithConstantElements(index);
      }
   return index;
   }


TR::KnownObjectTable::Index
J9::KnownObjectTable::getOrCreateIndexAt(uintptr_t *objectReferenceLocation)
   {
   TR::KnownObjectTable::Index result = UNKNOWN;
#if defined(J9VM_OPT_JITSERVER)
   if (self()->comp()->isOutOfProcessCompilation())
      {
      auto stream = self()->comp()->getStream();
      stream->write(JITServer::MessageType::KnownObjectTable_getOrCreateIndexAt, objectReferenceLocation);
      auto recv = stream->read<TR::KnownObjectTable::Index, struct ObjectInfo>();
      result = std::get<0>(recv);
      const struct ObjectInfo &objInfo = std::get<1>(recv);

      updateKnownObjectTableAtServer(result, objInfo);
      }
   else
#endif /* defined(J9VM_OPT_JITSERVER) */
      {
      TR::VMAccessCriticalSection getOrCreateIndexAtCriticalSection(self()->comp());
      uintptr_t objectPointer = *objectReferenceLocation; // Note: object references held as uintptr_t must never be compressed refs
      result = self()->getOrCreateIndex(objectPointer);
      }
   return result;
   }

TR::KnownObjectTable::Index
J9::KnownObjectTable::getOrCreateIndexAt(uintptr_t *objectReferenceLocation, bool isArrayWithConstantElements)
   {
   Index result = self()->getOrCreateIndexAt(objectReferenceLocation);
   if (isArrayWithConstantElements)
      self()->addArrayWithConstantElements(result);
   return result;
   }


TR::KnownObjectTable::Index
J9::KnownObjectTable::getExistingIndexAt(uintptr_t *objectReferenceLocation)
   {
   TR::KnownObjectTable::Index result = UNKNOWN;
#if defined(J9VM_OPT_JITSERVER)
   if (self()->comp()->isOutOfProcessCompilation())
      {
      auto stream = TR::CompilationInfo::getStream();
      stream->write(JITServer::MessageType::KnownObjectTable_getExistingIndexAt, objectReferenceLocation);
      result = std::get<0>(stream->read<TR::KnownObjectTable::Index>());
      }
   else
#endif /* defined(J9VM_OPT_JITSERVER) */
      {
      TR::VMAccessCriticalSection getExistingIndexAtCriticalSection(self()->comp());

      uintptr_t objectPointer = *objectReferenceLocation;
      for (Index i = 0; i < self()->getEndIndex() && (result == UNKNOWN); i++)
         {
         if (self()->getPointer(i) == objectPointer)
            {
            result = i;
            break;
            }
         }
      }
   return result;
   }


uintptr_t
J9::KnownObjectTable::getPointer(Index index)
   {
   if (self()->isNull(index))
      {
      return 0; // Assumes host and target representations of null match each other
      }
   else
      {
#if defined(J9VM_OPT_JITSERVER)
      TR_ASSERT_FATAL(!self()->comp()->isOutOfProcessCompilation(), "It is not safe to call getPointer() at the server. The object pointer could have become stale at the client.");
#endif /* defined(J9VM_OPT_JITSERVER) */
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(self()->fe());
      TR_ASSERT(fej9->haveAccess(), "Must haveAccess in J9::KnownObjectTable::getPointer");
      return *self()->getPointerLocation(index);
      }
   }


uintptr_t *
J9::KnownObjectTable::getPointerLocation(Index index)
   {
   TR_ASSERT(index != UNKNOWN && 0 <= index && index < _objectInfos.size(), "getPointerLocation(%d): index must be in range 0..%d", (int)index, _objectInfos.size());
   return _objectInfos[index]._jniReference;
   }

void
J9::KnownObjectTable::setObjectInfoFields(Index index, const ObjectInfo& objInfoSource)
   {
   ObjectInfo &objInfo = _objectInfos[index];
   TR_ASSERT_FATAL(objInfo._jniReference, "Index %d from known object table must have a valid jniReference", index);
   TR_ASSERT_FATAL(objInfo._jniReference == objInfoSource._jniReference, "The _jniReference for source and destination do not match. Index=%d %p != %p", index, objInfo._jniReference, objInfoSource._jniReference);
   objInfo = objInfoSource;
   }

// Delete all the JNI references to the known objects tracked by the table
void
J9::KnownObjectTable::freeKnownObjectTable()
   {
#if defined(J9VM_OPT_JITSERVER)
   if (comp()->isOutOfProcessCompilation())
      return;
#endif /* defined(J9VM_OPT_JITSERVER) */

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(self()->fe());
   J9VMThread *thread = fej9->vmThread();
   TR_ASSERT(thread, "assertion failure");

   TR::VMAccessCriticalSection freeKnownObjectTable(fej9);
   uint32_t nextIndex = self()->getEndIndex();
   for (uint32_t i = 1; i < nextIndex; i++)
      thread->javaVM->internalVMFunctions->j9jni_deleteLocalRef((JNIEnv*)thread, (jobject)_objectInfos.element(i)._jniReference);
   }

#if defined(J9VM_OPT_JITSERVER)
void
J9::KnownObjectTable::updateKnownObjectTableAtServer(Index index, uintptr_t *objectReferenceLocationClient, bool isArrayWithConstantElements)
   {
   TR_ASSERT_FATAL(self()->comp()->isOutOfProcessCompilation(), "updateKnownObjectTableAtServer should only be called at the server");
   if (index == TR::KnownObjectTable::UNKNOWN)
      return;

   TR_ASSERT(objectReferenceLocationClient || (index == 0), "objectReferenceLocationClient should not be NULL (index=%d)", index);

   uint32_t nextIndex = self()->getEndIndex();

   if (index == nextIndex)
      {
      _objectInfos.setSize(nextIndex+1);
      _objectInfos[nextIndex]._jniReference = objectReferenceLocationClient;
      }
   else if (index < nextIndex)
      {
      TR_ASSERT_FATAL((objectReferenceLocationClient == _objectInfos[index]._jniReference),
            "comp %p: server _references[%d]=%p is not the same as the client _references[%d]=%p (total size = %u)",
            self()->comp(), index, _objectInfos[index]._jniReference, index, objectReferenceLocationClient, nextIndex);
      }
   else
      {
      TR_ASSERT_FATAL(false, "index %d from the client is greater than the KOT nextIndex %d at the server", index, nextIndex);
      }

   if (isArrayWithConstantElements)
      addArrayWithConstantElements(index);
   }

void
J9::KnownObjectTable::updateKnownObjectTableAtServer(Index index, const struct ObjectInfo &objInfo, bool isArrayWithConstantElements)
   {
   TR_ASSERT_FATAL(self()->comp()->isOutOfProcessCompilation(), "updateKnownObjectTableAtServer should only be called at the server");
   if (index == TR::KnownObjectTable::UNKNOWN)
      return;

   TR_ASSERT(objInfo._jniReference || (index == 0), "jniReference should not be NULL (index=%d)", index);

   uint32_t nextIndex = self()->getEndIndex();

   if (index == nextIndex) // Adding a new element
      {
      _objectInfos.setSize(nextIndex+1); // Growing the size of the KNOT
      _objectInfos[nextIndex] = objInfo;
      }
   else if (index < nextIndex)
      {
      TR_ASSERT_FATAL((objInfo._jniReference == _objectInfos[index]._jniReference),
            "comp %p: server _references[%d]=%p is not the same as the client _references[%d]=%p (total size = %u)",
            self()->comp(), index, _objectInfos[index]._jniReference, index, objInfo._jniReference, nextIndex);
      }
   else
      {
      TR_ASSERT_FATAL(false, "index %d from the client is greater than the KOT nextIndex %d at the server", index, nextIndex);
      }

   if (isArrayWithConstantElements)
      addArrayWithConstantElements(index);
   }
#endif /* defined(J9VM_OPT_JITSERVER) */


static int32_t simpleNameOffset(const char *className, int32_t len)
   {
   int32_t result;
   for (result = len; result > 0 && className[result-1] != '/'; result--)
      {}
   return result;
   }

void
J9::KnownObjectTable::dumpObjectTo(
      OMR::Logger *log,
      Index i,
      const char *fieldName,
      const char *sep,
      TR::Compilation *comp,
      TR_BitVector &visited,
      TR_VMFieldsInfo **fieldsInfoByIndex,
      int32_t depth)
   {
   TR_ASSERT_FATAL(!comp->isOutOfProcessCompilation(), "dumpObjectTo() should not be executed at the server.");

   TR_J9VMBase *j9fe = (TR_J9VMBase*)self()->fe();
   int32_t indent = 2*depth;
   if (comp->getKnownObjectTable()->isNull(i))
      {
      // Usually don't care about null fields
      return;
      }
   else if (visited.isSet(i))
      {
      log->printf("%*s%s%sobj%d\n", indent, "", fieldName, sep, i);
      return;
      }
   else
      {
      visited.set(i);

      uintptr_t *ref = self()->getPointerLocation(i);
      int32_t len; char *className = TR::Compiler->cls.classNameChars(comp, j9fe->getObjectClass(*ref), len);
      J9MemoryManagerFunctions * mmf = jitConfig->javaVM->memoryManagerFunctions;
      int32_t hashCode = mmf->j9gc_objaccess_getObjectHashCode(jitConfig->javaVM, (J9Object*)(*ref));

      // Shorten the class name for legibility.  The full name is still in the ordinary known-object table dump.
      //
      int32_t offs = simpleNameOffset(className, len);
      log->printf("%*s%s%sobj%d @ %p hash %8x %.*s", indent, "", fieldName, sep, i, *ref, hashCode, len-offs, className+offs);

#if defined(J9VM_OPT_METHOD_HANDLE)
      if (len == 29 && !strncmp("java/lang/invoke/DirectHandle", className, 29))
         {
         J9Method *j9method  = (J9Method*)J9VMJAVALANGINVOKEPRIMITIVEHANDLE_VMSLOT(j9fe->vmThread(), (J9Object*)(*ref));
         J9UTF8   *className = J9ROMCLASS_CLASSNAME(J9_CLASS_FROM_METHOD(j9method)->romClass);
         J9UTF8   *methName  = J9ROMMETHOD_NAME(static_cast<TR_J9VM *>(j9fe)->getROMMethodFromRAMMethod(j9method));
         int32_t offs = simpleNameOffset(utf8Data(className), J9UTF8_LENGTH(className));
         log->printf("  vmSlot: %.*s.%.*s",
            J9UTF8_LENGTH(className)-offs, utf8Data(className)+offs,
            J9UTF8_LENGTH(methName),       utf8Data(methName));
         }
#endif /* defined(J9VM_OPT_METHOD_HANDLE) */

      TR_VMFieldsInfo *fieldsInfo = fieldsInfoByIndex[i];
      if (fieldsInfo)
         {
         ListIterator<TR_VMField> primitiveIter(fieldsInfo->getFields());
         for (TR_VMField *field = primitiveIter.getFirst(); field; field = primitiveIter.getNext())
            {
            if (field->isReference())
               continue;
            if (!strcmp(field->signature, "I"))
               log->printf("  %s: %d", field->name, j9fe->getInt32Field(*ref, field->name));
            }
         log->println();
         ListIterator<TR_VMField> refIter(fieldsInfo->getFields());
         for (TR_VMField *field = refIter.getFirst(); field; field = refIter.getNext())
            {
            if (field->isReference())
               {
               uintptr_t target = j9fe->getReferenceField(*ref, field->name, field->signature);
               Index targetIndex = self()->getExistingIndexAt(&target);
               if (targetIndex != UNKNOWN)
                  self()->dumpObjectTo(log, targetIndex, field->name, (field->modifiers & J9AccFinal)? " is " : " = ", comp, visited, fieldsInfoByIndex, depth+1);
               }
            }
         }
      else
         {
         log->println();
         }
      }
   }


#if defined(J9VM_OPT_JITSERVER)
void
J9::KnownObjectTable::getKnownObjectTableDumpInfo(std::vector<TR_KnownObjectTableDumpInfo> &knotDumpInfoList)
   {
   TR_ASSERT_FATAL(!self()->comp()->isOutOfProcessCompilation(), "getKnownObjectTableDumpInfo() can not be executed at the server.");

   TR_J9VMBase *j9fe = (TR_J9VMBase*)self()->fe();
   J9MemoryManagerFunctions * mmf = jitConfig->javaVM->memoryManagerFunctions;
   TR::KnownObjectTable::Index endIndex = self()->getEndIndex();
   TR::VMAccessCriticalSection j9KnownObjectTableDumpToCriticalSection(j9fe,
                                                                        TR::VMAccessCriticalSection::tryToAcquireVMAccess,
                                                                        self()->comp());
   if (j9KnownObjectTableDumpToCriticalSection.hasVMAccess())
      {
      uintptr_t *ref = NULL;
      int32_t   len = 0;
      char      *className = NULL;
      int32_t   hashCode = 0;

      for (uint32_t i = 0; i < endIndex; i++)
         {
         if (self()->isNull(i))
            {
            knotDumpInfoList.push_back(make_tuple(TR_KnownObjectTableDumpInfoStruct(NULL, 0, 0), std::string("")));
            }
         else
            {
            ref = self()->getPointerLocation(i);
            hashCode = mmf->j9gc_objaccess_getObjectHashCode(jitConfig->javaVM, (J9Object*)(*ref));
            className = j9fe->getClassNameChars(j9fe->getObjectClass(*ref), len);

            knotDumpInfoList.push_back(make_tuple(TR_KnownObjectTableDumpInfoStruct(ref, *ref, hashCode), std::string(className, len)));
            }
         }
      }
   }
#endif /* defined(J9VM_OPT_JITSERVER) */


void
J9::KnownObjectTable::dumpTo(OMR::Logger *log, TR::Compilation *comp)
   {
   TR::KnownObjectTable::Index endIndex = self()->getEndIndex();
#if defined(J9VM_OPT_JITSERVER)
   if (comp->isOutOfProcessCompilation())
      {
      auto stream = comp->getStream();
      stream->write(JITServer::MessageType::KnownObjectTable_getKnownObjectTableDumpInfo, JITServer::Void());

      auto recv = stream->read<std::vector<TR_KnownObjectTableDumpInfo>>();
      auto &knotDumpInfoList = std::get<0>(recv);

      uint32_t numOfEntries = knotDumpInfoList.size();
      TR_ASSERT_FATAL((numOfEntries == endIndex), "The client table size %u is different from the server table size %u", numOfEntries, endIndex);

      log->printf("<knownObjectTable size=\"%u\"> // ", numOfEntries);
      int32_t pointerLen = log->printf("%p", this);
      log->printf("\n  %-6s   %-*s   %-*s %-8s   Class\n", "id", pointerLen, "JNI Ref", pointerLen, "Address", "Hash");

      for (uint32_t i = 0; i < numOfEntries; i++)
         {
         log->printf("  obj%-3d", i);
         if (!std::get<0>(knotDumpInfoList[i]).ref)
            log->printf("   %*s   NULL\n", pointerLen, "");
         else
            {
            log->printf("   %p   %p %8x   %.*s\n",
                  std::get<0>(knotDumpInfoList[i]).ref,
                  std::get<0>(knotDumpInfoList[i]).objectPointer,
                  std::get<0>(knotDumpInfoList[i]).hashCode,
                  std::get<1>(knotDumpInfoList[i]).size(),
                  std::get<1>(knotDumpInfoList[i]).c_str());
            }
         }
      log->prints("</knownObjectTable>\n");

      if (comp->getOption(TR_TraceKnownObjectGraph))
         {
         log->prints("<knownObjectGraph>\n");
         // JITServer KOT TODO
         log->prints("</knownObjectGraph>\n");
         }
      }
   else
#endif /* defined(J9VM_OPT_JITSERVER) */
      {
      TR_J9VMBase *j9fe = (TR_J9VMBase*)self()->fe();
      J9MemoryManagerFunctions * mmf = jitConfig->javaVM->memoryManagerFunctions;
      TR::VMAccessCriticalSection j9KnownObjectTableDumpToCriticalSection(j9fe,
                                                                           TR::VMAccessCriticalSection::tryToAcquireVMAccess,
                                                                           comp);

      if (j9KnownObjectTableDumpToCriticalSection.hasVMAccess())
         {
         log->printf("<knownObjectTable size=\"%d\"> // ", endIndex);
         int32_t pointerLen = log->printf("%p", this);
         log->printf("\n  %-6s   %-*s   %-*s %-8s   Class\n", "id", pointerLen, "JNI Ref", pointerLen, "Address", "Hash");
         for (Index i = 0; i < endIndex; i++)
            {
            log->printf("  obj%-3d", i);
            if (self()->isNull(i))
               log->printf("   %*s   NULL\n", pointerLen, "");
            else
               {
               uintptr_t *ref = self()->getPointerLocation(i);
               int32_t len; char *className = TR::Compiler->cls.classNameChars(comp, j9fe->getObjectClass(*ref), len);
               int32_t hashCode = mmf->j9gc_objaccess_getObjectHashCode(jitConfig->javaVM, (J9Object*)(*ref));
               log->printf("   %p   %p %8x   %.*s", ref, *ref, hashCode, len, className);

               if (isArrayWithStableElements(i))
                  log->printf(" (%d dimension stable array)", getArrayWithStableElementsRank(i));

               log->println();
               }
            }
         log->prints("</knownObjectTable>\n");

         if (comp->getOption(TR_TraceKnownObjectGraph))
            {
            log->prints("<knownObjectGraph>\n");

            Index i;

            {
            TR::StackMemoryRegion stackMemoryRegion(*comp->trMemory());

            // Collect field info and determine which objects are reachable from other objects
            //
            TR_BitVector reachable(endIndex, comp->trMemory(), stackAlloc, notGrowable);
            TR_VMFieldsInfo **fieldsInfoByIndex = (TR_VMFieldsInfo**)alloca(endIndex * sizeof(TR_VMFieldsInfo*));
            for (i = 1; i < endIndex; i++)
               {
               uintptr_t    object = self()->getPointer(i);
               J9Class      *clazz  = (J9Class*)j9fe->getObjectClass(object);
               if (clazz->romClass->modifiers & J9AccClassArray)
                  {
                  fieldsInfoByIndex[i] = NULL;
                  continue; // TODO: Print out what reference arrays contain?
                  }
               fieldsInfoByIndex[i] = new (comp->trStackMemory()) TR_VMFieldsInfo(comp, clazz, 1, stackAlloc);
               ListIterator<TR_VMField> fieldIter(fieldsInfoByIndex[i]->getFields());
               for (TR_VMField *field = fieldIter.getFirst(); field; field = fieldIter.getNext())
                  {
                  // For the purpose of "reachability", we only look at final
                  // fields.  The intent is to reduce nondeterminism in the object
                  // graph log.
                  //
                  if (field->isReference() && (field->modifiers & J9AccFinal))
                     {
                     uintptr_t target = j9fe->getReferenceField(object, field->name, field->signature);
                     Index targetIndex = self()->getExistingIndexAt(&target);
                     if (targetIndex != UNKNOWN)
                        reachable.set(targetIndex);
                     }
                  }
               }

            // At the top level, walk objects not reachable from other objects
            //
            TR_BitVector visited(endIndex, comp->trMemory(), stackAlloc, notGrowable);
            for (i = 1; i < endIndex; i++)
               {
               if (!reachable.isSet(i) && !visited.isSet(i))
                  self()->dumpObjectTo(log, i, "", "", comp, visited, fieldsInfoByIndex, 0);
               }

            } // scope of the stack memory region

            log->prints("</knownObjectGraph>\n");
            }
         }
      else
         {
         log->printf("<knownObjectTable size=\"%d\"/> // unable to acquire VM access to print table contents\n", endIndex);
         }
      }
   }

void
J9::KnownObjectTable::addStableArray(Index index, int32_t stableArrayRank)
   {
   TR_J9VMBase *fej9 = static_cast<TR_J9VMBase*>(fe());
   TR_OpaqueClassBlock *clazz =
      fej9->getObjectClassFromKnownObjectIndex(comp(), index);

   TR_ASSERT_FATAL(
      fej9->isClassArray(clazz),
      "addStableArray can only be called for arrays");

   if (_stableArrayRanks[index] < stableArrayRank)
      {
      _stableArrayRanks[index] = stableArrayRank;
      }
   }

bool
J9::KnownObjectTable::isArrayWithStableElements(Index index)
   {
   TR_ASSERT_FATAL(index != UNKNOWN && 0 <= index && index < self()->getEndIndex(), "isArrayWithStableElements(%d): index must be in range 0..%d", index, self()->getEndIndex());

   return (index < _stableArrayRanks.size() && _stableArrayRanks[index] > 0);
   }


int32_t
J9::KnownObjectTable::getArrayWithStableElementsRank(Index index)
   {
   TR_ASSERT_FATAL(index != UNKNOWN && 0 <= index && index < self()->getEndIndex(), "getArrayWithStableElementsRank(%d): index must be in range 0..%d", index, self()->getEndIndex());

   return ((index < _stableArrayRanks.size()) ? _stableArrayRanks[index] : 0);
   }
