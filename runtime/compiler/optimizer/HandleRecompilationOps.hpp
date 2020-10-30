/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

#ifndef HANDLERECOMPILATINOOPS_INCL
#define HANDLERECOMPILATINOOPS_INCL

#include <stdint.h>
#include "infra/Cfg.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"

/**
 * The \c TR_HandleRecompilationOps optimization is an IL generation
 * optimization whose purpose is to detect situations in which IL generation
 * is unable to generate trees that correctly represent the semantics of
 * a particular operation.  In such cases, the optimization will generate an
 * OSR requesting recompilation.  This relies on voluntary OSR and
 * recompilation both being enabled, as well as being possible at the point
 * in the trees where the operation under consideration was encountered.
 * If the optimization is unable to induce OSR at that point, it will abort
 * the compilation.
 *
 * The optimization is currently needed for some value type operations
 * involving unresolved types or unresolved fields whose types are of value
 * types.
 */
class TR_HandleRecompilationOps : public TR::Optimization
   {
   private:

   TR::ResolvedMethodSymbol *_methodSymbol;
   bool _enableTransform;

   TR_HandleRecompilationOps(TR::OptimizationManager *manager) : TR::Optimization(manager)
      {
      _methodSymbol = comp()->getOwningMethodSymbol(comp()->getCurrentMethod());
      }

   bool resolveCHKGuardsValueTypeOperation(TR::TreeTop *currTree, TR::Node *node);
   bool visitTree(TR::TreeTop *currTree);

   public:

   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_HandleRecompilationOps(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   };

#endif // HANDLERECOMPILATINOOPS_INCL
