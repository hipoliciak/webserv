#include "../include/Client.hpp"
#include "../include/Utils.hpp"

Client::Client() : _fd(-1), _requestComplete(false), _lastActivity(time(NULL)) {
}

Client::Client(int fd) : _fd(fd), _requestComplete(false), _lastActivity(time(NULL)) {
}

Client::~Client() {
}

bool Client::readData() {
    char buffer[BUFFER_SIZE];
    ssize_t bytesRead = recv(_fd, buffer, BUFFER_SIZE - 1, 0);
    
    if (bytesRead <= 0) {
        if (bytesRead == 0) {
            Utils::logInfo("Client closed connection");
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            Utils::logError("Failed to read from client: " + std::string(strerror(errno)));
        }
        return false;
    }
    
    buffer[bytesRead] = '\0';
    _buffer.append(buffer, bytesRead);
    updateActivity();
    
    // Check if request is complete
    if (!_requestComplete) {
        _requestComplete = parseRequest();
    }
    
    return true;
}

void Client::appendToBuffer(const std::string& data) {
    _buffer += data;
    updateActivity();
}

bool Client::parseRequest() {
    // First check for headers end
    size_t headerEndPos = _buffer.find("\r\n\r\n");
    if (headerEndPos == std::string::npos) {
        headerEndPos = _buffer.find("\n\n");
        if (headerEndPos == std::string::npos) {
            return false; // Headers not complete yet
        }
        headerEndPos += 2;
    } else {
        headerEndPos += 4;
    }
    
    // Extract headers part
    std::string headers = _buffer.substr(0, headerEndPos);
    
    // Parse Content-Length from headers
    size_t contentLength = 0;
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
            size_t valueStart = headers.find(":", clPos) + 1;
            std::string lengthStr = headers.substr(valueStart, lineEnd - valueStart);
            lengthStr = Utils::trim(lengthStr);
            contentLength = Utils::stringToInt(lengthStr);
        }
    }
    
    // Check if we have complete request (headers + body)
    size_t totalRequired = headerEndPos + contentLength;
    if (_buffer.length() < totalRequired) {
        return false; // Body not complete yet
    }
    
    _request = _buffer.substr(0, totalRequired);
    _buffer.erase(0, totalRequired);
    
    return true;
}

size_t Client::findRequestEnd() const {
    return _buffer.find("\r\n\r\n");
}

bool Client::isRequestComplete() const {
    return _requestComplete;
}

void Client::clearRequest() {
    _request.clear();
    _requestComplete = false;
}

int Client::getFd() const {
    return _fd;
}

const std::string& Client::getRequest() const {
    return _request;
}

const std::string& Client::getBuffer() const {
    return _buffer;
}

time_t Client::getLastActivity() const {
    return _lastActivity;
}

void Client::updateActivity() {
    _lastActivity = time(NULL);
}