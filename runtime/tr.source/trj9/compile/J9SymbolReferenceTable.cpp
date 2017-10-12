/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "codegen/CodeGenerator.hpp"
#include "env/KnownObjectTable.hpp"        // for KnownObjectTable, etc
#include "compile/AliasBuilder.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"                  // for mcount_t
#include "compile/ResolvedMethod.hpp"          // for TR_ResolvedMethod
#include "compile/SymbolReferenceTable.hpp"
#include "cs2/hashtab.h"                       // for HashTable<>::Cursor, etc
#include "env/CHTable.hpp"
#include "env/CompilerEnv.hpp"
#include "env/PersistentInfo.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"                    // for TR_HeapMemory, etc
#include "env/VMJ9.h"
#include "env/j9method.h"
#include "env/jittypes.h"
#include "il/SymbolReference.hpp"              // for SymbolReference, etc
#include "il/symbol/StaticSymbol.hpp"          // for StaticSymbol, etc
#include "il/symbol/StaticSymbol_inlines.hpp"  // for StaticSymbol, etc
#include "il/symbol/ParameterSymbol.hpp"       // for ParameterSymbol
#include "il/symbol/RegisterMappedSymbol.hpp"  // for RegisterMappedSymbol, etc
#include "il/symbol/ResolvedMethodSymbol.hpp"  // for ResolvedMethodSymbol
#include "ilgen/IlGen.hpp"
#include "ilgen/J9ByteCodeIlGenerator.hpp"
#include "infra/Assert.hpp"                    // for TR_ASSERT
#include "infra/BitVector.hpp"                 // for TR_BitVector, etc
#include "infra/List.hpp"                      // for List, ListIterator, etc
#include "runtime/RuntimeAssumptions.hpp"
#include "trj9/env/PersistentCHTable.hpp"

J9::SymbolReferenceTable::SymbolReferenceTable(size_t sizeHint, TR::Compilation *c) :
   OMR::SymbolReferenceTableConnector(sizeHint, c),
     _immutableInfo(c->trMemory()),
     _immutableSymRefNumbers(c->trMemory(), _numImmutableClasses),
     _dynamicMethodSymrefsByCallSiteIndex(c->trMemory()),
     _unsafeJavaStaticSymRefs(NULL),
     _unsafeJavaStaticVolatileSymRefs(NULL)
   {
   for (uint32_t i = 0; i < _numImmutableClasses; i++)
      _immutableSymRefNumbers[i] = new (trHeapMemory()) TR_BitVector(sizeHint, c->trMemory(), heapAlloc, growable);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateCountForRecompileSymbolRef()
   {
   if (!element(countForRecompileSymbol))
      {
      TR::StaticSymbol * sym = TR::StaticSymbol::create(trHeapMemory(),TR::Int32);
      TR::PersistentInfo *pinfo = comp()->getPersistentInfo();
      sym->setStaticAddress(&(pinfo->_countForRecompile));
      sym->setCountForRecompile();
      sym->setNotDataAddress();
      element(countForRecompileSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), countForRecompileSymbol, sym);
      }
   return element(countForRecompileSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateOSRBufferSymbolRef()
   {
   if (!element(osrBufferSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::RegisterMappedSymbol::createMethodMetaDataSymbol(trHeapMemory(), "OSRBuffer");
      sym->setDataType(TR::Address);
      sym->setNotCollected();
      element(osrBufferSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), osrBufferSymbol, sym);
      element(osrBufferSymbol)->setOffset(fej9->thisThreadGetOSRBufferOffset());

      // We can't let the load/store of the exception symbol swing down
      aliasBuilder.addressStaticSymRefs().set(getNonhelperIndex(osrBufferSymbol)); // add the symRef to the statics list to get correct aliasing info
      }
   return element(osrBufferSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateOSRScratchBufferSymbolRef()
   {
   if (!element(osrScratchBufferSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::RegisterMappedSymbol::createMethodMetaDataSymbol(trHeapMemory(), "OSRScratchBuffer");
      sym->setDataType(TR::Address);
      sym->setNotCollected();
      element(osrScratchBufferSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), osrScratchBufferSymbol, sym);
      element(osrScratchBufferSymbol)->setOffset(fej9->thisThreadGetOSRScratchBufferOffset());

      // We can't let the load/store of the exception symbol swing down
      aliasBuilder.addressStaticSymRefs().set(getNonhelperIndex(osrScratchBufferSymbol)); // add the symRef to the statics list to get correct aliasing info
      }
   return element(osrScratchBufferSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateOSRFrameIndexSymbolRef()
   {
   if (!element(osrFrameIndexSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::RegisterMappedSymbol::createMethodMetaDataSymbol(trHeapMemory(), "osrFrameIndex");
      sym->setDataType(TR::Int32);
      element(osrFrameIndexSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), osrFrameIndexSymbol, sym);
      element(osrFrameIndexSymbol)->setOffset(fej9->thisThreadGetOSRFrameIndexOffset());

      // We can't let the load/store of the exception symbol swing down
      //SD: do we need this?
      aliasBuilder.addressStaticSymRefs().set(getNonhelperIndex(osrFrameIndexSymbol)); // add the symRef to the statics list to get correct aliasing info
      }
   return element(osrFrameIndexSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateAcquireVMAccessSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_acquireVMAccess, true, false, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateThrowCurrentExceptionSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_throwCurrentException, true, false, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateReleaseVMAccessSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_releaseVMAccess, true, false, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateStackOverflowSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_stackOverflow, true, true, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateWriteBarrierStoreSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_writeBarrierStore, false, false, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateWriteBarrierStoreGenerationalSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_writeBarrierStoreGenerational, false, false, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateWriteBarrierStoreGenerationalAndConcurrentMarkSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_writeBarrierStoreGenerationalAndConcurrentMark, false, false, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateWriteBarrierStoreRealTimeGCSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_writeBarrierStoreRealTimeGC, true, true, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateWriteBarrierClassStoreRealTimeGCSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_writeBarrierClassStoreRealTimeGC, true, true, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateWriteBarrierBatchStoreSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_writeBarrierBatchStore, false, false, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateFloatSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex)
   {
   void * dataAddress = owningMethodSymbol->getResolvedMethod()->floatConstant(cpIndex);
   TR::SymbolReference * symRef = findOrCreateCPSymbol(owningMethodSymbol, cpIndex, TR::Float, true, dataAddress);
   symRef->getSymbol()->setConst();
   return symRef;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateDoubleSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex)
   {
   void * dataAddress = owningMethodSymbol->getResolvedMethod()->doubleConstant(cpIndex, trMemory());
   TR::SymbolReference * symRef = findOrCreateCPSymbol(owningMethodSymbol, cpIndex, TR::Double, true, dataAddress);
   symRef->getSymbol()->setConst();
   return symRef;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::createSystemRuntimeHelper(
      TR_RuntimeHelper index,
      bool canGCandReturn,
      bool canGCandExcept,
      bool preservesAllRegisters)
   {
   TR::SymbolReference * symRef = createRuntimeHelper(index,false,false,false);
   if (symRef)
      {
      symRef->getSymbol()->castToMethodSymbol()->setSystemLinkageDispatch();
      symRef->getSymbol()->castToMethodSymbol()->setLinkage(TR_System);
      }
   return symRef;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateStaticMethodSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex)
   {
   bool isUnresolvedInCP;
   TR_ResolvedMethod * method = owningMethodSymbol->getResolvedMethod()->getResolvedStaticMethod(comp(), cpIndex, &isUnresolvedInCP);
   if (method)
      owningMethodSymbol->setMayHaveInlineableCall(true);

   return findOrCreateMethodSymbol(owningMethodSymbol->getResolvedMethodIndex(), cpIndex, method, TR::MethodSymbol::Static, isUnresolvedInCP);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateSpecialMethodSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex)
   {
   bool isUnresolvedInCP;
   TR_ResolvedMethod * method = owningMethodSymbol->getResolvedMethod()->getResolvedSpecialMethod(comp(), cpIndex, &isUnresolvedInCP);
   if (method)
      owningMethodSymbol->setMayHaveInlineableCall(true);

   return findOrCreateMethodSymbol(owningMethodSymbol->getResolvedMethodIndex(), cpIndex, method, TR::MethodSymbol::Special, isUnresolvedInCP);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateVirtualMethodSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex)
   {
   bool isUnresolvedInCP;
   TR_ResolvedMethod * method = owningMethodSymbol->getResolvedMethod()->getResolvedVirtualMethod(comp(), cpIndex, false, &isUnresolvedInCP);
   if (method)
      owningMethodSymbol->setMayHaveInlineableCall(true);

   return findOrCreateMethodSymbol(owningMethodSymbol->getResolvedMethodIndex(), cpIndex, method, TR::MethodSymbol::Virtual, false); //isUnresolvedInCP);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateInterfaceMethodSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex)
   {
   owningMethodSymbol->setMayHaveInlineableCall(true);

   TR::SymbolReference * symRef = findOrCreateMethodSymbol(owningMethodSymbol->getResolvedMethodIndex(), cpIndex, 0, TR::MethodSymbol::Interface);

   // javasoft.sqe.tests.vm.instr.invokeinterface.invokeinterface019.invokeinterface01910m1.invokeinterface01910m1
   // has an invoke interface on a final method in object.
   //
   if (symRef->getSymbol()->castToMethodSymbol()->getMethod()->isFinalInObject())
      {
      comp()->failCompilation<TR::CompilationException>("Method symbol reference is final in object");
      }

   return symRef;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateDynamicMethodSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t callSiteIndex)
   {
   List<TR::SymbolReference> *methods = dynamicMethodSymrefsByCallSiteIndex(callSiteIndex);
   ListIterator<TR::SymbolReference> li(methods);
   for (TR::SymbolReference *symRef = li.getFirst(); symRef; symRef = li.getNext())
      {
      if (symRef->getOwningMethodIndex() == owningMethodSymbol->getResolvedMethodIndex())
         return symRef;
      }

   TR_ResolvedMethod  * method = owningMethodSymbol->getResolvedMethod()->getResolvedDynamicMethod(comp(), callSiteIndex);
   if (method)
      owningMethodSymbol->setMayHaveInlineableCall(true);

   TR::SymbolReference * symRef = findOrCreateMethodSymbol(owningMethodSymbol->getResolvedMethodIndex(), -1, method, TR::MethodSymbol::ComputedVirtual);
   methods->add(symRef);
   return symRef;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateHandleMethodSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex, char *signature)
   {
   TR_ResolvedMethod  * method = owningMethodSymbol->getResolvedMethod()->getResolvedHandleMethodWithSignature(comp(), cpIndex, signature);
   if (method)
      owningMethodSymbol->setMayHaveInlineableCall(true);

   TR::SymbolReference * symRef = findOrCreateMethodSymbol(owningMethodSymbol->getResolvedMethodIndex(), cpIndex, method, TR::MethodSymbol::ComputedVirtual);
   return symRef;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateHandleMethodSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex)
   {
   bool isUnresolvedInCP;
   TR_ResolvedMethod  * method = owningMethodSymbol->getResolvedMethod()->getResolvedHandleMethod(comp(), cpIndex, &isUnresolvedInCP);
   if (method)
      owningMethodSymbol->setMayHaveInlineableCall(true);

   TR::SymbolReference * symRef = findOrCreateMethodSymbol(owningMethodSymbol->getResolvedMethodIndex(), cpIndex, method, TR::MethodSymbol::ComputedVirtual);
   return symRef;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateCallSiteTableEntrySymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t callSiteIndex)
   {
   TR::SymbolReference *symRef;
   TR_SymRefIterator i(aliasBuilder.callSiteTableEntrySymRefs(), self());
   TR_ResolvedMethod *owningMethod = owningMethodSymbol->getResolvedMethod();
   void *entryLocation = owningMethod->callSiteTableEntryAddress(callSiteIndex);
   for (symRef = i.getNext(); symRef; symRef = i.getNext())
      if (  owningMethodSymbol->getResolvedMethodIndex() == symRef->getOwningMethodIndex()
         && symRef->getSymbol()->castToStaticSymbol()->getStaticAddress() == entryLocation)
         {
         return symRef;
         }

   TR::StaticSymbol *sym = TR::StaticSymbol::create(trHeapMemory(),TR::Address);
   sym->makeCallSiteTableEntry(callSiteIndex);
   sym->setStaticAddress(entryLocation);
   bool isUnresolved = owningMethod->isUnresolvedCallSiteTableEntry(callSiteIndex);

   TR::KnownObjectTable::Index knownObjectIndex = TR::KnownObjectTable::UNKNOWN;
   if (!isUnresolved)
      {
      TR::KnownObjectTable *knot = comp()->getOrCreateKnownObjectTable();
      if (knot)
         knownObjectIndex = knot->getIndexAt((uintptrj_t*)entryLocation);
      }

   symRef = new (trHeapMemory()) TR::SymbolReference(self(), sym, owningMethodSymbol->getResolvedMethodIndex(), -1,
                                                    (isUnresolved ? _numUnresolvedSymbols++ : 0), knownObjectIndex);

   if (isUnresolved)
      {
      // Resolving call site table entries causes java code to run
      symRef->setUnresolved();
      symRef->setCanGCandReturn();
      symRef->setCanGCandExcept();
      }

   aliasBuilder.callSiteTableEntrySymRefs().set(symRef->getReferenceNumber());
   return symRef;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateMethodTypeTableEntrySymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex)
   {
   TR::SymbolReference *symRef;
   TR_SymRefIterator i(aliasBuilder.methodTypeTableEntrySymRefs(), self());
   TR_ResolvedMethod *owningMethod = owningMethodSymbol->getResolvedMethod();
   void *entryLocation = owningMethod->methodTypeTableEntryAddress(cpIndex);
   for (symRef = i.getNext(); symRef; symRef = i.getNext())
      if (  owningMethodSymbol->getResolvedMethodIndex() == symRef->getOwningMethodIndex()
         && symRef->getSymbol()->castToStaticSymbol()->getStaticAddress() == entryLocation)
         {
         return symRef;
         }

   TR::StaticSymbol *sym = TR::StaticSymbol::createMethodTypeTableEntry(trHeapMemory(),cpIndex);
   sym->setStaticAddress(entryLocation);
   bool isUnresolved = owningMethod->isUnresolvedMethodTypeTableEntry(cpIndex);
   symRef = new (trHeapMemory()) TR::SymbolReference(self(), sym, owningMethodSymbol->getResolvedMethodIndex(), -1,
                                                    isUnresolved ? _numUnresolvedSymbols++ : 0);

   if (isUnresolved)
      {
      // Resolving method type table entries causes java code to run
      symRef->setUnresolved();
      symRef->setCanGCandReturn();
      symRef->setCanGCandExcept();
      }

   aliasBuilder.methodTypeTableEntrySymRefs().set(symRef->getReferenceNumber());
   return symRef;
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateVarHandleMethodTypeTableEntrySymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex)
   {
   // This function does the same thing as what findOrCreateMethodTypeTableEntrySymbol does except the way it gets the method type table entry address
   TR::SymbolReference *symRef;
   TR_SymRefIterator i(aliasBuilder.methodTypeTableEntrySymRefs(), self());
   TR_ResolvedJ9Method *owningMethod = (TR_ResolvedJ9Method*)(owningMethodSymbol->getResolvedMethod());
   void *entryLocation = owningMethod->varHandleMethodTypeTableEntryAddress(cpIndex);
   for (symRef = i.getNext(); symRef; symRef = i.getNext())
      if (  owningMethodSymbol->getResolvedMethodIndex() == symRef->getOwningMethodIndex()
         && symRef->getSymbol()->castToStaticSymbol()->getStaticAddress() == entryLocation)
         {
         return symRef;
         }

   TR::StaticSymbol *sym = TR::StaticSymbol::createMethodTypeTableEntry(trHeapMemory(),cpIndex);
   sym->setStaticAddress(entryLocation);
   bool isUnresolved = *(j9object_t*)entryLocation == NULL;
   symRef = new (trHeapMemory()) TR::SymbolReference(self(), sym, owningMethodSymbol->getResolvedMethodIndex(), -1,
                                                       isUnresolved ? _numUnresolvedSymbols++ : 0);
   if (isUnresolved)
      {
      // Resolving method type table entries causes java code to run
      symRef->setUnresolved();
      symRef->setCanGCandReturn();
      symRef->setCanGCandExcept();
      }

   aliasBuilder.methodTypeTableEntrySymRefs().set(symRef->getReferenceNumber());
   return symRef;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::methodSymRefWithSignature(TR::SymbolReference *originalSymRef, char *effectiveSignature, int32_t effectiveSignatureLength)
   {
   TR::ResolvedMethodSymbol *originalSymbol = originalSymRef->getSymbol()->castToResolvedMethodSymbol();
   TR_ASSERT(originalSymbol, "methodSymRefWithSignature requires a resolved method symref");
   TR_ASSERT(!originalSymbol->isVirtual(), "methodSymRefFromName doesn't support virtual methods"); // Until we're able to look up vtable index
   int32_t cpIndex = originalSymRef->getCPIndex();

   // Check _methodsBySignature to see if we've already created a symref for this one
   //
   TR_Method *originalMethod = originalSymbol->getMethod();

   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   int32_t fullSignatureLength = originalMethod->classNameLength() + 1 + originalMethod->nameLength() + effectiveSignatureLength;
   char *fullSignature = (char*)trMemory()->allocateMemory(1 + fullSignatureLength, stackAlloc);
   sprintf(fullSignature, "%.*s.%.*s%.*s", originalMethod->classNameLength(), originalMethod->classNameChars(), originalMethod->nameLength(), originalMethod->nameChars(), effectiveSignatureLength, effectiveSignature);
   TR_ASSERT(strlen(fullSignature) == fullSignatureLength, "Computed fullSignatureLength must match actual length of fullSignature");
   CS2::HashIndex hashIndex = 0;
   static char *ignoreMBSCache = feGetEnv("TR_ignoreMBSCache");
   OwningMethodAndString key(originalSymRef->getOwningMethodIndex(), fullSignature);
   if (_methodsBySignature.Locate(key, hashIndex) && !ignoreMBSCache)
      {
      TR::SymbolReference *result = _methodsBySignature[hashIndex];
      if (comp()->getOption(TR_TraceMethodIndex))
         traceMsg(comp(), "-- MBS cache hit (2): M%p\n", result->getSymbol()->getResolvedMethodSymbol()->getResolvedMethod());
      return result;
      }
   else
      {
      // fullSignature will be kept as a key by _methodsBySignature, so it needs heapAlloc
      //
      key = OwningMethodAndString(originalSymRef->getOwningMethodIndex(), self()->strdup(fullSignature));
      if (comp()->getOption(TR_TraceMethodIndex))
         traceMsg(comp(), "-- MBS cache miss (2) owning method #%d, signature %s\n", originalSymRef->getOwningMethodIndex().value(), fullSignature);
      }

   //
   // No existing symref.  Create a new one.
   //

   TR_OpaqueMethodBlock *method = originalSymbol->getResolvedMethod()->getPersistentIdentifier();

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   // Note: we use cpIndex=-1 here so we don't end up getting back originalSymRef (which will not have the signature we want)
   TR::SymbolReference *result = findOrCreateMethodSymbol(originalSymRef->getOwningMethodIndex(), -1,
      fej9->createResolvedMethodWithSignature(comp()->trMemory(), method, NULL, effectiveSignature, effectiveSignatureLength, originalSymbol->getResolvedMethod()->owningMethod()), originalSymbol->getMethodKind());

   result->setCPIndex(cpIndex);
   _methodsBySignature.Add(key, result);
   TR_ASSERT(_methodsBySignature.Locate(key), "After _methodsBySignature.Add, _methodsBySignature.Locate must be true");

   return result;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateTypeCheckArrayStoreSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_typeCheckArrayStore, false, true, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateArrayClassRomPtrSymbolRef()
   {
   if (!element(arrayClassRomPtrSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(), TR::Address);
      element(arrayClassRomPtrSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), arrayClassRomPtrSymbol, sym);
      element(arrayClassRomPtrSymbol)->setOffset(fej9->getOffsetOfArrayClassRomPtrField());
      if (!TR::Compiler->cls.romClassObjectsMayBeCollected())
         sym->setNotCollected();
      }
   return element(arrayClassRomPtrSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateJavaLangReferenceReferentShadowSymbol(
      TR::ResolvedMethodSymbol * owningMethodSymbol,
      bool isResolved,
      TR::DataType type,
      uint32_t offset, bool isUnresolvedInCP)
   {
   TR_ResolvedMethod * owningMethod = owningMethodSymbol->getResolvedMethod();

   TR::SymbolReference * symRef = NULL;
   symRef = findJavaLangReferenceReferentShadowSymbol(owningMethod, TR::Address, offset);
   if (!symRef)
      {
      symRef = createShadowSymbolWithoutCpIndex(owningMethodSymbol, true, TR::Address, offset, false);
      }
   return symRef;
   }


// Right now it only works for private fields or fields that are guaranteed to be accessed only from one class
// because searchRecognizedField only does a name comparison, t does not work if the field is accessed from a subclass of the expected one
TR::SymbolReference *
J9::SymbolReferenceTable::findOrFabricateShadowSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, TR::Symbol::RecognizedField recognizedField, TR::DataType type, uint32_t offset, bool isVolatile, bool isPrivate, bool isFinal, char* name)
   {
   TR_ResolvedMethod * owningMethod = owningMethodSymbol->getResolvedMethod();

   //The following code (up to and including the call to initShadowSymbol) has been adapted from
   //J9::SymbolReferenceTable::findOrCreateShadowSymbol
   TR::SymbolReference * symRef = NULL;
   TR::Symbol * sym = NULL;
   symRef = findShadowSymbol(owningMethod, -1, type, &recognizedField);

   if (symRef)
      return symRef;
   else
      {
      sym = TR::Symbol::createRecognizedShadow(trHeapMemory(),type, recognizedField);
      if (name)
         {
         sym->setNamedShadowSymbol();
         sym->setName(name);
         }

      if (isVolatile)
         sym->setVolatile();
      if (isFinal)
         sym->setFinal();
      if (isPrivate)
         sym->setPrivate();
      static char *dontAliasShadowsToEarlierGIS = feGetEnv("TR_dontAliasShadowsToEarlierGIS");
      if (aliasBuilder.mutableGenericIntShadowHasBeenCreated() && !dontAliasShadowsToEarlierGIS)
         {
         // Some previously-created GIS might actually refer to this shadow
         aliasBuilder.setConservativeGenericIntShadowAliasing(true);
         }

      }
   symRef = new (trHeapMemory()) TR::SymbolReference(self(), sym, owningMethodSymbol->getResolvedMethodIndex(), -1);
   // isResovled = true, isUnresolvedInCP = false
   initShadowSymbol(owningMethod, symRef, true, type, offset, false);
   return symRef;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateShadowSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex, bool isStore)
   {
   TR_ResolvedMethod * owningMethod = owningMethodSymbol->getResolvedMethod();
   bool isVolatile = true, isFinal = false, isPrivate = false, isUnresolvedInCP;
   TR::DataType type = TR::NoType;
   uint32_t offset = 0;
   bool resolved = owningMethod->fieldAttributes(comp(), cpIndex, &offset, &type, &isVolatile, &isFinal, &isPrivate, isStore, &isUnresolvedInCP, true);
   bool sharesSymbol = false;

   TR::Symbol * sym = 0;

   TR::Symbol::RecognizedField recognizedField = TR::Symbol::searchRecognizedField(comp(), owningMethod, cpIndex, false);
   TR::SymbolReference * symRef = findShadowSymbol(owningMethod, cpIndex, type, &recognizedField);
   if (symRef)
      {
      if ((resolved && !symRef->isUnresolved()) ||
          (!resolved && symRef->isUnresolved() && owningMethod == symRef->getOwningMethod(comp())))
         return symRef;
      sym = symRef->getSymbol()->castToShadowSymbol();
      sharesSymbol = true;
      }
   else
      {
      if (recognizedField != TR::Symbol::UnknownField)
         sym = TR::Symbol::createRecognizedShadow(trHeapMemory(),type, recognizedField);
      else
         sym = TR::Symbol::createShadow(trHeapMemory(),type);
      if (isVolatile)
         sym->setVolatile();
      if (isFinal)
         sym->setFinal();
      if (isPrivate)
         sym->setPrivate();
      static char *dontAliasShadowsToEarlierGIS = feGetEnv("TR_dontAliasShadowsToEarlierGIS");
      if (aliasBuilder.mutableGenericIntShadowHasBeenCreated() && !dontAliasShadowsToEarlierGIS)
         {
         // Some previously-created GIS might actually refer to this shadow
         aliasBuilder.setConservativeGenericIntShadowAliasing(true);
         }
      }

   int32_t unresolvedIndex = resolved ? 0 : _numUnresolvedSymbols++;

   if (sharesSymbol)
     symRef->setReallySharesSymbol();

   symRef = new (trHeapMemory()) TR::SymbolReference(self(), sym, owningMethodSymbol->getResolvedMethodIndex(), cpIndex, unresolvedIndex);
   checkUserField(symRef);

   if (sharesSymbol)
     symRef->setReallySharesSymbol();

   initShadowSymbol(owningMethod, symRef, resolved, type, offset, isUnresolvedInCP);

   if (cpIndex > 0)
      aliasBuilder.cpSymRefs().set(symRef->getReferenceNumber());

   return symRef;
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateObjectNewInstanceImplSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol)
   {
   if (!_ObjectNewInstanceImplSymRef)
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)fe();
      TR_ResolvedMethod * resolvedMethod = fej9->getObjectNewInstanceImplMethod(trMemory());
      TR::ResolvedMethodSymbol * sym = TR::ResolvedMethodSymbol::create(trHeapMemory(),resolvedMethod,comp());
      sym->setMethodKind(TR::MethodSymbol::Virtual);

      mcount_t owningMethodIndex = owningMethodSymbol->getResolvedMethodIndex();

      _ObjectNewInstanceImplSymRef = new (trHeapMemory()) TR::SymbolReference(self(), sym, owningMethodIndex, -1, 0);
      _ObjectNewInstanceImplSymRef->setCanGCandReturn();
      _ObjectNewInstanceImplSymRef->setCanGCandExcept();
      _ObjectNewInstanceImplSymRef->setOffset(fej9->getNewInstanceImplVirtualCallOffset());

      aliasBuilder.methodSymRefs().set(_ObjectNewInstanceImplSymRef->getReferenceNumber()); // cmvc defect 193493

      // This is a dummy resolved method - would never be called.  However, we set
      // the count to zero here to make sure that the optimizer does not think that
      // this method is cold and never being called
      //
      resolvedMethod->setInvocationCount(resolvedMethod->getInvocationCount(), 0);
      }
   return _ObjectNewInstanceImplSymRef;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateDLTBlockSymbolRef()
   {
   if (!element(dltBlockSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::RegisterMappedSymbol::createMethodMetaDataSymbol(trHeapMemory(), "DLTBlockMeta");
      sym->setDataType(TR::Address);
      sym->setNotCollected();
      element(dltBlockSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), dltBlockSymbol, sym);
      element(dltBlockSymbol)->setOffset(fej9->thisThreadGetDLTBlockOffset());

      // We can't let the load/store of the exception symbol swing down
      //
      aliasBuilder.addressStaticSymRefs().set(getNonhelperIndex(dltBlockSymbol)); // add the symRef to the statics list to get correct aliasing info
      aliasBuilder.gcSafePointSymRefNumbers().set(getNonhelperIndex(dltBlockSymbol));
      }
   return element(dltBlockSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findDLTBlockSymbolRef()
   {
   return element(dltBlockSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateMultiANewArraySymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_multiANewArray, true, true, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateInstanceShapeSymbolRef()
   {
   if (!element(instanceShapeSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym;
      if (TR::Compiler->target.is64Bit())
         sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int64);
      else
         sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int32);
      element(instanceShapeSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), instanceShapeSymbol, sym);
      element(instanceShapeSymbol)->setOffset(fej9->getOffsetOfInstanceShapeFromClassField());
      }
   return element(instanceShapeSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateInstanceDescriptionSymbolRef()
   {
   if (!element(instanceDescriptionSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym;
      if (TR::Compiler->target.is64Bit())
         sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int64);
      else
         sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int32);
      element(instanceDescriptionSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), instanceDescriptionSymbol, sym);
      element(instanceDescriptionSymbol)->setOffset(fej9->getOffsetOfInstanceDescriptionFromClassField());
      }
   return element(instanceDescriptionSymbol);
   }



TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateDescriptionWordFromPtrSymbolRef()
   {
   if (!element(descriptionWordFromPtrSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym;
      if (TR::Compiler->target.is64Bit())
         sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int64);
      else
         sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int32);
      element(descriptionWordFromPtrSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), descriptionWordFromPtrSymbol, sym);
      element(descriptionWordFromPtrSymbol)->setOffset(fej9->getOffsetOfDescriptionWordFromPtrField());
      }
   return element(descriptionWordFromPtrSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateClassFlagsSymbolRef()
   {
   if (!element(isClassFlagsSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int32);
      element(isClassFlagsSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), isClassFlagsSymbol, sym);
      element(isClassFlagsSymbol)->setOffset(fej9->getOffsetOfClassFlags());
      }
   return element(isClassFlagsSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateClassAndDepthFlagsSymbolRef()
   {
   if (!element(isClassAndDepthFlagsSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int32);
      element(isClassAndDepthFlagsSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), isClassAndDepthFlagsSymbol, sym);
      element(isClassAndDepthFlagsSymbol)->setOffset(fej9->getOffsetOfClassAndDepthFlags());
      }
   return element(isClassAndDepthFlagsSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateArrayComponentTypeAsPrimitiveSymbolRef()
   {
   if (!element(componentClassAsPrimitiveSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym;
      if (TR::Compiler->target.is64Bit())
         sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int64);
      else
         sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int32);

      element(componentClassAsPrimitiveSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), componentClassAsPrimitiveSymbol, sym);
      element(componentClassAsPrimitiveSymbol)->setOffset(fej9->getOffsetOfArrayComponentTypeField());
      if (!TR::Compiler->cls.classObjectsMayBeCollected())
         sym->setNotCollected();
      }
   return element(componentClassAsPrimitiveSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateMethodTypeCheckSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_methodTypeCheck, false, true, true);
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateIncompatibleReceiverSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_incompatibleReceiver, false, true, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateReportStaticMethodEnterSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_reportStaticMethodEnter, true, false, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateReportMethodExitSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_reportMethodExit, true, false, true);
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateHeaderFlagsSymbolRef()
   {
   if (!element(headerFlagsSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::SymbolReference * symRef;
      // object header flags now occupy 4bytes on 64-bit
      symRef = new (trHeapMemory()) TR::SymbolReference(self(), headerFlagsSymbol, TR::Symbol::createShadow(trHeapMemory(),TR::Int32));
      symRef->setOffset(TR::Compiler->om.offsetOfHeaderFlags());
      element(headerFlagsSymbol) = symRef;
      aliasBuilder.intShadowSymRefs().set(symRef->getReferenceNumber());
      }

   return element(headerFlagsSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateDiscontiguousArraySizeSymbolRef()
   {
   if (!element(discontiguousArraySizeSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym;

      // Size field is 32-bits on ALL header shapes.
      //
      sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int32);
      element(discontiguousArraySizeSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), discontiguousArraySizeSymbol, sym);
      element(discontiguousArraySizeSymbol)->setOffset(fej9->getOffsetOfDiscontiguousArraySizeField());
      }
   return element(discontiguousArraySizeSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateClassLoaderSymbolRef(TR_ResolvedMethod * method)
   {
   ListIterator<TR::SymbolReference> i(&_classLoaderSymbolRefs);
   TR::SymbolReference * symRef;
   for (symRef = i.getFirst(); symRef; symRef = i.getNext())
      if (symRef->getOwningMethod(comp()) == method)
         return symRef;

   TR::StaticSymbol * sym = TR::StaticSymbol::create(trHeapMemory(),TR::Address);
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   sym->setStaticAddress(fej9->getLocationOfClassLoaderObjectPointer(method->classOfMethod()));
   mcount_t index = comp()->getOwningMethodSymbol(method)->getResolvedMethodIndex();
   symRef = new (trHeapMemory()) TR::SymbolReference(self(), sym, index, -1);

   aliasBuilder.addressStaticSymRefs().set(symRef->getReferenceNumber()); // add the symRef to the statics list to get correct aliasing info

   _classLoaderSymbolRefs.add(symRef);

   return symRef;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateCurrentThreadSymbolRef()
   {
   if (!element(currentThreadSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::RegisterMappedSymbol::createMethodMetaDataSymbol(trHeapMemory(), "CurrentThread");
      sym->setDataType(TR::Address);
      sym->setImmutableField();
      element(currentThreadSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), currentThreadSymbol, sym);
      element(currentThreadSymbol)->setOffset(fej9->thisThreadGetCurrentThreadOffset());
      }
   return element(currentThreadSymbol);
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateJ9MethodConstantPoolFieldSymbolRef(intptrj_t offset)
   {
   if (!element(j9methodConstantPoolSymbol))
      {
      TR::Symbol * sym;
      if (TR::Compiler->target.is64Bit())
         sym = TR::Symbol::createShadow(trHeapMemory(),TR::Int64);
      else
         sym = TR::Symbol::createShadow(trHeapMemory(),TR::Int32);

      element(j9methodConstantPoolSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), j9methodConstantPoolSymbol, sym);
      element(j9methodConstantPoolSymbol)->setOffset(offset);
      }
   TR::SymbolReference *result = element(j9methodConstantPoolSymbol);
   TR_ASSERT(result->getOffset() == offset, "findOrCreateJ9MethodConstantPoolFieldSymbolRef must use the right offset!  %d != %d", result->getOffset(), offset);
   return result;
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateJ9MethodExtraFieldSymbolRef(intptrj_t offset)
   {
   if (!element(j9methodExtraFieldSymbol))
      {
      TR::Symbol * sym;
      if (TR::Compiler->target.is64Bit())
         sym = TR::Symbol::createShadow(trHeapMemory(),TR::Int64);
      else
         sym = TR::Symbol::createShadow(trHeapMemory(),TR::Int32);

      element(j9methodExtraFieldSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), j9methodExtraFieldSymbol, sym);
      element(j9methodExtraFieldSymbol)->setOffset(offset);
      }
   TR::SymbolReference *result = element(j9methodExtraFieldSymbol);
   TR_ASSERT(result->getOffset() == offset, "findOrCreateJ9MethodExtraFieldSymbolRef must use the right offset!  %d != %d", result->getOffset(), offset);
   return result;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateStartPCLinkageInfoSymbolRef(intptrj_t offset)
   {
   if (!element(startPCLinkageInfoSymbol))
      {
      TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(),TR::Int32);
      element(startPCLinkageInfoSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), startPCLinkageInfoSymbol, sym);
      element(startPCLinkageInfoSymbol)->setOffset(offset);
      }
   TR::SymbolReference *result = element(startPCLinkageInfoSymbol);
   TR_ASSERT(result->getOffset() == offset, "findOrCreateStartPCLinkageInfoSymbolRef must use the right offset!  %d != %d", result->getOffset(), offset);
   return result;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreatePerCodeCacheHelperSymbolRef(TR_CCPreLoadedCode helper, uintptrj_t helperAddr)
   {
   CommonNonhelperSymbol index = (CommonNonhelperSymbol)(firstPerCodeCacheHelperSymbol + helper);
   if (!element(index))
      {
      TR::MethodSymbol * methodSymbol = TR::MethodSymbol::create(trHeapMemory(),TR_Private);
      methodSymbol->setHelper();
      // The address of the helper depends on the current codecache, which can change during compilation, so we don't have a single method address
      methodSymbol->setMethodAddress(NULL);
      element(index) = new (trHeapMemory()) TR::SymbolReference(self(), index, methodSymbol);
      }
   return element(index);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateANewArraySymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_aNewArray, true, true, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateStringSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex)
   {
   TR_ResolvedMethod * owningMethod = owningMethodSymbol->getResolvedMethod();
   void * stringConst = owningMethod->stringConstant(cpIndex);
   TR::SymbolReference * symRef;
   if (owningMethod->isUnresolvedString(cpIndex))
      {
      symRef = findOrCreateCPSymbol(owningMethodSymbol, cpIndex, TR::Address, false, 0);
      symRef->setOffset((uintptrj_t)stringConst);
      }
   else
      {
      symRef = findOrCreateCPSymbol(owningMethodSymbol, cpIndex, TR::Address, true, stringConst);
      }
   TR::StaticSymbol * sym = (TR::StaticSymbol *)symRef->getSymbol();
   sym->setConstString();
   return symRef;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateMethodMonitorEntrySymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_methodMonitorEntry, true, false, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateMethodMonitorExitSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_methodMonitorExit, true, false, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateCompiledMethodSymbolRef()
   {
   if (!element(compiledMethodSymbol))
      {
      TR::StaticSymbol * sym = TR::StaticSymbol::create(trHeapMemory(),TR::Address);
      sym->setStaticAddress(comp()->getCurrentMethod()->getPersistentIdentifier());
      sym->setCompiledMethod();
      sym->setNotDataAddress();
      element(compiledMethodSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), compiledMethodSymbol, sym);
      }
   return element(compiledMethodSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateMethodTypeSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex)
   {
   TR_ResolvedMethod * owningMethod = owningMethodSymbol->getResolvedMethod();
   void * methodTypeConst = owningMethod->methodTypeConstant(cpIndex);
   TR::SymbolReference * symRef;
   if (owningMethod->isUnresolvedMethodType(cpIndex))
      {
      symRef = findOrCreateCPSymbol(owningMethodSymbol, cpIndex, TR::Address, false, 0);
      symRef->setOffset((uintptrj_t)methodTypeConst);
      }
   else
      {
      symRef = findOrCreateCPSymbol(owningMethodSymbol, cpIndex, TR::Address, true, methodTypeConst);
      }
   TR::StaticSymbol * sym = (TR::StaticSymbol *)symRef->getSymbol();
   sym->setConstMethodType();
   return symRef;
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateMethodHandleSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex)
   {
   TR_ResolvedMethod * owningMethod = owningMethodSymbol->getResolvedMethod();
   void * methodHandleConst = owningMethod->methodHandleConstant(cpIndex);
   TR::SymbolReference * symRef;
   if (owningMethod->isUnresolvedMethodHandle(cpIndex))
      {
      symRef = findOrCreateCPSymbol(owningMethodSymbol, cpIndex, TR::Address, false, 0);
      symRef->setOffset((uintptrj_t)methodHandleConst);
      }
   else
      {
      symRef = findOrCreateCPSymbol(owningMethodSymbol, cpIndex, TR::Address, true, methodHandleConst);
      }
   TR::StaticSymbol * sym = (TR::StaticSymbol *)symRef->getSymbol();
   sym->setConstMethodHandle();
   return symRef;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateClassStaticsSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex)
   {
   TR_ResolvedMethod * owningMethod = owningMethodSymbol->getResolvedMethod();

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   void * classStatics = fej9->addressOfFirstClassStatic(owningMethod->classOfStatic(cpIndex, true));

   ListIterator<TR::SymbolReference> i(&_classStaticsSymbolRefs);
   TR::SymbolReference * symRef;
   for (symRef = i.getFirst(); symRef; symRef = i.getNext())
      if (symRef->getSymbol()->getStaticSymbol()->getStaticAddress() == classStatics)
         return symRef;

   TR::StaticSymbol * sym = TR::StaticSymbol::create(trHeapMemory(),TR::Address);
   sym->setStaticAddress(classStatics);
   if (!TR::Compiler->cls.classObjectsMayBeCollected())
      sym->setNotCollected();

   symRef = new (trHeapMemory()) TR::SymbolReference(self(), sym, owningMethodSymbol->getResolvedMethodIndex(), -1);

   aliasBuilder.addressStaticSymRefs().set(symRef->getReferenceNumber()); // add the symRef to the statics list to get correct aliasing info

   _classStaticsSymbolRefs.add(symRef);

   return symRef;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateClassFromJavaLangClassAsPrimitiveSymbolRef()
   {
   if (!element(classFromJavaLangClassAsPrimitiveSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym;
      if (TR::Compiler->target.is64Bit())
         sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int64);
      else
         sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int32);

      element(classFromJavaLangClassAsPrimitiveSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), classFromJavaLangClassAsPrimitiveSymbol, sym);
      element(classFromJavaLangClassAsPrimitiveSymbol)->setOffset(fej9->getOffsetOfClassFromJavaLangClassField());
      }
   return element(classFromJavaLangClassAsPrimitiveSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findClassFromJavaLangClassAsPrimitiveSymbolRef()
   {
   return element(classFromJavaLangClassAsPrimitiveSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::createShadowSymbolWithoutCpIndex(TR::ResolvedMethodSymbol * owningMethodSymbol, bool isResolved,
                                                          TR::DataType type, uint32_t offset, bool isUnresolvedInCP)
   {
   TR_ResolvedMethod * owningMethod = owningMethodSymbol->getResolvedMethod();

   TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(),type);
   TR::SymbolReference * symRef;

   int32_t unresolvedIndex = isResolved ? 0 : _numUnresolvedSymbols++;
   symRef = new (trHeapMemory()) TR::SymbolReference(self(), sym, owningMethodSymbol->getResolvedMethodIndex(), -1);
   initShadowSymbol(owningMethod, symRef, isResolved, type, offset, isUnresolvedInCP);
   return symRef;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findJavaLangReferenceReferentShadowSymbol(TR_ResolvedMethod * owningMethod, TR::DataType type, uint32_t offset)
   {
   TR::SymbolReference * symRef;
   TR_SymRefIterator i(type == TR::Address ? aliasBuilder.addressShadowSymRefs() :
                                            (type == TR::Int32 ? aliasBuilder.intShadowSymRefs() : aliasBuilder.nonIntPrimitiveShadowSymRefs()), self());
   while (symRef = i.getNext())
      if (symRef->getSymbol()->getDataType() == type &&
          symRef->getOffset() == offset &&
          symRef->getOwningMethod(comp()) == owningMethod)
         return symRef;
   return 0;
   }


void
J9::SymbolReferenceTable::initShadowSymbol(TR_ResolvedMethod * owningMethod, TR::SymbolReference *symRef, bool isResolved,
                                          TR::DataType type, uint32_t offset, bool isUnresolvedInCP)
   {
   if (isResolved)
      {
      symRef->setOffset(offset);
      }
   else
      {
      symRef->setUnresolved();
      symRef->setCanGCandExcept();
      aliasBuilder.unresolvedShadowSymRefs().set(symRef->getReferenceNumber());
      }

   symRef->setHasBeenAccessedAtRuntime(isUnresolvedInCP? TR_no : TR_maybe);

   if (type == TR::Address)
      aliasBuilder.addressShadowSymRefs().set(symRef->getReferenceNumber());
   else if (type == TR::Int32)
      aliasBuilder.intShadowSymRefs().set(symRef->getReferenceNumber());
   else
      aliasBuilder.nonIntPrimitiveShadowSymRefs().set(symRef->getReferenceNumber());

   if (shouldMarkBlockAsCold(owningMethod, isUnresolvedInCP))
      markBlockAsCold();

   return;
   }


List<TR::SymbolReference> *
J9::SymbolReferenceTable::dynamicMethodSymrefsByCallSiteIndex(int32_t index)
   {
   if (!_dynamicMethodSymrefsByCallSiteIndex[index])
      _dynamicMethodSymrefsByCallSiteIndex[index] = new (trHeapMemory()) List<TR::SymbolReference>(comp()->trMemory());
   return _dynamicMethodSymrefsByCallSiteIndex[index];
   }


bool
J9::SymbolReferenceTable::isFieldClassObject(TR::SymbolReference *symRef)
   {
   int32_t len;
   const char *fieldName = symRef->getOwningMethod(comp())->fieldSignatureChars(symRef->getCPIndex(), len);
   dumpOptDetails(comp(), "got fieldsig as %s\n", fieldName);
   return false;
   }


static bool parmSlotCameFromExpandingAnArchetypeArgPlaceholder(int32_t slot, TR::ResolvedMethodSymbol *sym, TR_Memory *mem)
   {
   TR_ResolvedMethod *meth = sym->getResolvedMethod();
   if (meth->convertToMethod()->isArchetypeSpecimen())
      return slot >= meth->archetypeArgPlaceholderSlot(mem);
   else
      return false;
   }


void
J9::SymbolReferenceTable::addParameters(TR::ResolvedMethodSymbol * methodSymbol)
   {
   mcount_t index = methodSymbol->getResolvedMethodIndex();
   methodSymbol->setParameterList();
   ListIterator<TR::ParameterSymbol> parms(&methodSymbol->getParameterList());
   for (TR::ParameterSymbol * p = parms.getFirst(); p; p = parms.getNext())
      {
      TR::SymbolReference *symRef = new (trHeapMemory()) TR::SymbolReference(self(), p, index, p->getSlot());
      methodSymbol->setParmSymRef(p->getSlot(), symRef);
      if (!parmSlotCameFromExpandingAnArchetypeArgPlaceholder(p->getSlot(), methodSymbol, trMemory()))
         methodSymbol->getAutoSymRefs(p->getSlot()).add(symRef);
      }
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateStaticSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex, bool isStore)
   {
   TR_ResolvedMethod * owningMethod = owningMethodSymbol->getResolvedMethod();
   void * dataAddress;
   TR::DataType type = TR::NoType;
   bool isVolatile, isFinal, isPrivate, isUnresolvedInCP;
   bool resolved = owningMethod->staticAttributes(comp(), cpIndex, &dataAddress, &type, &isVolatile, &isFinal, &isPrivate, isStore, &isUnresolvedInCP);

   if (  isUnresolvedInCP && (type != TR::Address && _compilation->cg()->getAccessStaticsIndirectly()))
      resolved = false;

   bool sharesSymbol = false;

   TR::StaticSymbol * sym = 0;
   TR::SymbolReference * symRef = findStaticSymbol(owningMethod, cpIndex, type);
   if (symRef)
      {
      if ((resolved && !symRef->isUnresolved()) ||
          (!resolved && symRef->isUnresolved() && owningMethod == symRef->getOwningMethod(comp())))
         {
         symRef->setHasBeenAccessedAtRuntime(isUnresolvedInCP ? TR_no: TR_maybe);
         if (shouldMarkBlockAsCold(owningMethod, isUnresolvedInCP))
            markBlockAsCold();

         return symRef;
         }

      sym = symRef->getSymbol()->castToStaticSymbol();
      sharesSymbol = true;
      }
   else
      {
      TR::Symbol::RecognizedField recognizedField = TR::Symbol::searchRecognizedField(comp(), owningMethod, cpIndex, true);
      if (recognizedField != TR::Symbol::UnknownField)
         sym = TR::StaticSymbol::createRecognized(trHeapMemory(), type, recognizedField);
      else
         sym = TR::StaticSymbol::create(trHeapMemory(), type);

      if (isVolatile)
         sym->setVolatile();
      if (isFinal)
         sym->setFinal();
      if (isPrivate)
         sym->setPrivate();
      }

   int32_t unresolvedIndex = resolved ? 0 : _numUnresolvedSymbols++;

   if (sharesSymbol)
      symRef->setReallySharesSymbol();

   symRef = new (trHeapMemory()) TR::SymbolReference(self(), sym, owningMethodSymbol->getResolvedMethodIndex(), cpIndex, unresolvedIndex);
   checkUserField(symRef);

   if (sharesSymbol)
      symRef->setReallySharesSymbol();

   if (resolved)
      {
      sym->setStaticAddress(dataAddress);
      if (type != TR::Address && _compilation->cg()->getAccessStaticsIndirectly() && !_compilation->compileRelocatableCode())
         {
         TR_OpaqueClassBlock * feClass = owningMethod->classOfStatic(cpIndex, true);
         TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
         symRef->setOffset((char *)dataAddress - (char *)fej9->addressOfFirstClassStatic(feClass));
         }
      }
   else
      {
      symRef->setUnresolved();
      symRef->setCanGCandReturn();
      symRef->setCanGCandExcept();
      }

   symRef->setHasBeenAccessedAtRuntime(isUnresolvedInCP? TR_no : TR_maybe);

   if (type == TR::Address)
      aliasBuilder.addressStaticSymRefs().set(symRef->getReferenceNumber());
   else if (type == TR::Int32)
      aliasBuilder.intStaticSymRefs().set(symRef->getReferenceNumber());
   else
      aliasBuilder.nonIntPrimitiveStaticSymRefs().set(symRef->getReferenceNumber());

   if (shouldMarkBlockAsCold(owningMethod, isUnresolvedInCP))
      markBlockAsCold();

   return symRef;
   }


static struct N { const char * name; } pnames[] =
   {
   "java/",
   "javax/",
   "com/ibm/",
   "com/sun/"
   };


void
J9::SymbolReferenceTable::checkUserField(TR::SymbolReference *symRef)
   {
   static const char *userField = feGetEnv("TR_UserField");
   if (!userField)
      {
      // In the absense of further analysis, treat everything as user fields
      setHasUserField(true);
      return;
      }

   if ((!symRef->getSymbol()->isShadow() && (!symRef->getSymbol()->isStatic() || symRef->getSymbol()->isMethod() || symRef->getSymbol()->isConstObjectRef())) || symRef->getCPIndex() <= 0)
      return;

   int32_t length;
   char * name = symRef->getOwningMethod(comp())->classNameOfFieldOrStatic(symRef->getCPIndex(), length);

   if (name == NULL || length == 0)
      return;

   TR_ASSERT(sizeof(pnames)/sizeof(char *) == _numNonUserFieldClasses,"Size of package names array is not correct\n");
   int32_t i;
   bool isNonUserField = false;
   for (i = 0; i < _numNonUserFieldClasses; i++)
      {
      if (!strncmp(pnames[i].name, name, strlen(pnames[i].name)))
         {
         isNonUserField = true;
         //printf ("User field symref %d name=%s  length=%d cpindex=%d\n", symRef->getReferenceNumber(), name, length, symRef->getCPIndex());
         break;
         }
      }

   if (isNonUserField)
      {
      // At the moment this is conservative. i.e. any field in any of the above packages will not be considered
      // a user field in any of the other packages. However it is possible to have fields from one package (or class) that we
      // may want to consider as a user field in another package (class) in which case having the different array elements
      // below would then be the way to acomplish this. The population of the array would need to be more selective.
      //
      }
   else
      {
      setHasUserField(true);
      for (i = 0; i < _numNonUserFieldClasses; i++)
         aliasBuilder.userFieldSymRefNumbers()[i]->set(symRef->getReferenceNumber());
      }
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateThreadLowTenureAddressSymbolRef()
   {
   if (!element(lowTenureAddressSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::RegisterMappedSymbol::createMethodMetaDataSymbol(trHeapMemory(), "lowTenureAddress");
      sym->setDataType(TR::Address);
      sym->setNotCollected();
      element(lowTenureAddressSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), lowTenureAddressSymbol, sym);
      element(lowTenureAddressSymbol)->setOffset(fej9->getThreadLowTenureAddressPointerOffset());
      }
   return element(lowTenureAddressSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateThreadHighTenureAddressSymbolRef()
   {
   if (!element(highTenureAddressSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::RegisterMappedSymbol::createMethodMetaDataSymbol(trHeapMemory(), "highTenureAddress");
      sym->setDataType(TR::Address);
      sym->setNotCollected();
      element(highTenureAddressSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), highTenureAddressSymbol, sym);
      element(highTenureAddressSymbol)->setOffset(fej9->getThreadHighTenureAddressPointerOffset());
      }
   return element(highTenureAddressSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateRamStaticsFromClassSymbolRef()
   {
   if (!element(ramStaticsFromClassSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(), TR::Address);
      element(ramStaticsFromClassSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), ramStaticsFromClassSymbol, sym);
      element(ramStaticsFromClassSymbol)->setOffset(fej9->getOffsetOfRamStaticsFromClassField());
      sym->setNotCollected();
      }
   return element(ramStaticsFromClassSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateJavaLangClassFromClassSymbolRef()
   {
   if (!element(javaLangClassFromClassSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(), TR::Address);
      element(javaLangClassFromClassSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), javaLangClassFromClassSymbol, sym);
      element(javaLangClassFromClassSymbol)->setOffset(fej9->getOffsetOfJavaLangClassFromClassField());
      }
   return element(javaLangClassFromClassSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateClassFromJavaLangClassSymbolRef()
   {
   if (!element(classFromJavaLangClassSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(), TR::Address);
      element(classFromJavaLangClassSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), classFromJavaLangClassSymbol, sym);
      element(classFromJavaLangClassSymbol)->setOffset(fej9->getOffsetOfClassFromJavaLangClassField());
      sym->setNotCollected();
      }
   return element(classFromJavaLangClassSymbol);
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findInitializeStatusFromClassSymbolRef()
   {
   return element(initializeStatusFromClassSymbol);
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateInitializeStatusFromClassSymbolRef()
   {
   if (!element(initializeStatusFromClassSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = NULL;
      if (TR::Compiler->target.is64Bit())
         sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int64);
      else
         sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int32);
      element(initializeStatusFromClassSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), initializeStatusFromClassSymbol, sym);
      element(initializeStatusFromClassSymbol)->setOffset(fej9->getOffsetOfInitializeStatusFromClassField());
      }
   return element(initializeStatusFromClassSymbol);
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateClassRomPtrSymbolRef()
   {
   if (!element(classRomPtrSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(), TR::Address);
      element(classRomPtrSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), classRomPtrSymbol, sym);
      element(classRomPtrSymbol)->setOffset(fej9->getOffsetOfClassRomPtrField());
      if (!TR::Compiler->cls.romClassObjectsMayBeCollected())
         sym->setNotCollected();
      }
   return element(classRomPtrSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateGlobalFragmentSymbolRef()
   {
  if (!element(globalFragmentSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym;
      if (TR::Compiler->target.is64Bit())
         sym = TR::Symbol::createShadow(trHeapMemory(),TR::Int64);
      else
         sym = TR::Symbol::createShadow(trHeapMemory(),TR::Int32);
      sym->setGlobalFragmentShadowSymbol();
      element(globalFragmentSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), globalFragmentSymbol, sym);
      element(globalFragmentSymbol)->setOffset(fej9->getRememberedSetGlobalFragmentOffset());
      }
   return element(globalFragmentSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateFragmentParentSymbolRef()
   {
   if (!element(fragmentParentSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::RegisterMappedSymbol::createMethodMetaDataSymbol(trHeapMemory(), "FragmentParent");
      sym->setDataType(TR::Address);
      sym->setNotCollected();
      element(fragmentParentSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), fragmentParentSymbol, sym);
      element(fragmentParentSymbol)->setOffset(fej9->thisThreadRememberedSetFragmentOffset() + fej9->getFragmentParentOffset());
      }
   return element(fragmentParentSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateThreadDebugEventData(int32_t index)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   intptrj_t offset = fej9->getThreadDebugEventDataOffset(index);
   ListIterator<TR::SymbolReference> li(&_currentThreadDebugEventDataSymbolRefs);
   TR::SymbolReference * symRef;
   for (symRef = li.getFirst(); symRef; symRef = li.getNext())
      if (symRef->getOffset() == offset)
         return symRef;

   // Not found; create
   if (!_currentThreadDebugEventDataSymbol)
      {
      _currentThreadDebugEventDataSymbol = TR::RegisterMappedSymbol::createMethodMetaDataSymbol(trHeapMemory(), "debugEventData");
      _currentThreadDebugEventDataSymbol->setDataType(TR::Address);
      _currentThreadDebugEventDataSymbol->setNotCollected();
      }
   TR::SymbolReference *result = new (trHeapMemory()) TR::SymbolReference(self(), _currentThreadDebugEventDataSymbol, offset);
   _currentThreadDebugEventDataSymbolRefs.add(result);
   return result;
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findUnsafeSymbolRef(TR::DataType type, bool javaObjectReference, bool javaStaticReference, bool isVolatile)
   {
   TR_Array<TR::SymbolReference *> * unsafeSymRefs = NULL;

   if (isVolatile)
      {
      unsafeSymRefs =
            javaStaticReference ?
                  _unsafeJavaStaticVolatileSymRefs :
                  _unsafeVolatileSymRefs;
      }
   else
      {
      unsafeSymRefs =
            javaStaticReference ?
                  _unsafeJavaStaticSymRefs :
                  _unsafeSymRefs;
      }

   TR::SymbolReference * symRef = NULL;

   if (unsafeSymRefs != NULL)
      {
      symRef = (*unsafeSymRefs)[type];
      }

   return symRef;
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateUnsafeSymbolRef(TR::DataType type, bool javaObjectReference, bool javaStaticReference, bool isVolatile)
   {
   TR_Array<TR::SymbolReference *> * unsafeSymRefs = NULL;

   if (isVolatile)
      {
      if (javaStaticReference)
         {
         if (_unsafeJavaStaticVolatileSymRefs == NULL)
            _unsafeJavaStaticVolatileSymRefs = new (trHeapMemory()) TR_Array<TR::SymbolReference *>(comp()->trMemory(), TR::NumTypes);
         unsafeSymRefs = _unsafeJavaStaticVolatileSymRefs;
         }
      else
         {
         if (_unsafeVolatileSymRefs == NULL)
            _unsafeVolatileSymRefs = new (trHeapMemory()) TR_Array<TR::SymbolReference *>(comp()->trMemory(), TR::NumTypes);
         unsafeSymRefs = _unsafeVolatileSymRefs;
         }
      }
   else
      {
      if (javaStaticReference)
         {
         if (_unsafeJavaStaticSymRefs == NULL)
            _unsafeJavaStaticSymRefs = new (trHeapMemory()) TR_Array<TR::SymbolReference *>(comp()->trMemory(), TR::NumTypes);
         unsafeSymRefs = _unsafeJavaStaticSymRefs;
         }
      else
         {
         if (_unsafeSymRefs == NULL)
            _unsafeSymRefs = new (trHeapMemory()) TR_Array<TR::SymbolReference *>(comp()->trMemory(), TR::NumTypes);
         unsafeSymRefs = _unsafeSymRefs;
         }
      }

   TR::SymbolReference * symRef = (*unsafeSymRefs)[type];

   if (symRef == NULL)
      {
      TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(),type);
      sym->setUnsafeShadowSymbol();
      sym->setArrayShadowSymbol();
      if (isVolatile)
         sym->setVolatile();
      (*unsafeSymRefs)[type] = symRef = new (trHeapMemory()) TR::SymbolReference(self(), sym, comp()->getMethodSymbol()->getResolvedMethodIndex(), -1);
      aliasBuilder.unsafeSymRefNumbers().set(symRef->getReferenceNumber());
      }

   // For unsafe symbols created by a call that contained an object and an offset we'll alias
   // the symbol against all shadow and statics. If the Unsafe symbol was created using only an
   // address then we won't alias against other Java objects.
   //
   if (javaObjectReference)
      comp()->setHasUnsafeSymbol();
   else
      symRef->setReallySharesSymbol();

   return symRef;
   }


void
J9::SymbolReferenceTable::performClassLookahead(TR_PersistentClassInfo *classInfo, TR_ResolvedMethod *method)
   {
   // Do not perform class lookahead when peeking (including recursive class lookahead)
   //
   ((TR_J9ByteCodeIlGenerator *)(comp()->getCurrentIlGenerator()))->performClassLookahead(classInfo);
   }


int32_t
J9::SymbolReferenceTable::userFieldMethodId(TR::MethodSymbol *methodSymbol)
   {
   static const char *userField = feGetEnv("TR_UserField");
   if (userField)
      {
      TR::RecognizedMethod method = methodSymbol->getRecognizedMethod();
      if (method == TR::java_util_HashMap_rehash)
         return nonUser_java_util_HashMap_rehash - 1;
      else if (method == TR::java_util_HashMap_analyzeMap)
         return nonUser_java_util_HashMap_analyzeMap - 1;
      else if (method == TR::java_util_HashMap_calculateCapacity)
         return nonUser_java_util_HashMap_calculateCapacity - 1;
      else if (method == TR::java_util_HashMap_findNullKeyEntry)
         return nonUser_java_util_HashMap_findNullKeyEntry - 1;
      }
   return -1;
   }


int32_t
J9::SymbolReferenceTable::immutableConstructorId(TR::MethodSymbol *methodSymbol)
   {
   TR::RecognizedMethod method = methodSymbol->getRecognizedMethod();
   switch (method)
      {
      case TR::java_lang_String_init_String:
      case TR::java_lang_String_init_String_char:
      case TR::java_lang_String_init_int_int_char_boolean:
         // All String constructors share the same ID
         method = TR::java_lang_String_init;
         break;
      default:
         break;
      }

   if (TR::java_lang_Boolean_init <= method && method <= TR::java_lang_String_init)
      return method - TR::java_lang_Boolean_init;
   else
      return -1;
   }


bool
J9::SymbolReferenceTable::isImmutable(TR::SymbolReference *symRef)
   {
   if (!_hasImmutable)
      return false;

   int32_t i;
   for (i = 0; i < _numImmutableClasses; i++)
      {
      if (_immutableSymRefNumbers[i]->get(symRef->getReferenceNumber()))
         return true;
      }

   ListElement<TR_ImmutableInfo> *immutableClassInfoElem = _immutableInfo.getListHead();
   while (immutableClassInfoElem)
      {
      TR_ImmutableInfo *immutableClassInfo = immutableClassInfoElem->getData();
      if (immutableClassInfo->_immutableSymRefNumbers->get(symRef->getReferenceNumber()))
         return true;
      immutableClassInfoElem = immutableClassInfoElem->getNextElement();
      }

   return false;
   }


TR_ImmutableInfo *
J9::SymbolReferenceTable::findOrCreateImmutableInfo(TR_OpaqueClassBlock *clazz)
   {
   ListElement<TR_ImmutableInfo> *immutableClassInfoElem = _immutableInfo.getListHead();
   while (immutableClassInfoElem)
      {
      TR_ImmutableInfo *immutableClassInfo = immutableClassInfoElem->getData();
      if (immutableClassInfo->_clazz == clazz)
         return immutableClassInfo;
      immutableClassInfoElem = immutableClassInfoElem->getNextElement();
      }

   TR_ImmutableInfo *newImmutableInfo = new (trHeapMemory()) TR_ImmutableInfo(clazz,
                                                             new (trHeapMemory()) TR_BitVector(_size_hint, comp()->trMemory(), heapAlloc, growable),
                                                             NULL);
   _immutableInfo.add(newImmutableInfo);
   return newImmutableInfo;
   }


void
J9::SymbolReferenceTable::checkImmutable(TR::SymbolReference *symRef)
   {
   if (!symRef->getSymbol()->isShadow() || symRef->getCPIndex() < 0)
      return;

   int32_t length;
   char * name = symRef->getOwningMethod(comp())->classNameOfFieldOrStatic(symRef->getCPIndex(), length);

   if (name == NULL || length == 0)
      return;

   static struct N { const char * name; } names[] =
      {
      "java/lang/Boolean",
      "java/lang/Character",
      "java/lang/Byte",
      "java/lang/Short",
      "java/lang/Integer",
      "java/lang/Long",
      "java/lang/Float",
      "java/lang/Double",
      "java/lang/String"
      };

   TR_ASSERT(sizeof(names)/sizeof(char *) == _numImmutableClasses,"Size of names array is not correct\n");
   int32_t i;
   for (i = 0; i < _numImmutableClasses; i++)
      {
      if (strcmp(names[i].name, name) == 0)
         {
         _hasImmutable = true;
         _immutableSymRefNumbers[i]->set(symRef->getReferenceNumber());
         break;
         }
      }

   if (true)
      {
      if (!symRef->getSymbol()->isArrayShadowSymbol())
         {
         TR::Symbol *sym = symRef->getSymbol();
         if (sym->isPrivate() || sym->isFinal())
            {
            int32_t len;
            char *classNameOfFieldOrStatic = symRef->getOwningMethod(comp())->classNameOfFieldOrStatic(symRef->getCPIndex(), len);
            TR_OpaqueClassBlock * classOfStatic = comp()->fe()->getClassFromSignature(classNameOfFieldOrStatic, len, symRef->getOwningMethod(comp()));

            bool isClassInitialized = false;
            TR_PersistentClassInfo * classInfo =
            comp()->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(classOfStatic, comp());
            if (classInfo && classInfo->isInitialized())
               isClassInitialized = true;

            if ((classOfStatic != comp()->getSystemClassPointer()) &&
                isClassInitialized && TR::Compiler->cls.isClassFinal(comp(), classOfStatic) &&
                !comp()->getOption(TR_AOT))
               {
               if (!classInfo->getFieldInfo() &&
                    (comp()->getMethodHotness() >= hot))
                  {
                  performClassLookahead(classInfo, symRef->getOwningMethod(comp()));
                  }

               if (classInfo->getFieldInfo())
                  {
                  TR_PersistentFieldInfo * fieldInfo = classInfo->getFieldInfo()->find(comp(), sym, symRef);
                  if (fieldInfo && fieldInfo->isImmutable())
                     {
                     setHasImmutable(true);
                     TR_ImmutableInfo *clazzImmutableInfo = findOrCreateImmutableInfo(classOfStatic);
                     clazzImmutableInfo->_immutableSymRefNumbers->set(symRef->getReferenceNumber());
                     }
                  }
               }
            }
         }
      }
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateImmutableGenericIntShadowSymbolReference(intptrj_t offset)
   {
   static char *disableImmutableIntShadows = feGetEnv("TR_disableImmutableIntShadows");
   if (disableImmutableIntShadows)
      return findOrCreateGenericIntShadowSymbolReference(offset);
   TR::SymbolReference * symRef = new (trHeapMemory()) TR::SymbolReference(self(), findOrCreateGenericIntShadowSymbol(), comp()->getMethodSymbol()->getResolvedMethodIndex(), -1);
   symRef->setOffset(offset);
   return symRef;
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateCheckCastForArrayStoreSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_checkCastForArrayStore, false, true, true);
   }



TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateProfilingBufferCursorSymbolRef()
   {
   if (!element(profilingBufferCursorSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::RegisterMappedSymbol::createMethodMetaDataSymbol(trHeapMemory(), "ProfilingBufferCursor");
      sym->setDataType(TR::Address);
      element(profilingBufferCursorSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), profilingBufferCursorSymbol, sym);
      element(profilingBufferCursorSymbol)->setOffset(fej9->thisThreadGetProfilingBufferCursorOffset());

      // We can't let the load/store of the exception symbol swing down
      aliasBuilder.addressStaticSymRefs().set(getNonhelperIndex(profilingBufferCursorSymbol)); // add the symRef to the statics list to get correct aliasing info
      }
   return element(profilingBufferCursorSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateProfilingBufferEndSymbolRef()
   {
   if (!element(profilingBufferEndSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::RegisterMappedSymbol::createMethodMetaDataSymbol(trHeapMemory(),"profilingBufferEnd");
      sym->setDataType(TR::Address);
      element(profilingBufferEndSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), profilingBufferEndSymbol, sym);
      element(profilingBufferEndSymbol)->setOffset(fej9->thisThreadGetProfilingBufferEndOffset());

      // We can't let the load/store of the exception symbol swing down
      aliasBuilder.addressStaticSymRefs().set(getNonhelperIndex(profilingBufferEndSymbol)); // add the symRef to the statics list to get correct aliasing info
      }
   return element(profilingBufferEndSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateProfilingBufferSymbolRef(intptrj_t offset)
   {
   if (!element(profilingBufferSymbol))
      {
      TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory());
      sym->setDataType(TR::Int32);
      TR::SymbolReference *symRef = new (trHeapMemory()) TR::SymbolReference(self(), profilingBufferSymbol, sym);
      element(profilingBufferSymbol) = symRef;
      element(profilingBufferSymbol)->setOffset(offset);
      }
   return element(profilingBufferSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findShadowSymbol(TR_ResolvedMethod * owningMethod, int32_t cpIndex, TR::DataType type, TR::Symbol::RecognizedField *recognizedField)
   {
   TR::SymbolReference * symRef;
   TR_SymRefIterator i(type == TR::Address ? aliasBuilder.addressShadowSymRefs() :
                                            (type == TR::Int32 ? aliasBuilder.intShadowSymRefs() : aliasBuilder.nonIntPrimitiveShadowSymRefs()), self());
   while (symRef = i.getNext())
      {
      if ((recognizedField &&
          *recognizedField != TR::Symbol::UnknownField &&
          *recognizedField == symRef->getSymbol()->getRecognizedField())||
          (symRef->getSymbol()->getDataType() == type &&
          symRef->getCPIndex() != -1 &&
          cpIndex != -1 &&
          TR::Compiler->cls.jitFieldsAreSame(comp(), owningMethod, cpIndex,
             symRef->getOwningMethod(comp()), symRef->getCPIndex(), symRef->getSymbol()->isStatic())))
         {
         if (cpIndex != -1 && owningMethod->classOfMethod() != symRef->getOwningMethod(comp())->classOfMethod())
            {
            bool isVolatile = true, isFinal = false, isPrivate = false, isUnresolvedInCP ;
            TR::DataType type = TR::DataTypes::NoType;
            uint32_t offset = 0;
            // isStore is a hardcoded false here since we only really want to set hasBeenAccessed correctly
            bool resolved = owningMethod->fieldAttributes(comp(), cpIndex, &offset, &type, &isVolatile, &isFinal, &isPrivate, false, &isUnresolvedInCP, true);
            symRef->setHasBeenAccessedAtRuntime(isUnresolvedInCP ? TR_no : TR_maybe);
            }
         return symRef;
         }
      }
   return 0;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateClassIsArraySymbolRef()
   {
   if (!element(isArraySymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int32);
      element(isArraySymbol) = new (trHeapMemory()) TR::SymbolReference(self(), isArraySymbol, sym);
      element(isArraySymbol)->setOffset(fej9->getOffsetOfIsArrayFieldFromRomClass());
      }
   return element(isArraySymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateArrayComponentTypeSymbolRef()
   {
   if (!element(componentClassSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(), TR::Address);
      element(componentClassSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), componentClassSymbol, sym);
      element(componentClassSymbol)->setOffset(fej9->getOffsetOfArrayComponentTypeField());
      if (!TR::Compiler->cls.classObjectsMayBeCollected())
         sym->setNotCollected();
      }
   return element(componentClassSymbol);
   }
