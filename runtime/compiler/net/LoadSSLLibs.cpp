/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#include "LoadSSLLibs.hpp"

#include <dlfcn.h>
#include <string.h>

#include "control/Options.hpp"

#define OPENSSL_VERSION_1_0 "OpenSSL 1.0."
#define OPENSSL_VERSION_1_1 "OpenSSL 1.1."

OOpenSSL_version_t * OOpenSSL_version = NULL;

OSSL_load_error_strings_t * OSSL_load_error_strings = NULL;
OSSL_library_init_t * OSSL_library_init = NULL;
OOPENSSL_init_ssl_t * OOPENSSL_init_ssl = NULL;

OSSLv23_server_method_t * OSSLv23_server_method = NULL;
OSSLv23_client_method_t * OSSLv23_client_method = NULL;

OSSL_CTX_set_ecdh_auto_t * OSSL_CTX_set_ecdh_auto = NULL;
OSSL_CTX_ctrl_t * OSSL_CTX_ctrl = NULL;

OBIO_ctrl_t * OBIO_ctrl = NULL;

OSSL_CIPHER_get_name_t * OSSL_CIPHER_get_name = NULL;
OSSL_get_current_cipher_t * OSSL_get_current_cipher = NULL;
OSSL_get_cipher_t * OSSL_get_cipher = NULL;

OEVP_cleanup_t * OEVP_cleanup = NULL;

Osk_num_t * Osk_num = NULL;
Osk_value_t * Osk_value = NULL;
Osk_pop_free_t * Osk_pop_free = NULL;

Osk_X509_INFO_num_t * Osk_X509_INFO_num = NULL;
Osk_X509_INFO_value_t * Osk_X509_INFO_value = NULL;
Osk_X509_INFO_pop_free_t * Osk_X509_INFO_pop_free = NULL;

OSSL_new_t * OSSL_new = NULL;
OSSL_free_t * OSSL_free = NULL;
OSSL_set_connect_state_t * OSSL_set_connect_state = NULL;
OSSL_set_accept_state_t * OSSL_set_accept_state = NULL;
OSSL_set_fd_t * OSSL_set_fd = NULL;
OSSL_get_version_t * OSSL_get_version = NULL;
OSSL_accept_t * OSSL_accept = NULL;
OSSL_connect_t * OSSL_connect = NULL;
OSSL_get_peer_certificate_t * OSSL_get_peer_certificate = NULL;
OSSL_get_verify_result_t * OSSL_get_verify_result = NULL;

OSSL_CTX_new_t * OSSL_CTX_new = NULL;
OSSL_CTX_set_session_id_context_t * OSSL_CTX_set_session_id_context = NULL;
OSSL_CTX_use_PrivateKey_t * OSSL_CTX_use_PrivateKey = NULL;
OSSL_CTX_use_certificate_t * OSSL_CTX_use_certificate = NULL;
OSSL_CTX_check_private_key_t * OSSL_CTX_check_private_key = NULL;
OSSL_CTX_set_verify_t * OSSL_CTX_set_verify = NULL;
OSSL_CTX_free_t * OSSL_CTX_free = NULL;
OSSL_CTX_get_cert_store_t * OSSL_CTX_get_cert_store = NULL;

OBIO_new_mem_buf_t * OBIO_new_mem_buf = NULL;
OBIO_free_all_t * OBIO_free_all = NULL;
OBIO_new_ssl_t * OBIO_new_ssl = NULL;
OBIO_write_t * OBIO_write = NULL;
OBIO_read_t * OBIO_read = NULL;

OPEM_read_bio_PrivateKey_t * OPEM_read_bio_PrivateKey = NULL;
OPEM_read_bio_X509_t * OPEM_read_bio_X509 = NULL;
OPEM_X509_INFO_read_bio_t * OPEM_X509_INFO_read_bio = NULL;

OX509_INFO_free_t * OX509_INFO_free = NULL;
OX509_STORE_add_cert_t * OX509_STORE_add_cert = NULL;
OX509_STORE_add_crl_t * OX509_STORE_add_crl = NULL;
OX509_free_t * OX509_free = NULL;

OERR_print_errors_fp_t * OERR_print_errors_fp = NULL;

int OSSL102_OOPENSSL_init_ssl(uint64_t opts, const void * settings)
   {
   // Does not exist in 1.0.2. Should not be called directly outside this file
   return 0;
   }

void OSSL110_load_error_strings(void)
   {
   // Do nothing here:
   // SSL_load_error_strings() is deprecated in OpenSSL 1.1.0 by OPENSSL_init_ssl().
   // CommunicationStream::initSSL() will call SSL_library_init. In 1.1.0 SSL_library_init
   // is a macro of OPENSSL_init_ssl.
   }

int OSSL110_library_init(void)
   {
   return (*OOPENSSL_init_ssl)(0, NULL);
   }

#define OPENSSL102_SSL_CTRL_SET_ECDH_AUTO 94
long OSSL102_CTX_set_ecdh_auto(SSL_CTX *ctx, int onoff)
   {
   return (*OSSL_CTX_ctrl)(ctx, OPENSSL102_SSL_CTRL_SET_ECDH_AUTO, onoff, NULL);
   }

long OSSL110_CTX_set_ecdh_auto(SSL_CTX *ctx, int onoff)
   {
   return ((onoff) != 0);
   }

const char * handle_SSL_get_cipher(const SSL *ssl)
   {
   return (*OSSL_CIPHER_get_name)((*OSSL_get_current_cipher)(ssl));
   }

void OEVP110_cleanup(void)
   {
   // In versions prior to 1.1.0 EVP_cleanup() removed all ciphers
   // and digests from the table. It no longer has any effect in OpenSSL 1.1.0
   }

# define OPENSSL102_CHECKED_STACK_OF(type, p) \
    ((_STACK*) (1 ? p : (STACK_OF(type)*)0))

# define OPENSSL102_CHECKED_SK_FREE_FUNC(type, p) \
    ((void (*)(void *)) ((1 ? p : (void (*)(type *))0)))

typedef void (*OPENSSL110_sk_freefunc)(void *);

int Osk102_X509_INFO_num(const STACK_OF(X509_INFO) *st)
   {
   //# define sk_X509_INFO_num(st) SKM_sk_num(X509_INFO, (st))
   //# define SKM_sk_num(type, st) \
   //  sk_num(CHECKED_STACK_OF(type, st))
   return (*Osk_num)(OPENSSL102_CHECKED_STACK_OF(X509_INFO, st));
   }

int Osk110_X509_INFO_num(const STACK_OF(X509_INFO) *st)
   {
   // static ossl_inline int sk_##t1##_num(const STACK_OF(t1) *sk) \
   // { \
   //     return OPENSSL_sk_num((const OPENSSL_STACK *)sk); \
   // }
   return (*Osk_num)((const _STACK *)st);
   }

X509_INFO * Osk102_X509_INFO_value(const STACK_OF(X509_INFO) *st, int i)
   {
   //# define sk_X509_INFO_value(st, i) SKM_sk_value(X509_INFO, (st), (i))
   //# define SKM_sk_value(type, st,i) \
   //     ((type *)sk_value(CHECKED_STACK_OF(type, st), i))
   return (X509_INFO *)(*Osk_value)(OPENSSL102_CHECKED_STACK_OF(X509_INFO, st), i);
   }

X509_INFO * Osk110_X509_INFO_value(const STACK_OF(X509_INFO) *st, int i)
   {
   // static ossl_inline t2 *sk_##t1##_value(const STACK_OF(t1) *sk, int idx) \
   // { \
   //     return (t2 *)OPENSSL_sk_value((const OPENSSL_STACK *)sk, idx); \
   // }
   return (X509_INFO *)(*Osk_value)((const _STACK *)st, i);
   }

void Osk102_X509_INFO_pop_free(STACK_OF(X509_INFO) *st, OX509_INFO_free_t *X509InfoFreeFunc)
   {
   //# define sk_X509_INFO_pop_free(st, free_func) SKM_sk_pop_free(X509_INFO, (st), (free_func))
   //# define SKM_sk_pop_free(type, st, free_func) \
   //     sk_pop_free(CHECKED_STACK_OF(type, st), CHECKED_SK_FREE_FUNC(type, free_func))
   (*Osk_pop_free)(OPENSSL102_CHECKED_STACK_OF(X509_INFO, st), OPENSSL102_CHECKED_SK_FREE_FUNC(X509_INFO, X509InfoFreeFunc));
   }

void Osk110_X509_INFO_pop_free(STACK_OF(X509_INFO) *st, OX509_INFO_free_t *X509InfoFreeFunc)
   {
   // static ossl_inline void sk_##t1##_pop_free(STACK_OF(t1) *sk, sk_##t1##_freefunc freefunc) \
   // { \
   //     OPENSSL_sk_pop_free((OPENSSL_STACK *)sk, (OPENSSL_sk_freefunc)freefunc); \
   // }
   (*Osk_pop_free)((_STACK *)st, (OPENSSL110_sk_freefunc)X509InfoFreeFunc);
   }

namespace JITServer
{
void *loadLibssl()
   {
   void *result = NULL;

   // Library names for OpenSSL 1.1.1, 1.1.0, 1.0.2 and symbolic links
   static const char * const libNames[] =
      {
      "libssl.so.1.1",   // 1.1.x library name
      "libssl.so.1.0.0", // 1.0.x library name
      "libssl.so.10",    // 1.0.x library name on RHEL
      "libssl.so"        // general symlink library name
      };

   int numOfLibraries = sizeof(libNames) / sizeof(libNames[0]);

   for (int i = 0; i < numOfLibraries; ++i)
      {
      result = dlopen(libNames[i], RTLD_NOW);

      if (result)
         {
         return result;
         }
      }
   return result;
   }

void unloadLibssl(void *handle)
   {
   (void)dlclose(handle);
   }

void * findLibsslSymbol(void *handle, const char *symName)
   {
   return dlsym(handle, symName);
   }

int findLibsslVersion(void *handle)
{
   const char * openssl_version = NULL;
   int ossl_ver = -1;

   OOpenSSL_version = (OOpenSSL_version_t*)findLibsslSymbol(handle, "OpenSSL_version");

   if (OOpenSSL_version)
      {
      openssl_version = (*OOpenSSL_version)(0);
      if (0 == strncmp(openssl_version, OPENSSL_VERSION_1_1, strlen(OPENSSL_VERSION_1_1)))
         {
         ossl_ver = 1;
         }
      }
   else
      {
      OOpenSSL_version = (OOpenSSL_version_t*)findLibsslSymbol(handle, "SSLeay_version");

      if (OOpenSSL_version)
         {
         openssl_version = (*OOpenSSL_version)(0);
         if (0 == strncmp(openssl_version, OPENSSL_VERSION_1_0, strlen(OPENSSL_VERSION_1_0)))
            {
            ossl_ver = 0;
            }
         }
      }

   return ossl_ver;
}

#if defined(DEBUG)
void dbgPrintSymbols()
   {
   printf("=============================================================\n");

   printf(" OpenSSL_version %p\n", OOpenSSL_version);

   printf(" SSL_load_error_strings %p\n", OSSL_load_error_strings);
   printf(" SSL_library_init %p\n", OSSL_library_init);
   printf(" OPENSSL_init_ssl %p\n", OOPENSSL_init_ssl);

   printf(" SSLv23_server_method %p\n", OSSLv23_server_method);
   printf(" SSLv23_client_method %p\n", OSSLv23_client_method);

   printf(" SSL_CTX_set_ecdh_auto %p\n", OSSL_CTX_set_ecdh_auto);
   printf(" SSL_CTX_ctrl %p\n", OSSL_CTX_ctrl);

   printf(" BIO_ctrl %p\n",  OBIO_ctrl);

   printf(" SSL_CIPHER_get_name %p\n",  OSSL_CIPHER_get_name);
   printf(" SSL_get_current_cipher %p\n",  OSSL_get_current_cipher);

   printf(" EVP_cleanup %p\n",  OEVP_cleanup);

   printf(" sk_num %p\n",  Osk_num);
   printf(" sk_value %p\n",  Osk_value);
   printf(" sk_pop_free %p\n",  Osk_pop_free);

   printf(" X509_INFO_free %p\n",  OX509_INFO_free);
   printf(" sk_X509_INFO_num %p\n",  Osk_X509_INFO_num);
   printf(" sk_X509_INFO_value %p\n",  Osk_X509_INFO_value);
   printf(" sk_X509_INFO_pop_free %p\n",  Osk_X509_INFO_pop_free);

   printf(" SSL_new %p\n", OSSL_new);
   printf(" SSL_free %p\n", OSSL_free);
   printf(" SSL_set_connect_state %p\n", OSSL_set_connect_state);
   printf(" SSL_set_accept_state %p\n", OSSL_set_accept_state);
   printf(" SSL_set_fd %p\n", OSSL_set_fd);
   printf(" SSL_get_version %p\n", OSSL_get_version);
   printf(" SSL_accept %p\n", OSSL_accept);
   printf(" SSL_connect %p\n", OSSL_connect);
   printf(" SSL_get_peer_certificate %p\n", OSSL_get_peer_certificate);
   printf(" SSL_get_verify_result %p\n", OSSL_get_verify_result);

   printf(" SSL_CTX_new %p\n", OSSL_CTX_new);
   printf(" SSL_CTX_set_session_id_context %p\n", OSSL_CTX_set_session_id_context);
   printf(" SSL_CTX_use_PrivateKey %p\n", OSSL_CTX_use_PrivateKey);
   printf(" SSL_CTX_use_certificate %p\n", OSSL_CTX_use_certificate);
   printf(" SSL_CTX_check_private_key %p\n", OSSL_CTX_check_private_key);
   printf(" SSL_CTX_set_verify %p\n", OSSL_CTX_set_verify);
   printf(" SSL_CTX_free %p\n", OSSL_CTX_free);
   printf(" SSL_CTX_get_cert_store %p\n", OSSL_CTX_get_cert_store);

   printf(" BIO_new_mem_buf %p\n", OBIO_new_mem_buf);
   printf(" BIO_free_all %p\n", OBIO_free_all);
   printf(" BIO_new_ssl %p\n", OBIO_new_ssl);
   printf(" BIO_write %p\n", OBIO_write);
   printf(" BIO_read %p\n", OBIO_read);

   printf(" PEM_read_bio_PrivateKey %p\n", OPEM_read_bio_PrivateKey);
   printf(" PEM_read_bio_X509 %p\n", OPEM_read_bio_X509);
   printf(" PEM_X509_INFO_read_bio %p\n", OPEM_X509_INFO_read_bio);

   printf(" X509_STORE_add_cert %p\n", OX509_STORE_add_cert);
   printf(" X509_STORE_add_crl %p\n", OX509_STORE_add_crl);
   printf(" X509_free %p\n", OX509_free);

   printf(" ERR_print_errors_fp %p\n", OERR_print_errors_fp);

   printf("=============================================================\n\n");
   }
#endif /* defined(DEBUG) */

bool loadLibsslAndFindSymbols()
{
   void *handle = NULL;

   handle = loadLibssl();
   if (!handle)
      {
      printf("#JITServer: Failed to load libssl\n");
      return false;
      }

   int ossl_ver = findLibsslVersion(handle);
   if (-1 == ossl_ver)
      {
      printf("#JITServer: Failed to find a correct version of libssl\n");
      unloadLibssl(handle);
      return false;
      }


   if (0 == ossl_ver)
      {
      OOPENSSL_init_ssl = &OSSL102_OOPENSSL_init_ssl;

      OSSL_load_error_strings = (OSSL_load_error_strings_t *)findLibsslSymbol(handle, "SSL_load_error_strings");
      OSSL_library_init = (OSSL_library_init_t *)findLibsslSymbol(handle, "SSL_library_init");

      OSSLv23_server_method = (OSSLv23_server_method_t *)findLibsslSymbol(handle, "SSLv23_server_method");
      OSSLv23_client_method = (OSSLv23_client_method_t *)findLibsslSymbol(handle, "SSLv23_client_method");

      OSSL_CTX_set_ecdh_auto = &OSSL102_CTX_set_ecdh_auto;

      OEVP_cleanup = (OEVP_cleanup_t *)findLibsslSymbol(handle, "EVP_cleanup");

      Osk_num = (Osk_num_t *)findLibsslSymbol(handle, "sk_num");
      Osk_value = (Osk_value_t *)findLibsslSymbol(handle, "sk_value");
      Osk_pop_free = (Osk_pop_free_t *)findLibsslSymbol(handle, "sk_pop_free");

      Osk_X509_INFO_num = &Osk102_X509_INFO_num;
      Osk_X509_INFO_value = &Osk102_X509_INFO_value;
      Osk_X509_INFO_pop_free = &Osk102_X509_INFO_pop_free;
      }
   else
      {
      OOPENSSL_init_ssl = (OOPENSSL_init_ssl_t *)findLibsslSymbol(handle, "OPENSSL_init_ssl");

      OSSL_load_error_strings = &OSSL110_load_error_strings;
      OSSL_library_init = &OSSL110_library_init;

      OSSLv23_server_method = (OSSLv23_server_method_t *)findLibsslSymbol(handle, "TLS_server_method");
      OSSLv23_client_method = (OSSLv23_client_method_t *)findLibsslSymbol(handle, "TLS_client_method");

      OSSL_CTX_set_ecdh_auto = &OSSL110_CTX_set_ecdh_auto;

      OEVP_cleanup = &OEVP110_cleanup;

      Osk_num = (Osk_num_t *)findLibsslSymbol(handle, "OPENSSL_sk_num");
      Osk_value = (Osk_value_t *)findLibsslSymbol(handle, "OPENSSL_sk_value");
      Osk_pop_free = (Osk_pop_free_t *)findLibsslSymbol(handle, "OPENSSL_sk_pop_free");

      Osk_X509_INFO_num = &Osk110_X509_INFO_num;
      Osk_X509_INFO_value = &Osk110_X509_INFO_value;
      Osk_X509_INFO_pop_free = &Osk110_X509_INFO_pop_free;
      }

   OSSL_CTX_ctrl = (OSSL_CTX_ctrl_t *)findLibsslSymbol(handle, "SSL_CTX_ctrl");
   OBIO_ctrl = (OBIO_ctrl_t *)findLibsslSymbol(handle, "BIO_ctrl");

   OSSL_CIPHER_get_name = (OSSL_CIPHER_get_name_t *)findLibsslSymbol(handle, "SSL_CIPHER_get_name");
   OSSL_get_current_cipher = (OSSL_get_current_cipher_t *)findLibsslSymbol(handle, "SSL_get_current_cipher");
   OSSL_get_cipher = &handle_SSL_get_cipher;


   OSSL_new = (OSSL_new_t *)findLibsslSymbol(handle, "SSL_new");
   OSSL_free = (OSSL_free_t *)findLibsslSymbol(handle, "SSL_free");
   OSSL_set_connect_state = (OSSL_set_connect_state_t *)findLibsslSymbol(handle, "SSL_set_connect_state");
   OSSL_set_accept_state = (OSSL_set_accept_state_t *)findLibsslSymbol(handle, "SSL_set_accept_state");
   OSSL_set_fd = (OSSL_set_fd_t *)findLibsslSymbol(handle, "SSL_set_fd");
   OSSL_get_version = (OSSL_get_version_t *)findLibsslSymbol(handle, "SSL_get_version");
   OSSL_accept = (OSSL_accept_t *)findLibsslSymbol(handle, "SSL_accept");
   OSSL_connect = (OSSL_connect_t *)findLibsslSymbol(handle, "SSL_connect");
   OSSL_get_peer_certificate = (OSSL_get_peer_certificate_t *)findLibsslSymbol(handle, "SSL_get_peer_certificate");
   OSSL_get_verify_result = (OSSL_get_verify_result_t *)findLibsslSymbol(handle, "SSL_get_verify_result");

   OSSL_CTX_new = (OSSL_CTX_new_t *)findLibsslSymbol(handle, "SSL_CTX_new");
   OSSL_CTX_set_session_id_context = (OSSL_CTX_set_session_id_context_t *)findLibsslSymbol(handle, "SSL_CTX_set_session_id_context");
   OSSL_CTX_use_PrivateKey = (OSSL_CTX_use_PrivateKey_t *)findLibsslSymbol(handle, "SSL_CTX_use_PrivateKey");
   OSSL_CTX_use_certificate = (OSSL_CTX_use_certificate_t *)findLibsslSymbol(handle, "SSL_CTX_use_certificate");
   OSSL_CTX_check_private_key = (OSSL_CTX_check_private_key_t *)findLibsslSymbol(handle, "SSL_CTX_check_private_key");
   OSSL_CTX_set_verify = (OSSL_CTX_set_verify_t *)findLibsslSymbol(handle, "SSL_CTX_set_verify");
   OSSL_CTX_free = (OSSL_CTX_free_t *)findLibsslSymbol(handle, "SSL_CTX_free");
   OSSL_CTX_get_cert_store = (OSSL_CTX_get_cert_store_t *)findLibsslSymbol(handle, "SSL_CTX_get_cert_store");

   OBIO_new_mem_buf = (OBIO_new_mem_buf_t *)findLibsslSymbol(handle, "BIO_new_mem_buf");
   OBIO_free_all = (OBIO_free_all_t *)findLibsslSymbol(handle, "BIO_free_all");
   OBIO_new_ssl = (OBIO_new_ssl_t *)findLibsslSymbol(handle, "BIO_new_ssl");
   OBIO_write = (OBIO_write_t *)findLibsslSymbol(handle, "BIO_write");
   OBIO_read = (OBIO_read_t *)findLibsslSymbol(handle, "BIO_read");

   OPEM_read_bio_PrivateKey = (OPEM_read_bio_PrivateKey_t *)findLibsslSymbol(handle, "PEM_read_bio_PrivateKey");
   OPEM_read_bio_X509 = (OPEM_read_bio_X509_t *)findLibsslSymbol(handle, "PEM_read_bio_X509");
   OPEM_X509_INFO_read_bio = (OPEM_X509_INFO_read_bio_t *)findLibsslSymbol(handle, "PEM_X509_INFO_read_bio");

   OX509_INFO_free = (OX509_INFO_free_t *)findLibsslSymbol(handle, "X509_INFO_free");
   OX509_STORE_add_cert = (OX509_STORE_add_cert_t *)findLibsslSymbol(handle, "X509_STORE_add_cert");
   OX509_STORE_add_crl = (OX509_STORE_add_crl_t *)findLibsslSymbol(handle, "X509_STORE_add_crl");
   OX509_free = (OX509_free_t *)findLibsslSymbol(handle, "X509_free");

   OERR_print_errors_fp = (OERR_print_errors_fp_t *)findLibsslSymbol(handle, "ERR_print_errors_fp");

   if (
       (OOpenSSL_version == NULL) ||

       (OSSL_load_error_strings == NULL) ||
       (OSSL_library_init == NULL) ||
       (OOPENSSL_init_ssl == NULL) ||

       (OSSLv23_server_method == NULL) ||
       (OSSLv23_client_method == NULL) ||

       (OEVP_cleanup == NULL) ||

       (OSSL_CTX_ctrl == NULL) ||
       (OBIO_ctrl == NULL) ||

       (Osk_num == NULL) ||
       (Osk_value == NULL) ||
       (Osk_pop_free == NULL) ||

       (OSSL_CIPHER_get_name == NULL) ||
       (OSSL_get_current_cipher == NULL) ||

       (OSSL_new == NULL) ||
       (OSSL_free == NULL) ||
       (OSSL_set_connect_state == NULL) ||
       (OSSL_set_accept_state == NULL) ||
       (OSSL_set_fd == NULL) ||
       (OSSL_get_version == NULL) ||
       (OSSL_accept == NULL) ||
       (OSSL_connect == NULL) ||
       (OSSL_get_peer_certificate == NULL) ||
       (OSSL_get_verify_result == NULL) ||

       (OSSL_CTX_new == NULL) ||
       (OSSL_CTX_set_session_id_context == NULL) ||
       (OSSL_CTX_use_PrivateKey == NULL) ||
       (OSSL_CTX_use_certificate == NULL) ||
       (OSSL_CTX_check_private_key == NULL) ||
       (OSSL_CTX_set_verify == NULL) ||
       (OSSL_CTX_free == NULL) ||
       (OSSL_CTX_get_cert_store == NULL) ||

       (OBIO_new_mem_buf == NULL) ||
       (OBIO_free_all == NULL) ||
       (OBIO_new_ssl == NULL) ||
       (OBIO_write == NULL) ||
       (OBIO_read == NULL) ||

       (OPEM_read_bio_PrivateKey == NULL) ||
       (OPEM_read_bio_X509 == NULL) ||
       (OPEM_X509_INFO_read_bio == NULL) ||

       (OX509_INFO_free == NULL) ||
       (OX509_STORE_add_cert == NULL) ||
       (OX509_STORE_add_crl == NULL) ||
       (OX509_free == NULL) ||

       (OERR_print_errors_fp == NULL)
      )
      {
      printf("#JITServer: Failed to load all the required OpenSSL symbols\n");
#if defined(DEBUG)
      dbgPrintSymbols();
#endif /* defined(DEBUG) */
      unloadLibssl(handle);
      return false;
      }

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Built against (%s); Loaded with (%s)\n",
         OPENSSL_VERSION_TEXT, (*OOpenSSL_version)(0));

   return true;
}

}; // JITServer
