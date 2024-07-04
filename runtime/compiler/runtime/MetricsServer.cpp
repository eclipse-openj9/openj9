/*******************************************************************************
 * Copyright IBM Corp. and others 2022
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
#include <unistd.h> // read, write

#include "control/CompilationRuntime.hpp"
#include "control/Options.hpp"
#include "env/TRMemory.hpp"
#include "env/PersistentInfo.hpp"
#include "env/VerboseLog.hpp"
#include "env/VMJ9.h"
#include "net/ServerStream.hpp"
#include "runtime/MetricsServer.hpp"

bool MetricsServer::useSSL(TR::CompilationInfo *compInfo)
   {
   return (compInfo->getJITServerMetricsSslKeys().size() || compInfo->getJITServerMetricsSslCerts().size());
   }

bool HttpGetRequest::handleSSLConnectionError(const char *errMsg)
   {
   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
       TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "%s: errno=%d", errMsg, errno);
   (*OERR_print_errors_fp)(stderr);

   if (_ssl)
      {
      (*OBIO_free_all)(_ssl);
      _ssl = NULL;
      _incompleteSSLConnection = NULL;
      }
   return false;
   }

bool HttpGetRequest::setupSSLConnection(SSL_CTX *sslCtx)
   {
   _ssl = (*OBIO_new_ssl)(sslCtx, false);
   if (!_ssl)
      {
      return handleSSLConnectionError("Error creating new BIO");
      }

   if ((*OBIO_ctrl)(_ssl, BIO_C_GET_SSL, false, (char *) &_incompleteSSLConnection) != 1) // BIO_get_ssl(bio, &ssl)
      {
      return handleSSLConnectionError("Failed to get BIO SSL");
      }

   if ((*OSSL_set_fd)(_incompleteSSLConnection, _sockfd) != 1)
      {
      return handleSSLConnectionError("Error setting SSL file descriptor");
      }

   return true;
   }

HttpGetRequest::ReturnCodes HttpGetRequest::acceptSSLConnection()
   {
   int ret = (*OSSL_accept)(_incompleteSSLConnection);
   if (1 == ret)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "SSL connection on socket 0x%x, Version: %s, Cipher: %s",
                                        _sockfd, (*OSSL_get_version)(_incompleteSSLConnection), (*OSSL_get_cipher)(_incompleteSSLConnection));
         }
      return SSL_CONNECTION_ESTABLISHED;
      }

   int err = (*OSSL_get_error)(_incompleteSSLConnection, ret);
   if (SSL_ERROR_WANT_READ == err)
      {
      return WANT_READ;
      }
   else if (SSL_ERROR_WANT_WRITE == err)
      {
      return WANT_WRITE;
      }
   else
      {
      handleSSLConnectionError("Error accepting SSL connection");
      return SSL_CONNECTION_ERROR;
      }
   }

double CPUUtilMetric::computeValue(TR::CompilationInfo *compInfo)
   {
   CpuUtilization *cpuUtil = compInfo->getCpuUtil();
   if (cpuUtil->isFunctional())
      {
      setValue(cpuUtil->getVmCpuUsage());
      }
   return getValue();
   }

double AvailableMemoryMetric::computeValue(TR::CompilationInfo *compInfo)
   {
   setValue(compInfo->getCachedFreePhysicalMemoryB());
   return getValue();
   }

double ConnectedClientsMetric::computeValue(TR::CompilationInfo *compInfo)
   {
   // Accessing clientSessionHT without a lock. This is just
   // for information purposes and getting the slightly wrong
   // value (rarely) may not matter
   setValue(compInfo->getClientSessionHT()->size());
   return getValue();
   }

double ActiveThreadsMetric::computeValue(TR::CompilationInfo *compInfo)
   {
   setValue(compInfo->getNumCompThreadsActive());
   return getValue();
   }

MetricsDatabase::MetricsDatabase(TR::CompilationInfo *compInfo) : _compInfo(compInfo)
   {
   _metrics[0] = new (PERSISTENT_NEW) CPUUtilMetric();
   _metrics[1] = new (PERSISTENT_NEW) AvailableMemoryMetric();
   _metrics[2] = new (PERSISTENT_NEW) ConnectedClientsMetric();
   _metrics[3] = new (PERSISTENT_NEW) ActiveThreadsMetric();
   static_assert(3 == MAX_METRICS - 1, "Unsupported number of metrics");
   }

MetricsDatabase::~MetricsDatabase()
   {
   for (int i =0; i < MAX_METRICS; i++)
      {
      _metrics[i]->~PrometheusMetric();
      TR_Memory::jitPersistentFree(_metrics[i]);
      }
   }

std::string
MetricsDatabase::serializeMetrics()
   {
   std::string output;
   for (int i =0; i < MAX_METRICS; i++)
      {
      _metrics[i]->computeValue(_compInfo);
      output.append(_metrics[i]->serialize());
      }
   return output;
   }

HttpGetRequest::~HttpGetRequest()
   {
   if (_ssl)
      {
      (*OBIO_free_all)(_ssl);
      }
   }

void HttpGetRequest::clear()
   {
   _sockfd = -1;
   _requestState = Inactive;
   _path = Undefined;
   _httpVersion[0] = '\0';
   _msgLength = 0;
   _buf[0] = '\0';
   if (_ssl)
      {
      (*OBIO_free_all)(_ssl);
      _ssl = NULL;
      }
   _incompleteSSLConnection = NULL;
   _response.clear();
   }

HttpGetRequest::ReturnCodes
HttpGetRequest::readHttpGetRequest()
   {
   size_t capacityLeft = BUF_SZ - 1 - _msgLength; // subtract 1 for NULL terminator
   int bytesRead = 0;
   if (_ssl)
      {
      bytesRead = (*OBIO_read)(_ssl, _buf + _msgLength, capacityLeft);
      // Even though we waited with poll(), there may still not be enough data available in the SSL case
      if ((bytesRead <= 0) && (*OBIO_should_retry)(_ssl))
         {
         if ((*OBIO_should_read)(_ssl))
            {
            return WANT_READ;
            }
         else if ((*OBIO_should_write)(_ssl))
            {
            return WANT_WRITE;
            }
         }
      }
   else
      {
      // Unlike the SSL case, there is guaranteed to be some data to read here because of poll()
      bytesRead = read(_sockfd, _buf + _msgLength, capacityLeft);
      }

   if (bytesRead <= 0)
      {
      fprintf(stderr, "Error reading from socket %d ", _sockfd);
      perror("");
      return READ_ERROR;
      }
   // Must have at least GET, one space, one path, one space, http protocol and CR LF
   if (_msgLength == 0)
      {
      // This is the first read on this socket. Make sure it starts with GET
      if (bytesRead < 4)
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "MetricsServer: Too few bytes received when reading from socket  %d", socket);
         return READ_ERROR;
         }
      if (strncmp(_buf, "GET ", 4) != 0)
         return NO_GET_REQUEST; // We only accept GET requests
      }
   _msgLength += bytesRead;
   _buf[_msgLength] = 0; // NULL string terminator

   // Check that we got the entire message. The GET request should end with a double CR LF sequence.
   char *endOfRequestPtr = strstr(_buf, "\r\n\r\n");
   if (!endOfRequestPtr)
      {
      // Incomplete message
      if (_msgLength >= BUF_SZ-1) // No more space to read
         return REQUEST_TOO_LARGE;
      return WANT_READ; // Will have to read more data
      }
   return FULL_REQUEST_RECEIVED;
   }

HttpGetRequest::ReturnCodes
HttpGetRequest::parseHttpGetRequest()
   {
   // Parse my message. Expect something like:
   // GET /metrics HTTP/1.1
   // Host: 127.0.0.1:9403
   // User-Agent: Prometheus/2.31.1
   // Accept: application/openmetrics-text; version=0.0.1,text/plain;version=0.0.4;q=0.5,*/*;q=0.1
   // Accept-Encoding: gzip
   // X-Prometheus-Scrape-Timeout-Seconds: 3
   static const char *MetricsPath = "/metrics";
   static const size_t MetricsPathLength = strlen(MetricsPath);
   static const char *tokenSeparators = " \r\n";
   static const char *endLineSequence = "\r\n";

   _buf[BUF_SZ-1] = 0; // make sure our string routines will find an end of string
   // We already checked that the message starts with "GET "
   size_t pos = 4;

   // Jump over spaces
   while (pos < _msgLength && _buf[pos] == ' ')
      pos++;
   if (pos >= _msgLength)
      return MALFORMED_REQUEST;

   // Get the path
   size_t pathLength = strcspn(_buf + pos, tokenSeparators);
   if (pathLength > HttpGetRequest::MAX_PATH_LENGTH - 1)
      return REQUEST_URI_TOO_LONG;
   // TODO: process absolute paths like http://server:port/metrics. Absolute paths always have ":" in them
   // Here we want to check the received path against well known strings like "/metrics", "/liveness", "/readiness"
   if (pathLength == MetricsPathLength && strncmp(_buf+pos, MetricsPath, pathLength) == 0)
      _path = Path::Metrics;
   else
      return INVALID_PATH;

   // Jump over path
   pos += pathLength;

   // Must have at least one space
   if (_buf[pos++] != ' ')
      return MALFORMED_REQUEST;
   // Jump over spaces
   while (pos < _msgLength && _buf[pos] == ' ')
      pos++;
   if (pos >= _msgLength)
      return MALFORMED_REQUEST;

   // Get the Http protocol; expect something like HTTP/1.1 or HTTP/1
   static const size_t MAX_HTTP_VERSION_STRING_SIZE = 8; // e.g HTTP/1.1 (major.minor)
   static const size_t MIN_HTTP_VERSION_STRING_SIZE = 6; // e.g HTTP/2
   size_t protocolLength = strcspn(_buf + pos, tokenSeparators);
   if (protocolLength < MIN_HTTP_VERSION_STRING_SIZE || protocolLength > MAX_HTTP_VERSION_STRING_SIZE)
      return INVALID_HTTP_PROTOCOL;
   static const char *HTTP_VERSION_HEADER_STRING = "HTTP/";
   static const size_t HTTP_VERSION_HEADER_STRING_SIZE = strlen(HTTP_VERSION_HEADER_STRING);
   if (strncmp(_buf+pos, HTTP_VERSION_HEADER_STRING, HTTP_VERSION_HEADER_STRING_SIZE))
      return INVALID_HTTP_PROTOCOL;
   memcpy(_httpVersion, _buf+pos+HTTP_VERSION_HEADER_STRING_SIZE, protocolLength-HTTP_VERSION_HEADER_STRING_SIZE);
   _httpVersion[protocolLength-HTTP_VERSION_HEADER_STRING_SIZE] = 0; // null terminator

   pos += protocolLength;
   // Find the "\r\n" sequence which ends the Request Line
   char *endOfRequestPtr = strstr(_buf+pos, endLineSequence);
   if (!endOfRequestPtr)
      return MALFORMED_REQUEST;

   // Now read the headers
   endOfRequestPtr += 2; // jump over "/r/n"
   // We know that we must have at least one other pair of "\r\n" at the very least
   char *endOfHeaderLine = strstr(endOfRequestPtr, endLineSequence);

   return HTTP_OK;
   }

HttpGetRequest::ReturnCodes
HttpGetRequest::sendHttpResponse()
   {
   int bytesWritten = 0;
   int remainingBytes = _response.length() - _responseBytesSent + 1; // add one for NULL terminator
   if (_ssl)
      {
      bytesWritten = (*OBIO_write)(_ssl, _response.c_str() + _responseBytesSent, remainingBytes);
      // Even though we waited with poll(), we may still not be able to send any data in the SSL case
      if ((bytesWritten <= 0) && (*OBIO_should_retry)(_ssl))
         {
         if ((*OBIO_should_read)(_ssl))
            {
            return WANT_READ;
            }
         else if ((*OBIO_should_write)(_ssl))
            {
            return WANT_WRITE;
            }
         }
      }
   else
      {
      // Unlike the SSL case, there will always be capacity to write data because we waited with poll()
      bytesWritten = write(_sockfd, _response.c_str() + _responseBytesSent, remainingBytes);
      }

   if (remainingBytes == bytesWritten)
      {
      return FULL_RESPONSE_SENT;
      }
   else if (bytesWritten > 0)
      {
      _responseBytesSent += bytesWritten;
      return WANT_WRITE;
      }
   else
      {
      fprintf(stderr, "Error writing to socket %d ", _sockfd);
      perror("");
      return WRITE_ERROR;
      }
   }

MetricsServer::MetricsServer()
   : _metricsThread(NULL), _metricsMonitor(NULL), _metricsOSThread(NULL),
   _metricsThreadAttachAttempted(false), _metricsThreadExitFlag(false), _jitConfig(NULL), _sslCtx(NULL)
   {
   for (int i = 0; i < 1 + MAX_CONCURRENT_REQUESTS; i++)
      _pfd[i].fd = -1; // invalid
   }

MetricsServer * MetricsServer::allocate()
   {
   MetricsServer * metricsServer = new (PERSISTENT_NEW) MetricsServer();
   return metricsServer;
   }

static int32_t J9THREAD_PROC metricsThreadProc(void * entryarg)
   {
   J9JITConfig * jitConfig = (J9JITConfig *) entryarg;
   J9JavaVM * vm = jitConfig->javaVM;
   MetricsServer *metricsServer = ((TR_JitPrivateConfig*)(jitConfig->privateConfig))->metricsServer;
   metricsServer->setJITConfig(jitConfig);
   J9VMThread *metricsThread = NULL;
   PORT_ACCESS_FROM_JITCONFIG(jitConfig);

   int rc = vm->internalVMFunctions->internalAttachCurrentThread(vm, &metricsThread, NULL,
                                  J9_PRIVATE_FLAGS_DAEMON_THREAD | J9_PRIVATE_FLAGS_NO_OBJECT |
                                  J9_PRIVATE_FLAGS_SYSTEM_THREAD | J9_PRIVATE_FLAGS_ATTACHED_THREAD,
                                  metricsServer->getMetricsOSThread());

   metricsServer->getMetricsMonitor()->enter();
   metricsServer->setAttachAttempted(true);
   if (rc == JNI_OK)
      metricsServer->setMetricsThread(metricsThread);
   metricsServer->getMetricsMonitor()->notifyAll();
   metricsServer->getMetricsMonitor()->exit();
   if (rc != JNI_OK)
      return JNI_ERR; // attaching the JITServer Listener thread failed

   j9thread_set_name(j9thread_self(), "JITServer Metrics");

   metricsServer->serveMetricsRequests(); // Will block here till shutdown

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Detaching JITServer metrics thread");

   vm->internalVMFunctions->DetachCurrentThread((JavaVM *) vm);
   metricsServer->getMetricsMonitor()->enter();
   metricsServer->setMetricsThread(NULL);
   metricsServer->getMetricsMonitor()->notifyAll();
   j9thread_exit((J9ThreadMonitor*)metricsServer->getMetricsMonitor()->getVMMonitor());

   return 0;
   }

void
MetricsServer::startMetricsThread(J9JavaVM *javaVM)
   {
   PORT_ACCESS_FROM_JAVAVM(javaVM);

   UDATA priority;
   priority = J9THREAD_PRIORITY_NORMAL;

   _metricsMonitor = TR::Monitor::create("JITServer-MetricsMonitor");
   if (_metricsMonitor)
      {
      // create the thread for listening to requests for metrics values
      const UDATA defaultOSStackSize = javaVM->defaultOSStackSize; //256KB stack size

      if (J9THREAD_SUCCESS != javaVM->internalVMFunctions->createJoinableThreadWithCategory(&_metricsOSThread,
                                                               defaultOSStackSize,
                                                               priority,
                                                               0,
                                                               &metricsThreadProc,
                                                               javaVM->jitConfig,
                                                               J9THREAD_CATEGORY_SYSTEM_JIT_THREAD))
         { // cannot create the MetricsServer thread
         j9tty_printf(PORTLIB, "Error: Unable to create JITServer MetricsServer Thread.\n");
         TR::Monitor::destroy(_metricsMonitor);
         _metricsMonitor = NULL;
         }
      else // must wait here until the thread gets created; otherwise an early shutdown
         { // does not know whether or not to destroy the thread
         _metricsMonitor->enter();
         while (!getAttachAttempted())
            _metricsMonitor->wait();
         _metricsMonitor->exit();
         if (!getMetricsThread())
            {
            j9tty_printf(PORTLIB, "Error: JITServer Metrics Thread attach failed.\n");
            }
         }
      }
   else
      {
      j9tty_printf(PORTLIB, "Error: Unable to create JITServer Metrics Monitor\n");
      }
   }

int32_t
MetricsServer::waitForMetricsThreadExit(J9JavaVM *javaVM)
   {
   if (NULL != _metricsOSThread)
      return omrthread_join(_metricsOSThread);
   else
      return 0;
   }

void
MetricsServer::stop()
   {
   if (getMetricsThread())
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Will stop the metrics thread");

      _metricsMonitor->enter();
      setMetricsThreadExitFlag();
      _metricsMonitor->wait();
      _metricsMonitor->exit();
      TR::Monitor::destroy(_metricsMonitor);
      _metricsMonitor = NULL;
      }
   }


int
MetricsServer::openSocketForListening(uint32_t port)
   {
   int sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
   if (sockfd < 0)
      {
      perror("can't open server socket");
      return sockfd;
      }

   // see `man 7 socket` for option explanations
   int flag = true;
   int rc = 0;
   rc = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&flag, sizeof(flag));
   if (rc < 0)
      {
      perror("Can't set SO_REUSEADDR");
      return rc;
      }
   rc =setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (void *)&flag, sizeof(flag));
   if (rc < 0)
      {
      perror("Can't set SO_KEEPALIVE");
      return rc;
      }

   struct sockaddr_in serv_addr;
   memset((char *)&serv_addr, 0, sizeof(serv_addr));
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
   serv_addr.sin_port = htons(port);

   rc = bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
   if (rc < 0)
      {
      perror("can't bind metrics port");
      return rc; // It is possible that the port is in use
      }
   rc = listen(sockfd, SOMAXCONN);
   if (rc < 0)
      {
      perror("listen for the metrics port failed");
      return rc;
      }
   return sockfd;
   }

void
MetricsServer::reArmSocketForReading(int sockIndex)
   {
   _pfd[sockIndex].events = POLLIN;
   _pfd[sockIndex].revents = 0;
   }

void
MetricsServer::reArmSocketForWriting(int sockIndex)
   {
   _pfd[sockIndex].events = POLLOUT;
   _pfd[sockIndex].revents = 0;
   }

void
MetricsServer::closeSocket(int sockIndex)
   {
   _requests[sockIndex].clear();
   close(_pfd[sockIndex].fd);
   _pfd[sockIndex].fd = -1;
   _pfd[sockIndex].revents = 0;
   _numActiveSockets--;
   }

void
MetricsServer::handleConnectionRequest()
   {
   static const int LISTEN_SOCKET = 0;
   TR_ASSERT_FATAL(_pfd[LISTEN_SOCKET].revents == POLLIN, "MetricsServer: Unexpected revent occurred during poll for new connection: revents=%d\n", _pfd[LISTEN_SOCKET].revents);
   struct sockaddr_in cli_addr;
   socklen_t clilen = sizeof(cli_addr);
   int sockfd = accept(_pfd[LISTEN_SOCKET].fd, (struct sockaddr *)&cli_addr, &clilen);
   if (sockfd >= 0) // success
      {
      int sockFlags = fcntl(sockfd, F_GETFL, 0);
      if (-1 == fcntl(sockfd, F_SETFL, sockFlags | O_NONBLOCK))
         {
         perror("MetricsServer error: Can't set the socket to be non-blocking");
         exit(1);
         }
      // Add this socket to the list (if space available)
      nfds_t k;
      for (k=1; k < 1 + MAX_CONCURRENT_REQUESTS; k++)
         {
         if (_requests[k].getRequestState() == HttpGetRequest::Inactive) // found empty slot
            {
            _pfd[k].fd = sockfd;
            _requests[k].setSockFd(sockfd);
            if (_sslCtx)
               {
               if (_requests[k].setupSSLConnection(_sslCtx))
                  {
                  _requests[k].setRequestState(HttpGetRequest::EstablishingSSLConnection);
                  }
               else
                  {
                  perror("MetricsServer error: Can't open SSL connection on socket");
                  _requests[k].clear();
                  _pfd[k].fd = -1;
                  k = 1 + MAX_CONCURRENT_REQUESTS; // Give up trying to connect
                  break;
                  }
               }
            else
               {
               _requests[k].setRequestState(HttpGetRequest::ReadingRequest);
               }
            reArmSocketForReading(k);
            _numActiveSockets++;
            break;
            }
         }
      if (k >= 1 + MAX_CONCURRENT_REQUESTS)
         {
         // We could not find any empty slot for our socket. Close it
         close(sockfd);
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "MetricsServer error: could not find an available socket to process a request");
         }
      }
   else // accept() failed
      {
      if ((EAGAIN != errno) && (EWOULDBLOCK != errno))
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "MetricsServer error: cannot accept connection: errno=%d", errno);
         }
      }
   // Prepare to wait for another connection request
   reArmSocketForReading(LISTEN_SOCKET);
   }

void
MetricsServer::handleDataForConnectedSocket(nfds_t sockIndex, MetricsDatabase &metricsDatabase)
   {
   // Check for errors first; we only expect the POLLIN or POLLOUT flag to be set
   if (_pfd[sockIndex].revents & (POLLRDHUP | POLLERR | POLLHUP | POLLNVAL))
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "MetricsServer error on socket %d revents=%d", _pfd[sockIndex].fd, _pfd[sockIndex].revents);
      closeSocket(sockIndex);
      }
   else if (_requests[sockIndex].getRequestState() == HttpGetRequest::EstablishingSSLConnection)
      {
      switch (_requests[sockIndex].acceptSSLConnection())
         {
         case HttpGetRequest::SSL_CONNECTION_ESTABLISHED:
            _requests[sockIndex].setRequestState(HttpGetRequest::ReadingRequest);
            reArmSocketForReading(sockIndex);
            break;
         case HttpGetRequest::WANT_READ:
            reArmSocketForReading(sockIndex);
            break;
         case HttpGetRequest::WANT_WRITE:
            reArmSocketForWriting(sockIndex);
            break;
         default: // An error occurred
            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               {
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "MetricsServer error on socket %d: Unable to establish SSL Connection", _pfd[sockIndex].fd);
               }
            closeSocket(sockIndex);
            break;
         }
      }
   else if (_requests[sockIndex].getRequestState() == HttpGetRequest::ReadingRequest)
      {
      HttpGetRequest::ReturnCodes rc = _requests[sockIndex].readHttpGetRequest();
      if (rc == HttpGetRequest::FULL_REQUEST_RECEIVED)
         {
         rc = _requests[sockIndex].parseHttpGetRequest();
         }
      switch (rc)
         {
         case HttpGetRequest::HTTP_OK:
            // Save the metric data response and wait to send it to requestor
            if (_requests[sockIndex].getPath() == HttpGetRequest::Path::Metrics)
               {
               _requests[sockIndex].setResponse(metricsDatabase.buildMetricHttpResponse());
               }
            else // Valid, but unrecognized request type
               {
               _requests[sockIndex].setResponse(std::string("HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n"));
               }
            _requests[sockIndex].setRequestState(HttpGetRequest::WritingResponse);
            reArmSocketForWriting(sockIndex);
            break;
         case HttpGetRequest::WANT_READ:
            reArmSocketForReading(sockIndex);
            break;
         case HttpGetRequest::WANT_WRITE:
            reArmSocketForWriting(sockIndex);
            break;
         default: // An error occurred
            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               {
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "MetricsServer experienced error code %d on socket index %u", rc, sockIndex);
               }
            _requests[sockIndex].setResponse(messageForErrorCode(rc));
            _requests[sockIndex].setRequestState(HttpGetRequest::WritingResponse);
            reArmSocketForWriting(sockIndex);
            break;
         }
      }
   else if (_requests[sockIndex].getRequestState() == HttpGetRequest::WritingResponse)
      {
      HttpGetRequest::ReturnCodes rc = _requests[sockIndex].sendHttpResponse();
      switch (rc)
         {
         case HttpGetRequest::FULL_RESPONSE_SENT:
            closeSocket(sockIndex);
            break;
         case HttpGetRequest::WANT_READ:
            reArmSocketForReading(sockIndex);
            break;
         case HttpGetRequest::WANT_WRITE:
            reArmSocketForWriting(sockIndex);
            break;
         default: // An error occurred
            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               {
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "MetricsServer error. Could not send reply.");
               }
            closeSocket(sockIndex);
            break;
         }
      }
   }


// Note: we close the sockets after each http request because otherwise the first 4
// agents that connect will hog the line and don't allow other agents to connect
void
MetricsServer::serveMetricsRequests()
   {
   TR::CompilationInfo *compInfo = TR::CompilationInfo::get(_jitConfig);
   TR::PersistentInfo *info = compInfo->getPersistentInfo();
   uint32_t port = info->getJITServerMetricsPort();
   int sockfd = openSocketForListening(port);
   if (sockfd < 0)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Cannot start MetricsServer. Will continue without.");
      return;
      }

   _pfd[0].fd = sockfd; // Add the socket descriptor for incomming connection requests
   reArmSocketForReading(0);
   _numActiveSockets = 1;

   MetricsDatabase metricsDatabase(compInfo);

   if (useSSL(compInfo))
      {
      auto &sslKeys = compInfo->getJITServerMetricsSslKeys();
      auto &sslCerts = compInfo->getJITServerMetricsSslCerts();
      std::string sslRootCerts = "";
      const char *sessionContextId = "MetricsServer";
      if (!JITServer::ServerStream::createSSLContext(_sslCtx, sessionContextId, sizeof(sessionContextId), sslKeys, sslCerts, sslRootCerts))
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Cannot create MetricsServer SSL context. Will continue without metrics.");
            }
         return;
         }
      }

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      {
      const char *logMessage = _sslCtx ? "MetricsServer waiting for https requests on port %u" : "MetricsServer waiting for http requests on port %u";
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, logMessage, port);
      }

   while (!getMetricsThreadExitFlag())
      {
      // Wait on all given sockets for some activity
      int rc = poll(_pfd, _numActiveSockets, METRICS_POLL_TIMEOUT);
      if (getMetricsThreadExitFlag()) // if we are exiting, no need to check poll() status
         {
         break;
         }
      else if (0 == rc) // poll() timed out and no fd is ready
         {
         // If read operations were in progress, we must close those descriptors
         // Note that if we enable keep-alive, then we should not consider this as a timeout
         for (nfds_t i=1; i < 1 + MAX_CONCURRENT_REQUESTS; i++)
            {
            HttpGetRequest::RequestState st = _requests[i].getRequestState();
            int sockfd = _pfd[i].fd;
            const char *logMessage;
            switch (_requests[i].getRequestState())
               {
               case HttpGetRequest::Inactive:
                  continue;
               case HttpGetRequest::EstablishingSSLConnection:
                  logMessage = "MetricsServer: Socket %d timed out while establishing SSL connection";
                  closeSocket(i);
                  break;
               case HttpGetRequest::ReadingRequest:
                  logMessage = "MetricsServer: Socket %d timed out while reading";
                  _requests[i].setResponse(messageForErrorCode(HttpGetRequest::REQUEST_TIMEOUT));
                  _requests[i].setRequestState(HttpGetRequest::WritingResponse);
                  reArmSocketForWriting(i);
                  break;
               case HttpGetRequest::WritingResponse:
                  logMessage = "MetricsServer: Socket %d timed out while writing";
                  closeSocket(i);
                  break;
               }
            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               {
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, logMessage, sockfd);
               }
            }
         continue; // Poll again waiting for a connection
         }
      else if (rc < 0) // Some error was encountered
         {
         if (errno == EINTR)
            {
            continue; // Poll again
            }
         else
            {
            perror("MetricsServer error in polling socket");
            exit(1);
            }
         }
      // At least one descriptor has incoming data
      int numSocketsChecked = 0;
      int numSocketsWithData = rc;
      for (nfds_t i = 0; i < 1 + MAX_CONCURRENT_REQUESTS && numSocketsChecked < numSocketsWithData; i++)
         {
         if (_pfd[i].fd >= 0 && _pfd[i].revents != 0) // Open socket with some data on it
            {
            numSocketsChecked++; // Increment number of active sockets checked
            if (i == 0) // First socket is used for new connection requests
               {
               handleConnectionRequest();
               }
            else // Socket 'i' has http data to read or write
               {
               handleDataForConnectedSocket(i, metricsDatabase);
               }
            }
         } // end for
      } // end while (!getMetricsThreadExitFlag())

   // The following piece of code will be executed only if the server shuts down properly
   if (_sslCtx)
      {
      (*OSSL_CTX_free)(_sslCtx);
      }
   closeSocket(0);
   }

std::string
MetricsServer::messageForErrorCode(int err)
   {
   std::string response;
   switch (err)
      {
      case HttpGetRequest::MALFORMED_REQUEST:
         response = std::string("HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n");
         break;
      case HttpGetRequest::INVALID_PATH:
         response = std::string("HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n");
         break;
      case HttpGetRequest::NO_GET_REQUEST:
         response = std::string("HTTP/1.1 405 Method Not Allowed\r\nConnection: close\r\n\r\n");
         break;
      case HttpGetRequest::REQUEST_TIMEOUT:
         response = std::string("HTTP/1.1 408 Request Timeout\r\nConnection: close\r\n\r\n");
         break;
      case HttpGetRequest::REQUEST_TOO_LARGE:
         response = std::string("HTTP/1.1 413 Payload Too Large\r\nConnection: close\r\n\r\n");
         break;
      case HttpGetRequest::REQUEST_URI_TOO_LONG:
         response = std::string("HTTP/1.1 414 URI Too Long\r\nConnection: close\r\n\r\n");
         break;
      case HttpGetRequest::INVALID_HTTP_PROTOCOL:
         response = std::string("HTTP/1.1 505 HTTP Version Not Supported\r\nConnection: close\r\n\r\n");
         break;
      default:
         response = std::string("HTTP/1.1 500 Internal Server Error\r\nConnection: close\r\n\r\n");
      }
   return response;
   }
