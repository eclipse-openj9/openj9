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

#ifndef HOTFIELDMARKING_INCL
#define HOTFIELDMARKING_INCL

#include <stdint.h>                           // for int32_t
#include "optimizer/Optimization.hpp"         // for Optimization
#include "optimizer/OptimizationManager.hpp"  // for OptimizationManager

namespace TR { class Block; class Node; }

class TR_HotFieldMarking : public TR::Optimization
   {
   public:
   TR_HotFieldMarking(TR::OptimizationManager *manager)
      : TR::Optimization(manager)
      {}
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_HotFieldMarking(manager);
      }

   /**
    * @brief Perform the hot field marking pass
    * @return 1 on success, 0 on failure or if hot field marking is disabled
    */
   virtual int32_t perform();
   
   /**
    * @brief Returns optimization string details
    */
   virtual const char * optDetailString() const throw();
   
   /**
    * @brief Return the hot field marking pass scaling factor based on the opt level of the current compilation
    * @return Scaling factors can be overriden by environment variables; default scaling factors of 1 for opt level warm and below, 10 for opt level hot or very hot, and 100 for opt level scorching
    */
   int32_t getUtilization();
   };

#endif
