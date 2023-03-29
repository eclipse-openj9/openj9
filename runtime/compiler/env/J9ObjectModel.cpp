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

#if defined(J9ZOS390)
//On zOS XLC linker can't handle files with same name at link time
//This workaround with pragma is needed. What this does is essentially
//give a different name to the codesection (csect) for this file. So it
//doesn't conflict with another file with same name.
#pragma csect(CODE,"J9ObjectModel#C")
#pragma csect(STATIC,"J9ObjectModel#S")
#pragma csect(TEST,"J9ObjectModel#T")
#endif

#include <algorithm>
#include <limits.h>
#include <stdint.h>
#include "j9.h"
#include "j9cfg.h"
#include "j9modron.h"
#include "codegen/CodeGenerator.hpp"
#include "env/CompilerEnv.hpp"
#include "env/ObjectModel.hpp"
#include "env/jittypes.h"
#include "env/VMAccessCriticalSection.hpp"
#include "il/DataTypes.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "env/VMJ9.h"
#if defined(J9VM_OPT_JITSERVER)
#include "control/CompilationThread.hpp"
#include "runtime/JITClientSession.hpp"
#endif /* defined(J9VM_OPT_JITSERVER) */

#define DEFAULT_OBJECT_ALIGNMENT (8)


void
J9::ObjectModel::initialize()
   {
   OMR::ObjectModelConnector::initialize();

   J9JavaVM *vm = TR::Compiler->javaVM;

   PORT_ACCESS_FROM_JAVAVM(vm);
   J9MemoryManagerFunctions * mmf = vm->memoryManagerFunctions;

   uintptr_t value;

   // Compressed refs
   uintptr_t result = mmf->j9gc_modron_getConfigurationValueForKey(vm,
                                                                   j9gc_modron_configuration_compressObjectReferences,
                                                                   &value);
   if (result == 1 && value == 1)
      _compressObjectReferences = true;
   else
      _compressObjectReferences = false;

   // Discontiguous arraylets
   //
   result = mmf->j9gc_modron_getConfigurationValueForKey(vm,
                                                         j9gc_modron_configuration_discontiguousArraylets,
                                                         &value);
   if (result == 1 && value == 1)
      {
      _usesDiscontiguousArraylets = true;
      _arrayLetLeafSize = (int32_t)(vm->memoryManagerFunctions->j9gc_arraylet_getLeafSize(vm));
      _arrayLetLeafLogSize = (int32_t)(vm->memoryManagerFunctions->j9gc_arraylet_getLeafLogSize(vm));
      }
   else
      {
      _usesDiscontiguousArraylets = false;
      _arrayLetLeafSize = 0;
      _arrayLetLeafLogSize = 0;
      }

   _readBarrierType  = (MM_GCReadBarrierType) mmf->j9gc_modron_getReadBarrierType(vm);
   _writeBarrierType = (MM_GCWriteBarrierType)mmf->j9gc_modron_getWriteBarrierType(vm);
   if (_writeBarrierType == gc_modron_wrtbar_satb_and_oldcheck)
      {
      // JIT treats satb_and_oldcheck same as satb
      _writeBarrierType = gc_modron_wrtbar_satb;
      }

   _objectAlignmentInBytes = objectAlignmentInBytes();
   }


bool
J9::ObjectModel::areValueTypesEnabled()
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
      return true;
#else /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
      return false;
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
      }
#endif /* defined(J9VM_OPT_JITSERVER) */

   J9JavaVM * javaVM = TR::Compiler->javaVM;
   return javaVM->internalVMFunctions->areValueTypesEnabled(javaVM);
   }

bool
J9::ObjectModel::areFlattenableValueTypesEnabled()
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
      return true;
#else /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
      return false;
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
      }
#endif /* defined(J9VM_OPT_JITSERVER) */

   J9JavaVM * javaVM = TR::Compiler->javaVM;
   return javaVM->internalVMFunctions->areFlattenableValueTypesEnabled(javaVM);
   }

bool
J9::ObjectModel::isQDescriptorForValueTypesSupported()
   {
   // TODO: Implementation is required to determine under which case 'Q' descriptor is removed
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
      return true;
#else /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
      return false;
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
      }
#endif /* defined(J9VM_OPT_JITSERVER) */

   J9JavaVM * javaVM = TR::Compiler->javaVM;
   if (javaVM->internalVMFunctions->areFlattenableValueTypesEnabled(javaVM))
     return true;

   return false;
   }

bool
J9::ObjectModel::areValueBasedMonitorChecksEnabled()
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      auto *vmInfo = TR::compInfoPT->getClientData()->getOrCacheVMInfo(stream);
	   return J9_ARE_ANY_BITS_SET(vmInfo->_extendedRuntimeFlags2, J9_EXTENDED_RUNTIME2_VALUE_BASED_EXCEPTION | J9_EXTENDED_RUNTIME2_VALUE_BASED_WARNING);
      }
#endif /* defined(J9VM_OPT_JITSERVER) */

   J9JavaVM * javaVM = TR::Compiler->javaVM;
   return javaVM->internalVMFunctions->areValueBasedMonitorChecksEnabled(javaVM);
   }

bool
J9::ObjectModel::isValueTypeArrayFlatteningEnabled()
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
      auto *vmInfo = TR::compInfoPT->getClientData()->getOrCacheVMInfo(stream);
      return J9_ARE_ANY_BITS_SET(vmInfo->_extendedRuntimeFlags2, J9_EXTENDED_RUNTIME2_ENABLE_VT_ARRAY_FLATTENING);
#else /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
      return false;
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
      }
#endif /* defined(J9VM_OPT_JITSERVER) */

   J9JavaVM *javaVM = TR::Compiler->javaVM;

   if (javaVM->internalVMFunctions->areFlattenableValueTypesEnabled(javaVM))
      return J9_ARE_ALL_BITS_SET(javaVM->extendedRuntimeFlags2, J9_EXTENDED_RUNTIME2_ENABLE_VT_ARRAY_FLATTENING);
   else
      return false;
   }

int32_t
J9::ObjectModel::sizeofReferenceField()
   {
   if (compressObjectReferences())
      return sizeof(uint32_t);
   return sizeof(uintptr_t);
   }


bool
J9::ObjectModel::isHotReferenceFieldRequired()
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      auto *vmInfo = TR::compInfoPT->getClientData()->getOrCacheVMInfo(stream);
      return vmInfo->_isHotReferenceFieldRequired;
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   return TR::Compiler->javaVM->memoryManagerFunctions->j9gc_hot_reference_field_required(TR::Compiler->javaVM);
   }

bool
J9::ObjectModel::isOffHeapAllocationEnabled()
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      auto *vmInfo = TR::compInfoPT->getClientData()->getOrCacheVMInfo(stream);
      return vmInfo->_isOffHeapAllocationEnabled;
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   return TR::Compiler->javaVM->memoryManagerFunctions->j9gc_off_heap_allocation_enabled(TR::Compiler->javaVM);
   }


UDATA
J9::ObjectModel::elementSizeOfBooleanArray()
   {
   return 1;
   }


uint32_t
J9::ObjectModel::getSizeOfArrayElement(TR::Node * node)
   {
   TR_ASSERT(node->getOpCodeValue() == TR::newarray || node->getOpCodeValue() == TR::anewarray, "getSizeOfArrayElement expects either newarray or anewarray at [%p]", node);

   if (node->getOpCodeValue() == TR::anewarray)
      {
      if (compressObjectReferences())
         return sizeofReferenceField();
      return TR::Symbol::convertTypeToSize(TR::Address);
      }

   TR_ASSERT(node->getSecondChild()->getOpCode().isLoadConst(), "Expecting const child \n");
   switch (node->getSecondChild()->getInt())
      {
      case 4:
         return (uint32_t) TR::Compiler->om.elementSizeOfBooleanArray();
      case 8:
         return 1;
      case 5:
      case 9:
         return 2;
      case 7:
      case 11:
         return 8;
      }
   return 4;
   }

int64_t
J9::ObjectModel::maxArraySizeInElementsForAllocation(
      TR::Node *newArray,
      TR::Compilation *comp)
   {
   int64_t result = TR::getMaxSigned<TR::Int64>();

   switch (newArray->getOpCodeValue())
      {
      case TR::newarray:
      case TR::anewarray:
         result = TR::Compiler->om.maxArraySizeInElements(TR::Compiler->om.getSizeOfArrayElement(newArray), comp);
         break;
      case TR::multianewarray:
         result = TR::Compiler->om.maxArraySizeInElements(TR::Compiler->om.sizeofReferenceField(), comp);
         break;
      default:
         TR_ASSERT(0, "Unexpected node %p in maxArraySizeInElementsForAllocation", newArray);
         break;
      }

   return result;
   }


int64_t
J9::ObjectModel::maxArraySizeInElements(
      int32_t knownMinElementSize,
      TR::Compilation *comp)
   {
   int64_t result = INT_MAX; // On Java, indexes are signed 32-bit ints

   // An array can't be larger than the heap.  Limit the index based on that.
   //
   int32_t minElementSize = std::max(1, knownMinElementSize);
   int64_t maxHeapSizeInBytes;

   if (comp->compileRelocatableCode())
      {
      maxHeapSizeInBytes = -1;
      }
   else
      {
      maxHeapSizeInBytes = TR::Compiler->vm.maxHeapSizeInBytes();
      }

   if (maxHeapSizeInBytes == -1)
      {
      // getMaximumHeapSize has an irritating habit of returning -1 sometimes,
      // for some reason.  Must compensate for this corner case here.
      //
      if (comp->target().is64Bit())
         {
         // Ok, in theory it could be TR::getMaxUnsigned<TR::Int64>().  This isn't worth the
         // hassle of using uint64_t and worrying about signedness.  When the
         // day comes that a Java program needs an array larger than 8 billion
         // gigabytes, our great-grandchildren can switch to int128_t, assuming
         // Testarossa has not yet become self-aware and fixed this limitation
         // by itself.
         //
         maxHeapSizeInBytes = TR::getMaxSigned<TR::Int64>();
         }
      else
         {
         maxHeapSizeInBytes = TR::getMaxUnsigned<TR::Int32>(); // Heap can't be larger than the address space
         }
      }

   result = std::min(result, maxHeapSizeInBytes / minElementSize);

   return result;
   }


bool
J9::ObjectModel::nativeAddressesCanChangeSize()
   {
#if defined(J9VM_OPT_SHARED_CLASSES) && defined(J9VM_INTERP_AOT_COMPILE_SUPPORT)
   return true;
#else
   return false;
#endif
   }


bool
J9::ObjectModel::generateCompressedObjectHeaders()
   {
   return compressObjectReferences();
   }


uintptr_t
J9::ObjectModel::contiguousArrayHeaderSizeInBytes()
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      auto *vmInfo = TR::compInfoPT->getClientData()->getOrCacheVMInfo(stream);
      return vmInfo->_contiguousIndexableHeaderSize;
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   return TR::Compiler->javaVM->contiguousIndexableHeaderSize;
   }


uintptr_t
J9::ObjectModel::discontiguousArrayHeaderSizeInBytes()
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      auto *vmInfo = TR::compInfoPT->getClientData()->getOrCacheVMInfo(stream);
      return vmInfo->_discontiguousIndexableHeaderSize;
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   return TR::Compiler->javaVM->discontiguousIndexableHeaderSize;
   }


// Returns the maximum contiguous arraylet size in bytes NOT including the header.
//
int32_t
J9::ObjectModel::maxContiguousArraySizeInBytes()
   {
   return TR::Compiler->om.arrayletLeafSize() - TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
   }


// 'sizeInBytes' should NOT include the header
//
bool
J9::ObjectModel::isDiscontiguousArray(int32_t sizeInBytes)
   {
   if (sizeInBytes > TR::Compiler->om.maxContiguousArraySizeInBytes())
      return true;
   else
      {
      if (TR::Compiler->om.useHybridArraylets() && sizeInBytes == 0)
         return true;
      else
         return false;
      }
   }


// 'sizeInElements' should NOT include the header
//
bool
J9::ObjectModel::isDiscontiguousArray(int32_t sizeInElements, int32_t elementSize)
   {
   int32_t shift = trailingZeroes(elementSize);
   int32_t maxContiguousArraySizeInElements = TR::Compiler->om.maxContiguousArraySizeInBytes() >> shift;

   if (sizeInElements > maxContiguousArraySizeInElements)
      return true;
   else
      {
      if (TR::Compiler->om.useHybridArraylets() && sizeInElements == 0)
         return true;
      else
         return false;
      }
   }


int32_t
J9::ObjectModel::compressedReferenceShiftOffset()
   {
   // FIXME: currently returns 0, has to be modified to
   // return the shift offset
   //
   return TR::Compiler->om.compressedReferenceShift();
   }


int32_t
J9::ObjectModel::compressedReferenceShift()
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      auto *vmInfo = TR::compInfoPT->getClientData()->getOrCacheVMInfo(stream);
      return vmInfo->_compressedReferenceShift;
      }
#endif /* defined(J9VM_OPT_JITSERVER) */

   if (compressObjectReferences())
      {
      J9JavaVM *javaVM = TR::Compiler->javaVM;
      if (!javaVM)
         return 0;

      J9VMThread *vmThread = javaVM->internalVMFunctions->currentVMThread(javaVM);
      J9MemoryManagerFunctions * mmf = javaVM->memoryManagerFunctions;
      int32_t result = mmf->j9gc_objaccess_compressedPointersShift(vmThread);
      return result;
      }
   return 0;
   }


uintptr_t
J9::ObjectModel::offsetOfObjectVftField()
   {
   return TMP_OFFSETOF_J9OBJECT_CLAZZ;
   }


uintptr_t
J9::ObjectModel::offsetOfHeaderFlags()
   {
#if defined(J9VM_INTERP_FLAGS_IN_CLASS_SLOT)
   return TR::Compiler->om.offsetOfObjectVftField();
#else
   #if defined(TMP_OFFSETOF_J9OBJECT_FLAGS)
      return TMP_OFFSETOF_J9OBJECT_FLAGS;
   #else
      return 0;
   #endif
#endif
   }


uintptr_t
J9::ObjectModel::maskOfObjectVftField()
   {
   if (TR::Compiler->om.offsetOfHeaderFlags() != TR::Compiler->om.offsetOfObjectVftField())
      {
      // Flags are not in the VFT field, so no need for a mask
      //
      if (TR::Options::getCmdLineOptions()->getOption(TR_DisableMaskVFTPointers))
         return ~(uintptr_t)0;
      }

   return (uintptr_t)(-J9_REQUIRED_CLASS_ALIGNMENT);
   }


// Answers whether this compilation may need spine checks.  The FE will answer yes
// if true discontiguous arrays could appear at all with this GC policy, but its a
// conservative answer.  The corresponding compilation query may know better for
// this compilation unit.
//
bool
J9::ObjectModel::mayRequireSpineChecks()
   {
   return TR::Compiler->om.useHybridArraylets();
   }


int32_t
J9::ObjectModel::arraySpineShift(int32_t width)
   {
   TR_ASSERT(TR::Compiler->om.canGenerateArraylets(), "not supposed to be generating arraylets!");
   TR_ASSERT(width >= 0, "unexpected arraylet datatype width");

   // for elements larger than bytes, need to reduce the shift because fewer elements
   // fit into each arraylet

   int32_t shift=-1;
   int32_t maxShift = TR::Compiler->om.arrayletLeafLogSize();

   switch(width)
      {
      case 1 : shift = maxShift-0; break;
      case 2 : shift = maxShift-1; break;
      case 4 : shift = maxShift-2; break;
      case 8 : shift = maxShift-3; break;
      default: TR_ASSERT(0,"unexpected element width");
      }
   return shift;
   }


int32_t
J9::ObjectModel::arrayletMask(int32_t width)
   {
   TR_ASSERT(TR::Compiler->om.canGenerateArraylets(), "not supposed to be generating arraylets!");
   TR_ASSERT(width >= 0, "unexpected arraylet datatype width");
   int32_t mask=(1 << TR::Compiler->om.arraySpineShift(width))-1;
   return mask;
   }


int32_t
J9::ObjectModel::arrayletLeafIndex(int32_t index, int32_t elementSize)
   {
   TR_ASSERT(TR::Compiler->om.canGenerateArraylets(), "not supposed to be generating arraylets!");
   TR_ASSERT(elementSize >= 0, "unexpected arraylet datatype width");

   if (index<0)
      return -1;

   int32_t arrayletIndex = (index >> TR::Compiler->om.arraySpineShift(elementSize));
   return arrayletIndex;
   }


int32_t
J9::ObjectModel::objectAlignmentInBytes()
   {
   J9JavaVM *javaVM = TR::Compiler->javaVM;
   if (!javaVM)
      return 0;

#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      auto *vmInfo = TR::compInfoPT->getClientData()->getOrCacheVMInfo(stream);
      return vmInfo->_objectAlignmentInBytes;
      }
#endif /* defined(J9VM_OPT_JITSERVER) */

   J9MemoryManagerFunctions * mmf = javaVM->memoryManagerFunctions;
   uintptr_t result = 0;
   result = mmf->j9gc_modron_getConfigurationValueForKey(javaVM, j9gc_modron_configuration_objectAlignment, &result) ? result : 0;
   return (int32_t)result;
   }

uintptr_t
J9::ObjectModel::offsetOfContiguousArraySizeField()
   {
   return compressObjectReferences() ? offsetof(J9IndexableObjectContiguousCompressed, size) : offsetof(J9IndexableObjectContiguousFull, size);
   }


uintptr_t
J9::ObjectModel::offsetOfDiscontiguousArraySizeField()
   {
   return compressObjectReferences() ? offsetof(J9IndexableObjectDiscontiguousCompressed, size) : offsetof(J9IndexableObjectDiscontiguousFull, size);
   }

#if defined(J9VM_ENV_DATA64)
uintptr_t
J9::ObjectModel::offsetOfContiguousDataAddrField()
   {
   return compressObjectReferences()
		? offsetof(J9IndexableObjectWithDataAddressContiguousCompressed, dataAddr)
		: offsetof(J9IndexableObjectWithDataAddressContiguousFull, dataAddr);
   }

uintptr_t
J9::ObjectModel::offsetOfDiscontiguousDataAddrField()
   {
   return compressObjectReferences()
		? offsetof(J9IndexableObjectWithDataAddressDiscontiguousCompressed, dataAddr)
		: offsetof(J9IndexableObjectWithDataAddressDiscontiguousFull, dataAddr);
   }
#endif /* defined(J9VM_ENV_DATA64) */


uintptr_t
J9::ObjectModel::objectHeaderSizeInBytes()
   {
   return compressObjectReferences() ? sizeof(J9ObjectCompressed) : sizeof(J9ObjectFull);
   }


uintptr_t
J9::ObjectModel::offsetOfIndexableSizeField()
   {
   return offsetof(J9ROMArrayClass, arrayShape);
   }


bool
J9::ObjectModel::isDiscontiguousArray(TR::Compilation* comp, uintptr_t objectPointer)
   {
   TR_ASSERT(TR::Compiler->vm.hasAccess(comp), "isDicontiguousArray requires VM access");
   TR_ASSERT(TR::Compiler->cls.isClassArray(comp, TR::Compiler->cls.objectClass(comp, (objectPointer))), "Object is not an array");

   int32_t length = *(int32_t*)(objectPointer + TR::Compiler->om.offsetOfContiguousArraySizeField());

   if (TR::Compiler->om.canGenerateArraylets()
       && length == 0)
      return true;

   return false;
   }

/**
 * \brief
 *    Get the address of an element given its offset into the array.
 * \parm comp
 *    Current compilation.
 *
 * \parm objectPointer.
 *    Pointer to the array.
 *
 * \parm offset
 *    The offset of the element in bytes. It should contain the array header. If
 *    objectPointer is a discontiguous array, offset should be an integer that's
 *    calculated as if the array was a contiguous array.
 *
 * \return
 *    The address of the element.
 */
uintptr_t
J9::ObjectModel::getAddressOfElement(TR::Compilation* comp, uintptr_t objectPointer, int64_t offset)
   {
   TR_ASSERT(TR::Compiler->vm.hasAccess(comp), "getAddressOfElement requires VM access");
   TR_ASSERT(TR::Compiler->cls.isClassArray(comp, TR::Compiler->cls.objectClass(comp, (objectPointer))), "Object is not an array");
   TR_ASSERT(offset >= TR::Compiler->om.contiguousArrayHeaderSizeInBytes() &&
             offset < TR::Compiler->om.getArrayLengthInBytes(comp, objectPointer) + TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), "Array is out of bound");

   // If the array is contiguous, return the addition of objectPointer and offset
   if (!TR::Compiler->om.isDiscontiguousArray(comp, objectPointer))
      return objectPointer + offset;

   // The following code handles discontiguous array
   //
   // Treat the array as a byte array, so the element size is 1
   uintptr_t elementSize = 1;
   int64_t elementIndex = offset - TR::Compiler->om.contiguousArrayHeaderSizeInBytes();

   uintptr_t leafIndex = comp->fej9()->getArrayletLeafIndex(elementIndex, elementSize);
   uintptr_t elementIndexInLeaf = comp->fej9()->getLeafElementIndex(elementIndex, elementSize);
   uintptr_t dataStart = objectPointer + TR::Compiler->om.discontiguousArrayHeaderSizeInBytes();

   if (comp->useCompressedPointers())
      {
      uint32_t *spine = (uint32_t*)dataStart;
      dataStart = spine[leafIndex];
      dataStart = TR::Compiler->om.decompressReference(comp, dataStart);
      }
   else
      {
      uintptr_t *spine = (uintptr_t*)dataStart;
      dataStart = spine[leafIndex];
      }

   return dataStart + elementIndexInLeaf * elementSize;
   }

uintptr_t
J9::ObjectModel::getArrayElementWidthInBytes(TR::DataType type)
   {
   if (type == TR::Address)
      return TR::Compiler->om.sizeofReferenceField();
   else
      return TR::Symbol::convertTypeToSize(type);
   }

uintptr_t
J9::ObjectModel::getArrayElementWidthInBytes(TR::Compilation* comp, uintptr_t objectPointer)
   {
   TR_ASSERT(TR::Compiler->vm.hasAccess(comp), "Must haveAccess in getArrayElementWidthInBytes");
   return TR::Compiler->cls.getArrayElementWidthInBytes(comp, TR::Compiler->cls.objectClass(comp, (objectPointer)));
   }

uintptr_t
J9::ObjectModel::getArrayLengthInBytes(TR::Compilation* comp, uintptr_t objectPointer)
   {
   TR_ASSERT(TR::Compiler->vm.hasAccess(comp), "Must haveAccess in getArrayLengthInBytes");
   return (uintptr_t)TR::Compiler->om.getArrayLengthInElements(comp, objectPointer) * TR::Compiler->om.getArrayElementWidthInBytes(comp, objectPointer);
   }

intptr_t
J9::ObjectModel::getArrayLengthInElements(TR::Compilation* comp, uintptr_t objectPointer)
   {
   return comp->fej9()->getArrayLengthInElements(objectPointer);
   }

uintptr_t
J9::ObjectModel::decompressReference(TR::Compilation* comp, uintptr_t compressedReference)
   {
   return (compressedReference << TR::Compiler->om.compressedReferenceShift());
   }

bool
J9::ObjectModel::usesDiscontiguousArraylets()
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      auto *vmInfo = TR::compInfoPT->getClientData()->getOrCacheVMInfo(stream);
      return vmInfo->_usesDiscontiguousArraylets;
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   return _usesDiscontiguousArraylets;
   }

int32_t
J9::ObjectModel::arrayletLeafSize()
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      auto *vmInfo = TR::compInfoPT->getClientData()->getOrCacheVMInfo(stream);
      return vmInfo->_arrayletLeafSize;
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   return _arrayLetLeafSize;
   }

int32_t
J9::ObjectModel::arrayletLeafLogSize()
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      auto *vmInfo = TR::compInfoPT->getClientData()->getOrCacheVMInfo(stream);
      return vmInfo->_arrayletLeafLogSize;
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   return _arrayLetLeafLogSize;
   }

/**
 * Query if the indexable data address field is present within the indexable object header.
 * @return true if isIndexableDualHeaderShapeEnabled is false OR if option -Xgcpolicy:balanced is specified at runtime, false otherwise
 */
bool
J9::ObjectModel::isIndexableDataAddrPresent()
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      auto *vmInfo = TR::compInfoPT->getClientData()->getOrCacheVMInfo(stream);
      return vmInfo->_isIndexableDataAddrPresent;
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
#if defined(J9VM_ENV_DATA64)
   return FALSE != TR::Compiler->javaVM->isIndexableDataAddrPresent;
#else
   return false;
#endif /* defined(J9VM_ENV_DATA64) */
   }

MM_GCReadBarrierType
J9::ObjectModel::readBarrierType()
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      auto *vmInfo = TR::compInfoPT->getClientData()->getOrCacheVMInfo(stream);
      return vmInfo->_readBarrierType;
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   return _readBarrierType;
   }

MM_GCWriteBarrierType
J9::ObjectModel::writeBarrierType()
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      auto *vmInfo = TR::compInfoPT->getClientData()->getOrCacheVMInfo(stream);
      return vmInfo->_writeBarrierType;
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   return _writeBarrierType;
   }

bool
J9::ObjectModel::compressObjectReferences()
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      auto *vmInfo = TR::compInfoPT->getClientData()->getOrCacheVMInfo(stream);
      return vmInfo->_compressObjectReferences;
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   return _compressObjectReferences;
   }

int32_t
J9::ObjectModel::getObjectAlignmentInBytes()
{
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      auto *vmInfo = TR::compInfoPT->getClientData()->getOrCacheVMInfo(stream);
      return vmInfo->_objectAlignmentInBytes;
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   return _objectAlignmentInBytes;
}
