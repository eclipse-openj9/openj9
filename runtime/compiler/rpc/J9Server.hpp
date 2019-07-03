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

#ifndef J9_SERVER_H
#define J9_SERVER_H

#include <google/protobuf/io/zero_copy_stream_impl.h> 
#include "rpc/ProtobufTypeConvert.hpp"
#include "rpc/J9Stream.hpp"
#include "env/CHTable.hpp"

#if defined(JITAAS_ENABLE_SSL)
#include <openssl/ssl.h>
class SSLOutputStream;
class SSLInputStream;
#endif
class TR_ResolvedJ9Method;

namespace JITServer
{
class ServerStream : CommunicationStream
   {
public:
#if defined(JITAAS_ENABLE_SSL)
   ServerStream(int connfd, BIO *ssl, uint32_t timeout);
#else
   ServerStream(int connfd, uint32_t timeout);
#endif
   virtual ~ServerStream() 
      {
      _numConnectionsClosed++;
      }

   template <typename ...T>
   void write(MessageType type, T... args)
      {
      setArgs<T...>(_sMsg.mutable_data(), args...);
      _sMsg.set_type(type);
      writeBlocking(_sMsg);
      }

   template <typename ...T>
   std::tuple<T...> read()
      {
      readBlocking(_cMsg);
      if (_cMsg.type() == MessageType::compilationAbort)
         throw StreamCancel(_cMsg.type());
      // We are expecting the response type (_cMsg.type()) to be the same as the request type (_sMsg.type())
      if (_cMsg.type() != _sMsg.type())
         throw StreamMessageTypeMismatch(_sMsg.type(), _cMsg.type());
         
      return getArgs<T...>(_cMsg.mutable_data());
      }

   template <typename... T>
   std::tuple<T...> readCompileRequest()
      {
      readBlocking(_cMsg);
      if (_cMsg.type() == JITServer::MessageType::clientTerminate)
         {
         uint64_t clientId = std::get<0>(getRecvData<uint64_t>());
         throw JITServer::StreamCancel(_cMsg.type(), clientId);
         }
      if (_cMsg.version() != 0 && _cMsg.version() != getJITaaSVersion())
         {
         throw StreamVersionIncompatible(getJITaaSVersion(), _cMsg.version());
         }
      if (_cMsg.type() != JITServer::MessageType::compilationRequest)
         {
         throw JITServer::StreamMessageTypeMismatch(JITServer::MessageType::compilationRequest, _cMsg.type());
         }
      return getArgs<T...>(_cMsg.mutable_data());
      }
   
   template <typename... T>
   std::tuple<T...> getRecvData()
      {
      return getArgs<T...>(_cMsg.mutable_data());
      }

   void finishCompilation(uint32_t statusCode, std::string codeCache = "", std::string dataCache = "", 
                          CHTableCommitData chTableData = {},
                          std::vector<TR_OpaqueClassBlock*> classesThatShouldNotBeNewlyExtended = {},
                          std::string logFileStr = "", std::string symbolToIdStr = "",
                          std::vector<TR_ResolvedJ9Method*> = {});
   void setClientId(uint64_t clientId)
      {
      _clientId = clientId;
      }
   uint64_t getClientId() const
      {
      return _clientId;
      }

   static int _numConnectionsOpened;
   static int _numConnectionsClosed;

private:
   uint32_t _msTimeout;
   uint64_t _clientId;
   };


// Abstract class defining the interface for the compilation handler
// Typically, an user would derive this class and provide an implementation for "compile()"
// An instance of the derived class needs to be passed to serveRemoteCompilationRequests which internally calls "compile()"
class BaseCompileDispatcher
   {
public:
   virtual void compile(ServerStream *stream) = 0;
   };


void serveRemoteCompilationRequests(BaseCompileDispatcher *compiler, TR::PersistentInfo *info);

}

#endif // J9_SERVER_H
