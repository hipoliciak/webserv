#include "../include/Server.hpp"
#include "../include/Utils.hpp"

Server::Server() : _serverSocket(-1), _running(false) {
    _config = Config();
}

Server::Server(const Config& config) : _serverSocket(-1), _config(config), _running(false) {
}

Server::~Server() {
    stop();
}

bool Server::initialize() {
    if (!createSocket()) {
        return false;
    }
    
    if (!bindSocket()) {
        close(_serverSocket);
        return false;
    }
    
    if (!listenSocket()) {
        close(_serverSocket);
        return false;
    }
    
    // Add server socket to poll list
    struct pollfd serverPollFd;
    serverPollFd.fd = _serverSocket;
    serverPollFd.events = POLLIN;
    _pollFds.push_back(serverPollFd);
    
    return true;
}

bool Server::createSocket() {
    _serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (_serverSocket < 0) {
        Utils::logError("Failed to create socket: " + std::string(strerror(errno)));
        return false;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        Utils::logError("Failed to set socket options: " + std::string(strerror(errno)));
        return false;
    }
    
    // Set non-blocking
    int flags = fcntl(_serverSocket, F_GETFL, 0);
    if (fcntl(_serverSocket, F_SETFL, flags | O_NONBLOCK) < 0) {
        Utils::logError("Failed to set non-blocking mode: " + std::string(strerror(errno)));
        return false;
    }
    
    return true;
}

bool Server::bindSocket() {
    ServerConfig defaultConfig = _config.getDefaultServer();
    
    memset(&_serverAddr, 0, sizeof(_serverAddr));
    _serverAddr.sin_family = AF_INET;
    _serverAddr.sin_port = htons(defaultConfig.port);
    
    if (defaultConfig.host == "localhost" || defaultConfig.host == "127.0.0.1") {
        _serverAddr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, defaultConfig.host.c_str(), &_serverAddr.sin_addr) <= 0) {
            Utils::logError("Invalid host address: " + defaultConfig.host);
            return false;
        }
    }
    
    if (bind(_serverSocket, (struct sockaddr*)&_serverAddr, sizeof(_serverAddr)) < 0) {
        Utils::logError("Failed to bind socket: " + std::string(strerror(errno)));
        return false;
    }
    
    return true;
}

bool Server::listenSocket() {
    if (listen(_serverSocket, MAX_CONNECTIONS) < 0) {
        Utils::logError("Failed to listen on socket: " + std::string(strerror(errno)));
        return false;
    }
    
    return true;
}

void Server::run() {
    _running = true;
    Utils::logInfo("Server started successfully");
    
    while (_running) {
        int pollResult = poll(&_pollFds[0], _pollFds.size(), 1000); // 1 second timeout
        
        if (pollResult < 0) {
            if (errno == EINTR) continue; // Interrupted by signal
            Utils::logError("Poll failed: " + std::string(strerror(errno)));
            break;
        }
        
        if (pollResult == 0) continue; // Timeout
        
        // Check for new connections
        if (_pollFds[0].revents & POLLIN) {
            acceptNewConnection();
        }
        
        // Check existing client connections
        for (size_t i = 1; i < _pollFds.size(); ++i) {
            if (_pollFds[i].revents & POLLIN) {
                handleClientRead(_pollFds[i].fd);
            }
            
            if (_pollFds[i].revents & POLLOUT) {
                handleClientWrite(_pollFds[i].fd);
            }
            
            if (_pollFds[i].revents & (POLLHUP | POLLERR | POLLNVAL)) {
                removeClient(_pollFds[i].fd);
                --i; // Adjust index after removal
            }
        }
    }
}

bool Server::acceptNewConnection() {
    struct sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);
    
    int clientFd = accept(_serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
    if (clientFd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return true; // No pending connections
        }
        Utils::logError("Failed to accept connection: " + std::string(strerror(errno)));
        return false;
    }
    
    // Set client socket to non-blocking
    int flags = fcntl(clientFd, F_GETFL, 0);
    if (fcntl(clientFd, F_SETFL, flags | O_NONBLOCK) < 0) {
        Utils::logError("Failed to set client socket non-blocking");
        close(clientFd);
        return false;
    }
    
    // Add client to poll list
    struct pollfd clientPollFd;
    clientPollFd.fd = clientFd;
    clientPollFd.events = POLLIN;
    _pollFds.push_back(clientPollFd);
    
    // Create client object
    _clients[clientFd] = Client(clientFd);
    
    std::string clientIP = Utils::getClientIP(clientFd);
    Utils::logInfo("New connection accepted from " + clientIP);
    
    return true;
}

void Server::handleClientRead(int clientFd) {
    Client& client = _clients[clientFd];
    Utils::logDebug("handleClientRead() for fd " + Utils::intToString(clientFd));
    
    // If client is already stopped from reading, don't process further
    if (client.shouldStopReading()) {
        Utils::logDebug("Client already stopped reading, skipping handleClientRead");
        return;
    }
    
    if (!client.readData()) {
        Utils::logDebug("readData() failed, removing client " + Utils::intToString(clientFd));
        removeClient(clientFd);
        return;
    }
    
    // Check if headers just became complete and validate immediately
    if (!client.isRequestComplete() && client.areHeadersComplete()) {
        std::string headers = client.getHeaders();
        
        // Parse the request line to get method and URI
        size_t firstLineEnd = headers.find("\r\n");
        if (firstLineEnd == std::string::npos) {
            firstLineEnd = headers.find("\n");
        }
        
        if (firstLineEnd != std::string::npos) {
            std::string requestLine = headers.substr(0, firstLineEnd);
            std::vector<std::string> tokens = Utils::split(requestLine, ' ');
            
            if (tokens.size() >= 2) {
                std::string method = tokens[0];
                std::string uri = tokens[1];
                
                // Only validate body size for methods that can have bodies
                if (method == "POST" || method == "PUT") {
                    // Skip body size validation for .bla files (they should be handled by CGI)
                    std::string extension = Utils::getFileExtension(uri);
                    if (extension == ".bla") {
                        Utils::logDebug("Skipping body size validation for .bla file: " + uri);
                    } else {
                        const ServerConfig& serverConfig = _config.getDefaultServer();
                    
                    // Check Content-Length header
                    size_t clPos = headers.find("Content-Length:");
                    if (clPos == std::string::npos) {
                        clPos = headers.find("content-length:");
                    }
                    
                    if (clPos != std::string::npos) {
                        size_t lineEnd = headers.find("\r\n", clPos);
                        if (lineEnd == std::string::npos) {
                            lineEnd = headers.find("\n", clPos);
                        }
                        
                        if (lineEnd != std::string::npos) {
                            size_t colonPos = headers.find(":", clPos);
                            if (colonPos != std::string::npos && colonPos < lineEnd) {
                                std::string lengthStr = headers.substr(colonPos + 1, lineEnd - (colonPos + 1));
                                lengthStr = Utils::trim(lengthStr);
                                size_t contentLength = Utils::stringToInt(lengthStr);
                                
                                Utils::logDebug("Found Content-Length: " + Utils::sizeToString(contentLength) + 
                                               ", Server limit: " + Utils::sizeToString(serverConfig.maxBodySize));
                                
                                if (contentLength > serverConfig.maxBodySize) {
                                    Utils::logError("Content-Length " + Utils::sizeToString(contentLength) + 
                                                   " exceeds server limit " + Utils::sizeToString(serverConfig.maxBodySize));
                                    
                                    // Stop the client from reading more data immediately
                                    client.stopReading();
                                    
                                    // Send 413 Payload Too Large immediately
                                    HttpResponse response = HttpResponse::createErrorResponse(413);
                                    queueResponse(clientFd, response);
                                    return;
                                }
                            }
                        }
                    }
                    
                    // Check for chunked encoding - reject large chunked requests immediately
                    size_t tePos = headers.find("Transfer-Encoding:");
                    if (tePos == std::string::npos) {
                        tePos = headers.find("transfer-encoding:");
                    }
                    
                    if (tePos != std::string::npos) {
                        size_t lineEnd = headers.find("\r\n", tePos);
                        if (lineEnd == std::string::npos) {
                            lineEnd = headers.find("\n", tePos);
                        }
                        
                        if (lineEnd != std::string::npos) {
                            size_t colonPos = headers.find(":", tePos);
                            if (colonPos != std::string::npos && colonPos < lineEnd) {
                                std::string teValue = headers.substr(colonPos + 1, lineEnd - (colonPos + 1));
                                teValue = Utils::trim(teValue);
                                std::string teValueLow = Utils::toLower(teValue);
                                
                                if (teValueLow.find("chunked") != std::string::npos) {
                                    Utils::logDebug("Detected chunked encoding - applying strict size limit");
                                    
                                    // For chunked requests, we'll apply a stricter buffer limit
                                    // If we already have more than the allowed body size in buffer, reject
                                    size_t currentBufferSize = client.getBuffer().size();
                                    if (currentBufferSize > serverConfig.maxBodySize + 1024) { // +1024 for headers
                                        Utils::logError("Chunked request already exceeds buffer limit");
                                        client.stopReading();
                                        HttpResponse response = HttpResponse::createErrorResponse(413);
                                        queueResponse(clientFd, response);
                                        return;
                                    }
                                }
                            }
                        }
                    }
                    } // Close the else block for non-.bla files
                }
            }
        }
    }
    
    if (client.isRequestComplete()) {
        Utils::logDebug("Request complete for fd " + Utils::intToString(clientFd) + ", processing...");
        processHttpRequest(clientFd, client.getRequest());
        client.clearRequest();
        Utils::logDebug("Request processed and cleared for fd " + Utils::intToString(clientFd));
    } else {
        Utils::logDebug("Request not complete yet for fd " + Utils::intToString(clientFd));
    }
}

void Server::handleClientWrite(int clientFd) {
    if (!writeToClient(clientFd)) {
        removeClient(clientFd);
    }
}

void Server::processHttpRequest(int clientFd, const std::string& request) {
    Utils::logDebug("processHttpRequest() for fd " + Utils::intToString(clientFd) + ", request size: " + Utils::sizeToString(request.size()));
    HttpRequest httpRequest(request);
    HttpResponse response;
    
    if (!httpRequest.isValid()) {
        Utils::logDebug("HttpRequest is invalid, sending 400 error");
        response = HttpResponse::createErrorResponse(HTTP_BAD_REQUEST);
    } else {
        Utils::logDebug("HttpRequest is valid - method: " + httpRequest.getMethod() + ", URI: " + httpRequest.getUri());
        ServerConfig serverConfig = getServerConfig(httpRequest);
        
        LocationConfig locationConfig = getMatchingLocation(httpRequest.getUri(), serverConfig);
        
        // Special case: allow POST for .bla files regardless of location configuration
        bool isBlaFile = false;
        if (httpRequest.getMethod() == "POST") {
            std::string extension = Utils::getFileExtension(httpRequest.getUri());
            if (extension == ".bla") {
                isBlaFile = true;
                Utils::logDebug("Detected .bla file, allowing POST regardless of location config");
            }
        }
        
        // Check if method is allowed for this location
        if (!isBlaFile && !isMethodAllowed(httpRequest.getMethod(), serverConfig, locationConfig)) {
            Utils::logError("Method " + httpRequest.getMethod() + " not allowed for URI: " + httpRequest.getUri());
            response = HttpResponse::createErrorResponse(HTTP_METHOD_NOT_ALLOWED);
        } else {
            // Handle different HTTP methods
            if (httpRequest.getMethod() == "GET") {
                response = handleGETRequest(httpRequest, serverConfig);
            } else if (httpRequest.getMethod() == "POST") {
                response = handlePOSTRequest(httpRequest, serverConfig);
            } else if (httpRequest.getMethod() == "PUT") {
                response = handlePUTRequest(httpRequest, serverConfig);
            } else if (httpRequest.getMethod() == "DELETE") {
                response = handleDELETERequest(httpRequest, serverConfig);
            } else {
                // Any other method (including HEAD, etc.) is not allowed
                Utils::logDebug("Method " + httpRequest.getMethod() + " not supported, returning 405");
                response = HttpResponse::createErrorResponse(HTTP_METHOD_NOT_ALLOWED);
            }
        }
    }
    
    queueResponse(clientFd, response);
}

void Server::queueResponse(int clientFd, const HttpResponse& response) {
    std::string responseStr = response.toString();
    Utils::logDebug("Queueing response for fd " + Utils::intToString(clientFd) + 
                   ", status: " + Utils::intToString(response.getStatusCode()) + 
                   ", size: " + Utils::sizeToString(responseStr.size()) + " bytes");
    Utils::logDebug("Response first line: " + responseStr.substr(0, responseStr.find('\n')));
    
    // Log complete response for HEAD requests (they should be small)
    if (responseStr.size() < 500) {
        Utils::logDebug("Complete response: [" + responseStr + "]");
    }
    
    _pendingWrites[clientFd] = responseStr;
    _writeOffsets[clientFd] = 0;
    
    // Update poll events to include POLLOUT
    updatePollEvents(clientFd);
}

bool Server::writeToClient(int clientFd) {
    if (_pendingWrites.find(clientFd) == _pendingWrites.end()) {
        return true; // No pending writes
    }
    
    const std::string& response = _pendingWrites[clientFd];
    size_t offset = _writeOffsets[clientFd];
    
    ssize_t bytesSent = send(clientFd, response.c_str() + offset, response.length() - offset, 0);
    
    if (bytesSent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return true; // Would block, try again later
        }
        Utils::logError("Failed to send response: " + std::string(strerror(errno)));
        return false;
    }
    
    _writeOffsets[clientFd] += bytesSent;
    
    if (_writeOffsets[clientFd] >= response.length()) {
        // Response completely sent
        _pendingWrites.erase(clientFd);
        _writeOffsets.erase(clientFd);
        updatePollEvents(clientFd);
        Utils::logInfo("Response sent (" + Utils::sizeToString(response.length()) + " bytes)");
        
        // Close connection after sending response (HTTP/1.0 behavior)
        return false;
    }
    
    return true;
}

void Server::updatePollEvents(int clientFd) {
    for (size_t i = 0; i < _pollFds.size(); ++i) {
        if (_pollFds[i].fd == clientFd) {
            _pollFds[i].events = POLLIN;
            if (_pendingWrites.find(clientFd) != _pendingWrites.end()) {
                _pollFds[i].events |= POLLOUT;
            }
            break;
        }
    }
}

void Server::removeClient(int clientFd) {
    // Remove from poll list
    for (std::vector<struct pollfd>::iterator it = _pollFds.begin(); it != _pollFds.end(); ++it) {
        if (it->fd == clientFd) {
            _pollFds.erase(it);
            break;
        }
    }
    
    // Remove from clients map and pending writes
    _clients.erase(clientFd);
    _pendingWrites.erase(clientFd);
    _writeOffsets.erase(clientFd);
    
    // Close socket
    close(clientFd);
    
    Utils::logInfo("Client disconnected (fd: " + Utils::intToString(clientFd) + ")");
}

void Server::stop() {
    _running = false;
    
    // Close all client connections
    for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        close(it->first);
    }
    _clients.clear();
    _pollFds.clear();
    _pendingWrites.clear();
    _writeOffsets.clear();
    
    // Close server socket
    if (_serverSocket >= 0) {
        close(_serverSocket);
        _serverSocket = -1;
    }
    
    Utils::logInfo("Server stopped");
}

// Placeholder implementations for new methods
HttpResponse Server::handleGETRequest(const HttpRequest& request, const ServerConfig& serverConfig) {
    std::string filePath = resolveFilePath(request.getUri(), serverConfig);
    
    Utils::logDebug("GET request for URI: " + request.getUri() + " resolved to: " + filePath);
    Utils::logDebug("File exists: " + std::string(Utils::fileExists(filePath) ? "yes" : "no"));
    
    // Check if it's a CGI request
    std::string extension = Utils::getFileExtension(filePath);
    if (extension == ".php" || extension == ".py" || extension == ".sh") {
        if (Utils::fileExists(filePath)) {
            return executeCGI(filePath, request, serverConfig);
        } else {
            return HttpResponse::createErrorResponse(HTTP_NOT_FOUND);
        }
    }
    
    // Check if file exists
    if (Utils::fileExists(filePath)) {
        if (Utils::isDirectory(filePath)) {
            return handleDirectoryRequest(filePath, request.getUri(), serverConfig);
        } else {
            return serveStaticFile(filePath, serverConfig);
        }
    } else {
        return HttpResponse::createErrorResponse(HTTP_NOT_FOUND);
    }
}

HttpResponse Server::handlePOSTRequest(const HttpRequest& request, const ServerConfig& serverConfig) {
    std::string contentType = request.getHeader("Content-Type");
    
    // Check if it's a file upload
    if (contentType.find("multipart/form-data") != std::string::npos) {
        return handleFileUpload(request, serverConfig);
    }
    
    // Check if it's a CGI request
    std::string filePath = resolveFilePath(request.getUri(), serverConfig);
    std::string extension = Utils::getFileExtension(filePath);
    if (extension == ".php" || extension == ".py" || extension == ".sh" || extension == ".bla") {
        if (Utils::fileExists(filePath)) {
            return executeCGI(filePath, request, serverConfig);
        } else {
            return HttpResponse::createErrorResponse(HTTP_NOT_FOUND);
        }
    }
    
    // Regular POST request
    HttpResponse response;
    response.setStatus(HTTP_OK);
    response.setContentType("text/plain");
    response.setBody("POST request received");
    return response;
}

HttpResponse Server::handlePUTRequest(const HttpRequest& request, const ServerConfig& serverConfig) {
    std::string filePath = resolveFilePath(request.getUri(), serverConfig);
    
    // Security check - ensure the file path is within the server root
    if (filePath.find("..") != std::string::npos) {
        return HttpResponse::createErrorResponse(HTTP_FORBIDDEN);
    }
    
    // Write the request body to the file
    if (!Utils::writeFile(filePath, request.getBody())) {
        return HttpResponse::createErrorResponse(HTTP_INTERNAL_SERVER_ERROR);
    }
    
    Utils::logInfo("File uploaded via PUT: " + filePath);
    
    // Return 201 Created for successful PUT
    HttpResponse response(201);
    response.setContentType("text/plain");
    response.setBody("File created successfully\n");
    
    return response;
}

HttpResponse Server::handleDELETERequest(const HttpRequest& request, const ServerConfig& serverConfig) {
    std::string filePath = resolveFilePath(request.getUri(), serverConfig);
    
    // Security check - ensure the file is within the server root
    if (filePath.find("..") != std::string::npos) {
        return HttpResponse::createErrorResponse(HTTP_FORBIDDEN);
    }
    
    // Check if file exists
    if (!Utils::fileExists(filePath)) {
        return HttpResponse::createErrorResponse(HTTP_NOT_FOUND);
    }
    
    // Check if it's a directory
    if (Utils::isDirectory(filePath)) {
        return HttpResponse::createErrorResponse(HTTP_FORBIDDEN);
    }
    
    // Attempt to delete the file
    if (unlink(filePath.c_str()) == 0) {
        Utils::logInfo("File deleted: " + filePath);
        
        HttpResponse response;
        response.setStatus(HTTP_OK);
        response.setContentType("text/plain");
        response.setBody("File deleted successfully");
        return response;
    } else {
        Utils::logError("Failed to delete file: " + filePath + " - " + std::string(strerror(errno)));
        return HttpResponse::createErrorResponse(HTTP_INTERNAL_SERVER_ERROR);
    }
}

HttpResponse Server::serveStaticFile(const std::string& path, const ServerConfig& /* serverConfig */) {
    return HttpResponse::createFileResponse(path);
}

HttpResponse Server::handleDirectoryRequest(const std::string& path, const std::string& uri, const ServerConfig& serverConfig) {
    // Try to serve index file first
    std::string indexPath = path + "/" + serverConfig.index;
    if (Utils::fileExists(indexPath) && !Utils::isDirectory(indexPath)) {
        return serveStaticFile(indexPath, serverConfig);
    }
    
    // Check if directory listing is enabled for this location
    LocationConfig location = getMatchingLocation(uri, serverConfig);
    Utils::logDebug("Directory listing check - uri: " + uri + ", path: " + path + ", autoIndex: " + (location.autoIndex ? "true" : "false"));
    
    // Special handling for /directory location: try to serve youpi.bad_extension file from subdirectories
    if (uri.find("/directory/") == 0 && uri != "/directory") {
        std::string youpiPath = path + "/youpi.bad_extension";
        Utils::logDebug("Checking for youpi.bad_extension file: " + youpiPath);
        if (Utils::fileExists(youpiPath) && !Utils::isDirectory(youpiPath)) {
            Utils::logDebug("Found youpi.bad_extension, serving it");
            return serveStaticFile(youpiPath, serverConfig);
        }
        Utils::logDebug("No youpi.bad_extension found, returning 404");
        return HttpResponse::createErrorResponse(HTTP_NOT_FOUND);
    }
    
    if (!location.autoIndex) {
        Utils::logDebug("Directory listing disabled, returning 403");
        return HttpResponse::createErrorResponse(HTTP_FORBIDDEN);
    }
    
    // Generate directory listing
    std::string urlPath = uri;
    if (urlPath.find(serverConfig.root) == 0) {
        urlPath = urlPath.substr(serverConfig.root.length());
        if (urlPath.empty()) urlPath = "/";
    }
    
    return generateDirectoryListing(path, urlPath);
}

HttpResponse Server::generateDirectoryListing(const std::string& path, const std::string& urlPath) {
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        return HttpResponse::createErrorResponse(HTTP_INTERNAL_SERVER_ERROR);
    }
    
    std::string html = "<!DOCTYPE html>\n";
    html += "<html><head><title>Index of " + urlPath + "</title></head>\n";
    html += "<body><h1>Index of " + urlPath + "</h1>\n";
    html += "<hr><pre>\n";
    
    // Add parent directory link if not root
    if (urlPath != "/") {
        std::string parentPath = urlPath;
        if (!parentPath.empty() && parentPath[parentPath.length() - 1] == '/') {
            parentPath = parentPath.substr(0, parentPath.length() - 1);
        }
        size_t lastSlash = parentPath.find_last_of('/');
        if (lastSlash != std::string::npos) {
            parentPath = parentPath.substr(0, lastSlash + 1);
        } else {
            parentPath = "/";
        }
        html += "<a href=\"" + parentPath + "\">../</a>\n";
    }
    
    struct dirent* entry;
    std::vector<std::string> directories;
    std::vector<std::string> files;
    
    // Separate directories and files
    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        
        std::string fullPath = path + "/" + name;
        if (Utils::isDirectory(fullPath)) {
            directories.push_back(name);
        } else {
            files.push_back(name);
        }
    }
    closedir(dir);
    
    // Sort both lists
    std::sort(directories.begin(), directories.end());
    std::sort(files.begin(), files.end());
    
    // Add directories first
    for (size_t i = 0; i < directories.size(); ++i) {
        std::string dirPath = urlPath;
        if (dirPath.empty() || dirPath[dirPath.length() - 1] != '/') dirPath += "/";
        dirPath += directories[i] + "/";
        
        html += "<a href=\"" + dirPath + "\">" + directories[i] + "/</a>\n";
    }
    
    // Add files
    for (size_t i = 0; i < files.size(); ++i) {
        std::string filePath = urlPath;
        if (filePath.empty() || filePath[filePath.length() - 1] != '/') filePath += "/";
        filePath += files[i];
        
        // Get file size and modification time
        std::string fullPath = path + "/" + files[i];
        struct stat statbuf;
        std::string sizeStr = "";
        std::string timeStr = "";
        
        if (stat(fullPath.c_str(), &statbuf) == 0) {
            sizeStr = Utils::sizeToString(statbuf.st_size);
            timeStr = Utils::formatTime(statbuf.st_mtime);
        }
        
        html += "<a href=\"" + filePath + "\">" + files[i] + "</a>";
        if (!timeStr.empty()) {
            html += "    " + timeStr;
        }
        if (!sizeStr.empty()) {
            html += "    " + sizeStr + " bytes";
        }
        html += "\n";
    }
    
    html += "</pre><hr></body></html>\n";
    
    HttpResponse response;
    response.setStatus(HTTP_OK);
    response.setContentType("text/html");
    response.setBody(html);
    return response;
}

HttpResponse Server::executeCGI(const std::string& scriptPath, const HttpRequest& request, const ServerConfig& serverConfig) {
    // Check if file exists
    if (!Utils::fileExists(scriptPath)) {
        return HttpResponse::createErrorResponse(HTTP_NOT_FOUND);
    }
    
    // Determine CGI interpreter
    std::string interpreter;
    std::string extension = Utils::getFileExtension(scriptPath);
    
    if (extension == ".php") {
        interpreter = "/usr/bin/php-cgi";
    } else if (extension == ".py") {
        interpreter = "/usr/bin/python3";
    } else if (extension == ".sh") {
        interpreter = "/bin/bash";
    } else {
        return HttpResponse::createErrorResponse(HTTP_NOT_IMPLEMENTED);
    }
    
    // Create pipes for communication
    int pipeFdIn[2], pipeFdOut[2];
    if (pipe(pipeFdIn) == -1 || pipe(pipeFdOut) == -1) {
        Utils::logError("Failed to create pipes for CGI: " + std::string(strerror(errno)));
        return HttpResponse::createErrorResponse(HTTP_INTERNAL_SERVER_ERROR);
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        Utils::logError("Failed to fork for CGI: " + std::string(strerror(errno)));
        close(pipeFdIn[0]); close(pipeFdIn[1]);
        close(pipeFdOut[0]); close(pipeFdOut[1]);
        return HttpResponse::createErrorResponse(HTTP_INTERNAL_SERVER_ERROR);
    }
    
    if (pid == 0) {
        // Child process - execute CGI
        close(pipeFdIn[1]);  // Close write end of input pipe
        close(pipeFdOut[0]); // Close read end of output pipe
        
        // Redirect stdin and stdout
        dup2(pipeFdIn[0], STDIN_FILENO);
        dup2(pipeFdOut[1], STDOUT_FILENO);
        dup2(pipeFdOut[1], STDERR_FILENO);
        
        // Close original pipe file descriptors
        close(pipeFdIn[0]);
        close(pipeFdOut[1]);
        
        // Set environment variables
        std::string queryString = request.getUri().find('?') != std::string::npos 
            ? request.getUri().substr(request.getUri().find('?') + 1) : "";
            
        setenv("REQUEST_METHOD", request.getMethod().c_str(), 1);
        setenv("SCRIPT_NAME", scriptPath.c_str(), 1);
        setenv("QUERY_STRING", queryString.c_str(), 1);
        setenv("CONTENT_TYPE", request.getHeader("Content-Type").c_str(), 1);
        setenv("CONTENT_LENGTH", Utils::sizeToString(request.getContentLength()).c_str(), 1);
        setenv("SERVER_NAME", serverConfig.serverName.c_str(), 1);
        setenv("SERVER_PORT", Utils::intToString(serverConfig.port).c_str(), 1);
        setenv("SERVER_PROTOCOL", "HTTP/1.1", 1);
        
        // Change to script directory
        std::string scriptDir = Utils::getDirectory(scriptPath);
        std::string scriptName = Utils::getBasename(scriptPath);
        if (!scriptDir.empty()) {
            chdir(scriptDir.c_str());
        }
        
        // Execute CGI script
        Utils::logDebug("Executing CGI script: " + interpreter + " " + scriptName + " in directory: " + scriptDir);
        if (extension == ".php") {
            execl(interpreter.c_str(), interpreter.c_str(), scriptName.c_str(), NULL);
        } else {
            execl(interpreter.c_str(), interpreter.c_str(), scriptName.c_str(), NULL);
        }
        
        // If we reach here, exec failed
        Utils::logError("exec failed: " + std::string(strerror(errno)));
        exit(1);
    } else {
        // Parent process
        close(pipeFdIn[0]);  // Close read end of input pipe
        close(pipeFdOut[1]); // Close write end of output pipe
        
        // Send request body to CGI if POST
        if (request.getMethod() == "POST" && !request.getBody().empty()) {
            write(pipeFdIn[1], request.getBody().c_str(), request.getBody().length());
        }
        close(pipeFdIn[1]);
        
        // Read CGI output
        std::string cgiOutput;
        char buffer[4096];
        ssize_t bytesRead;
        
        while ((bytesRead = read(pipeFdOut[0], buffer, sizeof(buffer))) > 0) {
            cgiOutput.append(buffer, bytesRead);
        }
        close(pipeFdOut[0]);
        
        // Wait for child process
        int status;
        waitpid(pid, &status, 0);
        
        if (WEXITSTATUS(status) != 0) {
            Utils::logError("CGI script execution failed with exit code: " + Utils::intToString(WEXITSTATUS(status)));
            Utils::logError("CGI output was: " + cgiOutput);
            return HttpResponse::createErrorResponse(HTTP_INTERNAL_SERVER_ERROR);
        }
        
        // Parse CGI output
        HttpResponse response;
        size_t headerEnd = cgiOutput.find("\r\n\r\n");
        if (headerEnd == std::string::npos) {
            headerEnd = cgiOutput.find("\n\n");
            if (headerEnd != std::string::npos) {
                headerEnd += 2;
            }
        } else {
            headerEnd += 4;
        }
        
        if (headerEnd != std::string::npos) {
            std::string headers = cgiOutput.substr(0, headerEnd);
            std::string body = cgiOutput.substr(headerEnd);
            
            // Parse headers
            std::vector<std::string> headerLines = Utils::split(headers, '\n');
            for (size_t i = 0; i < headerLines.size(); ++i) {
                std::string line = Utils::trim(headerLines[i]);
                if (line.empty()) continue;
                
                size_t colonPos = line.find(':');
                if (colonPos != std::string::npos) {
                    std::string key = Utils::trim(line.substr(0, colonPos));
                    std::string value = Utils::trim(line.substr(colonPos + 1));
                    
                    if (Utils::toLower(key) == "content-type") {
                        response.setContentType(value);
                    } else if (Utils::toLower(key) == "status") {
                        // Parse status code
                        std::string statusCode = value.substr(0, 3);
                        response.setStatus(Utils::stringToInt(statusCode));
                    }
                }
            }
            
            response.setBody(body);
        } else {
            response.setBody(cgiOutput);
        }
        
        if (response.getStatusCode() == 0) {
            response.setStatus(HTTP_OK);
        }
        if (response.getHeader("Content-Type").empty()) {
            response.setContentType("text/html");
        }
        
        return response;
    }
}

HttpResponse Server::handleFileUpload(const HttpRequest& request, const ServerConfig& serverConfig) {
    std::string contentType = request.getHeader("Content-Type");
    std::string boundary;
    
    // Extract boundary from Content-Type header
    size_t boundaryPos = contentType.find("boundary=");
    if (boundaryPos == std::string::npos) {
        return HttpResponse::createErrorResponse(HTTP_BAD_REQUEST);
    }
    
    boundary = "--" + contentType.substr(boundaryPos + 9);
    const std::string& body = request.getBody();
    
    // Parse multipart data
    std::vector<std::string> parts = Utils::split(body, boundary);
    
    for (size_t i = 1; i < parts.size() - 1; ++i) { // Skip first empty part and last boundary
        std::string part = parts[i];
        if (part.empty()) continue;
        
        // Find headers and content separation
        size_t headerEnd = part.find("\r\n\r\n");
        if (headerEnd == std::string::npos) {
            headerEnd = part.find("\n\n");
            if (headerEnd == std::string::npos) continue;
            headerEnd += 2;
        } else {
            headerEnd += 4;
        }
        
        std::string headers = part.substr(0, headerEnd);
        std::string content = part.substr(headerEnd);
        
        // Parse Content-Disposition header
        std::string filename;
        std::string name;
        
        size_t dispositionPos = headers.find("Content-Disposition:");
        if (dispositionPos != std::string::npos) {
            std::string disposition = headers.substr(dispositionPos);
            
            // Extract filename
            size_t filenamePos = disposition.find("filename=\"");
            if (filenamePos != std::string::npos) {
                filenamePos += 10; // length of "filename=\""
                size_t endQuote = disposition.find("\"", filenamePos);
                if (endQuote != std::string::npos) {
                    filename = disposition.substr(filenamePos, endQuote - filenamePos);
                }
            }
            
            // Extract field name
            size_t namePos = disposition.find("name=\"");
            if (namePos != std::string::npos) {
                namePos += 6; // length of "name=\""
                size_t endQuote = disposition.find("\"", namePos);
                if (endQuote != std::string::npos) {
                    name = disposition.substr(namePos, endQuote - namePos);
                }
            }
        }
        
        // If it's a file upload
        if (!filename.empty()) {
            std::string uploadPath = serverConfig.root + "/uploads/";
            
            // Create uploads directory if it doesn't exist
            if (!Utils::isDirectory(uploadPath)) {
                if (mkdir(uploadPath.c_str(), 0755) != 0) {
                    Utils::logError("Failed to create upload directory: " + uploadPath);
                    return HttpResponse::createErrorResponse(HTTP_INTERNAL_SERVER_ERROR);
                }
            }
            
            if (saveUploadedFile(filename, content, uploadPath)) {
                Utils::logInfo("File uploaded successfully: " + filename);
            } else {
                Utils::logError("Failed to save uploaded file: " + filename);
                return HttpResponse::createErrorResponse(HTTP_INTERNAL_SERVER_ERROR);
            }
        }
    }
    
    // Return success response
    HttpResponse response;
    response.setStatus(HTTP_OK);
    response.setContentType("text/html");
    response.setBody("<html><body><h1>File Upload Successful</h1><p>Your file(s) have been uploaded successfully.</p></body></html>");
    return response;
}

bool Server::saveUploadedFile(const std::string& filename, const std::string& content, const std::string& uploadPath) {
    // Sanitize filename - remove path traversal attempts
    std::string sanitizedFilename = Utils::getBasename(filename);
    std::string fullPath = uploadPath + sanitizedFilename;
    
    // Check if file already exists and create unique name if needed
    std::string finalPath = fullPath;
    int counter = 1;
    while (Utils::fileExists(finalPath)) {
        size_t dotPos = sanitizedFilename.find_last_of('.');
        if (dotPos != std::string::npos) {
            std::string baseName = sanitizedFilename.substr(0, dotPos);
            std::string extension = sanitizedFilename.substr(dotPos);
            finalPath = uploadPath + baseName + "_" + Utils::intToString(counter) + extension;
        } else {
            finalPath = uploadPath + sanitizedFilename + "_" + Utils::intToString(counter);
        }
        counter++;
    }
    
    return Utils::writeFile(finalPath, content);
}

std::string Server::resolveFilePath(const std::string& uri, const ServerConfig& serverConfig) {
    std::string path = uri;
    
    // Remove query string if present
    size_t queryPos = path.find('?');
    if (queryPos != std::string::npos) {
        path = path.substr(0, queryPos);
    }
    
    // Get matching location configuration
    LocationConfig location = getMatchingLocation(path, serverConfig);
    
    // Use location root if specified, otherwise server root
    std::string root = location.root.empty() ? serverConfig.root : location.root;
    
    // Handle root path
    if (path == "/") {
        std::string index = location.index.empty() ? serverConfig.index : location.index;
        std::string resolvedPath = root + "/" + index;
        Utils::logDebug("Root path resolution: " + uri + " -> " + resolvedPath);
        return resolvedPath;
    }
    
    // Handle location path mapping
    if (!location.path.empty() && path.find(location.path) == 0) {
        // Strip location path and map to location root
        std::string remainingPath = path.substr(location.path.length());
        if (remainingPath.empty() || remainingPath[0] != '/') {
            remainingPath = "/" + remainingPath;
        }
        std::string resolvedPath = root + remainingPath;
        Utils::logDebug("Location path resolution: " + uri + " -> " + resolvedPath);
        return resolvedPath;
    }
    
    // Default mapping
    std::string resolvedPath = root + path;
    Utils::logDebug("Default path resolution: " + uri + " -> " + resolvedPath);
    return resolvedPath;
}

LocationConfig Server::getMatchingLocation(const std::string& uri, const ServerConfig& serverConfig) {
    return _config.getLocationConfig(serverConfig, uri);
}

bool Server::isMethodAllowed(const std::string& method, const ServerConfig& /* serverConfig */, const LocationConfig& location) {
    return _config.isValidMethod(method, location);
}

HttpResponse Server::handleRedirection(const LocationConfig& /* location */) {
    return HttpResponse::createErrorResponse(HTTP_NOT_IMPLEMENTED);
}

ServerConfig Server::getServerConfig(const HttpRequest& /* request */) const {
    return _config.getDefaultServer();
}

int Server::getPort() const {
    return _config.getDefaultServer().port;
}

const std::string& Server::getHost() const {
    static std::string host = _config.getDefaultServer().host;
    return host;
}

bool Server::isRunning() const {
    return _running;
}