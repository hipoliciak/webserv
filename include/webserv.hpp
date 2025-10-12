#ifndef WEBSERV_HPP
#define WEBSERV_HPP

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <climits>

// System includes
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <dirent.h>

// Forward declarations
class Server;
class Client;
class HttpRequest;
class HttpResponse;
class Config;
class CGI;

// Location configuration structure
struct LocationConfig {
    std::string path;
    std::string root;
    std::string index;
    std::vector<std::string> allowedMethods;
    std::map<int, std::string> redirections;
    bool autoIndex;
    std::string uploadPath;
    std::string cgiPath;
    std::string cgiExtension;
    bool isRegex;
};

// Common constants
#define BUFFER_SIZE 4096
#define MAX_CONNECTIONS 1000
#define DEFAULT_PORT 8080
#define DEFAULT_HOST "127.0.0.1"
#define MAX_BODY_SIZE 1048576  // 1MB default

// HTTP status codes
#define HTTP_OK 200
#define HTTP_CREATED 201
#define HTTP_NO_CONTENT 204
#define HTTP_MOVED_PERMANENTLY 301
#define HTTP_FOUND 302
#define HTTP_BAD_REQUEST 400
#define HTTP_FORBIDDEN 403
#define HTTP_NOT_FOUND 404
#define HTTP_METHOD_NOT_ALLOWED 405
#define HTTP_REQUEST_TIMEOUT 408
#define HTTP_PAYLOAD_TOO_LARGE 413
#define HTTP_INTERNAL_SERVER_ERROR 500
#define HTTP_NOT_IMPLEMENTED 501
#define HTTP_SERVICE_UNAVAILABLE 503

#endif