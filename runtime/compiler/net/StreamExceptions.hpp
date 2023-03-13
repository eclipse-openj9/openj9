/*******************************************************************************
 * Copyright IBM Corp. and others 2018
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
#ifndef STREAM_EXCEPTIONS_H
#define STREAM_EXCEPTIONS_H

#include <string>
#include "net/MessageTypes.hpp"
#include "infra/Assert.hpp"

namespace JITServer
{
class StreamFailure: public virtual std::exception
   {
public:
   StreamFailure() : _message("Generic stream failure"), _retryConnectionImmediately(false) { }
   StreamFailure(const std::string &message) : _message(message), _retryConnectionImmediately(false) { }
   StreamFailure(const std::string &message, bool shouldRetry) : _message(message), _retryConnectionImmediately(shouldRetry) { }
   virtual const char* what() const throw() { return _message.c_str(); }
   bool retryConnectionImmediately() const { return _retryConnectionImmediately; }
private:
   std::string _message;
   bool _retryConnectionImmediately;
   };

class StreamInterrupted: public virtual std::exception
   {
public:
   StreamInterrupted() { }
   virtual const char* what() const throw()
      {
      return "Compilation interrupted at JITClient's request";
      }
   };

class StreamConnectionTerminate: public virtual std::exception
   {
public:
   StreamConnectionTerminate() { }
   virtual const char* what() const throw()
      {
      return "Connection terminated at JITClient's request";
      }
   };

class StreamClientSessionTerminate: public virtual std::exception
   {
public:
   StreamClientSessionTerminate(uint64_t clientId) : _clientId(clientId)
      {
      _message = "JITClient session " + std::to_string(_clientId) + " terminated at JITClient's request";
      }
   virtual const char* what() const throw()
      {
      return _message.c_str();
      }
   uint64_t getClientId() const
      {
      return _clientId;
      }
private:
   std::string _message;
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
   StreamMessageTypeMismatch() : _message("JITServer/JITClient message type mismatch detected") { }
   StreamMessageTypeMismatch(MessageType serverType, MessageType clientType)
      {
      const char *expectedName = (serverType < MessageType_MAXTYPE) ? messageNames[serverType] : "";
      const char *receivedName = (clientType < MessageType_MAXTYPE) ? messageNames[clientType] : "";
      _message = "JITServer expected message type " + std::to_string(serverType) + " " + expectedName +
                 " received " + std::to_string(clientType) + " " + receivedName;
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
   StreamVersionIncompatible() : _message("JITServer/JITClient incompatibility detected") { }
   StreamVersionIncompatible(uint64_t serverVersion, uint64_t clientVersion)
      {
      _message = "JITServer expected version " + std::to_string(serverVersion) + " received " + std::to_string(clientVersion);
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
   ServerCompilationFailure() : _message("Generic JITServer compilation failure") { }
   ServerCompilationFailure(const std::string &message) : _message(message) { }
   virtual const char* what() const throw() { return _message.c_str(); }
private:
   std::string _message;
   };
}
#endif // STREAM_EXCEPTIONS_H
