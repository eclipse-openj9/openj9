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
#include <openssl/ssl.h>
#include "rpc/ProtobufTypeConvert.hpp"
#include "rpc/J9Stream.hpp"
#include "env/CHTable.hpp"

class SSLOutputStream;
class SSLInputStream;
class TR_ResolvedJ9Method;

namespace JITaaS
{
class J9ServerStream : J9Stream
   {
public:
   J9ServerStream(int connfd, BIO *ssl, uint32_t timeout);
   virtual ~J9ServerStream() 
      {
      _numConnectionsClosed++;
      }

   template <typename ...T>
   void write(J9ServerMessageType type, T... args)
      {
      setArgs<T...>(_sMsg.mutable_data(), args...);
      _sMsg.set_type(type);
      writeBlocking(_sMsg);
      }
   template <typename ...T>
   std::tuple<T...> read()
      {
      uint32_t version = 0;
      readBlocking(_cMsg);
      if (!_cMsg.status())
         throw StreamCancel();
      //Version data will be present only in the first message for a connection.
      if ((_versionCheckStatus == NOTDONE) &&
            (version = _cMsg.version()))
         {
         if (!checkVersion(version))
            {
            _versionCheckStatus = FAILED;
            throw StreamCancel();
            }
         else
            {
            _versionCheckStatus = PASSED;
            }
         }
      return getArgs<T...>(_cMsg.mutable_data());
      }

   void cancel();
   void finishCompilation(uint32_t statusCode, std::string codeCache = "", std::string dataCache = "", CHTableCommitData chTableData = {},
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
   bool checkVersion(uint32_t version);

   static int _numConnectionsOpened;
   static int _numConnectionsClosed;

   bool getVersionStatus()
      {
      return _versionCheckStatus;
      }
private:
   void finish();

   uint32_t _msTimeout;
   uint64_t _clientId;
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
      }

   void buildAndServe(J9BaseCompileDispatcher *compiler, TR::PersistentInfo *info);

private:
   void serve(J9BaseCompileDispatcher *compiler, uint32_t timeout);
   bool waitOnQueue();
   };

}

#endif // J9_SERVER_H
