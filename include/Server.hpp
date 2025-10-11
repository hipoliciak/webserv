#ifndef SERVER_HPP
#define SERVER_HPP

#include "webserv.hpp"
#include "Config.hpp"
#include "Client.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "CGI.hpp"

class Server {
private:
    int _serverSocket;
    struct sockaddr_in _serverAddr;
    std::vector<struct pollfd> _pollFds;
    std::map<int, Client> _clients;
    std::map<int, std::string> _pendingWrites; // fd -> response data to write
    std::map<int, size_t> _writeOffsets; // fd -> offset in pending write data
    Config _config;
    bool _running;

public:
    Server();
    Server(const Config& config);
    ~Server();

    // Core functionality
    bool initialize();
    void run();
    void stop();
    
    // Socket operations
    bool createSocket();
    bool bindSocket();
    bool listenSocket();
    bool acceptNewConnection();
    void handleClientRead(int clientFd);
    void handleClientWrite(int clientFd);
    void removeClient(int clientFd);
    
    // HTTP handling
    void processHttpRequest(int clientFd, const std::string& request);
    void queueResponse(int clientFd, const HttpResponse& response);
    HttpResponse handleGETRequest(const HttpRequest& request, const ServerConfig& serverConfig);
    HttpResponse handlePOSTRequest(const HttpRequest& request, const ServerConfig& serverConfig);
    HttpResponse handlePUTRequest(const HttpRequest& request, const ServerConfig& serverConfig);
    HttpResponse handleDELETERequest(const HttpRequest& request, const ServerConfig& serverConfig);
    
    // File operations
    HttpResponse serveStaticFile(const std::string& path, const ServerConfig& serverConfig);
    HttpResponse handleDirectoryRequest(const std::string& path, const std::string& uri, const ServerConfig& serverConfig);
    HttpResponse generateDirectoryListing(const std::string& path, const std::string& urlPath);
    
    // CGI handling
    HttpResponse executeCGI(const std::string& scriptPath, const HttpRequest& request, const ServerConfig& serverConfig);
    
    // Upload handling
    HttpResponse handleFileUpload(const HttpRequest& request, const ServerConfig& serverConfig);
    bool saveUploadedFile(const std::string& filename, const std::string& content, const std::string& uploadPath);
    
    // Route handling
    std::string resolveFilePath(const std::string& uri, const ServerConfig& serverConfig);
    LocationConfig getMatchingLocation(const std::string& uri, const ServerConfig& serverConfig);
    bool isMethodAllowed(const std::string& method, const ServerConfig& serverConfig, const LocationConfig& location);
    
    // Redirection
    HttpResponse handleRedirection(const LocationConfig& location);
    
    // Getters
    int getPort() const;
    const std::string& getHost() const;
    bool isRunning() const;
    
private:
    void updatePollEvents(int clientFd);
    bool writeToClient(int clientFd);
    ServerConfig getServerConfig(const HttpRequest& request) const;
};

#endif