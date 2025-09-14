# webserv

A fully-featured HTTP server implementation written in C++98, designed as an educational project to understand the fundamentals of web server architecture and HTTP protocol.

## 🚀 Features

- **HTTP/1.1 Protocol Support**: Complete HTTP/1.1 implementation
- **Multiple HTTP Methods**: GET, POST, DELETE with full functionality
- **CGI Execution**: Python, PHP, and shell script execution
- **File Upload Support**: Multipart form data parsing and file handling
- **Static File Serving**: Efficient serving of HTML, CSS, JavaScript, and other static files
- **Directory Listing**: Configurable autoindex for directory browsing
- **Custom Error Pages**: Configurable error pages for different HTTP status codes
- **Configuration File Support**: Flexible nginx-like configuration system
- **Non-blocking I/O**: Uses `poll()` for efficient connection handling
- **MIME Type Detection**: Automatic content type detection based on file extensions
- **Request Size Limits**: Configurable max_body_size enforcement
- **Signal Handling**: Graceful shutdown on SIGINT/SIGTERM
- **Path Resolution**: Proper URL routing and index file resolution

## 📁 Project Structure

```
webserv/
├── include/                 # Header files
│   ├── webserv.hpp         # Main header with common includes
│   ├── Server.hpp          # Server class definition
│   ├── Client.hpp          # Client connection handling
│   ├── HttpRequest.hpp     # HTTP request parsing
│   ├── HttpResponse.hpp    # HTTP response generation
│   ├── Config.hpp          # Configuration parsing
│   └── Utils.hpp           # Utility functions
├── src/                    # Source files
│   ├── main.cpp           # Entry point
│   ├── Server.cpp         # Server implementation
│   ├── Client.cpp         # Client handling
│   ├── HttpRequest.cpp    # Request parsing logic
│   ├── HttpResponse.cpp   # Response generation logic
│   ├── Config.cpp         # Configuration parser
│   └── Utils.cpp          # Utility functions
├── config/                 # Configuration files
│   ├── default.conf       # Default server configuration
│   └── production.conf    # Production configuration example
├── www/                   # Web content directory
│   ├── index.html         # Main page
│   ├── test.html          # Test page
│   ├── upload.html        # Upload test page (fully functional)
│   ├── css/              # Stylesheets
│   ├── js/               # JavaScript files
│   ├── images/           # Image files
│   ├── error/            # Custom error pages
│   ├── cgi-bin/          # CGI scripts (Python, PHP, Shell)
│   ├── files/            # Static files with directory listing
│   └── uploads/          # File upload destination
├── tests/                 # Test directory
├── Makefile              # Build configuration
└── README.md             # This file
```

## 🛠️ Building the Project

### Prerequisites

- C++ compiler with C++98 support (g++, clang++)
- Make utility
- UNIX-like operating system (Linux, macOS)

### Compilation

```bash
# Build the project
make

# Build with debug flags
make debug

# Build optimized release version
make release

# Clean object files
make clean

# Clean everything
make fclean

# Rebuild everything
make re
```

## 🚀 Running the Server

### Basic Usage

```bash
# Run with default configuration
./webserv

# Run with custom configuration file
./webserv config/production.conf

# Using make targets
make run                    # Run with default config
make run-config            # Run with production config
```

### Configuration

The server uses configuration files to define server behavior. Here's an example configuration:

```
server {
    listen 8080
    host 127.0.0.1
    server_name localhost
    root www
    index index.html index.htm
    max_body_size 1048576
    autoindex on
    upload_path uploads
    cgi_path cgi-bin
    
    allow_methods GET POST DELETE
    
    error_page 404 www/error/404.html
    error_page 500 www/error/500.html
    
    location /uploads {
        allow_methods GET POST DELETE
        autoindex on
    }
    
    location /cgi-bin {
        allow_methods GET POST
        cgi_extensions .py .php .sh
    }
}
```

#### Configuration Directives

- `listen`: Port number to listen on
- `host`: IP address to bind to
- `server_name`: Server name for virtual hosting
- `root`: Document root directory
- `index`: Default index files (space-separated list)
- `max_body_size`: Maximum request body size in bytes
- `autoindex`: Enable/disable directory listing
- `upload_path`: Directory for file uploads
- `cgi_path`: Directory for CGI scripts
- `allow_methods`: Allowed HTTP methods
- `error_page`: Custom error page mapping
- `location`: Location-specific configuration blocks
- `cgi_extensions`: File extensions for CGI execution

## 🧪 Manual Testing

### Step 1: Launch the Server

1. **Compile the project:**
   ```bash
   make clean && make
   ```

2. **Start the server:**
   ```bash
   # Using default configuration (recommended for testing)
   ./webserv
   
   # OR using custom configuration
   ./webserv config/production.conf
   ```

3. **Verify server startup:**
   - Check that the server displays "Server listening on [host]:[port]"
   - Note the port number (default is 8080)

### Step 2: Browser Testing

Open your web browser and test these URLs:

```
http://localhost:8080/                     # Root path - serves index.html
http://localhost:8080/index.html          # Main page - welcome page
http://localhost:8080/test.html           # Test page
http://localhost:8080/upload.html         # File upload interface (fully functional)
http://localhost:8080/files/              # Directory listing (if autoindex enabled)
http://localhost:8080/uploads/            # Upload directory listing
http://localhost:8080/cgi-bin/test.py     # Python CGI script execution
http://localhost:8080/cgi-bin/info.sh     # Shell script CGI execution
http://localhost:8080/nonexistent         # 404 error page test
```

### Step 3: Command Line Testing

Open a new terminal and run these curl commands:

#### Basic HTTP Methods:
```bash
# GET request (returns 200 for existing files)
curl -i http://localhost:8080/index.html

# GET root path (serves index.html)
curl -i http://localhost:8080/

# POST request (basic response)
curl -X POST -d "name=test&value=123" http://localhost:8080/

# DELETE request (deletes files if allowed)
curl -X DELETE http://localhost:8080/uploads/testfile.txt
```

#### CGI Testing:
```bash
# Python CGI script execution
curl http://localhost:8080/cgi-bin/test.py

# Shell script CGI execution
curl http://localhost:8080/cgi-bin/info.sh

# CGI with query parameters
curl "http://localhost:8080/cgi-bin/test.py?name=john&age=25"

# POST to CGI script
curl -X POST -d "username=test&password=123" http://localhost:8080/cgi-bin/test.py
```

#### File Upload Testing:
```bash
# Upload a file
curl -X POST -F "file=@README.md" http://localhost:8080/uploads/

# Upload with custom filename
curl -X POST -F "file=@README.md;filename=custom.txt" http://localhost:8080/uploads/
```

#### Directory Listing:
```bash
# View directory contents
curl http://localhost:8080/files/
curl http://localhost:8080/uploads/
```

#### Error Testing:
```bash
# Test 404 error
curl -i http://localhost:8080/nonexistent

# Test method not allowed 
curl -X PUT http://localhost:8080/

# Test large request body (enforces max_body_size)
curl -X POST --data-binary "@README.md" http://localhost:8080/

# Test request size limit
dd if=/dev/zero bs=1M count=2 | curl -X POST --data-binary @- http://localhost:8080/
```

### Step 4: Configuration Testing

1. **Test multiple server configurations:**
   ```bash
   # Stop current server (Ctrl+C)
   # Start with production config
   ./webserv config/production.conf
   ```

2. **Test different ports (if configured):**
   ```bash
   curl http://localhost:8081/  # If second server is configured
   ```

### Step 5: Load Testing

Test server stability with concurrent requests:

```bash
# Simple concurrent test
for i in {1..10}; do curl http://localhost:8080/index.html & done; wait

# Test with multiple different requests
curl http://localhost:8080/ &
curl http://localhost:8080/test.html &
curl http://localhost:8080/cgi-bin/test.py &
wait
```

### Expected Results

**✅ What is fully implemented and working:**
- ✅ HTTP 200 status for existing static files (GET requests)
- ✅ HTTP 404 for non-existent files with custom error pages
- ✅ HTTP 405 for unsupported methods with proper headers
- ✅ Complete POST request handling including body parsing
- ✅ DELETE method with file deletion functionality
- ✅ CGI script execution (Python, PHP, Shell scripts)
- ✅ File upload via POST with multipart/form-data
- ✅ Directory listing with autoindex feature
- ✅ Root path resolution (/) to index.html
- ✅ Request body size limits enforcement (max_body_size)
- ✅ Graceful server shutdown on SIGINT/SIGTERM
- ✅ Proper Content-Type headers for all file types
- ✅ Multiple server configurations and location blocks
- ✅ Non-blocking I/O with poll() for concurrent connections
- ✅ Environment variable passing to CGI scripts
- ✅ Query parameter parsing for CGI
- ✅ Chunked transfer encoding support
- ✅ Custom error pages for all HTTP status codes

**🔍 What to verify:**
- Response headers include "Server: webserv/1.0"
- CGI scripts execute and return dynamic content
- File uploads save correctly to uploads/ directory
- Directory listings show proper HTML formatting
- Error pages use custom HTML templates
- Static file serving works for all MIME types
- POST data is correctly parsed and passed to CGI
- DELETE requests remove files with proper security checks
- Configuration changes take effect on server restart
- Server logs show detailed request/response information

**✅ Performance and Reliability:**
- Server handles multiple simultaneous connections
- Memory usage remains stable under load
- No memory leaks (verified with Valgrind)
- Proper resource cleanup on connection close
- Signal handling works correctly for graceful shutdown

## 🏗️ Architecture

### Core Components

1. **Server Class**: Main server logic, socket management, and request routing
2. **Client Class**: Individual client connection handling and buffering
3. **HttpRequest Class**: HTTP request parsing and validation
4. **HttpResponse Class**: HTTP response generation and formatting
5. **Config Class**: Configuration file parsing and server setup
6. **Utils Namespace**: Common utility functions for string manipulation, file I/O, etc.

### Request Flow

1. Server accepts incoming connections using `accept()`
2. Client data is read and buffered using `recv()`
3. Complete HTTP requests are parsed into `HttpRequest` objects
4. Server processes requests and generates `HttpResponse` objects
5. Responses are sent back to clients using `send()`
6. Connections are managed using `poll()` for non-blocking I/O

## 📝 HTTP Features

### Supported Methods

- **GET**: Retrieve resources (files, pages, directory listings)
- **POST**: Submit data to the server (form data, file uploads)
- **DELETE**: Delete resources (with security restrictions)

### Status Codes

- `200 OK`: Successful request
- `400 Bad Request`: Malformed request
- `403 Forbidden`: Access denied
- `404 Not Found`: Resource not found
- `405 Method Not Allowed`: HTTP method not supported
- `413 Payload Too Large`: Request body exceeds max_body_size
- `500 Internal Server Error`: Server error
- `502 Bad Gateway`: CGI execution error

### Content Types

Automatic MIME type detection for:
- HTML files (`text/html`)
- CSS files (`text/css`)
- JavaScript files (`application/javascript`)
- Images (PNG, JPEG, GIF, SVG, WebP)
- JSON files (`application/json`)
- Text files (`text/plain`)
- PDF files (`application/pdf`)
- And many more...

### CGI Support

- **Python Scripts**: `.py` files executed with `/usr/bin/python3`
- **PHP Scripts**: `.php` files executed with `/usr/bin/php-cgi`
- **Shell Scripts**: `.sh` files executed with `/bin/bash`
- **Environment Variables**: Full CGI environment support
- **Input/Output**: Proper stdin/stdout handling for CGI communication

## 🔧 Development

### Code Style

- C++98 standard compliance
- Orthodox Canonical Form for classes
- RAII principles
- Consistent naming conventions
- Comprehensive error handling

### Memory Management

- No memory leaks (use valgrind for verification)
- Proper resource cleanup in destructors
- Exception safety considerations

### Performance Considerations

- Non-blocking I/O using `poll()`
- Efficient string handling
- Minimal memory allocations
- Connection pooling and reuse

## 🐛 Debugging

```bash
# Build with debug information
make debug

# Run with Valgrind
valgrind --leak-check=full ./webserv

# Check for memory leaks
valgrind --tool=memcheck --leak-check=yes ./webserv
```

## 📚 Educational Value

This project demonstrates:
- Socket programming in C++
- Complete HTTP protocol implementation
- Non-blocking I/O with poll/select
- Configuration file parsing
- Web server architecture patterns
- CGI protocol implementation
- Multipart form data parsing
- Error handling and logging
- Memory management in C++
- MIME type handling
- URL parsing and routing
- File system operations
- Process management with fork/exec
- Signal handling for graceful shutdown
- Request/response lifecycle management

## 🎯 Webserv Requirements Compliance

This implementation fully satisfies all webserv project requirements:

✅ **Mandatory Features:**
- HTTP/1.1 compliant server
- GET, POST, DELETE method support
- Configuration file with nginx-like syntax
- Multiple server configurations
- Static file serving with correct MIME types
- Custom error pages
- Non-blocking I/O operations
- No external libraries except standard C++98

✅ **Bonus Features:**
- CGI execution for multiple languages
- File upload handling
- Directory listing (autoindex)
- Request size limitations
- Location-based routing
- Signal handling for graceful shutdown

**Performance Characteristics:**
- Handles concurrent connections efficiently
- Memory-safe with no leaks
- Responsive under moderate load
- Proper resource cleanup

## 🤝 Contributing

This is a fully functional educational project. Contributions welcome for:
- Performance optimizations
- Additional CGI language support
- Enhanced security features
- More configuration options
- Extended HTTP feature support
- Test suite improvements
- Documentation enhancements

## 📄 License

This project is developed for educational purposes as part of the 42 School curriculum. See the project requirements for specific usage guidelines.

## 📞 Support

For questions or issues:
1. Check the configuration file syntax carefully
2. Verify file permissions in the `www/` directory
3. Check server logs for detailed error messages
4. Ensure the specified port is available and not in use
5. Test with simple HTML files first before trying CGI
6. Verify CGI interpreter paths (`/usr/bin/python3`, `/usr/bin/php-cgi`, `/bin/bash`)
7. Check that uploaded files have proper write permissions

## 🚀 Quick Start Example

```bash
# Clone and build
git clone <repository-url> webserv
cd webserv
make

# Start server
./webserv

# In another terminal, test functionality:
curl http://localhost:8080/                    # Static file serving
curl http://localhost:8080/cgi-bin/test.py     # CGI execution
curl -F "file=@README.md" http://localhost:8080/uploads/  # File upload
curl -X DELETE http://localhost:8080/uploads/README.md    # File deletion
curl http://localhost:8080/files/              # Directory listing
```

---

*Built with ❤️ and C++98 - A complete HTTP server implementation for learning web technologies*