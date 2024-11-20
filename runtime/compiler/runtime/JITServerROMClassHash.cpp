/*******************************************************************************
 * Copyright IBM Corp. and others 2021
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

#include "j9.h"
#include "AtomicSupport.hpp"
#include "control/JITServerHelpers.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "net/LoadSSLLibs.hpp"
#include "runtime/JITServerROMClassHash.hpp"
#include "runtime/JITServerAOTSerializationRecords.hpp"


void
JITServerROMClassHash::init(const JITServerROMClassHash &objectArrayHash,
                            const JITServerROMClassHash &baseComponentHash, size_t numDimensions)
   {
   size_t data[ROMCLASS_HASH_WORDS * 2 + 1];
   memcpy(data, objectArrayHash._data, sizeof(_data));
   memcpy(data + ROMCLASS_HASH_WORDS, baseComponentHash._data, sizeof(_data));
   data[ROMCLASS_HASH_WORDS * 2] = numDimensions;

   init(data, sizeof(data));
   }


void
JITServerROMClassHash::init(const void *data, size_t size)
   {
   EVP_MD_CTX *ctx = OEVP_MD_CTX_new();
   if (!ctx)
      throw std::bad_alloc(); // The only possible error is memory allocation failure
   if (!OEVP_DigestInit_ex(ctx, OEVP_sha256(), NULL))
      throw std::bad_alloc(); // The only possible error is memory allocation failure

   int success = OEVP_DigestUpdate(ctx, data, size);
   TR_ASSERT(success, "EVP_DigestUpdate() failed");
   unsigned int hashSize = 0;
   success = OEVP_DigestFinal_ex(ctx, (uint8_t *)_data, &hashSize);
   TR_ASSERT(success, "EVP_DigestFinal() failed");
   TR_ASSERT(hashSize == sizeof(_data), "Invalid hash size");

   OEVP_MD_CTX_free(ctx);
   }

void
JITServerROMClassHash::init(const J9ROMClass *romClass, TR_Memory &trMemory,
                            TR_J9VMBase *fej9, bool checkGenerated, size_t prefixLength)
   {
   TR_ASSERT(!prefixLength || checkGenerated, "Invalid arguments");

   TR::StackMemoryRegion region(trMemory);
   size_t packedSize = 0;
   if (checkGenerated && !prefixLength)
      prefixLength = JITServerHelpers::getGeneratedClassNamePrefixLength(romClass);
   J9ROMClass *packedROMClass = JITServerHelpers::packROMClass(romClass, &trMemory, fej9, packedSize, 0, prefixLength);

   init(packedROMClass, packedSize);
   }


const char *
JITServerROMClassHash::toString(char *buffer, size_t size) const
   {
   TR_ASSERT(size > sizeof(_data) * 2, "Buffer too small");

   char *s = buffer;
   size_t len = size;
   for (size_t i = 0; i < sizeof(_data); ++i)
      {
      int amt = snprintf(s, len, "%02x", ((uint8_t *)_data)[i]);
      if ((0 < amt) && ((size_t)amt < len))
         {
         s += amt;
         len -= amt;
         }
      else
         {
         break;
         }
      }
   return buffer;
   }


volatile bool JITServerROMClassHash::_cachedObjectArrayHash = false;
JITServerROMClassHash JITServerROMClassHash::_objectArrayHash;

const JITServerROMClassHash &
JITServerROMClassHash::getObjectArrayHash(const J9ROMClass *objectArrayROMClass, TR_Memory &trMemory, TR_J9VMBase *fej9)
   {
   if (_cachedObjectArrayHash)
      {
      TR_ASSERT(_objectArrayHash == JITServerROMClassHash(objectArrayROMClass, trMemory, fej9),
                "Mismatching cached object array ROMClass hash");
      return _objectArrayHash;
      }

   // There is no need to synchronize writes into _objectArrayHash since the only existing
   // call site of this function in JITServerAOTDeserializer::isClassMatching() is always
   // inside a critical section of the same monitor JITServerAOTDeserializer::_classMonitor.
   _objectArrayHash.init(objectArrayROMClass, trMemory, fej9, false, 0);
   VM_AtomicSupport::writeBarrier();
   _cachedObjectArrayHash = true;
   return _objectArrayHash;
   }
