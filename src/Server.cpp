#include "../include/Server.hpp"
#include "../include/Utils.hpp"
#include "../include/CGI.hpp"
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sys/stat.h>
#include <cstdio>

Server::Server() : _running(false), _lastTimeoutCheck(time(NULL)) {
    _config = Config();
}

Server::Server(const Config& config) : _config(config), _running(false), _lastTimeoutCheck(time(NULL)) {
}

Server::~Server() {
    stop();
}

bool Server::initialize() {
    if (!createSockets()) {
        return false;
    }
    
    if (!bindSockets()) {
        return false;
    }
    
    if (!listenSockets()) {
        return false;
    }
    
    // Add all server sockets to poll list
    for (size_t i = 0; i < _servers.size(); ++i) {
        struct pollfd serverPollFd;
        serverPollFd.fd = _servers[i].socket;
        serverPollFd.events = POLLIN;
        _pollFds.push_back(serverPollFd);
    }
    
    return true;
}

bool Server::createSockets() {
    const std::vector<ServerConfig>& servers = _config.getServers();
    
    for (size_t i = 0; i < servers.size(); ++i) {
        const ServerConfig& serverConfig = servers[i];
        ServerInfo serverInfo;
        serverInfo.config = serverConfig;
        
        serverInfo.socket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverInfo.socket < 0) {
            Utils::logError("Failed to create socket for " + serverConfig.host + ":" + Utils::intToString(serverConfig.port));
            return false;
        }
        
        // Set socket options
        int opt = 1;
        if (setsockopt(serverInfo.socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            Utils::logError("Failed to set socket options for " + serverConfig.host + ":" + Utils::intToString(serverConfig.port));
            close(serverInfo.socket);
            return false;
        }
        
        // Set non-blocking
        int flags = fcntl(serverInfo.socket, F_GETFL, 0);
        if (fcntl(serverInfo.socket, F_SETFL, flags | O_NONBLOCK) < 0) {
            Utils::logError("Failed to set non-blocking mode for " + serverConfig.host + ":" + Utils::intToString(serverConfig.port));
            close(serverInfo.socket);
            return false;
        }
        
        _servers.push_back(serverInfo);
        _serverConfigs[serverInfo.socket] = serverConfig;
    }
    
    return true;
}

bool Server::bindSockets() {
    for (size_t i = 0; i < _servers.size(); ++i) {
        ServerInfo& serverInfo = _servers[i];
        const ServerConfig& config = serverInfo.config;
        
        memset(&serverInfo.addr, 0, sizeof(serverInfo.addr));
        serverInfo.addr.sin_family = AF_INET;
        serverInfo.addr.sin_port = htons(config.port);
        
        if (config.host == "localhost" || config.host == "127.0.0.1") {
            serverInfo.addr.sin_addr.s_addr = INADDR_ANY;
        } else {
            if (inet_pton(AF_INET, config.host.c_str(), &serverInfo.addr.sin_addr) <= 0) {
                Utils::logError("Invalid host address: " + config.host);
                return false;
            }
        }
        
        if (bind(serverInfo.socket, (struct sockaddr*)&serverInfo.addr, sizeof(serverInfo.addr)) < 0) {
            Utils::logError("Failed to bind socket " + config.host + ":" + Utils::intToString(config.port));
            return false;
        }
        
        Utils::logInfo("Socket bound to " + config.host + ":" + Utils::intToString(config.port));
    }
    
    return true;
}

bool Server::listenSockets() {
    for (size_t i = 0; i < _servers.size(); ++i) {
        const ServerInfo& serverInfo = _servers[i];
        if (listen(serverInfo.socket, MAX_CONNECTIONS) < 0) {
            Utils::logError("Failed to listen on socket " + serverInfo.config.host + ":" + Utils::intToString(serverInfo.config.port));
            return false;
        }
        Utils::logInfo("Listening on " + serverInfo.config.host + ":" + Utils::intToString(serverInfo.config.port));
    }
    
    return true;
}

void Server::run() {
    _running = true;
    
    while (_running) {
        int pollResult = poll(&_pollFds[0], _pollFds.size(), 1000);
        
        if (pollResult < 0) {
            if (errno == EINTR) {
                // Interrupted by signal (e.g., Ctrl+C), exit gracefully
                Utils::logInfo("Server interrupted by signal, shutting down...");
                break;
            }
            Utils::logError("Poll failed: " + std::string(strerror(errno)));
            break;
        }
        
        if (pollResult == 0) continue;
        
        // Check server sockets for new connections
        for (size_t i = 0; i < _servers.size(); ++i) {
            if (i < _pollFds.size() && _pollFds[i].revents & POLLIN) {
                acceptNewConnection(_servers[i].socket);
            }
        }
        
        // Check client sockets and CGI pipes
        for (size_t i = _servers.size(); i < _pollFds.size(); ++i) {
            int fd = _pollFds[i].fd;
            
            if (_pollFds[i].revents & (POLLHUP | POLLERR | POLLNVAL)) {
                // Check if this is a CGI pipe
                if (_cgiProcesses.find(fd) != _cgiProcesses.end()) {
                    handleCgiCompletion(fd);
                    --i;
                    continue;
                } else {
                    Utils::logError("Socket error for fd " + Utils::intToString(fd));
                    removeClient(fd);
                    --i;
                    continue;
                }
            }
            
            if (_pollFds[i].revents & POLLIN) {
                // Check if this is a CGI pipe
                if (_cgiProcesses.find(fd) != _cgiProcesses.end()) {
                    handleCgiCompletion(fd);
                } else {
                    handleClientRead(fd);
                }
            }
            
            if (_pollFds[i].revents & POLLOUT) {
				if (_cgiWritePipes.find(fd) != _cgiWritePipes.end()) {
                    handleCgiWrite(fd); // New function
                } else {
                handleClientWrite(fd);
                }
            }

			// Periodically check for timeouts
			time_t currentTime = time(NULL);
			if (currentTime - _lastTimeoutCheck >= 5) { // Check every 5 seconds
				checkClientTimeouts();
				checkCgiTimeouts();
				_lastTimeoutCheck = currentTime;
			}
		}
	}
}

bool Server::acceptNewConnection(int serverSocket) {
    struct sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);
    
    int clientFd = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
    if (clientFd < 0) {
        // For non-blocking sockets, since poll() indicated connection is ready,
        // a negative return typically indicates an error
        return false;
    }
    
    int flags = fcntl(clientFd, F_GETFL, 0);
    if (fcntl(clientFd, F_SETFL, flags | O_NONBLOCK) < 0) {
        Utils::logError("Failed to set client socket non-blocking");
        close(clientFd);
        return false;
    }
    
    int nodelay = 1;
    if (setsockopt(clientFd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay)) < 0) {
        Utils::logError("Failed to set TCP_NODELAY (non-fatal): " + std::string(strerror(errno)));
    }
    
    struct pollfd clientPollFd;
    clientPollFd.fd = clientFd;
    clientPollFd.events = POLLIN;
    clientPollFd.revents = 0;
    _pollFds.push_back(clientPollFd);
    
    _clients[clientFd] = Client(clientFd);
    _clientServerSockets[clientFd] = serverSocket; // Track which server socket this client came from
    
    std::string clientIP = Utils::getClientIP(clientFd);
    
    return true;
}

void Server::handleClientRead(int clientFd) {
    Client& client = _clients[clientFd];
    
    int socketError;
    socklen_t len = sizeof(socketError);
    if (getsockopt(clientFd, SOL_SOCKET, SO_ERROR, &socketError, &len) == 0 && socketError != 0) {
        Utils::logError("Socket error detected before read: " + std::string(strerror(socketError)));
        removeClient(clientFd);
        return;
    }
    
    if (client.shouldStopReading()) {
        return;
    }
    
    if (!client.readData()) {
        removeClient(clientFd);
        return;
    }

	if (client.getState() == Client::STATE_HEADERS_COMPLETE) {
		// Headers are parsed, but we haven't started reading the body.
		// This is our chance to check maxBodySize.

		// Get config
		ServerConfig serverConfig = getServerConfig(clientFd);
		HttpRequest tempRequest(client.getRequest(), ""); // Parse headers
		if (!tempRequest.isValid()) {
			HttpResponse response = createErrorResponse(400, serverConfig);
			queueResponse(clientFd, response);
			return;
		}
		LocationConfig location = _config.getLocationConfig(serverConfig, tempRequest.getUri(), tempRequest.getMethod());

		// Use location-specific maxBodySize if set, otherwise use server default
		size_t maxBodySize = (location.maxBodySize > 0) ? location.maxBodySize : serverConfig.maxBodySize;

		std::string extension = Utils::getFileExtension(tempRequest.getUri());
		if (extension == ".bla") {
				// For .bla files, check if any .bla regex CGI handler exists
				for (size_t i = 0; i < serverConfig.locations.size(); ++i) {
					const LocationConfig& regexLoc = serverConfig.locations[i];
					if (regexLoc.isRegex && !regexLoc.cgiPath.empty() && regexLoc.path.find(".bla") != std::string::npos) {
						// Found one, so apply the larger limit
						// maxBodySize = std::max(maxBodySize, static_cast<size_t>(100 * 1024 * 1024));
						// Utils::logInfo("Found .bla regex config, increasing maxBodySize to 100MB");
						break;
					}
				}
			}

		if (client.isChunked()) {
			Utils::logInfo("Chunked encoding detected. Setting max body size to " + Utils::sizeToString(maxBodySize));
		} else if (client.getContentLength() > 0) {
			Utils::logInfo("Content-Length detected. Setting max body size to " + Utils::sizeToString(maxBodySize));
		}

		// Tell the client to start reading the body
		client.beginReadingBody(maxBodySize);

		// Call readData() again to process any body data already in the buffer
		if (!client.readData()) {
			removeClient(clientFd);
			return;
		}
	}

	if (client.isRequestComplete()) {
		if (client.shouldStopReading()) {
			// This was set by the client's internal maxBodySize check
			Utils::logError("Request body exceeded max size during streaming. Queuing 413 and closing connection.");
			ServerConfig serverConfig = getServerConfig(clientFd);
			HttpResponse response = createErrorResponse(413, serverConfig);
			// Ensure Connection: close header for 413
			response.setHeader("Connection", "close"); 
			queueResponse(clientFd, response);
			client.markForCloseAfterWrite(); // Tell server to close after sending 413
			
			// Clean up the temporary body file if it exists
			if (!client.getBodyFilePath().empty()) {
				cleanupTempFile(client.getBodyFilePath());
			}
			
			client.clearRequest(); // Clear the request state
			return; // Stop processing this client for reads
		}
        Utils::logInfo("Request complete for client " + Utils::intToString(clientFd) + ", processing...");
        
        processHttpRequest(clientFd, client.getRequest(), client.getBodyFilePath());
        
        client.clearRequest();
    }
}

void Server::handleClientWrite(int clientFd) {
    if (!writeToClient(clientFd)) {
        removeClient(clientFd);
    }
}

void Server::processHttpRequest(int clientFd, const std::string& headers, const std::string& bodyFilePath) {
    HttpRequest httpRequest(headers, bodyFilePath);
	HttpResponse response;
    bool tempFileHandedToCGI = false; // Track if temp file ownership transferred to CGI
    
    if (!httpRequest.isValid()) {
        ServerConfig serverConfig = getServerConfig(clientFd);
        response = createErrorResponse(HTTP_BAD_REQUEST, serverConfig);
    } else {
        ServerConfig serverConfig = getServerConfig(clientFd);
        
        LocationConfig locationConfig = _config.getLocationConfig(serverConfig, httpRequest.getUri(), httpRequest.getMethod());
        
        // Check for redirections first
        if (!locationConfig.redirections.empty()) {
            response = handleRedirection(locationConfig);
            queueResponse(clientFd, response);
            // Clean up temp file before returning
            if (!bodyFilePath.empty()) {
                cleanupTempFile(bodyFilePath);
            }
            return;
        }
        
        if (!isMethodAllowed(httpRequest.getMethod(), serverConfig, locationConfig)) {
            response = createErrorResponse(HTTP_METHOD_NOT_ALLOWED, serverConfig);
        } else {
			if (httpRequest.getMethod() == "GET" || httpRequest.getMethod() == "HEAD") {
                // Check if this is a CGI request that should be handled asynchronously
                std::string filePath = resolveFilePath(httpRequest.getUri(), serverConfig);
                std::string extension = Utils::getFileExtension(filePath);
                
                if ((extension == ".php" || extension == ".py" || extension == ".sh") && Utils::fileExists(filePath)) {
                    LocationConfig location = _config.getLocationConfig(serverConfig, httpRequest.getUri(), httpRequest.getMethod());
                    
                    // Use async CGI for GET requests too
                    if (_cgiProcesses.size() < MAX_CONCURRENT_CGI_PROCESSES) {
                        if (startAsyncCGI(clientFd, filePath, httpRequest, serverConfig, location, "")) {
                            tempFileHandedToCGI = true;
                            return; // CGI started, response will be sent when ready
                        }
                    } else {
                        // Queue the CGI request
                        queueCgiRequest(clientFd, filePath, httpRequest, serverConfig, location);
                        tempFileHandedToCGI = true;
                        return;
                    }
                }
                
                // Not a CGI request or async CGI failed, handle normally
                response = handleGETRequest(httpRequest, serverConfig);
                if (httpRequest.getMethod() == "HEAD") {
                    response.setBody("");
                }
            } else if (httpRequest.getMethod() == "POST") {
                // Check if this should use async CGI
                std::string filePath = resolveFilePath(httpRequest.getUri(), serverConfig);
                std::string extension = Utils::getFileExtension(filePath);
                
                if ((extension == ".bla" || !httpRequest.getBodyFilePath().empty()) &&
                    (extension == ".php" || extension == ".py" || extension == ".sh" || extension == ".bla")) {
                    
                    LocationConfig location = _config.getLocationConfig(serverConfig, httpRequest.getUri(), httpRequest.getMethod());
                    bool canExecuteCGI = Utils::fileExists(filePath) || (location.isRegex && !location.cgiPath.empty());

					if (extension == ".bla") {
                        // For .bla files, ALWAYS find the regex location.
                        bool foundRegex = false;
                        for (size_t i = 0; i < serverConfig.locations.size(); ++i) {
                            const LocationConfig& regexLoc = serverConfig.locations[i];
                            if (regexLoc.isRegex && !regexLoc.cgiPath.empty() && regexLoc.path.find(".bla") != std::string::npos) {
                                location = regexLoc;
                                canExecuteCGI = true;
                                foundRegex = true;
                                // Utils::logInfo("Found regex CGI match for .bla: " + location.path);
                                break;
                            }
                        }
                        if (!foundRegex) {
                             Utils::logError("Request for .bla file but no regex CGI handler found.");
                             canExecuteCGI = false; // Force a 404
                        }
                    }

                    if (canExecuteCGI) {
                        // Queue CGI request or start immediately if capacity allows
                        if (_cgiProcesses.size() < MAX_CONCURRENT_CGI_PROCESSES) {
							if (startAsyncCGI(clientFd, filePath, httpRequest, serverConfig, location, httpRequest.getBodyFilePath())) {
								tempFileHandedToCGI = true;
								return;
							}
                        } else {
                            // Queue the request for later processing
                            queueCgiRequest(clientFd, filePath, httpRequest, serverConfig, location);
                            tempFileHandedToCGI = true;
                            return; // Don't queue any response - will be handled when CGI slot becomes available
                        }
                    }
                }
                // Fall back to synchronous handling
                response = handlePOSTRequest(httpRequest, serverConfig);
            } else if (httpRequest.getMethod() == "PUT") {
                response = handlePUTRequest(httpRequest, serverConfig);
            } else if (httpRequest.getMethod() == "DELETE") {
                response = handleDELETERequest(httpRequest, serverConfig);
            } else {
                response = createErrorResponse(HTTP_METHOD_NOT_ALLOWED, serverConfig);
            }
        }
    }
    
    queueResponse(clientFd, response);
    
    // Clean up temporary body file if not handed to CGI
    if (!bodyFilePath.empty() && !tempFileHandedToCGI) {
        cleanupTempFile(bodyFilePath);
    }
}

void Server::queueResponse(int clientFd, const HttpResponse& response) {
    // Make a copy to add headers
    HttpResponse modifiedResponse = response;
    
    // Add Connection keep-alive header for HTTP/1.1
    if (modifiedResponse.getHeader("Connection").empty()) {
        modifiedResponse.setHeader("Connection", "keep-alive");
    }
    
    std::string responseStr = modifiedResponse.toString();
    
    _pendingWrites[clientFd] = responseStr;
    _writeOffsets[clientFd] = 0;
    
    updatePollEvents(clientFd);
}

bool Server::writeToClient(int clientFd) {
    if (_pendingWrites.find(clientFd) == _pendingWrites.end()) {
        return true; // No pending writes
    }
    
    const std::string& response = _pendingWrites[clientFd];
    size_t offset = _writeOffsets[clientFd];
    
    ssize_t bytesSent = send(clientFd, response.c_str() + offset, response.length() - offset, 0);
    
    if (bytesSent <= 0) {
        // Connection closed or error - poll() should have indicated socket is ready
        return false;
    }
    
    _writeOffsets[clientFd] += bytesSent;
    
	if (_writeOffsets[clientFd] >= response.length()) {
    	// Write complete
    	bool shouldClose = false;
		if (_clients.count(clientFd)) { // Check if client still exists
			shouldClose = _clients[clientFd].shouldCloseAfterWrite();
		}

		_pendingWrites.erase(clientFd);
		_writeOffsets.erase(clientFd);

		if (shouldClose) {
			Utils::logInfo("Closing connection for client " + Utils::intToString(clientFd) + " after error response.");
			removeClient(clientFd); 
			return false; // Indicate client was removed
		} else {
			// Normal keep-alive: update poll events and wait for next request
			updatePollEvents(clientFd);
			return true;
		}
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
    // Clean up any temp file before removing the client
    std::map<int, Client>::iterator clientIt = _clients.find(clientFd);
    if (clientIt != _clients.end()) {
        const std::string& bodyFilePath = clientIt->second.getBodyFilePath();
        if (!bodyFilePath.empty()) {
            cleanupTempFile(bodyFilePath);
        }
    }
    
    for (std::vector<struct pollfd>::iterator it = _pollFds.begin(); it != _pollFds.end(); ++it) {
        if (it->fd == clientFd) {
            _pollFds.erase(it);
            break;
        }
    }

    _clients.erase(clientFd);
    _pendingWrites.erase(clientFd);
    _writeOffsets.erase(clientFd);
    _clientServerSockets.erase(clientFd); // Clean up server socket mapping

    close(clientFd);
}

void Server::stop() {
    _running = false;

    for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        close(it->first);
    }
    _clients.clear();
    _pollFds.clear();
    _pendingWrites.clear();
    _writeOffsets.clear();
    _clientServerSockets.clear(); // Clear client-server socket mapping
    
    // Close all server sockets
    for (size_t i = 0; i < _servers.size(); ++i) {
        close(_servers[i].socket);
    }
    _servers.clear();
    _serverConfigs.clear();
    
}

HttpResponse Server::handleGETRequest(const HttpRequest& request, const ServerConfig& serverConfig) {
    std::string filePath = resolveFilePath(request.getUri(), serverConfig);
    
    // CGI requests should now be handled asynchronously before reaching here
    // This function only handles static files and directories
    
    if (Utils::fileExists(filePath)) {
        if (Utils::isDirectory(filePath)) {
            return handleDirectoryRequest(filePath, request.getUri(), serverConfig);
        } else {
            return serveStaticFile(filePath, serverConfig);
        }
    } else {
        return createErrorResponse(HTTP_NOT_FOUND, serverConfig);
    }
}

HttpResponse Server::handlePOSTRequest(const HttpRequest& request, const ServerConfig& serverConfig) {
    // Validate body size against location-specific limits
    LocationConfig bodyCheckLocation = _config.getLocationConfig(serverConfig, request.getUri(), request.getMethod());
    size_t maxBodySize = (bodyCheckLocation.maxBodySize > 0) ? bodyCheckLocation.maxBodySize : serverConfig.maxBodySize;

	if (!request.getBodyFilePath().empty()) {
        struct stat st;
        if (stat(request.getBodyFilePath().c_str(), &st) == 0) {
            if (st.st_size > static_cast<long>(maxBodySize)) {
                Utils::logError("POST body file size " + Utils::sizeToString(st.st_size) + 
                               " exceeds limit " + Utils::sizeToString(maxBodySize) + 
                               " for location " + bodyCheckLocation.path);
                return createErrorResponse(413, serverConfig);
            }
        }
    }

    std::string contentType = request.getHeader("Content-Type");
    
    // Check if it's a file upload
    if (contentType.find("multipart/form-data") != std::string::npos) {
        return handleFileUpload(request, serverConfig);
    }
    
    // Check if it's a CGI request
    std::string filePath = resolveFilePath(request.getUri(), serverConfig);
    std::string extension = Utils::getFileExtension(filePath);
    Utils::logInfo("POST request to: " + request.getUri() + ", filePath: " + filePath + ", extension: " + extension);

	if (extension == ".php" || extension == ".py" || extension == ".sh" || extension == ".bla") {
        // CGI requests should now be handled asynchronously before reaching here
        // If we got here, it means async CGI wasn't triggered (shouldn't happen normally)
        Utils::logError("POST CGI request reached handlePOSTRequest - should be handled asynchronously");
        return createErrorResponse(HTTP_INTERNAL_SERVER_ERROR, serverConfig);
    }
    
    // Handle JSON POST requests to create files
    if (contentType.find("application/json") != std::string::npos) {
        return handleJSONPost(request, serverConfig);
    }
    
    // Check if this is a POST to an upload location
    LocationConfig location = getMatchingLocation(request.getUri(), serverConfig);
    if (!location.uploadPath.empty() && request.getUri().find("/post_body") == std::string::npos) {
        return handleSimpleFileUpload(request, serverConfig, location);
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
        return createErrorResponse(HTTP_FORBIDDEN, serverConfig);
    }

	std::ifstream srcFile(request.getBodyFilePath().c_str(), std::ios::binary);
    if (!srcFile.is_open()) {
        Utils::logError("PUT: Failed to open temp body file: " + request.getBodyFilePath());
        return createErrorResponse(HTTP_INTERNAL_SERVER_ERROR, serverConfig);
    }
    std::ofstream destFile(filePath.c_str(), std::ios::binary);
    if (!destFile.is_open()) {
        Utils::logError("PUT: Failed to open destination file: " + filePath);
        srcFile.close();
        return createErrorResponse(HTTP_INTERNAL_SERVER_ERROR, serverConfig);
    }
    destFile << srcFile.rdbuf();
    srcFile.close();
    destFile.close();

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
        return createErrorResponse(HTTP_FORBIDDEN, serverConfig);
    }
    
    // Check if file exists
    if (!Utils::fileExists(filePath)) {
        return createErrorResponse(HTTP_NOT_FOUND, serverConfig);
    }
    
    // Check if it's a directory
    if (Utils::isDirectory(filePath)) {
        return createErrorResponse(HTTP_FORBIDDEN, serverConfig);
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
        return createErrorResponse(HTTP_INTERNAL_SERVER_ERROR, serverConfig);
    }
}

HttpResponse Server::serveStaticFile(const std::string& path, const ServerConfig& /* serverConfig */) {
    return HttpResponse::createFileResponse(path);
}

HttpResponse Server::handleDirectoryRequest(const std::string& path, const std::string& uri, const ServerConfig& serverConfig) {
    LocationConfig location = getMatchingLocation(uri, serverConfig);
    
    // Check for location-specific default file first, then server default
    std::string indexFile = location.index.empty() ? serverConfig.index : location.index;
    std::string indexPath = path + "/" + indexFile;
    if (Utils::fileExists(indexPath) && !Utils::isDirectory(indexPath)) {
        return serveStaticFile(indexPath, serverConfig);
    }
    
    if (uri.find("/directory/") == 0 && uri != "/directory") {
        std::string youpiPath = path + "/youpi.bad_extension";
        if (Utils::fileExists(youpiPath) && !Utils::isDirectory(youpiPath)) {
            return serveStaticFile(youpiPath, serverConfig);
        }
        return createErrorResponse(HTTP_NOT_FOUND, serverConfig);
    }
    
    if (!location.autoIndex) {
        return createErrorResponse(HTTP_FORBIDDEN, serverConfig);
    }
    
    std::string urlPath = uri;
    if (urlPath.find(serverConfig.root) == 0) {
        urlPath = urlPath.substr(serverConfig.root.length());
        if (urlPath.empty()) urlPath = "/";
    }
    
    return generateDirectoryListing(path, urlPath, serverConfig);
}

HttpResponse Server::generateDirectoryListing(const std::string& path, const std::string& urlPath, const ServerConfig& serverConfig) {
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        return createErrorResponse(HTTP_INTERNAL_SERVER_ERROR, serverConfig);
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

HttpResponse Server::handleFileUpload(const HttpRequest& request, const ServerConfig& serverConfig) {
    std::string contentType = request.getHeader("Content-Type");
    std::string boundary;
    
    // Extract boundary from Content-Type header
    size_t boundaryPos = contentType.find("boundary=");
    if (boundaryPos == std::string::npos) {
        return HttpResponse::createErrorResponse(HTTP_BAD_REQUEST);
    }
    
    boundary = "--" + contentType.substr(boundaryPos + 9);
	std::string body = Utils::readFile(request.getBodyFilePath());
    
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

HttpResponse Server::handleSimpleFileUpload(const HttpRequest& request, const ServerConfig& serverConfig, const LocationConfig& location) {
    // Check location-specific body size limit
    // size_t bodySize = request.getBody().length();
	size_t bodySize = 0;
    if (!request.getBodyFilePath().empty()) {
        struct stat st;
        if (stat(request.getBodyFilePath().c_str(), &st) == 0) {
            bodySize = st.st_size;
        }
    }
    size_t maxBodySize = location.maxBodySize > 0 ? location.maxBodySize : serverConfig.maxBodySize;
    
    if (bodySize > maxBodySize) {
        Utils::logError("Upload body size " + Utils::sizeToString(bodySize) + 
                       " exceeds location limit " + Utils::sizeToString(maxBodySize));
        return createErrorResponse(413, serverConfig); // 413 Payload Too Large
    }
    
    // Get the upload path from location config
    std::string uploadPath = location.uploadPath;
    if (uploadPath.empty()) {
        uploadPath = serverConfig.root + "/uploads/";
    }
    
    // Ensure upload path ends with /
    if (!uploadPath.empty() && uploadPath[uploadPath.length() - 1] != '/') {
        uploadPath += "/";
    }
    
    // Create upload directory if it doesn't exist
    if (!Utils::isDirectory(uploadPath)) {
        if (mkdir(uploadPath.c_str(), 0755) != 0) {
            Utils::logError("Failed to create upload directory: " + uploadPath);
            return createErrorResponse(HTTP_INTERNAL_SERVER_ERROR, serverConfig);
        }
    }
    
    // Generate filename - use URI path or timestamp
    std::string filename;
    std::string uri = request.getUri();
    
    // Remove query parameters
    size_t queryPos = uri.find('?');
    if (queryPos != std::string::npos) {
        uri = uri.substr(0, queryPos);
    }
    
    // Extract filename from URI if it looks like a file
    size_t lastSlash = uri.find_last_of('/');
    if (lastSlash != std::string::npos && lastSlash < uri.length() - 1) {
        std::string possibleFilename = uri.substr(lastSlash + 1);
        if (!possibleFilename.empty() && possibleFilename.find('.') != std::string::npos) {
            filename = possibleFilename;
        }
    }
    
    // If no filename from URI, generate one with timestamp
    if (filename.empty()) {
        std::string contentType = request.getHeader("Content-Type");
        std::string extension = ".txt";
        if (contentType.find("image") != std::string::npos) {
            extension = ".dat";
        } else if (contentType.find("json") != std::string::npos) {
            extension = ".json";
        }
        
        // Simple timestamp-based filename
        time_t now = time(0);
        char timeStr[32];
        strftime(timeStr, sizeof(timeStr), "%Y%m%d_%H%M%S", localtime(&now));
        filename = std::string("upload_") + timeStr + extension;
    }
    
    // Save the file
	if (saveUploadedFile(filename, request.getBodyFilePath(), uploadPath, true)) {
        Utils::logInfo("File uploaded successfully via simple POST: " + filename);
        
        HttpResponse response(201);
        response.setContentType("text/html");
        response.setBody("<html><body><h1>File Upload Successful</h1><p>File '" + filename + "' uploaded successfully.</p></body></html>");
        return response;
    } else {
        Utils::logError("Failed to save uploaded file: " + filename);
        return createErrorResponse(HTTP_INTERNAL_SERVER_ERROR, serverConfig);
    }
	return createErrorResponse(HTTP_INTERNAL_SERVER_ERROR, serverConfig);
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

bool Server::saveUploadedFile(const std::string& filename, const std::string& srcFilePath, const std::string& uploadPath, bool isTempFile) {
    // Sanitize filename
    std::string sanitizedFilename = Utils::getBasename(filename);
    std::string fullPath = uploadPath + sanitizedFilename;
    
    // Check if file already exists
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

    if (isTempFile) {
        // Try to rename (move) the temp file, which is very fast
        if (std::rename(srcFilePath.c_str(), finalPath.c_str()) == 0) {
            return true;
        }
        // If rename fails (e.g., different devices), fall back to copy
    }
    
    // Fallback to copy
    std::ifstream srcFile(srcFilePath.c_str(), std::ios::binary);
    if (!srcFile.is_open()) {
        Utils::logError("saveUploadedFile: Failed to open source file: " + srcFilePath);
        return false;
    }
    std::ofstream destFile(finalPath.c_str(), std::ios::binary);
    if (!destFile.is_open()) {
        Utils::logError("saveUploadedFile: Failed to open destination file: " + finalPath);
        srcFile.close();
        return false;
    }
    destFile << srcFile.rdbuf();
    srcFile.close();
    destFile.close();
    return true;
}

std::string Server::resolveFilePath(const std::string& uri, const ServerConfig& serverConfig) {
    std::string path = uri;
    
    size_t queryPos = path.find('?');
    if (queryPos != std::string::npos) {
        path = path.substr(0, queryPos);
    }
    
    LocationConfig location = getMatchingLocation(path, serverConfig);
    
    std::string root = location.root.empty() ? serverConfig.root : location.root;
    
    if (path == "/") {
        std::string index = location.index.empty() ? serverConfig.index : location.index;
        return root + "/" + index;
    }
    
    if (!location.path.empty() && path.find(location.path) == 0) {
        std::string remainingPath = path.substr(location.path.length());
        if (remainingPath.empty() || remainingPath[0] != '/') {
            remainingPath = "/" + remainingPath;
        }
        return root + remainingPath;
    }
    
    return root + path;
}

LocationConfig Server::getMatchingLocation(const std::string& uri, const ServerConfig& serverConfig) {
    return _config.getLocationConfig(serverConfig, uri);
}



bool Server::isMethodAllowed(const std::string& method, const ServerConfig& /* serverConfig */, const LocationConfig& location) {
    return _config.isValidMethod(method, location);
}

HttpResponse Server::handleRedirection(const LocationConfig& location) {
    // Check if location has redirections defined
    if (location.redirections.empty()) {
        return HttpResponse::createErrorResponse(HTTP_INTERNAL_SERVER_ERROR);
    }
    
    // Get the first redirection (we typically use 301 or 302)
    std::map<int, std::string>::const_iterator it = location.redirections.begin();
    int redirectCode = it->first;
    std::string redirectUrl = it->second;
    
    // Create appropriate redirect response based on status code
    HttpResponse response(redirectCode);
    response.setHeader("Location", redirectUrl);
    
    std::string redirectPage = "<!DOCTYPE html>\n"
                              "<html>\n"
                              "<head>\n"
                              "    <title>" + Utils::intToString(redirectCode) + " " + HttpResponse::getStatusMessage(redirectCode) + "</title>\n"
                              "    <meta http-equiv=\"refresh\" content=\"0; url=" + redirectUrl + "\">\n"
                              "</head>\n"
                              "<body>\n"
                              "    <h1>" + HttpResponse::getStatusMessage(redirectCode) + "</h1>\n"
                              "    <p>The document has moved <a href=\"" + redirectUrl + "\">here</a>.</p>\n"
                              "</body>\n"
                              "</html>\n";
    
    response.setContentType("text/html");
    response.setBody(redirectPage);
    
    return response;
}

ServerConfig Server::getServerConfig(int clientFd) const {
    // Find which server socket this client came from
    std::map<int, int>::const_iterator it = _clientServerSockets.find(clientFd);
    if (it != _clientServerSockets.end()) {
        int serverSocket = it->second;
        std::map<int, ServerConfig>::const_iterator configIt = _serverConfigs.find(serverSocket);
        if (configIt != _serverConfigs.end()) {
            return configIt->second;
        }
    }
    // Fallback to default server if mapping not found
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

HttpResponse Server::handleJSONPost(const HttpRequest& request, const ServerConfig& serverConfig) {
    std::string uri = request.getUri();

	std::string body = Utils::readFile(request.getBodyFilePath());

    // Generate a filename based on current timestamp if posting to a directory
    std::string filePath;
    if (uri.empty() || uri[uri.length() - 1] == '/') {
        // Posting to directory - create timestamped file
        time_t now = time(0);
        std::string timestamp = Utils::sizeToString(now);
        filePath = resolveFilePath(uri + "post-" + timestamp + ".json", serverConfig);
    } else if (uri.find(".json") == std::string::npos) {
        // No .json extension - add it
        filePath = resolveFilePath(uri + ".json", serverConfig);
    } else {
        // Use the URI as-is
        filePath = resolveFilePath(uri, serverConfig);
    }
    
    // Security check - ensure the file path is within the server root
    if (filePath.find("..") != std::string::npos) {
        return createErrorResponse(HTTP_FORBIDDEN, serverConfig);
    }
    
    // Write the JSON content to the file
    if (!Utils::writeFile(filePath, body)) {
        return createErrorResponse(HTTP_INTERNAL_SERVER_ERROR, serverConfig);
    }
    
    Utils::logInfo("JSON file created via POST: " + filePath);
    
    // Return 201 Created with the file location
    HttpResponse response(201);
    response.setContentType("application/json");
    response.setHeader("Location", uri);
    response.setBody("{\"message\":\"JSON file created successfully\",\"location\":\"" + uri + "\"}");
    
    return response;
}

HttpResponse Server::createErrorResponse(int statusCode, const ServerConfig& serverConfig) {
    // Check if custom error page is configured
    std::map<int, std::string>::const_iterator it = serverConfig.errorPages.find(statusCode);
    if (it != serverConfig.errorPages.end()) {
        // Construct full path to error page file
        std::string errorPagePath = serverConfig.root;
        if (!errorPagePath.empty() && errorPagePath[errorPagePath.length() - 1] != '/') {
            errorPagePath += "/";
        }
        std::string relativePath = it->second;
        if (!relativePath.empty() && relativePath[0] == '/') {
            relativePath = relativePath.substr(1);
        }
        errorPagePath += relativePath;
        
        // Try to read custom error page
        std::ifstream file(errorPagePath.c_str());
        if (file.is_open()) {
            std::string content;
            std::string line;
            while (std::getline(file, line)) {
                content += line + "\n";
            }
            file.close();
            
            // Create response with custom error page content
            HttpResponse response;
            response.setStatus(statusCode);
            response.setHeader("Content-Type", "text/html");
            response.setHeader("Content-Length", Utils::sizeToString(content.length()));
            response.setBody(content);
            return response;
        } else {
            Utils::logError("Failed to open error page file: " + errorPagePath);
        }
    } else {
        Utils::logInfo("No custom error page configured for status " + Utils::intToString(statusCode));
    }
    
    // Fallback to default error response
    return HttpResponse::createErrorResponse(statusCode);
}

bool Server::startAsyncCGI(int clientFd, const std::string& scriptPath, const HttpRequest& request, const ServerConfig& serverConfig, const LocationConfig& locationConfig, const std::string& bodyFilePath) {
    // For regex locations with CGI, we don't require the physical file to exist
    if (!Utils::fileExists(scriptPath) && !locationConfig.isRegex) {
        HttpResponse response = createErrorResponse(HTTP_NOT_FOUND, serverConfig);
        queueResponse(clientFd, response);
        return false;
    }
    
    // Determine CGI interpreter/executable
    std::string interpreter;
    std::string extension = Utils::getFileExtension(scriptPath);
    
    if (!locationConfig.cgiPath.empty()) {
        interpreter = locationConfig.cgiPath;
    } else if (extension == ".php") {
        interpreter = "/usr/bin/php-cgi";
    } else if (extension == ".py") {
        interpreter = "/usr/bin/python3";
    } else if (extension == ".sh") {
        interpreter = "/bin/bash";
    } else {
        HttpResponse response = createErrorResponse(HTTP_NOT_IMPLEMENTED, serverConfig);
        queueResponse(clientFd, response);
        return false;
    }
    
    // Create pipes for communication
    int pipeFdIn[2], pipeFdOut[2];
    if (pipe(pipeFdIn) == -1 || pipe(pipeFdOut) == -1) {
        Utils::logError("Failed to create pipes for CGI: " + std::string(strerror(errno)));
        HttpResponse response = createErrorResponse(HTTP_INTERNAL_SERVER_ERROR, serverConfig);
        queueResponse(clientFd, response);
        return false;
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        Utils::logError("Failed to fork for CGI: " + std::string(strerror(errno)));
        close(pipeFdIn[0]); close(pipeFdIn[1]);
        close(pipeFdOut[0]); close(pipeFdOut[1]);
        HttpResponse response = createErrorResponse(HTTP_INTERNAL_SERVER_ERROR, serverConfig);
        queueResponse(clientFd, response);
        return false;
    }
    
    if (pid == 0) {
        // Child process - execute CGI (same as original executeCGI)
        close(pipeFdIn[1]);  // Close write end of input pipe
        close(pipeFdOut[0]); // Close read end of output pipe
        
        dup2(pipeFdIn[0], STDIN_FILENO);
        dup2(pipeFdOut[1], STDOUT_FILENO);
        // Don't redirect stderr - let it go to the parent's stderr
        
        close(pipeFdIn[0]);
        close(pipeFdOut[1]);
        
        CGI cgi;
        cgi.setScriptPath(scriptPath);
        cgi.setInterpreter(interpreter);
        
		if (!bodyFilePath.empty()) {
            // This is now the ONLY way the child gets a body
            cgi.setBodyFromFile(bodyFilePath);
        } else {
            cgi.setBody(""); // No file, no body
        }
		
        cgi.setupEnvironment(request, serverConfig.serverName, serverConfig.port);
        
        char** envArray = cgi.createEnvArray();
        if (!envArray) {
            Utils::logError("Failed to create environment array for CGI");
            exit(1);
        }
        
        std::string scriptDir = Utils::getDirectory(scriptPath);
        std::string scriptName = Utils::getBasename(scriptPath);
        if (!scriptDir.empty()) {
            chdir(scriptDir.c_str());
        }
        
        // Execute based on whether we have a custom CGI path or standard interpreter
        if (!locationConfig.cgiPath.empty()) {
            // Custom CGI executable (like ubuntu_cgi_tester)
            char* args[] = { const_cast<char*>(interpreter.c_str()), const_cast<char*>(scriptPath.c_str()), NULL };
            execve(interpreter.c_str(), args, envArray);
        } else if (extension == ".php") {
            char* args[] = { const_cast<char*>(interpreter.c_str()), const_cast<char*>(scriptPath.c_str()), NULL };
            execve(interpreter.c_str(), args, envArray);
        } else {
            char* args[] = { const_cast<char*>(interpreter.c_str()), const_cast<char*>(scriptName.c_str()), NULL };
            execve(interpreter.c_str(), args, envArray);
        }
        
        cgi.freeEnvArray(envArray);
        Utils::logError("exec failed: " + std::string(strerror(errno)));
        exit(1);
    } else {
        // Parent process - set up async monitoring
        close(pipeFdIn[0]);  // Close read end of input pipe
        close(pipeFdOut[1]); // Close write end of output pipe
        
        // Make output pipe non-blocking
        int flags = fcntl(pipeFdOut[0], F_GETFL, 0);
        fcntl(pipeFdOut[0], F_SETFL, flags | O_NONBLOCK);

		int cgiInputFd = pipeFdIn[1];
        flags = fcntl(cgiInputFd, F_GETFL, 0);
        fcntl(cgiInputFd, F_SETFL, flags | O_NONBLOCK);
        
        // Add CGI output pipe to poll monitoring (this part is the same)
        struct pollfd cgiPollFd;
        cgiPollFd.fd = pipeFdOut[0];
        cgiPollFd.events = POLLIN;
        cgiPollFd.revents = 0;
        _pollFds.push_back(cgiPollFd);
        
        // Store CGI process information
        CgiProcess cgiProc;
        cgiProc.pid = pid;
        cgiProc.outputFd = pipeFdOut[0];
        cgiProc.clientFd = clientFd;
        cgiProc.startTime = time(NULL);
        cgiProc.serverConfig = serverConfig;
        
        // Store new input pipe info
        cgiProc.inputFd = cgiInputFd;

		cgiProc.bodyFilePath = bodyFilePath;

		if (request.getMethod() == "POST" && !cgiProc.bodyFilePath.empty()) {
            cgiProc.bodyFile = new std::ifstream(cgiProc.bodyFilePath.c_str(), std::ios::binary);
            if (cgiProc.bodyFile->is_open()) { // Use ->
                // Add CGI *input* pipe to poll monitoring
                struct pollfd cgiWritePollFd;
                cgiWritePollFd.fd = cgiInputFd;
                cgiWritePollFd.events = POLLOUT;
                cgiWritePollFd.revents = 0;
                _pollFds.push_back(cgiWritePollFd);
                
                _cgiProcesses[pipeFdOut[0]] = cgiProc; // This copy is fine now
                _cgiWritePipes[cgiInputFd] = &_cgiProcesses[pipeFdOut[0]];

            } else {
                Utils::logError("Failed to open body file: " + cgiProc.bodyFilePath);
                delete cgiProc.bodyFile; // Clean up memory
                cgiProc.bodyFile = NULL;
                close(cgiInputFd);
                _cgiProcesses[pipeFdOut[0]] = cgiProc;
            }
        } else {
            // No body to write, just close the input pipe
            close(cgiInputFd);
            _cgiProcesses[pipeFdOut[0]] = cgiProc;
        }

        Utils::logInfo("Started async CGI process for client " + Utils::intToString(clientFd) +
                      " (active: " + Utils::intToString(_cgiProcesses.size()) +
                      ", queued: " + Utils::intToString(_cgiQueue.size()) + ")");
        return true;
    }
}

void Server::handleCgiCompletion(int cgiOutputFd) {
    std::map<int, CgiProcess>::iterator it = _cgiProcesses.find(cgiOutputFd);
    if (it == _cgiProcesses.end()) {
        Utils::logError("CGI completion called for unknown fd: " + Utils::intToString(cgiOutputFd));
        return;
    }
    
    CgiProcess& cgiProc = it->second;
    // Handle CGI completion for client
    
    // Sequential read: Read all available data in a loop until EOF or would block
    char buffer[65536];
    ssize_t bytesRead;
    
    // Read all available data - poll() indicated this fd is ready
    while ((bytesRead = read(cgiOutputFd, buffer, sizeof(buffer))) > 0) {
        cgiProc.output.append(buffer, bytesRead);
        
        // For very large files, occasionally yield control to prevent blocking
        if (cgiProc.output.length() % (10 * 1024 * 1024) == 0) { // Every 10MB
            break; // Yield control, continue reading in next poll cycle
        }
    }
    
    // bytesRead == 0: EOF - CGI process closed the pipe
    // bytesRead < 0: Either EAGAIN (would block) or real error
    // Since we can't check errno, we handle both cases:
    // - If it's EAGAIN, poll() will notify us again
    // - If it's a real error and the process died, poll() will give POLLERR/POLLHUP
    
    if (bytesRead == 0) {
        // EOF - CGI process finished writing
        // EOF - CGI process finished writing
        Utils::logInfo("CGI output complete for client " + Utils::intToString(cgiProc.clientFd) + 
                      ", total size: " + Utils::sizeToString(cgiProc.output.length()) + " bytes");
        
        // Wait for CGI process to finish
        int status;
        waitpid(cgiProc.pid, &status, WNOHANG);
        
        // Parse CGI output and send response
        HttpResponse response;
        size_t headerEnd = cgiProc.output.find("\r\n\r\n");
        if (headerEnd == std::string::npos) {
            headerEnd = cgiProc.output.find("\n\n");
            if (headerEnd != std::string::npos) {
                headerEnd += 2;
            }
        } else {
            headerEnd += 4;
        }
        
        if (headerEnd != std::string::npos) {
            std::string headers = cgiProc.output.substr(0, headerEnd);
            std::string body = cgiProc.output.substr(headerEnd);
            
            // Parse headers
            std::vector<std::string> headerLines = Utils::split(headers, '\n');
            for (size_t i = 0; i < headerLines.size(); ++i) {
                std::string line = Utils::trim(headerLines[i]);
                if (line.empty()) continue;
                
                size_t colonPos = line.find(':');
                if (colonPos != std::string::npos) {
                    std::string name = Utils::trim(line.substr(0, colonPos));
                    std::string value = Utils::trim(line.substr(colonPos + 1));
                    
                    if (name == "Status") {
                        int statusCode = Utils::stringToInt(value.substr(0, 3));
                        response.setStatus(statusCode);
                    } else if (name == "Content-Type") {
                        response.setContentType(value);
                    } else {
                        response.setHeader(name, value);
                    }
                }
            }
            
            response.setBody(body);
        } else {
            // No proper headers, treat as plain text
            response.setStatus(HTTP_OK);
            response.setContentType("text/plain");
            response.setBody(cgiProc.output);
        }
        
        // Send response to client
        queueResponse(cgiProc.clientFd, response);
        
        // Clean up
        cleanupCgiProcess(cgiOutputFd);
    }
}

void Server::cleanupCgiProcess(int cgiOutputFd) {
    std::map<int, CgiProcess>::iterator it = _cgiProcesses.find(cgiOutputFd);
    if (it == _cgiProcesses.end()) {
        return;
    }

	CgiProcess& cgiProc = it->second;

    // Remove output pipe from poll monitoring
    for (std::vector<struct pollfd>::iterator pollIt = _pollFds.begin(); pollIt != _pollFds.end(); ++pollIt) {
        if (pollIt->fd == cgiOutputFd) {
            _pollFds.erase(pollIt);
            break;
        }
    }
    
    // Close output pipe
    close(cgiOutputFd);

	if (cgiProc.bodyFile != NULL) {
        cgiProc.bodyFile->close();
        delete cgiProc.bodyFile;
        cgiProc.bodyFile = NULL;
    }
    
    // Clean up temp file
    if (!it->second.bodyFilePath.empty()) {
        cleanupTempFile(it->second.bodyFilePath);
    }

    // Remove from CGI processes map
    _cgiProcesses.erase(it);
    
    Utils::logInfo("Cleaned up CGI process for client " + Utils::intToString(it->second.clientFd) + 
                  " (active: " + Utils::intToString(_cgiProcesses.size()) + 
                  ", queued: " + Utils::intToString(_cgiQueue.size()) + ")");
    
    // Process queue to start next CGI if available
    processCgiQueue();
}

void Server::queueCgiRequest(int clientFd, const std::string& scriptPath, 
                           const HttpRequest& request, const ServerConfig& serverConfig, 
                           const LocationConfig& locationConfig) {
    QueuedCgiRequest queuedRequest;
    queuedRequest.clientFd = clientFd;
    queuedRequest.scriptPath = scriptPath;
    queuedRequest.serverConfig = serverConfig;
    queuedRequest.locationConfig = locationConfig;
	queuedRequest.request = request;

	queuedRequest.bodyFilePath = request.getBodyFilePath();

    _cgiQueue.push_back(queuedRequest);
    Utils::logInfo("Queued CGI request for client " + Utils::intToString(clientFd) + " (queue size: " + Utils::intToString(_cgiQueue.size()) + ")");
}

void Server::processCgiQueue() {
    while (!_cgiQueue.empty() && _cgiProcesses.size() < MAX_CONCURRENT_CGI_PROCESSES) {
        QueuedCgiRequest& queuedRequest = _cgiQueue.front();
        
        Utils::logInfo("Processing queued CGI request for client " + Utils::intToString(queuedRequest.clientFd) + 
                      " (remaining queue: " + Utils::intToString(_cgiQueue.size() - 1) + ")");
        
        if (startAsyncCGI(queuedRequest.clientFd, queuedRequest.scriptPath, 
                         queuedRequest.request, queuedRequest.serverConfig, 
                         queuedRequest.locationConfig, queuedRequest.bodyFilePath)) {
            // Successfully started - remove from queue
            _cgiQueue.erase(_cgiQueue.begin());
        } else {
            // Failed to start - keep in queue and break to avoid infinite loop
            std::cerr << "Failed to start queued CGI for client " << queuedRequest.clientFd << std::endl;
            break;
        }
    }
}

// Temporary file utilities for large body handling
std::string Server::createTempFile() {
    static int counter = 0;
    std::ostringstream oss;
    oss << "/tmp/webserv_body_" << getpid() << "_" << time(NULL) << "_" << (++counter);
    return oss.str();
}

bool Server::writeBodyToFile(const std::string& body, const std::string& filePath) {
    std::ofstream file(filePath.c_str(), std::ios::binary);
    if (!file) {
        Utils::logError("Failed to create temporary file: " + filePath);
        return false;
    }
    
    file.write(body.c_str(), body.length());
    if (!file) {
        Utils::logError("Failed to write body to temporary file: " + filePath);
        file.close();
        unlink(filePath.c_str()); // Cleanup on failure
        return false;
    }
    
    file.close();
    Utils::logInfo("Written " + Utils::sizeToString(body.length()) + " bytes to temp file: " + filePath);
    return true;
}

std::string Server::readBodyFromFile(const std::string& filePath) {
    std::ifstream file(filePath.c_str(), std::ios::binary);
    if (!file) {
        Utils::logError("Failed to open temporary file: " + filePath);
        return "";
    }
    
    std::ostringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    std::string body = buffer.str();
    Utils::logInfo("Read " + Utils::sizeToString(body.length()) + " bytes from temp file: " + filePath);
    return body;
}

void Server::cleanupTempFile(const std::string& filePath) {
    if (unlink(filePath.c_str()) == 0) {
        Utils::logInfo("Cleaned up temporary file: " + filePath);
    } else {
        Utils::logError("Failed to cleanup temporary file: " + filePath);
    }
}

void Server::handleCgiWrite(int cgiInputFd) {
    // Look up the CgiProcess using the input fd
    std::map<int, CgiProcess*>::iterator it = _cgiWritePipes.find(cgiInputFd);
    if (it == _cgiWritePipes.end()) return;
    
    CgiProcess* cgiProc = it->second;

    if (!cgiProc->bodyFile || !cgiProc->bodyFile->is_open()) {
        // Should not happen, but safeguard
        cleanupCgiProcess(cgiProc->outputFd); // Cleanup all
        return;
    }

    char readBuffer[65536];
    cgiProc->bodyFile->read(readBuffer, sizeof(readBuffer));
    ssize_t bytesRead = cgiProc->bodyFile->gcount();
    
    if (bytesRead > 0) {
        ssize_t written = write(cgiInputFd, readBuffer, bytesRead);
        
        if (written <= 0) {
            // Error or pipe closed - poll() said it was ready but write failed
            Utils::logError("CGI write failed");
            cleanupCgiProcess(cgiProc->outputFd);
            return;
        }
        
        if (written < bytesRead) {
            // Partially wrote. Rewind file to re-read the unwritten part.
            // This can happen if the pipe buffer is smaller than our read buffer
            cgiProc->bodyFile->seekg(written - bytesRead, std::ios::cur);
            return; // Wait for next POLLOUT
        }
    }
    
    // If we read 0 bytes, or we're at EOF, we are done writing.
    if (bytesRead == 0 || cgiProc->bodyFile->eof()) {
        // Done writing. Close file, pipe, and remove from poll.
        cgiProc->bodyFile->close();
        delete cgiProc->bodyFile;
        cgiProc->bodyFile = NULL;

        close(cgiInputFd);
        _cgiWritePipes.erase(it); // Erase from map
        
        // Remove from _pollFds
        for (std::vector<struct pollfd>::iterator pollIt = _pollFds.begin(); pollIt != _pollFds.end(); ++pollIt) {
            if (pollIt->fd == cgiInputFd) {
                _pollFds.erase(pollIt);
                break;
            }
        }
    }
}

void Server::checkClientTimeouts() {
    time_t currentTime = time(NULL);
    
    // Iterate safely, as removeClient will modify _clients
    std::map<int, Client>::iterator it = _clients.begin();
    while (it != _clients.end()) {
        int clientFd = it->first;
        Client& client = it->second;
        
        // Get the specific config for this client's server
        ServerConfig config = getServerConfig(clientFd);
        int timeout = config.keepAliveTimeout; 
        
        if (difftime(currentTime, client.getLastActivity()) > timeout) {
            Utils::logInfo("Client " + Utils::intToString(clientFd) + 
                          " timed out (idle for " + Utils::intToString(timeout) + 
                          "s). Disconnecting.");
            
            // Get iterator to next element *before* erasing
            std::map<int, Client>::iterator toRemove = it;
            ++it;
            removeClient(toRemove->first); // Just remove it.
        } else {
            ++it; // No timeout, check next client
        }
    }
}

void Server::checkCgiTimeouts() {
    time_t currentTime = time(NULL);
    
    std::map<int, CgiProcess>::iterator it = _cgiProcesses.begin();
    while (it != _cgiProcesses.end()) {
        // int cgiOutputFd = it->first; // This line is removed
        CgiProcess& cgiProc = it->second;
        
        int cgiTimeout = cgiProc.serverConfig.cgiTimeout;
        
        if (difftime(currentTime, cgiProc.startTime) > cgiTimeout) {
            Utils::logError("CGI process (pid " + Utils::intToString(cgiProc.pid) + 
                           ") for client " + Utils::intToString(cgiProc.clientFd) + 
                           " timed out (" + Utils::intToString(cgiTimeout) + "s). Killing.");
                           
            // 1. Kill the hanging CGI process
            kill(cgiProc.pid, SIGKILL);
            waitpid(cgiProc.pid, NULL, 0); // Reap the zombie
            
            // 2. Send 504 Gateway Timeout to the client
            HttpResponse response = createErrorResponse(504, cgiProc.serverConfig);
            response.setHeader("Connection", "close");
            queueResponse(cgiProc.clientFd, response);

            // 3. Mark client for close (if it still exists)
            if (_clients.count(cgiProc.clientFd)) {
                _clients[cgiProc.clientFd].markForCloseAfterWrite();
            }
            
            // 4. Clean up CGI resources (this erases `it` from _cgiProcesses)
            // We must get the next iterator *before* calling this.
            std::map<int, CgiProcess>::iterator toRemove = it;
            ++it;
            cleanupCgiProcess(toRemove->first); // Uses the key from the iterator directly
        } else {
            ++it; // No timeout, advance iterator
        }
    }
}
