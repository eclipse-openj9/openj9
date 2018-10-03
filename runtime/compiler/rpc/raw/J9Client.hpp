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

#ifndef J9_CLIENT_H
#define J9_CLIENT_H

#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <openssl/ssl.h>
#include "compile/CompilationTypes.hpp"
#include "rpc/StreamTypes.hpp"
#include "rpc/ProtobufTypeConvert.hpp"
#include "j9.h"
#include "ilgen/J9IlGeneratorMethodDetails.hpp"
#include "rpc/raw/J9Stream.hpp"

class SSLOutputStream;
class SSLInputStream;

namespace JITaaS
{
class J9ClientStream : J9Stream
   {
public:
   static void static_init(TR::PersistentInfo *info);

   J9ClientStream(TR::PersistentInfo *info);
   virtual ~J9ClientStream() {}

   template <typename... T>
   void buildCompileRequest(T... args)
      {
      write(args...);
      }

   Status waitForFinish();

   template <typename ...T>
   void write(T... args)
      {
      _cMsg.set_status(true);
      setArgs<T...>(_cMsg.mutable_data(), args...);
      writeBlocking(_cMsg);
      }

   void writeError();

   J9ServerMessageType read()
      {
      readBlocking(_sMsg);
      return _sMsg.type();
      }

   template <typename ...T>
   std::tuple<T...> getRecvData()
      {
      return getArgs<T...>(_sMsg.mutable_data());
      }

   void shutdown();

private:
   uint32_t _timeout;

   static SSL_CTX *_sslCtx;
   };

}

#endif // J9_CLIENT_H
