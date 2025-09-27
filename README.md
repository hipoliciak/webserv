# webserv

HTTP server implementation written in C++98.

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

### Manual Testing

**Basic functionality test:**
```bash
# Start server in one terminal
./webserv

# Test in another terminal
curl http://localhost:8080/
curl -X POST -F "file=@README.md" http://localhost:8080/uploads/
curl http://localhost:8080/cgi-bin/test.py
```

### Official Testers

Two official testers are included to validate webserv project compliance.

**Ubuntu Tester (`ubuntu_tester`)**

Tests core HTTP server functionality with specific requirements:

```bash
# Terminal 1: Start server with test configuration
make clean && make
mkdir -p put_files
./webserv config/test.conf

# Terminal 2: Run the tester
./ubuntu_tester http://localhost:8080
# Follow interactive prompts, press Enter to continue through tests
```

**Requirements tested:**
- Root path (`/`) - GET requests only
- PUT test (`/put_test/*`) - PUT requests and file saving
- CGI with `.bla` extension - POST requests calling `cgi_test` executable
- POST body limit (`/post_body`) - POST requests with 100-byte limit
- Directory listing (`/directory/`) - GET requests from `YoupiBanane` directory

**Ubuntu CGI Tester (`ubuntu_cgi_tester`)**

CGI script executed by the webserver during POST requests to `.bla` files:

```bash
# Test CGI functionality
./webserv config/test.conf
curl -X POST http://localhost:8080/YoupiBanane/youpi.bla
```

**Quick validation checklist:**
```bash
# Verify setup before testing
ls -la cgi_test ubuntu_tester webserv config/test.conf YoupiBanane*/youpi.bla
./webserv config/test.conf &
curl -s -o /dev/null -w "%{http_code}" http://localhost:8080/  # Should return 200
pkill webserv
```

**Notes:**
- Always use `config/test.conf` for official testers
- Tester is interactive - follow prompts
- Server must be restarted with correct configuration before testing