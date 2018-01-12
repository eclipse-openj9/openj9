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

#ifndef j9fieldsInfo_h
#define j9fieldsInfo_h

#include "j9field.h"
#include "infra/List.hpp"
#include "env/IO.hpp"
#include "env/VMJ9.h"

class TR_VMFieldsInfo
   {
public:
   TR_ALLOC(TR_Memory::VMFieldsInfo)

   TR_VMFieldsInfo( TR::Compilation *, J9Class *aClazz, int buildFields, TR_AllocationKind alloc=heapAlloc);

   List<TR_VMField>*   getFields() { return _fields; }
   List<TR_VMField>*   getStatics() { return _statics; }
   UDATA               getNumRefSlotsInObject() { return _numRefSlotsInObject; }
   int32_t *           getGCDescriptor() { return _gcDescriptor; }
   void                print(TR::FILE *outFile);

private:
   int                 buildField(J9Class *aClazz, J9ROMFieldShape *fieldShape);
   TR_J9VMBase *       _fe;
   TR::Compilation *   _comp;
   J9Class *          _ramClass;
   int32_t *          _gcDescriptor;
   UDATA              _numRefSlotsInObject;
   List <TR_VMField>* _fields;
   List <TR_VMField>* _statics;
   TR_AllocationKind  _allocKind;
   };
#endif

