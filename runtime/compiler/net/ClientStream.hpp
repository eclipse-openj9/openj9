/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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

#ifndef CLIENT_STREAM_H
#define CLIENT_STREAM_H

#include <google/protobuf/io/zero_copy_stream_impl.h>
#include "compile/CompilationTypes.hpp"
#include "ilgen/J9IlGeneratorMethodDetails.hpp"
#include "net/ProtobufTypeConvert.hpp"
#include "net/CommunicationStream.hpp"

#if defined(JITSERVER_ENABLE_SSL)
#include <openssl/ssl.h>
class SSLOutputStream;
class SSLInputStream;
#endif

namespace JITServer
{
enum VersionCheckStatus
   {
   NOT_DONE = 0,
   PASSED = 1,
   };

/**
   @class ClientStream
   @brief Implementation of the communication API for a client asking for remote JIT compilations

   Typical sequence executed by a client is:
   (1) Establish a connection to the JITServer by creating a ClientStream object
         client = new (PERSISTENT_NEW) JITServer::ClientStream(compInfo->getPersistentInfo());
   (2) Determine compilation parameters and send the compilation request over the network
          client->buildCompileRequest(method, clazz, .................);
   (3) Handle any queries from the server
       Typical seq: (3.1) client->read(); (3.2) client->getRecvData(...); (3.3) client->write(...);
          while(!handleServerMessage(client, frontend));
   (4) Read compilation result
          auto recv = client->getRecvData<uint32_t, std::string, std::string, .......>();
   (5) install compiled code into code cache
*/
class ClientStream : public CommunicationStream
   {
public:
   /**
       @brief Function called to perform static initialization of ClientStream

       This is called during startup from rossa.cpp.
       Creates SSL context, loads certificates and keys.
       Only needs to be done once during JVM initialization.

       Returns 0 if successful;; Otherwise, returns -1.
   */
   static int static_init(TR::PersistentInfo *info);

   explicit ClientStream(TR::PersistentInfo *info);
   virtual ~ClientStream()
      {
      _numConnectionsClosed++;
      }

   /**
      @brief Send a compilation request to the JITServer

      As a side-effect, this function may also embed version information in the message
      if this is the first message sent after a connection request.
   */
   template <typename... T>
   void buildCompileRequest(T... args)
      {
      if (getVersionCheckStatus() == NOT_DONE)
         {
         _cMsg.set_version(getJITServerVersion());
         write(MessageType::compilationRequest, args...);
         _cMsg.clear_version();
         }
      else // getVersionCheckStatus() == PASSED
         {
         _cMsg.clear_version(); // the compatibility check is done. We clear the version to save message size.
         write(MessageType::compilationRequest, args...);
         }
      }

   /**
      @brief Send a message to the JITServer

      @param [in] type Message type
      @param [in] args Additional arguments sent to the JITServer
   */
   template <typename ...T>
   void write(MessageType type, T... args)
      {
      _cMsg.set_type(type);
      setArgs<T...>(_cMsg.mutable_data(), args...);

      writeBlocking(_cMsg);
      }

   /**
      @brief Read a message from the server

      The read operation is blocking (subject to a timeout)
 
      @return Returns the type of the message being received
   */
   MessageType read()
      {
      readBlocking(_sMsg);
      return _sMsg.type();
      }

   /**
       @brief Extract the data from the received message and return it
   */
   template <typename ...T>
   std::tuple<T...> getRecvData()
      {
      return getArgs<T...>(_sMsg.mutable_data());
      }

   /**
      @brief Send an error message to the JITServer

      Examples of error messages include 'compilationInterrupted' (e.g. when class unloading happens),
      'clientSessionTerminate' (e.g. when the client is about to exit), 
      and 'connectionTerminate' (e.g. when the client is closing the connection)
   */
   template <typename ...T>
   void writeError(MessageType type, T... args)
      {
      _cMsg.set_type(type);
      if (type == MessageType::compilationInterrupted || type == MessageType::connectionTerminate)
         _cMsg.mutable_data()->clear_data();
      else
         setArgs<T...>(_cMsg.mutable_data(), args...);
      writeBlocking(_cMsg);
      }

   VersionCheckStatus getVersionCheckStatus()
      {
      return _versionCheckStatus;
      }

   void setVersionCheckStatus()
      {
      _versionCheckStatus = PASSED;
      }

   /**
      @brief Function called when JITServer was discovered to be incompatible with the client
   */
   static void incrementIncompatibilityCount(OMRPortLibrary *portLibrary)
      {
      OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
      _incompatibilityCount += 1;
      _incompatibleStartTime = omrtime_current_time_millis();
      }

   /**
       @brief Function that answers whether JITServer is compatible with the client

       Note that the discovery of the incompatibility is done in the first message
       that the client sends to the server. This function is actually called to
       determine if we need to try the compatibilty check again. The idea is that
       the incompatible server could be killed and another one (compatible) could be
       instantiated. If enough time has passed since the server was found to be 
       incompatible, then the client should try again. "Enough time" is defined as a
       constant 10 second interval.
   */
   static bool isServerCompatible(OMRPortLibrary *portLibrary)
      {
      OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
      uint64_t current_time = omrtime_current_time_millis();
      // the client periodically checks whether the server is compatible or not
      // the retry interval is defined by RETRY_COMPATIBILITY_INTERVAL_MS
      // we set _incompatibilityCount to 0 when it's time to retry for compatibility check
      // otherwise, check if we exceed the number of incompatible connections
      if ((current_time - _incompatibleStartTime) > RETRY_COMPATIBILITY_INTERVAL_MS)
         _incompatibilityCount = 0;

      return _incompatibilityCount < INCOMPATIBILITY_COUNT_LIMIT;
      }

   // Statistics
   static int getNumConnectionsOpened() { return _numConnectionsOpened; }
   static int getNumConnectionsClosed() { return _numConnectionsClosed; }

private:
   static int _numConnectionsOpened;
   static int _numConnectionsClosed;
   VersionCheckStatus _versionCheckStatus; // indicates whether a version checking has been performed
   static int _incompatibilityCount;
   static uint64_t _incompatibleStartTime; // Time when version incomptibility has been detected
   static const uint64_t RETRY_COMPATIBILITY_INTERVAL_MS; // (ms) When we should perform again a version compatibilty check
   static const int INCOMPATIBILITY_COUNT_LIMIT;

#if defined(JITSERVER_ENABLE_SSL)
   static SSL_CTX *_sslCtx;
#endif
   };

}

#endif // CLIENT_STREAM_H
