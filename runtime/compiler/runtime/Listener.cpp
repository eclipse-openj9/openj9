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
#include "env/VerboseLog.hpp"
#include "net/CommunicationStream.hpp"
#include "net/LoadSSLLibs.hpp"
#include "net/ServerStream.hpp"
#include "runtime/CompileService.hpp"
#include "runtime/Listener.hpp"

static bool
handleOpenSSLConnectionError(int connfd, SSL *&ssl, BIO *&bio, const char *errMsg, int ret, TR::CompilationInfo *compInfo)
   {
   int errnoCopy = errno;
   int sslError = SSL_ERROR_NONE;
   unsigned long errorCode = 0;
   int errorReason = 0;
   char errorString[256] = {0};

   // Filter out SSL_R_UNEXPECTED_EOF_WHILE_READING errors that were introduced in SSL version 3
   // These can appear if Kubernetes readiness/liveness probes are used on the TCP encrypted channel
   if (ret <= 0)
      {
      sslError = (*OSSL_get_error)(ssl, ret);
      // Get earliest error code from the thread's error queue without modifying it.
      errorCode = (*OERR_peek_error)();
      errorReason = ERR_GET_REASON(errorCode);
      (*OERR_error_string_n)(errorCode, errorString, sizeof(errorString));
      if (sslError == SSL_ERROR_SSL && errorReason == SSL_R_UNEXPECTED_EOF_WHILE_READING)
         {
         // Take this error out of the queue so that it doesn't get printed
         errorCode = (*OERR_get_error)();
         }
      }
   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
       TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "t=%lu %s: errno=%d sslError=%d errorString=%s",
                                      (unsigned long)compInfo->getPersistentInfo()->getElapsedTime(),
                                      errMsg, errnoCopy, sslError, errorString);
   // Print the error strings for all errors that OpenSSL has recorded and empty the error queue
   (*OERR_print_errors_fp)(stderr);

   if (bio)
      {
      (*OBIO_free_all)(bio);
      bio = NULL;
      }
   close(connfd);
   return false;
   }

static bool
acceptOpenSSLConnection(SSL_CTX *sslCtx, int connfd, BIO *&bio, TR::CompilationInfo *compInfo)
   {
   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "t=%lu Accepting SSL connection on socket 0x%x",
                                     (unsigned long)compInfo->getPersistentInfo()->getElapsedTime(), connfd);
   SSL *ssl = NULL;
   bio = (*OBIO_new_ssl)(sslCtx, false);
   if (!bio)
      return handleOpenSSLConnectionError(connfd, ssl, bio, "Error creating new BIO", 1 /*ret*/, compInfo);

   int ret = (*OBIO_ctrl)(bio, BIO_C_GET_SSL, false, (char *) &ssl); // BIO_get_ssl(bio, &ssl)
   if (ret != 1)
       return handleOpenSSLConnectionError(connfd, ssl, bio, "Failed to get BIO SSL", ret, compInfo);

   ret = (*OSSL_set_fd)(ssl, connfd);
   if (ret != 1)
      return handleOpenSSLConnectionError(connfd, ssl, bio, "Error setting SSL file descriptor", ret, compInfo);

   ret = (*OSSL_accept)(ssl);
   if (ret != 1)
      return handleOpenSSLConnectionError(connfd, ssl, bio, "Error accepting SSL connection", ret, compInfo);

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "t=%lu SSL connection on socket 0x%x, Version: %s, Cipher: %s",
                                     (unsigned long)compInfo->getPersistentInfo()->getElapsedTime(),
                                     connfd, (*OSSL_get_version)(ssl), (*OSSL_get_cipher)(ssl));
   return true;
   }

static int
openCommunicationSocket(uint32_t port, uint32_t &boundPort)
   {
   int sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
   if (sockfd < 0)
      {
      perror("can't open server socket");
      return sockfd;
      }
      // see `man 7 socket` for option explanations
   int flag = true;
   int retCode = 0;
   retCode = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
   if (retCode < 0)
      {
      perror("Can't set SO_REUSEADDR");
      close(sockfd);
      return retCode;
      }
   retCode = setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag));
   if (retCode < 0)
      {
      perror("Can't set SO_KEEPALIVE");
      close(sockfd);
      return retCode;
      }
   struct sockaddr_in serv_addr;
   memset(&serv_addr, 0, sizeof(serv_addr));
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
   serv_addr.sin_port = htons(port);

   retCode = bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
   if (retCode < 0)
      {
      perror("can't bind server address");
      close(sockfd);
      return retCode;
      }
   retCode = listen(sockfd, SOMAXCONN);
   if (retCode < 0)
      {
      perror("listen failed");
      close(sockfd);
      return retCode;
      }

   if (port == 0)
      {
      struct sockaddr_in sock_desc;
      socklen_t sock_desc_len = sizeof(sock_desc);
      retCode = getsockname(sockfd, (struct sockaddr *)&sock_desc, &sock_desc_len);
      if (retCode < 0)
         {
         perror("getsockname failed");
         close(sockfd);
         return retCode;
         }
      boundPort = ntohs(sock_desc.sin_port);
      }
   else
      {
      boundPort = port;
      }
   return sockfd;
   }

TR_Listener::TR_Listener()
   : _listenerThread(NULL), _listenerMonitor(NULL), _listenerOSThread(NULL),
   _listenerThreadAttachAttempted(false), _listenerThreadExitFlag(false)
   {
   }

void
TR_Listener::serveRemoteCompilationRequests(BaseCompileDispatcher *compiler)
   {
   TR::CompilationInfo *compInfo = getCompilationInfo(jitConfig);
   TR::PersistentInfo *info = compInfo->getPersistentInfo();
   SSL_CTX *sslCtx = NULL;
   if (JITServer::CommunicationStream::useSSL())
      {
      auto &sslKeys = compInfo->getJITServerSslKeys();
      auto &sslCerts = compInfo->getJITServerSslCerts();
      auto &sslRootCerts = compInfo->getJITServerSslRootCerts();
      const char *sessionContextId = "JITServer";
      if (!JITServer::ServerStream::createSSLContext(sslCtx, sessionContextId, sizeof(sessionContextId), sslKeys, sslCerts, sslRootCerts))
         {
         fprintf(stderr, "Failed to initialize the SSL context\n");
         exit(1);
         }
      }

   uint32_t port = info->getJITServerPort();
   uint32_t boundPort = 0;
   int sockfd = openCommunicationSocket(port, boundPort);
   if (sockfd >= 0)
      {
      info->setJITServerPort(boundPort);
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "t=%lu Communication socket opened on port %u",
                                        (unsigned long)compInfo->getPersistentInfo()->getElapsedTime(), boundPort);
         }
      }
   else
      {
      fprintf(stderr, "Failed to open server socket on port %d\n", port);
      exit(1);
      }

   // If desired, open readiness/liveness socket to be used by Kubernetes. This is unencrypted.
   uint32_t healthPort = info->getJITServerHealthPort();
   uint32_t boundHealthPort = 0;
   int healthSockfd = -1;
   if (info->getJITServerUseHealthPort())
      {
      healthSockfd = openCommunicationSocket(healthPort, boundHealthPort);
      if (healthSockfd >= 0)
         {
         info->setJITServerHealthPort(boundHealthPort);
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "t=%lu Health socket opened on port %u",
                                           (unsigned long)compInfo->getPersistentInfo()->getElapsedTime(), boundHealthPort);
            }
         }
      else
         {
         fprintf(stderr, "Failed to open health socket on port %d\n", healthPort);
         exit(1);
         }
      }

   // The following array accomodates two descriptors: healthSockfd and sockfd.
   // The first one is used for readiness/liveness probes, the second one is used for compilation requests.
   // If we don't want to use readiness/liveness probes, healthSockfd will be -1 and will be ignored by poll().
   struct pollfd pfd[2] = {{.fd = healthSockfd, .events = POLLIN, .revents = 0},
                           {.fd = sockfd,       .events = POLLIN, .revents = 0}
                          };
   static const size_t numFds = sizeof(pfd) / sizeof(pfd[0]);

   while (!getListenerThreadExitFlag())
      {
      int32_t rc = 0;
      struct sockaddr_in cli_addr;
      socklen_t clilen = sizeof(cli_addr);
      int connfd = -1;

      rc = poll(pfd, numFds, OPENJ9_LISTENER_POLL_TIMEOUT);
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
      // Check which file descriptor is ready
      for (size_t fdIndex = 0; fdIndex < numFds; fdIndex++)
         {
         if (pfd[fdIndex].revents == 0) // No event on this file descriptor
            continue;
         // We have an event and that event can only be POLLIN
         TR_ASSERT_FATAL(pfd[fdIndex].revents == POLLIN, "Unexpected event occurred during poll for new connection: socketIndex=%zu revents=%d\n", fdIndex, pfd[fdIndex].revents);

         // At this stage we should have a valid request for a new connection
         pfd[fdIndex].revents = 0; // Reset the event for the next poll operation
         do
            {
            connfd = accept(pfd[fdIndex].fd, (struct sockaddr *)&cli_addr, &clilen);
            if (connfd < 0)
               {
               if ((EAGAIN != errno) && (EWOULDBLOCK != errno))
                  {
                  if (TR::Options::getVerboseOption(TR_VerboseJITServer))
                     {
                     TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Error accepting connection: errno=%d: %s",
                                                   errno, strerror(errno));
                     }
                  }
               }
            else // accept() succeeded
               {
               if (fdIndex == 0) // readiness/liveness probe socket
                  {
                  close(connfd);
                  connfd = -1;
                  }
               else // compilation request socket
                  {
                  // Set the socket timeout (in milliseconds
                  uint32_t timeoutMs = info->getSocketTimeout();
                  struct timeval timeout = { timeoutMs / 1000, (timeoutMs % 1000) * 1000 };
                  if (setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
                     {
                     perror("Can't set option SO_RCVTIMEO on connfd socket");
                     exit(1);
                     }
                  if (setsockopt(connfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0)
                     {
                     perror("Can't set option SO_SNDTIMEO on connfd socket");
                     exit(1);
                     }

                  BIO *bio = NULL;
                  if (sslCtx && !acceptOpenSSLConnection(sslCtx, connfd, bio, compInfo))
                     continue;

                  JITServer::ServerStream *stream = new (TR::Compiler->persistentGlobalAllocator()) JITServer::ServerStream(connfd, bio);
                  compiler->compile(stream);
                  }
               }
            } while ((connfd >= 0) && !getListenerThreadExitFlag());
         }
      } //  while (!getListenerThreadExitFlag())

   // The following piece of code will be executed only if the server shuts down properly
   close(sockfd);
   if (sslCtx)
      {
      (*OSSL_CTX_free)(sslCtx);
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

   if (TR::Options::isAnyVerboseOptionSet())
      {
      OMRPORT_ACCESS_FROM_J9PORT(PORTLIB);
      char timestamp[32];
      char zoneName[32];
      int32_t zoneSecondsEast = 0;
      TR_VerboseLog::CriticalSection vlogLock;

      omrstr_ftime_ex(timestamp, sizeof(timestamp), "%b %d %H:%M:%S %Y", j9time_current_time_millis(), OMRSTR_FTIME_FLAG_LOCAL);
      TR_VerboseLog::writeLine(TR_Vlog_INFO, "StartTime: %s", timestamp);

      TR_VerboseLog::write(TR_Vlog_INFO, "TimeZone: ");
      if (0 != omrstr_current_time_zone(&zoneSecondsEast, zoneName, sizeof(zoneName)))
         {
         TR_VerboseLog::write("(unavailable)");
         }
      else
         {
         /* Write UTC[+/-]hr:min (zoneName) to the log. */
         TR_VerboseLog::write("UTC");

         if (0 != zoneSecondsEast)
            {
            const char *format = (zoneSecondsEast > 0) ? "+%d" : "-%d";
            int32_t offset = ((zoneSecondsEast > 0) ? zoneSecondsEast : -zoneSecondsEast) / 60;
            int32_t hours = offset / 60;
            int32_t minutes = offset % 60;

            TR_VerboseLog::write(format, hours);
            if (0 != minutes)
               {
               TR_VerboseLog::write(":%02d", minutes);
               }
            }
         if ('\0' != *zoneName)
            {
            TR_VerboseLog::write(" (%s)", zoneName);
            }
         TR_VerboseLog::write("\n");
         }
      }

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
