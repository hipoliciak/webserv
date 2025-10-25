# Webserv Evaluation Help Guide

This document provides detailed explanations for common evaluation questions about our HTTP server implementation.

## Table of Contents
1. [HTTP Server Basics](#http-server-basics)
2. [I/O Multiplexing](#io-multiplexing)
3. [Server Architecture](#server-architecture)

---

## HTTP Server Basics

### What is an HTTP Server?

An HTTP (HyperText Transfer Protocol) server is a software application that serves web content to clients (typically web browsers) over the HTTP protocol. Our webserver implementation includes:

**Core Components:**
- **Socket Management**: Creates and manages TCP sockets for client connections
- **Request Parser**: Parses incoming HTTP requests (headers, body, methods)
- **Response Generator**: Creates HTTP responses with appropriate status codes and content
- **Resource Handler**: Serves static files, executes CGI scripts, handles uploads
- **Configuration System**: Manages server settings, virtual hosts, locations

**HTTP Protocol Support:**
- HTTP/1.1 compliance
- Methods: GET, POST, DELETE, HEAD
- Status codes: 200, 404, 405, 500, etc.
- Headers: Content-Type, Content-Length, Transfer-Encoding
- Chunked transfer encoding for large requests
- Keep-alive connections

**Key Features Implemented:**
- Multi-server configuration (virtual hosts)
- CGI script execution (PHP, Python, Bash)
- File upload handling
- Custom error pages
- Directory listing
- Location-based routing with regex support

---

## I/O Multiplexing

### Which function do we use for I/O Multiplexing?

We use **`poll()`** for I/O multiplexing in our HTTP server implementation.

**Location in code:**
```cpp
// In src/main.cpp - Main server loop
int pollResult = poll(pollFds.data(), pollFds.size(), POLL_TIMEOUT);
```

### How does poll() work?

**poll() System Call Explanation:**

1. **Function Signature:**
   ```cpp
   int poll(struct pollfd *fds, nfds_t nfds, int timeout);
   ```

2. **pollfd Structure:**
   ```cpp
   struct pollfd {
       int   fd;         // File descriptor
       short events;     // Requested events (POLLIN, POLLOUT)
       short revents;    // Returned events
   };
   ```

3. **Our Implementation Process:**
   ```cpp
   // 1. Prepare pollfd array with all active file descriptors
   std::vector<struct pollfd> pollFds;
   
   // 2. Add server listening sockets
   for (size_t i = 0; i < servers.size(); ++i) {
       pollfd serverPollFd;
       serverPollFd.fd = servers[i].getListenFd();
       serverPollFd.events = POLLIN;  // Listen for incoming connections
       pollFds.push_back(serverPollFd);
   }
   
   // 3. Add client sockets
   for (std::map<int, Client>::iterator it = clients.begin(); it != clients.end(); ++it) {
       pollfd clientPollFd;
       clientPollFd.fd = it->first;
       clientPollFd.events = POLLIN;  // Listen for client data
       pollFds.push_back(clientPollFd);
   }
   
   // 4. Call poll() - blocks until activity or timeout
   int pollResult = poll(pollFds.data(), pollFds.size(), POLL_TIMEOUT);
   ```

4. **Event Handling:**
   - **POLLIN**: Data available for reading (new connection or client data)
   - **POLLOUT**: Socket ready for writing (not used in our implementation)
   - **POLLERR/POLLHUP**: Error conditions or connection closed

### Do we use only one poll() call?

**Yes, we use only one poll() call** in our main server loop.

**How we manage server accept and client read/write:**

1. **Single Event Loop Architecture:**
   ```cpp
   while (running) {
       // Single poll() call handles all file descriptors
       int pollResult = poll(pollFds.data(), pollFds.size(), POLL_TIMEOUT);
       
       // Process all ready file descriptors
       for (size_t i = 0; i < pollFds.size(); ++i) {
           if (pollFds[i].revents & POLLIN) {
               // Handle the ready file descriptor
           }
       }
   }
   ```

2. **Server Socket Handling (Accept New Connections):**
   ```cpp
   // Check if it's a server listening socket
   for (size_t j = 0; j < servers.size(); ++j) {
       if (pollFds[i].fd == servers[j].getListenFd()) {
           // Accept new client connection
           int clientFd = accept(serverFd, (struct sockaddr*)&clientAddr, &clientLen);
           clients[clientFd] = Client(clientFd);
           break;
       }
   }
   ```

3. **Client Socket Handling (Read/Write Operations):**
   ```cpp
   // Check if it's a client socket
   std::map<int, Client>::iterator clientIt = clients.find(pollFds[i].fd);
   if (clientIt != clients.end()) {
       Client& client = clientIt->second;
       
       // Read client data
       if (!client.readData()) {
           // Handle client disconnection
           close(clientIt->first);
           clients.erase(clientIt);
           continue;
       }
       
       // Process complete requests
       if (client.isRequestComplete()) {
           // Generate and send response
           HttpResponse response = server.handleRequest(request);
           send(clientFd, response.toString().c_str(), response.toString().length(), 0);
           
           // Clean up for next request
           client.clearRequest();
       }
   }
   ```

**Advantages of this approach:**
- **Efficiency**: Single system call handles all I/O operations
- **Scalability**: Can handle many concurrent connections
- **Non-blocking**: Server doesn't block on individual client operations
- **Simplicity**: Clear separation between accept and client handling logic
- **Resource Management**: Easy to track and clean up client connections

**Key Implementation Details:**
- Server sockets are added first in the pollfd array for priority
- Client sockets are dynamically added/removed as connections open/close
- Timeout prevents infinite blocking (POLL_TIMEOUT = 1000ms)
- Error handling for POLLERR and POLLHUP events
- Graceful cleanup of disconnected clients

---

## Test Setup and Examples

This section provides practical examples for testing various server features during evaluation.

### 1. Multiple Servers with Different Ports

**Configuration Example:**
```properties
# Server 1 - Port 8080
server {
    listen 8080
    host localhost
    server_name webserv-main
    root www-main
    
    location / {
        allow_methods GET POST
        default index.html
    }
}

# Server 2 - Port 8081  
server {
    listen 8081
    host localhost
    server_name webserv-api
    root www-api
    
    location / {
        allow_methods GET POST PUT DELETE
        default api.html
    }
}

# Server 3 - Port 8082
server {
    listen 8082
    host localhost  
    server_name webserv-files
    root www-files
    
    location / {
        allow_methods GET
        autoindex on
    }
}
```

**Test Commands:**
```bash
# Test server on port 8080
curl http://localhost:8080/

# Test server on port 8081
curl http://localhost:8081/

# Test server on port 8082  
curl http://localhost:8082/

# Verify different content/behavior on each port
curl -I http://localhost:8080/
curl -I http://localhost:8081/
curl -I http://localhost:8082/
```

### 2. Multiple Servers with Different Hostnames

**IMPORTANT**: For hostname-based virtual hosting, you need **ONE server block** with **multiple server_name entries**, not multiple server blocks with the same port.

**Correct Configuration Example:**
```properties
# Single server handling multiple hostnames
server {
    listen 8080
    host localhost
    server_name example.com www.example.com api.example.com files.example.com
    root www-main
    
    # Main website location
    location / {
        allow_methods GET POST
        default index.html
    }
    
    # API endpoints (can be differentiated by path)
    location /api {
        allow_methods GET POST PUT DELETE
        root www-api
        default api.json
    }
    
    # File downloads
    location /files {
        allow_methods GET
        root www-files
        autoindex on
    }
}
```

**Alternative: Use different ports for true separation**
```properties
# Main website - Port 8080
server {
    listen 8080
    host localhost
    server_name example.com www.example.com
    root www-main
    
    location / {
        allow_methods GET POST
        default index.html
    }
}

# API server - Port 8081
server {
    listen 8081
    host localhost
    server_name api.example.com
    root www-api
    
    location / {
        allow_methods GET POST PUT DELETE
        default api.json
    }
}

# File server - Port 8082
server {
    listen 8082
    host localhost
    server_name files.example.com
    root www-files
    
    location / {
        allow_methods GET
        autoindex on
    }
}
```

**Test Commands for Single Server Method:**
```bash
# Test main website (served from root location)
curl -H "Host: example.com" http://localhost:8080/
curl -H "Host: www.example.com" http://localhost:8080/

# Test API endpoints (served from /api location)
curl -H "Host: api.example.com" http://localhost:8080/api/
curl -H "Host: example.com" http://localhost:8080/api/

# Test file server (served from /files location)
curl -H "Host: files.example.com" http://localhost:8080/files/
curl -H "Host: example.com" http://localhost:8080/files/

# Using --resolve (alternative method)
curl --resolve example.com:8080:127.0.0.1 http://example.com:8080/
curl --resolve api.example.com:8080:127.0.0.1 http://api.example.com:8080/api/
```

**Test Commands for Multiple Ports Method:**
```bash
# Test main website on port 8080
curl --resolve example.com:8080:127.0.0.1 http://example.com:8080/

# Test API server on port 8081  
curl --resolve api.example.com:8081:127.0.0.1 http://api.example.com:8081/

# Test file server on port 8082
curl --resolve files.example.com:8082:127.0.0.1 http://files.example.com:8082/

# Alternative using Host headers
curl -H "Host: example.com" http://localhost:8080/
curl -H "Host: api.example.com" http://localhost:8081/
curl -H "Host: files.example.com" http://localhost:8082/
```

### 3. Custom Error Pages Setup

**Configuration Example:**
```properties
server {
    listen 8080
    host localhost
    server_name webserv
    root www-main
    
    # Custom error pages
    error_page 400 /error/400.html
    error_page 403 /error/403.html
    error_page 404 /error/custom404.html
    error_page 500 /error/500.html
    error_page 502 /error/502.html
    
    location / {
        allow_methods GET POST
        default index.html
    }
}
```

**Test Commands:**
```bash
# Test 404 error (request non-existent file)
curl http://localhost:8080/nonexistent-file.html

# Test 404 error with different paths
curl http://localhost:8080/fake/path/file.txt

# Test 405 error (wrong method)
curl -X DELETE http://localhost:8080/  # If DELETE not allowed

# Test 403 error (if configured)
curl http://localhost:8080/forbidden-directory/

# Verify custom error page content
curl -v http://localhost:8080/nonexistent.html | grep -i "custom\|error"
```

### 4. Client Body Size Limits

**Configuration Example:**
```properties
server {
    listen 8080
    host localhost
    server_name webserv
    root www-main
    client_max_body_size 1M  # Global limit: 1MB
    
    # Error pages (required for proper error handling)
    error_page 400 /error/400.html
    error_page 403 /error/403.html
    error_page 404 /error/404.html
    error_page 500 /error/500.html
    error_page 502 /error/502.html
    
    location / {
        allow_methods GET POST
        default index.html
    }
    
    # Main upload endpoint
    location /upload/ {
        allow_methods POST PUT
        upload_path www-main/uploads
        client_max_body_size 50M  # Location-specific limit: 50MB
    }
    
    location /small-uploads {
        allow_methods POST
        client_max_body_size 100  # Small limit: 100 bytes
        upload_path www-main/small-uploads
    }
    
    location /large-uploads {
        allow_methods POST
        client_max_body_size 10M  # Large limit: 10MB
        upload_path www-main/large-uploads
    }
}
```

**Test Commands:**
```bash
# Generate test files first
echo "Short content" > small.txt
head -c 1500000 /dev/zero | tr '\0' 'A' > medium.txt  # 1.5MB (exceeds global 1M limit)

# Test small uploads location - within limit (should succeed)
curl -X POST -H "Content-Type: text/plain" --data "tiny" http://localhost:8080/small-uploads/test-small.txt

# Test small uploads location - exceeding limit (should fail with 413)
curl -v -X POST -H "Content-Type: text/plain" --data "$(head -c 150 /dev/zero | tr '\0' 'B')" http://localhost:8080/small-uploads/should-fail.txt

# Test large uploads location - within limit
curl -X POST -H "Content-Type: text/plain" --data "$(head -c 5000 /dev/zero | tr '\0' 'C')" http://localhost:8080/large-uploads/big-file.txt

# Test large uploads location - exceeding location limit (11MB > 10MB limit)
head -c 11000000 /dev/zero > too-big.txt
curl -X POST --data-binary @too-big.txt http://localhost:8080/large-uploads/too-big.txt  # Should fail with 413

# Test global limit enforcement - even with higher location limit
curl -X POST --data-binary @medium.txt http://localhost:8080/upload/  # Fails: 1.5MB > 1MB global limit

# Working file uploads within limits:
curl -X POST -H "Content-Type: text/plain" --data-binary @small.txt http://localhost:8080/upload/my-file.txt
curl -X POST -H "Content-Type: text/plain" --data "Small content" http://localhost:8080/upload/  # Auto-filename

# Verify uploaded files
curl http://localhost:8080/uploads/  # Directory listing
curl http://localhost:8080/uploads/my-file.txt  # Download uploaded file

# Clean up test files
rm -f small.txt medium.txt too-big.txt
```

**Expected Results:**
- ✅ **Within Limits**: Files uploaded successfully with 201 Created response
- ❌ **Exceeding Limits**: HTTP 413 "Payload Too Large" with custom error page
- 🔍 **Validation Order**: Location limit checked first, then global server limit
- 📁 **File Storage**: Files saved to configured `upload_path` with correct names

### 5. Routes to Different Directories

**Configuration Example:**
```properties
server {
    listen 8080
    host localhost
    server_name webserv
    root www-main
    
    # Main website - serves from root directory
    location / {
        allow_methods GET POST
        default index.html
        autoindex on
    }
    
    # API endpoints - serves from www-main/api subdirectory
    location /api {
        allow_methods GET POST PUT DELETE
        root www-main/api
        default api.json
        autoindex on
    }
    
    # File downloads - serves from www-main/files subdirectory
    location /files {
        allow_methods GET
        root www-main/files
        default file1.txt
        autoindex on
    }
    
    # Uploads directory - browsable uploaded files
    location /uploads {
        allow_methods GET
        root www-main/uploads
        autoindex on
    }
}
```

**Directory Structure:**
```
www-main/
├── index.html          # Main website
├── test.html
├── upload.html
├── api/
│   ├── api.json        # API responses
│   ├── endpoints.json
│   └── test.json
├── files/
│   ├── file1.txt       # Downloadable files  
│   └── file2.txt
├── uploads/            # User uploaded files
├── css/
│   └── style.css
└── js/
    └── main.js
```

**Test Commands:**
```bash
# Test main directory (serves from www-main/)
curl http://localhost:8080/                # Should serve index.html
curl http://localhost:8080/test.html       # Direct file access
curl http://localhost:8080/upload.html     # Upload form page

# Test API directory (serves from www-main/api/)
curl http://localhost:8080/api/            # Should serve api.json (default file)
curl http://localhost:8080/api/api.json    # Direct API file access
curl http://localhost:8080/api/test.json   # Another API endpoint

# Test files directory (serves from www-main/files/) 
curl http://localhost:8080/files/          # Should serve file1.txt (default file)
curl http://localhost:8080/files/file1.txt # Direct file access
curl http://localhost:8080/files/file2.txt # Download file2.txt

# Test uploads directory (serves from www-main/uploads/)
curl http://localhost:8080/uploads/        # Shows directory listing (no default file configured)
curl http://localhost:8080/uploads/my-uploaded-file.txt  # Download uploaded file

# Test static assets
curl http://localhost:8080/css/style.css   # CSS file
curl http://localhost:8080/js/main.js      # JavaScript file
```

**Expected Results:**
- ✅ **Root location** (`/`): Serves `index.html` from `www-main/`
- ✅ **API location** (`/api/`): Serves `api.json` (default file) from `www-main/api/`  
- ✅ **Files location** (`/files/`): Serves `file1.txt` (default file) from `www-main/files/`
- ✅ **Uploads location** (`/uploads/`): Shows directory listing (no default file configured)
- ✅ **Default file priority**: Default files served before directory listings when both are configured
- ✅ **Direct file access**: All files accessible via direct paths

### 6. Default Files for Directory Requests

**Configuration Example (Based on Actual Working Config):**
```properties
server {
    listen 8080
    host localhost
    server_name webserv
    root www-main
    
    # Main website - default file configured
    location / {
        allow_methods GET POST
        default index.html  # Default file for root
        autoindex on
    }
    
    # API endpoints - default file configured
    location /api {
        allow_methods GET POST PUT DELETE
        root www-main/api
        default api.json    # Default file for API
        autoindex on
    }
    
    # Files directory - default file configured
    location /files {
        allow_methods GET
        root www-main/files
        default file1.txt   # Default file for files
        autoindex on
    }
    
    # Uploads directory - no default file (directory listing only)
    location /uploads {
        allow_methods GET
        root www-main/uploads
        autoindex on        # Shows directory listing (no default file)
    }
}
```

**Test Commands (Using Actual Directories):**
```bash
# Test root directory default file
curl http://localhost:8080/          # Should serve index.html
curl http://localhost:8080/index.html # Direct access

# Test API directory default
curl http://localhost:8080/api/       # Should serve api.json (working)
curl http://localhost:8080/api/api.json # Direct access

# Test files directory default  
curl http://localhost:8080/files/     # Should serve file1.txt (working)
curl http://localhost:8080/files/file1.txt # Direct access

# Test uploads directory (no default - shows listing)
curl http://localhost:8080/uploads/   # Should show directory listing

# Test other actual files
curl http://localhost:8080/test.html  # Actual file in root
curl http://localhost:8080/upload.html # Upload form page
curl http://localhost:8080/css/style.css # CSS file
```

**Actual Directory Structure:**
```
www-main/
├── index.html          # Default for root (/)
├── test.html
├── upload.html
├── api/
│   ├── api.json        # Default for /api/
│   ├── endpoints.json
│   └── test.json
├── files/
│   ├── file1.txt       # Default for /files/
│   └── file2.txt
├── uploads/            # No default file - shows directory listing
├── cgi-bin/           # CGI scripts
├── css/
│   └── style.css
├── js/
│   └── main.js
└── error/             # Custom error pages
    ├── 400.html
    ├── 403.html
    ├── 404.html
    ├── 500.html
    └── 502.html
```

### 7. Method Restrictions for Routes

**Configuration Example (Based on Actual Working Config):**
```properties
server {
    listen 8080
    host localhost
    server_name webserv
    root www-main
    
    # Main website - allows GET and POST
    location / {
        allow_methods GET POST    # Both methods allowed
        default index.html
        autoindex on
    }
    
    # API endpoints - full CRUD operations
    location /api {
        allow_methods GET POST PUT DELETE  # All methods allowed
        root www-main/api
        default api.json
        autoindex on
    }
    
    # Files directory - read-only access
    location /files {
        allow_methods GET         # Only GET allowed
        root www-main/files
        default file1.txt
        autoindex on
    }
    
    # Uploads directory - read-only browsing
    location /uploads {
        allow_methods GET         # Only GET for browsing
        root www-main/uploads
        autoindex on
    }
}
```

**Alternative Example (More Restrictive):**
```properties
server {
    listen 8080
    host localhost
    server_name webserv
    root www-main
    
    # Read-only main site
    location / {
        allow_methods GET         # Only GET allowed
        default index.html
        autoindex on
    }
    
    # API with limited methods
    location /api {
        allow_methods GET POST    # No PUT/DELETE
        root www-main/api
        default api.json
    }
    
    # Upload location (from upload config)
    location /upload/ {
        allow_methods POST PUT    # Only upload methods
        upload_path www-main/uploads
    }
}
```

**Test Commands (Using Actual Configuration):**
```bash
# Test currently allowed methods (should succeed with current config)
curl -X GET http://localhost:8080/                    # ✓ Allowed (GET POST configured)
curl -X POST http://localhost:8080/                   # ✓ Allowed (GET POST configured)
curl -X GET http://localhost:8080/api/                # ✓ Allowed (GET POST PUT DELETE)
curl -X POST http://localhost:8080/api/               # ✓ Allowed (GET POST PUT DELETE)
curl -X PUT http://localhost:8080/api/users           # ✓ Allowed (GET POST PUT DELETE)
curl -X DELETE http://localhost:8080/api/users        # ✓ Allowed (GET POST PUT DELETE)
curl -X GET http://localhost:8080/files/              # ✓ Allowed (GET only)
curl -X GET http://localhost:8080/uploads/            # ✓ Allowed (GET only)

# Test forbidden methods (should return 405 Method Not Allowed)
curl -X PUT http://localhost:8080/                    # ✗ Should fail (405) - only GET POST allowed
curl -X DELETE http://localhost:8080/                 # ✗ Should fail (405) - only GET POST allowed  
curl -X POST http://localhost:8080/files/             # ✗ Should fail (405) - only GET allowed
curl -X PUT http://localhost:8080/uploads/            # ✗ Should fail (405) - only GET allowed

# Verify error responses
curl -v -X PUT http://localhost:8080/ 2>&1 | grep "405\|Method Not Allowed"
curl -v -X DELETE http://localhost:8080/files/ 2>&1 | grep "405\|Method Not Allowed"

# Test upload endpoints (from upload configuration)
curl -X POST -d "test data" http://localhost:8080/upload/test.txt  # ✓ Allowed (POST PUT)
curl -X PUT -d "test data" http://localhost:8080/upload/test.txt   # ✓ Allowed (POST PUT)
curl -X GET http://localhost:8080/upload/                         # ✗ Should fail (405) - only POST PUT
```

**Expected Results:**
- ✅ **Current Config**: GET/POST work on root, all methods on /api/, GET only on /files/ and /uploads/
- ✅ **Method Validation**: Server validates HTTP methods per location configuration
- ✅ **405 Errors**: Forbidden methods return HTTP 405 Method Not Allowed with custom error page
- ✅ **Upload Locations**: POST/PUT work on /upload/ endpoints, other methods return 405

### 8. HTTP Method Testing with Telnet, curl, and Files

**This section demonstrates comprehensive HTTP method testing using telnet, curl, and prepared files to verify:**
- ✅ GET requests work properly
- ✅ POST requests work properly  
- ✅ DELETE requests work properly
- ✅ UNKNOWN requests don't crash the server
- ✅ All tests return appropriate status codes

#### Method 1: Testing with curl

**GET Request Tests (Using Existing Files):**
```bash
# Test basic GET request
curl -v http://localhost:8080/

# Test GET with existing files from your project
curl -v http://localhost:8080/test.html         # Actual file in www-main
curl -v http://localhost:8080/index.html        # Main page
curl -v http://localhost:8080/upload.html       # Upload form

# Test GET with API endpoints (actual files)
curl -v http://localhost:8080/api/              # Should serve api.json
curl -v http://localhost:8080/api/test.json     # Actual API file
curl -v http://localhost:8080/api/endpoints.json # Actual endpoints file

# Test GET with files directory (actual files)
curl -v http://localhost:8080/files/            # Should serve file1.txt
curl -v http://localhost:8080/files/file1.txt   # Actual file
curl -v http://localhost:8080/files/file2.txt   # Actual file

# Test GET with static assets
curl -v http://localhost:8080/css/style.css     # Actual CSS file
curl -v http://localhost:8080/js/main.js        # Actual JS file

# Test 404 with non-existent files
curl -v http://localhost:8080/nonexistent.html  # Should return 404

# Expected: HTTP/1.1 200 OK for existing resources
# Expected: HTTP/1.1 404 Not Found for non-existing resources
```

**POST Request Tests (Using Existing Files):**
```bash
# Test POST with small data to root
curl -v -X POST -d "Small POST data" http://localhost:8080/

# Test POST using existing files as data
curl -v -X POST --data-binary @www-main/test.html http://localhost:8080/
curl -v -X POST --data-binary @www-main/files/file1.txt http://localhost:8080/

# Test POST to upload endpoint (creates new files)
curl -v -X POST -H "Content-Type: text/plain" -d "Upload test content" http://localhost:8080/upload/new-upload.txt
curl -v -X POST --data-binary @www-main/files/file1.txt http://localhost:8080/upload/copy-of-file1.txt

# Test POST to API endpoint
curl -v -X POST -d '{"test": "data"}' -H "Content-Type: application/json" http://localhost:8080/api/
curl -v -X POST --data-binary @www-main/api/test.json http://localhost:8080/api/

# Expected: HTTP/1.1 200 OK or 201 Created for successful POST
# Expected: HTTP/1.1 405 Method Not Allowed if POST not allowed on location
```

**DELETE Request Tests (Using Existing Files):**
```bash
# Test DELETE on root (should work if configured)
curl -v -X DELETE http://localhost:8080/test.html

# Test DELETE on API endpoints with actual files
curl -v -X DELETE http://localhost:8080/api/test.json
curl -v -X DELETE http://localhost:8080/api/endpoints.json

# Test DELETE on files directory (should fail - GET only configured)
curl -v -X DELETE http://localhost:8080/files/file1.txt
curl -v -X DELETE http://localhost:8080/files/file2.txt

# Test DELETE on uploads (should fail - GET only configured)
curl -v -X DELETE http://localhost:8080/uploads/testfile2.txt

# Expected: HTTP/1.1 200 OK for successful DELETE (where allowed)
# Expected: HTTP/1.1 405 Method Not Allowed if DELETE not allowed
# Expected: HTTP/1.1 404 Not Found if resource doesn't exist
```

**UNKNOWN Request Tests:**
```bash
# Test unknown/invalid HTTP methods
curl -v -X PATCH http://localhost:8080/
curl -v -X TRACE http://localhost:8080/
curl -v -X CONNECT http://localhost:8080/
curl -v -X OPTIONS http://localhost:8080/
curl -v -X INVALID http://localhost:8080/

# Expected: HTTP/1.1 405 Method Not Allowed or 501 Not Implemented
# Expected: Server should NOT crash and continue serving other requests
```

#### Method 2: Testing with telnet (Raw HTTP)

**GET Request via Telnet:**
```bash
# Test basic GET request
telnet localhost 8080
# Then type:
GET / HTTP/1.1
Host: localhost
Connection: close

# Press Enter twice to send the request
```

**POST Request via Telnet:**
```bash
# Test POST request with data
telnet localhost 8080
# Then type:
POST /upload/telnet-upload.txt HTTP/1.1
Host: localhost
Content-Type: text/plain
Content-Length: 17
Connection: close

Test POST content
# Press Enter twice after the content
```

**DELETE Request via Telnet:**
```bash
# Test DELETE request on actual API file
telnet localhost 8080
# Then type:
DELETE /api/test.json HTTP/1.1
Host: localhost
Connection: close

```

**UNKNOWN Method via Telnet:**
```bash
# Test invalid method
telnet localhost 8080
# Then type:
INVALID / HTTP/1.1
Host: localhost
Connection: close

```

#### Method 3: Testing with Prepared Files

**Create Test Script:**
```bash
# Create comprehensive test script
cat > http_method_test.sh << 'EOF'
#!/bin/bash

echo "=== HTTP Method Testing Script ==="
echo "Starting webserver test..."

# Test GET requests
echo "Testing GET requests..."
curl -s -o /dev/null -w "GET /: %{http_code}\n" http://localhost:8080/
curl -s -o /dev/null -w "GET /api/: %{http_code}\n" http://localhost:8080/api/
curl -s -o /dev/null -w "GET /files/: %{http_code}\n" http://localhost:8080/files/
curl -s -o /dev/null -w "GET /nonexistent: %{http_code}\n" http://localhost:8080/nonexistent

# Test POST requests
echo "Testing POST requests..."
curl -s -o /dev/null -w "POST /: %{http_code}\n" -X POST -d "test data" http://localhost:8080/
curl -s -o /dev/null -w "POST /api/: %{http_code}\n" -X POST -d "test data" http://localhost:8080/api/
curl -s -o /dev/null -w "POST /upload/: %{http_code}\n" -X POST -d "test data" http://localhost:8080/upload/script-upload.txt

# Test DELETE requests using actual files
echo "Testing DELETE requests..."
curl -s -o /dev/null -w "DELETE /test.html: %{http_code}\n" -X DELETE http://localhost:8080/test.html
curl -s -o /dev/null -w "DELETE /api/test.json: %{http_code}\n" -X DELETE http://localhost:8080/api/test.json
curl -s -o /dev/null -w "DELETE /files/file1.txt: %{http_code}\n" -X DELETE http://localhost:8080/files/file1.txt

# Test UNKNOWN methods
echo "Testing UNKNOWN methods..."
curl -s -o /dev/null -w "PATCH /: %{http_code}\n" -X PATCH http://localhost:8080/
curl -s -o /dev/null -w "TRACE /: %{http_code}\n" -X TRACE http://localhost:8080/
curl -s -o /dev/null -w "INVALID /: %{http_code}\n" -X INVALID http://localhost:8080/

echo "=== Test completed ==="
EOF

chmod +x http_method_test.sh
./http_method_test.sh
```

**Expected Output:**
```bash
=== HTTP Method Testing Script ===
Starting webserver test...
Testing GET requests...
GET /: 200
GET /api/: 200
GET /files/: 200
GET /nonexistent: 404
Testing POST requests...
POST /: 200
POST /api/: 200
POST /upload/: 201
Testing DELETE requests...
DELETE /test.html: 200 or 405 (depends on configuration)
DELETE /api/test.json: 200 or 405 (depends on configuration)
DELETE /files/file1.txt: 405 (GET only location)
Testing UNKNOWN methods...
PATCH /: 405
TRACE /: 405
INVALID /: 405
=== Test completed ===
```

#### Method 4: Stress Testing Methods

**Multiple Request Testing:**
```bash
# Test multiple requests to ensure server stability
for i in {1..10}; do
    curl -s -o /dev/null -w "Request $i: %{http_code}\n" http://localhost:8080/
done

# Test mixed methods rapidly
for method in GET POST DELETE PATCH INVALID; do
    echo "Testing $method method:"
    curl -s -o /dev/null -w "  $method: %{http_code}\n" -X $method http://localhost:8080/
done
```

**Concurrent Request Testing:**
```bash
# Test concurrent requests (requires GNU parallel or similar)
seq 1 10 | xargs -n1 -P10 -I{} curl -s -o /dev/null -w "Concurrent {}: %{http_code}\n" http://localhost:8080/
```

#### Server Stability Verification

**Check Server Status:**
```bash
# Verify server is still running after all tests
ps aux | grep webserv

# Test basic functionality still works
curl -I http://localhost:8080/

# Check for any memory leaks or issues
curl -s http://localhost:8080/ > /dev/null && echo "Server still responsive"
```

**Expected Results Summary:**
- ✅ **GET Requests**: Return 200 OK for existing resources, 404 for non-existing
- ✅ **POST Requests**: Return 200 OK or 201 Created for allowed locations, 405 for forbidden
- ✅ **DELETE Requests**: Return 200 OK for successful delete, 405 for forbidden locations
- ✅ **UNKNOWN Methods**: Return 405 Method Not Allowed (never crash)
- ✅ **Status Codes**: Always appropriate HTTP status codes returned
- ✅ **Server Stability**: Server continues running and responding after all tests
- ✅ **Error Handling**: Custom error pages displayed for 4xx/5xx responses

## 9. Port Binding and Multiple Server Conflicts

### Testing Port Conflicts - Same Port Multiple Times

**The Question:** "In the configuration try to setup the same port multiple times. It should not work."

**What This Tests:**
- Socket binding behavior when multiple servers try to use the same port
- Proper error handling for port conflicts
- Server startup validation and graceful failure

### Method 1: Multiple Servers in Same Configuration File

**Create a config file with port conflicts:**
```bash
# Create test configuration with duplicate ports
cat > config/port-conflict-test.conf << 'EOF'
# Server 1 - Port 8080
server {
    listen 8080
    host localhost
    server_name server1
    root www-main
    
    location / {
        allow_methods GET POST
        default index.html
    }
}

# Server 2 - SAME PORT 8080 (should cause conflict)
server {
    listen 8080
    host localhost
    server_name server2
    root www-api
    
    location / {
        allow_methods GET POST
        default index.html
    }
}

# Server 3 - Different port (should work)
server {
    listen 8081
    host localhost
    server_name server3
    root www-files
    
    location / {
        allow_methods GET
        autoindex on
    }
}
EOF
```

**Test the configuration:**
```bash
# Try to start the server with conflicting ports
./webserv config/port-conflict-test.conf

# Expected behavior:
# - Server should detect the port conflict during startup
# - Should display an error message about port already in use
# - Should exit gracefully without starting any servers
# - OR start only the first server and fail on the second
```

**Expected Output Examples:**
```bash
# Possible error messages:
Error: Port 8080 is already in use by another server configuration
Error: Failed to bind socket on port 8080: Address already in use
Error: Cannot start server on localhost:8080 - port conflict detected
Server startup failed due to port binding conflicts
```

### Method 2: Launch Multiple Server Instances

**Testing multiple webserv processes with same port:**

**Terminal 1:**
```bash
# Start first server instance
./webserv config/default.conf
# Should start successfully and bind to port 8080
```

**Terminal 2 (while first is running):**
```bash
# Try to start second server instance with same port
./webserv config/default.conf

# Expected behavior:
# - Second instance should fail to start
# - Error message about port already in use
# - First server should continue running normally
```

**Terminal 3 (test both scenarios):**
```bash
# Test that only first server is accessible
curl http://localhost:8080/  # Should work (first server)

# Check running processes
ps aux | grep webserv  # Should show only one webserv process

# Check port usage
netstat -tlnp | grep :8080  # Should show only one process using port 8080
# OR using ss command:
ss -tlnp | grep :8080
```

### Method 3: Different Configurations, Same Port

**Create two different config files:**

**Config 1 (`config/server1.conf`):**
```bash
cat > config/server1.conf << 'EOF'
server {
    listen 8080
    host localhost
    server_name webserv-main
    root www-main
    
    location / {
        allow_methods GET POST
        default index.html
    }
}
EOF
```

**Config 2 (`config/server2.conf`):**
```bash
cat > config/server2.conf << 'EOF'
server {
    listen 8080
    host localhost
    server_name webserv-api
    root www-api
    
    location / {
        allow_methods GET POST PUT DELETE
        default index.html
    }
}
EOF
```

**Test launching both:**
```bash
# Terminal 1: Start first server
./webserv config/server1.conf
# Should succeed

# Terminal 2: Try to start second server
./webserv config/server2.conf
# Should fail with port binding error

# Terminal 3: Verify only first server works
curl http://localhost:8080/  # Should serve content from www-main (first server)
```

### Method 4: Testing with Different Host Same Port

**Edge case testing:**
```bash
cat > config/host-port-test.conf << 'EOF'
# Server 1 - localhost:8080
server {
    listen 8080
    host localhost
    server_name local-server
    root www-main
}

# Server 2 - 0.0.0.0:8080 (same port, different host)
server {
    listen 8080
    host 0.0.0.0
    server_name any-server
    root www-api
}
EOF

# Test this configuration
./webserv config/host-port-test.conf
# Behavior depends on implementation:
# - May allow both (different bind addresses)
# - May conflict (if using wildcard binding)
```

### Expected Server Behavior

**Proper Implementation Should:**

1. **Detect Conflicts Early:**
   - Validate configuration during parsing
   - Check for duplicate port bindings before socket creation
   - Display clear error messages

2. **Handle Socket Binding Errors:**
   ```cpp
   // Example error handling in C++
   if (bind(serverFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
       if (errno == EADDRINUSE) {
           std::cerr << "Error: Port " << port << " is already in use" << std::endl;
           return false;
       }
   }
   ```

3. **Graceful Failure:**
   - Exit cleanly on port conflicts
   - Don't leave partial server states
   - Clean up any successfully created sockets

4. **Clear Error Messages:**
   - Specify which port is conflicting
   - Indicate which server configuration failed
   - Provide actionable error information

### Testing Commands Summary

```bash
# Test 1: Same config file with duplicate ports
./webserv config/port-conflict-test.conf

# Test 2: Multiple instances same config
./webserv config/default.conf &  # Background process
./webserv config/default.conf    # Should fail

# Test 3: Different configs same port
./webserv config/server1.conf &  # Background
./webserv config/server2.conf    # Should fail

# Cleanup after tests
pkill webserv  # Kill all webserv processes
```

### Why This Test Matters

**Network Programming Fundamentals:**
- Tests understanding of socket binding behavior
- Verifies proper error handling for system-level failures
- Demonstrates knowledge of port exclusivity in TCP

**Server Reliability:**
- Ensures server fails gracefully rather than in undefined state
- Validates configuration parsing and validation
- Tests startup sequence robustness

**Real-World Scenarios:**
- Prevents accidental deployment conflicts
- Handles common configuration mistakes
- Provides clear debugging information for administrators

## 10. Stress Testing with Siege

### Overview

Siege is a powerful HTTP load testing and benchmarking utility. This section covers comprehensive stress testing to verify server stability, performance, and resource management.

**Key Requirements to Test:**
- ✅ Availability should be above 99.5% for simple GET requests
- ✅ No memory leaks (memory usage should not grow indefinitely)
- ✅ No hanging connections
- ✅ Server should handle siege indefinitely without restart

### Basic Siege Commands

**Test Setup:**
```bash
# Ensure your server is running
./webserv config/default.conf &
SERVER_PID=$!

# Verify server is responding
curl http://localhost:8080/
```

### 1. Basic Availability Testing

**Simple GET Request Test (99.5% availability requirement):**
```bash
# Test 1: Basic availability test
siege -b -t30s -c10 http://localhost:8080/

# Parameters explained:
# -b = benchmark mode (no delays)
# -t30s = run for 30 seconds
# -c10 = 10 concurrent connections
# Target: >99.5% availability
```

**Expected Output:**
```bash
Transactions:		     1234 hits
Availability:		   99.92 %    # Should be > 99.5%
Elapsed time:		   30.00 secs
Data transferred:	    0.12 MB
Response time:		    0.02 secs
Transaction rate:	   41.13 trans/sec
Throughput:		    0.00 MB/sec
Concurrency:		    0.82
Successful transactions:    1234
Failed transactions:	       1
Longest transaction:	    0.15
Shortest transaction:	    0.01
```

**Extended Availability Test:**
```bash
# Test 2: Longer duration test
siege -b -t2m -c20 http://localhost:8080/

# Test 3: Higher concurrency
siege -b -t1m -c50 http://localhost:8080/

# Test 4: Very high concurrency (stress test)
siege -b -t30s -c100 http://localhost:8080/
```

### 2. Memory Leak Detection

**Monitor Memory Usage During Testing:**

**Terminal 1 (Start monitoring):**
```bash
# First, get the server PID
SERVER_PID=$(pgrep webserv)
echo "Monitoring webserv PID: $SERVER_PID"

# Monitor server memory usage (shows PID, %CPU, %MEM, VSZ)
watch -n1 "ps aux | grep webserv | grep -v grep"

# Alternative: More detailed memory monitoring (replace $SERVER_PID with actual PID)
watch -n1 "ps -p $SERVER_PID -o pid,vsz,rss,pmem,comm"

# Alternative: If you know the PID (e.g., 12345), use it directly:
# watch -n1 "ps -p 12345 -o pid,vsz,rss,pmem,comm"

# VSZ = Virtual memory size
# RSS = Resident Set Size (physical memory)
# %MEM = Memory percentage
```

**Terminal 2 (Run siege tests):**
```bash
# Run extended siege test while monitoring memory
siege -b -t5m -c25 http://localhost:8080/

# Expected behavior:
# - Memory usage should stabilize after initial ramp-up
# - RSS should not continuously increase
# - Memory should not grow indefinitely over time
```

**Working Example (Copy-Paste Ready):**
```bash
# Terminal 1: Start monitoring (run this first)
SERVER_PID=$(pgrep webserv)
if [ -n "$SERVER_PID" ]; then
    echo "Monitoring webserv PID: $SERVER_PID"
    watch -n1 "echo 'PID: $SERVER_PID'; ps -p $SERVER_PID -o pid,vsz,rss,pmem,comm --no-headers"
else
    echo "Webserv not running! Start it first."
fi

# Terminal 2: Run siege while monitoring
siege -b -t2m -c25 http://localhost:8080/
```

**Memory Leak Test Script:**
```bash
# Create automated memory monitoring script
cat > memory_test.sh << 'EOF'
#!/bin/bash

SERVER_PID=$(pgrep webserv)
if [ -z "$SERVER_PID" ]; then
    echo "Webserv not running!"
    exit 1
fi

echo "Monitoring memory for webserv PID: $SERVER_PID"
echo "Time,VSZ(KB),RSS(KB),%MEM" > memory_log.csv

# Monitor for 10 minutes while running siege
for i in {1..600}; do
    MEMORY_INFO=$(ps -p $SERVER_PID -o vsz,rss,pmem --no-headers)
    TIMESTAMP=$(date '+%H:%M:%S')
    echo "$TIMESTAMP,$MEMORY_INFO" >> memory_log.csv
    sleep 1
done
EOF

chmod +x memory_test.sh

# Run memory monitoring in background
./memory_test.sh &
MONITOR_PID=$!

# Run siege test
siege -b -t10m -c30 http://localhost:8080/

# Stop monitoring
kill $MONITOR_PID

# Analyze results
echo "Memory usage analysis:"
tail -20 memory_log.csv
```

### 3. Connection Handling Tests

**Understanding Connection States:**

Before testing, it's important to understand what different connection states mean:

- **LISTEN**: Server socket listening for connections (this is your webserver)
- **ESTABLISHED**: Active connections currently in use
- **TIME_WAIT**: Properly closed connections in cleanup phase (NORMAL - lasts 60-120 seconds)
- **CLOSE_WAIT**: Connections where client closed but server hasn't fully closed yet
- **FIN_WAIT**: Connections in process of closing

**Expected Results:**
- ✅ **During siege**: Many ESTABLISHED connections
- ✅ **After siege**: Many TIME_WAIT connections (normal cleanup)
- ✅ **Problem indicators**: Many persistent ESTABLISHED or CLOSE_WAIT connections long after siege stops

**Test for Hanging Connections:**

**Monitor Active Connections:**
```bash
# Terminal 1: Monitor connections with details
watch -n1 "echo 'Total: '$(netstat -an | grep :8080 | grep -v LISTEN | wc -l); echo 'ESTABLISHED: '$(netstat -an | grep :8080 | grep ESTABLISHED | wc -l); echo 'TIME_WAIT: '$(netstat -an | grep :8080 | grep TIME_WAIT | wc -l); echo 'CLOSE_WAIT: '$(netstat -an | grep :8080 | grep CLOSE_WAIT | wc -l)"

# Alternative using ss:
watch -n1 "ss -tn | grep :8080 | wc -l"
```

**Connection Stress Tests:**
```bash
# Test 1: Rapid connection cycling
siege -b -t2m -c50 http://localhost:8080/

# Test 2: Keep-alive connections
siege -b -t2m -c25 --header="Connection: keep-alive" http://localhost:8080/

# Test 3: Mixed request patterns
siege -b -t3m -c40 -f urls.txt

# Create urls.txt for mixed testing:
cat > urls.txt << 'EOF'
http://localhost:8080/
http://localhost:8080/test.html
http://localhost:8080/api/
http://localhost:8080/files/
http://localhost:8080/uploads/
EOF
```

### 4. Indefinite Running Tests

**Long-Duration Stability Test:**
```bash
# Test: Run siege indefinitely (use Ctrl+C to stop)
siege -b -c20 http://localhost:8080/

# Test: Scheduled intervals
siege -b -t10m -c25 http://localhost:8080/ && sleep 30 && \
siege -b -t10m -c25 http://localhost:8080/ && sleep 30 && \
siege -b -t10m -c25 http://localhost:8080/

# Test: Overnight stability (if needed for evaluation)
nohup siege -b -t8h -c15 http://localhost:8080/ > siege_overnight.log 2>&1 &
```

**Resource Monitoring for Long Tests:**
```bash
# Comprehensive monitoring script
cat > stress_monitor.sh << 'EOF'
#!/bin/bash

SERVER_PID=$(pgrep webserv)
LOG_FILE="stress_test_$(date +%Y%m%d_%H%M%S).log"

echo "Starting stress test monitoring - PID: $SERVER_PID" | tee $LOG_FILE
echo "Time,Connections,Memory(RSS),CPU%" | tee -a $LOG_FILE

while true; do
    TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')
    CONNECTIONS=$(ss -tn | grep :8080 | wc -l)
    MEMORY=$(ps -p $SERVER_PID -o rss --no-headers 2>/dev/null || echo "0")
    CPU=$(ps -p $SERVER_PID -o pcpu --no-headers 2>/dev/null || echo "0")
    
    echo "$TIMESTAMP,$CONNECTIONS,$MEMORY,$CPU" | tee -a $LOG_FILE
    sleep 10
done
EOF

chmod +x stress_monitor.sh

# Start monitoring
./stress_monitor.sh &
MONITOR_PID=$!

# Run long siege test
siege -b -t30m -c30 http://localhost:8080/

# Stop monitoring
kill $MONITOR_PID
```

### 5. Different Content Types Testing

**Test Various Endpoints:**
```bash
# Test static HTML files
siege -b -t2m -c25 http://localhost:8080/test.html

# Test API endpoints
siege -b -t2m -c25 http://localhost:8080/api/test.json

# Test file downloads
siege -b -t2m -c25 http://localhost:8080/files/file1.txt

# Test directory listings
siege -b -t2m -c25 http://localhost:8080/uploads/

# Test CSS/JS files
siege -b -t2m -c25 http://localhost:8080/css/style.css
```

**POST Request Stress Testing:**
```bash
# Create test data file
echo "POST test data for siege stress testing" > post_data.txt

# Test POST requests
siege -b -t2m -c20 --content-type="text/plain" --file=post_data.txt http://localhost:8080/upload/siege-test.txt

# Test mixed GET/POST
cat > mixed_urls.txt << 'EOF'
http://localhost:8080/ GET
http://localhost:8080/upload/siege1.txt POST <post_data.txt
http://localhost:8080/api/ GET
http://localhost:8080/upload/siege2.txt POST <post_data.txt
EOF

siege -b -t3m -c30 -f mixed_urls.txt
```

### 6. Error Handling Stress Tests

**Test Server Resilience:**
```bash
# Test 404 errors (should not affect availability)
siege -b -t2m -c25 http://localhost:8080/nonexistent-file.html

# Test mixed valid/invalid requests
cat > error_test_urls.txt << 'EOF'
http://localhost:8080/
http://localhost:8080/nonexistent1.html
http://localhost:8080/test.html
http://localhost:8080/fake/path/file.txt
http://localhost:8080/api/
http://localhost:8080/missing-api.json
EOF

siege -b -t3m -c30 -f error_test_urls.txt
```

### 7. Analysis and Validation

**Analyze Siege Results:**
```bash
# Key metrics to verify:
# 1. Availability > 99.5%
# 2. No failed transactions due to server errors
# 3. Consistent response times
# 4. No connection timeouts

# Example good result:
# Availability: 99.95%
# Failed transactions: 0
# Response time: <1 second average
```

**Validate Server Health:**
```bash
# Check server is still responsive after stress test
curl -I http://localhost:8080/

# Check for memory leaks (RSS should be stable)
ps aux | grep webserv | grep -v grep

# Check for hanging connections
netstat -an | grep :8080 | grep -v LISTEN

# What you'll see (this is NORMAL):
# - Many TIME_WAIT connections (normal TCP cleanup, will disappear in 60-120 seconds)
# - Possibly some CLOSE_WAIT connections (should be minimal)
# - ESTABLISHED connections should be 0 or very few after siege stops

# Count connection types:
echo "TIME_WAIT connections: $(netstat -an | grep :8080 | grep TIME_WAIT | wc -l)"
echo "ESTABLISHED connections: $(netstat -an | grep :8080 | grep ESTABLISHED | wc -l)"
echo "CLOSE_WAIT connections: $(netstat -an | grep :8080 | grep CLOSE_WAIT | wc -l)"

# Verify server can handle new requests normally
curl -v http://localhost:8080/test.html
```

### 8. Complete Stress Test Suite

**Comprehensive Test Script:**
```bash
cat > complete_stress_test.sh << 'EOF'
#!/bin/bash

echo "=== Webserv Stress Test Suite ==="

# Test 1: Basic availability (99.5% requirement)
echo "Test 1: Basic availability test..."
siege -b -t1m -c20 http://localhost:8080/ > test1_results.txt
AVAILABILITY=$(grep "Availability" test1_results.txt | awk '{print $2}' | sed 's/%//')
echo "Availability: $AVAILABILITY% (Required: >99.5%)"

# Test 2: Memory stability
echo "Test 2: Memory leak test..."
SERVER_PID=$(pgrep webserv)
INITIAL_MEMORY=$(ps -p $SERVER_PID -o rss --no-headers)
siege -b -t3m -c25 http://localhost:8080/ > test2_results.txt
FINAL_MEMORY=$(ps -p $SERVER_PID -o rss --no-headers)
echo "Memory usage - Initial: ${INITIAL_MEMORY}KB, Final: ${FINAL_MEMORY}KB"

# Test 3: Connection handling
echo "Test 3: Connection stability test..."
INITIAL_CONNECTIONS=$(netstat -an | grep :8080 | wc -l)
siege -b -t2m -c30 http://localhost:8080/ > test3_results.txt
sleep 5  # Allow connections to close
FINAL_CONNECTIONS=$(netstat -an | grep :8080 | wc -l)
echo "Connections - Initial: $INITIAL_CONNECTIONS, Final: $FINAL_CONNECTIONS"

# Test 4: Extended duration
echo "Test 4: Extended duration test..."
siege -b -t5m -c20 http://localhost:8080/ > test4_results.txt

echo "=== Stress Test Complete ==="
echo "Check test*_results.txt files for detailed results"
EOF

chmod +x complete_stress_test.sh
./complete_stress_test.sh
```

### Expected Results for Evaluation

**✅ Success Criteria:**
- **Availability**: >99.5% in all tests
- **Memory Usage**: Stable RSS, no continuous growth
- **Connections**: No hanging connections after tests
- **Server Stability**: Responds normally after all stress tests
- **Duration**: Can run indefinitely without restart
- **Resource Management**: CPU and memory usage reasonable

**🔍 Key Siege Flags Reference:**
- `-b`: Benchmark mode (no delays between requests)
- `-t`: Duration (30s, 2m, 1h)
- `-c`: Concurrent users/connections
- `-f`: URL file for multiple endpoints
- `--content-type`: Set Content-Type header
- `--file`: File to POST/PUT

### 9. Connection State Troubleshooting

**What's Normal vs Problematic:**

**✅ NORMAL Behavior After Siege:**
```bash
# Many TIME_WAIT connections (100+ is fine)
netstat -an | grep :8080 | grep TIME_WAIT | wc -l
# Output: 50-200+ connections

# Few or zero ESTABLISHED connections
netstat -an | grep :8080 | grep ESTABLISHED | wc -l  
# Output: 0-5 connections

# Minimal CLOSE_WAIT connections
netstat -an | grep :8080 | grep CLOSE_WAIT | wc -l
# Output: 0-2 connections
```

**❌ PROBLEMATIC Behavior (Real Hanging Connections):**
```bash
# Many persistent ESTABLISHED connections (5+ minutes after siege stops)
netstat -an | grep :8080 | grep ESTABLISHED | wc -l
# Output: 20+ connections (PROBLEM)

# Many persistent CLOSE_WAIT connections
netstat -an | grep :8080 | grep CLOSE_WAIT | wc -l
# Output: 10+ connections (PROBLEM)
```

**Real-Time Monitoring Script:**
```bash
# Monitor connection states in real-time
cat > connection_monitor.sh << 'EOF'
#!/bin/bash
while true; do
    echo "=== $(date) ==="
    echo "ESTABLISHED: $(netstat -an | grep :8080 | grep ESTABLISHED | wc -l)"
    echo "TIME_WAIT: $(netstat -an | grep :8080 | grep TIME_WAIT | wc -l)"
    echo "CLOSE_WAIT: $(netstat -an | grep :8080 | grep CLOSE_WAIT | wc -l)"
    echo "TOTAL: $(netstat -an | grep :8080 | grep -v LISTEN | wc -l)"
    echo ""
    sleep 5
done
EOF

chmod +x connection_monitor.sh
./connection_monitor.sh
```

**Wait and Recheck Test:**
```bash
# Run siege, then wait and check for hanging connections
siege -b -t1m -c25 http://localhost:8080/

echo "Checking connections immediately after siege..."
netstat -an | grep :8080 | grep -v LISTEN | wc -l

echo "Waiting 2 minutes for TIME_WAIT cleanup..."
sleep 120

echo "Checking connections after 2 minutes..."
REMAINING=$(netstat -an | grep :8080 | grep -v LISTEN | grep -v TIME_WAIT | wc -l)
echo "Non-TIME_WAIT connections remaining: $REMAINING"

if [ $REMAINING -le 5 ]; then
    echo "✅ PASS: No hanging connections detected"
else
    echo "❌ POTENTIAL ISSUE: $REMAINING connections still active"
    netstat -an | grep :8080 | grep -v LISTEN | grep -v TIME_WAIT
fi
```

## 11. Client Body Size Limit Testing

### Overview

This section provides comprehensive testing for client body size limits to verify your server correctly enforces `client_max_body_size` settings at both global and location levels.

**Key Requirements to Test:**
- ✅ Global body size limits are enforced
- ✅ Location-specific limits override global limits
- ✅ Requests exceeding limits return HTTP 413 "Payload Too Large"
- ✅ Requests within limits are processed normally
- ✅ Custom error pages are displayed for 413 errors

### 1. Quick Testing Commands

**Basic Tests (Using Current Configuration):**
```bash
# Test 1: Small body (should work)
curl -v -X POST -H "Content-Type: text/plain" --data "Small body content" http://localhost:8080/

# Test 2: Large body using inline data (should fail if limit is small)
curl -v -X POST -H "Content-Type: text/plain" --data "$(printf 'A%.0s' {1..5000})" http://localhost:8080/

# Test 3: Check response code only
curl -s -o /dev/null -w "Response: %{http_code}\n" -X POST --data "$(printf 'B%.0s' {1..10000})" http://localhost:8080/

# Test 4: Test different endpoints
curl -s -o /dev/null -w "API endpoint: %{http_code}\n" -X POST --data "$(printf 'C%.0s' {1..2000})" http://localhost:8080/api/
curl -s -o /dev/null -w "Upload endpoint: %{http_code}\n" -X POST --data "$(printf 'D%.0s' {1..2000})" http://localhost:8080/upload/test.txt
```

### 2. File-Based Testing

**Create Test Files:**
```bash
# Create files of different sizes
echo "Small content" > small.txt                           # ~13 bytes
head -c 1000 /dev/zero | tr '\0' 'A' > medium.txt           # 1KB
head -c 5000 /dev/zero | tr '\0' 'B' > large.txt            # 5KB
head -c 50000 /dev/zero | tr '\0' 'C' > huge.txt            # 50KB

# Check file sizes
ls -la *.txt
```

**Test with Files:**
```bash
# Test each file size
echo "Testing small file:"
curl -v -X POST -H "Content-Type: text/plain" --data-binary @small.txt http://localhost:8080/

echo "Testing medium file:"
curl -v -X POST -H "Content-Type: text/plain" --data-binary @medium.txt http://localhost:8080/

echo "Testing large file:"
curl -v -X POST -H "Content-Type: text/plain" --data-binary @large.txt http://localhost:8080/

echo "Testing huge file:"
curl -v -X POST -H "Content-Type: text/plain" --data-binary @huge.txt http://localhost:8080/

# Cleanup
rm -f small.txt medium.txt large.txt huge.txt
```

### 3. Automated Test Script

**Create Comprehensive Test Script:**
```bash
cat > body_limit_test.sh << 'EOF'
#!/bin/bash

echo "=== Client Body Size Limit Testing ==="

# Function to test body size with specific data
test_body_size() {
    local size=$1
    local endpoint=$2
    local description=$3
    
    echo -n "Testing $description ($size bytes): "
    response=$(curl -s -o /dev/null -w "%{http_code}" -X POST \
               -H "Content-Type: text/plain" \
               --data "$(head -c $size /dev/zero | tr '\0' 'A')" \
               "http://localhost:8080$endpoint")
    
    if [ "$response" = "200" ] || [ "$response" = "201" ]; then
        echo "✅ SUCCESS ($response)"
    elif [ "$response" = "413" ]; then
        echo "🚫 REJECTED ($response - Payload Too Large)"
    else
        echo "❓ UNEXPECTED ($response)"
    fi
}

# Test various sizes on root endpoint
echo "Root endpoint tests:"
test_body_size 100 "/" "tiny body"
test_body_size 500 "/" "small body"
test_body_size 1000 "/" "medium body"
test_body_size 5000 "/" "large body"
test_body_size 10000 "/" "huge body"

echo ""
echo "API endpoint tests:"
test_body_size 100 "/api/" "tiny API data"
test_body_size 1000 "/api/" "medium API data"
test_body_size 5000 "/api/" "large API data"

echo ""
echo "Upload endpoint tests:"
test_body_size 100 "/upload/test1.txt" "tiny upload"
test_body_size 1000 "/upload/test2.txt" "medium upload"
test_body_size 5000 "/upload/test3.txt" "large upload"

echo ""
echo "=== Test Complete ==="
echo "Expected results:"
echo "✅ SUCCESS: Request within configured limits"
echo "🚫 REJECTED: Request exceeds configured limits (HTTP 413)"
EOF

chmod +x body_limit_test.sh
./body_limit_test.sh
```

### 4. Edge Case Testing

**Boundary Testing:**
```bash
# If you know your limit is 1KB (1024 bytes), test around the boundary
test_boundary() {
    local limit=1024  # Adjust this to your actual limit
    
    echo "Testing boundary cases for ${limit} byte limit:"
    
    # Just under limit
    curl -s -w "Under limit ($(($limit-1)) bytes): %{http_code}\n" -o /dev/null \
         -X POST --data "$(head -c $(($limit-1)) /dev/zero | tr '\0' 'A')" http://localhost:8080/
    
    # Exactly at limit
    curl -s -w "At limit ($limit bytes): %{http_code}\n" -o /dev/null \
         -X POST --data "$(head -c $limit /dev/zero | tr '\0' 'B')" http://localhost:8080/
    
    # Just over limit
    curl -s -w "Over limit ($(($limit+1)) bytes): %{http_code}\n" -o /dev/null \
         -X POST --data "$(head -c $(($limit+1)) /dev/zero | tr '\0' 'C')" http://localhost:8080/
}

test_boundary
```

### 5. Different Content Types Testing

**Test Various Content Types:**
```bash
# Test with different Content-Type headers
test_content_types() {
    local data="$(printf 'X%.0s' {1..2000})"  # 2KB of data
    
    echo "Testing different content types:"
    
    curl -s -w "text/plain: %{http_code}\n" -o /dev/null \
         -X POST -H "Content-Type: text/plain" --data "$data" http://localhost:8080/
    
    curl -s -w "application/json: %{http_code}\n" -o /dev/null \
         -X POST -H "Content-Type: application/json" --data "$data" http://localhost:8080/
    
    curl -s -w "application/octet-stream: %{http_code}\n" -o /dev/null \
         -X POST -H "Content-Type: application/octet-stream" --data "$data" http://localhost:8080/
    
    curl -s -w "text/html: %{http_code}\n" -o /dev/null \
         -X POST -H "Content-Type: text/html" --data "$data" http://localhost:8080/
}

test_content_types
```

### 6. Server Stability Testing

**Test Server Resilience:**
```bash
# Test multiple large requests to ensure server doesn't crash
echo "Testing server stability with multiple large requests:"

for i in {1..10}; do
    response=$(curl -s -o /dev/null -w "%{http_code}" \
               -X POST --data "$(head -c 10000 /dev/zero | tr '\0' 'Z')" \
               http://localhost:8080/)
    echo "Request $i: $response"
done

# Verify server is still responsive
echo "Checking server responsiveness:"
curl -s -w "GET request after large POSTs: %{http_code}\n" -o /dev/null http://localhost:8080/
```

### 7. Error Response Analysis

**Analyze 413 Error Responses:**
```bash
# Test and capture full error response
echo "Analyzing 413 error response:"

# Create large data and capture response
large_data="$(head -c 10000 /dev/zero | tr '\0' 'E')"
curl -v -X POST -H "Content-Type: text/plain" --data "$large_data" http://localhost:8080/ 2>&1 | tee error_response.log

# Check if custom error page is served
echo "Checking for custom error page content:"
curl -X POST --data "$large_data" http://localhost:8080/ 2>/dev/null | grep -i "413\|payload\|too large\|limit"

# Clean up
rm -f error_response.log
```

### 8. Configuration-Specific Testing

**Create Test Configuration for Body Limits:**
```bash
cat > config/test-body-limits.conf << 'EOF'
server {
    listen 8080
    host localhost
    server_name webserv-test
    root www-main
    client_max_body_size 2K  # Global limit: 2KB
    
    error_page 413 /error/413.html
    
    location / {
        allow_methods GET POST
        default index.html
    }
    
    location /small {
        allow_methods POST
        client_max_body_size 500  # 500 bytes
    }
    
    location /large {
        allow_methods POST
        client_max_body_size 10K  # 10KB
    }
}
EOF

echo "Test the configuration above with:"
echo "./webserv config/test-body-limits.conf"
echo ""
echo "Then run these tests:"
echo "curl -X POST --data \"\$(printf 'A%.0s' {1..1000})\" http://localhost:8080/        # Should work (within 2K global)"
echo "curl -X POST --data \"\$(printf 'B%.0s' {1..1000})\" http://localhost:8080/small   # Should fail (exceeds 500B location)"
echo "curl -X POST --data \"\$(printf 'C%.0s' {1..5000})\" http://localhost:8080/large   # Should work (within 10K location)"
```

### 9. Expected Results Guide

**Understanding Response Codes:**
- **200 OK**: Request processed successfully (body within limits)
- **201 Created**: Upload/creation successful (body within limits)
- **413 Payload Too Large**: Request body exceeds configured limit
- **400 Bad Request**: Malformed request (check Content-Length header)
- **500 Internal Server Error**: Server error processing request (potential bug)

**What to Look For:**
- ✅ **Consistent 413 responses** for oversized requests
- ✅ **Successful processing** for requests within limits
- ✅ **Location-specific limits** override global limits correctly
- ✅ **Custom error pages** displayed for 413 errors
- ✅ **Server stability** after limit violations

**Troubleshooting:**
- **No 413 errors**: Body size limits may not be implemented
- **Server crashes**: Memory handling issues with large requests
- **Wrong limits applied**: Location parsing or inheritance issues
- **Generic error pages**: Custom error page configuration problems

---

*This completes the comprehensive client body size limit testing section for evaluation purposes.*
