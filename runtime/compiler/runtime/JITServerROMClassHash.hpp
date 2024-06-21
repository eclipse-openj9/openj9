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

#ifndef JITSERVER_ROMCLASS_HASH_H
#define JITSERVER_ROMCLASS_HASH_H

#include <functional>
#include <limits.h>
#include <string.h>

#include "infra/Assert.hpp"

struct J9Class;
struct J9ROMClass;
class TR_J9VMBase;
class TR_Memory;


// Assuming a 256-bit hash such as SHA-256
#define ROMCLASS_HASH_BITS 256

static_assert(ROMCLASS_HASH_BITS % (CHAR_BIT * sizeof(size_t)) == 0,
              "Invalid number of ROMClass hash bits");

#define ROMCLASS_HASH_BYTES (ROMCLASS_HASH_BITS / CHAR_BIT)
#define ROMCLASS_HASH_WORDS (ROMCLASS_HASH_BYTES / sizeof(size_t))


struct JITServerROMClassHash
   {
public:
   JITServerROMClassHash() : _data() { }

   // Computes the hash of romClass assuming it is already packed (only possible on the server side)
   JITServerROMClassHash(const J9ROMClass *romClass) { init(romClass, romClass->romSize); }

   // Builds a hash for an array class by combining the ROMClass hash of the "[L" object array class,
   // the ROMClass hash of the base component class, and the number of array dimensions.
   JITServerROMClassHash(const JITServerROMClassHash &objectArrayHash,
                         const JITServerROMClassHash &baseComponentHash, size_t numDimensions)
      {
      init(objectArrayHash, baseComponentHash, numDimensions);
      }

   // Computes the hash of romClass, packing it if needed, using trMemory for scratch allocations.
   // A NULL fej9 means that romClass is already packed (only possible on the server side).
   // If checkGenerated is true and romClass is a recognized runtime-generated class (e.g., a lambda),
   // it is re-packed to exclude non-deterministic parts of all instances of the class name string.
   JITServerROMClassHash(const J9ROMClass *romClass, TR_Memory &trMemory, TR_J9VMBase *fej9,
                         bool checkGenerated = false, size_t prefixLength = 0)
      {
      init(romClass, trMemory, fej9, checkGenerated, prefixLength);
      }

   bool operator==(const JITServerROMClassHash &h) const
      {
      return memcmp(_data, h._data, sizeof(_data)) == 0;
      }

   bool operator!=(const JITServerROMClassHash &h) const { return !(*this == h); }

   size_t getWord(size_t idx) const
      {
      TR_ASSERT(idx < sizeof(_data) / sizeof(_data[0]), "Out of bounds");
      return _data[idx];
      }

   const char *toString(char *buffer, size_t size) const;

   // Returns the hash of the "[L" ROMClass. Caches the result for reuse in future invocations. Only used on the client.
   static const JITServerROMClassHash &getObjectArrayHash(const J9ROMClass *objectArrayROMClass,
                                                          TR_Memory &trMemory, TR_J9VMBase *fej9);

private:
   void init(const void *data, size_t size);
   void init(const J9ROMClass *romClass, TR_Memory &trMemory,
             TR_J9VMBase *fej9, bool checkGenerated, size_t prefixLength);
   void init(const JITServerROMClassHash &objectArrayHash,
             const JITServerROMClassHash &baseComponentHash, size_t numDimensions);

   static volatile bool _cachedObjectArrayHash;
   static JITServerROMClassHash _objectArrayHash;

   size_t _data[ROMCLASS_HASH_WORDS];
   };


// std::hash specialization for using JITServerROMClassHash as unordered map key
namespace std
   {
   template<> struct hash<JITServerROMClassHash>
      {
      size_t operator()(const JITServerROMClassHash &k) const noexcept
         {
         // Simply truncate the hash and use its first sizeof(size_t) bytes
         return k.getWord(0);
         }
      };
   }


#endif /* JITSERVER_ROMCLASS_HASH_H */
