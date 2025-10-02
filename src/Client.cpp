// src/Client.cpp
#include "../include/Client.hpp"
#include "../include/Utils.hpp"

#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

Client::Client() : _fd(-1), _requestComplete(false), _lastActivity(time(NULL)) {
}

Client::Client(int fd) : _fd(fd), _requestComplete(false), _lastActivity(time(NULL)) {
}

Client::~Client() {
}

bool Client::readData() {
    char buffer[BUFFER_SIZE];
    ssize_t bytesRead = recv(_fd, buffer, BUFFER_SIZE - 1, 0);

    if (bytesRead < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return true;
        }
        Utils::logError("Failed to read from client: " + std::string(strerror(errno)));
        return false;
    } else if (bytesRead == 0) {
        // client closed connection
        Utils::logInfo("Client closed connection");
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



static std::string get_line(const std::string& s, size_t start, size_t& lineEndPos) {
    // returns line contents without line break; sets lineEndPos to index after the line break sequence
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
    // First, find header end (\r\n\r\n or \n\n)
    size_t headerEndPos = _buffer.find("\r\n\r\n");
    if (headerEndPos == std::string::npos) {
        headerEndPos = _buffer.find("\n\n");
        if (headerEndPos == std::string::npos) {
            return false; // headers not complete yet
        }
        headerEndPos += 2; // position after the double newline
    } else {
        headerEndPos += 4; // position after \r\n\r\n
    }

    // Extract headers substring
    std::string headers = _buffer.substr(0, headerEndPos);

    // If Expect: 100-continue header present, send 100 Continue immediately
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
                // If client expects 100-continue, send it now
                if (Utils::toLower(val).find("100-continue") != std::string::npos) {
                    const char *continue_msg = "HTTP/1.1 100 Continue\r\n\r\n";
                    // best-effort send; ignore errors (the client may have closed)
                    ssize_t s = send(_fd, continue_msg, strlen(continue_msg), 0);
                    (void)s;
                }
            }
        }
    }

    // Try parse Content-Length header (if present)
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

    // Detect Transfer-Encoding: chunked
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

    // If chunked: parse chunks fully to determine end-of-body position
    if (isChunked) {
        size_t cur = headerEndPos;
        // iterate over chunks
        while (true) {
            // find chunk-size line
            size_t lineEndPos;
            std::string sizeLine = get_line(_buffer, cur, lineEndPos);
            if (lineEndPos == std::string::npos) {
                return false; // chunk-size line not fully received yet
            }
            // trim and parse hex number
            sizeLine = Utils::trim(sizeLine);
            if (sizeLine.empty()) {
                // abnormal; wait for more
                return false;
            }

            // Allow chunk-size to have extensions: "HEX[;ext]" -> take until ';'
            size_t semi = sizeLine.find(';');
            std::string hexStr = (semi == std::string::npos) ? sizeLine : sizeLine.substr(0, semi);
            // parse hex
            char *endptr = NULL;
            unsigned long chunkSize = strtoul(hexStr.c_str(), &endptr, 16);
            if (endptr == hexStr.c_str()) {
                // invalid hex => treat as malformed; we'll wait or eventually error upstream
                return false;
            }

            // move cur to start of chunk data
            cur = lineEndPos;

            // check if full chunk data + following CRLF is present
            // chunk data length = chunkSize bytes
            // after chunk data should be CRLF or LF
            size_t need = cur + chunkSize;
            if (_buffer.size() < need) {
                return false; // chunk data not fully received yet
            }
            // after data, expect CRLF or LF
            if (_buffer.size() < need + 1) {
                return false;
            }
            // if there is CRLF
            if (need + 1 < _buffer.size() && _buffer[need] == '\r' && _buffer[need + 1] == '\n') {
                cur = need + 2;
            } else if (_buffer[need] == '\n') {
                cur = need + 1;
            } else {
                // not enough yet for line ending, wait
                return false;
            }

            if (chunkSize == 0) {
                // reached last chunk; after this may follow trailer headers ending with CRLFCRLF
                // find end of trailers: look for "\r\n\r\n" or "\n\n" starting at cur
                size_t trailersEnd = _buffer.find("\r\n\r\n", cur);
                if (trailersEnd != std::string::npos) {
                    trailersEnd += 4;
                    // we have entire request
                    _request = _buffer.substr(0, trailersEnd);
                    _buffer.erase(0, trailersEnd);
                    return true;
                }
                trailersEnd = _buffer.find("\n\n", cur);
                if (trailersEnd != std::string::npos) {
                    trailersEnd += 2;
                    _request = _buffer.substr(0, trailersEnd);
                    _buffer.erase(0, trailersEnd);
                    return true;
                }
                // trailers not fully received yet; wait
                return false;
            }
            // else continue loop to parse next chunk
        }
    } else {
        // Not chunked: rely on Content-Length (could be zero)
        size_t totalRequired = headerEndPos + contentLength;
        if (_buffer.length() < totalRequired) {
            return false; // body not complete yet
        }

        _request = _buffer.substr(0, totalRequired);
        _buffer.erase(0, totalRequired);
        return true;
    }
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
