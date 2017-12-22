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

#ifndef J9_KNOWN_OBJECT_TABLE_INCL
#define J9_KNOWN_OBJECT_TABLE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_KNOWN_OBJECT_TABLE_CONNECTOR
#define J9_KNOWN_OBJECT_TABLE_CONNECTOR
namespace J9 { class KnownObjectTable; }
namespace J9 { typedef J9::KnownObjectTable KnownObjectTableConnector; }
#endif

#include "env/OMRKnownObjectTable.hpp"
#include "infra/Annotations.hpp"
#include "infra/Array.hpp"
#include "infra/BitVector.hpp"
namespace J9 { class Compilation; }
namespace TR { class Compilation; }
class TR_J9VMBase;
namespace J9 { class ObjectModel; }
class TR_DebugExt;
class TR_VMFieldsInfo;
class TR_BitVector;

namespace J9
{

class OMR_EXTENSIBLE KnownObjectTable : public OMR::KnownObjectTableConnector
   {
   friend class ::TR_J9VMBase;
   friend class Compilation;
   friend class ::TR_DebugExt;
   TR_Array<uintptrj_t*> _references;

public:
   TR_ALLOC(TR_Memory::FrontEnd);

   KnownObjectTable(TR::Compilation *comp);

   virtual Index getEndIndex();
   virtual Index getIndex(uintptrj_t objectPointer);
   virtual uintptrj_t *getPointerLocation(Index index);
   virtual bool isNull(Index index);

   virtual void dumpTo(TR::FILE *file, TR::Compilation *comp);

private:

   void dumpObjectTo(TR::FILE *file, Index i, const char *fieldName, const char *sep,  TR::Compilation *comp, TR_BitVector &visited, TR_VMFieldsInfo **fieldsInfoByIndex, int32_t depth);
   };

}

#endif
