/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#include "env/ClassLoaderTable.hpp"

#include <stdint.h>
#include "env/PersistentCHTable.hpp"
#include "env/jittypes.h"

struct TR_ClassLoaderInfo
   {
   TR_ALLOC(TR_Memory::PersistentCHTable)
   TR_ClassLoaderInfo(void *classLoaderPointer, void *classChainPointer) :
      _classLoaderPointer(classLoaderPointer),
      _classChainPointer(classChainPointer),
      _next(NULL)
      {
      }

   void *_classLoaderPointer;
   void *_classChainPointer;
   TR_ClassLoaderInfo *_next;
   };


TR_PersistentClassLoaderTable::TR_PersistentClassLoaderTable(TR_PersistentMemory *trPersistentMemory)
   {
   for (int32_t i=0;i < CLASSLOADERTABLE_SIZE;i++)
      {
      _loaderTable[i] = NULL;
      _classChainTable[i] = NULL;
      }
   _trPersistentMemory = trPersistentMemory;
   _sharedCache = NULL;
   }

int32_t
TR_PersistentClassLoaderTable::hashLoader(void *classLoaderPointer)
   {
   return ((uintptrj_t)classLoaderPointer % CLASSLOADERTABLE_SIZE);
   }

int32_t
TR_PersistentClassLoaderTable::hashClassChain(void *classChainPointer)
   {
   return ((uintptrj_t)classChainPointer % CLASSLOADERTABLE_SIZE);
   }

int32_t TR_PersistentClassLoaderTable::_numClassLoaders = 0;

void
TR_PersistentClassLoaderTable::associateClassLoaderWithClass(void *classLoaderPointer, TR_OpaqueClassBlock *clazz)
   {
   if (!sharedCache())
      return;

   // need to acquire some kind of lock here

   // first index by class loader
   int32_t index = hashLoader(classLoaderPointer);
   TR_ClassLoaderInfo *info = _loaderTable[index];
   while (info != NULL && info->_classLoaderPointer != classLoaderPointer)
      info = info->_next;

   if (!info)
      {
      //fprintf(stderr, "PCLT:associating loader %p with first loaded class %p\n", classLoaderPointer, clazz);
      void *classChainPointer = (void *) sharedCache()->rememberClass(clazz);
      //fprintf(stderr,"\tclass chain pointer %p\n", classChainPointer);
      if (classChainPointer)
         {
         void *classChainOffsetInSharedCache=NULL;
         bool inCache = sharedCache()->isPointerInSharedCache(classChainPointer, classChainOffsetInSharedCache);
         TR_ASSERT(inCache, "rememberClass should only return class chain pointers that are in the shared class cache");

         //fprintf(stderr,"PCLT:\tclass chain pointer %p, offset %p\n", classChainPointer, classChainOffsetInSharedCache);

         info = new (trPersistentMemory()) TR_ClassLoaderInfo(classLoaderPointer, classChainPointer);
         if (!info)
            {
            // this is a bad situation because we can't associate the right class with this class loader
            // how to prevent this class loader from associating with a different class?
            // probably not critical if multiple class loaders aren't routinely loading the exact same class
            return;
            }

         info->_next = _loaderTable[index];
         _loaderTable[index] = info;
         //fprintf(stderr,"PCLT:\tinserted into loader table at index %d\n", index);

         // now index by class chain
         index = hashClassChain(classChainPointer);
         info = _classChainTable[index];
         while (info != NULL && info->_classChainPointer != classChainPointer)
            info = info->_next;

         if (info)
            {
            //fprintf(stderr,"PCLT:\tanother loader %p has the same first loaded class (chain %p)!\n", info->_classLoaderPointer, info->_classChainPointer);
            // getting here means there are two class loaders with identical first loaded class
            // current heuristic doesn't work in this scenario
            // TODO some diagnostic output here
            // we have added this loader to _loaderTable, which has nice side benefit that we won't keep trying it, so
            //    leave it there
            //fprintf(stderr,"WARNING: multiple class loaders have same identifying class %p\n", clazz);
            return;
            }

         info = new (trPersistentMemory()) TR_ClassLoaderInfo(classLoaderPointer, classChainPointer);
         if (!info)
            {
            // we have to leave the _loaderTable entry we just created intact to prevent other loaded classes from
            // becoming the identifying class for this loader
            return;
            }
         info->_next = _classChainTable[index];
         _classChainTable[index] = info;
         //fprintf(stderr,"PCLT:\tinserted into class chain table at index %d\n", index);
         }
      else
         {
         //fprintf(stderr,"PCLT:\tclass can't be stored in cache\n");
         }

      //fprintf(stderr,"PCLT:num class loaders now %d\n", ++_numClassLoaders);
      }
   }

void *
TR_PersistentClassLoaderTable::lookupClassChainAssociatedWithClassLoader(void *classLoaderPointer)
   {
   if (!sharedCache())
      return NULL;

   // need to acquire some kind of lock here

   int32_t index = hashLoader(classLoaderPointer);
   TR_ClassLoaderInfo *info = _loaderTable[index];
   while (info != NULL && info->_classLoaderPointer != classLoaderPointer)
      info = info->_next;

   void *classChainPointer=NULL;
   if (info)
      {
      classChainPointer = info->_classChainPointer;
      }

   //fprintf(stderr, "PCLT:looking up class chain for loader %p: found %p\n", classLoaderPointer, classChainPointer);

   return classChainPointer;
   }

void *
TR_PersistentClassLoaderTable::lookupClassLoaderAssociatedWithClassChain(void *classChainPointer)
   {
   if (!sharedCache())
      return NULL;

   // need to acquire some kind of lock here

   int32_t index = hashClassChain(classChainPointer);
   TR_ClassLoaderInfo *info = _classChainTable[index];
   while (info != NULL && info->_classChainPointer != classChainPointer)
      info = info->_next;

   void *classLoaderPointer=NULL;
   if (info)
      {
      classLoaderPointer = info->_classLoaderPointer;
      }

   //fprintf(stderr, "PCLT:looking up class loader for class chain %p: found %p\n", classChainPointer, classLoaderPointer);

   return classLoaderPointer;
   }

void
TR_PersistentClassLoaderTable::removeClassLoader(void *classLoaderPointer)
   {
   // need to acquire some kind of lock here

   //fprintf(stderr,"PCLT:removing unloaded loader %p\n", classLoaderPointer);

   // now remove it from the loaderTable
   int32_t index = hashLoader(classLoaderPointer);
   TR_ClassLoaderInfo *prevInfo = NULL;
   TR_ClassLoaderInfo *info = _loaderTable[index];
   while (info != NULL && info->_classLoaderPointer != classLoaderPointer)
      {
      prevInfo = info;
      info = info->_next;
      }

   if (info)
      {
      //fprintf(stderr,"PCLT:\tfound it in loaderTable at index %d\n", index);
      if (prevInfo)
         prevInfo->_next = info->_next;
      else
         _loaderTable[index] = info->_next;
      trPersistentMemory()->freePersistentMemory(info);
      }

   // now remove it from the classChainTable
   for (index = 0;index < CLASSLOADERTABLE_SIZE;index++)
      {
      prevInfo = NULL;
      info = _classChainTable[index];
      while (info && info->_classLoaderPointer != classLoaderPointer)
         {
         prevInfo = info;
         info = info->_next;
         }

      if (info)
         break;
      }

   if (info)
      {
      //fprintf(stderr,"PCLT:\tfound it in classChainTable at index %d\n", index);
      if (prevInfo)
         prevInfo->_next = info->_next;
      else
         _classChainTable[index] = info->_next;
      trPersistentMemory()->freePersistentMemory(info);
      }

   //fprintf(stderr,"PCLT:num class loaders now %d\n", --_numClassLoaders);
   }
