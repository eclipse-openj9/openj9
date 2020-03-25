/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#include "compile/Compilation.hpp"
#if defined(J9VM_OPT_JITSERVER)
#include "control/CompilationRuntime.hpp"
#include "control/CompilationThread.hpp"
#include "control/JITServerHelpers.hpp"
#endif /* defined(J9VM_OPT_JITSERVER) */
#include "env/ClassEnv.hpp"
#include "env/CompilerEnv.hpp"
#include "env/jittypes.h"
#include "env/TypeLayout.hpp"
#include "env/VMJ9.h"
#include "j9.h"
#include "j9protos.h"
#include "j9cp.h"
#include "j9cfg.h"
#include "j9fieldsInfo.h"
#include "rommeth.h"


/*  REQUIRES STATE (_vmThread).  MOVE vmThread to COMPILATION

TR_OpaqueClassBlock *
J9::ClassEnv::getClassFromJavaLangClass(uintptr_t objectPointer)
   {
   return (TR_OpaqueClassBlock*)J9VM_J9CLASS_FROM_HEAPCLASS(_vmThread, objectPointer);
   }
*/

TR::ClassEnv *
J9::ClassEnv::self()
   {
   return static_cast<TR::ClassEnv *>(this);
   }

J9Class *
J9::ClassEnv::convertClassOffsetToClassPtr(TR_OpaqueClassBlock *clazzOffset)
   {
   // NOTE : We could pass down vmThread() in the call below if the conversion
   // required the VM thread. Currently it does not. If we did change that
   // such that the VM thread was reqd, we would need to handle AOT where the
   // TR_FrontEnd is created with a NULL J9VMThread object.
   //
   return (J9Class*)((TR_OpaqueClassBlock *)clazzOffset);
   }

TR_OpaqueClassBlock *
J9::ClassEnv::convertClassPtrToClassOffset(J9Class *clazzPtr)
   {
   // NOTE : We could pass down vmThread() in the call below if the conversion
   // required the VM thread. Currently it does not. If we did change that
   // such that the VM thread was reqd, we would need to handle AOT where the
   // TR_FrontEnd is created with a NULL J9VMThread object.
   //
   return (TR_OpaqueClassBlock*)(clazzPtr);
   }

bool
J9::ClassEnv::isClassSpecialForStackAllocation(TR_OpaqueClassBlock * clazz)
   {
   const UDATA mask = (J9AccClassReferenceWeak |
                       J9AccClassReferenceSoft |
                       J9AccClassFinalizeNeeded |
                       J9AccClassOwnableSynchronizer);

#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      uintptr_t classDepthAndFlags = 0;
      JITServerHelpers::getAndCacheRAMClassInfo((J9Class *)clazz, TR::compInfoPT->getClientData(), stream, JITServerHelpers::CLASSINFO_CLASS_DEPTH_AND_FLAGS, (void *)&classDepthAndFlags);
      return ((classDepthAndFlags & mask)?true:false);
      }
   else
#endif /* defined(J9VM_OPT_JITSERVER) */
      {
      if (((J9Class *)clazz)->classDepthAndFlags & mask)
         {
         return true;
         }
      }

   return false;
   }

uintptr_t
J9::ClassEnv::classFlagsValue(TR_OpaqueClassBlock * classPointer)
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      stream->write(JITServer::MessageType::ClassEnv_classFlagsValue, classPointer);
      return std::get<0>(stream->read<uintptr_t>());
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   return (TR::Compiler->cls.convertClassOffsetToClassPtr(classPointer)->classFlags);
   }

uintptr_t
J9::ClassEnv::classFlagReservableWordInitValue(TR_OpaqueClassBlock * classPointer)
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      uintptr_t classFlags = 0;
      JITServerHelpers::getAndCacheRAMClassInfo((J9Class *)classPointer, TR::compInfoPT->getClientData(), stream, JITServerHelpers::CLASSINFO_CLASS_FLAGS, (void *)&classFlags);
#ifdef DEBUG
      stream->write(JITServer::MessageType::ClassEnv_classFlagsValue, classPointer);
      uintptr_t classFlagsRemote = std::get<0>(stream->read<uintptr_t>());
      // Check that class flags from remote call is equal to the cached ones
      classFlags = classFlags & J9ClassReservableLockWordInit;
      classFlagsRemote = classFlagsRemote & J9ClassReservableLockWordInit;
      TR_ASSERT(classFlags == classFlagsRemote, "remote call class flags is not equal to cached class flags");
#endif
      return classFlags & J9ClassReservableLockWordInit;
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   return (TR::Compiler->cls.convertClassOffsetToClassPtr(classPointer)->classFlags) & J9ClassReservableLockWordInit;
   }

uintptr_t
J9::ClassEnv::classDepthOf(TR_OpaqueClassBlock * clazzPointer)
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      uintptr_t classDepthAndFlags = 0;
      JITServerHelpers::getAndCacheRAMClassInfo((J9Class *)clazzPointer, TR::compInfoPT->getClientData(), stream, JITServerHelpers::CLASSINFO_CLASS_DEPTH_AND_FLAGS, (void *)&classDepthAndFlags);
      return (classDepthAndFlags & J9AccClassDepthMask);
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   return J9CLASS_DEPTH(TR::Compiler->cls.convertClassOffsetToClassPtr(clazzPointer));
   }


uintptr_t
J9::ClassEnv::classInstanceSize(TR_OpaqueClassBlock * clazzPointer)
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      uintptr_t totalInstanceSize = 0;
      JITServerHelpers::getAndCacheRAMClassInfo((J9Class *)clazzPointer, TR::compInfoPT->getClientData(), stream, JITServerHelpers::CLASSINFO_TOTAL_INSTANCE_SIZE, (void *)&totalInstanceSize);
      return totalInstanceSize;
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   return TR::Compiler->cls.convertClassOffsetToClassPtr(clazzPointer)->totalInstanceSize;
   }


J9ROMClass *
J9::ClassEnv::romClassOf(TR_OpaqueClassBlock * clazz)
   {
   J9Class *j9clazz = TR::Compiler->cls.convertClassOffsetToClassPtr(clazz);
#if defined(J9VM_OPT_JITSERVER)
   if (TR::compInfoPT && TR::compInfoPT->getStream())
      {
      return TR::compInfoPT->getAndCacheRemoteROMClass(j9clazz);
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   return j9clazz->romClass;
   }

J9Class **
J9::ClassEnv::superClassesOf(TR_OpaqueClassBlock * clazz)
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      stream->write(JITServer::MessageType::ClassEnv_superClassesOf, clazz);
      return std::get<0>(stream->read<J9Class **>());
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   return TR::Compiler->cls.convertClassOffsetToClassPtr(clazz)->superclasses;
   }

J9ROMClass *
J9::ClassEnv::romClassOfSuperClass(TR_OpaqueClassBlock * clazz, size_t index)
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      stream->write(JITServer::MessageType::ClassEnv_indexedSuperClassOf, clazz, index);
      J9Class *j9clazz = std::get<0>(stream->read<J9Class *>());
      return TR::compInfoPT->getAndCacheRemoteROMClass(j9clazz);
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   return self()->superClassesOf(clazz)[index]->romClass;
   }

J9ITable *
J9::ClassEnv::iTableOf(TR_OpaqueClassBlock * clazz)
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      stream->write(JITServer::MessageType::ClassEnv_iTableOf, clazz);
      return std::get<0>(stream->read<J9ITable*>());
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   return (J9ITable*) self()->convertClassOffsetToClassPtr(clazz)->iTable;
   }

J9ITable *
J9::ClassEnv::iTableNext(J9ITable *current)
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      stream->write(JITServer::MessageType::ClassEnv_iTableNext, current);
      return std::get<0>(stream->read<J9ITable*>());
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   return current->next;
   }

J9ROMClass *
J9::ClassEnv::iTableRomClass(J9ITable *current)
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      stream->write(JITServer::MessageType::ClassEnv_iTableRomClass, current);
      return std::get<0>(stream->read<J9ROMClass*>());
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   return current->interfaceClass->romClass;
   }

#if defined(J9VM_OPT_JITSERVER)
std::vector<TR_OpaqueClassBlock *>
J9::ClassEnv::getITable(TR_OpaqueClassBlock *clazz)
   {
   if (auto stream = TR::CompilationInfo::getStream())
      {
      // This normally shouldn't be called from the server,
      // because it will have a cached table
      stream->write(JITServer::MessageType::ClassEnv_getITable, clazz);
      return std::get<0>(stream->read<std::vector<TR_OpaqueClassBlock *>>());
      }
   std::vector<TR_OpaqueClassBlock *> iTableSerialization;
   iTableSerialization.reserve((TR::Compiler->cls.romClassOf(clazz)->interfaceCount));
   for (J9ITable *iTableCur = TR::Compiler->cls.iTableOf(clazz); iTableCur; iTableCur = iTableCur->next)
      iTableSerialization.push_back((TR_OpaqueClassBlock *) iTableCur->interfaceClass);
   return iTableSerialization;
   }
#endif /* defined(J9VM_OPT_JITSERVER) */

bool
J9::ClassEnv::isStringClass(TR_OpaqueClassBlock *clazz)
   {
   //return (J9Class*)clazz == J9VMJAVALANGSTRING(jitConfig->javaVM);
   return false;
   }


bool
J9::ClassEnv::isStringClass(uintptr_t objectPointer)
   {
   /*
   TR_ASSERT(TR::Compiler->vm.hasAccess(omrVMThread), "isString requires VM access");
   return isString(getObjectClass(objectPointer));
   */
   return false;
   }

bool
J9::ClassEnv::isAbstractClass(TR::Compilation *comp, TR_OpaqueClassBlock *clazzPointer)
   {
   return comp->fej9()->isAbstractClass(clazzPointer);
   }

bool
J9::ClassEnv::isInterfaceClass(TR::Compilation *comp, TR_OpaqueClassBlock *clazzPointer)
   {
   return comp->fej9()->isInterfaceClass(clazzPointer);
   }

bool
J9::ClassEnv::isEnumClass(TR::Compilation *comp, TR_OpaqueClassBlock *clazzPointer, TR_ResolvedMethod *method)
   {
   return comp->fej9()->isEnumClass(clazzPointer, method);
   }

bool
J9::ClassEnv::isPrimitiveClass(TR::Compilation *comp, TR_OpaqueClassBlock *clazz)
   {
   return comp->fej9()->isPrimitiveClass(clazz);
   }

bool
J9::ClassEnv::isPrimitiveArray(TR::Compilation *comp, TR_OpaqueClassBlock *clazz)
   {
   return comp->fej9()->isPrimitiveArray(clazz);
   }

bool
J9::ClassEnv::isReferenceArray(TR::Compilation *comp, TR_OpaqueClassBlock *clazz)
   {
   return comp->fej9()->isReferenceArray(clazz);
   }

bool
J9::ClassEnv::isClassArray(TR::Compilation *comp, TR_OpaqueClassBlock *clazz)
   {
   return comp->fej9()->isClassArray(clazz);
   }

bool
J9::ClassEnv::isClassFinal(TR::Compilation *comp, TR_OpaqueClassBlock *clazz)
   {
   return comp->fej9()->isClassFinal(clazz);
   }

bool
J9::ClassEnv::hasFinalizer(TR::Compilation *comp, TR_OpaqueClassBlock *clazz)
   {
   return comp->fej9()->hasFinalizer(clazz);
   }

bool
J9::ClassEnv::isClassInitialized(TR::Compilation *comp, TR_OpaqueClassBlock *clazz)
   {
   return comp->fej9()->isClassInitialized(clazz);
   }

bool
J9::ClassEnv::classHasIllegalStaticFinalFieldModification(TR_OpaqueClassBlock * clazzPointer)
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      stream->write(JITServer::MessageType::ClassEnv_classHasIllegalStaticFinalFieldModification, clazzPointer);
      return std::get<0>(stream->read<bool>());
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   J9Class* j9clazz = TR::Compiler->cls.convertClassOffsetToClassPtr(clazzPointer);
   return J9_ARE_ANY_BITS_SET(j9clazz->classFlags, J9ClassHasIllegalFinalFieldModifications);
   }

bool
J9::ClassEnv::hasFinalFieldsInClass(TR::Compilation *comp, TR_OpaqueClassBlock *clazz)
   {
   return comp->fej9()->hasFinalFieldsInClass(clazz);
   }

bool
J9::ClassEnv::sameClassLoaders(TR::Compilation *comp, TR_OpaqueClassBlock *class1, TR_OpaqueClassBlock *class2)
   {
   return comp->fej9()->sameClassLoaders(class1, class2);
   }

bool
J9::ClassEnv::isString(TR::Compilation *comp, TR_OpaqueClassBlock *clazz)
   {
   return comp->fej9()->isString(clazz);
   }

bool
J9::ClassEnv::isString(TR::Compilation *comp, uintptr_t objectPointer)
   {
   return comp->fej9()->isString(objectPointer);
   }

bool
J9::ClassEnv::jitStaticsAreSame(
      TR::Compilation *comp,
      TR_ResolvedMethod * method1,
      int32_t cpIndex1,
      TR_ResolvedMethod * method2,
      int32_t cpIndex2)
   {
   return comp->fej9()->jitStaticsAreSame(method1, cpIndex1, method2, cpIndex2);
   }

bool
J9::ClassEnv::jitFieldsAreSame(
      TR::Compilation *comp,
      TR_ResolvedMethod * method1,
      int32_t cpIndex1,
      TR_ResolvedMethod * method2,
      int32_t cpIndex2,
      int32_t isStatic)
   {
   return comp->fej9()->jitFieldsAreSame(method1, cpIndex1, method2, cpIndex2, isStatic);
   }

bool
J9::ClassEnv::isAnonymousClass(TR::Compilation *comp, TR_OpaqueClassBlock *clazz)
   {
   return comp->fej9()->isAnonymousClass(clazz);
   }

const TR::TypeLayout*
J9::ClassEnv::enumerateFields(TR::Region& region, TR_OpaqueClassBlock * opaqueClazz, TR::Compilation *comp)
   {  
   J9Class *clazz = (J9Class*)opaqueClazz;
   TR_VMFieldsInfo fieldsInfo(comp, clazz, 1, stackAlloc);
   ListIterator<TR_VMField> iter(fieldsInfo.getFields());
   TR::TypeLayoutBuilder tlb(region);
   for (TR_VMField *field = iter.getFirst(); field; field = iter.getNext())
      {
      char *signature = field->signature;
      char charSignature = *signature;
      TR::DataType dataType;
      switch(charSignature)
         {
         case 'Z': 
         case 'B': 
         case 'C': 
         case 'S': 
         case 'I':
            {
            dataType = TR::Int32;
            break;
            }
         case 'J':
            {
            dataType = TR::Int64;
            break;
            }
         case 'F':
            {
            dataType = TR::Float;
            break;
            }
         case 'D':
            {
            dataType = TR::Double;
            break;
            }
         case 'L': 
         case '[':
            {
            dataType = TR::Address;
            break;
            }
         }
      size_t nameSize = strlen(field->name)+1;
      char *fieldName = new (region) char[nameSize];
      strncpy(fieldName, field->name, nameSize);
      TR_ASSERT_FATAL(fieldName[nameSize-1] == '\0', "fieldName buffer was too small.");
      int32_t offset = field->offset + TR::Compiler->om.objectHeaderSizeInBytes();
      bool isVolatile = (field->modifiers & J9AccVolatile) ? true : false;
      bool isPrivate = (field->modifiers & J9AccPrivate) ? true : false;
      bool isFinal = (field->modifiers & J9AccFinal) ? true : false;
      tlb.add(TR::TypeLayoutEntry(dataType, offset, fieldName, isVolatile, isPrivate, isFinal, signature));
      }
   return tlb.build();
   }

int32_t
J9::ClassEnv::vTableSlot(TR::Compilation *comp, TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *clazz)
   {
   return comp->fej9()->getVTableSlot(method, clazz);
   }


int32_t
J9::ClassEnv::flagValueForPrimitiveTypeCheck(TR::Compilation *comp)
   {
   return comp->fej9()->getFlagValueForPrimitiveTypeCheck();
   }


int32_t
J9::ClassEnv::flagValueForArrayCheck(TR::Compilation *comp)
   {
   return comp->fej9()->getFlagValueForArrayCheck();
   }


int32_t
J9::ClassEnv::flagValueForFinalizerCheck(TR::Compilation *comp)
   {
   return comp->fej9()->getFlagValueForFinalizerCheck();
   }


// this should be a method of TR_SymbolReference
char *
J9::ClassEnv::classNameChars(TR::Compilation *comp, TR::SymbolReference * symRef, int32_t & length)
   {
   return comp->fej9()->classNameChars(comp, symRef, length);
   }


char *
J9::ClassEnv::classNameChars(TR::Compilation *comp, TR_OpaqueClassBlock *clazz, int32_t & length)
   {
   return comp->fej9()->getClassNameChars(clazz, length);
   }


char *
J9::ClassEnv::classSignature_DEPRECATED(TR::Compilation *comp, TR_OpaqueClassBlock * clazz, int32_t & length, TR_Memory *m)
   {
   return comp->fej9()->getClassSignature_DEPRECATED(clazz, length, m);
   }


char *
J9::ClassEnv::classSignature(TR::Compilation *comp, TR_OpaqueClassBlock * clazz, TR_Memory *m)
   {
   return comp->fej9()->getClassSignature(clazz, m);
   }

uintptr_t
J9::ClassEnv::persistentClassPointerFromClassPointer(TR::Compilation *comp, TR_OpaqueClassBlock *clazz)
   {
   return comp->fej9()->getPersistentClassPointerFromClassPointer(clazz);
   }

TR_OpaqueClassBlock *
J9::ClassEnv::objectClass(TR::Compilation *comp, uintptr_t objectPointer)
   {
   return comp->fej9()->getObjectClass(objectPointer);
   }

TR_OpaqueClassBlock *
J9::ClassEnv::classFromJavaLangClass(TR::Compilation *comp, uintptr_t objectPointer)
   {
   return comp->fej9()->getClassFromJavaLangClass(objectPointer);
   }


uint16_t
J9::ClassEnv::getStringCharacter(TR::Compilation *comp, uintptr_t objectPointer, int32_t index)
   {
   return comp->fej9()->getStringCharacter(objectPointer, index);
   }


bool
J9::ClassEnv::getStringFieldByName(TR::Compilation *comp, TR::SymbolReference *stringRef, TR::SymbolReference *fieldRef, void* &pResult)
   {
   return comp->fej9()->getStringFieldByName(comp, stringRef, fieldRef, pResult);
   }

uintptr_t
J9::ClassEnv::getArrayElementWidthInBytes(TR::Compilation *comp, TR_OpaqueClassBlock* arrayClass)
   {
   TR_ASSERT(TR::Compiler->cls.isClassArray(comp, arrayClass), "Class must be array");
   int32_t logElementSize = ((J9ROMArrayClass*)((J9Class*)arrayClass)->romClass)->arrayShape & 0x0000FFFF;
   return 1 << logElementSize;
   }

intptr_t
J9::ClassEnv::getVFTEntry(TR::Compilation *comp, TR_OpaqueClassBlock* clazz, int32_t offset)
   {
   return comp->fej9()->getVFTEntry(clazz, offset);
   }

uint8_t *
J9::ClassEnv::getROMClassRefName(TR::Compilation *comp, TR_OpaqueClassBlock *clazz, uint32_t cpIndex, int &classRefLen)
   {
   J9ROMConstantPoolItem *romCP = self()->getROMConstantPool(comp, clazz);
#if defined(J9VM_OPT_JITSERVER)
   if (comp->isOutOfProcessCompilation())
      {
      J9ROMFieldRef *romFieldRef = (J9ROMFieldRef *)&romCP[cpIndex];
      TR_ASSERT(JITServerHelpers::isAddressInROMClass(romFieldRef, self()->romClassOf(clazz)), "Field ref must be in ROM class");

      J9ROMClassRef *romClassRef = (J9ROMClassRef *)&romCP[romFieldRef->classRefCPIndex];
      TR_ASSERT(JITServerHelpers::isAddressInROMClass(romClassRef, self()->romClassOf(clazz)), "Class ref must be in ROM class");

      TR::CompilationInfoPerThread *compInfoPT = TR::compInfoPT;
      char *name = NULL;

      OMR::CriticalSection getRemoteROMClass(compInfoPT->getClientData()->getROMMapMonitor()); 
      auto &classMap = compInfoPT->getClientData()->getROMClassMap();
      auto it = classMap.find(reinterpret_cast<J9Class *>(clazz));
      auto &classInfo = it->second;
      name = classInfo.getROMString(classRefLen, romClassRef,
                             {
                             offsetof(J9ROMClassRef, name)
                             });
      return (uint8_t *) name;
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   J9ROMFieldRef *romFieldRef = (J9ROMFieldRef *)&romCP[cpIndex];
   J9ROMClassRef *romClassRef = (J9ROMClassRef *)&romCP[romFieldRef->classRefCPIndex];
   J9UTF8 *classRefNameUtf8 = J9ROMCLASSREF_NAME(romClassRef);
   classRefLen = J9UTF8_LENGTH(classRefNameUtf8);
   uint8_t *classRefName = J9UTF8_DATA(classRefNameUtf8);
   return classRefName;
   }

J9ROMConstantPoolItem *
J9::ClassEnv::getROMConstantPool(TR::Compilation *comp, TR_OpaqueClassBlock *clazz)
   {
#if defined(J9VM_OPT_JITSERVER)
   if (comp->isOutOfProcessCompilation())
      {
      J9ROMClass *romClass = TR::compInfoPT->getAndCacheRemoteROMClass(reinterpret_cast<J9Class *>(clazz));
      return (J9ROMConstantPoolItem *) ((UDATA) romClass + sizeof(J9ROMClass));
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   J9ConstantPool *ramCP = reinterpret_cast<J9ConstantPool *>(comp->fej9()->getConstantPoolFromClass(clazz));
   return ramCP->romConstantPool;
   }

bool
J9::ClassEnv::isValueTypeClass(TR_OpaqueClassBlock *clazz)
   {
   J9Class *j9class = reinterpret_cast<J9Class*>(clazz);
   return J9_IS_J9CLASS_VALUETYPE(j9class);
   }
