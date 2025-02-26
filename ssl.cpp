#include <openssl/ssl.h>
#include <openssl/err.h>

class SSLclass
{
private:

public:
SSL_CTX *create_context();
void configure_context(SSL_CTX *ctx);
SSL* create_ssl(SSL_CTX *ctx, int new_socket);
SSLclass();
~SSLclass();
};

SSL* SSLclass::create_ssl(SSL_CTX *ctx, int new_socket)
{
    SSL *ssl;
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, new_socket);
    if (SSL_accept(ssl) <= 0) 
    {
        fprintf(stderr, "SSL handshake failed.\n");
        ERR_print_errors_fp(stderr);
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(new_socket);
        return NULL;
    }
    printf("SSL handshake successful.\n");
    return ssl;
}

SSL_CTX* SSLclass::create_context()
{
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    method = TLS_server_method();

    ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    return ctx;
}

void SSLclass::configure_context(SSL_CTX *ctx)
{
    /* Set the key and cert */
    if (SSL_CTX_use_certificate_file(ctx, "certificates/cert.pem", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, "certificates/privkey.pem", SSL_FILETYPE_PEM) <= 0 ) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
}

SSLclass::SSLclass()
{
    SSL_load_error_strings();
    SSL_library_init();
    OpenSSL_add_ssl_algorithms();
}
SSLclass::~SSLclass()
{
    EVP_cleanup();
}

