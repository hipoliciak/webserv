#include "../include/HttpResponse.hpp"
#include "../include/Utils.hpp"

HttpResponse::HttpResponse() : _statusCode(200), _version("HTTP/1.1") {
    setStatus(200);
    setHeader("Server", "webserv/1.0");
    setHeader("Date", Utils::getCurrentTime());
}

HttpResponse::HttpResponse(int statusCode) : _statusCode(statusCode), _version("HTTP/1.1") {
    setStatus(statusCode);
    setHeader("Server", "webserv/1.0");
    setHeader("Date", Utils::getCurrentTime());
}

HttpResponse::~HttpResponse() {
}

void HttpResponse::setStatus(int code) {
    _statusCode = code;
    _statusMessage = getStatusMessage(code);
}

void HttpResponse::setStatus(int code, const std::string& message) {
    _statusCode = code;
    _statusMessage = message;
}

int HttpResponse::getStatusCode() const {
    return _statusCode;
}

const std::string& HttpResponse::getStatusMessage() const {
    return _statusMessage;
}

void HttpResponse::setHeader(const std::string& key, const std::string& value) {
    _headers[key] = value;
}

void HttpResponse::setContentType(const std::string& type) {
    setHeader("Content-Type", type);
}

void HttpResponse::setContentLength(size_t length) {
    setHeader("Content-Length", Utils::sizeToString(length));
}

std::string HttpResponse::getHeader(const std::string& key) const {
    std::map<std::string, std::string>::const_iterator it = _headers.find(key);
    return (it != _headers.end()) ? it->second : "";
}

void HttpResponse::setBody(const std::string& body) {
    _body = body;
    setContentLength(_body.length());
}

void HttpResponse::appendBody(const std::string& data) {
    _body += data;
    setContentLength(_body.length());
}

const std::string& HttpResponse::getBody() const {
    return _body;
}

std::string HttpResponse::toString() const {
    std::string response = _version + " " + Utils::intToString(_statusCode) + " " + _statusMessage + "\r\n";
    
    // Add headers
    for (std::map<std::string, std::string>::const_iterator it = _headers.begin(); it != _headers.end(); ++it) {
        response += it->first + ": " + it->second + "\r\n";
    }
    
    response += "\r\n"; // Empty line separating headers from body
    response += _body;
    
    return response;
}

void HttpResponse::clear() {
    _statusCode = 200;
    _statusMessage = "OK";
    _headers.clear();
    _body.clear();
    _version = "HTTP/1.1";
    
    setHeader("Server", "webserv/1.0");
    setHeader("Date", Utils::getCurrentTime());
}

HttpResponse HttpResponse::createErrorResponse(int statusCode) {
    HttpResponse response(statusCode);
    
    std::string errorMessage;
    if (statusCode == 413) {
        errorMessage = "The request payload is too large.";
    } else if (statusCode == 404) {
        errorMessage = "The requested resource could not be found.";
    } else if (statusCode == 403) {
        errorMessage = "Access to this resource is forbidden.";
    } else if (statusCode == 405) {
        errorMessage = "The request method is not allowed for this resource.";
    } else if (statusCode == 500) {
        errorMessage = "An internal server error occurred.";
    } else {
        errorMessage = "An error occurred.";
    }
    
    std::string errorPage = "<!DOCTYPE html>\n"
                           "<html>\n"
                           "<head>\n"
                           "    <title>" + Utils::intToString(statusCode) + " " + getStatusMessage(statusCode) + "</title>\n"
                           "</head>\n"
                           "<body>\n"
                           "    <h1>" + Utils::intToString(statusCode) + " " + getStatusMessage(statusCode) + "</h1>\n"
                           "    <p>" + errorMessage + "</p>\n"
                           "    <hr>\n"
                           "    <small>webserv/1.0</small>\n"
                           "</body>\n"
                           "</html>\n";
    
    response.setContentType("text/html");
    response.setBody(errorPage);
    
    return response;
}

HttpResponse HttpResponse::createFileResponse(const std::string& filePath) {
    HttpResponse response;
    
    if (!Utils::fileExists(filePath)) {
        Utils::logDebug("File not found: " + filePath + ", returning 404");
        return createErrorResponse(404);
    }
    
    std::string content = Utils::readFile(filePath);
    // Note: Empty file content is valid, don't treat as error
    
    std::string mimeType = getMimeType(filePath);
    response.setContentType(mimeType);
    response.setBody(content);
    Utils::logDebug("File response created for: " + filePath + ", status: " + Utils::intToString(response.getStatusCode()) + 
                   ", content length: " + Utils::sizeToString(content.size()));
    
    return response;
}

HttpResponse HttpResponse::createRedirectResponse(const std::string& location) {
    HttpResponse response(301);
    response.setHeader("Location", location);
    
    std::string redirectPage = "<!DOCTYPE html>\n"
                              "<html>\n"
                              "<head>\n"
                              "    <title>301 Moved Permanently</title>\n"
                              "</head>\n"
                              "<body>\n"
                              "    <h1>Moved Permanently</h1>\n"
                              "    <p>The document has moved <a href=\"" + location + "\">here</a>.</p>\n"
                              "</body>\n"
                              "</html>\n";
    
    response.setContentType("text/html");
    response.setBody(redirectPage);
    
    return response;
}

std::string HttpResponse::getStatusMessage(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 408: return "Request Timeout";
        case 413: return "Payload Too Large";
        case 414: return "URI Too Long";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        default: return "Unknown";
    }
}

std::string HttpResponse::getMimeType(const std::string& filePath) {
    std::string extension = Utils::getFileExtension(filePath);
    extension = Utils::toLower(extension);
    
    if (extension == ".html" || extension == ".htm") return "text/html";
    if (extension == ".css") return "text/css";
    if (extension == ".js") return "application/javascript";
    if (extension == ".json") return "application/json";
    if (extension == ".xml") return "application/xml";
    if (extension == ".png") return "image/png";
    if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
    if (extension == ".gif") return "image/gif";
    if (extension == ".svg") return "image/svg+xml";
    if (extension == ".ico") return "image/x-icon";
    if (extension == ".txt") return "text/plain";
    if (extension == ".pdf") return "application/pdf";
    
    return "application/octet-stream";
}