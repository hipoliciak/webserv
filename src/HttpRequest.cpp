#include "../include/HttpRequest.hpp"
#include "../include/Utils.hpp"

HttpRequest::HttpRequest() : _isValid(false) {
}

HttpRequest::HttpRequest(const std::string& rawRequest) : _isValid(false) {
    parse(rawRequest);
}

HttpRequest::~HttpRequest() {
}

bool HttpRequest::parse(const std::string& rawRequest) {
    if (rawRequest.empty()) {
        return false;
    }
    
    std::vector<std::string> lines = Utils::split(rawRequest, '\n');
    if (lines.empty()) {
        return false;
    }
    
    // Parse request line (first line)
    parseRequestLine(Utils::trim(lines[0]));
    
    // Parse headers
    std::vector<std::string> headerLines;
    size_t i = 1;
    for (; i < lines.size(); ++i) {
        std::string line = Utils::trim(lines[i]);
        if (line.empty()) {
            break; // End of headers
        }
        headerLines.push_back(line);
    }
    
    parseHeaders(headerLines);
    
    // Parse body (everything after headers)
    if (i < lines.size()) {
        for (++i; i < lines.size(); ++i) {
            _body += lines[i];
            if (i < lines.size() - 1) {
                _body += "\n";
            }
        }
    }
    
    // Parse query string from URI
    parseQueryString(_uri);
    
    _isValid = !_method.empty() && !_uri.empty() && !_version.empty();
    return _isValid;
}

void HttpRequest::parseRequestLine(const std::string& line) {
    std::vector<std::string> parts = Utils::split(line, ' ');
    if (parts.size() >= 3) {
        _method = Utils::toUpper(parts[0]);
        _uri = parts[1];
        _version = parts[2];
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

const std::string& HttpRequest::getBody() const {
    return _body;
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

void HttpRequest::print() const {
    std::cout << "=== HTTP Request ===" << std::endl;
    std::cout << "Method: " << _method << std::endl;
    std::cout << "URI: " << _uri << std::endl;
    std::cout << "Version: " << _version << std::endl;
    std::cout << "Headers:" << std::endl;
    for (std::map<std::string, std::string>::const_iterator it = _headers.begin(); it != _headers.end(); ++it) {
        std::cout << "  " << it->first << ": " << it->second << std::endl;
    }
    if (!_queryParams.empty()) {
        std::cout << "Query Parameters:" << std::endl;
        for (std::map<std::string, std::string>::const_iterator it = _queryParams.begin(); it != _queryParams.end(); ++it) {
            std::cout << "  " << it->first << "=" << it->second << std::endl;
        }
    }
    if (!_body.empty()) {
        std::cout << "Body: " << _body << std::endl;
    }
    std::cout << "===================" << std::endl;
}