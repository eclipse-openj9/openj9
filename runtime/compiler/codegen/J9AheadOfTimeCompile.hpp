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

#ifndef J9_AHEADOFTIMECOMPILE_HPP
#define J9_AHEADOFTIMECOMPILE_HPP

#ifndef J9_AHEADOFTIMECOMPILE_CONNECTOR
#define J9_AHEADOFTIMECOMPILE_CONNECTOR
namespace J9 { class AheadOfTimeCompile; }
namespace J9 { typedef J9::AheadOfTimeCompile AheadOfTimeCompileConnector; }
#endif // J9_AHEADOFTIMECOMPILE_CONNECTOR

#include "codegen/OMRAheadOfTimeCompile.hpp"
#include "runtime/RelocationRecord.hpp"

#include <stdint.h>
#include "env/jittypes.h"

namespace TR { class Compilation; }

namespace J9
{

class OMR_EXTENSIBLE AheadOfTimeCompile : public OMR::AheadOfTimeCompileConnector
   {
   public:
   static const size_t SIZEPOINTER = sizeof(uintptr_t);

   AheadOfTimeCompile(uint32_t *headerSizeMap, TR::Compilation * c) :
      OMR::AheadOfTimeCompileConnector(headerSizeMap, c)
      {
      }

   uint8_t* emitClassChainOffset(uint8_t* cursor, TR_OpaqueClassBlock* classToRemember);
   uintptr_t getClassChainOffset(TR_OpaqueClassBlock* classToRemember);
   uintptr_t findCorrectInlinedSiteIndex(void *constantPool, uintptr_t currentInlinedSiteIndex);

   void dumpRelocationData();
   uint8_t* dumpRelocationHeaderData(uint8_t *cursor, bool isVerbose);

   uint8_t *initializeCommonAOTRelocationHeader(TR::IteratedExternalRelocation *relocation, TR_RelocationRecord *reloRecord);

   static void interceptAOTRelocation(TR::ExternalRelocation *relocation);

   /**
    * Return true if an ExternalRelocation of kind TR_ClassAddress is expected
    * to contain a pointer to TR_RelocationRecordInformation.
    */
   static bool classAddressUsesReloRecordInfo() { return false; }

   protected:

   /**
    * @brief TR_J9SharedCache::offsetInSharedCacheFrom* asserts if the pointer
    * passed in does not exist in the SCC. Under HCR, when an agent redefines
    * a class, it causes the J9Class pointer to stay the same, but the
    * J9ROMClass pointer changes. This means that if the compiler has a
    * reference to a J9Class who J9ROMClass was in the SCC at one point in the
    * compilation, it may no longer be so at another point in the compilation.
    *
    * This means that the compilation is no longer valid and should be aborted.
    * Even if there isn't an abort during the compilation, at the end of the
    * compilation, the compiler will fail the compile if such a redefinition
    * occurred.
    *
    * Calling TR_J9SharedCache::offsetInSharedCacheFromPointer after such a
    * redefinition could result in an assert. Therefore, this method exists as
    * a wrapper around TR_J9SharedCache::isROMClassInSharedCache which doesn't
    * assert and conveniently, updates the location referred to by the cacheOffset
    * pointer passed in as a parameter.
    *
    * If the ptr isn't in the the SCC, then the current method will abort the
    * compilation. If the ptr is in the SCC, then the cacheOffset will be updated.
    *
    * @param sharedCache pointer to the TR_SharedCache object
    * @param romClass J9ROMClass * whose offset in the SCC is required
    * @return The offset into the SCC of romClass
    */
   uintptr_t offsetInSharedCacheFromROMClass(TR_SharedCache *sharedCache, J9ROMClass *romClass);

   /**
    * @brief Same circumstance as offsetInSharedCacheFromROMClass above
    *
    * @param sharedCache pointer to the TR_SharedCache object
    * @param romMethod J9ROMMethod * whose offset in the SCC is required
    * @return The offset into the SCC of romMethod
    */
   uintptr_t offsetInSharedCacheFromROMMethod(TR_SharedCache *sharedCache, J9ROMMethod *romMethod);

   /**
    * @brief Wrapper around TR_J9SharedCache::offsetInSharedCacheFromPointer for
    *        consistency with the above APIs
    *
    * @param sharedCache pointer to the TR_SharedCache object
    * @param ptr pointer whose offset in the SCC is required
    * @return The offset into the SCC of ptr
    */
   uintptr_t offsetInSharedCacheFromPointer(TR_SharedCache *sharedCache, void *ptr);
   };

}

#endif // TR_J9AHEADOFTIMECOMPILE_HPP
