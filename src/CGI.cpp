#include "../include/CGI.hpp"
#include "../include/HttpRequest.hpp"
#include "../include/Utils.hpp"

CGI::CGI() {
    setupCommonEnvVars();
}

CGI::CGI(const std::string& scriptPath, const std::string& interpreter) 
    : _scriptPath(scriptPath), _interpreter(interpreter) {
    _scriptDir = extractDirectory(scriptPath);
    setupCommonEnvVars();
}

CGI::~CGI() {
}

void CGI::setScriptPath(const std::string& path) {
    _scriptPath = path;
    _scriptDir = extractDirectory(path);
}

void CGI::setInterpreter(const std::string& interpreter) {
    _interpreter = interpreter;
}

void CGI::setBody(const std::string& body) {
    _body = body;
}

void CGI::setEnvironmentVariable(const std::string& key, const std::string& value) {
    _envVars[key] = value;
}

void CGI::setupEnvironment(const HttpRequest& request, const std::string& serverName, int serverPort) {
    // Required CGI environment variables according to RFC 3875
    _envVars["REQUEST_METHOD"] = request.getMethod();
    _envVars["REQUEST_URI"] = request.getUri();
    _envVars["QUERY_STRING"] = request.getUri().find('?') != std::string::npos ? 
                              request.getUri().substr(request.getUri().find('?') + 1) : "";
    _envVars["CONTENT_TYPE"] = request.getHeader("content-type");
    _envVars["CONTENT_LENGTH"] = Utils::sizeToString(request.getBody().length());
    _envVars["SERVER_NAME"] = serverName;
    _envVars["SERVER_PORT"] = Utils::intToString(serverPort);
    _envVars["SERVER_PROTOCOL"] = request.getVersion();
    _envVars["GATEWAY_INTERFACE"] = "CGI/1.1";
    _envVars["SCRIPT_NAME"] = extractFilename(_scriptPath);
    _envVars["SCRIPT_FILENAME"] = _scriptPath;
    _envVars["PATH_INFO"] = "";
    _envVars["PATH_TRANSLATED"] = "";
    _envVars["REMOTE_ADDR"] = "127.0.0.1"; // TODO: Get actual client IP
    _envVars["REMOTE_HOST"] = "";
    _envVars["AUTH_TYPE"] = "";
    _envVars["REMOTE_USER"] = "";
    _envVars["REMOTE_IDENT"] = "";
    
    // HTTP headers as environment variables
    const std::map<std::string, std::string>& headers = request.getHeaders();
    for (std::map<std::string, std::string>::const_iterator it = headers.begin(); 
         it != headers.end(); ++it) {
        std::string headerName = "HTTP_" + Utils::toUpper(it->first);
        std::replace(headerName.begin(), headerName.end(), '-', '_');
        _envVars[headerName] = it->second;
    }
}

char** CGI::createEnvArray() const {
    char** envArray = new char*[_envVars.size() + 1];
    size_t i = 0;
    
    for (std::map<std::string, std::string>::const_iterator it = _envVars.begin(); 
         it != _envVars.end(); ++it, ++i) {
        std::string envVar = it->first + "=" + it->second;
        envArray[i] = new char[envVar.length() + 1];
        strcpy(envArray[i], envVar.c_str());
    }
    
    envArray[i] = NULL;
    return envArray;
}

void CGI::freeEnvArray(char** envArray) const {
    if (!envArray) return;
    
    for (size_t i = 0; envArray[i] != NULL; ++i) {
        delete[] envArray[i];
    }
    delete[] envArray;
}

std::string CGI::execute() {
    if (!isValidScript()) {
        Utils::logError("Invalid CGI script: " + _scriptPath);
        return "";
    }
    
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        Utils::logError("Failed to create pipe for CGI");
        return "";
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        Utils::logError("Failed to fork for CGI execution");
        close(pipefd[0]);
        close(pipefd[1]);
        return "";
    }
    
    if (pid == 0) {
        // Child process
        close(pipefd[0]); // Close read end
        
        // Redirect stdout to pipe
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            Utils::logError("Failed to redirect stdout in CGI child");
            exit(1);
        }
        close(pipefd[1]);
        
        // Change directory to script directory
        if (chdir(_scriptDir.c_str()) == -1) {
            Utils::logError("Failed to change directory for CGI execution");
            exit(1);
        }
        
        // Set up environment
        char** envArray = createEnvArray();
        
        // Execute the script
        if (_interpreter.empty()) {
            // Try to execute script directly
            char* args[] = { 
                const_cast<char*>(_scriptPath.c_str()), 
                NULL 
            };
            execve(_scriptPath.c_str(), args, envArray);
        } else {
            // Use interpreter
            char* args[] = { 
                const_cast<char*>(_interpreter.c_str()), 
                const_cast<char*>(_scriptPath.c_str()), 
                NULL 
            };
            execve(_interpreter.c_str(), args, envArray);
        }
        
        // If we reach here, exec failed
        Utils::logError("Failed to execute CGI script: " + _scriptPath);
        freeEnvArray(envArray);
        exit(1);
    } else {
        // Parent process
        close(pipefd[1]); // Close write end
        
        // Send body to CGI if needed (for POST requests)
        if (!_body.empty()) {
            // We should use another pipe for stdin, but for simplicity we'll skip this for now
            // TODO: Implement proper stdin handling for POST data
        }
        
        // Read output from CGI
        std::string output;
        char buffer[BUFFER_SIZE];
        ssize_t bytesRead;
        
        while ((bytesRead = read(pipefd[0], buffer, BUFFER_SIZE - 1)) > 0) {
            buffer[bytesRead] = '\0';
            output += buffer;
        }
        
        close(pipefd[0]);
        
        // Wait for child to finish
        int status;
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            Utils::logInfo("CGI script executed successfully");
            return output;
        } else {
            Utils::logError("CGI script execution failed");
            return "";
        }
    }
    
    return "";
}

bool CGI::isCGIScript(const std::string& path) const {
    std::string extension = Utils::getFileExtension(path);
    return extension == ".php" || extension == ".py" || extension == ".pl" || extension == ".sh";
}

std::string CGI::getScriptDirectory() const {
    return _scriptDir;
}

bool CGI::isValidScript() const {
    return Utils::fileExists(_scriptPath) && !_scriptPath.empty();
}

void CGI::setupCommonEnvVars() {
    _envVars["PATH"] = "/usr/local/bin:/usr/bin:/bin";
    _envVars["SERVER_SOFTWARE"] = "webserv/1.0";
}

std::string CGI::extractFilename(const std::string& path) const {
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos) {
        return path;
    }
    return path.substr(pos + 1);
}

std::string CGI::extractDirectory(const std::string& path) const {
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos) {
        return "./";
    }
    return path.substr(0, pos + 1);
}