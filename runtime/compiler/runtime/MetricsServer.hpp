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

#ifndef METRICSSERVER_HPP
#define METRICSSERVER_HPP

#include <poll.h> // for struct pollfd
#include <string>
#include "j9.h" // for J9JavaVM
#include "infra/Monitor.hpp"  // for TR::Monitor

namespace TR { class CompilationInfo; }

/**
   @class PrometheusMetric
   @brief Abstraction for a class capable of serializing a metric in a format understood by Prometheus

   PrometheusMetric is an abstract class and concrete classes need to be derived from it.
   Derived classes need to implement the `computeValue()` function and possibly the
   destructor, if they allocate memory dynamically.
 */
class PrometheusMetric
   {
   public:
   PrometheusMetric(const std::string &name, const std::string &help) :_name(name), _help(help) {}
   virtual ~PrometheusMetric() {}
   /**
      @brief Compute the value of the metric that is to be monitored and cache it
      @param compInfo Pointer to CompilationInfo object which may used to gather the metric of interest
      @return The value of the metric
   */
   virtual double computeValue(TR::CompilationInfo *compInfo) = 0;
   const std::string &getName() const { return _name; }
   const std::string &getHelp() const { return _help; }
   double getValue() const { return _value; }
   void setValue(double v) { _value = v; }
   /**
      @brief Build a std::string that encodes the value of the metric in a format understood by Prometheus
      @return Serialized value of the metric (as a std::string)
   */
   std::string serialize()
      {
      return "# HELP " + getName() + " " + getHelp() + "\n# TYPE " + getName() + " gauge\n" + getName() + " " + std::to_string(getValue()) + "\n";
      }

   protected:
   const std::string _name;
   const std::string _help;
   double _value;
   }; // class PrometheusMetric

/**
   @brief Class used to serialize CPU utilization of OpenJ9, as a metric understood by Prometheus
 */
class CPUUtilMetric : public PrometheusMetric
   {
public:
   CPUUtilMetric() : PrometheusMetric("jitserver_cpu_utilization", "Cpu utilization of the JITServer")
      {}
   virtual double computeValue(TR::CompilationInfo *compInfo);
   }; // class CPUUtilMetric

/**
   @brief Class used to serialize physical memory available to an OpenJ9 JVM, as a metric understood by Prometheus
 */
class AvailableMemoryMetric : public PrometheusMetric
   {
public:
   AvailableMemoryMetric() : PrometheusMetric("jitserver_available_memory", "Available memory for JITServer")
      {}
   virtual double computeValue(TR::CompilationInfo *compInfo);
   }; // class AvailableMemoryMetric

/**
   @brief Class used to serialize the number of clients connected to JITServer, as a metric understood by Prometheus
 */
class ConnectedClientsMetric : public PrometheusMetric
   {
public:
   ConnectedClientsMetric() : PrometheusMetric("jitserver_connected_clients", "Number of connected clients")
      {}
   virtual double computeValue(TR::CompilationInfo *compInfo);
   }; // class ConnectedClientsMetric

/**
   @brief Class used to serialize the number of active compilation threads in OpenJ9, as a metric understood by Prometheus
 */
class ActiveThreadsMetric : public PrometheusMetric
   {
public:
   ActiveThreadsMetric() : PrometheusMetric("jitserver_active_threads", "Number of active compilation threads")
      {}
   virtual double computeValue(TR::CompilationInfo *compInfo);
   }; // class ActiveThreadsMetric


/**
   @class MetricsDatabase
   @brief Collection of metrics that need to be sent to Prometheus on demand

   In order to add a new metric, derive a new class from PrometheusMetric and implement its
   computeValue() method. Increment the MAX_METRICS constant accordingly. Change the constructor
   of this class to dynamically allocate an instance of the new metric and store a pointer
   of this metric instance into the _metrics array
 */
class MetricsDatabase
   {
   public:
   static const size_t MAX_METRICS = 4; // Maximum number of metrics our database can hold
   MetricsDatabase(TR::CompilationInfo *compInfo);
   ~MetricsDatabase();

   /**
      @brief Build a std::string that serializes the values of all the metrics in the database.

      The returned string represents a valid HTTP response. Example of output:

      HTTP/1.1 200 OK
      Content - type : text / plain; version = 0.0.4; charset = utf - 8
      Content - length: 12014
      CR LF
      Rest of message body contains all the serialized metrics
   */
   std::string serializeMetrics();

   std::string buildMetricHttpResponse()
      {
      std::string serializedMetrics = serializeMetrics();
      return "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-type: text/plain; version=0.0.4; charset=utf-8\r\nContent-length: " +
         std::to_string(serializedMetrics.size()) + "\r\n\r\n" + serializedMetrics;
      }

   private:
   PrometheusMetric *_metrics[MAX_METRICS]; // Array with pointers to metrics to be scrapped
   TR::CompilationInfo *_compInfo;
   }; // MetricsDatabase

/**
   @class HttpGetRequest
   @brief Class used for receiving and parsing HTTP GET requests

   Note that this class has the bare minimum functionality to parse GET requests
   received from Prometheus. It will respond with an error if anything other than
   GET is received or if the URI is anything else other than "/metrics".
   Any headers that are received in the HTTP requests are ignored. If we wanted to
   add them later-on, the the best approach would be a map like
   std::map<std::string, std::string> _headers;
 */
class HttpGetRequest
   {
public:
   enum Path
      {
      Undefined = 0,
      Metrics,
      Liveness,
      Readiness,
      };
   enum ReturnCodes
      {
      SSL_CONNECTION_ESTABLISHED = 0,
      FULL_REQUEST_RECEIVED = 0,
      FULL_RESPONSE_SENT    = 0,
      WANT_READ    = -1,
      WANT_WRITE   = -2,
      SSL_CONNECTION_ERROR = -3,

      HTTP_OK               = 0,
      MALFORMED_REQUEST     = -400,
      INVALID_PATH          = -404,
      NO_GET_REQUEST        = -405,
      REQUEST_TIMEOUT       = -408,
      REQUEST_TOO_LARGE     = -413,
      REQUEST_URI_TOO_LONG  = -414,
      READ_ERROR            = -500,
      WRITE_ERROR           = -500,
      INVALID_HTTP_PROTOCOL = -505,
      };
   enum RequestState
      {
      Inactive = 0,
      EstablishingSSLConnection,
      ReadingRequest,
      WritingResponse
      };

   HttpGetRequest() : _requestState(Inactive), _sockfd(-1), _path(Path::Undefined), _msgLength(0), _ssl(NULL),
                      _incompleteSSLConnection(NULL), _responseBytesSent(0)
      {}
   HttpGetRequest(const HttpGetRequest &other)
      {
      if (this != &other)
         {
         _sockfd = other.getSockFd();
         _requestState = other.getRequestState();
         _msgLength = other.getMsgLength();
         memcpy(_buf, other._buf, other.getMsgLength());
         if (_ssl)
            {
            (*OBIO_free_all)(_ssl);
            }
         _ssl = other._ssl;
         _response = other._response;
         _responseBytesSent = other._responseBytesSent;
         _incompleteSSLConnection = other._incompleteSSLConnection;
         // The following are not really needed
         _path = other.getPath();
         memcpy(_httpVersion, other._httpVersion, 4);
         }
      }
   ~HttpGetRequest();
   void clear();
   RequestState getRequestState() const { return _requestState; }
   void setRequestState(RequestState requestState) { _requestState = requestState; }
   int getSockFd() const { return _sockfd; }
   void setSockFd(int sockfd) { _sockfd = sockfd; }
   void setSSL(BIO *ssl) { _ssl = ssl; }
   BIO *getSSL() const { return _ssl; }
   void setIncompleteSSLConnection(SSL *conn) { _incompleteSSLConnection = conn; }
   size_t getMsgLength() const { return _msgLength; }
   Path getPath() const { return _path; }
   void setResponse(std::string response) { _response = response; _responseBytesSent = 0; }

   bool setupSSLConnection(SSL_CTX *sslCtx);
   /**
      @brief Accept an SSL connection
      @return An error code: SSL_CONNECTION_ERROR, WANT_READ, WANT_WRITE, SSL_CONNECTION_ESTABLISHED
   */
   ReturnCodes acceptSSLConnection();
   /**
      @brief Read from a socket and validate that the received data is a valid HTTP GET request
      @return An error code: READ_ERROR, WANT_READ, WANT_WRITE, REQUEST_TOO_LARGE, NO_GET_REQUEST, FULL_REQUEST_RECEIVED
   */
   ReturnCodes readHttpGetRequest();
   /**
      @brief Parse the HTTP GET request held in the internal buffer
      Note that only "/metrics" URIs are accepted at this moment
      @return An error code: MALFORMED_REQUEST, REQUEST_URI_TOO_LONG, INVALID_PATH, INVALID_HTTP_PROTOCOL, HTTP_OK
   */
   ReturnCodes parseHttpGetRequest();
   /**
      @brief Write the response to the socket
      @return An error code: WRITE_ERROR, WANT_READ, WANT_WRITE, FULL_RESPONSE_SENT
   */
   ReturnCodes sendHttpResponse();
private:
   static const size_t BUF_SZ = 1024; // Size of internal buffer for receiving data
   static const size_t MAX_PATH_LENGTH = 16; // Max size of the URI received

   bool handleSSLConnectionError(const char *errMsg);

   RequestState _requestState;
   int _sockfd; // Socket descriptor for reading the data
   Path _path; // Enum that encodes well defined paths like "/metrics" or "/liveness" or "/readiness"
   char _httpVersion[4]; // 1.0 1.1  2.0, etc
   size_t _msgLength; // How much of the buffer is filled
   char _buf[BUF_SZ]; // Buffer for holding incoming data
   BIO *_ssl;
   SSL *_incompleteSSLConnection; // Stored in _ssl, but kept separate for accepting an SSL connection

   std::string _response;
   size_t _responseBytesSent;
   }; // class HttpRequest

/**
   @class MetricsServer
   @brief Class used to provide internal metrics to a Prometheus agent.

   This class (together with the helper classes) implements a very simple HTTP server
   that responds to HTTP requests from a metric scraping agent like Prometheus.
   Only "GET /metrics" is accepted at this moment and any headers are ignored.
   The response is an HTTP reply that encapsulates values for the supported metrics,
   in an OpenMetrics format that is understood by Prometheus. HTTP connections are always
   closed after the reply is sent and currently they are not encrypted.
   The code runs in a single, dedicated thread. Up to 4 requests can be handled
   concurrently, through the use of the 'poll' mechanism (with a timeout of 250 ms).
   The port on which the MetricsServer operates is specified with "-XX:JITServerMetricsPort=<NNN>".
 */
class MetricsServer
   {
public:
   MetricsServer();
   static MetricsServer* allocate();
   void startMetricsThread(J9JavaVM *javaVM);
   void stop();
   int32_t waitForMetricsThreadExit(J9JavaVM *javaVM);
   void setAttachAttempted(bool b) { _metricsThreadAttachAttempted = b; }
   bool getAttachAttempted() const { return _metricsThreadAttachAttempted; }
   J9VMThread* getMetricsThread() const { return _metricsThread; }
   void setMetricsThread(J9VMThread* thread) { _metricsThread = thread; }
   j9thread_t getMetricsOSThread() const { return _metricsOSThread; }
   TR::Monitor* getMetricsMonitor() const { return _metricsMonitor; }
   bool getMetricsThreadExitFlag() const { return _metricsThreadExitFlag; }
   void setMetricsThreadExitFlag() { _metricsThreadExitFlag = true; }
   void setJITConfig(J9JITConfig *jitConfig) { _jitConfig = jitConfig; }
   void serveMetricsRequests();

   static const int METRICS_POLL_TIMEOUT = 250; // ms
   static const size_t MAX_CONCURRENT_REQUESTS = 4;
   static const uint32_t SEND_TIMEOUT = 500; // ms

private:
   int openSocketForListening(uint32_t port);
   std::string messageForErrorCode(int err);
   void handleConnectionRequest();
   void handleDataForConnectedSocket(nfds_t i, MetricsDatabase &metricsDatabase);
   void reArmSocketForReading(int sockIndex);
   void reArmSocketForWriting(int sockIndex);
   void closeSocket(int sockIndex);
   void freeSSLConnection(int sockIndex);
   bool useSSL(TR::CompilationInfo *compInfo);

   J9VMThread *_metricsThread;
   TR::Monitor *_metricsMonitor;
   j9thread_t _metricsOSThread;
   volatile bool _metricsThreadAttachAttempted;
   volatile bool _metricsThreadExitFlag;
   J9JITConfig * _jitConfig;

   nfds_t _numActiveSockets = 0;
   struct pollfd _pfd[1 + MAX_CONCURRENT_REQUESTS] = {{0}}; // poll file descriptors; first entry is for connection requests
   HttpGetRequest _requests[1 + MAX_CONCURRENT_REQUESTS];

   SSL_CTX *_sslCtx;
   }; // class MetricsServer

#endif // #ifndef METRICSSERVER_HPP
