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

#include "env/annotations/TRNoSideFXAnnotation.hpp"
#include "il/Symbol.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "il/SymbolReference.hpp"

TR_NoSideFXAnnotation::TR_NoSideFXAnnotation(TR::Compilation *comp,TR::SymbolReference *symRef):
   TR_AnnotationBase(comp)
   {
   TR_ResolvedMethod *resolvedMethod= symRef->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod();
   TR_ASSERT(resolvedMethod,"symref NULL\n");
   TR_ASSERT(comp,"comp NULL\n");
   
   _isValid=false;
   
   J9Class * clazz = (J9Class *)resolvedMethod->containingClass();
   
   if(!loadAnnotation(clazz,kX10NoSideFx)) return;
   if(NULL == getTaggedAnnotationInfoEntry(symRef,kX10NoSideFx)) return;
   if(false)printf("Found no sfx of %s in %s\n",resolvedMethod->signature(comp->trMemory()),comp->signature());
   _isValid=true;
   }

