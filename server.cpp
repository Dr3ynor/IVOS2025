#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string>
#include <map>

#include "ssl.cpp"
#include "response.cpp"
#include "requestparser.cpp"
#include "logger_strategy.cpp"

#include "defs.h"

class Server
{
    private:
    SSLclass sslclass;
    struct sockaddr_in address;
    socklen_t addrlen= sizeof(address);
    int opt = 1;
    int server_fd, new_socket;
    SSL_CTX *ctx;
    sem_t *sem = nullptr;

    Server();
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;



    public:
    static Server& getInstance();

    void create_socket();
    void bind_socket();
    void listen_socket();
    void semaphore_unlink_and_open();
    void run();
    void send_response(SSL *ssl, HttpRequest http_request);
    void send_response(SSL *ssl, std::string path);
    void handle_client(SSL *ssl);
    std::string get_mime_type(const std::string& file_path);

    ConsoleLogger console_logger;
    FileLogger file_logger;
    Logger logger;


    std::map<std::string, std::string> mime_types;


};

Server& Server::getInstance()
{
    static Server instance;
    return instance;
}


void Server::create_socket()
{
    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) 
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
}

void Server::bind_socket()
{
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) 
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
}

void Server::listen_socket()
{
    if (listen(server_fd, 3) < 0) 
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
}

void Server::semaphore_unlink_and_open()
{
    sem_unlink(SEM_NAME);
    sem = sem_open(SEM_NAME, O_RDWR | O_CREAT, 0666, MAX_CLIENTS);
    if (sem == SEM_FAILED) 
    {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
}

std::string Server::get_mime_type(const std::string& file_path) 
{
    size_t pos = file_path.find_last_of('.');
    if (pos != std::string::npos) 
    {
        std::string ext = file_path.substr(pos);
        if (mime_types.count(ext)) 
        {
            return mime_types[ext];
        }
    }
    return "text/html";
}


void Server::handle_client(SSL *ssl)
{
    int bytes_read;
    while(1)
    {
        char buffer[1024] = {0};
        printf("Waiting for client request...\n");

        bytes_read = SSL_read(ssl, buffer, 1024-1);
        printf("Bytes read: %d\n", bytes_read);
        if(bytes_read == 0)
        {
            break;
        }
        else if(bytes_read < 0)
        {
            perror("read");
            break;
        }

        RequestParser request_parser(buffer);
        HttpRequest http_request = request_parser.parse();
        send_response(ssl, http_request);
    }
    SSL_free(ssl);
}

void Server::send_response(SSL *ssl, std::string file_path)
{
    Response response;
    std::string body = response.loadFile(file_path);
    std::string mime_type = get_mime_type(file_path);
    std::string response_message = response.buildResponse(body, mime_type);
    SSL_write(ssl, response_message.c_str(), response_message.length());
    logger.log("Response message sent");
}


void Server::send_response(SSL *ssl, HttpRequest http_request)
{
    Response response;
    std::string body;
    std::string file_path = "www" + http_request.path;
    std::string mime_type = get_mime_type(file_path);
    if (response.fileExists(file_path)) 
    {
        body = response.loadFile(file_path);
        logger.log("File found at: " + std::string(INDEX_PATH)); 
    } 
    else 
    {
        body = response.loadFile(FILE_NOT_FOUND_PATH);
        logger.log("File not found: " + http_request.path);
    }
    if (http_request.path == "/") 
    {
        body = response.loadFile(INDEX_PATH);
        logger.log("Index file requested");
    }

    std::string response_message = response.buildResponse(body, mime_type);
    SSL_write(ssl, response_message.c_str(), response_message.length());
    logger.log("Response message sent");
}

void Server::run()
{
    while ((new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen))) 
    {
        SSL *ssl =sslclass.create_ssl(ctx, new_socket);

        if(sem_trywait(sem) == -1)
        {
            send_response(ssl, "www/503.html");

            perror("sem_trywait");
            SSL_shutdown(ssl);
            SSL_free(ssl);
            close(new_socket);
            continue;
        }

        pid_t pid = fork();
        if (pid == 0) // Child
        {
            close(server_fd);
            handle_client(ssl);

            
            close(new_socket);
            sem_post(sem);
            exit(EXIT_SUCCESS);
        }
        else if (pid > 0) // Parent
        {
            close(new_socket);
        }
        else
        {
            perror("fork failed");
        }
    }
    SSL_CTX_free(ctx);
    sem_close(sem);
    sem_unlink(SEM_NAME);
}




Server::Server() : sslclass(), ctx(sslclass.create_context()), logger(&file_logger)
{

    sslclass = SSLclass();
    sslclass.configure_context(ctx);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    mime_types = 
    {
        {".html", "text/html"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".gif", "image/gif"},
        {".svg", "image/svg+xml"},
        {".ico", "image/x-icon"},
    };

}
