#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include "webserv.hpp"

class HttpRequest {
private:
    std::string _method;
    std::string _uri;
    std::string _version;
    std::map<std::string, std::string> _headers;
    std::string _body;
    std::map<std::string, std::string> _queryParams;
    bool _isValid;

public:
    HttpRequest();
    HttpRequest(const std::string& rawRequest);
    ~HttpRequest();

    // Parsing
    bool parse(const std::string& rawRequest);
    void parseRequestLine(const std::string& line);
    void parseHeaders(const std::vector<std::string>& headerLines);
    void parseQueryString(const std::string& uri);
    
    // Getters
    const std::string& getMethod() const;
    const std::string& getUri() const;
    const std::string& getVersion() const;
    const std::string& getBody() const;
    const std::map<std::string, std::string>& getHeaders() const;
    const std::map<std::string, std::string>& getQueryParams() const;
    std::string getHeader(const std::string& key) const;
    bool isValid() const;
    
    // Utilities
    std::string getPath() const;
    bool hasHeader(const std::string& key) const;
    size_t getContentLength() const;
    
    // Debug
    void print() const;
};

#endif