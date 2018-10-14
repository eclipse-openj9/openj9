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

#include "compile/AliasBuilder.hpp"
#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "il/SymbolReference.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ILOpCodes.hpp"
#include "infra/BitVector.hpp"


J9::AliasBuilder::AliasBuilder(TR::SymbolReferenceTable *symRefTab, size_t sizeHint, TR::Compilation *c) :
   OMR::AliasBuilderConnector(symRefTab, sizeHint, c),
      _userFieldSymRefNumbers(c->trMemory(), _numNonUserFieldClasses),
      _tenantDataMetaSymRefs(sizeHint, c->trMemory(), heapAlloc, growable),
      _callSiteTableEntrySymRefs(sizeHint, c->trMemory(), heapAlloc, growable),
      _unresolvedShadowSymRefs(sizeHint, c->trMemory(), heapAlloc, growable),
      _immutableConstructorDefAliases(c->trMemory(), _numImmutableClasses),
      _methodTypeTableEntrySymRefs(sizeHint, c->trMemory(), heapAlloc, growable)
   {
   for (int32_t i = 0; i < _numNonUserFieldClasses; i++)
      _userFieldSymRefNumbers[i] = new (trHeapMemory()) TR_BitVector(sizeHint, c->trMemory(), heapAlloc, growable);
   }


TR_BitVector *
J9::AliasBuilder::methodAliases(TR::SymbolReference *symRef)
   {
   static bool newImmutableAlias = feGetEnv("TR_noNewImmutableAlias") == NULL;
   static bool newUserFieldAlias = feGetEnv("TR_noNewUserFieldAlias") == NULL;

   if (symRef->isUnresolved() || !newImmutableAlias || !symRefTab()->hasImmutable() || !newUserFieldAlias || !symRefTab()->hasUserField())
      {
      if (comp()->getOption(TR_TraceAliases))
         traceMsg(comp(), "For method sym %d default aliases\n", symRef->getReferenceNumber());
      return &defaultMethodDefAliases();
      }

   TR::ResolvedMethodSymbol *method = symRef->getSymbol()->castToResolvedMethodSymbol();

   while (true)
      {
      int32_t id = symRefTab()->immutableConstructorId(method);
      if (id >= 0)
         return _immutableConstructorDefAliases[id];

      id = symRefTab()->userFieldMethodId(method);
      if (id >= 0)
         {
         if (comp()->getOption(TR_TraceAliases))
            {
            traceMsg(comp(), "For method sym %d aliases\n", symRef->getReferenceNumber());
            _userFieldMethodDefAliases[id]->print(comp());
            traceMsg(comp(), "\n");
            }
         return _userFieldMethodDefAliases[id];
         }

      if (symRef->isInitMethod())
         {
         TR_BitVector *result          = NULL;
         TR_BitVector *allocatedResult = NULL;
         for (TR_OpaqueClassBlock *clazz = method->getResolvedMethod()->containingClass(); clazz; clazz = comp()->fe()->getSuperClass(clazz))
            {
            int32_t clazzNameLen;
            char   *clazzName = TR::Compiler->cls.classNameChars(comp(), clazz, clazzNameLen);

            ListElement<TR_ImmutableInfo> *immutableClassInfoElem = symRefTab()->immutableInfo().getListHead();
            while (immutableClassInfoElem)
               {
               TR_ImmutableInfo *immutableClassInfo = immutableClassInfoElem->getData();
               int32_t immutableClassNameLen;
               char *immutableClassName = TR::Compiler->cls.classNameChars(comp(), immutableClassInfo->_clazz, immutableClassNameLen);
               if ((immutableClassNameLen == clazzNameLen) &&
                   !strncmp(immutableClassName, clazzName, clazzNameLen))
                  {
                  TR_BitVector *symrefsToInclude = immutableClassInfo->_immutableConstructorDefAliases;
                  if (comp()->getOption(TR_TraceAliases))
                     {
                     traceMsg(comp(), "Method sym %d includes aliases for %.*s.<init>\n", symRef->getReferenceNumber(), clazzNameLen, clazzName);
                     symrefsToInclude->print(comp());
                     traceMsg(comp(), "\n");
                     }
                  if (allocatedResult)
                     *allocatedResult |= *symrefsToInclude;
                  else if (result)
                     {
                     allocatedResult = new (comp()->trHeapMemory()) TR_BitVector(symRefTab()->getNumSymRefs(), comp()->trMemory(), heapAlloc, growable);
                     *allocatedResult = *result;
                     *allocatedResult |= *symrefsToInclude;
                     result = allocatedResult;
                     }
                  else
                     result = symrefsToInclude;
                  }
               immutableClassInfoElem = immutableClassInfoElem->getNextElement();
               }
            }
         if (result)
            return result;
         }

      method = symRef->getOwningMethodSymbol(_compilation);
      mcount_t method_index = ((TR::ResolvedMethodSymbol *)method)->getResolvedMethodIndex();
      if (method_index == JITTED_METHOD_INDEX)
         break;

      // get symbol reference for the owning method
      symRef = _compilation->getResolvedMethodSymbolReference(method_index);
      if (!symRef)
         break;
      }

   if (comp()->getOption(TR_TraceAliases))
      {
      traceMsg(comp(), "For method sym %d default aliases without immutable\n", symRef->getReferenceNumber());
      defaultMethodDefAliasesWithoutImmutable().print(comp());
      traceMsg(comp(), "\n");
      }

   return &defaultMethodDefAliasesWithoutImmutable();

   }


void
J9::AliasBuilder::createAliasInfo()
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());
   self()->unresolvedShadowSymRefs().pack();
   addressShadowSymRefs().pack();
   genericIntShadowSymRefs().pack();
   genericIntArrayShadowSymRefs().pack();
   genericIntNonArrayShadowSymRefs().pack();
   intShadowSymRefs().pack();
   nonIntPrimitiveShadowSymRefs().pack();
   addressStaticSymRefs().pack();
   intStaticSymRefs().pack();
   nonIntPrimitiveStaticSymRefs().pack();
   methodSymRefs().pack();
   unsafeSymRefNumbers().pack();
   unsafeArrayElementSymRefs().pack();
   gcSafePointSymRefNumbers().pack();

   self()->tenantDataMetaSymRefs().pack();

   int32_t i;

   for (i = 0; i < _numImmutableClasses; i++)
      {
      symRefTab()->immutableSymRefNumbers()[i]->pack();
      }

   ListElement<TR_ImmutableInfo> *immutableClassInfoElem = symRefTab()->immutableInfo().getListHead();
   while (immutableClassInfoElem)
      {
      TR_ImmutableInfo *immutableClassInfo = immutableClassInfoElem->getData();
      immutableClassInfo->_immutableSymRefNumbers->pack();
      immutableClassInfoElem = immutableClassInfoElem->getNextElement();
      }

   for (i = 0; i < _numNonUserFieldClasses; i++)
      {
      self()->userFieldSymRefNumbers()[i]->pack();
      }

   setCatchLocalUseSymRefs();

   defaultMethodDefAliases().init(symRefTab()->getNumSymRefs(), comp()->trMemory(), heapAlloc, growable);
   defaultMethodDefAliases() |= addressShadowSymRefs();
   defaultMethodDefAliases() |= intShadowSymRefs();
   defaultMethodDefAliases() |= nonIntPrimitiveShadowSymRefs();
   defaultMethodDefAliases() |= arrayElementSymRefs();
   defaultMethodDefAliases() |= arrayletElementSymRefs();
   defaultMethodDefAliases() |= addressStaticSymRefs();
   defaultMethodDefAliases() |= intStaticSymRefs();
   defaultMethodDefAliases() |= nonIntPrimitiveStaticSymRefs();
   defaultMethodDefAliases() |= unsafeSymRefNumbers();
   defaultMethodDefAliases() |= gcSafePointSymRefNumbers();

   defaultMethodDefAliases() |= self()->tenantDataMetaSymRefs();

   defaultMethodDefAliasesWithoutImmutable().init(symRefTab()->getNumSymRefs(), comp()->trMemory(), heapAlloc, growable);
   defaultMethodDefAliasesWithoutUserField().init(symRefTab()->getNumSymRefs(), comp()->trMemory(), heapAlloc, growable);

   defaultMethodDefAliasesWithoutUserField() |= defaultMethodDefAliases();

    for (i = 0; i < _numNonUserFieldClasses; i++)
      {
      defaultMethodDefAliasesWithoutUserField() -= *(self()->userFieldSymRefNumbers()[i]);
      }

   defaultMethodDefAliasesWithoutImmutable() |= defaultMethodDefAliases();

   for (i = 0; i < _numImmutableClasses; i++)
      {
      defaultMethodDefAliasesWithoutImmutable() -= *(symRefTab()->immutableSymRefNumbers()[i]);
      }

   immutableClassInfoElem = symRefTab()->immutableInfo().getListHead();
   while (immutableClassInfoElem)
      {
      TR_ImmutableInfo *immutableClassInfo = immutableClassInfoElem->getData();
      defaultMethodDefAliasesWithoutImmutable() -= *(immutableClassInfo->_immutableSymRefNumbers);
      immutableClassInfoElem = immutableClassInfoElem->getNextElement();
      }

   for (i = 0; i < _numImmutableClasses; i++)
      {
      immutableConstructorDefAliases()[i] = new (trHeapMemory()) TR_BitVector(symRefTab()->getNumSymRefs(), comp()->trMemory(), heapAlloc, growable);
      *(immutableConstructorDefAliases()[i]) = defaultMethodDefAliasesWithoutImmutable();
      *(immutableConstructorDefAliases()[i]) |= *(symRefTab()->immutableSymRefNumbers()[i]);
      }

   for (i = 0; i < _numNonUserFieldClasses; i++)
      {
      userFieldMethodDefAliases()[i] = new (trHeapMemory()) TR_BitVector(symRefTab()->getNumSymRefs(), comp()->trMemory(), heapAlloc, growable);
      *(userFieldMethodDefAliases()[i]) = defaultMethodDefAliasesWithoutUserField();
      }

   immutableClassInfoElem = symRefTab()->immutableInfo().getListHead();
   while (immutableClassInfoElem)
      {
      TR_ImmutableInfo *immutableClassInfo = immutableClassInfoElem->getData();
      immutableClassInfo->_immutableConstructorDefAliases = new (trHeapMemory()) TR_BitVector(symRefTab()->getNumSymRefs(), comp()->trMemory(), heapAlloc, growable);
      *(immutableClassInfo->_immutableConstructorDefAliases) = defaultMethodDefAliasesWithoutImmutable();
      *(immutableClassInfo->_immutableConstructorDefAliases) |= *(immutableClassInfo->_immutableSymRefNumbers);
      immutableClassInfoElem = immutableClassInfoElem->getNextElement();
      }

   defaultMethodUseAliases().init(symRefTab()->getNumSymRefs(), comp()->trMemory(), heapAlloc, growable);
   defaultMethodUseAliases() |= defaultMethodDefAliases();
   defaultMethodUseAliases() |= catchLocalUseSymRefs();

   if (symRefTab()->element(TR::SymbolReferenceTable::contiguousArraySizeSymbol))
      defaultMethodUseAliases().set(symRefTab()->element(TR::SymbolReferenceTable::contiguousArraySizeSymbol)->getReferenceNumber());
   if (symRefTab()->element(TR::SymbolReferenceTable::discontiguousArraySizeSymbol))
      defaultMethodUseAliases().set(symRefTab()->element(TR::SymbolReferenceTable::discontiguousArraySizeSymbol)->getReferenceNumber());
   if (symRefTab()->element(TR::SymbolReferenceTable::vftSymbol))
      defaultMethodUseAliases().set(symRefTab()->element(TR::SymbolReferenceTable::vftSymbol)->getReferenceNumber());

   methodsThatMayThrow().init(symRefTab()->getNumSymRefs(), comp()->trMemory(), heapAlloc, growable);
   methodsThatMayThrow() |= methodSymRefs();

   static TR_RuntimeHelper helpersThatMayThrow[] =
      {
      TR_typeCheckArrayStore,
      TR_arrayStoreException,
      TR_arrayBoundsCheck,
      TR_checkCast,
      TR_checkCastForArrayStore,
      TR_divCheck,
      TR_aThrow,
      TR_aNewArray,
      TR_monitorExit,
      TR_newObject,
      TR_newObjectNoZeroInit,
      TR_newArray,
      TR_nullCheck,
      TR_methodTypeCheck,
      TR_incompatibleReceiver,
      TR_IncompatibleClassChangeError,
      TR_multiANewArray
      };

   for (i = 0; i < (sizeof(helpersThatMayThrow) / 4); ++i)
      if (symRefTab()->element(helpersThatMayThrow[i]))
         methodsThatMayThrow().set(helpersThatMayThrow[i]);

   static TR::SymbolReferenceTable::CommonNonhelperSymbol nonhelpersThatMayThrow[] =
      {
      TR::SymbolReferenceTable::resolveCheckSymbol
      };

   for (i = 0; i < (sizeof(nonhelpersThatMayThrow) / 4); ++i)
      if (symRefTab()->element(helpersThatMayThrow[i]))
         methodsThatMayThrow().set(symRefTab()->getNumHelperSymbols() + nonhelpersThatMayThrow[i]);

   for (CallAliases * callAliases = _callAliases.getFirst(); callAliases; callAliases = callAliases->getNext())
      callAliases->_methodSymbol->setHasVeryRefinedAliasSets(false);
   _callAliases.setFirst(0);

   if (comp()->getOption(TR_TraceAliases))
      {
      comp()->getDebug()->printAliasInfo(comp()->getOutFile(), symRefTab());
      }

   }
