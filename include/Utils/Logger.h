#ifndef MCATOOL_LOGGER_H
#define MCATOOL_LOGGER_H

#include <iostream>
#include <sstream>
#include <string>
#include <chrono>
#include <iomanip>

namespace MCATool {

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class Logger {
public:
    static void setLogLevel(LogLevel level) {
        currentLevel = level;
    }
    
    static void debug(const std::string& message) {
        log(LogLevel::DEBUG, message);
    }
    
    static void info(const std::string& message) {
        log(LogLevel::INFO, message);
    }
    
    static void warning(const std::string& message) {
        log(LogLevel::WARNING, message);
    }
    
    static void error(const std::string& message) {
        log(LogLevel::ERROR, message);
    }
    
private:
    static LogLevel currentLevel;
    
    static void log(LogLevel level, const std::string& message) {
        if (level < currentLevel) {
            return;
        }
        
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        
        std::ostringstream oss;
        oss << "[" << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S") << "] ";
        
        switch (level) {
            case LogLevel::DEBUG:
                oss << "[DEBUG] ";
                break;
            case LogLevel::INFO:
                oss << "[INFO] ";
                break;
            case LogLevel::WARNING:
                oss << "[WARNING] ";
                break;
            case LogLevel::ERROR:
                oss << "[ERROR] ";
                break;
        }
        
        oss << message;
        
        if (level == LogLevel::ERROR) {
            std::cerr << oss.str() << std::endl;
        } else {
            std::cout << oss.str() << std::endl;
        }
    }
};

// 静态成员初始化
inline LogLevel Logger::currentLevel = LogLevel::INFO;

} // namespace MCATool

#endif // MCATOOL_LOGGER_H
