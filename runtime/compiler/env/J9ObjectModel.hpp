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

#ifndef J9_OBJECTMODEL_INCL
#define J9_OBJECTMODEL_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_OBJECTMODEL_CONNECTOR
#define J9_OBJECTMODEL_CONNECTOR
namespace J9 { class ObjectModel; }
namespace J9 { typedef J9::ObjectModel ObjectModelConnector; }
#endif

#include "env/OMRObjectModel.hpp"

#include <stdint.h>
#include "env/jittypes.h"

namespace TR { class Node; }
namespace TR { class Compilation; }

namespace J9
{

class ObjectModel : public OMR::ObjectModelConnector
   {
public:

   ObjectModel() :
      OMR::ObjectModelConnector(),
      _compressObjectReferences(false),
      _usesDiscontiguousArraylets(false),
      _arrayLetLeafSize(0),
      _arrayLetLeafLogSize(0),
      _readBarrierType(gc_modron_readbar_none),
      _writeBarrierType(gc_modron_wrtbar_none),
      _objectAlignmentInBytes(0)
   {}

   void initialize();

   bool mayRequireSpineChecks();

   /**
   * @brief Whether or not value object is enabled
   */
   bool areValueTypesEnabled();
   /**
   * @brief Whether or not flattenable value object (aka null restricted) type is enabled
   */
   bool areFlattenableValueTypesEnabled();
   /**
   * @brief Whether or not `Q` signature is supported
   */
   bool isQDescriptorForValueTypesSupported();

   /**
   * @brief Whether the check is enabled on monitor object being value based class type
   */
   bool areValueBasedMonitorChecksEnabled();
   /**
   * @brief Whether the array flattening is enabled for value types
   */
   bool isValueTypeArrayFlatteningEnabled();

   int32_t sizeofReferenceField();
   bool isHotReferenceFieldRequired();
   bool isOffHeapAllocationEnabled();
   uintptr_t elementSizeOfBooleanArray();
   uint32_t getSizeOfArrayElement(TR::Node *node);
   int64_t maxArraySizeInElementsForAllocation(TR::Node *newArray, TR::Compilation *comp);
   int64_t maxArraySizeInElements(int32_t knownMinElementSize, TR::Compilation *comp);

   int32_t maxContiguousArraySizeInBytes();

   uintptr_t contiguousArrayHeaderSizeInBytes();

   uintptr_t discontiguousArrayHeaderSizeInBytes();

   // For array access
   bool isDiscontiguousArray(int32_t sizeInBytes);
   bool isDiscontiguousArray(int32_t sizeInElements, int32_t elementSize);
   bool isDiscontiguousArray(TR::Compilation* comp, uintptr_t objectPointer);
   intptr_t getArrayLengthInElements(TR::Compilation* comp, uintptr_t objectPointer);
   uintptr_t getArrayLengthInBytes(TR::Compilation* comp, uintptr_t objectPointer);
   uintptr_t getArrayElementWidthInBytes(TR::DataType type);
   uintptr_t getArrayElementWidthInBytes(TR::Compilation* comp, uintptr_t objectPointer);
   uintptr_t getAddressOfElement(TR::Compilation* comp, uintptr_t objectPointer, int64_t offset);
   uintptr_t decompressReference(TR::Compilation* comp, uintptr_t compressedReference);

   bool generateCompressedObjectHeaders();

   bool usesDiscontiguousArraylets();
   bool canGenerateArraylets() { return usesDiscontiguousArraylets(); }
   bool useHybridArraylets() { return usesDiscontiguousArraylets(); }
   int32_t arrayletLeafSize();
   int32_t arrayletLeafLogSize();

   bool isIndexableDataAddrPresent();

   int32_t compressedReferenceShiftOffset();
   int32_t compressedReferenceShift();

   bool nativeAddressesCanChangeSize();

   uintptr_t offsetOfObjectVftField();

   uintptr_t offsetOfHeaderFlags();

   uintptr_t maskOfObjectVftField();

   int32_t arraySpineShift(int32_t width);
   int32_t arrayletMask(int32_t width);
   int32_t arrayletLeafIndex(int32_t index, int32_t elementSize);
   int32_t objectAlignmentInBytes();
   uintptr_t offsetOfContiguousArraySizeField();
   uintptr_t offsetOfDiscontiguousArraySizeField();
   uintptr_t objectHeaderSizeInBytes();
   uintptr_t offsetOfIndexableSizeField();
#if defined(TR_TARGET_64BIT)
   uintptr_t offsetOfContiguousDataAddrField();
   uintptr_t offsetOfDiscontiguousDataAddrField();
#endif /* TR_TARGET_64BIT */

   /**
   * @brief Returns the read barrier type of VM's GC
   */
   MM_GCReadBarrierType  readBarrierType();

   /**
   * @brief Returns the write barrier type of VM's GC
   */
   MM_GCWriteBarrierType writeBarrierType();

   /**
   * @brief Returns whether or not object references are compressed
   */
   bool compressObjectReferences();

   int32_t getObjectAlignmentInBytes();

private:

   bool                  _compressObjectReferences;
   bool                  _usesDiscontiguousArraylets;
   int32_t               _arrayLetLeafSize;
   int32_t               _arrayLetLeafLogSize;
   MM_GCReadBarrierType  _readBarrierType;
   MM_GCWriteBarrierType _writeBarrierType;
   int32_t               _objectAlignmentInBytes;
   };

}

#endif
