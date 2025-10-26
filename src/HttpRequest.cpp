#include "../include/HttpRequest.hpp"
#include "../include/Utils.hpp"

HttpRequest::HttpRequest() : _isValid(false) {
}

HttpRequest::HttpRequest(const std::string& headers, const std::string& bodyFilePath) 
    : _bodyFilePath(bodyFilePath), _isValid(false) 
{
    parse(headers, bodyFilePath);
}

HttpRequest::~HttpRequest() {
}

bool HttpRequest::parse(const std::string& headers, const std::string& bodyFilePath) {
    _isValid = false; // Start as invalid
    if (headers.empty()) {
        return false;
    }

	size_t reqLineStart = headers.find_first_not_of(" \t\r\n");
        if (reqLineStart == std::string::npos) {
            Utils::logError("Headers block contains only whitespace.");
            return false; // Only whitespace found
        }

	// Find the end of the *actual* request line, searching from reqLineStart
    size_t firstLineEnd = headers.find("\r\n", reqLineStart);
    size_t firstLineEndLen = 2;
    if (firstLineEnd == std::string::npos) {
        firstLineEnd = headers.find("\n", reqLineStart);
        if (firstLineEnd == std::string::npos) {
            Utils::logError("Could not find end of first request line after position " + Utils::sizeToString(reqLineStart));
            return false; // No line ending found after the start
        }
        firstLineEndLen = 1;
    }

    // Extract the request line using the correct start and end
    std::string firstLineRaw = headers.substr(reqLineStart, firstLineEnd - reqLineStart);
    // Utils::logInfo("HttpRequest received raw first line: [" + firstLineRaw + "]");

    parseRequestLine(Utils::trim(firstLineRaw));
    if (_method.empty()) {
        Utils::logError("Request line parsing failed.");
        return false;
    }

    // Adjust headerStart to be after the first line's actual ending
    size_t headerStart = firstLineEnd + firstLineEndLen;

    std::vector<std::string> headerLines;
    size_t currentPos = headerStart;

    while (currentPos < headers.length()) {
        size_t nextLineEnd = headers.find("\r\n", currentPos);
        size_t nextLineEndLen = 2;
        if (nextLineEnd == std::string::npos) {
            nextLineEnd = headers.find("\n", currentPos);
            if (nextLineEnd != std::string::npos) {
                nextLineEndLen = 1;
            } else {
                // This means we reached the end of the headers string without finding \r\n\r\n or \n\n
                // which shouldn't happen if Client::parseHeadersFromBuffer worked.
                Utils::logError("Malformed headers: No final empty line found.");
                break; 
            }
        }

        // Extract the line content *before* the line ending
        std::string line = Utils::trim(headers.substr(currentPos, nextLineEnd - currentPos));

        if (line.empty()) {
            // Found the empty line separating headers from body
            break;
        }

        headerLines.push_back(line);
        currentPos = nextLineEnd + nextLineEndLen;
    }

    parseHeaders(headerLines);

    _bodyFilePath = bodyFilePath;
    parseQueryString(_uri); // Make sure this happens after getting _uri

    // Final validity check
    _isValid = !_method.empty() && !_uri.empty() && !_version.empty();
    if (!_isValid) {
        Utils::logError("Request deemed invalid after parsing.");
    }
    return _isValid;
}

void HttpRequest::parseRequestLine(const std::string& line) {
    std::vector<std::string> parts = Utils::split(line, ' ');
    // Filter out empty strings that might result from multiple spaces
    std::vector<std::string> validParts;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (!parts[i].empty()) {
            validParts.push_back(parts[i]);
        }
    }

    if (validParts.size() >= 3) {
        _method = Utils::toUpper(validParts[0]);
        _uri = validParts[1];
        _version = validParts[2]; // Take the first 3 valid parts
    } else {
        // Explicitly clear fields if parsing fails
        _method.clear();
        _uri.clear();
        _version.clear();
        Utils::logError("Failed to parse request line: '" + line + "'"); // Add logging
    }
}

void HttpRequest::parseHeaders(const std::vector<std::string>& headerLines) {
    for (size_t i = 0; i < headerLines.size(); ++i) {
        const std::string& line = headerLines[i];
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key = Utils::trim(line.substr(0, colonPos));
            std::string value = Utils::trim(line.substr(colonPos + 1));
            _headers[Utils::toLower(key)] = value;
        }
    }
}

void HttpRequest::parseQueryString(const std::string& uri) {
    size_t queryPos = uri.find('?');
    if (queryPos != std::string::npos) {
        std::string queryString = uri.substr(queryPos + 1);
        std::vector<std::string> params = Utils::split(queryString, '&');
        
        for (size_t i = 0; i < params.size(); ++i) {
            const std::string& param = params[i];
            size_t equalPos = param.find('=');
            if (equalPos != std::string::npos) {
                std::string key = Utils::urlDecode(param.substr(0, equalPos));
                std::string value = Utils::urlDecode(param.substr(equalPos + 1));
                _queryParams[key] = value;
            }
        }
        
        // Remove query string from URI
        _uri = uri.substr(0, queryPos);
    }
}

const std::string& HttpRequest::getMethod() const {
    return _method;
}

const std::string& HttpRequest::getUri() const {
    return _uri;
}

const std::string& HttpRequest::getVersion() const {
    return _version;
}

const std::string& HttpRequest::getBodyFilePath() const {
	return _bodyFilePath;
}

const std::map<std::string, std::string>& HttpRequest::getHeaders() const {
    return _headers;
}

const std::map<std::string, std::string>& HttpRequest::getQueryParams() const {
    return _queryParams;
}

std::string HttpRequest::getHeader(const std::string& key) const {
    std::string lowerKey = Utils::toLower(key);
    std::map<std::string, std::string>::const_iterator it = _headers.find(lowerKey);
    return (it != _headers.end()) ? it->second : "";
}

bool HttpRequest::isValid() const {
    return _isValid;
}

std::string HttpRequest::getPath() const {
    return _uri;
}

bool HttpRequest::hasHeader(const std::string& key) const {
    return _headers.find(Utils::toLower(key)) != _headers.end();
}

size_t HttpRequest::getContentLength() const {
    std::string contentLength = getHeader("content-length");
    return contentLength.empty() ? 0 : Utils::stringToInt(contentLength);
}
