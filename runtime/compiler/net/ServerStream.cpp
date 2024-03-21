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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "ServerStream.hpp"

namespace JITServer
{
int ServerStream::_numConnectionsOpened = 0;
int ServerStream::_numConnectionsClosed = 0;

ServerStream::ServerStream(int connfd, BIO *ssl)
   : CommunicationStream()
   {
   initStream(connfd, ssl);
   _numConnectionsOpened++;
   _pClientSessionData = NULL;
   }

static bool handleCreateSSLContextError(SSL_CTX *&ctx, const char *errMsg)
   {
   perror(errMsg);
   (*OERR_print_errors_fp)(stderr);
   if (ctx)
      {
      (*OSSL_CTX_free)(ctx);
      ctx = NULL;
      }
   return false;
   }

bool ServerStream::createSSLContext(SSL_CTX *&ctx, const char *sessionContextID, size_t sessionContextIDLen,
                                    const PersistentVector<std::string> &sslKeys, const PersistentVector<std::string> &sslCerts,
                                    const std::string &sslRootCerts)
   {
   ctx = (*OSSL_CTX_new)((*OSSLv23_server_method)());

   if (!ctx)
      {
      return handleCreateSSLContextError(ctx, "can't create SSL context");
      }

   (*OSSL_CTX_set_session_id_context)(ctx, (const unsigned char*)sessionContextID, sessionContextIDLen);

   if ((*OSSL_CTX_set_ecdh_auto)(ctx, 1) != 1)
      {
      return handleCreateSSLContextError(ctx, "failed to configure SSL ecdh");
      }

   TR_ASSERT_FATAL(sslKeys.size() == 1 && sslCerts.size() == 1, "only one key and cert is supported for now");
   TR_ASSERT_FATAL(sslRootCerts.size() == 0, "server does not understand root certs yet");

   // Parse and set private key
   BIO *keyMem = (*OBIO_new_mem_buf)(&sslKeys[0][0], sslKeys[0].size());
   if (!keyMem)
      {
      return handleCreateSSLContextError(ctx, "cannot create memory buffer for private key (OOM?)");
      }
   EVP_PKEY *privKey = (*OPEM_read_bio_PrivateKey)(keyMem, NULL, NULL, NULL);
   if (!privKey)
      {
      return handleCreateSSLContextError(ctx, "cannot parse private key");
      }
   if ((*OSSL_CTX_use_PrivateKey)(ctx, privKey) != 1)
      {
      return handleCreateSSLContextError(ctx, "cannot use private key");
      }

   // Parse and set certificate
   BIO *certMem = (*OBIO_new_mem_buf)(&sslCerts[0][0], sslCerts[0].size());
   if (!certMem)
      {
      return handleCreateSSLContextError(ctx, "cannot create memory buffer for cert (OOM?)");
      }
   X509 *certificate = (*OPEM_read_bio_X509)(certMem, NULL, NULL, NULL);
   if (!certificate)
      {
      return handleCreateSSLContextError(ctx, "cannot parse cert");
      }
   if ((*OSSL_CTX_use_certificate)(ctx, certificate) != 1)
      {
      return handleCreateSSLContextError(ctx, "cannot use cert");
      }

   // Verify key and cert are valid
   if ((*OSSL_CTX_check_private_key)(ctx) != 1)
      {
      return handleCreateSSLContextError(ctx, "private key check failed");
      }

   // verify server identity using standard method
   (*OSSL_CTX_set_verify)(ctx, SSL_VERIFY_PEER, NULL);

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Successfully initialized SSL context (%s)", (*OOpenSSL_version)(0));

   return true;
   }
}
