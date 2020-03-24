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

#include "env/FrontEnd.hpp"
#include "env/KnownObjectTable.hpp"
#include "compile/Compilation.hpp"
#include "compile/InlineBlock.hpp"
#include "compile/Method.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "env/VMAccessCriticalSection.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "ilgen/J9ByteCodeIlGenerator.hpp"
#include "env/VMJ9.h"



namespace J9
{

TR::IlGeneratorMethodDetails *
IlGeneratorMethodDetails::clone(TR::IlGeneratorMethodDetails &storage, const TR::IlGeneratorMethodDetails & other)
   {
   // The if nest below covers every concrete subclass of IlGeneratorMethodDetails.
   // If other is not one of these classes, then it will assert.

   if (other.isOrdinaryMethod())
      return new (&storage) TR::IlGeneratorMethodDetails(static_cast<const TR::IlGeneratorMethodDetails &>(other));
   else if (other.isDumpMethod())
      return new (&storage) DumpMethodDetails(static_cast<const DumpMethodDetails &>(other));
   else if (other.isNewInstanceThunk())
      return new (&storage) NewInstanceThunkDetails(static_cast<const NewInstanceThunkDetails &>(other));
   else if (other.isMethodInProgress())
      return new (&storage) MethodInProgressDetails(static_cast<const MethodInProgressDetails &>(other));
   else if (other.isMethodHandleThunk())
      {
      if (static_cast<const MethodHandleThunkDetails &>(other).isShareable())
         return new (&storage) ShareableInvokeExactThunkDetails(static_cast<const ShareableInvokeExactThunkDetails &>(other));
      else if (static_cast<const MethodHandleThunkDetails &>(other).isCustom())
         return new (&storage) CustomInvokeExactThunkDetails(static_cast<const CustomInvokeExactThunkDetails &>(other));
      }

   TR_ASSERT(0, "Unexpected IlGeneratorMethodDetails object\n");
   return NULL; // error case
   }


#if defined(J9VM_OPT_JITSERVER)
TR::IlGeneratorMethodDetails *
IlGeneratorMethodDetails::clone(TR::IlGeneratorMethodDetails &storage, const TR::IlGeneratorMethodDetails & other, const IlGeneratorMethodDetailsType type)
   {
   // The if nest below covers every concrete subclass of IlGeneratorMethodDetails.
   // If other is not one of these classes, then it will assert.

   if (type & ORDINARY_METHOD)
      return new (&storage) TR::IlGeneratorMethodDetails(static_cast<const TR::IlGeneratorMethodDetails &>(other));
   else if (type & DUMP_METHOD)
      return new (&storage) DumpMethodDetails(static_cast<const DumpMethodDetails &>(other));
   else if (type & NEW_INSTANCE_THUNK)
      return new (&storage) NewInstanceThunkDetails(static_cast<const NewInstanceThunkDetails &>(other));
   else if (type & METHOD_IN_PROGRESS)
      return new (&storage) MethodInProgressDetails(static_cast<const MethodInProgressDetails &>(other));
   else if (type & METHOD_HANDLE_THUNK)
      {
      if (type & SHAREABLE_THUNK)
         return new (&storage) ShareableInvokeExactThunkDetails(static_cast<const ShareableInvokeExactThunkDetails &>(other));
      else if (type & CUSTOM_THUNK)
         return new (&storage) CustomInvokeExactThunkDetails(static_cast<const CustomInvokeExactThunkDetails &>(other));
      }

   TR_ASSERT(0, "Unexpected IlGeneratorMethodDetails object\n");
   return NULL; // error case
   }
#endif /* defined(J9VM_OPT_JITSERVER) */


IlGeneratorMethodDetails::IlGeneratorMethodDetails(const TR::IlGeneratorMethodDetails & other) :
   _method(other.getMethod())
   {
   }


IlGeneratorMethodDetails::IlGeneratorMethodDetails(TR_ResolvedMethod *method)
   {
   _method = (J9Method *)(method->getPersistentIdentifier());
   }

const J9ROMClass *
IlGeneratorMethodDetails::getRomClass() const
   {
   return J9_CLASS_FROM_METHOD(self()->getMethod())->romClass;
   }

const J9ROMMethod *
IlGeneratorMethodDetails::getRomMethod() const
   {
   return J9_ROM_METHOD_FROM_RAM_METHOD(self()->getMethod());
   }

#if defined(J9VM_OPT_JITSERVER)
IlGeneratorMethodDetailsType
IlGeneratorMethodDetails::getType() const
   {
   int type = EMPTY;
   if (self()->isOrdinaryMethod()) type |= ORDINARY_METHOD;
   if (self()->isDumpMethod()) type |= DUMP_METHOD;
   if (self()->isNewInstanceThunk()) type |= NEW_INSTANCE_THUNK;
   if (self()->isMethodInProgress()) type |= METHOD_IN_PROGRESS;
   if (self()->isArchetypeSpecimen()) type |= ARCHETYPE_SPECIMEN;
   if (self()->isMethodHandleThunk())
      {
      type |= METHOD_HANDLE_THUNK;
      if (static_cast<const MethodHandleThunkDetails *>(self())->isShareable())
         type |= SHAREABLE_THUNK;
      else if (static_cast<const MethodHandleThunkDetails *>(self())->isCustom())
         type |= CUSTOM_THUNK;
      }
   return (IlGeneratorMethodDetailsType) type;
   }
#endif /* defined(J9VM_OPT_JITSERVER) */


bool
IlGeneratorMethodDetails::sameAs(TR::IlGeneratorMethodDetails & other, TR_FrontEnd *fe)
   {
   return other.isOrdinaryMethod() && self()->sameMethod(other);
   }


bool
IlGeneratorMethodDetails::sameMethod(TR::IlGeneratorMethodDetails & other)
   {
   return (other.getMethod() == self()->getMethod());
   }


TR::IlGeneratorMethodDetails & IlGeneratorMethodDetails::create(
      TR::IlGeneratorMethodDetails & target,
      TR_ResolvedMethod *method)
   {

   TR_ResolvedJ9Method * j9method = static_cast<TR_ResolvedJ9Method *>(method);

   if (j9method->isNewInstanceImplThunk())
      return * new (&target) NewInstanceThunkDetails((J9Method *)j9method->getNonPersistentIdentifier(), (J9Class *)j9method->classOfMethod());

   else if (j9method->convertToMethod()->isArchetypeSpecimen())
      {
      if (j9method->getMethodHandleLocation())
         return * new (&target) CustomInvokeExactThunkDetails((J9Method *)j9method->getNonPersistentIdentifier(), j9method->getMethodHandleLocation(), NULL);
      else
         return * new (&target) ArchetypeSpecimenDetails((J9Method *)j9method->getNonPersistentIdentifier());
      }

   return * new (&target) TR::IlGeneratorMethodDetails((J9Method *)j9method->getNonPersistentIdentifier());

   }


TR_IlGenerator *
IlGeneratorMethodDetails::getIlGenerator(TR::ResolvedMethodSymbol *methodSymbol,
                                         TR_FrontEnd * fe,
                                         TR::Compilation *comp,
                                         TR::SymbolReferenceTable *symRefTab,
                                         bool forceClassLookahead,
                                         TR_InlineBlocks *blocksToInline)
   {
   TR_ASSERT((J9Method *)methodSymbol->getResolvedMethod()->getNonPersistentIdentifier() == self()->getMethod(),
             "getIlGenerator methodSymbol must match _method");

   return new (comp->trHeapMemory()) TR_J9ByteCodeIlGenerator(*(self()),
                                                              methodSymbol,
                                                              static_cast<TR_J9VMBase *>(fe),
                                                              comp,
                                                              symRefTab,
                                                              forceClassLookahead,
                                                              blocksToInline,
                                                              -1);
   }


void
IlGeneratorMethodDetails::print(TR_FrontEnd *fe, TR::FILE *file)
   {
   if (file == NULL)
      return;

   trfprintf(file, "%s(", self()->name());
   self()->printDetails(fe, file);
   trfprintf(file, ")");
   }


void
IlGeneratorMethodDetails::printDetails(TR_FrontEnd *fe, TR::FILE *file)
   {
   trfprintf(file, "%s", fe->sampleSignature((TR_OpaqueMethodBlock *)(self()->getMethod())));
   }

J9Class *
IlGeneratorMethodDetails::getClass() const
   {
   return J9_CLASS_FROM_METHOD(self()->getMethod());
   }


void
IlGeneratorMethodDetailsOverrideForReplay::changeMethod(
      TR::IlGeneratorMethodDetails & details,
      J9Method *newMethod)
   {
   details._method = newMethod;
   }


void
NewInstanceThunkDetails::printDetails(TR_FrontEnd *fe, TR::FILE *file)
   {
   int32_t len;
   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;
   char *className = fej9->getClassNameChars((TR_OpaqueClassBlock *)getClass(), len);
   trfprintf(file, "%.*s.newInstancePrototype(Ljava/lang/Class;)Ljava/lang/Object;", len, className);
   }


void
MethodInProgressDetails::printDetails(TR_FrontEnd *fe, TR::FILE *file)
   {
   trfprintf(file, "DLT %d,%s", getByteCodeIndex(), fe->sampleSignature((TR_OpaqueMethodBlock *)getMethod()));
   }


TR_IlGenerator *
ArchetypeSpecimenDetails::getIlGenerator(TR::ResolvedMethodSymbol *methodSymbol,
                                         TR_FrontEnd * fe,
                                         TR::Compilation *comp,
                                         TR::SymbolReferenceTable *symRefTab,
                                         bool forceClassLookahead,
                                         TR_InlineBlocks *blocksToInline)
   {
   TR_ASSERT((J9Method *)methodSymbol->getResolvedMethod()->getNonPersistentIdentifier() == getMethod(),
             "getIlGenerator methodSymbol must match _method");

   // Figure out the argPlaceholderSlot, which is the slot number of the last argument.
   // Note that this creates and discards a TR_ResolvedMethod.  TODO: A query
   // on J9MethodBase could avoid this.
   //
   TR_ResolvedMethod *method = fe->createResolvedMethod(comp->trMemory(), (TR_OpaqueMethodBlock *)getMethod());
   int32_t argPlaceholderSlot = method->numberOfParameterSlots()-1; // Assumes the arg placeholder is a 1-slot type

   return new (comp->trHeapMemory()) TR_J9ByteCodeIlGenerator(*this,
                                                              methodSymbol,
                                                              static_cast<TR_J9VMBase *>(fe),
                                                              comp,
                                                              symRefTab,
                                                              forceClassLookahead,
                                                              blocksToInline,
                                                              argPlaceholderSlot);
   }


void
MethodHandleThunkDetails::printDetails(TR_FrontEnd *fe, TR::FILE *file)
   {
#if 0
   // annoying: knot can only be accessed from the compilation object which isn't always handy: wait for thread locals
   TR::KnownObjectTable *knot = fe->getKnownObjectTable();
   if (knot)
      trfprintf(file, "obj%d,%s", knot->getOrCreateIndexAt(getHandleRef()), fe->sampleSignature((TR_OpaqueMethodBlock *)getMethod()));
   else
#endif
      trfprintf(file, "%p,%s", getHandleRef(), fe->sampleSignature((TR_OpaqueMethodBlock *)getMethod()));
   }


bool
MethodHandleThunkDetails::isSameThunk(MethodHandleThunkDetails &other, TR_J9VMBase *fe)
   {
   TR_ASSERT(0, "MethodHandleThunk must be either shareable or custom");
   return false;
   }


bool
ShareableInvokeExactThunkDetails::isSameThunk(MethodHandleThunkDetails & other, TR_J9VMBase *fe)
   {
   if (!other.isShareable())
      return false;

   uintptr_t thisThunkTuple, otherThunkTuple;

   // Consider: there is a race condition here.  If there are two shareable thunks requested,
   // and then we change the thunktuple of one of the handles, then this code will not detect
   // the dup and we'll end up compiling two identical thunks.
   //
      {
      TR::VMAccessCriticalSection isSameThunk(fe);
      thisThunkTuple  = fe->getReferenceField(*this->getHandleRef(), "thunks", "Ljava/lang/invoke/ThunkTuple;");
      otherThunkTuple = fe->getReferenceField(*other.getHandleRef(), "thunks", "Ljava/lang/invoke/ThunkTuple;");
      }

   // Same MethodHandle thunk request iff the two requests point to the same ThunkTuple
   return thisThunkTuple == otherThunkTuple;
   }


bool
CustomInvokeExactThunkDetails::isSameThunk(MethodHandleThunkDetails & other, TR_J9VMBase *fe)
   {
   if (!other.isCustom())
      return false;

   bool bothHaveNullArg = false;
   if (this->getArgRef() == NULL)
      {
      if (other.getArgRef() == NULL)
         bothHaveNullArg = true;
      else
         return false;
      }
   else if (other.getArgRef() == NULL)
      return false;

   bool sameHandle, sameArg;

      {
      TR::VMAccessCriticalSection isSameThunk(fe);
      sameHandle = (*this->getHandleRef()) == (*other.getHandleRef());
      sameArg    = bothHaveNullArg || ((*this->getArgRef()) == (*other.getArgRef()));
      }

   // Same thunk request iff it's for the same handle with the same arg
   return sameHandle && sameArg;
   }

}
