#include "../include/ServerConfig.hpp"

ServerConfig::ServerConfig() {}

ServerConfig::ServerConfig(string path, Parse::ServerBlock serverBlock)
{
	port = serverBlock._port;
	name = serverBlock._server_name;
	host = serverBlock._host;
	rootDir = serverBlock._root_directory;
	absoluteRootDir = path + rootDir;
	maxClientBodySize = serverBlock._client_max_body_size;

	for (map<string, vector<string> >::const_iterator it = serverBlock._allowed_methods.begin(); it != serverBlock._allowed_methods.end(); it++)
		locations[it->first] = Location(it->first, serverBlock);

	for (map<int, string>::const_iterator it = serverBlock._error_pages.begin(); it != serverBlock._error_pages.end(); it++)
		errorPages[it->first] = it->second;
}

const string &ServerConfig::getUploadPath(string location) const
{
	if (1 != locations.count(location))
		return emptyString;
	return locations.find(location)->second.getUploadPath();
}

// Getters
const string &ServerConfig::getAbsoluteRootDir() const { return absoluteRootDir; }
const map<int, string> &ServerConfig::getErrorPages() const { return errorPages; }
const string &ServerConfig::getHost() const { return host; }
const map<string, Location> &ServerConfig::getLocations() const { return locations; }
const int &ServerConfig::getMaxClientBodySize() const { return maxClientBodySize; }
const string &ServerConfig::getName() const { return name; }
const int &ServerConfig::getPort() const { return port; }
const string &ServerConfig::getRootDir() const { return rootDir; }

ostream	&operator<<(ostream &stream, ServerConfig const &serverConfig)
{
	stream << "ServerConfig:" << endl
		<< "  Name: " << serverConfig.getName() << endl
		<< "  Host: " << serverConfig.getHost() << endl
		<< "  Port: " << serverConfig.getPort() << endl
		<< "  Root Directory: " << serverConfig.getRootDir() << endl
		<< "  Max Client Body Size: " << serverConfig.getMaxClientBodySize() << endl
		<< "  Error Pages:" << endl;
		for (map<int, string>::const_iterator it = serverConfig.getErrorPages().begin(); it != serverConfig.getErrorPages().end(); it++)
		stream << "    Error Code: " << it->first << endl << "        Path: " << it->second << endl;
		stream << "    Locations:" << endl;
		for (map<string, Location>::const_iterator it = serverConfig.getLocations().begin(); it != serverConfig.getLocations().end(); it++)
		{
			stream << "      Path: " << it->second.getName() << endl
			<< "        Allowed Methods: ";
			vector<string> methods = it->second.getAllowedMethods();
			for (vector<string>::const_iterator it2 = methods.begin(); it2 != methods.end(); it2++)
				stream << *it2 << " ";
			stream << endl;
			if (it->second.getAutoindex())
				stream << "        Autoindex on" << endl;
			if (!it->second.getUploadPath().empty())
				stream << "        Upload Path: " << it->second.getUploadPath() << endl;
			if (!it->second.getDefaultFile().empty())
				stream << "        Default file: " << it->second.getDefaultFile() << endl;
			if (!it->second.getRedirPath().empty())
				stream << "        Redirects to: " << it->second.getRedirPath() << endl;
			if (!it->second.getCgiExtensions().empty())
			{
				stream << "        CGI Extensions: ";
				vector<string> temp = it->second.getCgiExtensions();
				for (vector<string>::const_iterator it2 = temp.begin(); it2 != temp.end(); it2++)
					stream << *it2 << " ";
				stream << endl;
			}
		}

	return stream;
}
