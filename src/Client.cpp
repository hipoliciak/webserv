// src/Client.cpp
#include "../include/Client.hpp"
#include "../include/Utils.hpp"

#include <cstdlib>
#include <cstring>
#include <climits>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sstream>
#include <limits>

Client::Client() : _fd(-1), _state(STATE_READING_HEADERS), _bodyFile(NULL), 
                   _contentLength(0), _maxBodySize(0), _bodyBytesReceived(0), _currentChunkSize(0),
                   _isChunked(false), _requestComplete(false), _closeConnectionAfterWrite(false) {}

Client::Client(int fd) : _fd(fd), _lastActivity(time(NULL)), _stopReading(false),
                         _state(STATE_READING_HEADERS), _bodyFile(NULL),
                         _contentLength(0), _maxBodySize(0), _bodyBytesReceived(0), _currentChunkSize(0),
                         _isChunked(false), _requestComplete(false), _closeConnectionAfterWrite(false) {}

Client::~Client() {
    clearRequest();
}

std::string Client::createTempFile() {
    static int counter = 0;
    std::ostringstream oss;
    oss << "/tmp/webserv_body_" << getpid() << "_" << time(NULL) << "_" << (++counter);
    return oss.str();
}

bool Client::openBodyFile() {
    if (_bodyFile) return true;

    _bodyFilePath = createTempFile();
    _bodyFile = new std::ofstream(_bodyFilePath.c_str(), std::ios::binary);
    if (!_bodyFile->is_open()) {
        Utils::logError("Failed to create temporary body file: " + _bodyFilePath);
        delete _bodyFile;
        _bodyFile = NULL;
        return false;
    }
    Utils::logInfo("Streaming request body to temp file: " + _bodyFilePath);
    return true;
}

bool Client::readData() {
    if (_stopReading || _state == STATE_REQUEST_COMPLETE) {
        return true;
    }
    
    char buffer[BUFFER_SIZE];
    ssize_t bytesRead = recv(_fd, buffer, BUFFER_SIZE - 1, 0);

    if (bytesRead > 0) {
        // Successfully read new data
        buffer[bytesRead] = '\0';
        _buffer.append(buffer, bytesRead);
        updateActivity();

        if (!_requestComplete) {
            _requestComplete = parseRequest();
        }

        return true;
    }
    
    // bytesRead <= 0: Either no data available or connection closed
    // Before giving up, try to parse what's already in the buffer
    // This handles the case where all data arrived in one packet
    if (!_requestComplete && !_buffer.empty()) {
        _requestComplete = parseRequest();
        // If request is now complete, return success
        if (_requestComplete) {
            return true;
        }
    }
    
    // If bytesRead == 0, connection was closed by peer
    // If bytesRead < 0, it's an error (likely EAGAIN if non-blocking, but we can't check)
    // Since poll() indicated ready, bytesRead < 0 shouldn't happen unless there's an error
    // Return false to close the connection
    return (bytesRead == 0) ? false : true;
}

bool Client::parseHeadersFromBuffer() {
	size_t startPos = _buffer.find_first_not_of(" \t\r\n");
    if (startPos == std::string::npos) {
        // Buffer contains only whitespace
        return false; 
    }

	size_t headerEndPos = _buffer.find("\r\n\r\n");

    size_t headerEndLen = 4;
    if (headerEndPos == std::string::npos) {
        headerEndPos = _buffer.find("\n\n");
        if (headerEndPos == std::string::npos) {
            return false; // Headers not complete yet
        }
        headerEndLen = 2;
    }
    headerEndPos += headerEndLen;
    _headers = _buffer.substr(0, headerEndPos);

	// Utils::logInfo("Client extracted headers block:\n--- START ---\n" + _headers + "--- END ---");
    
    size_t expectPos = _headers.find("Expect:");
    if (expectPos == std::string::npos) expectPos = _headers.find("expect:");
    if (expectPos != std::string::npos) {
        if (_headers.find("100-continue", expectPos) != std::string::npos) {
            const char *continue_msg = "HTTP/1.1 100 Continue\r\n\r\n";
            send(_fd, continue_msg, strlen(continue_msg), 0);
        }
    }

    size_t clPos = _headers.find("Content-Length:");
    if (clPos == std::string::npos) clPos = _headers.find("content-length:");
    if (clPos != std::string::npos) {
        size_t lineEnd = _headers.find("\r\n", clPos);
        if (lineEnd != std::string::npos) {
            size_t valueStart = _headers.find(":", clPos);
            if (valueStart != std::string::npos) {
                std::string lenStr = _headers.substr(valueStart + 1, lineEnd - (valueStart + 1));
                _contentLength = Utils::stringToInt(Utils::trim(lenStr));
            }
        }
    }

    size_t tePos = _headers.find("Transfer-Encoding:");
    if (tePos == std::string::npos) tePos = _headers.find("transfer-encoding:");
    if (tePos != std::string::npos) {
        if (_headers.find("chunked", tePos) != std::string::npos) {
            _isChunked = true;
        }
    }

    // Remove headers from buffer, keeping the first part of the body
    _buffer.erase(0, headerEndPos);

    // --- Transition to next state ---
	if (_contentLength > 0 || _isChunked) {
        if (!openBodyFile()) return false; // Could not open file
        _state = STATE_HEADERS_COMPLETE;
    } else {
        // No body, request is complete
        _state = STATE_REQUEST_COMPLETE;
		_requestComplete = true;
        return true; 
    }

    return false; // Not complete yet, but headers are parsed
}

bool Client::handleBodyRead() {
    if (_buffer.empty()) return false;

	size_t bytesToReceive = _buffer.length();
    if (_bodyBytesReceived + bytesToReceive > _maxBodySize && _maxBodySize > 0) {
        Utils::logError("Body size exceeds limit. Stopping read.");
        _stopReading = true; // Stop reading from socket
        _bodyFile->close();
        _state = STATE_REQUEST_COMPLETE;
		_requestComplete = true;
        return true; // Mark as "complete" to trigger 413 in server
    }

    _bodyFile->write(_buffer.c_str(), _buffer.length());
    _bodyBytesReceived += _buffer.length();
    _buffer.clear();

    if (_bodyBytesReceived >= _contentLength) {
        _bodyFile->close();
        _state = STATE_REQUEST_COMPLETE;
		_requestComplete = true;
        return true;
    }
    return false;
}

bool Client::handleChunkRead() {
    while (true) {
        if (_state == STATE_READING_CHUNK_SIZE) {
            size_t lineEnd = _buffer.find("\r\n");
            if (lineEnd == std::string::npos) return false; // Need more data

            std::string sizeLine = _buffer.substr(0, lineEnd);
            _buffer.erase(0, lineEnd + 2); // Erase size + \r\n

            _currentChunkSize = strtoul(sizeLine.c_str(), NULL, 16);

            if (_currentChunkSize == 0) {
                // End of chunks
                _bodyFile->close();
                _state = STATE_REQUEST_COMPLETE;
				_requestComplete = true;
                if (_buffer.rfind("\r\n", 0) == 0) {
                    _buffer.erase(0, 2);
                }
                return true;
            } else {
                _state = STATE_READING_CHUNK_DATA;
				if (_bodyBytesReceived + _currentChunkSize > _maxBodySize && _maxBodySize > 0) {
					Utils::logError("Chunked body size will exceed limit. Stopping read.");
					_stopReading = true;
					_bodyFile->close();
					_state = STATE_REQUEST_COMPLETE;
					_requestComplete = true;
					return true; // Mark as "complete" to trigger 413
				}
            }
        }

        if (_state == STATE_READING_CHUNK_DATA) {
            if (_buffer.empty()) return false; // Need more data

            size_t bytesToWrite = std::min(_buffer.length(), _currentChunkSize);
            _bodyFile->write(_buffer.c_str(), bytesToWrite);
            _buffer.erase(0, bytesToWrite);
            _currentChunkSize -= bytesToWrite;

            if (_currentChunkSize == 0) {
                // Chunk is done, expect \r\n
                if (_buffer.rfind("\r\n", 0) == 0) {
                    _buffer.erase(0, 2);
                    _state = STATE_READING_CHUNK_SIZE;
                    // Loop again to read next chunk size
                } else {
                     // Need to wait for the \r\n
                    return false;
                }
            } else {
                // Need more data for this chunk
                return false;
            }
        }
    }
}

bool Client::parseRequest() {
    bool stateChanged = true;

    // Loop to process data in buffer immediately
    while (stateChanged) {
        stateChanged = false;

        switch (_state) {
            case STATE_READING_HEADERS:
                if (parseHeadersFromBuffer()) {
                    return true; // Complete (no body)
                }
                // If headers were just parsed, _state changed
                if (_state != STATE_READING_HEADERS) {
                    stateChanged = true;
                }
                break;

			case STATE_HEADERS_COMPLETE:
				// Do nothing, wait for server to call beginReadingBody()
				return false;
			
            case STATE_READING_BODY:
                if (handleBodyRead()) {
                    return true; // Complete
                }
                break;

            case STATE_READING_CHUNK_SIZE:
            case STATE_READING_CHUNK_DATA:
                if (handleChunkRead()) {
                    return true; // Complete
                }
                break;

            case STATE_REQUEST_COMPLETE:
                return true;
        }
    }
    return false; // Not complete, need more data
}

void Client::clearRequest() {
    _buffer.clear();
    _headers.clear();
    _requestComplete = false;
    
    if (_bodyFile) {
        if (_bodyFile->is_open()) {
            _bodyFile->close();
        }
        delete _bodyFile;
        _bodyFile = NULL;
    }
    // Don't delete the temp file here - Server owns it after processHttpRequest is called
    // The Server will clean it up via cleanupTempFile()
    if (!_bodyFilePath.empty()) {
        _bodyFilePath.clear();
    }

    _state = STATE_READING_HEADERS;
    _contentLength = 0;
    _bodyBytesReceived = 0;
    _currentChunkSize = 0;
    _isChunked = false;
}

int Client::getFd() const {
	return _fd;
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

bool Client::isRequestComplete() const {
	return _requestComplete;
}

// Returns HEADERS ONLY
const std::string& Client::getRequest() const {
    return _headers;
}

// Returns path to body temp file
const std::string& Client::getBodyFilePath() const {
    return _bodyFilePath;
}

bool Client::areHeadersComplete() const {
    return _state != STATE_READING_HEADERS;
}

size_t Client::getContentLength() const {
    return _contentLength;
}

bool Client::isChunked() const {
    return _isChunked;
}

void Client::beginReadingBody(size_t maxBodySize) {
    if (_state != STATE_HEADERS_COMPLETE) {
        return;
    }

    _maxBodySize = (maxBodySize > 0) ? maxBodySize : std::numeric_limits<size_t>::max();

    // Server should have already checked this, but as a safety.
    if (_contentLength > 0 && _contentLength > _maxBodySize) {
        Utils::logError("Content-Length " + Utils::sizeToString(_contentLength) + 
                       " exceeds limit " + Utils::sizeToString(_maxBodySize));
        _stopReading = true;
        _bodyFile->close();
        _state = STATE_REQUEST_COMPLETE;
		_requestComplete = true;
        return;
    }

    if (_isChunked) {
        _state = STATE_READING_CHUNK_SIZE;
    } else if (_contentLength > 0) {
        _state = STATE_READING_BODY;
    } else {
        // No body, but headers were parsed.
        _state = STATE_REQUEST_COMPLETE;
		_requestComplete = true;
    }
}

Client::ClientState Client::getState() const {
    return _state;
}

void Client::markForCloseAfterWrite() {
    _closeConnectionAfterWrite = true;
}

bool Client::shouldCloseAfterWrite() const {
    return _closeConnectionAfterWrite;
}
