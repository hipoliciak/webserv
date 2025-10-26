# Webserver Copilot Instructions

## Project Overview
HTTP/1.1 server implementation in C++98 with multi-server support, non-blocking I/O via poll(), CGI execution, and file uploads. The architecture follows a single-threaded event loop with asynchronous CGI processing.

## Core Architecture

### Component Hierarchy
- **Server**: Main orchestrator managing multiple server instances, client connections, and poll() event loop
- **Config**: Nginx-style configuration parser supporting multi-server setups with location blocks
- **HttpRequest/HttpResponse**: HTTP protocol handlers with chunked encoding and multipart form support
- **CGI**: Asynchronous CGI execution with process queuing (Python, PHP, Bash)
- **Client**: Connection state management with buffered I/O

### Key Data Flows
1. **Request Processing**: `poll()` → `handleClientRead()` → `HttpRequest::parse()` → route resolution via `getMatchingLocation()` → response generation
2. **CGI Execution**: Request queuing → `startAsyncCGI()` → fork/exec with pipes → `handleCgiCompletion()` on poll() readiness
3. **File Uploads**: Multipart parsing → temporary file creation → `saveUploadedFile()` to configured upload paths

## Critical Patterns

### Configuration System
Configuration uses Nginx-style blocks parsed in `Config.cpp`. Server blocks define ports/hosts, location blocks define route-specific behavior:
```cpp
// Key structure: ServerConfig contains vector<LocationConfig>
LocationConfig getMatchingLocation(const std::string& uri, const ServerConfig& serverConfig);
// Regex locations use ~ prefix, exact matches prioritized over patterns
```

### Non-Blocking I/O Management
All I/O uses poll() with careful state tracking:
```cpp
std::map<int, Client> _clients;           // Active connections
std::map<int, std::string> _pendingWrites; // Buffered responses
std::map<int, CgiProcess> _cgiProcesses;   // Async CGI state
```

### CGI Process Management
Sequential CGI processing with queuing:
- POST bodies written to temp files for large requests
- Environment variables set per CGI spec (REQUEST_METHOD, QUERY_STRING, etc.)
- Child process cleanup via waitpid() on completion

## Development Workflows

### Building & Testing
```bash
make                    # Build with C++98 compliance
./webserv config/default.conf  # Single server on port 8080
./webserv config/multi_server.conf  # Multiple themed servers (8080-8083)

# Ubuntu tester workflow (run server in background)
pkill webserv                       # Kill any existing webserv processes first
./webserv config/ubuntu_tester.conf &  # Background server for testing
./ubuntu_tester http://localhost:8080  # Official compliance tests
```

### Configuration Testing
- `default.conf`: Full-featured server with all HTTP methods
- `multi_server.conf`: Demonstrates multiple server instances with different themes
- `ubuntu_tester.conf`: Official test suite compatibility with specific routing requirements

### CGI Development
CGI scripts in `www-*/cgi-bin/` with extensions mapped in config:
- `.py` files use `/usr/bin/python3`
- `.sh` files use `/bin/bash`  
- `.php` files use `/usr/bin/php-cgi`

Environment variables automatically set per CGI/1.1 spec. Test with curl:
```bash
curl http://localhost:8080/cgi-bin/test.py
curl -X POST -d "data=test" http://localhost:8080/cgi-bin/test.py
```

## Error Handling Conventions

### HTTP Status Codes
Defined as constants in `webserv.hpp` (HTTP_OK, HTTP_NOT_FOUND, etc.). Custom error pages configured per server block and served from `error/` directories.

### Resource Management
- RAII for file descriptors and socket cleanup
- Graceful shutdown on SIGINT with active connection draining
- Temporary file cleanup in `/tmp/webserv_*` pattern

## File Organization

### Source Structure
- `include/`: Headers with forward declarations in `webserv.hpp`
- `src/`: Implementation files, one class per file
- `config/`: Server configuration files (Nginx-style syntax)
- `www-*/`: Multiple document roots for different server themes

### Testing Infrastructure
- `ubuntu_tester`: Official 42 compliance tester
- `YoupiBanane/`: Required directory structure for ubuntu_tester
- Multiple `www-*` directories demonstrate different server configurations

## Common Pitfalls

### Ubuntu Tester Requirements
The `./ubuntu_tester` requires exact configuration compliance:
- `/` must answer GET requests ONLY
- `/put_test/*` must answer PUT requests and save to configured upload directory
- Files with `.bla` extension must answer POST by calling `ubuntu_cgi_tester` executable
- `/post_body` must answer POST with max body size of 100 bytes
- `/directory/` must serve from `YoupiBanane/` with `youpi.bad_extension` as default file

The `ubuntu_cgi_tester` is an executable provided by the ubuntu_tester that interprets `.bla` files. It processes POST requests as CGI scripts, even with large bodies (equal or more than 100MB). The output is a simple toUppercase transformation of the input data. The server must handle this correctly without corrupting the data pipeline.

All tests performed by `ubuntu_tester`:

Test GET http://localhost:8080/
Test POST http://localhost:8080/ with a size of 0
Test HEAD http://localhost:8080/
Test GET http://localhost:8080/directory
Test GET http://localhost:8080/directory/youpi.bad_extension
Test GET http://localhost:8080/directory/youpi.bla
Test GET Expected 404 on http://localhost:8080/directory/oulalala
Test GET http://localhost:8080/directory/nop
Test GET http://localhost:8080/directory/nop/
Test GET http://localhost:8080/directory/nop/other.pouic
Test GET Expected 404 on http://localhost:8080/directory/nop/other.pouac
Test GET Expected 404 on http://localhost:8080/directory/Yeah
Test GET http://localhost:8080/directory/Yeah/not_happy.bad_extension
Test Put http://localhost:8080/put_test/file_should_exist_after with a size of 1000
Test Put http://localhost:8080/put_test/file_should_exist_after with a size of 10000000
Test POST http://localhost:8080/directory/youpi.bla with a size of 100000000
Test POST http://localhost:8080/directory/youpla.bla with a size of 100000000
Test POST http://localhost:8080/directory/youpi.bla with a size of 100000 with special headers
Test POST http://localhost:8080/post_body with a size of 0
Test POST http://localhost:8080/post_body with a size of 100
Test POST http://localhost:8080/post_body with a size of 200
Test POST http://localhost:8080/post_body with a size of 101
Test multiple workers(5) doing multiple times(15): GET on /
Test multiple workers(20) doing multiple times(5000): GET on /
Test multiple workers(128) doing multiple times(50): GET on /directory/nop
Test multiple workers(20) doing multiple times(5): Post on /directory/youpi.bla with size 100000000

**Required directory structure (already provided in repo):**
```
YoupiBanane/
├── youpi.bad_extension
├── youpi.bla
├── nop/
│   ├── youpi.bad_extension
│   └── other.pouic
└── Yeah/
    └── not_happy.bad_extension
```

### C++98 Compliance
- No auto, nullptr, or C++11 features
- Use std::string.c_str() for C API calls
- Prefer std::map and std::vector over newer containers

### Configuration Parsing
- Location regex patterns require `~` prefix
- Method restrictions apply at location level, not globally
- Body size limits can be set per location with client_max_body_size

### I/O Multiplexing Architecture
Single `poll()` call handles all file descriptors in main event loop:
- Server sockets monitored with POLLIN for new connections
- Client sockets tracked for read/write events
- Non-blocking I/O prevents server blocking on individual clients
- Connection state managed via fd-to-Client mapping

### Stress Testing Expectations
- Availability must exceed 99.5% under siege testing
- Memory usage should stabilize (no continuous growth indicating leaks)
- TIME_WAIT connections after load tests are normal TCP cleanup
- Server must handle indefinite siege runs without restart

### Common Implementation Issues
- **Temp File Lifecycle**: Never delete temporary files until CGI process completes reading them
- **Log Output Separation**: All logs must use stderr; stdout contamination breaks HTTP responses
- **CGI Queue Order**: Maintain proper sequence: create temp file → start CGI → wait for completion → cleanup
- **Large Request Handling**: ubuntu_cgi_tester handles 100MB inputs correctly; server must not corrupt the data pipeline

### CGI Implementation
- Environment variables must follow CGI/1.1 spec exactly
- POST body handling requires careful Content-Length management
- Process cleanup essential to prevent zombie processes
- **Critical**: Temporary files must NOT be cleaned up until CGI process completes
- **Critical**: Server logs must go to stderr, never stdout (prevents HTTP header corruption)
- **Critical**: CGI queue processing must maintain proper file descriptor lifecycle