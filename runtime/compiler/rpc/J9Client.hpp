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

#ifndef J9_CLIENT_H
#define J9_CLIENT_H

#include <google/protobuf/io/zero_copy_stream_impl.h>
#include "compile/CompilationTypes.hpp"
#include "rpc/StreamTypes.hpp"
#include "rpc/ProtobufTypeConvert.hpp"
#include "ilgen/J9IlGeneratorMethodDetails.hpp"
#include "rpc/J9Stream.hpp"

#if defined(JITAAS_ENABLE_SSL)
#include <openssl/ssl.h>
class SSLOutputStream;
class SSLInputStream;
#endif

namespace JITaaS
{
enum VersionCheckStatus
   {
   NOT_DONE = 0,
   PASSED = 1,
   };

class J9ClientStream : J9Stream
   {
public:
   static void static_init(TR::PersistentInfo *info);

   J9ClientStream(TR::PersistentInfo *info);
   virtual ~J9ClientStream()
      {
      _numConnectionsClosed++;
      }

   template <typename... T>
   void buildCompileRequest(T... args)
      {
      if (getVersionCheckStatus() == NOT_DONE)
         {
         _cMsg.set_version(getJITaaSVersion());
         write(MessageType::compilationRequest, args...);
         _cMsg.clear_version();
         }
      else // getVersionCheckStatus() == PASSED
         {
         _cMsg.clear_version(); // the compatibility check is done. We clear the version to save message size.
         write(MessageType::compilationRequest, args...);
         }
      }

   Status waitForFinish();

   template <typename ...T>
   void write(MessageType type, T... args)
      {
      _cMsg.set_type(type);
      setArgs<T...>(_cMsg.mutable_data(), args...);

      writeBlocking(_cMsg);
      }

   MessageType read()
      {
      readBlocking(_sMsg);
      return _sMsg.type();
      }

   template <typename ...T>
   std::tuple<T...> getRecvData()
      {
      return getArgs<T...>(_sMsg.mutable_data());
      }

   template <typename ...T>
   void writeError(MessageType type, T... args)
      {
      _cMsg.set_type(type);
      if (type == MessageType::compilationAbort)
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

   static void incrementIncompatibilityCount(OMRPortLibrary *portLibrary)
      {
      OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
      _incompatibilityCount += 1;
      _incompatibleStartTime = omrtime_current_time_millis();
      }

   static bool isServerCompatible(OMRPortLibrary *portLibrary)
      {
      OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
      uint64_t current_time = omrtime_current_time_millis();
      // the client periodically checks whether the server is compatible or not
      // the retry interval is defined by RETRY_COMPATIBILITY_INTERVAL
      // we set _incompatibilityCount to 0 when it's time to retry for compatibility check
      // otherwise, check if we exceed the number of incompatible connections
      if ((current_time - _incompatibleStartTime) > RETRY_COMPATIBILITY_INTERVAL)
         _incompatibilityCount = 0;

      return _incompatibilityCount < INCOMPATIBILITY_COUNT_LIMIT;
      }

   static int _numConnectionsOpened;
   static int _numConnectionsClosed;

private:
   uint32_t _timeout;
   VersionCheckStatus _versionCheckStatus;
   static int _incompatibilityCount;
   static uint64_t _incompatibleStartTime;
   static const uint64_t RETRY_COMPATIBILITY_INTERVAL;
   static const int INCOMPATIBILITY_COUNT_LIMIT;

#if defined(JITAAS_ENABLE_SSL)
   static SSL_CTX *_sslCtx;
#endif
   };

}

#endif // J9_CLIENT_H
