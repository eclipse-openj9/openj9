/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
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
#if defined(J9VM_OPT_JITSERVER)
#include <string>
#include <tuple>
#include <vector>
#endif /* defined(J9VM_OPT_JITSERVER) */

namespace J9 { class Compilation; }
namespace TR { class Compilation; }
class TR_J9VMBase;
namespace J9 { class ObjectModel; }
class TR_VMFieldsInfo;
class TR_BitVector;

#if defined(J9VM_OPT_JITSERVER)
struct
TR_KnownObjectTableDumpInfoStruct
   {
   uintptr_t  *ref;
   uintptr_t   objectPointer;
   int32_t     hashCode;

   TR_KnownObjectTableDumpInfoStruct(uintptr_t *objRef, uintptr_t objPtr, int32_t code) :
      ref(objRef),
      objectPointer(objPtr),
      hashCode(code) {}
   };

// <TR_KnownObjectTableDumpInfoStruct, std::string classNameStr>
using TR_KnownObjectTableDumpInfo = std::tuple<TR_KnownObjectTableDumpInfoStruct, std::string>;
#endif /* defined(J9VM_OPT_JITSERVER) */


namespace J9
{

class OMR_EXTENSIBLE KnownObjectTable : public OMR::KnownObjectTableConnector
   {
   friend class ::TR_J9VMBase;
   friend class Compilation;
   TR_Array<uintptr_t*> _references;
   TR_Array<int32_t> _stableArrayRanks;


public:
   TR_ALLOC(TR_Memory::FrontEnd);

   // Note that the KnownObjectTable is often initialized inside the J9::Compilation
   // constructor of the TR::Compilation that gets passed in here. For that reason, the
   // TR::Compilation *comp() can only be used in the methods of this class if you know
   // the relevant components of the compilation have have been initialized or otherwise
   // properly set up by the time those methods will be called. This affects
   // getExistingIndexAt and getOrCreateIndexAt in particular.
   KnownObjectTable(TR::Compilation *comp);

   TR::KnownObjectTable *self();

   Index getEndIndex();
   Index getOrCreateIndex(uintptr_t objectPointer);
   Index getOrCreateIndex(uintptr_t objectPointer, bool isArrayWithConstantElements);
   uintptr_t *getPointerLocation(Index index);
   bool isNull(Index index);

   void dumpTo(TR::FILE *file, TR::Compilation *comp);

   Index getOrCreateIndexAt(uintptr_t *objectReferenceLocation);
   Index getOrCreateIndexAt(uintptr_t *objectReferenceLocation, bool isArrayWithConstantElements);
   Index getExistingIndexAt(uintptr_t *objectReferenceLocation);

   uintptr_t getPointer(Index index);

#if defined(J9VM_OPT_JITSERVER)
   void updateKnownObjectTableAtServer(Index index, uintptr_t *objectReferenceLocationClient);
   void getKnownObjectTableDumpInfo(std::vector<TR_KnownObjectTableDumpInfo> &knotDumpInfoList);
#endif /* defined(J9VM_OPT_JITSERVER) */

   void addStableArray(Index index, int32_t stableArrayRank);
   bool isArrayWithStableElements(Index index);
   int32_t getArrayWithStableElementsRank(Index index);

private:

   void dumpObjectTo(TR::FILE *file, Index i, const char *fieldName, const char *sep,  TR::Compilation *comp, TR_BitVector &visited, TR_VMFieldsInfo **fieldsInfoByIndex, int32_t depth);
   };

}

#endif
