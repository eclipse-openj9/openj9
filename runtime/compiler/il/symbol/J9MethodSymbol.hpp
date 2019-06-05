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

#ifndef J9_METHODSYMBOL_INCL
#define J9_METHODSYMBOL_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_METHODSYMBOL_CONNECTOR
#define J9_METHODSYMBOL_CONNECTOR
namespace J9 { class MethodSymbol; }
namespace J9 { typedef J9::MethodSymbol MethodSymbolConnector; }
#endif

#include "il/symbol/OMRMethodSymbol.hpp"

#include <stdint.h>
#include "codegen/LinkageConventionsEnum.hpp"
#include "compile/Method.hpp"
#include "il/DataTypes.hpp"
#include "runtime/J9Runtime.hpp"

namespace TR { class Compilation; }

namespace J9
{

/**
 * Symbol for methods, along with information about the method
 */
class OMR_EXTENSIBLE MethodSymbol : public OMR::MethodSymbolConnector
   {

protected:

   MethodSymbol(TR_LinkageConventions lc = TR_Private, TR_Method * m = NULL) :
      OMR::MethodSymbolConnector(lc, m) { }

public:

   bool isPureFunction();

   TR_RuntimeHelper getVMCallHelper() { return TR_j2iTransition; } // deprecated

   bool safeToSkipNullChecks();
   bool safeToSkipBoundChecks();
   bool safeToSkipDivChecks();
   bool safeToSkipCheckCasts();
   bool safeToSkipArrayStoreChecks();
   bool safeToSkipZeroInitializationOnNewarrays();
   bool safeToSkipChecksOnArrayCopies();

   };

}

#endif

