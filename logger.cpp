#include <string>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <iomanip>

class Logger {
private:
    std::ofstream log_file;

public:
    Logger();
    ~Logger();
    void log(const std::string& message);
};

void Logger::log(const std::string& message) 
{
    time_t now = time(nullptr);
    struct tm *localTime = localtime(&now);

    log_file << "[" << std::setw(2) << std::setfill('0') << localTime->tm_hour << ":"
             << std::setw(2) << std::setfill('0') << localTime->tm_min << ":"
             << std::setw(2) << std::setfill('0') << localTime->tm_sec << "] "
             << message << std::endl;
}


Logger::Logger() 
{
    std::filesystem::create_directories("logs");


    time_t now = time(nullptr);
    struct tm *localTime = localtime(&now);

    std::ostringstream filename;
    filename << "logs/"
             << std::setw(2) << std::setfill('0') << localTime->tm_mday << "_"
             << std::setw(2) << std::setfill('0') << (localTime->tm_mon + 1) << "_"
             << (localTime->tm_year + 1900) << "_"
             << std::setw(2) << std::setfill('0') << localTime->tm_hour << "_"
             << std::setw(2) << std::setfill('0') << localTime->tm_min
             << ".txt";

    log_file.open(filename.str(), std::ios::app);
    if (!log_file) {
        std::cerr << "Error during creating the file!" << std::endl;
    } else {
        log_file << "==== Logger started ====" << std::endl;
    }
}

Logger::~Logger() {}