#include <string>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <iomanip>

class LoggerStrategy {
public:
    virtual ~LoggerStrategy() = default;
    virtual void log(const std::string &message) = 0;
};

class ConsoleLogger : public LoggerStrategy {
public:
    void log(const std::string &message) override {
        std::cout << message << std::endl;
    }
};

class FileLogger : public LoggerStrategy {
private:
    std::ofstream log_file;
public:
    FileLogger() {
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
            log_file << "==== Logger started ====\n";
        }
    }

    void log(const std::string &message) override {
        time_t now = time(nullptr);
        struct tm *localTime = localtime(&now);
        log_file << "[" << std::setw(2) << std::setfill('0') << localTime->tm_hour << ":"
                 << std::setw(2) << std::setfill('0') << localTime->tm_min << ":"
                 << std::setw(2) << std::setfill('0') << localTime->tm_sec << "] "
                 << message << std::endl;
    }
};

class Logger {
private:
    LoggerStrategy *strategy;
public:
    Logger(LoggerStrategy *loggerStrategy) : strategy(loggerStrategy) {}
    void setStrategy(LoggerStrategy *loggerStrategy) { strategy = loggerStrategy; }
    void log(const std::string &message) { strategy->log(message); }
};
