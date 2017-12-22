/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#ifndef J9_ALIASBUILDER_INCL
#define J9_ALIASBUILDER_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_ALIASBUILDER_CONNECTOR
#define J9_ALIASBUILDER_CONNECTOR
namespace J9 { class AliasBuilder; }
namespace J9 { typedef J9::AliasBuilder AliasBuilderConnector; }
#endif

#include "compile/OMRAliasBuilder.hpp"

#include "infra/BitVector.hpp"
#include "infra/Array.hpp"

namespace TR { class SymbolReferenceTable; }
namespace TR { class Compilation; }

namespace J9
{

class AliasBuilder : public OMR::AliasBuilderConnector
   {

public:

   AliasBuilder(TR::SymbolReferenceTable *symRefTab, size_t sizeHint, TR::Compilation *c);

   void createAliasInfo();

   TR_Array<TR_BitVector *> & userFieldSymRefNumbers() { return _userFieldSymRefNumbers; }

   TR_BitVector & tenantDataMetaSymRefs() {return _tenantDataMetaSymRefs; }
   TR_BitVector & callSiteTableEntrySymRefs() { return _callSiteTableEntrySymRefs; }
   TR_BitVector & unresolvedShadowSymRefs() { return _unresolvedShadowSymRefs; }
   TR_BitVector & methodTypeTableEntrySymRefs() { return _methodTypeTableEntrySymRefs; }

   TR_BitVector * methodAliases(TR::SymbolReference *);
   TR_Array<TR_BitVector *> &immutableConstructorDefAliases() { return _immutableConstructorDefAliases; }
   TR_Array<TR_BitVector *> &userFieldMethodDefAliases() { return _userFieldMethodDefAliases; }

private:

   TR_Array<TR_BitVector *> _userFieldSymRefNumbers;
   TR_BitVector _tenantDataMetaSymRefs;
   TR_BitVector _callSiteTableEntrySymRefs;
   TR_BitVector _unresolvedShadowSymRefs;
   TR_BitVector _methodTypeTableEntrySymRefs;
   TR_Array<TR_BitVector *> _immutableConstructorDefAliases;

   };

}

#endif
