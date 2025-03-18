#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <fstream>
#include <map>
#include <vector>
#include <sys/wait.h>

#include <csignal>

#include "requestparser.cpp"
#include "response.cpp"

#define INDEX_PATH          "www/index.html"
#define FILE_NOT_FOUND_PATH "www/404.html"
#define SERVICE_UNAVAILABLE "www/503.html"
#define LOG_FILE            "server.log"

#define PORT 8080
#define WORKER_COUNT 1
#define LOG_QUEUE_KEY 1234

std::vector<pid_t> workers;
pid_t logger_pid;
int msg_queue_id;
int server_fd;

void cleanup(int signo) {
    std::cout << "Shutting down server..." << std::endl;

    while(waitpid(-1, nullptr, 0) > 0);
    {
        std::cout << "Waiting for child processes to finish..." << std::endl;
    }

    close(server_fd);
    msgctl(msg_queue_id, IPC_RMID, nullptr);

    std::cout << "Server shutdown complete." << std::endl;
    exit(0);
}


void setup_signal_handler() {
    struct sigaction sa;
    sa.sa_handler = cleanup;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
}


std::map<std::string, std::string> mime_types = 
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

struct LogMessage {
    long type;
    char text[256];
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

SSL_CTX* create_ssl_context() 
{
    SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) 
    {
        perror("SSL_CTX_new failed");
        exit(EXIT_FAILURE);
    }
    SSL_CTX_set_ecdh_auto(ctx, 1);
    if (SSL_CTX_use_certificate_file(ctx, "../certificates/cert.pem", SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ctx, "../certificates/privkey.pem", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    return ctx;
}

void log_message(int msg_queue_id, const std::string& message) {
    LogMessage log_msg;
    log_msg.type = 1;
    strncpy(log_msg.text, message.c_str(), sizeof(log_msg.text) - 1);
    log_msg.text[sizeof(log_msg.text) - 1] = '\0';
    msgsnd(msg_queue_id, &log_msg, sizeof(log_msg.text), 0);
}

void logger_process(int msg_queue_id) {
    std::ofstream log_file(LOG_FILE, std::ios::app);
    printf("Logger process started\n");
    if (!log_file.is_open()) {
        perror("Failed to open log file");
        exit(EXIT_FAILURE);
    }
    while (true) {
        LogMessage log_msg;
        if (msgrcv(msg_queue_id, &log_msg, sizeof(log_msg.text), 1, 0) > 0) {
            log_file << log_msg.text << std::endl;
            log_file.flush();
        }
    }
}

void handle_client(SSL* ssl, int msg_queue_id) 
{
    char buffer[1024] = {0};
    int bytes = SSL_read(ssl, buffer, sizeof(buffer));

    RequestParser request_parser = RequestParser(buffer);
    HttpRequest http_request = request_parser.parse();

    Response response;
    std::string body;
    std::string file_path = "../www" + http_request.path;
    std::string mime_type = get_mime_type(file_path);
    if (response.fileExists(file_path)) 
    {
        body = response.loadFile(file_path);
        log_message(msg_queue_id, "File found: " + file_path);
    } 
    else 
    {
        body = response.loadFile(FILE_NOT_FOUND_PATH);
        log_message(msg_queue_id, "File not found: " + http_request.path);
    }
    if (http_request.path == "/") 
    {
        body = response.loadFile(INDEX_PATH);
        log_message(msg_queue_id, "Index file requested");
    }

    std::string response_message = response.buildResponse(body, mime_type);
    SSL_write(ssl, response_message.c_str(), response_message.length());

    SSL_shutdown(ssl);
    SSL_free(ssl);
}

void worker_process(int sock_fd, SSL_CTX* ctx, int msg_queue_id) 
{
    while (true) 
    {
        struct msghdr msg = {};
        char buf[CMSG_SPACE(sizeof(int))] = {};
        struct iovec io = { buf, sizeof(buf) };
        msg.msg_iov = &io;
        msg.msg_iovlen = 1;
        msg.msg_control = buf;
        msg.msg_controllen = sizeof(buf);

        if (recvmsg(sock_fd, &msg, 0) < 0) 
        {
            perror("recvmsg failed");
            exit(EXIT_FAILURE);
        }
        struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
        int client_sock = *(int*)CMSG_DATA(cmsg);
        SSL* ssl = SSL_new(ctx);
        SSL_set_fd(ssl, client_sock);
        if (SSL_accept(ssl) <= 0) 
        {
            ERR_print_errors_fp(stderr);
            SSL_free(ssl);
            close(client_sock);
            continue;
        }
        handle_client(ssl, msg_queue_id);
    }
}

int main() 
{
    setup_signal_handler();
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    SSL_CTX* ctx = create_ssl_context();
    msg_queue_id = msgget(LOG_QUEUE_KEY, IPC_CREAT | 0666);
    
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address = {AF_INET, htons(PORT), INADDR_ANY};
    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, SOMAXCONN);
    
    logger_pid = fork();
    if (logger_pid == 0) {
        logger_process(msg_queue_id);
        exit(0);
    }
    
    int worker_pipes[WORKER_COUNT][2];
    for (int i = 0; i < WORKER_COUNT; i++) 
    {
        socketpair(AF_UNIX, SOCK_STREAM, 0, worker_pipes[i]);
        pid_t pid = fork();
        if (pid == 0) {
            close(worker_pipes[i][1]);
            worker_process(worker_pipes[i][0], ctx, msg_queue_id);
            exit(0);
        }
        workers.push_back(pid);
        close(worker_pipes[i][0]);
    }
    
    int next_worker = 0;
    while (true) 
    {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_sock = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        
        struct msghdr msg = {};
        struct iovec io = { (void*)"", 1 };
        msg.msg_iov = &io;
        msg.msg_iovlen = 1;
        char buf[CMSG_SPACE(sizeof(int))] = {};
        msg.msg_control = buf;
        msg.msg_controllen = sizeof(buf);
        struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));
        *((int *) CMSG_DATA(cmsg)) = client_sock;
        
        sendmsg(worker_pipes[next_worker][1], &msg, 0);
        next_worker = (next_worker + 1) % WORKER_COUNT;
        close(client_sock);
    }
}
