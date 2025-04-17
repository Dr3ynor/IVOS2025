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
#include <vector>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>


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

    Server();
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;



    public:
    static Server& getInstance();

    void create_socket();
    void bind_socket();
    void listen_socket();
    void run();
    void send_response(SSL *ssl, HttpRequest http_request);
    void send_response(SSL *ssl, std::string path);
    void handle_client(SSL *ssl);
    std::string get_mime_type(const std::string& file_path);

    ConsoleLogger console_logger;
    FileLogger file_logger;
    Logger logger;
    std::map<std::string, std::string> mime_types;

    int worker_sockets[MAX_WORKERS][2];
    void create_workers();
    void create_logger();
    std::vector<int> workers;
    int logger_worker;
    int log_queue_id;
    int recv_fd(int socket);
    void send_fd(int socket, int fd);
};

struct log_msg {
    long mtype;
    char text[256];
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
    printf("handle_client start\n");
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
        /*else if(bytes_read < 0)
        {
            perror("read");
            break;
        }*/

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
    //logger.log("Response message sent");
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
        //logger.log("File found at: " + std::string(INDEX_PATH)); 
    } 
    else 
    {
        body = response.loadFile(FILE_NOT_FOUND_PATH);
        //logger.log("File not found: " + http_request.path);
    }
    if (http_request.path == "/") 
    {
        body = response.loadFile(INDEX_PATH);
        //logger.log("Index file requested");
    }

    std::string response_message = response.buildResponse(body, mime_type);
    SSL_write(ssl, response_message.c_str(), response_message.length());
    //logger.log("Response message sent");
}
void Server::create_workers()
{
    for (int i = 0; i < MAX_WORKERS; i++) 
    {
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, worker_sockets[i]) < 0) 
        {
            perror("socketpair failed");
            exit(EXIT_FAILURE);
        }

        pid_t pid = fork();
        workers.push_back(pid);
        if (pid == -1) 
        {
            perror("fork");
            exit(1);
        }
        
        if (pid == 0)
        {
            int new_socket;
            new_socket = recv_fd(worker_sockets[i][1]);
            SSL *ssl = sslclass.create_ssl(ctx, new_socket);
            handle_client(ssl);
        }
        else
        {
            printf("Worker PID: %d\n", pid);
            close(new_socket);
            close(worker_sockets[i][1]);
        }
    }
}
void Server::create_logger()
{
    pid_t pid = fork();
    logger_worker = pid;
    if (pid == -1) 
    {
        perror("fork");
        exit(1);
    }
    if (pid == 0)
    {
        FILE* log_file = fopen("server.log", "a");
        if (!log_file) exit(EXIT_FAILURE);
        
        log_msg msg;
        while (msgrcv(log_queue_id, &msg, sizeof(msg.text), 0, 0) > 0) {
            fprintf(log_file, "%s\n", msg.text);
            fflush(log_file);
        }
        fclose(log_file);
        exit(0);
    }
    else
    {
        perror("fork");
    }
}



void Server::run()
{
    create_workers();
    create_logger();

    while ((new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen))) 
    {
        for (int i = 0; i < MAX_WORKERS; i++) 
        {
            if (workers[i] > 0) 
            {
                send_fd(worker_sockets[i][0], new_socket);
                break;
            }
        }

    }
    SSL_CTX_free(ctx);
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






void Server::send_fd(int socket, int fd) {
    struct msghdr msg = {0};
    struct cmsghdr *cmsg;
    char buf[CMSG_SPACE(sizeof(fd))];
    memset(buf, 0, sizeof(buf));
    struct iovec io = { .iov_base = (void*)"", .iov_len = 1 };

    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = buf;
    msg.msg_controllen = sizeof(buf);

    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
    *((int *) CMSG_DATA(cmsg)) = fd;

    sendmsg(socket, &msg, 0);
}

int Server::recv_fd(int socket) {
    struct msghdr msg = {0};
    struct cmsghdr *cmsg;
    char buf[CMSG_SPACE(sizeof(int))];
    struct iovec io = { .iov_base = (void*)"", .iov_len = 1 };

    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = buf;
    msg.msg_controllen = sizeof(buf);

    if (recvmsg(socket, &msg, 0) < 0) {
        return -1;
    }

    cmsg = CMSG_FIRSTHDR(&msg);
    return *((int *) CMSG_DATA(cmsg));
}
