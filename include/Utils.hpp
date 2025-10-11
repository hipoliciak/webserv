#ifndef UTILS_HPP
#define UTILS_HPP

#include "webserv.hpp"

namespace Utils {
    // String utilities
    std::string trim(const std::string& str);
    std::string toLower(const std::string& str);
    std::string toUpper(const std::string& str);
    std::vector<std::string> split(const std::string& str, char delimiter);
    std::vector<std::string> split(const std::string& str, const std::string& delimiter);
    bool startsWith(const std::string& str, const std::string& prefix);
    bool endsWith(const std::string& str, const std::string& suffix);
    
    // File utilities
    bool fileExists(const std::string& path);
    bool isDirectory(const std::string& path);
    std::string readFile(const std::string& path);
    bool writeFile(const std::string& path, const std::string& content);
    std::string getDirectory(const std::string& path);
    std::string getBasename(const std::string& path);
    std::string getFileExtension(const std::string& path);
    size_t getFileSize(const std::string& path);
    
    // HTTP utilities
    std::string urlDecode(const std::string& str);
    std::string urlEncode(const std::string& str);
    std::string getMimeType(const std::string& extension);
    std::string getCurrentTime();
    std::string formatTime(time_t time);
    
    // Network utilities
    std::string getClientIP(int socket);
    bool isValidIP(const std::string& ip);
    bool isValidPort(int port);
    
    // Conversion utilities
    std::string intToString(int value);
    int stringToInt(const std::string& str);
    std::string sizeToString(size_t size);
    
    // Logging utilities
    void log(const std::string& message);
    void logError(const std::string& message);
    void logInfo(const std::string& message);
    void logDebug(const std::string& message);
}

#endif