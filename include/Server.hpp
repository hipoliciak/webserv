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
    struct ServerInfo {
        int socket;
        struct sockaddr_in addr;
        ServerConfig config;
    };
    
    std::vector<ServerInfo> _servers;
    std::vector<struct pollfd> _pollFds;
    std::map<int, Client> _clients;
    std::map<int, std::string> _pendingWrites;
    std::map<int, size_t> _writeOffsets;
    std::map<int, ServerConfig> _serverConfigs; // Map socket fd to server config
    std::map<int, int> _clientServerSockets; // Map client fd to server socket fd
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
    bool createSockets();
    bool bindSockets();
    bool listenSockets();
    bool acceptNewConnection(int serverSocket);
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
    HttpResponse generateDirectoryListing(const std::string& path, const std::string& urlPath, const ServerConfig& serverConfig);
    
    // CGI handling
    HttpResponse executeCGI(const std::string& scriptPath, const HttpRequest& request, const ServerConfig& serverConfig, const LocationConfig& locationConfig);
    
    // Upload handling
    HttpResponse handleFileUpload(const HttpRequest& request, const ServerConfig& serverConfig);
    HttpResponse handleSimpleFileUpload(const HttpRequest& request, const ServerConfig& serverConfig, const LocationConfig& location);
    HttpResponse handleJSONPost(const HttpRequest& request, const ServerConfig& serverConfig);
    bool saveUploadedFile(const std::string& filename, const std::string& content, const std::string& uploadPath);
    
    // Route handling
    std::string resolveFilePath(const std::string& uri, const ServerConfig& serverConfig);
    LocationConfig getMatchingLocation(const std::string& uri, const ServerConfig& serverConfig);
    bool isMethodAllowed(const std::string& method, const ServerConfig& serverConfig, const LocationConfig& location);
    
    // Redirection
    HttpResponse handleRedirection(const LocationConfig& location);
    
    // Error handling
    HttpResponse createErrorResponse(int statusCode, const ServerConfig& serverConfig);
    
    // Getters
    int getPort() const;
    const std::string& getHost() const;
    bool isRunning() const;
    
private:
    void updatePollEvents(int clientFd);
    bool writeToClient(int clientFd);
    ServerConfig getServerConfig(int clientFd) const;
};

#endif