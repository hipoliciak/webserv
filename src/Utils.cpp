#include "../include/Utils.hpp"

namespace Utils {
    // String utilities
    std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        
        size_t end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }

    std::string toLower(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }

    std::string toUpper(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::toupper);
        return result;
    }

    std::vector<std::string> split(const std::string& str, char delimiter) {
        std::vector<std::string> tokens;
        std::stringstream ss(str);
        std::string token;
        
        while (std::getline(ss, token, delimiter)) {
            tokens.push_back(token);
        }
        
        return tokens;
    }

    std::vector<std::string> split(const std::string& str, const std::string& delimiter) {
        std::vector<std::string> tokens;
        size_t start = 0;
        size_t end = str.find(delimiter);
        
        while (end != std::string::npos) {
            tokens.push_back(str.substr(start, end - start));
            start = end + delimiter.length();
            end = str.find(delimiter, start);
        }
        
        tokens.push_back(str.substr(start));
        return tokens;
    }

    bool startsWith(const std::string& str, const std::string& prefix) {
        return str.size() >= prefix.size() && str.substr(0, prefix.size()) == prefix;
    }

    bool endsWith(const std::string& str, const std::string& suffix) {
        return str.size() >= suffix.size() && str.substr(str.size() - suffix.size()) == suffix;
    }
    
    // File utilities
    bool fileExists(const std::string& path) {
        return access(path.c_str(), F_OK) == 0;
    }

    bool isDirectory(const std::string& path) {
        struct stat statbuf;
        if (stat(path.c_str(), &statbuf) != 0) {
            return false;
        }
        return S_ISDIR(statbuf.st_mode);
    }

    std::string readFile(const std::string& path) {
        std::ifstream file(path.c_str());
        if (!file.is_open()) {
            return "";
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    bool writeFile(const std::string& path, const std::string& content) {
        std::ofstream file(path.c_str());
        if (!file.is_open()) {
            return false;
        }
        
        file << content;
        return file.good();
    }

    std::string getDirectory(const std::string& path) {
        size_t slashPos = path.find_last_of('/');
        if (slashPos == std::string::npos) {
            return "";
        }
        return path.substr(0, slashPos);
    }

    std::string getBasename(const std::string& path) {
        size_t slashPos = path.find_last_of('/');
        if (slashPos == std::string::npos) {
            return path;
        }
        return path.substr(slashPos + 1);
    }

    std::string getFileExtension(const std::string& path) {
        size_t dotPos = path.find_last_of('.');
        if (dotPos == std::string::npos) {
            return "";
        }
        return path.substr(dotPos);
    }

    size_t getFileSize(const std::string& path) {
        struct stat statbuf;
        if (stat(path.c_str(), &statbuf) != 0) {
            return 0;
        }
        return statbuf.st_size;
    }
    
    // HTTP utilities
    std::string urlDecode(const std::string& str) {
        std::string result;
        for (size_t i = 0; i < str.length(); ++i) {
            if (str[i] == '%' && i + 2 < str.length()) {
                std::string hex = str.substr(i + 1, 2);
                char c = static_cast<char>(strtol(hex.c_str(), NULL, 16));
                result += c;
                i += 2;
            } else if (str[i] == '+') {
                result += ' ';
            } else {
                result += str[i];
            }
        }
        return result;
    }

    std::string urlEncode(const std::string& str) {
        std::string result;
        for (size_t i = 0; i < str.length(); ++i) {
            char c = str[i];
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                result += c;
            } else {
                std::stringstream ss;
                ss << '%' << std::hex << std::uppercase << static_cast<int>(static_cast<unsigned char>(c));
                result += ss.str();
            }
        }
        return result;
    }

    std::string getMimeType(const std::string& extension) {
        std::string ext = toLower(extension);
        
        if (ext == ".html" || ext == ".htm") return "text/html";
        if (ext == ".css") return "text/css";
        if (ext == ".js") return "application/javascript";
        if (ext == ".json") return "application/json";
        if (ext == ".xml") return "application/xml";
        if (ext == ".png") return "image/png";
        if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
        if (ext == ".gif") return "image/gif";
        if (ext == ".svg") return "image/svg+xml";
        if (ext == ".ico") return "image/x-icon";
        if (ext == ".txt") return "text/plain";
        if (ext == ".pdf") return "application/pdf";
        if (ext == ".php") return "application/x-httpd-php";
        if (ext == ".py") return "text/x-python";
        
        return "application/octet-stream";
    }

    std::string getCurrentTime() {
        time_t now = time(0);
        struct tm* timeinfo = gmtime(&now);
        char buffer[80];
        strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", timeinfo);
        return std::string(buffer);
    }

    std::string formatTime(time_t time) {
        struct tm* timeinfo = gmtime(&time);
        char buffer[80];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
        return std::string(buffer);
    }
    
    // Network utilities
    std::string getClientIP(int socket) {
        struct sockaddr_in addr;
        socklen_t addrLen = sizeof(addr);
        
        if (getpeername(socket, (struct sockaddr*)&addr, &addrLen) == 0) {
            return std::string(inet_ntoa(addr.sin_addr));
        }
        
        return "unknown";
    }

    bool isValidIP(const std::string& ip) {
        struct sockaddr_in sa;
        return inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr)) != 0;
    }

    bool isValidPort(int port) {
        return port > 0 && port <= 65535;
    }
    
    // Conversion utilities
    std::string intToString(int value) {
        std::stringstream ss;
        ss << value;
        return ss.str();
    }

    int stringToInt(const std::string& str) {
        return atoi(str.c_str());
    }

    std::string sizeToString(size_t size) {
        std::stringstream ss;
        ss << size;
        return ss.str();
    }
    
    // Logging utilities
    void log(const std::string& message) {
        std::cerr << "[" << getCurrentTime() << "] " << message << std::endl;
    }

    void logError(const std::string& message) {
        std::cerr << "[" << getCurrentTime() << "] ERROR: " << message << std::endl;
    }

    void logInfo(const std::string& message) {
        std::cerr << "[" << getCurrentTime() << "] INFO: " << message << std::endl;
    }

    void logDebug(const std::string& message) {
        std::cerr << "[" << getCurrentTime() << "] DEBUG: " << message << std::endl;
    }
}
