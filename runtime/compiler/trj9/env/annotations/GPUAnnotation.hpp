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

#ifndef GPUANNOTATION_INCL
#define GPUANNOTATION_INCL

#include "compile/Compilation.hpp"


class TR_SharedMemoryField
   {
public:
   TR_SharedMemoryField(char *fieldName, int32_t fieldNameLength,
                       char *fieldSig, int32_t fieldSigLength, int32_t size)
           : _fieldName(fieldName),
             _fieldNameLength(fieldNameLength),  
             _fieldSig(fieldSig),
             _fieldSigLength(fieldSigLength),  
             _size(size),
             _parmNum(-1) {}

   char   *getFieldName() { return _fieldName; }
   int32_t getFieldNameLength() { return _fieldNameLength; }
   char   *getFieldSig() { return _fieldSig; }
   int32_t getFieldSigLength() { return _fieldSigLength; }
   int32_t getSize() { return _size; }
   int32_t getParmNum() { return _parmNum; }
   void    setParmNum(int32_t num) { _parmNum = num; }
   int32_t getOffset() { return _offset; }
   void    setOffset(int32_t offset) { _offset = offset; }

private:
   char *  _fieldName;
   int32_t _fieldNameLength;  
   char *  _fieldSig;
   int32_t _fieldSigLength;  
   int32_t _size;
   int32_t _parmNum;
   int32_t _offset;


friend class TR_SharedMemoryAnnotations;

   };

class TR_SharedMemoryAnnotations 
   {
   public:
   TR_SharedMemoryAnnotations(TR::Compilation *comp) : _sharedMemoryFields(getTypedAllocator<TR_SharedMemoryField>(comp->allocator()))
      {
      loadAnnotations(comp);
      }

   TR_SharedMemoryField find(TR::Compilation *comp, TR::SymbolReference *symRef);
   void setParmNum(TR::Compilation *comp, TR::SymbolReference *symRef, int32_t num);

   TR::list<TR_SharedMemoryField> &getSharedMemoryFields() { return _sharedMemoryFields; }

   private:
   void loadAnnotations(TR::Compilation *comp);
   TR::list<TR_SharedMemoryField> _sharedMemoryFields;
   };


bool currentMethodHasFpreductionAnnotation(TR::Compilation *comp, bool trace);

#endif
