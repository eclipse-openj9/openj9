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

#ifndef J9_ILGENERATOR_METHOD_DETAILS_INCL
#define J9_ILGENERATOR_METHOD_DETAILS_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_ILGENERATOR_METHOD_DETAILS_CONNECTOR
#define J9_ILGENERATOR_METHOD_DETAILS_CONNECTOR
namespace J9 { class IlGeneratorMethodDetails; }
namespace J9 { typedef J9::IlGeneratorMethodDetails IlGeneratorMethodDetailsConnector; }
#endif

#include "ilgen/OMRIlGeneratorMethodDetails.hpp"

#include <stdint.h>
#include "infra/Annotations.hpp"
#include "env/IO.hpp"
#include "env/jittypes.h"

class J9Class;
class J9Method;
#if defined(JITSERVER_SUPPORT)
class J9ROMClass;
class J9ROMMethod;
#endif /* defined(JITSERVER_SUPPORT) */
class TR_FrontEnd;
class TR_IlGenerator;
class TR_InlineBlocks;
class TR_J9VMBase;
class TR_ResolvedMethod;
namespace TR { class Compilation; }
namespace TR { class IlGeneratorMethodDetails; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class SymbolReferenceTable;}

namespace J9
{
#if defined(JITSERVER_SUPPORT)
enum IlGeneratorMethodDetailsType
   {
   EMPTY = 0,
   ORDINARY_METHOD = 1<<0,
   DUMP_METHOD = 1<<1,
   NEW_INSTANCE_THUNK = 1<<2,
   METHOD_IN_PROGRESS = 1<<3,
   ARCHETYPE_SPECIMEN = 1<<4,
   METHOD_HANDLE_THUNK = 1<<5,
   SHAREABLE_THUNK = 1<<6,
   CUSTOM_THUNK = 1<<7,
   };
#endif /* defined(JITSERVER_SUPPORT) */

class OMR_EXTENSIBLE IlGeneratorMethodDetails : public OMR::IlGeneratorMethodDetailsConnector
   {
   friend class IlGeneratorMethodDetailsOverrideForReplay;

public:
   IlGeneratorMethodDetails() :
      OMR::IlGeneratorMethodDetailsConnector()
      {
      _method = NULL;
      }

   IlGeneratorMethodDetails(J9Method * const method) :
      OMR::IlGeneratorMethodDetailsConnector(),
      _method(method)
   { }

   IlGeneratorMethodDetails(TR_ResolvedMethod *method);

   IlGeneratorMethodDetails(const TR::IlGeneratorMethodDetails & other);

   static TR::IlGeneratorMethodDetails & create(TR::IlGeneratorMethodDetails & target, TR_ResolvedMethod *method);

   static TR::IlGeneratorMethodDetails * clone(TR::IlGeneratorMethodDetails & storage, const TR::IlGeneratorMethodDetails & other);

#if defined(JITSERVER_SUPPORT)
   // Constructs a new IlGeneratorMethodDetails object of given type based on an existing TR::IlGeneratorMethodDetails object (other).
   // The existing TR::IlGeneratorMethodDetails object (other) is obtained through de-serialization. It is missing the vtable pointer.
   // Therefore the new object instance type cannot be determined through the virtual function calls such as other.isOrdinaryMethod(), etc.
   static TR::IlGeneratorMethodDetails * clone(TR::IlGeneratorMethodDetails & storage, const TR::IlGeneratorMethodDetails & other, const IlGeneratorMethodDetailsType type);
#endif /* defined(JITSERVER_SUPPORT) */

   virtual const char * name() const { return "OrdinaryMethod"; }

   virtual bool isOrdinaryMethod()     const { return true; }
   virtual bool isDumpMethod()         const { return false; }
   virtual bool isNewInstanceThunk()   const { return false; }
   virtual bool isMethodInProgress()   const { return false; }
   virtual bool isArchetypeSpecimen()  const { return false; }
   virtual bool isMethodHandleThunk()  const { return false; }
   virtual bool supportsInvalidation() const { return true; }

   J9Method *getMethod() const { return _method; }
   virtual J9Class *getClass() const;
#if defined(JITSERVER_SUPPORT)
   IlGeneratorMethodDetailsType getType() const;
   virtual const J9ROMClass *getRomClass() const;
   virtual const J9ROMMethod *getRomMethod() const;
#endif /* defined(JITSERVER_SUPPORT) */

   virtual TR_IlGenerator *getIlGenerator(TR::ResolvedMethodSymbol *methodSymbol,
                                          TR_FrontEnd * fe,
                                          TR::Compilation *comp,
                                          TR::SymbolReferenceTable *symRefTab,
                                          bool forceClassLookahead,
                                          TR_InlineBlocks *blocksToInline);

   virtual bool sameAs(TR::IlGeneratorMethodDetails & other, TR_FrontEnd *fe);

   bool sameMethod(TR::IlGeneratorMethodDetails & other);

   void print(TR_FrontEnd *fe, TR::FILE *file);

   virtual void printDetails(TR_FrontEnd *fe, TR::FILE *file);

protected:

   // All data across subclasses of IlGeneratorMethodDetails MUST be stored in the base class
   //    (using a union to save space across the hierarchy where possible)
   // Primary reason is that an embedded instance of this class is stored in MethodToBeCompiled so that instance
   //    must be able to transmute itself into any kind of IlGeneratorMethodDetails in place (i.e. via placement new)

   J9Method *_method;
   union
      {
      J9Class *_class;
      int32_t _byteCodeIndex;
      struct
         {
         uintptrj_t *_handleRef;
         uintptrj_t *_argRef;
         } _methodHandleData;
      } _data;

   };

// Replay compilation support that must not be used by anyone else because it breaks encapsulation
//
class IlGeneratorMethodDetailsOverrideForReplay
   {
public:

   // When replay compilation initializes it needs to redirect the existing
   // details object to compile the requested replay method.
   //
   static void changeMethod(TR::IlGeneratorMethodDetails & details, J9Method *newMethod);
   };




}

#endif
