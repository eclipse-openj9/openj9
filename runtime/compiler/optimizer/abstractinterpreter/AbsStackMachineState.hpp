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

#ifndef ABS_STACK_MACHINE_STATE_INCL
#define ABS_STACK_MACHINE_STATE_INCL

#include "optimizer/abstractinterpreter/AbsOpArray.hpp"
#include "optimizer/abstractinterpreter/AbsOpStack.hpp"

namespace TR {

class AbsStackMachineState
   {
   public:
   explicit AbsStackMachineState(TR::Region &region, uint32_t maxArraySize) :
         _array(new (region) TR::AbsOpArray(maxArraySize, region)),
         _stack(new (region) TR::AbsOpStack(region))
      {}

   /**
    * @brief clone an stack machine state
    *
    * @param region The memory region where the state should be allocated
    * @return the cloned state
    *
    */
   TR::AbsStackMachineState* clone(TR::Region& region) const;

   /**
    * @brief Set an abstract value at index i of the local variable array in this state.
    *
    * @param i the local variable array index
    * @param value the value to be set
    */
   void set(uint32_t i, TR::AbsValue* value) { _array->set(i, value); }

   /**
    * @brief Get the AbsValue at index i of the local variable array in this state.
    *
    * @param i the local variable array index
    * @return the abstract value
    */
   TR::AbsValue *at(uint32_t i) const { return _array->at(i); }

   /**
    * @brief Push an AbsValue to the operand stack in this state.
    *
    * @param value the value to be pushed
    */
   void push(TR::AbsValue* value) { _stack->push(value); }

   /**
    * @brief Get and pop an AbsValue off of the operand stack in this state.
    *
    * @return the abstract value
    */
   TR::AbsValue* pop() { return _stack->pop(); }

   /**
    * @brief Peek the operand stack in this state.
    *
    * @return the abstract value
    */
   TR::AbsValue* peek() const { return _stack->peek();  }

   /**
    * @brief Merge with another state. This is in-place merge.
    *
    * @param other another state to be merged with
    */
   void merge(const TR::AbsStackMachineState* other, TR::Region& region);

   size_t getStackSize() const { return _stack->size();  }
   size_t getArraySize() const { return _array->size();  }

   void print(TR::Compilation* comp) const;

   private:

   AbsStackMachineState(TR::AbsOpArray* array, TR::AbsOpStack* stack) :
         _array(array),
         _stack(stack)
      {}

   TR::AbsOpArray* _array;
   TR::AbsOpStack* _stack;
   };

}

#endif
