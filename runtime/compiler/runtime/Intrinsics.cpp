/*******************************************************************************
 * Copyright IBM Corp. and others 2023
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH
 *Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include <cstdint>
#include <runtime/Intrinsics.hpp>

extern "C" {

static uint32_t LONG_MASK = 0xffffffffL;

inline uint64_t rotateRight(uint64_t x, uint32_t n)
   {
   return (x >> n) | (x << (-n & 31));
   }

// Compute a big integer add of a carry in bit stored in `carry` starting at the index
// offset by `offset` and `mlen`, return the carry out resulting from the addition.
inline uint32_t addOne(uint32_t a[], uint32_t alen, uint32_t offset, uint32_t mlen,
                       uint32_t carry)
   {
   offset = alen - 1 - mlen - offset;
   uint64_t t = (a[offset] & LONG_MASK) + (carry & LONG_MASK);

   a[offset] = t;
   if ((t >> 32) == 0)
      return 0;

   while (--mlen >= 0)
      {
      if (--offset < 0)
         return 1;

      a[offset]++;

      if (a[offset] != 0)
         return 0;
      }

   return 1;
   }


// Multiply digits of a big integer `in[]` by `k` and accumulate the product
// to `out[]` starting from the index specified by `offset`. This is used to
// compute a single rank of an off diagonal multipliciation specified in `implSquareToLen`
inline uint32_t implMulAdd(uint32_t out[], uint32_t outlen, uint32_t in[],
                  uint32_t offset, uint32_t len, uint32_t k)
   {
   uint64_t kLong = k & LONG_MASK;
   uint64_t carry = 0;

   offset = outlen - offset - 1;
   for (auto j = len - 1; j >= 0; j--)
      {
      uint64_t product = kLong * in[j] + out[offset] + carry;
      out[offset--] = (uint32_t) product;
      carry = rotateRight(product, 32);
      }

   return (uint32_t) carry;
   }

// Shift a big integer stored in 32-bit digits by 1 bit.
inline void shiftLeftBy1(uint32_t a[], uint32_t len)
   {
   for (uint32_t i = 0, c = a[i], m = i + len - 1; i < m; i++)
      {
      auto b = c;
      c = a[i + 1];
      a[i] = (b << 31) | rotateRight(c, 31);
      }
   a[len - 1] <<= 31;
   }

/** \brief
 *    Computes a multiprecision mutiplication between two operands
 *    which are equal, also known as squaring. Adapted from the
 *    implementation of java.math.BigInteger.implSquareToLen
 *
 *  \param xarray the input to be squared stored as 32-bit digits
 *
 *  \param len    the length of xarray
 *
 *  \param zarray the array for the product to be stored, initially empty
 *
 *  \param zlen   the lenfth of zarray
 *
 *  \return
 *    the z array populated with the square of x
 */
void *J9FASTCALL intrinsicImplSquareToLen(J9VMThread *currentThread,
                                          j9array_t xarray, uint32_t len,
                                          j9array_t zarray, uint32_t zlen)
   {
   U_32 *x = J9JAVAARRAY_EA(currentThread, xarray, 0, U_32);
   U_32 *z = J9JAVAARRAY_EA(currentThread, zarray, 0, U_32);

   /*
    * The algorithm used here is adapted from the Java class library.
    * which itself was adapted from Colin Plumb's C library.
    *
    * Technique: Consider the partial products in the multiplication
    * of "abcde" by itself:
    *
    *               a  b  c  d  e
    *            *  a  b  c  d  e
    *          ==================
    *              ae be ce de ee
    *           ad bd cd dd de
    *        ac bc cc cd ce
    *     ab bb bc bd be
    *  aa ab ac ad ae
    *
    * Note that everything above the main diagonal:
    *              ae be ce de = (abcd) * e
    *           ad bd cd       = (abc) * d
    *        ac bc             = (ab) * c
    *     ab                   = (a) * b
    *
    * is a copy of everything below the main diagonal:
    *                       de
    *                 cd ce
    *           bc bd be
    *     ab ac ad ae
    *
    * Thus, the sum is 2 * (off the diagonal) + diagonal.
    *
    * This is accumulated beginning with the diagonal (which
    * consist of the squares of the digits of the input), which is then
    * divided by two, the off-diagonal added, and multiplied by two
    * again.  The low bit is simply a copy of the low bit of the
    * input, so it doesn't need special care.
    */

   uint32_t lastProductLowWord = 0;

   // compute the main diagonal
   for (auto j = 0, i = 0; j < len; j++)
      {
      uint64_t piece = x[j];
      uint64_t product = piece * piece;
      z[i++] = (lastProductLowWord << 31) | rotateRight(product, 33);
      z[i++] = (uint32_t) rotateRight(product, 1);
      lastProductLowWord = (uint32_t) product;
      }

   // compute the off diagonal
   for (uint32_t i = len, offset = 1; i > 0; i--, offset += 2)
      {
      auto t = x[i - 1];
      t = implMulAdd(z, zlen, x, offset, i - 1, t);
      addOne(z, zlen, offset - 1, i, t);
      }

   shiftLeftBy1(z, zlen);
   z[zlen - 1] |= x[len - 1] & 1;

   return zarray;
   }
}
