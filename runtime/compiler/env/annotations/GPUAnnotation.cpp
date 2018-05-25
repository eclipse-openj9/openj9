/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#include "env/annotations/GPUAnnotation.hpp"
#include "il/Symbol.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "il/SymbolReference.hpp"
#include "env/j9method.h"
#include "cfreader.h"
#include "env/VMJ9.h"

static char *getNameFromCP(int32_t &len, I_32 cpIndex, J9ROMClass *romClass)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");

   J9ROMConstantPoolItem * constantPool = J9_ROM_CP_FROM_ROM_CLASS(romClass);
   J9UTF8 * name = J9ROMCLASSREF_NAME((J9ROMClassRef *)(&constantPool[cpIndex]));
   len = J9UTF8_LENGTH(name);
   return utf8Data(name);
   }

static U_32 getIntFromCP(I_32 cpIndex, J9ROMClass *romClass)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");

   J9ROMConstantPoolItem * constantPool = J9_ROM_CP_FROM_ROM_CLASS(romClass);
   return *((U_32 *) & constantPool[cpIndex]);
   }


void TR_SharedMemoryAnnotations::loadAnnotations(TR::Compilation *comp) 
   {
   if (!comp->isGPUCompilation()) return;

   J9Class * clazz = (J9Class *)comp->getCurrentMethod()->containingClass();
   J9Class **supClasses = clazz->superclasses;
   int numSupClasses = J9CLASS_DEPTH(clazz);
   traceMsg(comp, "looking for annotations\n");

   for (int i = 0; i <= numSupClasses; i++)
      {
      J9ROMClass *romCl = clazz->romClass;

      if (i != numSupClasses)
         {
         J9Class *supClass = supClasses[i];
         romCl = supClass->romClass;
         } 

      J9UTF8 *utf8 = J9ROMCLASS_CLASSNAME(romCl);
      char *className = (char *) J9UTF8_DATA(utf8);
      int32_t classNameLength = J9UTF8_LENGTH(utf8);
      traceMsg(comp, "class %.*s\n", classNameLength, className);

      J9ROMFieldWalkState state;
      J9ROMFieldShape * currentField;

      TR_ASSERT(!(romCl->modifiers & J9AccClassArray), "Cannot get fields for array class %p", clazz);
      currentField = romFieldsStartDo(romCl, &state);

      for ( ; currentField; currentField = romFieldsNextDo(&state))
         {
         utf8 = J9ROMFIELDSHAPE_NAME(currentField);
         char *fieldName = (char *) J9UTF8_DATA(utf8);
         int32_t fieldNameLength = J9UTF8_LENGTH(utf8);

         utf8 =  J9ROMFIELDSHAPE_SIGNATURE(currentField);
         char *fieldSig = (char *) J9UTF8_DATA(utf8);
         int32_t fieldSigLength = J9UTF8_LENGTH(utf8);

         traceMsg(comp, "   field %.*s %.*s\n", fieldNameLength, fieldName, fieldSigLength, fieldSig);
         int32_t size = 0;  // non-shared field

         U_32 * annotationsData = getFieldAnnotationsDataFromROMField(currentField);
         U_16 numAnnotations = 0;
         U_8 *data = NULL;
         if (annotationsData)
            {
            data = (U_8 *)(annotationsData + 1);
	        NEXT_U16(numAnnotations, data);
            }

         for (int i= 0; i < numAnnotations; i++)
            {
            U_16 typeIndex;
            NEXT_U16(typeIndex, data);

            int32_t len;
            char * str = getNameFromCP(len, typeIndex, romCl);
            traceMsg(comp, "      annotation %.*s\n",  len, str);

            if (len != 33 ||
                strncmp(str, "Lcom/ibm/gpu/Kernel$SharedMemory;", len) != 0)
                continue;

            U_16 numElementValuePairs;
            NEXT_U16(numElementValuePairs, data);
            for (int i = 0; i < numElementValuePairs; i++)
               {
               U_16 elementNameIndex;
               NEXT_U16(elementNameIndex, data);
            
               int32_t len;
               char * str = getNameFromCP(len, elementNameIndex, romCl);
               U_8 tag;
               NEXT_U8(tag, data);
               U_16 constValueIndex;
               NEXT_U16(constValueIndex, data);
               size = getIntFromCP(constValueIndex, romCl) + 4;  // TODO: header !!!
               }
            }
          
         traceMsg(comp, "       size = %d\n", size);              
         _sharedMemoryFields.push_back(TR_SharedMemoryField(fieldName, fieldNameLength, fieldSig, fieldSigLength, size));
         }
      }
   }

void extractFieldName(TR::Compilation *comp, TR::SymbolReference *symRef,
                      int32_t &classNameLength, char * &className,
                      int32_t &fieldNameLength, char * &fieldName,
                      int32_t &fieldSigLength, char * &fieldSig)
   {
   int32_t cpIndex = symRef->getCPIndex();
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");

   J9ROMFieldRef *romCPBase = ((TR_ResolvedJ9Method *)symRef->getOwningMethod(comp))->romCPBase();

   J9ROMFieldRef * ref = (J9ROMFieldRef *) (&romCPBase[cpIndex]);
   J9ROMNameAndSignature * nameAndSignature = J9ROMFIELDREF_NAMEANDSIGNATURE(ref);
   J9UTF8 * declName = J9ROMCLASSREF_NAME((J9ROMClassRef *) (&romCPBase[ref->classRefCPIndex]));

   classNameLength = J9UTF8_LENGTH(declName);
   className = utf8Data(declName);
   fieldNameLength = J9UTF8_LENGTH(J9ROMNAMEANDSIGNATURE_NAME(nameAndSignature));
   fieldName = utf8Data(J9ROMNAMEANDSIGNATURE_NAME(nameAndSignature));
   fieldSigLength = J9UTF8_LENGTH(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSignature));
   fieldSig = utf8Data(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSignature));
   }


TR_SharedMemoryField TR_SharedMemoryAnnotations::find(TR::Compilation *comp, TR::SymbolReference *symRef) 
   {
   int32_t classNameLength, fieldNameLength, fieldSigLength;
   char *className, *fieldName, *fieldSig;

   extractFieldName(comp, symRef, classNameLength, className,
                                  fieldNameLength, fieldName,
                                  fieldSigLength, fieldSig);

   for(auto lit = _sharedMemoryFields.begin(); lit != _sharedMemoryFields.end(); ++lit)
      {
      if ((*lit)._fieldNameLength == fieldNameLength &&  // TODO: check class !!!!
          strncmp((*lit)._fieldName, fieldName, fieldNameLength) == 0 &&
          (*lit)._fieldSigLength == fieldSigLength &&
          strncmp((*lit)._fieldSig, fieldSig, fieldSigLength) == 0)
          return *lit;
      }

   return TR_SharedMemoryField(NULL, 0, NULL, 0, -1);
   }

// TODO: use another data structure to speedup the search
void TR_SharedMemoryAnnotations::setParmNum(TR::Compilation *comp, TR::SymbolReference *symRef, int32_t num) 
   {
   int32_t classNameLength, fieldNameLength, fieldSigLength;
   char *className, *fieldName, *fieldSig;

   extractFieldName(comp, symRef, classNameLength, className,
                                  fieldNameLength, fieldName,
                                  fieldSigLength, fieldSig);

   for(auto lit = _sharedMemoryFields.begin(); lit != _sharedMemoryFields.end(); ++lit)
      {
      if ((*lit)._fieldNameLength == fieldNameLength &&  // TODO: check class !!!!
          strncmp((*lit)._fieldName, fieldName, fieldNameLength) == 0 &&
          (*lit)._fieldSigLength == fieldSigLength &&
          strncmp((*lit)._fieldSig, fieldSig, fieldSigLength) == 0)
         {
    	 (*lit).setParmNum(num);
         return;
         }
      }

   TR_ASSERT(false, "could not find field\n");

   }


bool currentMethodHasFpreductionAnnotation(TR::Compilation *comp, bool trace)
   {
    J9ROMMethod * romMethod = (J9ROMMethod *)J9_ROM_METHOD_FROM_RAM_METHOD((J9Method *)comp->getCurrentMethod()->getPersistentIdentifier());

    U_32 * annotationsData = getMethodAnnotationsDataFromROMMethod(romMethod);

    J9Class * clazz = (J9Class *)comp->getCurrentMethod()->containingClass();
    J9ROMClass *romCl = clazz->romClass;

         U_16 numAnnotations = 0;
         U_8 *data = NULL;
         if (annotationsData)
            {
            data = (U_8 *)(annotationsData + 1);
	        NEXT_U16(numAnnotations, data);
            }

         if (trace) traceMsg(comp, "current method has %d annotations %p\n", numAnnotations, annotationsData); 

         for (int i= 0; i < numAnnotations; i++)
            {
            U_16 typeIndex;
            NEXT_U16(typeIndex, data);

            int32_t len;
            char * str = getNameFromCP(len, typeIndex, romCl);

            if (trace) traceMsg(comp, "found annotation %.*s\n",  len, str);

            if (len == 44
                && strncmp(str, "Lorg/apache/spark/sql/execution/fpreduction;", len) == 0)
               {
               if (trace) traceMsg(comp, "current method has @fpreduction annotation\n"); 
               return true;
               }
            }

    return false;
   }
