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

#ifndef SSL_PROTOBUF_STREAM_HPP
#define SSL_PROTOBUF_STREAM_HPP

#include <openssl/ssl.h>
#include <openssl/err.h>
#include "net/LoadSSLLibs.hpp"

class SSLOutputStream
   {
public:
   SSLOutputStream(BIO *bio)
      : _ssl(bio)
      {
      }
   virtual ~SSLOutputStream()
      {
      }
   virtual bool Write(const void *buffer, int size)
      {
      TR_ASSERT(size > 0, "writing zero bytes is undefined behavior");
      int n = (*OBIO_write)(_ssl, buffer, size);
      if (n <= 0)
         {
         (*OERR_print_errors_fp)(stderr);
         return false;
         }
      /*if (BIO_flush(_ssl) != 1)
         {
         perror("flushing");
         (*OERR_print_errors_fp)(stderr);
         return false;
         }*/
      TR_ASSERT(n == size, "not expecting partial write");
      return true;
      }

private:
   BIO *_ssl;
   };

class SSLInputStream
   {
public:
   SSLInputStream(BIO *bio)
      : _ssl(bio)
      {
      }
   virtual ~SSLInputStream()
      {
      }
   virtual int Read(void *buffer, int size)
      {
      TR_ASSERT(size > 0, "reading zero bytes is undefined behavior");
      int n = (*OBIO_read)(_ssl, buffer, size);
      if (n <= 0)
         {
         (*OERR_print_errors_fp)(stderr);
         }
      return n;
      }

private:
   BIO *_ssl;
   };

#endif // SSL_PROTOBUF_STREAM_HPP
