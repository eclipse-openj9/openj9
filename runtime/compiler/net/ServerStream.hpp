/*******************************************************************************
 * Copyright (c) 2018, 2020 IBM Corp. and others
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

#ifndef SERVER_STREAM_H
#define SERVER_STREAM_H

#include "net/RawTypeConvert.hpp"
#include "net/CommunicationStream.hpp"
#include "env/VerboseLog.hpp"
#include "control/CompilationThread.hpp" // for TR::compInfoPT->getCompThreadId()
#include "control/Options.hpp"
#include "runtime/JITClientSession.hpp"
#include <openssl/ssl.h>

class SSLOutputStream;
class SSLInputStream;

namespace JITServer
{

/**
   @class ServerStream
   @brief Implementation of the communication API for a server receiving JIT compilations requests

   Typical usage:
   1) Define a compilation handler by extending JITServer::BaseCompileDispatcher and providing
      an implementation for abstract method "compile(JITServer::ServerStream *stream)"
      This method will be called when a new connection request has been received at the server.
   2) Create a dedicated thread that will listen for incoming connection requests
   3) In this thread, instantiate a CompileDispatcher from a class defined in step (1)
      E.g.:    J9CompileDispatcher handler(jitConfig);
   4) Call  "TR_Listener::serveRemoteCompilationRequests(&handler);"
      which will wait for a connection, accept the connection, create a ServerStream and call
      handler->compile(stream) for further processing, e.g. add the stream
      to a compilation queue
   5) On a different thread, extract the compilation entry from  the queue (which contains
      the stream) and wait for a compilation request:
         auto req = stream->readCompileRequest<....>();
   6) At this point the server could query the client with:
         stream->write(MessageType type, T... args);
         auto recv = stream->read<....>();
   7) When compilation is completed successfully, the server responds with finishCompilation(T... args).
      When compilation is aborted, the sever responds with writeError(uint32_t statusCode).
 */
class ServerStream : public CommunicationStream
   {
public:
   /**
      @brief Constructor of ServerStream class

      @param connfd socket descriptor for the communication channel
      @param ssl  BIO for the SSL enabled stream
      @param timeout timeout value (ms) to be set for connfd
   */
   explicit ServerStream(int connfd, BIO *ssl);
   virtual ~ServerStream()
      {
      _numConnectionsClosed++;
      _pClientSessionData = NULL;
      }

   /**
      @brief Send a message to the client

      @param [in] type Message type to be sent
      @param [in] args Variable number of additional parameters to be sent
   */
   template <typename ...Args>
   void write(MessageType type, Args... args)
      {
      if (isReadingClassUnload() &&
          isClassUnloadingAttempted() &&
          (MessageType::compilationFailure != type) &&
          (MessageType::compilationCode != type))
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "compThreadID=%d MessageType[%u] %s: throw TR::CompilationInterrupted",
               TR::compInfoPT->getCompThreadId(), type, messageNames[type]);

         throw TR::CompilationInterrupted();
         }

      _sMsg.setType(type);
      setArgsRaw<Args...>(_sMsg, args...);
      writeMessage(_sMsg);
      }

   /**
      @brief Read a message from the client

      The read operation is blocking, subject to a timeout.
      If the message received is `compilationInterrupted` then an exception of type StreamInterrupted is thrown.
      If the message received is `connectionTerminate` then an exception of type StreamConnectionTerminate is thrown.
      If the server detects an incompatibility with the client then a StreamMessageTypeMissmatch
      exception is thrown.
      Otherwise, the arguments sent by the client are returned as a tuple

      @return Returns a tuple of arguments sent by the client
   */
   template <typename ...T>
   std::tuple<T...> read()
      {
      readMessage(_cMsg);
      switch (_cMsg.type())
         {
         case MessageType::compilationInterrupted:
            {
            throw StreamInterrupted();
            }
         case MessageType::connectionTerminate:
            {
            throw StreamConnectionTerminate();
            }
         default:
            {
            // We are expecting the response type (_cMsg.type()) to be the same as the request type (_sMsg.type())
            if (_cMsg.type() != _sMsg.type())
               throw StreamMessageTypeMismatch(_sMsg.type(), _cMsg.type());
            }
         }
      return getArgsRaw<T...>(_cMsg);
      }

   /**
      @brief Function to read the compilation request from a client

      This is the first type of message received after a connection is established.
      The number and position of parameters in the template must match the
      the one sent by the client. In order to ensure this, the client will embed
      version information in the first message it sends after a connection is established.
      The server will check whether its version matches the client's version and throw
      `StreamVersionIncompatible` if it doesn't.

      Exceptions thrown: StreamConnectionTerminate, StreamClientSessionTerminate, StreamVersionIncompatible, StreamMessageTypeMismatch

      @return Returns a tuple with information sent by the client
   */
   template <typename... T>
   std::tuple<T...> readCompileRequest()
      {
      readMessage(_cMsg);
      if (_cMsg.fullVersion() != 0 && _cMsg.fullVersion() != getJITServerFullVersion())
         {
         throw StreamVersionIncompatible(getJITServerFullVersion(), _cMsg.fullVersion());
         }

      switch (_cMsg.type())
         {
         case MessageType::connectionTerminate:
            {
            throw StreamConnectionTerminate();
            }
         case MessageType::clientSessionTerminate:
            {
            uint64_t clientId = std::get<0>(getRecvData<uint64_t>());
            throw StreamClientSessionTerminate(clientId);
            }
         case MessageType::compilationRequest:
            {
            return getArgsRaw<T...>(_cMsg);
            }
         default:
            {
            throw StreamMessageTypeMismatch(MessageType::compilationRequest, _cMsg.type());
            }
         }
      }

   /**
      @brief Extract the data from the received message and return it
   */
   template <typename... T>
   std::tuple<T...> getRecvData()
      {
      return getArgsRaw<T...>(_cMsg);
      }

   /**
      @brief Function invoked by server when compilation is completed successfully

      This should be the last message sent by a server as a response to a compilation request.
      It includes a variable number of parameters with compilation artifacts (including the compiled body).
   */
   template <typename... T>
   void finishCompilation(T... args)
      {
      try
         {
         write(MessageType::compilationCode, args...);
         }
      catch (std::exception &e)
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Could not finish compilation: %s", e.what());
         }
      }

   /**
      @brief Function invoked by server when compilation is aborted
   */
   void writeError(uint32_t statusCode)
      {
      try
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "compThreadID=%d MessageType::compilationFailure: statusCode %u",
                  TR::compInfoPT->getCompThreadId(), statusCode);
         write(MessageType::compilationFailure, statusCode);
         }
      catch (std::exception &e)
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Could not write error code: %s", e.what());
         }
      }

   void setClientId(uint64_t clientId)
      {
      _clientId = clientId;
      }
   uint64_t getClientId() const
      {
      return _clientId;
      }

   void setClientData(ClientSessionData *pClientData)
      {
      _pClientSessionData = pClientData;
      }

   volatile bool isReadingClassUnload()
      {
      return (_pClientSessionData) ? _pClientSessionData->isReadingClassUnload() : false;
      }

   volatile bool isClassUnloadingAttempted()
      {
      return (_pClientSessionData) ? _pClientSessionData->isClassUnloadingAttempted() : false;
      }

   // Statistics
   static int getNumConnectionsOpened() { return _numConnectionsOpened; }
   static int getNumConnectionsClosed() { return _numConnectionsClosed; }

private:
   static int _numConnectionsOpened;
   static int _numConnectionsClosed;
   uint64_t _clientId;  // UID of client connected to this communication stream
   ClientSessionData *_pClientSessionData;
   };


}

#endif // SERVER_STREAM_H
