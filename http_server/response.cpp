#include <fstream>
#include <sstream>
#include <string>
#include <iostream>

class Response {
public:
    Response();
    ~Response();

    void setStatusCode(int code);
    int getStatusCode() const;
    std::string buildResponse(std::string body, std::string mime_type);
    void setBody(const std::string& body);
    std::string getBody() const;
    std::string loadFile(const std::string& path);
    bool fileExists(const std::string& path);
private:
    int statusCode;
    std::string body;
};

Response::Response() : statusCode(200), body("") {}

Response::~Response() {}

void Response::setStatusCode(int code) {
    statusCode = code;
}

int Response::getStatusCode() const {
    return statusCode;
}

void Response::setBody(const std::string& body) {
    this->body = body;
}

std::string Response::getBody() const {
    return body;
}

std::string Response::buildResponse(std::string body, std::string mime_type) {
    std::string response = "HTTP/1.1 ";
    switch (statusCode) {
    case 200:
        response += "200 OK\r\n";
        break;
    case 404:
        response += "404 Not Found\r\n";
        break;
    case 500:
        response += "500 Internal Server Error\r\n";
        break;
    default:
        response += "200 OK\r\n";
    }
    response += "Content-Type: " + mime_type + "\r\n";
    response += "Content-Length: " + std::to_string(body.length()) + "\r\n";
    response += "\r\n";
    response += body;

    return response;
}


std::string Response::loadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Could not open file: " + path);
    }

    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

bool Response::fileExists(const std::string& path) {
    std::ifstream file(path);
    return file.is_open();
}
