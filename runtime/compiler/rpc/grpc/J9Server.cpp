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

#ifdef JITAAS_USE_GRPC

#include <chrono>
#include "J9Server.hpp"
#include "control/Options.hpp"
#include "env/VerboseLog.hpp"

bool
JITaaS::J9ServerStream::waitOnQueue()
   {
   void *tag;
   bool ok;
   if (_msTimeout == 0)
      return _cq->Next(&tag, &ok) && ok;

   auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(_msTimeout);
   grpc::CompletionQueue::NextStatus nextStatus = _cq->AsyncNext(&tag, &ok, deadline);
   return ok && nextStatus == grpc::CompletionQueue::GOT_EVENT;
   }

void
JITaaS::J9ServerStream::readBlocking()
   {
   auto tag = (void *)_streamNum;
   bool ok;
   _stream->Read(&_cMsg, tag);
   if (!waitOnQueue())
      throw JITaaS::StreamFailure("Server stream failure while doing readBlocking()");
   }

void
JITaaS::J9ServerStream::writeBlocking()
   {
   auto tag = (void *)_streamNum;
   bool ok;
   _stream->Write(_sMsg, tag);
   if (!waitOnQueue())
      throw JITaaS::StreamFailure("Server stream failure while doing writeBlocking()");
   }

void
JITaaS::J9ServerStream::finish()
   {
   auto tag = (void *)_streamNum;
   bool ok;
   _stream->Finish(Status::OK, tag);
   _cq->Next(&tag, &ok);
   drainQueue();
   acceptNewRPC();
   }

void
JITaaS::J9ServerStream::cancel()
   {
   auto tag = (void *)_streamNum;
   bool ok;
   _stream->Finish(Status::CANCELLED, tag);
   _cq->Next(&tag, &ok);
   drainQueue();
   acceptNewRPC();
   }

void
JITaaS::J9ServerStream::finishCompilation(uint32_t statusCode, std::string codeCache, std::string dataCache, CHTableCommitData chTableData, std::vector<TR_OpaqueClassBlock*> classesThatShouldNotBeNewlyExtended, std::string logFileStr)
   {
   try
      {
      write(J9ServerMessageType::compilationCode, statusCode, codeCache, dataCache, chTableData, classesThatShouldNotBeNewlyExtended, logFileStr);
      finish();
      }

   catch (const JITaaS::StreamFailure &e)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "Server-Client stream failed before finishing");
      // Cleanup using cancel instead of finish
      cancel();
      }
   }

void
JITaaS::J9ServerStream::acceptNewRPC()
   {
   _ctx.reset(new grpc::ServerContext);
   _stream.reset(new J9ServerReaderWriter(_ctx.get()));
   _cq.reset(new grpc::CompletionQueue);
   _service->RequestCompile(_ctx.get(), _stream.get(), _cq.get(), _notif, (void *)_streamNum);
   }

void
JITaaS::J9ServerStream::drainQueue()
   {
   void *tag;
   bool ok;
   _cq->Shutdown();
   while (_cq->Next(&tag, &ok));
   }

void
JITaaS::J9CompileServer::buildAndServe(J9BaseCompileDispatcher *compiler, TR::PersistentInfo *info)
   {
   grpc::ServerBuilder builder;
   auto &sslKeys = info->getJITaaSSslKeys();
   auto &sslCerts = info->getJITaaSSslCerts();
   auto &sslRootCerts = info->getJITaaSSslRootCerts();
   bool useSSL =  sslKeys.size() || sslCerts.size() || sslRootCerts.size();
   grpc::SslServerCredentialsOptions sslOpts(GRPC_SSL_REQUEST_AND_REQUIRE_CLIENT_CERTIFICATE_AND_VERIFY);
   if (useSSL)
      {
      sslOpts.pem_key_cert_pairs.reserve(sslKeys.size());
      TR_ASSERT_FATAL(sslKeys.size() == sslCerts.size(), "expecting the same number of keys and certs");
      for (size_t i = 0; i < sslKeys.size(); i++)
         sslOpts.pem_key_cert_pairs.emplace_back(grpc::SslServerCredentialsOptions::PemKeyCertPair { sslKeys.at(i), sslCerts.at(i) });
      sslOpts.pem_root_certs = sslRootCerts;
      }
   auto creds = useSSL ? grpc::SslServerCredentials(sslOpts) : grpc::InsecureServerCredentials();
   uint32_t port = info->getJITaaSServerPort();
   builder.AddListeningPort("0.0.0.0:" + std::to_string(port), creds);
   builder.RegisterService(&_service);
   _notificationQueue = builder.AddCompletionQueue();
   _server = builder.BuildAndStart();
   uint32_t timeout = info->getJITaaSTimeout();
   serve(compiler, timeout);
   }

void
JITaaS::J9CompileServer::serve(J9BaseCompileDispatcher *compiler, uint32_t timeout)
   {
   bool ok = false;
   void *tag;

   // JITaaS TODO: make this nicer; 7 should be stored in a const somewhere
   for (size_t i = 0; i < 7; ++i)
      {
      _streams.push_back(std::unique_ptr<J9ServerStream>(new J9ServerStream(i, &_service, _notificationQueue.get(), timeout)));
      }

   while (true)
      {
      _notificationQueue->Next(&tag, &ok);
      compiler->compile(_streams[(size_t)tag].get());
      }
   }

#endif
