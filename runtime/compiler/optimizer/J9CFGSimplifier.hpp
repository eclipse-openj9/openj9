/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#ifndef J9_CFGSIMPLIFIER_INCL
#define J9_CFGSIMPLIFIER_INCL

#include "optimizer/OMRCFGSimplifier.hpp"

namespace J9
{

// Performs local common subexpression elimination within
// a basic block.
//

class CFGSimplifier : public OMR::CFGSimplifier
   {
   public:

   CFGSimplifier(TR::OptimizationManager *manager) : OMR::CFGSimplifier(manager)
      {}

   protected:
   /**
    * \brief
    *    This virtual function calls individual routines to try to match different `if` control flow structures 
    *    for simplification.
    *
    * \parm needToDuplicateTree
    *    Boolean to indicate whether or not to duplicate node.
    *
    * \return Boolean that indicates whether tranformation is performed based on a matched pattern
    */
   virtual bool simplifyIfPatterns(bool needToDuplicateTree);

   /**
    * \brief
    *    This function tries to match an `ifacmpeq/ifacmpne` of NULL node with a throw of an NPE exception 
    *    for the unresolved case and replace with a NULLCHK node
    *
    * \parm needToDuplicateTree
    *    Boolean to indicate whether or not to duplicate node.
    *
    * \return Boolean that indicates true if tranformation is performed based on a matched pattern.
    */
   bool simplifyUnresolvedRequireNonNull(bool needToDuplicateTree);

   /**
    * \brief
    *    This function tries to match an `ifacmpeq/ifacmpne` of NULL node with a throw of an NPE exception 
    *    for the resolved case and replace with a NULLCHK node
    *
    * \parm needToDuplicateTree
    *    Boolean to indicate whether or not to duplicate node.
    *
    * \return Boolean that indicates true if tranformation is performed based on a matched pattern.
    */
   bool simplifyResolvedRequireNonNull(bool needToDuplicateTree);

   };

}

#endif
