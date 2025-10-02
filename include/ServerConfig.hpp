#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

# include <iostream>
# include <map>
# include <string>

# include "Location.hpp"
# include "Parse.hpp"

using namespace std;

class ServerConfig
{
	private:
		string					absoluteRootDir;
		map<int, string>		errorPages;
		string					host;
		map<string, Location>	locations;
		int						maxClientBodySize;
		string					name;
		int						port;
		string					rootDir;
		string					emptyString;

	public:
		ServerConfig();
		ServerConfig(string path, Parse::ServerBlock serverBlock);

		// Getters
		const string				&getAbsoluteRootDir() const;
		const map<int, string>		&getErrorPages() const;
		const string				&getHost() const;
		const map<string, Location>	&getLocations() const;
		const int					&getMaxClientBodySize() const;
		const string				&getName() const;
		const string				&getRootDir() const;
		const int					&getPort() const;
		const string				&getUploadPath(string location) const;
	};

#endif

ostream &operator<<(ostream &stream, ServerConfig const &serverConfig);
