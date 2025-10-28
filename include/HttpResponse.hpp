#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include "webserv.hpp"

class HttpResponse {
	public:
		HttpResponse();
		HttpResponse(int statusCode);
		~HttpResponse();

		// Status
		void setStatus(int code);
		void setStatus(int code, const std::string& message);
		int getStatusCode() const;
		const std::string& getStatusMessage() const;
		
		// Headers
		void setHeader(const std::string& key, const std::string& value);
		void setContentType(const std::string& type);
		void setContentLength(size_t length);
		std::string getHeader(const std::string& key) const;
		
		// Body
		void setBody(const std::string& body);
		void appendBody(const std::string& data);
		const std::string& getBody() const;
		
		// Generation
		std::string toString() const;
		void clear();
		
		// Common responses
		static HttpResponse createErrorResponse(int statusCode);
		static HttpResponse createFileResponse(const std::string& filePath);
		
		// Utilities
		static std::string getStatusMessage(int code);
		static std::string getMimeType(const std::string& filePath);
	private:
		int _statusCode;
		std::string _statusMessage;
		std::map<std::string, std::string> _headers;
		std::string _body;
		std::string _version;
};

#endif