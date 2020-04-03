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

#include <arpa/inet.h>
#include <chrono>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>	/* for TCP_NODELAY option */
#include <openssl/err.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> /// gethostname, read, write
#include "control/CompilationRuntime.hpp"
#include "env/TRMemory.hpp"
#include "env/VMJ9.h"
#include "net/CommunicationStream.hpp"
#include "net/LoadSSLLibs.hpp"
#include "net/ServerStream.hpp"
#include "runtime/CompileService.hpp"
#include "runtime/Listener.hpp"

static SSL_CTX *
createSSLContext(TR::PersistentInfo *info)
   {
   SSL_CTX *ctx = (*OSSL_CTX_new)((*OSSLv23_server_method)());

   if (!ctx)
      {
      perror("can't create SSL context");
      (*OERR_print_errors_fp)(stderr);
      exit(1);
      }

   const char *sessionIDContext = "JITServer";
   (*OSSL_CTX_set_session_id_context)(ctx, (const unsigned char*)sessionIDContext, strlen(sessionIDContext));

   if ((*OSSL_CTX_set_ecdh_auto)(ctx, 1) != 1)
      {
      perror("failed to configure SSL ecdh");
      (*OERR_print_errors_fp)(stderr);
      exit(1);
      }

   TR::CompilationInfo *compInfo = TR::CompilationInfo::get();
   auto &sslKeys = compInfo->getJITServerSslKeys();
   auto &sslCerts = compInfo->getJITServerSslCerts();
   auto &sslRootCerts = compInfo->getJITServerSslRootCerts();

   TR_ASSERT_FATAL(sslKeys.size() == 1 && sslCerts.size() == 1, "only one key and cert is supported for now");
   TR_ASSERT_FATAL(sslRootCerts.size() == 0, "server does not understand root certs yet");

   // Parse and set private key
   BIO *keyMem = (*OBIO_new_mem_buf)(&sslKeys[0][0], sslKeys[0].size());
   if (!keyMem)
      {
      perror("cannot create memory buffer for private key (OOM?)");
      (*OERR_print_errors_fp)(stderr);
      exit(1);
      }
   EVP_PKEY *privKey = (*OPEM_read_bio_PrivateKey)(keyMem, NULL, NULL, NULL);
   if (!privKey)
      {
      perror("cannot parse private key");
      (*OERR_print_errors_fp)(stderr);
      exit(1);
      }
   if ((*OSSL_CTX_use_PrivateKey)(ctx, privKey) != 1)
      {
      perror("cannot use private key");
      (*OERR_print_errors_fp)(stderr);
      exit(1);
      }

   // Parse and set certificate
   BIO *certMem = (*OBIO_new_mem_buf)(&sslCerts[0][0], sslCerts[0].size());
   if (!certMem)
      {
      perror("cannot create memory buffer for cert (OOM?)");
      (*OERR_print_errors_fp)(stderr);
      exit(1);
      }
   X509 *certificate = (*OPEM_read_bio_X509)(certMem, NULL, NULL, NULL);
   if (!certificate)
      {
      perror("cannot parse cert");
      (*OERR_print_errors_fp)(stderr);
      exit(1);
      }
   if ((*OSSL_CTX_use_certificate)(ctx, certificate) != 1)
      {
      perror("cannot use cert");
      (*OERR_print_errors_fp)(stderr);
      exit(1);
      }

   // Verify key and cert are valid
   if ((*OSSL_CTX_check_private_key)(ctx) != 1)
      {
      perror("private key check failed");
      (*OERR_print_errors_fp)(stderr);
      exit(1);
      }

   // verify server identity using standard method
   (*OSSL_CTX_set_verify)(ctx, SSL_VERIFY_PEER, NULL);

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Successfully initialized SSL context (%s)\n", (*OOpenSSL_version)(0));

   return ctx;
   }

static bool
handleOpenSSLConnectionError(int connfd, SSL *&ssl, BIO *&bio, const char *errMsg)
   {
   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
       TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "%s: errno=%d", errMsg, errno);
   (*OERR_print_errors_fp)(stderr);

   close(connfd);
   if (bio)
      {
      (*OBIO_free_all)(bio);
      bio = NULL;
      }
   if (ssl)
      {
      (*OSSL_free)(ssl);
      ssl = NULL;
      }
   return false;
   }

static bool
acceptOpenSSLConnection(SSL_CTX *sslCtx, int connfd, BIO *&bio)
   {
   SSL *ssl = (*OSSL_new)(sslCtx);
   if (!ssl)
      return handleOpenSSLConnectionError(connfd, ssl, bio, "Error creating SSL connection");

   (*OSSL_set_accept_state)(ssl);

   if ((*OSSL_set_fd)(ssl, connfd) != 1)
      return handleOpenSSLConnectionError(connfd, ssl, bio, "Error setting SSL file descriptor");

   if ((*OSSL_accept)(ssl) <= 0)
      return handleOpenSSLConnectionError(connfd, ssl, bio, "Error accepting SSL connection");

   bio = (*OBIO_new_ssl)(sslCtx, false);
   if (!bio)
      return handleOpenSSLConnectionError(connfd, ssl, bio, "Error creating new BIO");

   if ((*OBIO_ctrl)(bio, BIO_C_SET_SSL, true, (char *)ssl) != 1) // BIO_set_ssl(bio, ssl, true)
      return handleOpenSSLConnectionError(connfd, ssl, bio, "Error setting BIO SSL");

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "SSL connection on socket 0x%x, Version: %s, Cipher: %s\n",
                                                     connfd, (*OSSL_get_version)(ssl), (*OSSL_get_cipher)(ssl));
   return true;
   }

TR_Listener::TR_Listener()
   : _listenerThread(NULL), _listenerMonitor(NULL), _listenerOSThread(NULL), 
   _listenerThreadAttachAttempted(false), _listenerThreadExitFlag(false)
   {
   }

void
TR_Listener::serveRemoteCompilationRequests(BaseCompileDispatcher *compiler)
   {
   TR::PersistentInfo *info = getCompilationInfo(jitConfig)->getPersistentInfo();
   SSL_CTX *sslCtx = NULL;
   if (JITServer::CommunicationStream::useSSL())
      {
      JITServer::CommunicationStream::initSSL();
      sslCtx = createSSLContext(info);
      }

   uint32_t port = info->getJITServerPort();
   uint32_t timeoutMs = info->getSocketTimeout();
   struct pollfd pfd = {0};
   int sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
   if (sockfd < 0)
      {
      perror("can't open server socket");
      exit(1);
      }

   // see `man 7 socket` for option explanations
   int flag = true;
   if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&flag, sizeof(flag)) < 0)
      {
      perror("Can't set SO_REUSEADDR");
      exit(-1);
      }
   if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (void *)&flag, sizeof(flag)) < 0)
      {
      perror("Can't set SO_KEEPALIVE");
      exit(-1);
      }

   struct sockaddr_in serv_addr;
   memset((char *)&serv_addr, 0, sizeof(serv_addr));
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
   serv_addr.sin_port = htons(port);

   if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
      {
      perror("can't bind server address");
      exit(1);
      }
   if (listen(sockfd, SOMAXCONN) < 0)
      {
      perror("listen failed");
      exit(1);
      }

   pfd.fd = sockfd;
   pfd.events = POLLIN;

   while (!getListenerThreadExitFlag())
      {
      int32_t rc = 0;
      struct sockaddr_in cli_addr;
      socklen_t clilen = sizeof(cli_addr);
      int connfd = -1;

      rc = poll(&pfd, 1, OPENJ9_LISTENER_POLL_TIMEOUT);
      if (getListenerThreadExitFlag()) // if we are exiting, no need to check poll() status
         {
         break;
         }
      else if (0 == rc) // poll() timed out and no fd is ready
         {
         continue;
         }
      else if (rc < 0)
         {
         if (errno == EINTR)
            {
            continue;
            }
         else
            {
            perror("error in polling listening socket");
            exit(1);
            }
         }
      else if (pfd.revents != POLLIN)
         {
         fprintf(stderr, "Unexpected event occurred during poll for new connection: revents=%d\n", pfd.revents);
         exit(1);
         }
      do
         {
         /* at this stage we should have a valid request for new connection */
         connfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
         if (connfd < 0)
            {
            if ((EAGAIN != errno) && (EWOULDBLOCK != errno))
               {
               if (TR::Options::getVerboseOption(TR_VerboseJITServer))
                  {
                  TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Error accepting connection: errno=%d", errno);
                  }
               }
            }
         else
            {
            struct timeval timeoutMsForConnection = {(timeoutMs / 1000), ((timeoutMs % 1000) * 1000)};
            if (setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, (void *)&timeoutMsForConnection, sizeof(timeoutMsForConnection)) < 0)
               {
               perror("Can't set option SO_RCVTIMEO on connfd socket");
               exit(-1);
               }
            if (setsockopt(connfd, SOL_SOCKET, SO_SNDTIMEO, (void *)&timeoutMsForConnection, sizeof(timeoutMsForConnection)) < 0)
               {
               perror("Can't set option SO_SNDTIMEO on connfd socket");
               exit(-1);
               }

            BIO *bio = NULL;
            if (sslCtx && !acceptOpenSSLConnection(sslCtx, connfd, bio))
               continue;

            JITServer::ServerStream *stream = new (PERSISTENT_NEW) JITServer::ServerStream(connfd, bio);
            compiler->compile(stream);
            }
         } while ((-1 != connfd) && !getListenerThreadExitFlag());
      }

   // The following piece of code will be executed only if the server shuts down properly
   if (sslCtx)
      {
      (*OSSL_CTX_free)(sslCtx);
      (*OEVP_cleanup)();
      }
   }

TR_Listener * TR_Listener::allocate()
   {
   TR_Listener * listener = new (PERSISTENT_NEW) TR_Listener();
   return listener;
   }

static int32_t J9THREAD_PROC listenerThreadProc(void * entryarg)
   {
   J9JITConfig * jitConfig = (J9JITConfig *) entryarg;
   J9JavaVM * vm = jitConfig->javaVM;
   TR_Listener *listener = ((TR_JitPrivateConfig*)(jitConfig->privateConfig))->listener; 
   J9VMThread *listenerThread = NULL;
   PORT_ACCESS_FROM_JITCONFIG(jitConfig);

   int rc = vm->internalVMFunctions->internalAttachCurrentThread(vm, &listenerThread, NULL,
                                  J9_PRIVATE_FLAGS_DAEMON_THREAD | J9_PRIVATE_FLAGS_NO_OBJECT |
                                  J9_PRIVATE_FLAGS_SYSTEM_THREAD | J9_PRIVATE_FLAGS_ATTACHED_THREAD,
                                  listener->getListenerOSThread()); 

   listener->getListenerMonitor()->enter();
   listener->setAttachAttempted(true);
   if (rc == JNI_OK)
      listener->setListenerThread(listenerThread);
   listener->getListenerMonitor()->notifyAll();
   listener->getListenerMonitor()->exit();
   if (rc != JNI_OK)
      return JNI_ERR; // attaching the JITServer Listener thread failed

   j9thread_set_name(j9thread_self(), "JITServer Listener");

   J9CompileDispatcher handler(jitConfig);
   listener->serveRemoteCompilationRequests(&handler);

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Detaching JITServer listening thread");

   vm->internalVMFunctions->DetachCurrentThread((JavaVM *) vm);
   listener->getListenerMonitor()->enter();
   listener->setListenerThread(NULL);
   listener->getListenerMonitor()->notifyAll();
   j9thread_exit((J9ThreadMonitor*)listener->getListenerMonitor()->getVMMonitor());

   return 0;
   }

void TR_Listener::startListenerThread(J9JavaVM *javaVM)
   {
   PORT_ACCESS_FROM_JAVAVM(javaVM); 

   UDATA priority;
   priority = J9THREAD_PRIORITY_NORMAL;

   _listenerMonitor = TR::Monitor::create("JITServer-ListenerMonitor");
   if (_listenerMonitor)
      {
      // create the thread for listening to a Client compilation request
      const UDATA defaultOSStackSize = javaVM->defaultOSStackSize; //256KB stack size

      if (J9THREAD_SUCCESS != javaVM->internalVMFunctions->createJoinableThreadWithCategory(&_listenerOSThread,
                                                               defaultOSStackSize,
                                                               priority,
                                                               0,
                                                               &listenerThreadProc,
                                                               javaVM->jitConfig,
                                                               J9THREAD_CATEGORY_SYSTEM_JIT_THREAD))
         { // cannot create the listener thread
         j9tty_printf(PORTLIB, "Error: Unable to create JITServer Listener Thread.\n"); 
         TR::Monitor::destroy(_listenerMonitor);
         _listenerMonitor = NULL;
         }
      else // must wait here until the thread gets created; otherwise an early shutdown
         { // does not know whether or not to destroy the thread
         _listenerMonitor->enter();
         while (!getAttachAttempted())
            _listenerMonitor->wait();
         _listenerMonitor->exit();
         if (!getListenerThread())
            {
            j9tty_printf(PORTLIB, "Error: JITServer Listener Thread attach failed.\n");
            }
         }
      }
   else
      {
      j9tty_printf(PORTLIB, "Error: Unable to create JITServer Listener Monitor\n");
      }
   }

int32_t
TR_Listener::waitForListenerThreadExit(J9JavaVM *javaVM)
   {
   if (NULL != _listenerOSThread)
      return omrthread_join(_listenerOSThread);
   else
      return 0;
   }

void
TR_Listener::stop()
   {
   if (getListenerThread())
      {
      _listenerMonitor->enter();
      setListenerThreadExitFlag();
      _listenerMonitor->wait();
      _listenerMonitor->exit();
      TR::Monitor::destroy(_listenerMonitor);
      _listenerMonitor = NULL;
      }
   }
