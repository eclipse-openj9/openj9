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

#ifndef HASHTABLE_INCL
#define HASHTABLE_INCL

#include <stddef.h>
#include <stdint.h>
#include "env/jittypes.h"
#include "infra/Assert.hpp"

class TR_Memory;

#define MINIMUM_HASHTABLE_SIZE 16

typedef uint32_t TR_HashIndex;
typedef uintptrj_t TR_HashCode;

class TR_HashTableEntry
   {
   public:

   TR_HashTableEntry() {}

   TR_HashTableEntry(void *key, void *data, TR_HashCode hashCode)
         : _key(key),
           _data(data),
           _hashCode(hashCode),
           _chain(0) {}

   TR_HashTableEntry(const TR_HashTableEntry &entry)
      : _key(entry._key),
        _data(entry._data),
        _hashCode(entry._hashCode),
        _chain(entry._chain) {}

   void *operator new[](size_t size, TR_Memory *m);
   void *operator new(size_t size, void *cell) {return cell;}

   void *getKey() const    {return _key;}
   void *setKey(void *key) {return (_key = key);}

   void *getData() const     {return _data;}
   void *setData(void *data) {return (_data = data);}

   TR_HashCode getHashCode() const               {return _hashCode;}
   TR_HashCode setHashCode(TR_HashCode hashCode) {return (_hashCode = hashCode);}

   TR_HashIndex getCollisionChain() const             {return _chain;}
   TR_HashIndex setCollisionChain(TR_HashIndex chain) {return (_chain = chain);}

   bool isValid() const {return (_hashCode != 0);}
   void invalidate()    {_hashCode = 0;}

   private:

   void *        _key;
   void *        _data;
   TR_HashCode   _hashCode;    // unmasked hash value
   TR_HashIndex  _chain;       // collision chain
   };

// TR_HashTable
//
// An extensible hash table with arbitrary key and data types.
// Note that keys and data must be explicitly cast when adding
// to, and retrieving from, the hash table, for efficiency sake.
// The following methods can be overridden to suit the desired
// key type.
//
//    TR_HashCode calculateHashCode(void *key)
//       Computes a hash function for the given key
//
//    bool isEqual (void *key1, void *key2)
//       Determines if a pair of keys are equal
//
class TR_HashTable
   {
   friend class TR_DebugExt;
   public:

   TR_HashTable(TR_Memory *mem, TR_HashIndex numElements = 64);

   TR_HashTable(const TR_HashTable &table, TR_Memory *mem); // copy constructor

   void *operator new(size_t size, TR_Memory *mem);

   // Locates a record with the given key.  Returns true iff a record is
   // found.  If the record is found, set the index reference parameter.
   // If a hash code is given, use it instead of computing a new code.
   //
   bool locate(void *key, TR_HashIndex &index, TR_HashCode hashCode = 0);

   // Adds a record given a (key, data) pair.  Returns true iff the record
   // is successfully added.  If a record with the given key already exists,
   // the add will fail.  If the add succeeds, the index reference parameter
   // is set to the index at which the record was added.  If a hash code is
   // given, it is used instead of recomputing the code based on the key.
   //
   bool add(void *key, void *data, TR_HashCode hashCode = 0);

   void *getData(TR_HashIndex index)
      {
      TR_ASSERT(_table[index].isValid(), "cannot retrieve invalid data from hash table\n");
      return _table[index].getData();
      }

   void remove(TR_HashIndex index);
   void removeAll();

   bool isEmpty() const;

   void grow(uint32_t newSize);

   // Assuming key is a pointer to an object , divide its
   // address by 4, and return the quotient as the hash code
   //
   virtual TR_HashCode calculateHashCode(void *key) const {return (TR_HashCode) key >> 2;}

   virtual bool isEqual (void *key1, void *key2) const {return (key1 == key2);}

   protected:

   void grow();
   void growAndRehash(TR_HashTableEntry *, TR_HashIndex, TR_HashIndex, TR_HashIndex);

   TR_Memory                *_mem;
   TR_HashIndex              _tableSize;        // Total table size (closed + open)
   TR_HashIndex              _mask;             // Mask to compute modulus for closed area
   TR_HashIndex              _nextFree;         // Next free slot in the open area
   TR_HashIndex              _highestIndex;     // Highest allocated index
   TR_HashTableEntry        *_table;
   };
#endif
