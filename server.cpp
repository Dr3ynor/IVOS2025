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

#define PORT 8080

#include "response.cpp"
#include "requestparser.cpp"
#include "logger.cpp"

#define INDEX_PATH "www/index.html"
#define FILE_NOT_FOUND_PATH "www/404.html"


Logger logger;

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


void handle_client(int client_socket) 
{
    Response response;
    std::string body;
    char buffer[1024] = {0};
    read(client_socket, buffer, 1024);
    RequestParser request_parser(buffer);
    HttpRequest http_request = request_parser.parse();
    std::string file_path = "www" + http_request.path;
    std::string mime_type = get_mime_type(file_path);;

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

    dprintf(client_socket, "%s", response.buildResponse(body,mime_type).c_str());
    logger.log("Response message sent");
}



int main()
{
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
        pid_t pid = fork();
        if (pid == 0) // Child
        {
            close(server_fd);
            handle_client(new_socket);
            exit(0);
        } else // Parent
        {
            close(new_socket);
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

    return 0;
}
