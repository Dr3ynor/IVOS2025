// Server side C program to demonstrate Socket
// programming
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <string.h>
#include <thread>
#include <map>
#include <wait.h>

#include <semaphore.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PORT 8080

#include "response.cpp"
#include "requestparser.cpp"
#include "logger.cpp"
#include "ssl.cpp"

#define INDEX_PATH "www/index.html"
#define FILE_NOT_FOUND_PATH "www/404.html"
#define SEM_NAME    "/semaphore"
#define MAX_CLIENTS 10

Logger logger;
sem_t *sem = nullptr;
SSLclass sslclass = SSLclass();

std::map<std::string, std::string> mime_types = 
{
    {".html", "text/html"},
    {".css", "text/css"},
    {".js", "application/javascript"},
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".gif", "image/gif"},
    {".svg", "image/svg+xml"},
};

std::string get_mime_type(const std::string& file_path) 
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


void handle_client(SSL *ssl) 
{
    Response response;
    std::string body;
    char buffer[1024] = {0};
    printf("Waiting for client request...\n");

    int bytes_read = SSL_read(ssl, buffer, 1024);
    printf("Bytes read: %d\n", bytes_read);
    printf("Buffer content: %s\n", buffer);
    //read(client_socket, buffer, 1024);
    RequestParser request_parser(buffer);
    HttpRequest http_request = request_parser.parse();
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



    SSL_write(ssl, response.buildResponse(body,mime_type).c_str(), response.buildResponse(body,mime_type).size());
    //dprintf(client_socket, "%s", response.buildResponse(body,mime_type).c_str());
    logger.log("Response message sent");
}



int main()
{
    SSL_CTX *ctx = sslclass.create_context();
    sslclass.configure_context(ctx);

    sem = sem_open(SEM_NAME, O_RDWR | O_CREAT, 0666, MAX_CLIENTS);
    if (sem == SEM_FAILED) 
    {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

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

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Forcefully attaching socket to the port 8080
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) 
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) 
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // Fork
    
    while ((new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen))) 
    {
        SSL *ssl;
        ssl = SSL_new(ctx);
        SSL_set_fd(ssl, new_socket);
        if (SSL_accept(ssl) <= 0) 
        {
            fprintf(stderr, "SSL handshake failed.\n");
            ERR_print_errors_fp(stderr);
            close(new_socket);
            continue;
        }
        printf("SSL handshake successful.\n");

        
        //sem_wait(sem);

        pid_t pid = fork();
        if (pid == 0) // Child
        {
            close(server_fd);
            handle_client(ssl);
            SSL_shutdown(ssl);
            SSL_free(ssl);
            close(new_socket);
            // sem_post(sem);
            exit(0);
        }
        else if (pid > 0) // Parent
        {
            close(new_socket); // Rodič zavře socket a pokračuje
        }
        else
        {
            perror("fork failed");
        }
    }
    

    // Threads
    
    /*
    while ((new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen))) 
    {
        std::thread clientThread(handle_client, new_socket);
        clientThread.detach();
    }
    */
    SSL_CTX_free(ctx);
    //sem_close(sem);
    //sem_unlink(SEM_NAME);
    return 0;
}
