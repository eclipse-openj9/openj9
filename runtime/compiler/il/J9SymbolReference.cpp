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

#include "il/J9SymbolReference.hpp"

#include <stdint.h>
#include "compile/Compilation.hpp"
#include "env/CompilerEnv.hpp"
#include "env/VMAccessCriticalSection.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "il/symbol/ParameterSymbol.hpp"
#include "env/KnownObjectTable.hpp"
#include "runtime/RuntimeAssumptions.hpp"
#include "env/CHTable.hpp"
#include "env/PersistentCHTable.hpp"
#include "env/VMJ9.h"

namespace J9
{

SymbolReference::SymbolReference(
      TR::SymbolReferenceTable *symRefTab,
      TR::Symbol *sym,
      mcount_t owningMethodIndex,
      int32_t cpIndex,
      int32_t unresolvedIndex,
      TR::KnownObjectTable::Index knownObjectIndex)
   {
   self()->init(symRefTab,
        symRefTab->assignSymRefNumber(self()),
        sym,
        0,  // Offset 0
        owningMethodIndex,
        cpIndex,
        unresolvedIndex);

   _knownObjectIndex = knownObjectIndex;

   if (sym->isResolvedMethod())
      symRefTab->comp()->registerResolvedMethodSymbolReference(self());

   //Check for init method
   if (sym->isMethod())
      {
      char *name         = sym->castToMethodSymbol()->getMethod()->nameChars();
      int   nameLen      = sym->castToMethodSymbol()->getMethod()->nameLength();
      if ((nameLen == 6) &&
          !strncmp(name, "<init>", 6))
         {
         self()->setInitMethod();
         }
      }

   symRefTab->checkImmutable(self());
   }


static char * dataTypeToSig[] = {0,"B","Z","C","S","I","J","F","D",0,0,0};

/**
 * This method is used when _cpIndex is to be used for resolution.
 * For any other purpose, use getCPIndex().
 *
 * This method explicitly clears unused higher order bits in _cpIndex to
 * avoid undesired sign extension.
 */
uint32_t
SymbolReference::getCPIndexForVM()
   {
   uint32_t index = (uint32_t) self()->getCPIndex();
   return index & 0x3FFFF;      // return lower order 18 bits
   }

bool
SymbolReference::isClassArray(TR::Compilation * c)
   {
   TR::StaticSymbol * sym = self()->getSymbol()->getStaticSymbol();
   if (sym == NULL || self()->isUnresolved())
      return false;

   return TR::Compiler->cls.isClassArray(c, (TR_OpaqueClassBlock *) sym->getStaticAddress());
   }

bool
SymbolReference::isClassFinal(TR::Compilation * c)
   {
   TR::StaticSymbol * sym = self()->getSymbol()->getStaticSymbol();
   if (sym == NULL || self()->isUnresolved())
      return false;

   return TR::Compiler->cls.isClassFinal(c, (TR_OpaqueClassBlock *) sym->getStaticAddress());
   }

bool
SymbolReference::isClassAbstract(TR::Compilation * c)
   {
   TR::StaticSymbol * sym = self()->getSymbol()->getStaticSymbol();
   if (sym == NULL || self()->isUnresolved())
      return false;

   return TR::Compiler->cls.isAbstractClass(c, (TR_OpaqueClassBlock *) sym->getStaticAddress());
   }

bool
SymbolReference::isClassInterface(TR::Compilation * c)
   {
   TR::StaticSymbol * sym = self()->getSymbol()->getStaticSymbol();
   if (sym == NULL || self()->isUnresolved())
      return false;

   return TR::Compiler->cls.isInterfaceClass(c, (TR_OpaqueClassBlock *) sym->getStaticAddress());
   }

// resolved final class that is not an array returns TRUE
bool
SymbolReference::isNonArrayFinal(TR::Compilation * c)
   {
   TR::StaticSymbol * sym = self()->getSymbol()->getStaticSymbol();
   if (sym == NULL || self()->isUnresolved())
      {
      return false;
      }
   return !self()->isClassArray(c) && self()->isClassFinal(c);
   }

int32_t
SymbolReference::classDepth(TR::Compilation * c)
   {
   TR::StaticSymbol * sym = self()->getSymbol()->getStaticSymbol();
   if (sym == NULL || self()->isUnresolved())
      {
      return -1;
      }
   return (int32_t)TR::Compiler->cls.classDepthOf((TR_OpaqueClassBlock *) sym->getStaticAddress());
   }


char *
prependNumParensToSig(const char *name, int32_t &len, int32_t numParens,  TR_AllocationKind allocKind)
   {
   TR::Compilation * comp = TR::comp();
   char * sig;

   len += numParens;
   sig = (char *)comp->trMemory()->allocateMemory(len, allocKind);
   int32_t i = 0;
   while (i < numParens)
      {
      sig[i] = '[';
      i++;
      }
   memcpy(sig+i,name,len-numParens);

   return sig;
   }


const char *
SymbolReference::getTypeSignature(int32_t & len, TR_AllocationKind allocKind, bool *isFixed)
   {
   TR::Compilation * comp = TR::comp();
   bool allowForAOT = comp->getOption(TR_UseSymbolValidationManager);

   TR_PersistentClassInfo * persistentClassInfo = NULL;
   switch (_symbol->getKind())
      {
      case TR::Symbol::IsParameter:
         return _symbol->castToParmSymbol()->getTypeSignature(len);

      case TR::Symbol::IsShadow:
          persistentClassInfo =
             (comp->getPersistentInfo()->getPersistentCHTable() == NULL) ? NULL :
             comp->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(comp->getCurrentMethod()->containingClass(), comp, allowForAOT);
          if (persistentClassInfo &&
              persistentClassInfo->getFieldInfo() &&
              persistentClassInfo->getFieldInfo()->getFirst() &&
              !self()->isUnresolved() &&
              !_symbol->isArrayShadowSymbol() &&
              !_symbol->isArrayletShadowSymbol())
              {
              TR_PersistentFieldInfo *fieldInfo = NULL;
              if (_symbol->isPrivate() ||
                  _symbol->isFinal())
                  fieldInfo = persistentClassInfo->getFieldInfo()->find(comp, _symbol, self());

              if (fieldInfo &&
                  fieldInfo->isTypeInfoValid() &&
                  (fieldInfo->getNumChars() > 0) &&
                  performTransformation(comp,"O^O CLASS LOOKAHEAD: Returning type %s info for symbol %p based on class file examination\n",
                                          fieldInfo->getClassPointer(), _symbol))
                  {
                  if (isFixed)
                     *isFixed = true;
                  len = fieldInfo->getNumChars();
                  return fieldInfo->getClassPointer();
                  }

              if (fieldInfo &&
                  fieldInfo->isBigDecimalType() &&
                  performTransformation(comp,"O^O CLASS LOOKAHEAD: Returning type %s info for symbol %p based on class file examination\n",
                                          "Ljava/math/BigDecimal;", _symbol))
                 {
                 len = 22;
                 return "Ljava/math/BigDecimal;";
                 }

             if (fieldInfo &&
                  fieldInfo->isBigIntegerType() &&
                  performTransformation(comp,"O^O CLASS LOOKAHEAD: Returning type %s info for symbol %p based on class file examination\n",
                                          "Ljava/math/BigInteger;", _symbol))
                 {
                 len = 22;
                 return "Ljava/math/BigInteger;";
                 }
              }

           return (_cpIndex > 0 ? self()->getOwningMethod(comp)->fieldSignatureChars(_cpIndex, len) : 0);

      case TR::Symbol::IsStatic:
           if (_symbol->isStatic() && _symbol->isFinal() && !self()->isUnresolved())
               {
               TR::StaticSymbol * symbol = _symbol->castToStaticSymbol();
               TR::DataType type = symbol->getDataType();
               TR_OpaqueClassBlock * classOfStatic = self()->getOwningMethod(comp)->classOfStatic(_cpIndex, allowForAOT);

               bool isClassInitialized = false;
               TR_PersistentClassInfo * classInfo =
                  (comp->getPersistentInfo()->getPersistentCHTable() == NULL) ? NULL :
                  comp->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(classOfStatic, comp, allowForAOT);
               if (classInfo && classInfo->isInitialized())
                  {
                  if (classInfo->getFieldInfo() && !classInfo->cannotTrustStaticFinal())
                     isClassInitialized = true;
                  }

              if ((classOfStatic != comp->getSystemClassPointer() &&
                  isClassInitialized &&
                   (type == TR::Address)))
                 {
                 TR::VMAccessCriticalSection vmAccessCriticalSection(comp->fej9(),
                                                                      TR::VMAccessCriticalSection::tryToAcquireVMAccess,
                                                                      comp);
                 if (vmAccessCriticalSection.hasVMAccess())
                    {
                    uintptrj_t objectStaticAddress = (uintptrj_t)symbol->getStaticAddress();
                    uintptrj_t objectRef = comp->fej9()->getStaticReferenceFieldAtAddress(objectStaticAddress);
                    if (objectRef != 0)
                       {
                       TR_OpaqueClassBlock *classOfObject = comp->fej9()->getObjectClass(objectRef);
                       const char * s = TR::Compiler->cls.classNameChars(comp, classOfObject, len);
                       if (s && (s[0] != '['))
                          {
                          s = classNameToSignature(s, len, comp);
                          }
                       else
                          {
                          int32_t numParens = 0;
                          while (s && (s[0] == '[') && (s[1] == 'L'))
                             {
                             numParens++;
                             classOfObject = comp->fe()->getComponentClassFromArrayClass(classOfObject);
                             s = TR::Compiler->cls.classNameChars(comp, classOfObject, len);
                              }
                          s = classNameToSignature(s, len, comp);
                          s = prependNumParensToSig(s, len, numParens);
                          }

                       if (isFixed)
                          *isFixed = true;
                       return s;
                       }
                    }
                 }
               }

         if (_symbol->isClassObject())
            {
            const char * sig = TR::Compiler->cls.classNameChars(comp, self(), len);
            if (!sig)
               {
               len = 18;
               return "Ljava/lang/Object;";
               }
            return classNameToSignature(sig, len, comp, allocKind);
            }

         if (_symbol->isConstString())
            {
            len = 18;
            return "Ljava/lang/String;";
            }
         if (_symbol->isConstMethodType())
            {
            len = 21;
            return "Ljava/lang/invoke/MethodType;";
            }
         if (_symbol->isConstMethodHandle())
            {
            len = 23;
            return "Ljava/lang/invoke/MethodHandle;";
            }
         if (_symbol->isConstantDynamic())
            {
            TR::StaticSymbol * symbol = _symbol->castToStaticSymbol();
            int32_t condySigLength;
            char *returnType = symbol->getConstantDynamicClassSignature(condySigLength);
            len = condySigLength;
            return returnType;
            }
         if (_symbol->isConst())
            {
            len = 1;
            return dataTypeToSig[_symbol->getDataType()];
            }

         persistentClassInfo =
            (comp->getPersistentInfo()->getPersistentCHTable() == NULL) ? NULL :
             comp->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(comp->getCurrentMethod()->containingClass(), comp, allowForAOT);
         if (persistentClassInfo &&
             persistentClassInfo->getFieldInfo() &&
             persistentClassInfo->getFieldInfo()->getFirst() &&
             !self()->isUnresolved() &&
             !_symbol->isArrayShadowSymbol() &&
             !_symbol->isArrayletShadowSymbol())
            {
            TR_PersistentFieldInfo *fieldInfo = NULL;
            if (_symbol->isPrivate() ||
                _symbol->isFinal())
                fieldInfo = persistentClassInfo->getFieldInfo()->find(comp, _symbol, self());

            if (fieldInfo &&
                fieldInfo->isTypeInfoValid() &&
                (fieldInfo->getNumChars() > 0) &&
                performTransformation(comp, "O^O CLASS LOOKAHEAD: Returning type %s info for symbol %p based on class file examination\n",
                                      fieldInfo->getClassPointer(), _symbol))
               {
               if (isFixed)
                  *isFixed = true;
               len = fieldInfo->getNumChars();
               return fieldInfo->getClassPointer();
               }

            if (fieldInfo &&
                fieldInfo->isBigDecimalType() &&
                performTransformation(comp,"O^O CLASS LOOKAHEAD: Returning type %s info for symbol %p based on class file examination\n",
                                      "Ljava/math/BigDecimal;", _symbol))
               {
               len = 22;
               return "Ljava/math/BigDecimal;";
               }

            if (fieldInfo &&
                fieldInfo->isBigIntegerType() &&
                performTransformation(comp,"O^O CLASS LOOKAHEAD: Returning type %s info for symbol %p based on class file examination\n",
                                          "Ljava/math/BigInteger;", _symbol))
               {
               len = 22;
               return "Ljava/math/BigInteger;";
               }
            }

         return self()->getOwningMethod(comp)->staticSignatureChars(_cpIndex, len);

      case TR::Symbol::IsMethod:
      case TR::Symbol::IsResolvedMethod:
         {
         TR_Method * method = _symbol->castToMethodSymbol()->getMethod();
         if (method)
            {
            char * sig   = method->signatureChars();
            char *returnType = strchr(sig,')')+1;
            len = method->signatureLength() - (returnType-sig);
            return returnType;
            }
         return 0;
         }

      case TR::Symbol::IsAutomatic:
         if (_symbol->isLocalObject())
            {
            // todo:
            }
         return 0;

      case TR::Symbol::IsMethodMetaData:
      case TR::Symbol::IsLabel:
      default:
         return 0;
      }
   }

}
