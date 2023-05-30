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

#ifndef ClassLookahead_h
#define ClassLookahead_h

#include "compile/Compilation.hpp"
#include "env/CHTable.hpp"
#include "env/VMJ9.h"

class TR_ClassLookahead
   {
public:
   TR_ALLOC(TR_Memory::ClassLookahead)

   TR_ClassLookahead(TR_PersistentClassInfo *, TR_FrontEnd *, TR::Compilation *, TR::SymbolReferenceTable *);
   int32_t perform();

   TR_PersistentArrayFieldInfo *getExistingArrayFieldInfo(TR::Symbol *, TR::SymbolReference *);
   TR_PersistentFieldInfo *getExistingFieldInfo(TR::Symbol *, TR::SymbolReference *, bool canMorph = true);

   void  findInitializerMethods(List<TR_ResolvedMethod> *, List<TR::ResolvedMethodSymbol> *, List<TR::ResolvedMethodSymbol> *, TR::ResolvedMethodSymbol **, bool *peekFailedForAnyMethod);
   void initializeFieldInfo();
   void updateFieldInfo();
   void makeInfoPersistent();

   bool examineNode(TR::TreeTop *, TR::Node *, TR::Node *, int32_t, TR::Node *, vcount_t);
   void invalidateIfEscapingLoad(TR::TreeTop *, TR::Node *, TR::Node *, int32_t, TR::Node *);
   bool isProperFieldAccess(TR::Node *);
   bool isPrivateFieldAccess(TR::Node *);

   TR::Compilation * comp()           {return _compilation;}
   TR_FrontEnd *    fe()             {return _fe;}
   TR_J9VMBase * fej9()              {return (TR_J9VMBase *)_fe; }
   TR_Debug   * getDebug()       {return _compilation->getDebug();}

   // Checking NaN
   //void TR_ClassLookahead::checkAndInvalidateNonNaNProperty(TR::Node *, TR_PersistentFieldInfo *);
   //int32_t TR_ClassLookahead::mustNotContainNaN(TR::Node *);

   bool findMethod(List<TR::ResolvedMethodSymbol> *, TR::ResolvedMethodSymbol *);

   static char *getFieldSignature(TR::Compilation *, TR::Symbol *, TR::SymbolReference *, int32_t &length);

private:
   // data
   //
   TR_FrontEnd *                           _fe;
   TR::Compilation *                  _compilation;
   TR::SymbolReferenceTable *         _symRefTab;
   TR_OpaqueClassBlock *             _classPointer;
   TR_PersistentClassInfoForFields * _classFieldInfo;
   TR_PersistentClassInfo          * _classInfo;
   TR::ResolvedMethodSymbol *         _currentMethodSymbol;
   bool                              _inFirstBlock;
   bool                              _inInitializerMethod;
   bool                              _inFirstInitializerMethod;
   bool                              _inClassInitializerMethod;
   bool                              _traceIt;
   };

#endif






