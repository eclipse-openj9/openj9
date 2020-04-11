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

#ifndef TR_ILGENERATOR_METHOD_DETAILS_INCL
#define TR_ILGENERATOR_METHOD_DETAILS_INCL

#include "ilgen/J9IlGeneratorMethodDetails.hpp"

#include <stdint.h>
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "infra/Annotations.hpp"

class TR_InlineBlocks;

namespace TR
{

class OMR_EXTENSIBLE IlGeneratorMethodDetails : public J9::IlGeneratorMethodDetailsConnector
   {

public:

   IlGeneratorMethodDetails() :
      J9::IlGeneratorMethodDetailsConnector() {}

   IlGeneratorMethodDetails(J9Method* method) :
      J9::IlGeneratorMethodDetailsConnector(method) {}

   IlGeneratorMethodDetails(TR_ResolvedMethod *method) :
      J9::IlGeneratorMethodDetailsConnector(method) {}

   IlGeneratorMethodDetails(const TR::IlGeneratorMethodDetails & other) :
      J9::IlGeneratorMethodDetailsConnector(other) {}

   };

}


namespace J9
{

class JitDumpMethodDetails : public TR::IlGeneratorMethodDetails
   {
   // Objects cannot hold data of its own: must store in the _data union in TR::IlGeneratorMethodDetails

public:
   JitDumpMethodDetails(J9Method* method) : TR::IlGeneratorMethodDetails(method) { }
   JitDumpMethodDetails(const JitDumpMethodDetails& other) : TR::IlGeneratorMethodDetails(other.getMethod()) { }

   virtual const char * name()     const { return "DumpMethod"; }

   virtual bool isOrdinaryMethod() const { return false; }
   virtual bool isJitDumpMethod()     const { return true; }


   virtual bool sameAs(TR::IlGeneratorMethodDetails & other, TR_FrontEnd *fe)
      {
      return other.isJitDumpMethod() && sameMethod(other);
      }

   virtual bool supportsInvalidation() { return false; }
   };


class MethodInProgressDetails : public TR::IlGeneratorMethodDetails
   {
   // Objects cannot hold data of its own: must store in the _data union in TR::IlGeneratorMethodDetails

public:
   MethodInProgressDetails(J9Method* method, int32_t byteCodeIndex) :
      TR::IlGeneratorMethodDetails(method)
      {
      _data._byteCodeIndex = byteCodeIndex;
      }
   MethodInProgressDetails(TR_ResolvedMethod *method, int32_t byteCodeIndex) :
      TR::IlGeneratorMethodDetails(method)
      {
      _data._byteCodeIndex = byteCodeIndex;
      }
   MethodInProgressDetails(const MethodInProgressDetails & other) :
      TR::IlGeneratorMethodDetails(other.getMethod())
      {
      _data._byteCodeIndex = other.getByteCodeIndex();
      }

   virtual const char * name()         const { return "MethodInProgress"; }

   virtual bool isOrdinaryMethod()     const { return false; }
   virtual bool isMethodInProgress()   const { return true; }
   virtual bool supportsInvalidation() const { return false; }

   int32_t getByteCodeIndex()          const { return _data._byteCodeIndex; }

   virtual bool sameAs(TR::IlGeneratorMethodDetails & other, TR_FrontEnd *fe)
      {
      return other.isMethodInProgress() &&
             isSimilarEnough(static_cast<MethodInProgressDetails &>(other), fe);
      }

   virtual void printDetails(TR_FrontEnd *fe, TR::FILE *file);

private:
   bool isSimilarEnough(MethodInProgressDetails & other, TR_FrontEnd *fe)
      {
      return sameMethod(other);
      }

   bool sameAsMethodInProgress(TR::IlGeneratorMethodDetails & other, TR_FrontEnd *fe)
      {
      return TR::IlGeneratorMethodDetails::sameAs(other, fe) &&
             static_cast<MethodInProgressDetails &>(other).getByteCodeIndex() == getByteCodeIndex();
      }

   };


class NewInstanceThunkDetails : public TR::IlGeneratorMethodDetails
   {
   // Objects cannot hold data of its own: must store in the _data union in TR::IlGeneratorMethodDetails

public:
   NewInstanceThunkDetails(J9Method* method, J9Class *clazz) :
      TR::IlGeneratorMethodDetails(method)
      {
      _data._class = clazz;
      }
   NewInstanceThunkDetails(TR_ResolvedMethod *method, J9Class *clazz) :
      TR::IlGeneratorMethodDetails(method)
      {
      _data._class = clazz;
      }
   NewInstanceThunkDetails(const NewInstanceThunkDetails & other) :
      TR::IlGeneratorMethodDetails(other.getMethod())
      {
      _data._class = other.classNeedingThunk();
      }

   virtual const char * name()         const { return "NewInstanceThunk"; }

   virtual bool isOrdinaryMethod()     const { return false; }
   virtual bool isNewInstanceThunk()   const { return true; }
   virtual bool supportsInvalidation() const { return false; }

   J9Class *classNeedingThunk()        const { return _data._class; }

   virtual J9Class *getClass()         const { return classNeedingThunk(); }

   bool isThunkFor(J9Class *clazz)     const { return clazz == classNeedingThunk(); }

   virtual bool sameAs(TR::IlGeneratorMethodDetails & other, TR_FrontEnd *fe)
      {
      return other.isNewInstanceThunk() &&
             sameMethod(other) &&
             static_cast<NewInstanceThunkDetails &>(other).classNeedingThunk() == classNeedingThunk();
      }

   virtual void printDetails(TR_FrontEnd *fe, TR::FILE *file);
   };


// This class is currently not instantiated directly
class ArchetypeSpecimenDetails : public TR::IlGeneratorMethodDetails
   {
   // Objects cannot hold data of its own: must store in the _data union in TR::IlGeneratorMethodDetails

public:
   ArchetypeSpecimenDetails(J9Method* method) : TR::IlGeneratorMethodDetails(method) { }
   ArchetypeSpecimenDetails(TR_ResolvedMethod *method) : TR::IlGeneratorMethodDetails(method) { }
   ArchetypeSpecimenDetails(const ArchetypeSpecimenDetails &other) : TR::IlGeneratorMethodDetails(other) { }

   virtual const char * name()        const { return "ArchetypeSpecimen"; }

   virtual bool isOrdinaryMethod()    const { return false; }
   virtual bool isArchetypeSpecimen() const { return true; }

   virtual TR_IlGenerator *getIlGenerator(TR::ResolvedMethodSymbol *methodSymbol,
                                          TR_FrontEnd * fe,
                                          TR::Compilation *comp,
                                          TR::SymbolReferenceTable *symRefTab,
                                          bool forceClassLookahead,
                                          TR_InlineBlocks *blocksToInline);

   virtual bool sameAs(TR::IlGeneratorMethodDetails & other, TR_FrontEnd *fe)
      {
      return other.isArchetypeSpecimen() && sameMethod(other);
      }
   };


// This class is currently not instantiated directly
class MethodHandleThunkDetails : public ArchetypeSpecimenDetails
   {
   // Objects cannot hold data of its own: must store in the _data union in TR::IlGeneratorMethodDetails

public:
   MethodHandleThunkDetails(J9Method* method, uintptr_t *handleRef, uintptr_t *argRef) :
      ArchetypeSpecimenDetails(method)
      {
      _data._methodHandleData._handleRef = handleRef;
      _data._methodHandleData._argRef = argRef;
      }
   MethodHandleThunkDetails(TR_ResolvedMethod *method, uintptr_t *handleRef, uintptr_t *argRef) :
      ArchetypeSpecimenDetails(method)
      {
      _data._methodHandleData._handleRef = handleRef;
      _data._methodHandleData._argRef = argRef;
      }
   MethodHandleThunkDetails(const MethodHandleThunkDetails &other) :
      ArchetypeSpecimenDetails(other)
      {
      _data._methodHandleData._handleRef = other.getHandleRef();
      _data._methodHandleData._argRef = other.getArgRef();
      }

   virtual const char * name()         const { return "MethodHandleThunk"; }

   virtual bool isMethodHandleThunk()  const { return true; }

   uintptr_t *getHandleRef()          const { return _data._methodHandleData._handleRef; }
   uintptr_t *getArgRef()             const { return _data._methodHandleData._argRef;    }

   virtual bool sameAs(TR::IlGeneratorMethodDetails & other, TR_FrontEnd *fe)
      {
      return other.isMethodHandleThunk() &&
             sameMethod(other) &&
             isSameThunk(static_cast<MethodHandleThunkDetails &>(other),
                         (TR_J9VMBase *)(fe));
      }

   virtual bool supportsInvalidation() const { return false; }

   virtual bool isShareable()          const { return false; }

   virtual bool isCustom()             const { return false; }

   virtual void printDetails(TR_FrontEnd *fe, TR::FILE *file);

private:
   virtual bool isSameThunk(MethodHandleThunkDetails & otherThunk, TR_J9VMBase *fe);
   };


class ShareableInvokeExactThunkDetails : public MethodHandleThunkDetails
   {
   // Objects cannot hold data of its own: must store in the _data union in TR::IlGeneratorMethodDetails

public:
   ShareableInvokeExactThunkDetails(J9Method* method, uintptr_t *handleRef, uintptr_t *argRef) :
      MethodHandleThunkDetails(method, handleRef, argRef) { }
   ShareableInvokeExactThunkDetails(TR_ResolvedMethod *method, uintptr_t *handleRef, uintptr_t *argRef) :
      MethodHandleThunkDetails(method, handleRef, argRef) { }
   ShareableInvokeExactThunkDetails(const ShareableInvokeExactThunkDetails & other) :
      MethodHandleThunkDetails(other.getMethod(), other.getHandleRef(), other.getArgRef()) { }

   virtual const char * name() const { return "SharableInvokeExactThunk"; }

   virtual bool isShareable()  const { return true; }

private:
   virtual bool isSameThunk(MethodHandleThunkDetails & otherThunk, TR_J9VMBase *fe);
   };


class CustomInvokeExactThunkDetails : public MethodHandleThunkDetails
   {
   // Objects cannot hold data of its own: must store in the _data union in TR::IlGeneratorMethodDetails

public:
   CustomInvokeExactThunkDetails(J9Method* method, uintptr_t *handleRef, uintptr_t *argRef) :
      MethodHandleThunkDetails(method, handleRef, argRef) { }
   CustomInvokeExactThunkDetails(TR_ResolvedMethod *method, uintptr_t *handleRef, uintptr_t *argRef) :
      MethodHandleThunkDetails(method, handleRef, argRef) { }
   CustomInvokeExactThunkDetails(const CustomInvokeExactThunkDetails & other) :
      MethodHandleThunkDetails(other.getMethod(), other.getHandleRef(), other.getArgRef()) { }

   virtual const char * name() const { return "CustomInvokeExactThunk"; }

   virtual bool isCustom()     const { return true; }

private:
   virtual bool isSameThunk(MethodHandleThunkDetails & otherThunk, TR_J9VMBase *fe);
   };


}

#endif
