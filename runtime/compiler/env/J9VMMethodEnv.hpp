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

#ifndef J9_VMMETHODENV_INCL
#define J9_VMMETHODENV_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_VMMETHODENV_CONNECTOR
#define J9_VMMETHODENV_CONNECTOR
namespace J9 { class VMMethodEnv; }
namespace J9 { typedef J9::VMMethodEnv VMMethodEnvConnector; }
#endif

#include "env/OMRVMMethodEnv.hpp"
#include "infra/Annotations.hpp"
#include "env/jittypes.h"


namespace J9
{

class OMR_EXTENSIBLE VMMethodEnv : public OMR::VMMethodEnvConnector
   {
public:

   bool hasBackwardBranches(TR_OpaqueMethodBlock *method);

   bool isCompiledMethod(TR_OpaqueMethodBlock *method);

   uintptr_t startPC(TR_OpaqueMethodBlock *method);

   uintptr_t bytecodeStart(TR_OpaqueMethodBlock *method);

   uint32_t bytecodeSize(TR_OpaqueMethodBlock *method);

   /**
    * @brief Given a method signature, tokenize it into its class name, method name,
    *        and type signature strings.
    *
    * @param[in] methodSignature : the char string of the full method signature to tokenize
    * @param[out] methodClass : pointer to the start of the string within the methodSignature
    *                representing the class name of the given method signature
    * @param[out] methodClassLen : length in bytes of the class name string
    * @param[out] methodName : pointer to the start of the string within the methodSignature
    *                representing the method name of the given method signature
    * @param[out] methodNameLen : length in bytes of the method name string
    * @param[out] typeSignature : pointer to the start of the string within the methodSignature
    *                representing the method type signature of the given method signature
    * @param[out] typeSignatureLen : length in bytes of the type signature string
    */
   void tokenizeSignature(
      const char * methodSignature,
      const char * &methodClass,
      uint32_t     &methodClassLen,
      const char * &methodName,
      uint32_t     &methodNameLen,
      const char * &typeSignature,
      uint32_t     &typeSignatureLen);

   };

}

#endif
