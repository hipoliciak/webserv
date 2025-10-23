#ifndef CGI_HPP
#define CGI_HPP

#include "webserv.hpp"

class CGI {
private:
    std::string _scriptPath;
    std::string _scriptDir;
    std::string _interpreter;
    std::map<std::string, std::string> _envVars;
    std::string _body;
    
public:
    CGI();
    CGI(const std::string& scriptPath, const std::string& interpreter);
    ~CGI();
    
    // Setup methods
    void setScriptPath(const std::string& path);
    void setInterpreter(const std::string& interpreter);
    void setBody(const std::string& body);
    void setBodyFromFile(const std::string& filePath); // For memory-efficient large body handling
    void setEnvironmentVariable(const std::string& key, const std::string& value);
    
    // Environment setup
    void setupEnvironment(const HttpRequest& request, const std::string& serverName, int serverPort);
    char** createEnvArray() const;
    void freeEnvArray(char** envArray) const;
    
    // Execution
    std::string execute();
    bool isCGIScript(const std::string& path) const;
    
    // Utilities
    std::string getScriptDirectory() const;
    bool isValidScript() const;
    
private:
    void setupCommonEnvVars();
    std::string extractFilename(const std::string& path) const;
    std::string extractDirectory(const std::string& path) const;
};

#endif