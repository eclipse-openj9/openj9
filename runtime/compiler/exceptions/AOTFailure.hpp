/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef AOT_FAILURE_HPP
#define AOT_FAILURE_HPP

#pragma once

#include "compile/CompilationException.hpp"
#include "exceptions/RuntimeFailure.hpp"

namespace J9 {

/**
 * AOT Has Invoke Handle exception type.
 *
 * Thrown when a method that has an invoke handle is AOT Compiled.
 */
class AOTHasInvokeHandle : public virtual TR::RecoverableILGenException
   {
   virtual const char* what() const throw() { return "AOT Has Invoke Handle"; }
   };

/**
 * AOT Has Invoke VarHandle exception type.
 *
 * Thrown when a method that has an invoke varhandle is AOT Compiled.
 */
class AOTHasInvokeVarHandle : public virtual TR::RecoverableILGenException
   {
   virtual const char* what() const throw() { return "AOT Has Invoke VarHandle"; }
   };

/**
 * AOT Has Invoke Special in Interface exception type.
 *
 * Thrown when a method that has an invoke special in an interface is AOT Compiled.
 */
class AOTHasInvokeSpecialInInterface : public virtual TR::RecoverableILGenException
   {
   virtual const char* what() const throw() { return "AOT Has Invoke Special in Interface"; }
   };

/**
 * AOT Has Constant Dynamic exception type.
 *
 * Thrown when a method that has a constant dynamic is AOT Compiled.
 */
class AOTHasConstantDynamic : public virtual TR::RecoverableILGenException
   {
   virtual const char* what() const throw() { return "AOT Has Constant Dynamic"; }
   };

/**
 * AOT Has Method Handle Constant exception type.
 *
 * Thrown when a method that has a method handle constant is AOT Compiled.
 */
class AOTHasMethodHandleConstant : public virtual TR::RecoverableILGenException
   {
   virtual const char* what() const throw() { return "AOT Has Method Handle Constant"; }
   };

/**
 * AOT Has Method Type Constant exception type.
 *
 * Thrown when a method that has a method type constant is AOT Compiled.
 */
class AOTHasMethodTypeConstant : public virtual TR::RecoverableILGenException
   {
   virtual const char* what() const throw() { return "AOT Has Method Type Constant"; }
   };

/**
 * AOT Has patched CP constant exception type.
 *
 * Thrown when a method that has a constant object in CP entry patched to a different type is AOT Compiled.
 */
class AOTHasPatchedCPConstant: public virtual TR::RecoverableILGenException
   {
   virtual const char* what() const throw() { return "AOT Has Patched CP Constant"; }
   };

/**
 * AOT Relocation Failure exception type.
 *
 * Thrown when an AOT compilation fails in the relocation phase.
 */
class AOTRelocationFailed : public virtual RuntimeFailure
   {
   virtual const char* what() const throw() { return "AOT Relocation Failed"; }
   };

/**
 * AOT Symbol Validation Failure exception type.
 *
 * Thrown when an AOT validation record that should succeed fails.
 */
class AOTSymbolValidationManagerFailure : public virtual TR::CompilationException
   {
   virtual const char* what() const throw() { return "AOT Symbol Validation Manager Failure"; }
   };

/**
 * Thrown when certain code path doesn't fully support AOT.
 */
class AOTNoSupportForAOTFailure : public virtual TR::CompilationException
   {
   virtual const char* what() const throw() { return "This code doesn't support AOT"; }
   };

/**
 * AOT Relocation Record Generation Failure exception type.
 *
 * Thrown when an AOT compilation fails in relocation record generation phase.
 */
class AOTRelocationRecordGenerationFailure: public virtual TR::CompilationException
   {
   virtual const char* what() const throw() { return "AOT Relocation Record Generation Failed"; }
   };

class AOTThunkPersistenceFailure: public virtual TR::CompilationException
   {
   virtual const char* what() const throw() { return "Thunk persistence to SCC failed"; }
   };

#if defined(J9VM_OPT_JITSERVER)
/**
 * AOT Cache Deserialization Failure exception type.
 *
 * Thrown when the client JVM fails to deserialize an AOT method received from the JITServer AOT cache.
 */
class AOTCacheDeserializationFailure : public virtual RuntimeFailure
   {
   virtual const char *what() const throw() { return "AOT cache deserialization failure"; }
   };

/**
 * JITServer AOT Deserializer Reset exception type.
 *
 * Thrown when a concurrent JITServer AOT deserializer reset has been detected.
 */
class AOTDeserializerReset : public virtual RuntimeFailure
   {
   virtual const char *what() const throw() { return "AOT deserializer reset"; }
   };
#endif /* defined(J9VM_OPT_JITSERVER) */
}

#endif // AOT_FAILURE_HPP
