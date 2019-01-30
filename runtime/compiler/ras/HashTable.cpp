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

#include "ras/HashTable.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "env/TRMemory.hpp"
#include "infra/Assert.hpp"

void *
TR_HashTableEntry::operator new[] (size_t s, TR_Memory *mem)
   {
   return mem->allocateMemory(s, heapAlloc, TR_MemoryBase::HashTableEntry);
   }

void *
TR_HashTable::operator new (size_t s, TR_Memory *mem)
   {
   return mem->allocateMemory(s, heapAlloc, TR_MemoryBase::HashTable);
   }

TR_HashTable::TR_HashTable(TR_Memory *mem, TR_HashIndex numElements)
   :_mem(mem)
   {
   uint32_t closedAreaSize = 2, openAreaSize;

   if (numElements > MINIMUM_HASHTABLE_SIZE)
      while (closedAreaSize < numElements)
         closedAreaSize <<= 1;
   else
      closedAreaSize = MINIMUM_HASHTABLE_SIZE;

   openAreaSize = closedAreaSize >> 2;

   _mask = closedAreaSize - 1;
   _nextFree = closedAreaSize + 1;
   _tableSize = closedAreaSize + openAreaSize;
   _highestIndex = 0;

   _table = new (_mem) TR_HashTableEntry[_tableSize];

   // Invalidate all hash table entries.
   TR_HashIndex i;
   for (i = 0; i < _nextFree; i++)
      _table[i].invalidate();

   // Initialize the rehash area to link up the free chain.
   for (i = _nextFree; i < _tableSize - 1; i++)
      {
      _table[i].invalidate();
      _table[i].setCollisionChain(i + 1);
      }

   _table[_tableSize - 1].invalidate();
   _table[_tableSize - 1].setCollisionChain(0);
   }

TR_HashTable::TR_HashTable(const TR_HashTable &table, TR_Memory *mem)
   : _mem(mem),
     _mask(table._mask),
     _nextFree(table._nextFree),
     _tableSize(table._tableSize),
     _highestIndex(table._highestIndex)
   {
   _table = new (_mem) TR_HashTableEntry[_tableSize];

   for (TR_HashIndex i = 0; i < _tableSize; i++)
      {
      TR_HashTableEntry &entry = table._table[i];
      if (entry.isValid())
         {
         new ((void *)(_table + i)) TR_HashTableEntry(entry);
         }
      else
         {
         _table[i].invalidate();
         _table[i].setCollisionChain(entry.getCollisionChain());
         }
      }
   }

bool
TR_HashTable::locate(void *key, TR_HashIndex &index, TR_HashCode hashCode)
   {
   if (hashCode == 0)
      {
      hashCode = calculateHashCode(key);
      TR_ASSERT( hashCode != 0, "invalid hash code\n");
      }

   index = (hashCode & _mask) + 1;
   TR_ASSERT( index != 0, "invalid index\n");

   if (!_table[index].isValid())
      return false;

#ifdef DEBUG
   uint32_t     collisions = 0;
   TR_HashIndex collisionIndex = index;
#endif
   while ((_table[index].getHashCode() != hashCode) ||
          !isEqual(key, _table[index].getKey()))
      {
#ifdef DEBUG
      ++collisions;
#endif
      if (_table[index].getCollisionChain() != 0)
         index = _table[index].getCollisionChain();
      else
         return false;
      }

   return true;
   }

bool
TR_HashTable::add(void *key, void *data, TR_HashCode hashCode)
   {
   TR_HashIndex index;

   if (hashCode == 0)
      {
      hashCode = calculateHashCode(key);
      TR_ASSERT( hashCode != 0, "invalid hash code\n");
      }

   if (locate(key, index, hashCode))
      return false;

   if (_nextFree == 0)
      {
      grow();
      bool found = locate(key, index, hashCode);
      TR_ASSERT( !found, "buggy TR_HashTable::growAndRehash()\n");
      }

   if (_table[index].isValid())
      {
      _table[index].setCollisionChain(_nextFree);
      index = _nextFree;
      _nextFree = _table[_nextFree].getCollisionChain();
      }

   if (index > _highestIndex)
      _highestIndex = index;

   new ((void *)(_table + index)) TR_HashTableEntry(key, data, hashCode);

   return true;
   }

void
TR_HashTable::remove(TR_HashIndex index)
   {
   TR_ASSERT( _table[index].isValid(), "attempt to remove invalid hash table entry\n");

   // If the entry is in the rehash area, locate the head of the hash chain and
   // follow it to unlink this entry from the chain.  Then return the rehash area
   // space to the free pool.
   if (index > _mask + 1)
      {
      TR_HashIndex headOfChain = (_table[index].getHashCode() & _mask) + 1;
      TR_HashIndex collisionIndex;

      for (collisionIndex = headOfChain;
           _table[collisionIndex].getCollisionChain() != index;
           collisionIndex = _table[collisionIndex].getCollisionChain())
         TR_ASSERT( collisionIndex != 0, "cannot find record on expected chain\n");

      _table[collisionIndex].setCollisionChain(_table[index].getCollisionChain());
      _table[index].setCollisionChain(_nextFree);
      _table[index].invalidate();
      _nextFree = index;
      }
   else
      {
      // The entry is in the closed hash table section.
      // If it has any chained entries in the rehash area, then choose one
      // to put in its place.
      TR_HashIndex firstCollision;
      if (( firstCollision = _table[index].getCollisionChain() ) != 0)
         {
         memcpy(_table + index, _table + firstCollision, sizeof(TR_HashTableEntry));
         _table[firstCollision].setCollisionChain(_nextFree);
         _table[firstCollision].invalidate();
         _nextFree = firstCollision;
         }
      else
         {
         _table[index].invalidate();
         }
      }
   }

bool
TR_HashTable::isEmpty() const
   {
   bool empty = true;
   for (TR_HashIndex i = 0; i < _tableSize; i++)
      {
      if (_table[i].isValid())
         {
         empty = false;
         break;
         }
      }
   return empty;
   }

void
TR_HashTable::removeAll()
   {
   TR_HashIndex i;

   _highestIndex = 0;

   for (i = 0; i <= _mask + 1; i++)
      if (_table[i].isValid())
         _table[i].invalidate();

   _nextFree = _mask + 2;

   for (i = _nextFree; i < _tableSize - 1; i++)
      {
      if (_table[i].isValid())
         _table[i].invalidate();

      _table[i].setCollisionChain(i + 1);
      }

   if (_table[_tableSize - 1].isValid())
      _table[_tableSize - 1].invalidate();

   _table[_tableSize - 1].setCollisionChain(0);
   }

void
TR_HashTable::grow(uint32_t newSize)
   {
   uint32_t closedAreaSize = 2, openAreaSize;

   while (closedAreaSize < newSize)
      closedAreaSize <<= 1;

   openAreaSize = closedAreaSize >> 2;

   if (closedAreaSize + openAreaSize < _tableSize)
      return;

   growAndRehash(_table, _tableSize, closedAreaSize, openAreaSize);
   }

void
TR_HashTable::grow()
   {
   uint32_t newSize = (_mask << 1) | 1;
   growAndRehash(_table, _tableSize, newSize + 1, (newSize + 1) / 4);
   }

void
TR_HashTable::growAndRehash(TR_HashTableEntry *oldTable,
                            TR_HashIndex       oldSize,
                            TR_HashIndex       closedAreaSize,
                            TR_HashIndex       openAreaSize)
   {
   _mask = closedAreaSize - 1;
   _nextFree = closedAreaSize + 1;
   _tableSize = closedAreaSize + openAreaSize;
   _highestIndex = 0;

   _table = new (_mem) TR_HashTableEntry[_tableSize];

   TR_HashIndex i;
   for (i = 0; i < _nextFree; i++)
      _table[i].invalidate();

   for (i = _nextFree; i < _tableSize - 1; i++)
      {
      _table[i].invalidate();
      _table[i].setCollisionChain(i + 1);
      }

   _table[_tableSize - 1].invalidate();
   _table[_tableSize - 1].setCollisionChain(0);

   for (i = 0; i < oldSize; i++)
      {
      if (oldTable[i].isValid())
         {
         TR_HashIndex index;
         TR_HashCode oldHashCode = oldTable[i].getHashCode();

         bool found = locate(oldTable[i].getKey(), index, oldHashCode);
         TR_ASSERT(!found, "unable to rehash entry %d", i);

         if (_table[index].isValid())
            {
            _table[index].setCollisionChain(_nextFree);
            index = _nextFree;
            _nextFree = _table[_nextFree].getCollisionChain();
            }

         if (index > _highestIndex)
            _highestIndex = index;

         memcpy(_table + index, oldTable + i, sizeof(TR_HashTableEntry));
         _table[index].setCollisionChain(0);
         }
      }
   }
