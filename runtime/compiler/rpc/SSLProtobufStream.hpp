#ifndef SSL_PROTOBUF_STREAM_HPP
#define SSL_PROTOBUF_STREAM_HPP

#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

class SSLOutputStream : public google::protobuf::io::CopyingOutputStream
   {
public:
   SSLOutputStream(BIO *bio)
      : _ssl(bio)
      {
      }
   virtual ~SSLOutputStream() override
      {
      }
   virtual bool Write(const void *buffer, int size) override
      {
      TR_ASSERT(size, "writing zero bytes is undefined behavior");
      int n = BIO_write(_ssl, buffer, size);
      if (n <= 0)
         {
         ERR_print_errors_fp(stderr);
         return false;
         }
      /*if (BIO_flush(_ssl) != 1)
         {
         perror("flushing");
         ERR_print_errors_fp(stderr);
         return false;
         }*/
      TR_ASSERT(n == size, "not expecting partial write");
      return true;
      }

private:
   BIO *_ssl;
   };

class SSLInputStream : public google::protobuf::io::CopyingInputStream
   {
public:
   SSLInputStream(BIO *bio)
      : _ssl(bio)
      {
      }
   virtual ~SSLInputStream() override
      {
      }
   virtual int Read(void *buffer, int size) override
      {
      TR_ASSERT(size, "reading zero bytes is undefined behavior");
      int n = BIO_read(_ssl, buffer, size);
      if (n <= 0)
         {
         ERR_print_errors_fp(stderr);
         }
      return n;
      }

private:
   BIO *_ssl;
   };

#endif // SSL_PROTOBUF_STREAM_HPP
