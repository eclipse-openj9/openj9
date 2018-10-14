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

#ifndef ANNOTATIONTROPT_INCL
#define ANNOTATIONTROPT_INCL

#include "env/annotations/AnnotationBase.hpp"

#include <stdint.h>
#include "compile/CompilationTypes.hpp"
#include "il/Symbol.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"

class TR_ResolvedMethod;

class TR_OptAnnotation : public TR_AnnotationBase
   {
   public:

   bool hasAnnotation(TR::Symbol *sym);
   TR_OptAnnotation(TR::Compilation *comp,TR_ResolvedMethod *);

   // assumes isValid is true
   TR_Hotness getOptLevel() { return _optLevel;}
   int32_t getCount(){ return _count;}

   private:
   TR_Hotness      _optLevel;
   int32_t         _count;
};


#endif
