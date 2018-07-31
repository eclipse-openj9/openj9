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
