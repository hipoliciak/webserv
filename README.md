# webserv

HTTP/1.1 server implementation written in C++98 with multi-server support, CGI execution, and non-blocking I/O.

## Features

- ✅ HTTP/1.1 protocol support with GET, POST, PUT, DELETE methods
- ✅ Multi-server configuration (multiple ports/hosts)  
- ✅ Non-blocking I/O using poll()
- ✅ CGI script execution (Python, Bash, PHP)
- ✅ File upload handling with size limits
- ✅ Directory listing and static file serving
- ✅ Custom error pages (404, 403, 500, etc.)
- ✅ Location-based routing with regex support
- ✅ HTTP redirections and method restrictions

## Building the Project

```bash
# Build the project
make

# Clean build files
make clean

# Full clean (removes executable)
make fclean

# Rebuild everything
make re
```

## Running the Server

```bash
# Run with default configuration
./webserv

# Run with custom configuration file
./webserv config/default.conf
```

## Testing

### Quick Configuration Tests

**1. Multi-Server Configuration**
```bash
# Start multi-server setup
./webserv config/multi_server.conf &

# Test each server
curl http://localhost:8080/  # Main server (GET only)
curl http://localhost:8081/  # API server (all methods)
curl http://localhost:8082/  # File server (GET, uploads)

# Expected: All return 200 with homepage content
```

**2. Method Restrictions**
```bash
curl http://localhost:8080/              # ✅ 200 - GET allowed
curl -X POST http://localhost:8080/      # ❌ 405 - POST forbidden
curl -X PUT http://localhost:8080/put_test/file.txt  # ✅ 200 - PUT allowed
```

**3. Directory Listing**
```bash
curl http://localhost:8080/directory/    # ✅ Directory listing from YoupiBanane/
curl http://localhost:8080/directory/youpi.bla  # ✅ 200 - File content
curl http://localhost:8080/files/        # ✅ File listing (file1.txt, file2.txt)
```

**4. CGI Scripts**
```bash
curl http://localhost:8080/cgi-bin/test.py    # ✅ Python CGI output
curl http://localhost:8080/cgi-bin/info.sh    # ✅ Bash CGI system info
```

**5. Error Handling**
```bash
curl http://localhost:8080/nonexistent   # ❌ 404 with custom error page
curl -X POST http://localhost:8080/      # ❌ 405 Method Not Allowed
```

**6. File Upload**
```bash
# Test file upload (requires form data)
curl -X POST -F "file=@README.md" http://localhost:8080/upload/
# ✅ 200 - File uploaded to uploads/ directory
```

### Ubuntu Tester

**Prerequisites Setup:**
```bash
# 1. Create required directory structure
mkdir -p YoupiBanane/nop YoupiBanane/Yeah
touch YoupiBanane/youpi.bad_extension YoupiBanane/youpi.bla
touch YoupiBanane/nop/youpi.bad_extension YoupiBanane/nop/other.pouic
touch YoupiBanane/Yeah/not_happy.bad_extension

# 2. Start server with ubuntu_tester configuration
./webserv config/ubuntu_tester.conf &

# 3. Run tester (interactive)
./ubuntu_tester http://localhost:8080
```

**Expected Test Results:**
- `GET /` → 200 (homepage)
- `POST /` → 405 (method not allowed) 
- `GET /directory` → 200 (directory listing)
- `GET /directory/youpi.bla` → 200 (file content)
- `GET /directory/nonexistent` → 404 (not found)

**Note:** Ubuntu tester requires specific configuration matching its requirements (GET-only root, directory mapping, etc.)

## Project Structure

```
webserv/
├── src/           # Source files (.cpp)
├── include/       # Header files (.hpp)  
├── config/        # Configuration files
├── www/           # Web content (HTML, CSS, JS, CGI)
├── YoupiBanane/   # Test directory for ubuntu_tester
├── uploads/       # File upload destination
├── Makefile       # Build configuration
├── ubuntu_tester  # Official test executable
└── README.md      # This file
```

## Configuration

Server behavior is controlled by configuration files in `config/`:
- `default.conf` - General website usage (all HTTP methods)
- `multi_server.conf` - Multiple servers on different ports  
- `ubuntu_tester.conf` - Specific configuration for ubuntu_tester requirements

Key directives: `listen`, `server_name`, `root`, `location`, `allow_methods`, `client_max_body_size`, `error_page`, `cgi_path`