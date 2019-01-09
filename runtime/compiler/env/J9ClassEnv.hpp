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

#ifndef J9_CLASSENV_INCL
#define J9_CLASSENV_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_CLASSENV_CONNECTOR
#define J9_CLASSENV_CONNECTOR
namespace J9 { class ClassEnv; }
namespace J9 { typedef J9::ClassEnv ClassEnvConnector; }
#endif

#include "env/OMRClassEnv.hpp"
#include "infra/Annotations.hpp"
#include "env/jittypes.h"
#include "j9.h"

namespace TR { class SymbolReference; }

namespace J9
{

class OMR_EXTENSIBLE ClassEnv : public OMR::ClassEnvConnector
   {
public:

   bool classesOnHeap() { return true; }

   bool classObjectsMayBeCollected() { return false; }

   bool romClassObjectsMayBeCollected() { return false; }

   TR_OpaqueClassBlock *getClassFromJavaLangClass(uintptrj_t objectPointer);

   J9Class *convertClassOffsetToClassPtr(TR_OpaqueClassBlock *clazzOffset);

   uintptrj_t classFlagsValue(TR_OpaqueClassBlock * classPointer);
   uintptrj_t classDepthOf(TR_OpaqueClassBlock *clazzPointer);
   uintptrj_t classInstanceSize(TR_OpaqueClassBlock * clazzPointer);

   J9ROMClass *romClassOf(TR_OpaqueClassBlock * clazz);

   bool isStringClass(TR_OpaqueClassBlock *clazz);

   bool isStringClass(uintptrj_t objectPointer);

   bool classHasIllegalStaticFinalFieldModification(TR_OpaqueClassBlock * clazzPointer);
   bool isAbstractClass(TR::Compilation *comp, TR_OpaqueClassBlock *clazzPointer);
   bool isInterfaceClass(TR::Compilation *comp, TR_OpaqueClassBlock *clazzPointer);
   bool isEnumClass(TR::Compilation *comp, TR_OpaqueClassBlock *clazzPointer, TR_ResolvedMethod *method);
   bool isPrimitiveClass(TR::Compilation *comp, TR_OpaqueClassBlock *clazz);
   bool isAnonymousClass(TR::Compilation *comp, TR_OpaqueClassBlock *clazz);
   bool isPrimitiveArray(TR::Compilation *comp, TR_OpaqueClassBlock *);
   bool isReferenceArray(TR::Compilation *comp, TR_OpaqueClassBlock *);
   bool isClassArray(TR::Compilation *comp, TR_OpaqueClassBlock *);
   bool isClassFinal(TR::Compilation *comp, TR_OpaqueClassBlock *);
   bool hasFinalizer(TR::Compilation *comp, TR_OpaqueClassBlock *classPointer);
   bool isClassInitialized(TR::Compilation *comp, TR_OpaqueClassBlock *);
   bool hasFinalFieldsInClass(TR::Compilation *comp, TR_OpaqueClassBlock *classPointer);
   bool sameClassLoaders(TR::Compilation *comp, TR_OpaqueClassBlock *, TR_OpaqueClassBlock *);
   bool isString(TR::Compilation *comp, TR_OpaqueClassBlock *clazz);
   bool isString(TR::Compilation *comp, uintptrj_t objectPointer);
   bool jitStaticsAreSame(TR::Compilation *comp, TR_ResolvedMethod * method1, int32_t cpIndex1, TR_ResolvedMethod * method2, int32_t cpIndex2);
   bool jitFieldsAreSame(TR::Compilation *comp, TR_ResolvedMethod * method1, int32_t cpIndex1, TR_ResolvedMethod * method2, int32_t cpIndex2, int32_t isStatic);

   uintptrj_t getArrayElementWidthInBytes(TR::Compilation *comp, TR_OpaqueClassBlock* arrayClass);

   uintptrj_t persistentClassPointerFromClassPointer(TR::Compilation *comp, TR_OpaqueClassBlock *clazz);
   TR_OpaqueClassBlock *objectClass(TR::Compilation *comp, uintptrj_t objectPointer);
   TR_OpaqueClassBlock *classFromJavaLangClass(TR::Compilation *comp, uintptrj_t objectPointer);

   uint16_t getStringCharacter(TR::Compilation *comp, uintptrj_t objectPointer, int32_t index);
   bool getStringFieldByName(TR::Compilation *, TR::SymbolReference *stringRef, TR::SymbolReference *fieldRef, void* &pResult);

   using OMR::ClassEnv::classNameChars;
   char *classNameChars(TR::Compilation *, TR::SymbolReference *symRef, int32_t & length);
   char *classNameChars(TR::Compilation *, TR_OpaqueClassBlock * clazz, int32_t & length);

   char *classSignature_DEPRECATED(TR::Compilation *comp, TR_OpaqueClassBlock * clazz, int32_t & length, TR_Memory *);
   char *classSignature(TR::Compilation *comp, TR_OpaqueClassBlock * clazz, TR_Memory *);

   int32_t vTableSlot(TR::Compilation *comp, TR_OpaqueMethodBlock *, TR_OpaqueClassBlock *);
   int32_t flagValueForPrimitiveTypeCheck(TR::Compilation *comp);
   int32_t flagValueForArrayCheck(TR::Compilation *comp);
   int32_t flagValueForFinalizerCheck(TR::Compilation *comp);
   };

}

#endif
