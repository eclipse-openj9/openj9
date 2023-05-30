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
#include "net/LoadSSLLibs.hpp"
#include "runtime/JITServerROMClassHash.hpp"


JITServerROMClassHash::JITServerROMClassHash(const J9ROMClass *romClass)
   {
   EVP_MD_CTX *ctx = OEVP_MD_CTX_new();
   if (!ctx)
      throw std::bad_alloc();//The only possible error is memory allocation failure
   if (!OEVP_DigestInit_ex(ctx, OEVP_sha256(), NULL))
      throw std::bad_alloc();//The only possible error is memory allocation failure

   int success = OEVP_DigestUpdate(ctx, romClass, romClass->romSize);
   TR_ASSERT(success, "EVP_DigestUpdate() failed");
   unsigned int hashSize = 0;
   success = OEVP_DigestFinal_ex(ctx, (uint8_t *)_data, &hashSize);
   TR_ASSERT(success, "EVP_DigestFinal() failed");
   TR_ASSERT(hashSize == sizeof(_data), "Invalid hash size");

   OEVP_MD_CTX_free(ctx);
   }

const char *
JITServerROMClassHash::toString(char *buffer, size_t size) const
   {
   TR_ASSERT(size > sizeof(_data) * 2, "Buffer too small");

   char *s = buffer;
   for (size_t i = 0; i < sizeof(_data); ++i)
      s += sprintf(s, "%02x", ((uint8_t *)_data)[i]);
   return buffer;
   }
