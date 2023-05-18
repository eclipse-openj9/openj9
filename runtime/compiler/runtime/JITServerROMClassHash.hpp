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

struct J9ROMClass;


// Assuming a 256-bit hash such as SHA-256
#define ROMCLASS_HASH_BITS 256

static_assert(ROMCLASS_HASH_BITS % (CHAR_BIT * sizeof(size_t)) == 0,
              "Invalid number of ROMClass hash bits");

#define ROMCLASS_HASH_BYTES (ROMCLASS_HASH_BITS / CHAR_BIT)
#define ROMCLASS_HASH_WORDS (ROMCLASS_HASH_BYTES / sizeof(size_t))


struct JITServerROMClassHash
   {
public:
   JITServerROMClassHash() { memset(_data, 0, sizeof(_data)); }
   JITServerROMClassHash(const J9ROMClass *romClass);

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

private:
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
