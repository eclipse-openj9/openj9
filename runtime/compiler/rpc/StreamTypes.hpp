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

#include "rpc/gen/compile.pb.h"

#ifndef RPC_TYPES_H
#define RPC_TYPES_H

#include "infra/Assert.hpp"

namespace JITaaS
{
class StreamFailure: public virtual std::exception
   {
public:
   StreamFailure() : _message("Generic stream failure") { }
   StreamFailure(const std::string &message) : _message(message) { }
   virtual const char* what() const throw() { return _message.c_str(); }
private:
   std::string _message;
   };

class StreamCancel: public virtual std::exception
   {
public:
   StreamCancel(MessageType type, uint64_t clientId=0) : _type(type), _clientId(clientId) { }
   virtual const char* what() const throw()
      {
      if (_type == MessageType::clientTerminate)
         return "Client session terminated at client's request";
      else
         return "Compilation cancelled by client";
      }
   MessageType getType() const
      {
      return _type;
      }
   uint64_t getClientId() const
      {
      return _clientId;
      }
private:
   MessageType _type;
   uint64_t _clientId;
   };

class StreamOOO : public virtual std::exception
   {
   public:
      virtual const char* what() const throw() { return "Messages arriving out-of-order"; }
   };

class StreamTypeMismatch: public virtual StreamFailure
   {
public:
   StreamTypeMismatch(const std::string &message) : StreamFailure(message) { TR_ASSERT(false, "Type mismatch: %s", message.c_str()); }
   };

class StreamMessageTypeMismatch: public virtual std::exception
   {
public:
   StreamMessageTypeMismatch(MessageType serverType, MessageType clientType)
      {
      _message = "server expected mesasge type " + std::to_string(serverType) + " received " + std::to_string(clientType);
      }
   virtual const char* what() const throw() 
      {
      return _message.c_str();
      }
private:
   std::string _message;
   };

class StreamVersionIncompatible: public virtual std::exception
   {
public:
   StreamVersionIncompatible() : _message("client/server incompatibility detected") { }
   StreamVersionIncompatible(uint64_t serverVersion, uint64_t clientVersion)
      {
      _message = "server expected version " + std::to_string(serverVersion) + " received " + std::to_string(clientVersion);
      }
   virtual const char* what() const throw()
      {
      return _message.c_str();
      }
private:
   std::string _message;
   };

class StreamArityMismatch: public virtual StreamFailure
   {
public:
   StreamArityMismatch(const std::string &message) : StreamFailure(message) { TR_ASSERT(false, "Arity mismatch: %s", message.c_str()); }
   };

class ServerCompilationFailure: public virtual std::exception
   {
public:
   ServerCompilationFailure() : _message("Generic JITaaS server compilation failure") { }
   ServerCompilationFailure(const std::string &message) : _message(message) { }
   virtual const char* what() const throw() { return _message.c_str(); }
private:
   std::string _message;
   };
}
#endif // RPC_TYPES_H
