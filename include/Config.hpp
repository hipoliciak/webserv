#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "webserv.hpp"

struct ServerConfig {
    std::string host;
    int port;
    std::string serverName;
    std::string root;
    std::string index;
    std::map<int, std::string> errorPages;
    size_t maxBodySize;
    std::vector<LocationConfig> locations;
    std::vector<std::string> allowedMethods;
    bool autoIndex;
    std::string uploadPath;
    std::string cgiPath;
    std::map<std::string, std::string> cgiExtensions;
};

class Config {
private:
    std::vector<ServerConfig> _servers;
    std::string _configFile;

public:
    Config();
    Config(const std::string& configFile);
    ~Config();

    // Parsing
    bool parse();
    bool parseFile(const std::string& filename);
    bool parseServerBlock(const std::string& block);
    bool parseLocationBlock(const std::string& block, LocationConfig& location);
    void setDefaults(ServerConfig& server);
    void setLocationDefaults(LocationConfig& location) const;
    
    // Getters
    const std::vector<ServerConfig>& getServers() const;
    ServerConfig getDefaultServer() const;
    ServerConfig getServerByPort(int port) const;
    ServerConfig getServerByName(const std::string& serverName) const;
    LocationConfig getLocationConfig(const ServerConfig& server, const std::string& path) const;
    
    // Validation
    bool validate() const;
    bool isValidMethod(const std::string& method, const ServerConfig& server) const;
    bool isValidMethod(const std::string& method, const LocationConfig& location) const;
    
    // CGI support
    std::string getCGIInterpreter(const std::string& extension, const ServerConfig& server) const;
    bool isCGIEnabled(const ServerConfig& server) const;
    
    // Utilities
    static std::vector<std::string> split(const std::string& str, char delimiter);
    static std::string trim(const std::string& str);
    static std::string extractValue(const std::string& line);
    static std::vector<std::string> extractValues(const std::string& line);
};

#endif