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

#ifndef J9_SERVER_H
#define J9_SERVER_H

#include <grpc++/grpc++.h>
#include "rpc/StreamTypes.hpp"
#include "rpc/ProtobufTypeConvert.hpp"
#include "env/CHTable.hpp"

namespace JITaaS
{

class J9ServerStream
   {
public:
   J9ServerStream(size_t streamNum,
                  J9CompileService::AsyncService *service,
                  grpc::ServerCompletionQueue *notif,
                  uint32_t timeout)
      : _streamNum(streamNum), _service(service), _notif(notif), _msTimeout(timeout)
      {
      acceptNewRPC();
      }

   template <typename ...T>
   void write(J9ServerMessageType type, T... args)
      {
      setArgs<T...>(_sMsg.mutable_data(), args...);
      _sMsg.set_type(type);
      writeBlocking();
      }
   template <typename ...T>
   std::tuple<T...> read()
      {
      readBlocking();
      if (!_cMsg.status())
         throw StreamCancel();
      return getArgs<T...>(_cMsg.mutable_data());
      }

   void finish();
   void cancel();   // Same as finish, but with Status::CANCELLED
   void finishCompilation(uint32_t statusCode, std::string codeCache = "", std::string dataCache = "", CHTableCommitData chTableData = {}, std::vector<TR_OpaqueClassBlock*> classesThatShouldNotBeNewlyExtended = {}, std::string logFileStr = "");
   void acceptNewRPC();
   void setClientId(uint64_t clientId)
      {
      _clientId = clientId;
      }
   uint64_t getClientId() const
      {
      return _clientId;
      }

private:
   void readBlocking();
   void writeBlocking();
   bool waitOnQueue();
   void drainQueue();

   uint32_t _msTimeout;
   const size_t _streamNum; // tagging for notification loop, used to identify associated CompletionQueue in vector
   grpc::ServerCompletionQueue *_notif;
   J9CompileService::AsyncService *const _service;
   std::unique_ptr<grpc::CompletionQueue> _cq;
   std::unique_ptr<J9ServerReaderWriter> _stream;
   std::unique_ptr<grpc::ServerContext> _ctx;
   uint64_t _clientId;

   // re-usable message objects
   J9ServerMessage _sMsg;
   J9ClientMessage _cMsg;
   };

//
// Inherited class is starting point for the received compilation request
//
class J9BaseCompileDispatcher
   {
public:
   virtual void compile(J9ServerStream *stream) = 0;
   };

class J9CompileServer
   {
public:
   ~J9CompileServer()
      {
      _server->Shutdown();
      _notificationQueue->Shutdown();
      }

   void buildAndServe(J9BaseCompileDispatcher *compiler, TR::PersistentInfo *info);

private:
   void serve(J9BaseCompileDispatcher *compiler, uint32_t timeout);
   bool waitOnQueue();

   std::unique_ptr<grpc::Server> _server;
   J9CompileService::AsyncService _service;
   std::unique_ptr<grpc::ServerCompletionQueue> _notificationQueue;
   std::vector<std::unique_ptr<J9ServerStream>> _streams;
   };

}

#endif // J9_SERVER_H
