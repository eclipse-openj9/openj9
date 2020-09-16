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

#include <stddef.h>

#ifdef DEBUG
#include <stdlib.h>
#endif

#include "env/VMJ9.h"
#include "j9field.h"
#include "env/j9fieldsInfo.h"
#include "env/IO.hpp"
#include "env/CompilerEnv.hpp"

#include "compile/Compilation.hpp"

static int isReferenceSignature(char *signature)
{
     return ( (signature[0] == 'L' )  || (signature[0] == '[') );
}

int TR_VMField::isReference()
{
     return ( isReferenceSignature(signature) );
}

TR_VMField::TR_VMField(TR::Compilation * comp, J9Class *aClazz, J9ROMFieldShape *fieldShape,  TR_AllocationKind allocKind)
{
   int nameLength=0, sigLength=0;
   char *nPtr=NULL, *sPtr=NULL;
   J9UTF8 *nameUtf8;
   J9UTF8 *sigUtf8;

   ramClass = aClazz;
   modifiers = fieldShape->modifiers;

   shape = fieldShape;
   nameUtf8 = J9ROMFIELDSHAPE_NAME(fieldShape);
   nPtr = (char *) J9UTF8_DATA(nameUtf8);
   sigUtf8 =  J9ROMFIELDSHAPE_SIGNATURE(fieldShape);
   sPtr = (char *) J9UTF8_DATA(sigUtf8);
   nameLength = J9UTF8_LENGTH(nameUtf8)+1;
   name = (char *) comp->trMemory()->allocateMemory(nameLength, allocKind);
   sigLength = J9UTF8_LENGTH(sigUtf8)+1;
   signature = (char *) comp->trMemory()->allocateMemory(sigLength, allocKind);
   memcpy(name, nPtr, nameLength);
   memcpy(signature, sPtr, sigLength);
   name[nameLength-1]='\0';
   signature[sigLength-1]='\0';

   if (modifiers & J9AccStatic)
      offset = 0;
   else
      offset = comp->fej9()->getInstanceFieldOffset((TR_OpaqueClassBlock *)aClazz, name, (uint32_t)(nameLength-1), signature, (uint32_t)(sigLength-1));
}


void TR_VMField::print(TR_FrontEnd *fe, TR::FILE *outFile)
{
   trfprintf(outFile, "name=%s signature=%s modifiers=0x%p offset=%d\n", name, signature, modifiers, offset);
}


TR_VMFieldsInfo::TR_VMFieldsInfo(TR::Compilation * comp, J9Class *aClazz, int buildFields, TR_AllocationKind allocKind)
   : _fe(comp->fej9()),
     _comp(comp),
     _gcDescriptor(allocKind == stackAlloc ? comp->trMemory()->currentStackRegion() : comp->trMemory()->heapMemoryRegion()),
     _fields(NULL),
     _statics(NULL),
     _allocKind(allocKind)
{
   if (buildFields)
      {
      // Weird: We can't seem to just use List with stackAlloc?  I need to use a subclass??
      switch (allocKind)
         {
         case heapAlloc:
            _fields = new (_comp->trMemory(), allocKind) List<TR_VMField> (_comp->trMemory());
            _statics = new (_comp->trMemory(), allocKind) List<TR_VMField> (_comp->trMemory());
            break;
         case stackAlloc:
            _fields = new (_comp->trMemory(), allocKind) TR_ScratchList<TR_VMField> (_comp->trMemory());
            _statics = new (_comp->trMemory(), allocKind) TR_ScratchList<TR_VMField> (_comp->trMemory());
            break;
         default:
            TR_ASSERT_FATAL(false, "Unknown allocation kind %d", allocKind);
         }
      }

   // self
   TR_ASSERT(!(aClazz->romClass->modifiers & J9AccClassArray), "Cannot construct TR_VMFieldsInfo for array class %p", aClazz);
   collectFieldInfo(aClazz);

   //supers
   const int32_t numSupClasses = J9CLASS_DEPTH(aClazz);
   J9Class *supClass = aClazz;
   for (int32_t i = 0; i < numSupClasses-1; i++)
      {
      supClass = (J9Class*)comp->fej9()->getSuperClass((TR_OpaqueClassBlock*)supClass);

      if (comp->compileRelocatableCode())
         {
         if (!supClass)
            comp->failCompilation<J9::AOTNoSupportForAOTFailure>("Found NULL supClass in inheritance chain");
         }
      else
         {
         TR_ASSERT_FATAL(supClass, "Found NULL supClass in inheritance chain");
         }

      collectFieldInfo(supClass);
      }

   // copy the GCData
   UDATA *descriptorPtr = aClazz->instanceDescription;
   UDATA descriptorWord=0;

   int32_t bitIndex = 0;
   if ( ((UDATA) descriptorPtr) & BCT_J9DescriptionImmediate )
      {
      bitIndex++;
      descriptorWord = ((UDATA) descriptorPtr) >> 1;
      }
   else
      {
      descriptorWord = descriptorPtr[0];
      }

   const int32_t numBytesInSlot = TR::Compiler->om.sizeofReferenceField();
   const int32_t numBitsInWord = 8*sizeof(decltype(*(aClazz->instanceDescription)));
   const int32_t numSlotsInObject = (aClazz->totalInstanceSize + numBytesInSlot - 1)/numBytesInSlot;
   const uintptr_t slotsInHeader = (TR::Compiler->om.objectHeaderSizeInBytes()/numBytesInSlot);
   uintptr_t countSlots = slotsInHeader;
   while (1)
      {
      if ( descriptorWord & 0x1 )
         {
         _gcDescriptor.push_back(countSlots);
         }
      countSlots++;
      if (countSlots >= (slotsInHeader + numSlotsInObject))
         {
         break;
         }
      if (bitIndex == (numBitsInWord - 1))
         {
         descriptorPtr++;
         bitIndex = 0;
         descriptorWord = *descriptorPtr;
         }
      else
         {
         descriptorWord = descriptorWord >> 1;
         bitIndex++;
         }
      }
   // null terminated
   _gcDescriptor.push_back(0);
}

void
TR_VMFieldsInfo::collectFieldInfo(J9Class *aClazz)
   {
   J9ROMClass *romCl = aClazz->romClass;
   J9ROMFieldWalkState state;
   J9ROMFieldShape * currentField = romFieldsStartDo(romCl, &state);
   while (currentField)
      {
      buildField(aClazz, currentField);
      currentField = romFieldsNextDo(&state);
      }
   }

void
TR_VMFieldsInfo::buildField(J9Class *aClazz, J9ROMFieldShape *fieldShape)
   {
   TR_VMField *field=NULL;
   if ((fieldShape->modifiers & J9AccStatic) && _statics)
      {
      field = new (_comp->trMemory(), _allocKind) TR_VMField(_comp, aClazz, fieldShape, _allocKind);
      _statics->add(field);
	   }
   else if (_fields)
      {
      field = new (_comp->trMemory(), _allocKind) TR_VMField(_comp, aClazz, fieldShape,  _allocKind);
      _fields->add(field);
      }
   }
