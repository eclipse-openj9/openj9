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

#ifndef IPA_h
#define IPA_h

#include <stdint.h>
#include "env/TRMemory.hpp"
#include "il/Node.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"

class TR_ClassExtendCheck;
class TR_ClassLoadCheck;
class TR_FrontEnd;
class TR_OpaqueClassBlock;
class TR_OpaqueMethodBlock;
class TR_ResolvedMethod;
namespace OMR { class RuntimeAssumption; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class SymbolReference; }
namespace TR { class SymbolReferenceTable; }
namespace TR { class Compilation; }

namespace TR {

class GlobalSymbol : public TR_Link<TR::GlobalSymbol>
   {
public:
   GlobalSymbol(TR::SymbolReference *symRef) : _symRef(symRef) { }
   TR::SymbolReference *_symRef;
   };

struct PriorPeekInfo
   {
   TR_ResolvedMethod *_method;
   TR_LinkHead<TR_ClassLoadCheck> _classesThatShouldNotBeLoaded;
   TR_LinkHead<TR_ClassExtendCheck> _classesThatShouldNotBeNewlyExtended; 
   TR_LinkHead<TR::GlobalSymbol> _globalsWritten;
   };

class InterProceduralAnalyzer 
   {
public:
   TR_ALLOC(TR_Memory::InterProceduralAnalyzer)

   InterProceduralAnalyzer(TR::Compilation *, bool trace);

   int32_t perform();

   TR_FrontEnd * fe() { return _fe; }
   TR::Compilation * comp() { return _compilation; }

   TR_Memory *               trMemory()                    { return _trMemory; }
   TR_StackMemory            trStackMemory()               { return _trMemory; }
   TR_HeapMemory             trHeapMemory()                { return _trMemory; }

   List<OMR::RuntimeAssumption> *analyzeCall(TR::Node *);
   List<OMR::RuntimeAssumption> *analyzeCallGraph(TR::Node *, bool *);
   List<OMR::RuntimeAssumption> *analyzeMethod(TR::Node *, TR_ResolvedMethod *, bool *);
     
   bool isOnPeekingStack(TR_ResolvedMethod *method);
   bool capableOfPeekingVirtualCalls();
   bool trace() { return _trace; }
     
   bool addClassThatShouldNotBeLoaded(char *name, int32_t len);
   bool addClassThatShouldNotBeNewlyExtended(TR_OpaqueClassBlock *clazz);
   bool addSingleClassThatShouldNotBeNewlyExtended(TR_OpaqueClassBlock *clazz);
   bool addMethodThatShouldNotBeNewlyOverridden(TR_OpaqueMethodBlock *method);
   bool addWrittenGlobal(TR::SymbolReference *symRef);
   uint32_t hash(void * h, uint32_t size);

   virtual bool alreadyPeekedMethod(TR_ResolvedMethod *method, bool *success, TR::PriorPeekInfo **);
   virtual bool analyzeNode(TR::Node *node, vcount_t visitCount, bool *success) = 0;

protected:
   //
   // data
   //
   int32_t _sniffDepth, _maxSniffDepth;
   int32_t _totalPeekedBytecodeSize;
   int32_t _maxPeekedBytecodeSize;
   bool _maxSniffDepthExceeded;
   bool _trace;
   TR_OpaqueClassBlock *_classPointer;
   TR::Compilation *_compilation;
   TR_Memory * _trMemory;
   TR::SymbolReferenceTable *_symRefTab;
   TR::SymbolReferenceTable *_currentPeekingSymRefTab;
   TR_FrontEnd *_fe;
   TR::ResolvedMethodSymbol *_currentMethodSymbol;
   List<TR::PriorPeekInfo> _successfullyPeekedMethods;
   List<TR_ResolvedMethod> _unsuccessfullyPeekedMethods;
   TR_ScratchList<TR_ClassLoadCheck> _classesThatShouldNotBeLoadedInCurrentPeek;
   TR_ScratchList<TR_ClassExtendCheck> _classesThatShouldNotBeNewlyExtendedInCurrentPeek;
   TR_ScratchList<TR_ClassExtendCheck> * _classesThatShouldNotBeNewlyExtendedInCurrentPeekHT;
   TR_ScratchList<TR::GlobalSymbol> _globalsWrittenInCurrentPeek;
   ListElement<TR_ClassLoadCheck> *_prevClc;
   ListElement<TR_ClassExtendCheck> *_prevCec;
   ListElement<TR::GlobalSymbol> *_prevSymRef;

public:
   TR_LinkHead<TR_ClassLoadCheck> _classesThatShouldNotBeLoaded;
   TR_LinkHead<TR_ClassExtendCheck> _classesThatShouldNotBeNewlyExtended; 
   TR_LinkHead<TR_ClassExtendCheck> *_classesThatShouldNotBeNewlyExtendedHT;
   TR_LinkHead<TR::GlobalSymbol> _globalsWritten; 
   };

}

#endif
