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
    _envVars["SCRIPT_NAME"] = request.getUri();
    _envVars["SCRIPT_FILENAME"] = _scriptPath;
    _envVars["PATH_INFO"] = request.getUri();
    _envVars["PATH_TRANSLATED"] = "";
    _envVars["REMOTE_ADDR"] = "127.0.0.1";
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
    char** envArray = NULL;
    try {
        envArray = new char*[_envVars.size() + 1];
        size_t i = 0;
        
        for (std::map<std::string, std::string>::const_iterator it = _envVars.begin(); 
             it != _envVars.end(); ++it, ++i) {
            std::string envVar = it->first + "=" + it->second;
            try {
                envArray[i] = new char[envVar.length() + 1];
                strcpy(envArray[i], envVar.c_str());
            } catch (const std::bad_alloc& e) {
                // Clean up previously allocated strings
                for (size_t j = 0; j < i; ++j) {
                    delete[] envArray[j];
                }
                delete[] envArray;
                Utils::logError("Memory allocation failed in createEnvArray");
                return NULL;
            }
        }
        
        envArray[i] = NULL;
        return envArray;
    } catch (const std::bad_alloc& e) {
        Utils::logError("Memory allocation failed in createEnvArray");
        return NULL;
    }
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
    
    int stdout_pipe[2];
    int stdin_pipe[2];
    
    if (pipe(stdout_pipe) == -1 || pipe(stdin_pipe) == -1) {
        Utils::logError("Failed to create pipes for CGI");
        return "";
    }
    
    // Set up environment before forking
    char** envArray = createEnvArray();
    if (!envArray) {
        Utils::logError("Failed to create environment array for CGI");
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        return "";
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        Utils::logError("Failed to fork for CGI execution");
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        freeEnvArray(envArray);
        return "";
    }
    
    if (pid == 0) {
        // Child process
        close(stdout_pipe[0]);  // Close read end of stdout pipe
        close(stdin_pipe[1]);   // Close write end of stdin pipe
        
        // Redirect stdout to pipe
        if (dup2(stdout_pipe[1], STDOUT_FILENO) == -1) {
            Utils::logError("Failed to redirect stdout in CGI child");
            freeEnvArray(envArray);
            exit(1);
        }
        
        // Redirect stdin to pipe
        if (dup2(stdin_pipe[0], STDIN_FILENO) == -1) {
            Utils::logError("Failed to redirect stdin in CGI child");
            freeEnvArray(envArray);
            exit(1);
        }
        
        close(stdout_pipe[1]);
        close(stdin_pipe[0]);
        
        if (chdir(_scriptDir.c_str()) == -1) {
            Utils::logError("Failed to change directory for CGI execution");
            freeEnvArray(envArray);
            exit(1);
        }
        
        if (_interpreter.empty()) {
            char* args[] = { 
                const_cast<char*>(_scriptPath.c_str()), 
                NULL 
            };
            execve(_scriptPath.c_str(), args, envArray);
        } else {
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
        close(stdout_pipe[1]);  // Close write end of stdout pipe
        close(stdin_pipe[0]);   // Close read end of stdin pipe
        
        // Write POST body to stdin pipe if available
        if (!_body.empty()) {
            write(stdin_pipe[1], _body.c_str(), _body.length());
        }
        close(stdin_pipe[1]);  // Close stdin pipe to signal EOF
        
        std::string output;
        char buffer[BUFFER_SIZE];
        ssize_t bytesRead;
        
        while ((bytesRead = read(stdout_pipe[0], buffer, BUFFER_SIZE - 1)) > 0) {
            buffer[bytesRead] = '\0';
            output += buffer;
        }
        
        close(stdout_pipe[0]);
        
        int status;
        waitpid(pid, &status, 0);
        
        freeEnvArray(envArray);
        
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