#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>

struct FormFile {
    std::string filename;
    std::string content_type;
    std::string data;
};

struct HttpRequest {
    std::string method;
    std::string path;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string boundary;
    std::vector<FormFile> files;
};

class RequestParser 
{
public:
    explicit RequestParser(const std::string& raw_request) : request_(raw_request) {}
    HttpRequest parse();
    HttpRequest request;
    std::vector<FormFile> parse_files();
    void save_files(std::vector<FormFile> vector_files);
private:
    std::string request_;
};

HttpRequest RequestParser::parse() 
{
    //printf("request: %s\n", request_.c_str());

    std::istringstream stream(request_);
    std::string line;

    // Parse request line
    if (std::getline(stream, line)) {
        std::istringstream line_stream(line);
        line_stream >> request.method >> request.path >> request.version;
    }
    
    // Parse headers
    std::string body;
    while (std::getline(stream, line) && !line.empty()) 
    {
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);
            while (!value.empty() && (value[0] == ' ' || value[0] == '\t')) {
                value.erase(0, 1);
            }
            request.headers[key] = value;
            
            if (key == "Content-Type") {
                size_t boundary_pos = value.find("boundary=");
                if (boundary_pos != std::string::npos) {
                    request.boundary = "--" + value.substr(boundary_pos + 9);
                }
            }
        }
    }

    // Read body
    body.assign(std::istreambuf_iterator<char>(stream), {});
    return request;
}


std::vector<FormFile> RequestParser::parse_files()
{
    std::vector<FormFile> vector_files;
    int boundary_len = request.boundary.length();
    size_t pos = request_.find(request.boundary);
    while (pos != std::string::npos) 
    {
        FormFile file;
        size_t end_pos = request_.find(request.boundary, pos + boundary_len);
        std::string file_data = request_.substr(pos, end_pos - pos);
        
        size_t filename_pos = file_data.find("filename=\"");
        if (filename_pos != std::string::npos) 
        {
            filename_pos += 10;
            size_t filename_end = file_data.find("\"", filename_pos);
            file.filename = file_data.substr(filename_pos, filename_end - filename_pos);
        }
        
        size_t content_type_pos = file_data.find("Content-Type: ");
        if (content_type_pos != std::string::npos) 
        {
            content_type_pos += 14;
            size_t content_type_end = file_data.find("\r\n", content_type_pos);
            file.content_type = file_data.substr(content_type_pos, content_type_end - content_type_pos);
        }
        
        size_t data_pos = file_data.find("\r\n\r\n");
        if (data_pos != std::string::npos) 
        {
            data_pos += 4;
            file.data = file_data.substr(data_pos);
        }

        vector_files.push_back(file);

        pos = end_pos;
    }
    return vector_files;
    
}

void RequestParser::save_files(std::vector<FormFile> vector_files)
{
    const std::string default_directory = "uploads/";
    for (const auto& file : vector_files) 
    {
        std::string file_path = default_directory + file.filename;
        std::ofstream out(file_path, std::ios::binary);
        out << file.data;
        out.close();
    }

}
