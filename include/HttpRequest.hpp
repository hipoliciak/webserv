#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include "webserv.hpp"

class HttpRequest {
	public:
		HttpRequest();
		HttpRequest(const std::string& headers, const std::string& bodyFilePath);
		~HttpRequest();

		// Parsing
		bool parse(const std::string& headers, const std::string& bodyFilePath);
		void parseRequestLine(const std::string& line);
		void parseHeaders(const std::vector<std::string>& headerLines);
		void parseQueryString(const std::string& uri);
		
		// Getters
		const std::string& getMethod() const;
		const std::string& getUri() const;
		const std::string& getVersion() const;
		const std::string& getBodyFilePath() const;
		const std::map<std::string, std::string>& getHeaders() const;
		const std::map<std::string, std::string>& getQueryParams() const;
		std::string getHeader(const std::string& key) const;
		bool isValid() const;
		
		// Utilities
		std::string getPath() const;
		bool hasHeader(const std::string& key) const;
		size_t getContentLength() const;

	private:
		std::string _method;
		std::string _uri;
		std::string _version;
		std::map<std::string, std::string> _headers;
		std::string _bodyFilePath;
		std::map<std::string, std::string> _queryParams;
		bool _isValid;
};

#endif