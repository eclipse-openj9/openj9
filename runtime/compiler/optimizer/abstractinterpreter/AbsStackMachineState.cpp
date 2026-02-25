/*******************************************************************************
 * Copyright IBM Corp. and others 2020
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "optimizer/abstractinterpreter/AbsStackMachineState.hpp"

TR::AbsStackMachineState* TR::AbsStackMachineState::clone(TR::Region& region) const
   {
   TR::AbsStackMachineState* copy = new (region) TR::AbsStackMachineState(_array->clone(region), _stack->clone(region));
   return copy;
   }

void TR::AbsStackMachineState::merge(const TR::AbsStackMachineState* other, TR::Region& region)
   {
   _array->merge(other->_array, region);
   _stack->merge(other->_stack, region);
   }

void TR::AbsStackMachineState::print(TR::Compilation* comp) const
   {
   traceMsg(comp, "\n|| Contents of AbsStackMachineState ||\n");
   _array->print(comp);
   _stack->print(comp);
   }
