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

#ifndef TR_J9_I386_CODEGENERATORBASE_INCL
#define TR_J9_I386_CODEGENERATORBASE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef TRJ9_CODEGENERATORBASE_CONNECTOR
#define TRJ9_CODEGENERATORBASE_CONNECTOR

namespace J9 { namespace X86 { namespace i386 { class CodeGenerator; } } }
namespace J9 { typedef J9::X86::i386::CodeGenerator CodeGeneratorConnector; }

#else
#error J9::X86::i386::CodeGenerator expected to be a primary connector, but a J9 connector is already defined
#endif


#include "x/codegen/J9CodeGenerator.hpp"

namespace J9
{

namespace X86
{

namespace i386
{

class OMR_EXTENSIBLE CodeGenerator : public J9::X86::CodeGenerator
   {
   public:

   CodeGenerator() :
      J9::X86::CodeGenerator() {}

   TR::Linkage *createLinkage(TR_LinkageConventions lc);

   void lowerTreesPreTreeTopVisit(TR::TreeTop *tt, vcount_t visitCount);
   void lowerTreesPostTreeTopVisit(TR::TreeTop *tt, vcount_t visitCount);

   void lowerTreesPreChildrenVisit(TR::Node * parent, TR::TreeTop * treeTop, vcount_t visitCount);
   void lowerTreesPostChildrenVisit(TR::Node * parent, TR::TreeTop * treeTop, vcount_t visitCount);

   };

} // namespace i386

} // namespace X86

} // namespace J9

#endif
