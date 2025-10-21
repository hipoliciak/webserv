// src/Client.cpp
#include "../include/Client.hpp"
#include "../include/Utils.hpp"

#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <climits>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

Client::Client() : _fd(-1), _requestComplete(false), _lastActivity(time(NULL)), _stopReading(false), _lastBufferSize(0) {}

Client::Client(int fd) : _fd(fd), _requestComplete(false), _lastActivity(time(NULL)), _stopReading(false), _lastBufferSize(0) {}

Client::~Client() {
}

bool Client::readData() {
    if (_stopReading) {
        return true;
    }
    
    char buffer[BUFFER_SIZE];
    ssize_t bytesRead = recv(_fd, buffer, BUFFER_SIZE - 1, 0);

    if (bytesRead < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No data available right now for non-blocking socket
            return true;
        }
        // Real error occurred
        return false;
    } else if (bytesRead == 0) {
        // Connection closed by client
        return false;
    }

    buffer[bytesRead] = '\0';
    _buffer.append(buffer, bytesRead);
    updateActivity();

    if (!_requestComplete) {
        _requestComplete = parseRequest();
    }

    return true;
}

void Client::appendToBuffer(const std::string& data) {
    _buffer += data;
    updateActivity();
}



static std::string get_line(const std::string& s, size_t start, size_t& lineEndPos) {
    size_t rn = s.find("\r\n", start);
    if (rn != std::string::npos) {
        lineEndPos = rn + 2;
        return s.substr(start, rn - start);
    }
    size_t n = s.find("\n", start);
    if (n != std::string::npos) {
        lineEndPos = n + 1;
        return s.substr(start, n - start);
    }
    lineEndPos = std::string::npos;
    return std::string();
}

bool Client::parseRequest() {
    // If no new data since last parse, don't waste CPU
    if (_buffer.size() == _lastBufferSize) {
        return false;
    }
    _lastBufferSize = _buffer.size();
    
    size_t headerEndPos = _buffer.find("\r\n\r\n");
    if (headerEndPos == std::string::npos) {
        headerEndPos = _buffer.find("\n\n");
        if (headerEndPos == std::string::npos) {
            return false;
        }
        headerEndPos += 2;
    } else {
        headerEndPos += 4;
    }

    std::string headers = _buffer.substr(0, headerEndPos);
    
    size_t methodEnd = headers.find(' ');
    std::string method = (methodEnd != std::string::npos) ? headers.substr(0, methodEnd) : "UNKNOWN";

    size_t expectPos = headers.find("Expect:");
    if (expectPos == std::string::npos) {
        expectPos = headers.find("expect:");
    }
    if (expectPos != std::string::npos) {
        size_t lineEnd = headers.find("\r\n", expectPos);
        if (lineEnd == std::string::npos) lineEnd = headers.find("\n", expectPos);
        if (lineEnd != std::string::npos) {
            size_t valStart = headers.find(":", expectPos);
            if (valStart != std::string::npos && valStart + 1 < lineEnd) {
                std::string val = headers.substr(valStart + 1, lineEnd - (valStart + 1));
                val = Utils::trim(val);
                if (Utils::toLower(val).find("100-continue") != std::string::npos) {
                    const char *continue_msg = "HTTP/1.1 100 Continue\r\n\r\n";
                    ssize_t s = send(_fd, continue_msg, strlen(continue_msg), 0);
                    if (s < 0) {
                        if (errno != EAGAIN && errno != EWOULDBLOCK) {
                            // Real error occurred, but continue processing
                            Utils::logError("Failed to send 100 Continue response");
                        }
                    }
                }
            }
        }
    }

    size_t contentLength = 0;
    size_t clPos = headers.find("Content-Length:");
    if (clPos == std::string::npos) clPos = headers.find("content-length:");
    if (clPos != std::string::npos) {
        size_t lineEnd = headers.find("\r\n", clPos);
        if (lineEnd == std::string::npos) lineEnd = headers.find("\n", clPos);
        if (lineEnd != std::string::npos) {
            size_t valueStart = headers.find(":", clPos);
            if (valueStart != std::string::npos) {
                std::string lenStr = headers.substr(valueStart + 1, lineEnd - (valueStart + 1));
                lenStr = Utils::trim(lenStr);
                contentLength = Utils::stringToInt(lenStr);
            }
        }
    }

    bool isChunked = false;
    size_t tePos = headers.find("Transfer-Encoding:");
    if (tePos == std::string::npos) tePos = headers.find("transfer-encoding:");
    if (tePos != std::string::npos) {
        size_t lineEnd = headers.find("\r\n", tePos);
        if (lineEnd == std::string::npos) lineEnd = headers.find("\n", tePos);
        if (lineEnd != std::string::npos) {
            size_t valueStart = headers.find(":", tePos);
            if (valueStart != std::string::npos) {
                std::string teVal = headers.substr(valueStart + 1, lineEnd - (valueStart + 1));
                teVal = Utils::trim(teVal);
                std::string teValLow = Utils::toLower(teVal);
                if (teValLow.find("chunked") != std::string::npos) {
                    isChunked = true;
                }
            }
        }
    }

    if (isChunked) {
        size_t cur = headerEndPos;
        size_t maxChunks = 100000; // Increased for large requests
        size_t chunkCount = 0;
        
        while (true) {
            // Safety check to prevent infinite loops
            if (++chunkCount > maxChunks) {
                Utils::logError("Too many chunks in request (" + Utils::sizeToString(chunkCount) + "), possible large file or attack");
                return false;
            }
            
            if (chunkCount % 10000 == 0) {
                Utils::logInfo("Processing chunk " + Utils::sizeToString(chunkCount) + ", buffer size: " + Utils::sizeToString(_buffer.size()));
            }
            
            size_t oldCur = cur;
            size_t lineEndPos;
            std::string sizeLine = get_line(_buffer, cur, lineEndPos);
            if (lineEndPos == std::string::npos) {
                return false;
            }
            
            // Check if we actually made progress
            if (lineEndPos == oldCur) {
                Utils::logError("No progress in chunk parsing, breaking to avoid infinite loop");
                return false;
            }
            sizeLine = Utils::trim(sizeLine);
            if (sizeLine.empty()) {
                return false;
            }

            size_t semi = sizeLine.find(';');
            std::string hexStr = (semi == std::string::npos) ? sizeLine : sizeLine.substr(0, semi);
            char *endptr = NULL;
            unsigned long chunkSize = strtoul(hexStr.c_str(), &endptr, 16);
            if (endptr == hexStr.c_str()) {
                return false;
            }
            
            // if (chunkCount == 1 || chunkCount % 10000 == 0) {
            //     Utils::logInfo("Chunk " + Utils::sizeToString(chunkCount) + " size: " + Utils::sizeToString(chunkSize) + " bytes");
            // }
            
            cur = lineEndPos;

            if (chunkSize == 0) {
                std::string remainingAfterZero = _buffer.substr(cur);
                
                if (_buffer.size() >= cur + 2 && _buffer.substr(cur, 2) == "\r\n") {
                    size_t trailersEnd = cur + 2;
                    _request = _buffer.substr(0, trailersEnd);
                    _buffer.erase(0, trailersEnd);
                    return true;
                } else if (_buffer.size() >= cur + 1 && _buffer[cur] == '\n') {
                    size_t trailersEnd = cur + 1;
                    _request = _buffer.substr(0, trailersEnd);
                    _buffer.erase(0, trailersEnd);
                    return true;
                }
                
                return false;
            }

            size_t need = cur + chunkSize;
            if (_buffer.size() < need) {
                // Not enough data yet, need to wait for more
                return false;
            }
            if (_buffer.size() < need + 2) {
                // Need at least 2 more bytes for CRLF
                return false;
            }
            if (need + 1 < _buffer.size() && _buffer[need] == '\r' && _buffer[need + 1] == '\n') {
                cur = need + 2;
            } else if (_buffer[need] == '\n') {
                cur = need + 1;
            } else {
                return false;
            }
        }
    } else {
        if (method == "GET" && contentLength == 0) {
            _request = _buffer.substr(0, headerEndPos);
            _buffer.erase(0, headerEndPos);
            return true;
        }
        
        if ((method == "POST" || method == "PUT" || method == "PATCH") && 
            contentLength == 0 && 
            headers.find("Content-Length:") == std::string::npos && 
            headers.find("content-length:") == std::string::npos) {
            _request = _buffer.substr(0, headerEndPos);
            _buffer.erase(0, headerEndPos);
            return true;
        }
        
        size_t totalRequired = headerEndPos + contentLength;
        
        if (_buffer.length() < totalRequired) {
            return false;
        }

        _request = _buffer.substr(0, totalRequired);
        _buffer.erase(0, totalRequired);
        return true;
    }
}


void Client::clearRequest() {
    _request.clear();
    _requestComplete = false;
    _lastBufferSize = 0;
}

int Client::getFd() const {
    return _fd;
}

const std::string& Client::getRequest() const {
    return _request;
}


bool Client::isRequestComplete() const {
    return _requestComplete;
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

void Client::stopReading() {
    _stopReading = true;
}

bool Client::shouldStopReading() const {
    return _stopReading;
}

bool Client::areHeadersComplete() const {
    size_t headerEndPos = _buffer.find("\r\n\r\n");
    if (headerEndPos == std::string::npos) {
        headerEndPos = _buffer.find("\n\n");
    }
    return headerEndPos != std::string::npos;
}

std::string Client::getHeaders() const {
    size_t headerEndPos = _buffer.find("\r\n\r\n");
    if (headerEndPos == std::string::npos) {
        headerEndPos = _buffer.find("\n\n");
        if (headerEndPos != std::string::npos) {
            return _buffer.substr(0, headerEndPos + 2);
        }
    } else {
        return _buffer.substr(0, headerEndPos + 4);
    }
    return "";
}
