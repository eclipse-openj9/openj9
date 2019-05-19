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

#ifndef TR_STATICSYMBOL_INCL
#define TR_STATICSYMBOL_INCL

#include "il/symbol/J9StaticSymbol.hpp"

#include <stdint.h>
#include "il/DataTypes.hpp"

/**
 * A symbol with an address
 */
namespace TR
{

class OMR_EXTENSIBLE StaticSymbol : public J9::StaticSymbolConnector
   {

protected:

   StaticSymbol(TR::DataType d) :
      J9::StaticSymbolConnector(d) { }

   StaticSymbol(TR::DataType d, void * address) :
      J9::StaticSymbolConnector(d,address) { }

   StaticSymbol(TR::DataType d, uint32_t s) :
      J9::StaticSymbolConnector(d, s) { }

private:

   // When adding another class to the hierarchy, add it as a friend here
   friend class J9::StaticSymbol;
   friend class OMR::StaticSymbol;

   };

}

/**
 * To isolate the addition of the _inlines.hpp file from OMR, where the OMR 
 * layer versions would be empty stubs, I've added this here. 
 */
#include "il/symbol/StaticSymbol_inlines.hpp"

#endif

