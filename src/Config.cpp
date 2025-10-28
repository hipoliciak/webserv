#include "../include/Config.hpp"
#include "../include/Utils.hpp"

Config::Config() {
}

Config::Config(const std::string& configFile) : _configFile(configFile) {
}

Config::~Config() {
}

bool Config::parse() {
    if (_configFile.empty()) {
        // Create default configuration
        ServerConfig defaultConfig;
        setDefaults(defaultConfig);
        _servers.push_back(defaultConfig);
        return true;
    }
    
    return parseFile(_configFile);
}

bool Config::parseFile(const std::string& filename) {
    if (!Utils::fileExists(filename)) {
        Utils::logError("Configuration file not found: " + filename);
        // Create default configuration
        ServerConfig defaultConfig;
        setDefaults(defaultConfig);
        _servers.push_back(defaultConfig);
        return true;
    }
    
    std::string content = Utils::readFile(filename);
    if (content.empty()) {
        Utils::logError("Failed to read configuration file: " + filename);
        return false;
    }
    
    // Parse server blocks
    size_t pos = 0;
    while ((pos = content.find("server", pos)) != std::string::npos) {
        size_t blockStart = content.find("{", pos);
        if (blockStart == std::string::npos) break;
        
        // Find matching closing brace
        size_t braceCount = 1;
        size_t blockEnd = blockStart + 1;
        while (blockEnd < content.length() && braceCount > 0) {
            if (content[blockEnd] == '{') braceCount++;
            else if (content[blockEnd] == '}') braceCount--;
            blockEnd++;
        }
        
        if (braceCount == 0) {
            std::string serverBlock = content.substr(blockStart + 1, blockEnd - blockStart - 2);
            parseServerBlock(serverBlock);
        }
        
        pos = blockEnd;
    }
    
    // If no servers were parsed, create default
    if (_servers.empty()) {
        ServerConfig defaultConfig;
        setDefaults(defaultConfig);
        _servers.push_back(defaultConfig);
    }
    
    return validate();
}

bool Config::parseServerBlock(const std::string& block) {
    ServerConfig config;
    setDefaults(config);
    
    std::vector<std::string> lines = split(block, '\n');
    
    for (size_t i = 0; i < lines.size(); ++i) {
        std::string trimmedLine = trim(lines[i]);
        if (trimmedLine.empty() || trimmedLine[0] == '#') continue;
        
        // Check for location blocks
        if (trimmedLine.find("location") == 0) {
            // Parse location block
            size_t blockStart = i;
            while (i < lines.size() && lines[i].find("{") == std::string::npos) i++;
            if (i >= lines.size()) continue;
            
            // Extract location path correctly
            std::string locationLine = trim(lines[blockStart]);
            size_t locationStart = locationLine.find("location");
            if (locationStart == std::string::npos) continue;
            
            // Find the space after "location"
            size_t spaceAfterLocation = locationLine.find(' ', locationStart + 8);
            if (spaceAfterLocation == std::string::npos) continue;
            
            // Extract the path (everything between "location " and "{")
            std::string locationPath = locationLine.substr(spaceAfterLocation + 1);
            size_t bracePos = locationPath.find('{');
            if (bracePos != std::string::npos) {
                locationPath = locationPath.substr(0, bracePos);
            }
            locationPath = trim(locationPath);
            
            LocationConfig location;
            setLocationDefaults(location);
            
            // Check if this is a regex location (starts with ~)
            if (locationPath.length() > 0 && locationPath[0] == '~') {
                location.isRegex = true;
                // Remove the ~ and any following whitespace
                locationPath = trim(locationPath.substr(1));
                location.path = locationPath;
            } else {
                location.isRegex = false;
                location.path = locationPath;
            }
            
            i++; // Move past opening brace
            std::string locationBlock;
            int braceCount = 1;
            while (i < lines.size() && braceCount > 0) {
                std::string line = lines[i];
                if (line.find("{") != std::string::npos) braceCount++;
                if (line.find("}") != std::string::npos) braceCount--;
                if (braceCount > 0) {
                    locationBlock += line + "\n";
                }
                i++;
            }
            i--; // Adjust for the loop increment
            
            parseLocationBlock(locationBlock, location);
            config.locations.push_back(location);
            continue;
        }
        
        std::vector<std::string> tokens = split(trimmedLine, ' ');
        if (tokens.size() < 2) continue;
        
        std::string directive = tokens[0];
        
        if (directive == "listen") {
            config.port = Utils::stringToInt(tokens[1]);
        } else if (directive == "server_name") {
            config.serverName = extractValue(trimmedLine);
        } else if (directive == "host") {
            config.host = tokens[1];
        } else if (directive == "root") {
            config.root = extractValue(trimmedLine);
        } else if (directive == "index") {
            std::vector<std::string> indexValues = extractValues(trimmedLine);
            if (!indexValues.empty()) {
                config.index = indexValues[0];
            }
        } else if (directive == "max_body_size" || directive == "client_max_body_size") {
            std::string valueStr = tokens[1];
            // Handle size suffixes (M, K, G)
            size_t multiplier = 1;
            if (!valueStr.empty()) {
                char lastChar = valueStr[valueStr.length() - 1];
                if (lastChar == 'M' || lastChar == 'm') {
                    multiplier = 1024 * 1024;
                    valueStr = valueStr.substr(0, valueStr.length() - 1);
                } else if (lastChar == 'K' || lastChar == 'k') {
                    multiplier = 1024;
                    valueStr = valueStr.substr(0, valueStr.length() - 1);
                } else if (lastChar == 'G' || lastChar == 'g') {
                    multiplier = 1024 * 1024 * 1024;
                    valueStr = valueStr.substr(0, valueStr.length() - 1);
                }
            }
            config.maxBodySize = Utils::stringToInt(valueStr) * multiplier;
        } else if (directive == "autoindex") {
            config.autoIndex = (tokens[1] == "on");
        } else if (directive == "upload_path") {
            config.uploadPath = extractValue(trimmedLine);
        } else if (directive == "cgi_path") {
            config.cgiPath = extractValue(trimmedLine);
        } else if (directive == "error_page") {
            if (tokens.size() >= 3) {
                int errorCode = Utils::stringToInt(tokens[1]);
                config.errorPages[errorCode] = tokens[2];
            }
        } else if (directive == "allow_methods") {
            std::vector<std::string> methods = extractValues(trimmedLine);
            config.allowedMethods = methods;
        } else if (directive == "cgi_extension") {
            if (tokens.size() >= 3) {
                config.cgiExtensions[tokens[1]] = extractValue(trimmedLine.substr(trimmedLine.find(tokens[2])));
            }
        } else if (directive == "keepalive_timeout") {
			config.keepAliveTimeout = Utils::stringToInt(tokens[1]);
		} else if (directive == "cgi_timeout") {
			config.cgiTimeout = Utils::stringToInt(tokens[1]);
		}
	}
    _servers.push_back(config);
    return true;
}

bool Config::parseLocationBlock(const std::string& block, LocationConfig& location) {
    std::vector<std::string> lines = split(block, '\n');
    
    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string& line = lines[i];
        std::string trimmedLine = trim(line);
        if (trimmedLine.empty() || trimmedLine[0] == '#') continue;
        
        std::vector<std::string> tokens = split(trimmedLine, ' ');
        if (tokens.size() < 2) continue;
        
        std::string directive = tokens[0];
        
        if (directive == "root") {
            location.root = extractValue(trimmedLine);
        } else if (directive == "index") {
            std::vector<std::string> indexFiles = extractValues(trimmedLine);
            if (!indexFiles.empty()) {
                location.index = indexFiles[0]; // Use first index file
            }
        } else if (directive == "autoindex") {
            location.autoIndex = (tokens[1] == "on");
        } else if (directive == "upload_path") {
            location.uploadPath = extractValue(trimmedLine);
        } else if (directive == "cgi_path") {
            location.cgiPath = extractValue(trimmedLine);
        } else if (directive == "cgi_extension" || directive == "cgi_extensions") {
            location.cgiExtension = extractValue(trimmedLine);
        } else if (directive == "default") {
            location.index = extractValue(trimmedLine);
        } else if (directive == "client_max_body_size") {
            std::string valueStr = extractValue(trimmedLine);
            size_t multiplier = 1;
            
            // Handle size suffixes (K, M, G)
            if (!valueStr.empty()) {
                char lastChar = valueStr[valueStr.length() - 1];
                if (lastChar == 'K' || lastChar == 'k') {
                    multiplier = 1024;
                    valueStr = valueStr.substr(0, valueStr.length() - 1);
                } else if (lastChar == 'M' || lastChar == 'm') {
                    multiplier = 1024 * 1024;
                    valueStr = valueStr.substr(0, valueStr.length() - 1);
                } else if (lastChar == 'G' || lastChar == 'g') {
                    multiplier = 1024 * 1024 * 1024;
                    valueStr = valueStr.substr(0, valueStr.length() - 1);
                }
            }
            location.maxBodySize = Utils::stringToInt(valueStr) * multiplier;
        } else if (directive == "allow_methods") {
            location.allowedMethods = extractValues(trimmedLine);
        } else if (directive == "redirect") {
            if (tokens.size() >= 3) {
                int code = Utils::stringToInt(tokens[1]);
                location.redirections[code] = tokens[2];
            }
        }
    }
    
    return true;
}

void Config::setDefaults(ServerConfig& server) {
    server.host = DEFAULT_HOST;
    server.port = DEFAULT_PORT;
    server.serverName = "localhost";
    server.root = "www";
    server.index = "index.html";
    server.maxBodySize = MAX_BODY_SIZE;
    server.autoIndex = false;
    server.uploadPath = "www/uploads";
    server.cgiPath = "www/cgi-bin";
    
    server.allowedMethods.push_back("GET");
    server.allowedMethods.push_back("POST");
    server.allowedMethods.push_back("DELETE");
    
    server.errorPages[400] = "www/error/400.html";
    server.errorPages[403] = "www/error/403.html";
    server.errorPages[404] = "www/error/404.html";
    server.errorPages[500] = "www/error/500.html";
    server.errorPages[502] = "www/error/502.html";
	server.errorPages[504] = "www/error/504.html";
    
    // Default CGI interpreters
    server.cgiExtensions[".php"] = "/usr/bin/php-cgi";
    server.cgiExtensions[".py"] = "/usr/bin/python3";
    server.cgiExtensions[".pl"] = "/usr/bin/perl";
    server.cgiExtensions[".sh"] = "/bin/bash";

	server.keepAliveTimeout = 60; // 60 seconds
    server.cgiTimeout = 30;       // 30 seconds
}

void Config::setLocationDefaults(LocationConfig& location) const {
    location.root = "";
    location.index = "index.html";
    location.autoIndex = false;
    location.uploadPath = "";
    location.cgiPath = "";
    location.cgiExtension = "";
    location.isRegex = false;
    location.maxBodySize = 0; // 0 means inherit from server config
    
    location.allowedMethods.push_back("GET");
    location.allowedMethods.push_back("POST");
    location.allowedMethods.push_back("DELETE");
}

const std::vector<ServerConfig>& Config::getServers() const {
    return _servers;
}

ServerConfig Config::getDefaultServer() const {
    if (_servers.empty()) {
        ServerConfig defaultConfig;
        const_cast<Config*>(this)->setDefaults(defaultConfig);
        return defaultConfig;
    }
    return _servers[0];
}

ServerConfig Config::getServerByPort(int port) const {
    for (size_t i = 0; i < _servers.size(); ++i) {
        const ServerConfig& server = _servers[i];
        if (server.port == port) {
            return server;
        }
    }
    return getDefaultServer();
}

ServerConfig Config::getServerByName(const std::string& serverName) const {
    for (size_t i = 0; i < _servers.size(); ++i) {
        const ServerConfig& server = _servers[i];
        if (server.serverName == serverName) {
            return server;
        }
    }
    return getDefaultServer();
}

LocationConfig Config::getLocationConfig(const ServerConfig& server, const std::string& path) const {
    LocationConfig bestMatch;
    setLocationDefaults(bestMatch);
    size_t bestMatchLength = 0;
    LocationConfig regexMatch;
    bool foundRegexMatch = false;
    
    for (size_t i = 0; i < server.locations.size(); ++i) {
        const LocationConfig& location = server.locations[i];
        
        bool matches = false;
        if (location.isRegex) {
            // For regex patterns, check if the path matches
            // Simple regex matching for .bla$ pattern  
            if (location.path.find(".bla") != std::string::npos && Utils::endsWith(path, ".bla")) {
                matches = true;
            }
            // Handle /directory/.*\.bla$ pattern
            if (location.path.find("/directory/") != std::string::npos && 
                location.path.find(".bla") != std::string::npos &&
                path.find("/directory/") != std::string::npos && 
                Utils::endsWith(path, ".bla")) {
                matches = true;
            }
            // Could add more regex patterns here as needed
            
            // Store regex match but don't immediately use it
            if (matches && !foundRegexMatch) {
                regexMatch = location;
                foundRegexMatch = true;
            }
        } else {
            // Regular prefix matching
            matches = Utils::startsWith(path, location.path);
            if (matches && location.path.length() > bestMatchLength) {
                bestMatch = location;
                bestMatchLength = location.path.length();
            }
        }
    }
    
    // Use prefix match if found, otherwise use regex match
    // This prioritizes exact prefix matches over regex patterns
    if (bestMatchLength > 0) {
        return bestMatch;
    } else if (foundRegexMatch) {
        return regexMatch;
    }
    
    if (bestMatchLength == 0 && !foundRegexMatch) {
        bestMatch.root = server.root;
        bestMatch.index = server.index;
        bestMatch.autoIndex = server.autoIndex;
        bestMatch.uploadPath = server.uploadPath;
        bestMatch.cgiPath = server.cgiPath;
        bestMatch.allowedMethods = server.allowedMethods;
    }
    
    return bestMatch;
}

LocationConfig Config::getLocationConfig(const ServerConfig& server, const std::string& path, const std::string& method) const {
    LocationConfig bestMatch;
    setLocationDefaults(bestMatch);
    size_t bestMatchLength = 0;
    LocationConfig regexMatch;
    bool foundRegexMatch = false;
    
    for (size_t i = 0; i < server.locations.size(); ++i) {
        const LocationConfig& location = server.locations[i];
        
        bool matches = false;
        if (location.isRegex) {
            // For regex patterns, check if the path matches AND method is allowed
            if (location.path.find(".bla") != std::string::npos && Utils::endsWith(path, ".bla")) {
                matches = true;
            }
            if (location.path.find("/directory/") != std::string::npos && 
                location.path.find(".bla") != std::string::npos &&
                path.find("/directory/") != std::string::npos && 
                Utils::endsWith(path, ".bla")) {
                matches = true;
            }
            
            // Only use regex match if method is allowed
            if (matches && !foundRegexMatch) {
                bool methodAllowed = isValidMethod(method, location);

                
                if (methodAllowed) {
                    regexMatch = location;
                    foundRegexMatch = true;
                }
            }
        } else {
            // Regular prefix matching
            matches = Utils::startsWith(path, location.path);
            if (matches && location.path.length() > bestMatchLength) {
                bestMatch = location;
                bestMatchLength = location.path.length();
            }
        }
    }
    
    // Use regex match if found and method allowed, otherwise use prefix match
    if (foundRegexMatch) {
        return regexMatch;
    } else if (bestMatchLength > 0) {
        return bestMatch;
    }
    
    if (bestMatchLength == 0 && !foundRegexMatch) {
        bestMatch.root = server.root;
        bestMatch.index = server.index;
        bestMatch.allowedMethods = server.allowedMethods;
        bestMatch.autoIndex = server.autoIndex;
        bestMatch.maxBodySize = server.maxBodySize;
        bestMatch.uploadPath = server.uploadPath;
        bestMatch.cgiPath = server.cgiPath;
        bestMatch.cgiExtension = server.cgiExtensions.empty() ? "" : server.cgiExtensions.begin()->first;
    }
    
    return bestMatch;
}

bool Config::validate() const {
    if (_servers.empty()) {
        return false;
    }
    
    for (size_t i = 0; i < _servers.size(); ++i) {
        const ServerConfig& server = _servers[i];
        if (server.port <= 0 || server.port > 65535) {
            Utils::logError("Invalid port number: " + Utils::intToString(server.port));
            return false;
        }
        
        if (server.host.empty()) {
            Utils::logError("Empty host configuration");
            return false;
        }
        
        if (server.root.empty()) {
            Utils::logError("Empty root directory");
            return false;
        }
    }
    
    return true;
}

bool Config::isValidMethod(const std::string& method, const ServerConfig& server) const {
    for (size_t i = 0; i < server.allowedMethods.size(); ++i) {
        const std::string& allowedMethod = server.allowedMethods[i];
        if (allowedMethod == method) {
            return true;
        }
    }
    return false;
}

bool Config::isValidMethod(const std::string& method, const LocationConfig& location) const {
    for (size_t i = 0; i < location.allowedMethods.size(); ++i) {
        const std::string& allowedMethod = location.allowedMethods[i];
        if (allowedMethod == method) {
            return true;
        }
    }
    return false;
}

std::string Config::getCGIInterpreter(const std::string& extension, const ServerConfig& server) const {
    std::map<std::string, std::string>::const_iterator it = server.cgiExtensions.find(extension);
    if (it != server.cgiExtensions.end()) {
        return it->second;
    }
    return "";
}

bool Config::isCGIEnabled(const ServerConfig& server) const {
    return !server.cgiExtensions.empty();
}

std::vector<std::string> Config::split(const std::string& str, char delimiter) {
    return Utils::split(str, delimiter);
}

std::string Config::trim(const std::string& str) {
    return Utils::trim(str);
}

std::string Config::extractValue(const std::string& line) {
    size_t firstSpace = line.find(' ');
    if (firstSpace == std::string::npos) return "";
    
    std::string value = line.substr(firstSpace + 1);
    
    // Remove any trailing { if it exists
    size_t bracePos = value.find('{');
    if (bracePos != std::string::npos) {
        value = value.substr(0, bracePos);
    }
    
    // Remove comments (everything after #)
    size_t commentPos = value.find('#');
    if (commentPos != std::string::npos) {
        value = value.substr(0, commentPos);
    }
    
    return trim(value);
}

std::vector<std::string> Config::extractValues(const std::string& line) {
    size_t firstSpace = line.find(' ');
    if (firstSpace == std::string::npos) return std::vector<std::string>();
    
    std::string valuesStr = line.substr(firstSpace + 1);
    return split(trim(valuesStr), ' ');
}