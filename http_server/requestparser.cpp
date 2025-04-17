#include <string>
#include <iostream>
#include <sstream>
#include <unordered_map>

struct HttpRequest {
    std::string method;
    std::string path;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
};

class RequestParser {
public:
    RequestParser(const std::string& request) : request_(request) {}
    HttpRequest parse();

private:
    HttpRequest http_request;
    std::string request_;
};

HttpRequest RequestParser::parse()
{
    printf("--------------------\n");
    printf("%s\n", request_.c_str());
    printf("--------------------\n");

    std::istringstream stream(request_);
    HttpRequest request;
    std::string line;
    
    // Parse request line
    if (std::getline(stream, line)) {
        std::istringstream line_stream(line);
        line_stream >> request.method >> request.path >> request.version;
    }
    
    // Parse headers
    while (std::getline(stream, line) && !line.empty()) {
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);
            while (!value.empty() && (value[0] == ' ' || value[0] == '\t')) {
                value.erase(0, 1);
            }
            request.headers[key] = value;
        }
    }
    return request;
}
