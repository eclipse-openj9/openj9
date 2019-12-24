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
#ifndef LOAD_SSL_LIBS_H
#define LOAD_SSL_LIBS_H

#include <stdint.h>
#include <openssl/ssl.h>

typedef const char * OOpenSSL_version_t(int);

typedef void OSSL_load_error_strings_t(void);
typedef int OSSL_library_init_t(void);
typedef int OOPENSSL_init_ssl_t(uint64_t opts, const void * settings);

typedef const SSL_METHOD * OSSLv23_server_method_t(void);
typedef const SSL_METHOD * OSSLv23_client_method_t(void);

typedef long OSSL_CTX_set_ecdh_auto_t(SSL_CTX *ctx, int onoff);
typedef long OSSL_CTX_ctrl_t(SSL_CTX *ctx, int cmd, long larg, void *parg);

typedef long OBIO_ctrl_t(BIO *bp, int cmd, long larg, void *parg);

typedef const char * OSSL_CIPHER_get_name_t(const SSL_CIPHER *c);
typedef const SSL_CIPHER * OSSL_get_current_cipher_t(const SSL *s);
typedef const char * OSSL_get_cipher_t(const SSL *s);

typedef void OEVP_cleanup_t(void);

typedef int Osk_num_t(const _STACK *);
typedef void * Osk_value_t(const _STACK *, int);
typedef void * Osk_pop_free_t(_STACK *st, void (*func) (void *));

typedef void OX509_INFO_free_t(X509_INFO *a);
typedef int Osk_X509_INFO_num_t(const STACK_OF(X509_INFO) *st);
typedef X509_INFO * Osk_X509_INFO_value_t(const STACK_OF(X509_INFO) *st, int i);
typedef void Osk_X509_INFO_pop_free_t(STACK_OF(X509_INFO) *st, OX509_INFO_free_t* X509InfoFreeFunc);

typedef SSL * OSSL_new_t(SSL_CTX *ctx);
typedef void OSSL_free_t(SSL *ssl);
typedef void OSSL_set_connect_state_t(SSL *ssl);
typedef void OSSL_set_accept_state_t(SSL *ssl);
typedef int OSSL_set_fd_t(SSL *ssl, int fd);
typedef const char * OSSL_get_version_t(const SSL *ssl);
typedef int OSSL_accept_t(SSL *ssl);
typedef int OSSL_connect_t(SSL *ssl);
typedef X509 * OSSL_get_peer_certificate_t(const SSL *ssl);
typedef long OSSL_get_verify_result_t(const SSL *ssl);

typedef SSL_CTX * OSSL_CTX_new_t(const SSL_METHOD *method);
typedef int OSSL_CTX_set_session_id_context_t(SSL_CTX *ctx, const unsigned char *sid_ctx, unsigned int sid_ctx_len);
typedef int OSSL_CTX_use_PrivateKey_t(SSL_CTX *ctx, EVP_PKEY *pkey);
typedef int OSSL_CTX_use_certificate_t(SSL_CTX *ctx, X509 *x);
typedef int OSSL_CTX_check_private_key_t(const SSL_CTX *ctx);
typedef void OSSL_CTX_set_verify_t(SSL_CTX *ctx, int mode, int (*verify_callback)(int, X509_STORE_CTX *));
typedef void OSSL_CTX_free_t(SSL_CTX *ctx);
typedef X509_STORE * OSSL_CTX_get_cert_store_t(const SSL_CTX *ctx);

typedef BIO * OBIO_new_mem_buf_t(const void *buf, int len);
typedef void OBIO_free_all_t(BIO *a);
typedef BIO * OBIO_new_ssl_t(SSL_CTX *ctx, int client);
typedef int OBIO_write_t(BIO *b, const void *data, int dlen);
typedef int OBIO_read_t(BIO *b, void *data, int dlen);

typedef EVP_PKEY * OPEM_read_bio_PrivateKey_t(BIO *bp, EVP_PKEY **x, pem_password_cb *cb, void *u);
typedef X509 * OPEM_read_bio_X509_t(BIO *bp, X509 **x, pem_password_cb *cb, void *u);
typedef STACK_OF(X509_INFO) *OPEM_X509_INFO_read_bio_t(BIO *bp, STACK_OF(X509_INFO) *sk, pem_password_cb *cb, void *u);

typedef int OX509_STORE_add_cert_t(X509_STORE *ctx, X509 *x);
typedef int OX509_STORE_add_crl_t(X509_STORE *ctx, X509_CRL *x);
typedef void OX509_free_t(X509 *a);

typedef void OERR_print_errors_fp_t(FILE *fp);

extern "C" OOpenSSL_version_t * OOpenSSL_version;

extern "C" OSSL_load_error_strings_t * OSSL_load_error_strings;
extern "C" OSSL_library_init_t * OSSL_library_init;

extern "C" OSSLv23_server_method_t * OSSLv23_server_method;
extern "C" OSSLv23_client_method_t * OSSLv23_client_method;

extern "C" OSSL_CTX_set_ecdh_auto_t * OSSL_CTX_set_ecdh_auto;

extern "C" OBIO_ctrl_t * OBIO_ctrl;

extern "C" OSSL_get_cipher_t * OSSL_get_cipher;

extern "C" OEVP_cleanup_t * OEVP_cleanup;

extern "C" OX509_INFO_free_t * OX509_INFO_free;
extern "C" Osk_X509_INFO_num_t * Osk_X509_INFO_num;
extern "C" Osk_X509_INFO_value_t * Osk_X509_INFO_value;
extern "C" Osk_X509_INFO_pop_free_t * Osk_X509_INFO_pop_free;

extern "C" OSSL_new_t * OSSL_new;
extern "C" OSSL_free_t * OSSL_free;
extern "C" OSSL_set_connect_state_t * OSSL_set_connect_state;
extern "C" OSSL_set_accept_state_t * OSSL_set_accept_state;
extern "C" OSSL_set_fd_t * OSSL_set_fd;
extern "C" OSSL_get_version_t * OSSL_get_version;
extern "C" OSSL_accept_t * OSSL_accept;
extern "C" OSSL_connect_t * OSSL_connect;
extern "C" OSSL_get_peer_certificate_t * OSSL_get_peer_certificate;
extern "C" OSSL_get_verify_result_t * OSSL_get_verify_result;

extern "C" OSSLv23_server_method_t * OSSLv23_server_method;
extern "C" OSSLv23_client_method_t * OSSLv23_client_method;

extern "C" OSSL_CTX_new_t * OSSL_CTX_new;
extern "C" OSSL_CTX_set_session_id_context_t * OSSL_CTX_set_session_id_context;
extern "C" OSSL_CTX_use_PrivateKey_t * OSSL_CTX_use_PrivateKey;
extern "C" OSSL_CTX_use_certificate_t * OSSL_CTX_use_certificate;
extern "C" OSSL_CTX_check_private_key_t * OSSL_CTX_check_private_key;
extern "C" OSSL_CTX_set_verify_t * OSSL_CTX_set_verify;
extern "C" OSSL_CTX_free_t * OSSL_CTX_free;
extern "C" OSSL_CTX_get_cert_store_t * OSSL_CTX_get_cert_store;

extern "C" OBIO_new_mem_buf_t * OBIO_new_mem_buf;
extern "C" OBIO_free_all_t * OBIO_free_all;
extern "C" OBIO_new_ssl_t * OBIO_new_ssl;
extern "C" OBIO_write_t * OBIO_write;
extern "C" OBIO_read_t * OBIO_read;

extern "C" OPEM_read_bio_PrivateKey_t * OPEM_read_bio_PrivateKey;
extern "C" OPEM_read_bio_X509_t * OPEM_read_bio_X509;
extern "C" OPEM_X509_INFO_read_bio_t * OPEM_X509_INFO_read_bio;

extern "C" OX509_STORE_add_cert_t * OX509_STORE_add_cert;
extern "C" OX509_STORE_add_crl_t * OX509_STORE_add_crl;
extern "C" OX509_free_t * OX509_free;

extern "C" OEVP_cleanup_t * OEVP_cleanup;

extern "C" OERR_print_errors_fp_t * OERR_print_errors_fp;

namespace JITServer
{
void * loadLibssl();
void   unloadLibssl(void * handle);
void * findLibsslSymbol(void * handle, const char * symName);

bool loadLibsslAndFindSymbols();
};
#endif // LOAD_SSL_LIBS_H
