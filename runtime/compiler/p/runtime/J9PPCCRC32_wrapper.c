/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#define CRC_TABLE
#include "p/runtime/J9PPCCRC32_constants.h"

#define VMX_ALIGN       16
#define VMX_ALIGN_MASK  (VMX_ALIGN-1)

#ifdef REFLECT
static unsigned int crc32_align(unsigned int crc, unsigned char *p,
                                unsigned long len)
   {
   while (len--)
      crc = crc_table[(crc ^ *p++) & 0xff] ^ (crc >> 8);
   return crc;
   }

unsigned int crc32_oneByte(unsigned int crc, unsigned int b)
   {
#ifdef CRC_XOR
   crc ^= 0xffffffff;
#endif

   crc = crc_table[(crc ^ b) & 0xff] ^ (crc >> 8);

#ifdef CRC_XOR
   crc ^= 0xffffffff;
#endif
   return crc;
   }
#else
static unsigned int crc32_align(unsigned int crc, unsigned char *p,
                                unsigned long len)
   {
   while (len--)
      crc = crc_table[((crc >> 24) ^ *p++) & 0xff] ^ (crc << 8);
   return crc;
   }

unsigned int crc32_oneByte(unsigned int crc, unsigned int b)
   {
#ifdef CRC_XOR
   crc ^= 0xffffffff;
#endif

   crc = crc_table[((crc >> 24) ^ b) & 0xff] ^ (crc << 8);

#ifdef CRC_XOR
   crc ^= 0xffffffff;
#endif
   return crc;
   }
#endif

unsigned int crc32_no_vpmsum(unsigned int crc, unsigned char *p,
                             unsigned long len)
   {
#ifdef CRC_XOR
   crc ^= 0xffffffff;
#endif

   crc = crc32_align(crc, p, len);

#ifdef CRC_XOR
   crc ^= 0xffffffff;
#endif

   return crc;
   }

unsigned int __crc32_vpmsum(unsigned int crc, unsigned char *p,
                            unsigned long len);

unsigned int crc32_vpmsum(unsigned int crc, unsigned char *p,
                          unsigned long len)
   {
   unsigned int prealign;
   unsigned int tail;

#ifdef CRC_XOR
   crc ^= 0xffffffff;
#endif

   if (len < VMX_ALIGN + VMX_ALIGN_MASK) {
      crc = crc32_align(crc, p, len);
      goto out;
   }

   if ((unsigned long)p & VMX_ALIGN_MASK) {
      prealign = VMX_ALIGN - ((unsigned long)p & VMX_ALIGN_MASK);
      crc = crc32_align(crc, p, prealign);
      len -= prealign;
      p += prealign;
   }

   crc = __crc32_vpmsum(crc, p, len & ~VMX_ALIGN_MASK);

   tail = len & VMX_ALIGN_MASK;
   if (tail) {
      p += len & ~VMX_ALIGN_MASK;
      crc = crc32_align(crc, p, tail);
   }

out:
#ifdef CRC_XOR
   crc ^= 0xffffffff;
#endif

   return crc;
   }
