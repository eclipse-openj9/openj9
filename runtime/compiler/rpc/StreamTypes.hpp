/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

#ifndef RPC_TYPES_H
#define RPC_TYPES_H

#include "infra/Assert.hpp"

#ifdef JITAAS_USE_GRPC
#include "grpc/StreamTypes.hpp"
#endif
#ifdef JITAAS_USE_RAW_SOCKETS
#include "raw/StreamTypes.hpp"
#endif

namespace JITaaS
{
   class StreamFailure: public virtual std::exception
      {
   public:
      StreamFailure() : _message("Generic stream failure") { }
      StreamFailure(std::string message) : _message(message) { }
      virtual const char* what() const throw() { return _message.c_str(); }
   private:
      std::string _message;
      };

   class StreamCancel: public virtual std::exception
      {
   public:
      virtual const char* what() const throw() { return "compilation canceled by client"; }
      };

   class StreamOOO : public virtual std::exception
      {
      public:
         virtual const char* what() const throw() { return "Messages arriving out-of-order"; }
      };

   class StreamTypeMismatch: public virtual StreamFailure
      {
   public:
      StreamTypeMismatch(std::string message) : StreamFailure(message) { TR_ASSERT(false, "type mismatch"); }
      };

   class StreamArityMismatch: public virtual StreamFailure
      {
   public:
      StreamArityMismatch(std::string message) : StreamFailure(message) { TR_ASSERT(false, "arity mismatch"); }
      };

   class ServerCompFailure: public virtual std::exception
      {
   public:
      ServerCompFailure() : _message("Generic JITaaS server compilation failure") { }
      ServerCompFailure(std::string message) : _message(message) { }
      virtual const char* what() const throw() { return _message.c_str(); }
   private:
      std::string _message;
      };
}

#endif // RPC_TYPES_H
