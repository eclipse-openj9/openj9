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
#if defined(J9VM_OPT_JITSERVER)
#include "control/CompilationRuntime.hpp"
#include "control/CompilationThread.hpp"
#include "control/JITServerHelpers.hpp"
#include "runtime/JITClientSession.hpp"
#endif /* defined(J9VM_OPT_JITSERVER) */
#include "env/ClassEnv.hpp"
#include "env/CompilerEnv.hpp"
#include "env/jittypes.h"
#include "env/TypeLayout.hpp"
#include "env/VMJ9.h"
#include "j9.h"
#include "j9cfg.h"
#include "j9cp.h"
#include "j9fieldsInfo.h"
#include "j9nonbuilder.h"
#include "j9protos.h"
#include "rommeth.h"
#include "runtime/RuntimeAssumptions.hpp"

class TR_PersistentClassInfo;
template <typename ListKind> class List;

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
                       J9AccClassOwnableSynchronizer |
                       J9AccClassContinuation );

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
J9::ClassEnv::isConcreteClass(TR::Compilation *comp, TR_OpaqueClassBlock * clazzPointer)
   {
   return comp->fej9()->isConcreteClass(clazzPointer);
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

TR::DataTypes
J9::ClassEnv::primitiveArrayComponentType(TR::Compilation *comp, TR_OpaqueClassBlock *clazz)
   {
   TR_ASSERT_FATAL(
      self()->isPrimitiveArray(comp, clazz), "not a primitive array: %p", clazz);

   static const int32_t firstNewArrayTypeCode = 4;
   static const TR::DataTypes newArrayTypes[] =
      {
      TR::Int8, // boolean
      TR::Int16, // char
      TR::Float,
      TR::Double,
      TR::Int8,
      TR::Int16,
      TR::Int32,
      TR::Int64,
      };

   TR_J9VMBase *fej9 = comp->fej9();
   for (int32_t i = 0; i < sizeof(newArrayTypes) / sizeof(newArrayTypes[0]); i++)
      {
      TR_OpaqueClassBlock *primitiveArrayClass =
         fej9->getClassFromNewArrayTypeNonNull(firstNewArrayTypeCode + i);

      if (clazz == primitiveArrayClass)
         return newArrayTypes[i];
      }

   return TR::NoType;
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
J9::ClassEnv::isClassVisible(TR::Compilation *comp, TR_OpaqueClassBlock *sourceClass, TR_OpaqueClassBlock *destClass)
   {
   return comp->fej9()->isClassVisible(sourceClass, destClass);
   }

bool
J9::ClassEnv::classHasIllegalStaticFinalFieldModification(TR_OpaqueClassBlock * clazzPointer)
   {
   J9Class* j9clazz = TR::Compiler->cls.convertClassOffsetToClassPtr(clazzPointer);
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      // J9ClassHasIllegalFinalFieldModifications bit is cached by ClientSessionData::processIllegalFinalFieldModificationList()
      // before the compilation takes place.
      uintptr_t classFlags = 0;
      JITServerHelpers::getAndCacheRAMClassInfo(j9clazz, TR::compInfoPT->getClientData(), stream, JITServerHelpers::CLASSINFO_CLASS_FLAGS, (void *)&classFlags);
      return J9_ARE_ANY_BITS_SET(classFlags, J9ClassHasIllegalFinalFieldModifications);
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
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

/*
 * Merges the prefix with the field name to a new string.
 *
 * \param prefix
 *    prefix could be NULL.
 * \param prefixLength
 *    The length of the prefix string.
 * \param mergedLength
 *    The merged length of the concatenated field name that is returned to the caller.
 */
static char * mergeFieldNames(char *prefix, uint32_t prefixLength, J9ROMFieldShape *field,
                              TR::Region &region, uint32_t &mergedLength)
   {
   J9UTF8 *fieldNameUTF = J9ROMFIELDSHAPE_NAME(field);
   char *fieldName = reinterpret_cast<char *>(J9UTF8_DATA(fieldNameUTF));
   uint32_t nameLength = J9UTF8_LENGTH(fieldNameUTF);

   mergedLength = nameLength + prefixLength;
   mergedLength++; // for adding '\0' at the end

   char *newName = new (region) char[mergedLength];

   if (prefixLength > 0)
      strncpy(newName, prefix, prefixLength);
   strncpy(newName + prefixLength, fieldName, nameLength);

   newName[mergedLength-1] = '\0';

   return newName;
   }

/*
 * Builds a new string with the prefix and the field name and appends the string with "." to be used
 * as a part of the flattened field chain name.
 *
 * \param prefix
 *    prefix could be ended with `.` or NULL.
 * \param prefixLength
 *    The length of the prefix string.
 * \param mergedLength
 *    The merged length of the concatenated field name that is returned to the caller.
 */
static char * buildTransitiveFieldNames(char *prefix, uint32_t prefixLength, J9ROMFieldShape *field,
                                        TR::Region &region, uint32_t &mergedLength)
   {
   J9UTF8 *fieldNameUTF = J9ROMFIELDSHAPE_NAME(field);
   char *fieldName = reinterpret_cast<char *>(J9UTF8_DATA(fieldNameUTF));
   uint32_t nameLength = J9UTF8_LENGTH(fieldNameUTF);

   mergedLength = nameLength + prefixLength;
   mergedLength++; // for appending '.'
   mergedLength++; // for adding '\0' at the end

   char *newName = new (region) char[mergedLength];

   if (prefixLength > 0)
      strncpy(newName, prefix, prefixLength);
   strncpy(newName + prefixLength, fieldName, nameLength);

   newName[mergedLength-2] = '.';
   newName[mergedLength-1] = '\0';

   return newName;
   }

static void addEntryForFieldImpl(TR_VMField *field, TR::TypeLayoutBuilder &tlb, TR::Region &region, J9Class *definingClass,
                                 char *prefix, uint32_t prefixLength, IDATA offsetBase, TR::Compilation *comp)
   {
   J9JavaVM *vm = comp->fej9()->getJ9JITConfig()->javaVM;
   bool trace = comp->getOption(TR_TraceILGen);
   uint32_t mergedLength = 0;
   J9UTF8 *signature = J9ROMFIELDSHAPE_SIGNATURE(field->shape);

   bool isFieldPrimitiveValueType = false;

   if (TR::Compiler->om.areFlattenableValueTypesEnabled())
      {
      if (TR::Compiler->om.isQDescriptorForValueTypesSupported())
         {
         isFieldPrimitiveValueType = vm->internalVMFunctions->isNameOrSignatureQtype(signature);
         }
      else
         {
         TR_ASSERT_FATAL(false, "Support for null-restricted types without Q descriptor is to be implemented!!!");
         }
      }

   if (isFieldPrimitiveValueType &&
       vm->internalVMFunctions->isFlattenableFieldFlattened(definingClass, field->shape))
      {
      char *prefixForChild = buildTransitiveFieldNames(prefix, prefixLength, field->shape, comp->trMemory()->currentStackRegion(), mergedLength);
      uint32_t prefixLengthForChild = mergedLength-1;
      IDATA offsetBaseForChild = field->offset + offsetBase;

      if (trace)
         traceMsg(comp, "field %s:%s is flattened. offset from TR_VMField %d, offset from fcc %d\n",
            field->name, field->signature, field->offset,
            vm->internalVMFunctions->getFlattenableFieldOffset(definingClass, field->shape));

      J9Class *fieldClass = vm->internalVMFunctions->getFlattenableFieldType(definingClass, field->shape);
      TR_VMFieldsInfo fieldsInfo(comp, fieldClass, 1, stackAlloc);
      ListIterator<TR_VMField> iter(fieldsInfo.getFields());
      for (TR_VMField *childField = iter.getFirst(); childField; childField = iter.getNext())
         {
         IDATA offsetBaseForChild = field->offset + offsetBase;
         /* Types with fields (flat or non-flat) that require double (64bit) alignment are pre-padded if there isn't
          * a smaller type that can be used as pre-padding. Pre-padding is eliminated when a type is flattened within
          * its container. As a result pre-padding must be subtracted from the base offset of the flattened field.
          */
         offsetBaseForChild -= J9CLASS_PREPADDING_SIZE(fieldClass);

         addEntryForFieldImpl(childField, tlb, region, fieldClass, prefixForChild, prefixLengthForChild, offsetBaseForChild, comp);
         }
      }
   else
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
         case 'Q':
         case '[':
            {
            dataType = TR::Address;
            break;
            }
         }

      char *fieldName = mergeFieldNames(prefix, prefixLength, field->shape, region, mergedLength);
      int32_t offset = offsetBase + field->offset + TR::Compiler->om.objectHeaderSizeInBytes();
      bool isVolatile = (field->modifiers & J9AccVolatile) ? true : false;
      bool isPrivate = (field->modifiers & J9AccPrivate) ? true : false;
      bool isFinal = (field->modifiers & J9AccFinal) ? true : false;

      int sigLen = strlen(signature);
      char *fieldSignature = new (region) char[sigLen+1];
      memcpy(fieldSignature, signature, sigLen);
      fieldSignature[sigLen] = '\0';

      if (trace)
         traceMsg(comp, "type layout definingClass %p field: %s signature: %s field offset: %d offsetBase %d\n", definingClass, fieldName, fieldSignature, field->offset, offsetBase);
      tlb.add(TR::TypeLayoutEntry(dataType, offset, fieldName, isVolatile, isPrivate, isFinal, fieldSignature));
      }
   }

static void addEntryForField(TR_VMField *field, TR::TypeLayoutBuilder &tlb, TR::Region &region, TR_OpaqueClassBlock *opaqueClazz, TR::Compilation *comp)
   {
   char *prefix = NULL;
   uint32_t prefixLength = 0;
   IDATA offsetBase = 0;
   J9Class *definingClass = reinterpret_cast<J9Class*>(opaqueClazz);

   addEntryForFieldImpl(field, tlb, region, definingClass, prefix, prefixLength, offsetBase, comp);
   }

const TR::TypeLayout *
J9::ClassEnv::enumerateFields(TR::Region &region, TR_OpaqueClassBlock *opaqueClazz, TR::Compilation *comp)
   {
   TR::TypeLayoutBuilder tlb(region);
#if defined(J9VM_OPT_JITSERVER)
   if (comp->isOutOfProcessCompilation())
      {
      auto stream = comp->getStream();
      stream->write(JITServer::MessageType::ClassEnv_enumerateFields, opaqueClazz);
      auto recv = stream->read<std::vector<TR::TypeLayoutEntry>, std::vector<std::string>, std::vector<std::string>>();
      auto &entries = std::get<0>(recv);
      auto &fieldNames = std::get<1>(recv);
      auto &typeSignatures = std::get<2>(recv);
      for (int32_t idx = 0; idx < entries.size(); ++idx)
         {
         TR::TypeLayoutEntry entry = entries[idx];
         char *fieldname = new (region) char[fieldNames[idx].length() + 1];
         memcpy(fieldname, fieldNames[idx].data(), fieldNames[idx].length() + 1);
         entry._fieldname = fieldname;
         char *typeSignature = new (region) char[typeSignatures[idx].length() + 1];
         memcpy(typeSignature, typeSignatures[idx].data(), typeSignatures[idx].length() + 1);
         entry._typeSignature = typeSignature;
         tlb.add(entry);
         }

      }
   else
#endif /* defined(J9VM_OPT_JITSERVER) */
      {
      TR_VMFieldsInfo fieldsInfo(comp, reinterpret_cast<J9Class *>(opaqueClazz), 1, stackAlloc);
      ListIterator<TR_VMField> iter(fieldsInfo.getFields());
      for (TR_VMField *field = iter.getFirst(); field; field = iter.getNext())
         {
         addEntryForField(field, tlb, region, opaqueClazz, comp);
         }
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
   J9ROMClass *romClass = TR::Compiler->cls.romClassOf(arrayClass);
   int32_t logElementSize = ((J9ROMArrayClass*)romClass)->arrayShape & 0x0000FFFF;
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
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      uintptr_t classFlags = 0;
      JITServerHelpers::getAndCacheRAMClassInfo((J9Class *)clazz, TR::compInfoPT->getClientData(), stream, JITServerHelpers::CLASSINFO_CLASS_FLAGS, (void *)&classFlags);
#ifdef DEBUG
      stream->write(JITServer::MessageType::ClassEnv_classFlagsValue, clazz);
      uintptr_t classFlagsRemote = std::get<0>(stream->read<uintptr_t>());
      // Check that class flags from remote call is equal to the cached ones
      classFlags = classFlags & J9ClassIsValueType;
      classFlagsRemote = classFlagsRemote & J9ClassIsValueType;
      TR_ASSERT(classFlags == classFlagsRemote, "remote call class flags is not equal to cached class flags");
#endif
      return classFlags & J9ClassIsValueType;
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   J9Class *j9class = reinterpret_cast<J9Class*>(clazz);
   return J9_IS_J9CLASS_VALUETYPE(j9class);
   }

bool
J9::ClassEnv::isPrimitiveValueTypeClass(TR_OpaqueClassBlock *clazz)
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      uintptr_t classFlags = 0;
      JITServerHelpers::getAndCacheRAMClassInfo((J9Class *)clazz, TR::compInfoPT->getClientData(), stream, JITServerHelpers::CLASSINFO_CLASS_FLAGS, (void *)&classFlags);
#ifdef DEBUG
      stream->write(JITServer::MessageType::ClassEnv_classFlagsValue, clazz);
      uintptr_t classFlagsRemote = std::get<0>(stream->read<uintptr_t>());
      // Check that class flags from remote call is equal to the cached ones
      classFlags = classFlags & J9ClassIsPrimitiveValueType;
      classFlagsRemote = classFlagsRemote & J9ClassIsPrimitiveValueType;
      TR_ASSERT(classFlags == classFlagsRemote, "remote call class flags is not equal to cached class flags");
#endif
      return classFlags & J9ClassIsPrimitiveValueType;
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   J9Class *j9class = reinterpret_cast<J9Class*>(clazz);
   return J9_IS_J9CLASS_PRIMITIVE_VALUETYPE(j9class);
   }

bool
J9::ClassEnv::isValueTypeClassFlattened(TR_OpaqueClassBlock *clazz)
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      uintptr_t classFlags = 0;
      JITServerHelpers::getAndCacheRAMClassInfo((J9Class *)clazz, TR::compInfoPT->getClientData(), stream, JITServerHelpers::CLASSINFO_CLASS_FLAGS, (void *)&classFlags);
#ifdef DEBUG
      stream->write(JITServer::MessageType::ClassEnv_classFlagsValue, clazz);
      uintptr_t classFlagsRemote = std::get<0>(stream->read<uintptr_t>());
      // Check that class flags from remote call is equal to the cached ones
      classFlags = classFlags & J9ClassIsFlattened;
      classFlagsRemote = classFlagsRemote & J9ClassIsFlattened;
      TR_ASSERT(classFlags == classFlagsRemote, "remote call class flags is not equal to cached class flags");
#endif
      return classFlags & J9ClassIsFlattened;
      }
#endif /* defined(J9VM_OPT_JITSERVER) */

   return (clazz && J9_IS_J9CLASS_FLATTENED(reinterpret_cast<J9Class*>(clazz)));
   }

bool
J9::ClassEnv::isValueBasedOrValueTypeClass(TR_OpaqueClassBlock *clazz)
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      uintptr_t classFlags = 0;
      JITServerHelpers::getAndCacheRAMClassInfo((J9Class *)clazz, TR::compInfoPT->getClientData(), stream, JITServerHelpers::CLASSINFO_CLASS_FLAGS, (void *)&classFlags);
#ifdef DEBUG
      stream->write(JITServer::MessageType::ClassEnv_classFlagsValue, clazz);
      uintptr_t classFlagsRemote = std::get<0>(stream->read<uintptr_t>());
      // Check that class flags from remote call is equal to the cached ones
      classFlags = classFlags & J9_CLASS_DISALLOWS_LOCKING_FLAGS;
      classFlagsRemote = classFlagsRemote & J9_CLASS_DISALLOWS_LOCKING_FLAGS;
      TR_ASSERT_FATAL(classFlags == classFlagsRemote, "remote call class flags is not equal to cached class flags");
#endif
      return J9_ARE_ANY_BITS_SET(classFlags, J9_CLASS_DISALLOWS_LOCKING_FLAGS);
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   J9Class *j9class = reinterpret_cast<J9Class*>(clazz);
   return J9_ARE_ANY_BITS_SET(j9class->classFlags, J9_CLASS_DISALLOWS_LOCKING_FLAGS);
   }

bool
J9::ClassEnv::classHasIdentity(TR_OpaqueClassBlock *clazz)
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      uintptr_t classFlags = 0;
      JITServerHelpers::getAndCacheRAMClassInfo((J9Class *)clazz, TR::compInfoPT->getClientData(), stream, JITServerHelpers::CLASSINFO_CLASS_FLAGS, (void *)&classFlags);
#ifdef DEBUG
      stream->write(JITServer::MessageType::ClassEnv_classFlagsValue, clazz);
      uintptr_t classFlagsRemote = std::get<0>(stream->read<uintptr_t>());
      // Check that class flags from remote call is equal to the cached ones
      classFlags = classFlags & J9ClassHasIdentity;
      classFlagsRemote = classFlagsRemote & J9ClassHasIdentity;
      TR_ASSERT_FATAL(classFlags == classFlagsRemote, "remote call class flags is not equal to cached class flags");
#endif
      return J9_ARE_ANY_BITS_SET(classFlags, J9ClassHasIdentity);
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   J9Class *j9class = reinterpret_cast<J9Class*>(clazz);
   return J9_ARE_ANY_BITS_SET(j9class->classFlags, J9ClassHasIdentity);
   }

bool
J9::ClassEnv::classSupportsDirectMemoryComparison(TR_OpaqueClassBlock *clazz)
   {
   uintptr_t classFlags = 0;
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      JITServerHelpers::getAndCacheRAMClassInfo((J9Class *)clazz, TR::compInfoPT->getClientData(), stream, JITServerHelpers::CLASSINFO_CLASS_FLAGS, (void *)&classFlags);
#ifdef DEBUG
      stream->write(JITServer::MessageType::ClassEnv_classFlagsValue, clazz);
      uintptr_t classFlagsRemote = std::get<0>(stream->read<uintptr_t>());
      // Check that class flags from remote call is equal to the cached ones
      classFlags = classFlags & J9ClassCanSupportFastSubstitutability;
      classFlagsRemote = classFlagsRemote & J9ClassCanSupportFastSubstitutability;
      TR_ASSERT_FATAL(classFlags == classFlagsRemote, "remote call class flags is not equal to cached class flags");
#endif
      }
   else
#endif /* defined(J9VM_OPT_JITSERVER) */
      {
      J9Class *j9class = reinterpret_cast<J9Class*>(clazz);
      classFlags = j9class->classFlags;
      }

   // If the value type has reference fields and the GC policy is concurrent scavenge, direct
   // memory comparison cannot be supported.
   return J9_ARE_ANY_BITS_SET(classFlags, J9ClassCanSupportFastSubstitutability) &&
            (J9_ARE_NO_BITS_SET(classFlags, J9ClassHasReferences) ||
             (TR::Compiler->om.readBarrierType() != gc_modron_readbar_none));
   }

bool
J9::ClassEnv::isZeroInitializable(TR_OpaqueClassBlock *clazz)
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      uintptr_t classFlags = 0;
      JITServerHelpers::getAndCacheRAMClassInfo((J9Class *)clazz, TR::compInfoPT->getClientData(), stream, JITServerHelpers::CLASSINFO_CLASS_FLAGS, (void *)&classFlags);
#ifdef DEBUG
      stream->write(JITServer::MessageType::ClassEnv_classFlagsValue, clazz);
      uintptr_t classFlagsRemote = std::get<0>(stream->read<uintptr_t>());
      // Check that class flags from remote call is equal to the cached ones
      classFlags = classFlags & J9ClassContainsUnflattenedFlattenables;
      classFlagsRemote = classFlagsRemote & J9ClassContainsUnflattenedFlattenables;
      TR_ASSERT(classFlags == classFlagsRemote, "remote call class flags is not equal to cached class flags");
#endif
      return classFlags & J9ClassContainsUnflattenedFlattenables;
      }
#endif
   return (self()->classFlagsValue(clazz) & J9ClassContainsUnflattenedFlattenables) == 0;
   }

bool
J9::ClassEnv::containsZeroOrOneConcreteClass(TR::Compilation *comp, List<TR_PersistentClassInfo>* subClasses)
   {
   int count = 0;
#if defined(J9VM_OPT_JITSERVER)
   if (comp->isOutOfProcessCompilation())
      {
      ListIterator<TR_PersistentClassInfo> j(subClasses);
      TR_ScratchList<TR_PersistentClassInfo> subClassesNotCached(comp->trMemory());

      // Process classes cached at the server first
      ClientSessionData * clientData = TR::compInfoPT->getClientData();
      for (TR_PersistentClassInfo *ptClassInfo = j.getFirst(); ptClassInfo; ptClassInfo = j.getNext())
         {
         TR_OpaqueClassBlock *clazz = ptClassInfo->getClassId();
         J9Class *j9clazz = TR::Compiler->cls.convertClassOffsetToClassPtr(clazz);
         auto romClass = JITServerHelpers::getRemoteROMClassIfCached(clientData, j9clazz);
         if (romClass == NULL)
            {
            subClassesNotCached.add(ptClassInfo);
            }
         else
            {
            if (TR::Compiler->cls.isConcreteClass(comp, clazz))
               {
               if (++count > 1)
                  return false;
               }
            }
         }
      // Traverse through classes that are not cached on server
      ListIterator<TR_PersistentClassInfo> i(&subClassesNotCached);
      for (TR_PersistentClassInfo *ptClassInfo = i.getFirst(); ptClassInfo; ptClassInfo = i.getNext())
         {
         TR_OpaqueClassBlock *clazz = ptClassInfo->getClassId();
         if (TR::Compiler->cls.isConcreteClass(comp, clazz))
            {
            if (++count > 1)
               return false;
            }
         }
      }
   else // non-jitserver
#endif /* defined(J9VM_OPT_JITSERVER) */
      {
      ListIterator<TR_PersistentClassInfo> i(subClasses);
      for (TR_PersistentClassInfo *ptClassInfo = i.getFirst(); ptClassInfo; ptClassInfo = i.getNext())
         {
         TR_OpaqueClassBlock *clazz = ptClassInfo->getClassId();
         if (TR::Compiler->cls.isConcreteClass(comp, clazz))
            {
            if (++count > 1)
               return false;
            }
         }
      }
   return true;
   }

bool
J9::ClassEnv::isClassRefPrimitiveValueType(TR::Compilation *comp, TR_OpaqueClassBlock *cpContextClass, int32_t cpIndex)
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = comp->getStream())
      {
      stream->write(JITServer::MessageType::ClassEnv_isClassRefPrimitiveValueType, cpContextClass, cpIndex);
      return std::get<0>(stream->read<bool>());
      }
   else // non-jitserver
#endif /* defined(J9VM_OPT_JITSERVER) */
      {
      J9Class * j9class = reinterpret_cast<J9Class *>(cpContextClass);
      J9JavaVM *vm = comp->fej9()->getJ9JITConfig()->javaVM;
      return vm->internalVMFunctions->isClassRefQtype(j9class, cpIndex);
      }
   }

char *
J9::ClassEnv::classNameToSignature(const char *name, int32_t &len, TR::Compilation *comp, TR_AllocationKind allocKind, TR_OpaqueClassBlock *clazz)
   {
   char *sig;

   if (name[0] == '[')
      {
      sig = (char *)comp->trMemory()->allocateMemory(len+1, allocKind);
      memcpy(sig,name,len);
      }
   else
      {
      len += 2;
      sig = (char *)comp->trMemory()->allocateMemory(len+1, allocKind);
      if (clazz &&
         TR::Compiler->om.areFlattenableValueTypesEnabled() &&
         TR::Compiler->om.isQDescriptorForValueTypesSupported() &&
         self()->isPrimitiveValueTypeClass(clazz)
         )
         sig[0] = 'Q';
      else
         sig[0] = 'L';
      memcpy(sig+1,name,len-2);
      sig[len-1]=';';
      }

   sig[len] = '\0';
   return sig;
   }

int32_t
J9::ClassEnv::flattenedArrayElementSize(TR::Compilation *comp, TR_OpaqueClassBlock *arrayClass)
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = comp->getStream())
      {
      int32_t arrayElementSize = 0;
      JITServerHelpers::getAndCacheRAMClassInfo((J9Class *)arrayClass, TR::compInfoPT->getClientData(), stream, JITServerHelpers::CLASSINFO_ARRAY_ELEMENT_SIZE, (void *)&arrayElementSize);
      return arrayElementSize;
      }
   else
#endif /* defined(J9VM_OPT_JITSERVER) */
      {
      J9JavaVM *vm = comp->fej9()->getJ9JITConfig()->javaVM;
      return vm->internalVMFunctions->arrayElementSize((J9ArrayClass*)self()->convertClassOffsetToClassPtr(arrayClass));
      }
   }

j9object_t*
J9::ClassEnv::getDefaultValueSlotAddress(TR::Compilation *comp, TR_OpaqueClassBlock *clazz)
   {
   TR_ASSERT_FATAL(self()->isClassInitialized(comp, clazz), "clazz %p must be initialized when getDefaultValueSlotAddress is called", clazz);

#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = comp->getStream())
      {
      j9object_t* defaultValueSlotAddress = NULL;
      ClientSessionData *clientSessionData = TR::compInfoPT->getClientData();

      JITServerHelpers::getAndCacheRAMClassInfo((J9Class *)clazz, clientSessionData, stream, JITServerHelpers::CLASSINFO_DEFAULT_VALUE_SLOT_ADDRESS, (void *)&defaultValueSlotAddress);

      if (!defaultValueSlotAddress)
         {
         stream->write(JITServer::MessageType::ClassEnv_getDefaultValueSlotAddress, clazz);
         defaultValueSlotAddress = std::get<0>(stream->read<j9object_t*>());

         if (defaultValueSlotAddress)
            {
            OMR::CriticalSection getRemoteROMClass(clientSessionData->getROMMapMonitor());
            auto it = clientSessionData->getROMClassMap().find((J9Class*) clazz);
            if (it != clientSessionData->getROMClassMap().end())
               {
               it->second._defaultValueSlotAddress = defaultValueSlotAddress;
               }
            }
         }

      return defaultValueSlotAddress;
      }
   else // non-jitserver
#endif /* defined(J9VM_OPT_JITSERVER) */
      {
      J9Class *j9class = reinterpret_cast<J9Class *>(clazz);
      J9JavaVM *vm = comp->fej9()->getJ9JITConfig()->javaVM;

      return vm->internalVMFunctions->getDefaultValueSlotAddress(j9class);
      }
   }
