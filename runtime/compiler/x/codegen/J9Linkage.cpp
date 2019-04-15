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

#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"

/** \brief
 *     Align stackIndex so that rsp+stackIndex is a multiple of localObjectAlignment
 *
 *  \param stackIndex
 *     Stack offset to be aligned (negative)
 *
 *  \return
 *     stackIndex is modified in place.
 */
void J9::X86::Linkage::alignOffset(uint32_t &stackIndex, int32_t localObjectAlignment)
   {
   /* On the entry of a method
    * RSP = 16*N + sizeOfReturnAddress
    * The address of a local object needs to be aligned to localObjectAlignment
    * Thus its offset needs to be aligned to localObjectAlignment*N - sizeOfReturnAddress
    * i.e. (sizeOfReturnAddress -stackIndex) % localObjectAlignment = 0
    *
    * Limitation of current implementation:
    *   Only support alignment of 16-byte or less, if larger alignment is required, rsp has to be
    *   localObjectAlignment*N + sizeOfReturnAddress (on the entry of a method)
   */
   uint32_t sizeOfReturnAddress = self()->getProperties().getRetAddressWidth();
   // sizeOfReturnAddress-stackIndex is always positive if stackIndex is read as a signed integer
   uint32_t remainder = (sizeOfReturnAddress - stackIndex) % localObjectAlignment;
   if (remainder)
      {
      // Align stackIndex
      uint32_t adjust = localObjectAlignment - remainder;
      stackIndex -= adjust;
      }
   }

/** \brief
 *     Align stackIndex as part of alignment for local object with collected fields
 *
 *  \param stackIndex
 *     Offset of the first collected references or local objects with collected fields
 *
 *  \return
 *     stackIndex is modified in place
 *
 *  \note
 *     Only to be called after stackIndex is calculated in mapStack/mapCompactedStack,
 *     and before the real stack mapping for collected references and local objects with collected fields
 */
void J9::X86::Linkage::alignLocalObjectWithCollectedFields(uint32_t & stackIndex)
   {
   int32_t localObjectAlignment = self()->cg()->fej9()->getLocalObjectAlignmentInBytes();
   TR::GCStackAtlas *atlas = self()->cg()->getStackAtlas();
   uint8_t pointerSize  = self()->getProperties().getPointerSize();
   // Note that sizeofReferenceAddress is identical to the size of a stack slot
   // Both sizeofReferenceAddress and localObjectAlignment are powers of 2,
   // and it's safe to skip the alignment when sizeofReferenceAddress is a multiple
   // of localObjectAlignment
   if (localObjectAlignment <= TR::Compiler->om.sizeofReferenceAddress())
      return;
   // Collected local objects have gc indice larger than -1
   // Offset of a collected local object determined by (stackIndex + pointerSize*(localCursor->getGCMapIndex()-firstLocalGCIndex))
   // In createStackAtlas, we align pointerSize*(localCursor->getGCMapIndex()-firstLocalGCIndex) by modifying local objects' gc indice
   // Here we align the stackIndex
   //
   traceMsg(self()->comp(),"\nLOCAL OBJECT ALIGNMENT: stack offset before alignment: %d,", stackIndex);
   // When compaction is enabled, stackIndex is calculated using only collected local refs/objects size,
   // it doesn't include the size of padding slots added by aligning collected local objects' gc indice
   // Add them to reflect the correct space needed for collected locals
   if (self()->cg()->getLocalsIG() && self()->cg()->getSupportsCompactedLocals())
      {
      uint8_t pointerSize  = self()->getProperties().getPointerSize();
      stackIndex -= pointerSize*atlas->getNumberOfPaddingSlots();
      traceMsg(self()->comp()," with padding: %d,", stackIndex);
      }

   uint32_t stackIndexBeforeAlignment = stackIndex;
   self()->alignOffset(stackIndex, localObjectAlignment);

   traceMsg(self()->comp()," after alignment: %d\n", stackIndex);

   // Update numberOfSlotsMapped in stackAtlas otherwise there will be mis-match
   // between the compile-time gc maps and the runtime gc maps
   //
   uint32_t adjust = stackIndexBeforeAlignment - stackIndex;
   int32_t numberOfSlotsMapped = atlas->getNumberOfSlotsMapped();
   atlas->setNumberOfSlotsMapped(numberOfSlotsMapped + adjust/pointerSize);
   }

/** \brief
 *     Align stack offset local object without collected fields
 *
 *  \param stackIndex
 *     Offset of the local object
 *
 *  \return
 *     stackIndex is modified in place
 */
void J9::X86::Linkage::alignLocalObjectWithoutCollectedFields(uint32_t & stackIndex)
   {
   int32_t localObjectAlignment = self()->cg()->fej9()->getLocalObjectAlignmentInBytes();
   // Note that sizeofReferenceAddress is identical to the size of a stack slot
   // Both sizeofReferenceAddress and localObjectAlignment are powers of 2,
   // and it's safe to skip the alignment when sizeofReferenceAddress is a multiple
   // of localObjectAlignment
   if (localObjectAlignment <= TR::Compiler->om.sizeofReferenceAddress())
      return;
   // Align uncollected local object
   traceMsg(self()->cg()->comp(), "\nLOCAL OBJECT ALIGNMENT: stack offset before alignment: %d,", stackIndex);

   self()->alignOffset(stackIndex, localObjectAlignment);

   traceMsg(self()->cg()->comp(), " after alignment: %d\n", stackIndex);
   }
