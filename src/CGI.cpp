#include "../include/CGI.hpp"
#include "../include/HttpRequest.hpp"
#include "../include/Utils.hpp"
#include <fstream>
#include <sstream>
#include <unistd.h>

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

void CGI::setBodyFromFile(const std::string& filePath) {
    std::ifstream file(filePath.c_str(), std::ios::binary);
    if (!file) {
        Utils::logError("CGI: Failed to open body file: " + filePath);
        _body = "";
        return;
    }
    
    std::ostringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    _body = buffer.str();
    Utils::logInfo("CGI: Read " + Utils::sizeToString(_body.length()) + " bytes from body file: " + filePath);
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
	_envVars["CONTENT_LENGTH"] = Utils::sizeToString(_body.length());
    _envVars["SERVER_NAME"] = serverName;
    _envVars["SERVER_PORT"] = Utils::intToString(serverPort);
    _envVars["SERVER_PROTOCOL"] = request.getVersion();
    _envVars["GATEWAY_INTERFACE"] = "CGI/1.1";
    _envVars["SCRIPT_NAME"] = request.getUri();
    
    // Ensure SCRIPT_FILENAME is absolute path
    std::string absoluteScriptPath = _scriptPath;
    if (!absoluteScriptPath.empty() && absoluteScriptPath[0] != '/') {
        char* cwd = getcwd(NULL, 0);
        if (cwd) {
            absoluteScriptPath = std::string(cwd) + "/" + _scriptPath;
            free(cwd);
        }
    }
    _envVars["SCRIPT_FILENAME"] = absoluteScriptPath;
    _envVars["PATH_INFO"] = request.getUri();
    _envVars["PATH_TRANSLATED"] = "";
    _envVars["REMOTE_ADDR"] = "127.0.0.1";
    _envVars["REMOTE_HOST"] = "";
    _envVars["REDIRECT_STATUS"] = "200"; // Required for PHP CGI security
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