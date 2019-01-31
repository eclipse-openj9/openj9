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

#include "il/symbol/J9StaticSymbol.hpp"

#include <stddef.h>
#include "env/TRMemory.hpp"
#include "il/Symbol.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"

#include "il/symbol/StaticSymbol_inlines.hpp"

template <typename AllocatorType>
TR::StaticSymbol * J9::StaticSymbol::createRecognized(AllocatorType m, TR::DataType d, TR::Symbol::RecognizedField f)
   {
   TR::StaticSymbol * sym = new (m) TR::StaticSymbol(d);
   sym->makeRecognized(f);
   return sym;
   }

template <typename AllocatorType>
TR::StaticSymbol * J9::StaticSymbol::createMethodTypeTableEntry(AllocatorType m, int32_t methodTypeIndex)
   {
   TR::StaticSymbol * sym = new (m) TR::StaticSymbol(TR::Address);
   sym->makeMethodTypeTableEntry(methodTypeIndex);
   return sym;
   }


//Explicit Instantiations
template TR::StaticSymbol * J9::StaticSymbol::createRecognized(TR_HeapMemory m, TR::DataType d, TR::Symbol::RecognizedField f) ;
template TR::StaticSymbol * J9::StaticSymbol::createMethodTypeTableEntry(TR_HeapMemory m, int32_t methodTypeIndex) ;

template TR::StaticSymbol * J9::StaticSymbol::createRecognized(TR_StackMemory m, TR::DataType d, TR::Symbol::RecognizedField f) ;
template TR::StaticSymbol * J9::StaticSymbol::createMethodTypeTableEntry(TR_StackMemory m, int32_t methodTypeIndex) ;

template TR::StaticSymbol * J9::StaticSymbol::createRecognized(PERSISTENT_NEW_DECLARE m, TR::DataType d, TR::Symbol::RecognizedField f) ;
template TR::StaticSymbol * J9::StaticSymbol::createMethodTypeTableEntry(PERSISTENT_NEW_DECLARE m, int32_t methodTypeIndex) ;



