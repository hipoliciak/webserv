#ifndef LOCATION_HPP
#define LOCATION_HPP

# include <iostream>
# include <string>
# include <vector>

# include "Parse.hpp"

using namespace std;

class Location
{
	private:
		string name;
		vector<string> allowedMethods;
		vector<string> cgiExtensions;
		bool autoindex;
		string uploadPath;
		string redirpath;
		string defaultFile;

	public:
		Location();
		Location(string name, Parse::ServerBlock serverBlock);
		const string &getName() const;
		const vector<string> &getAllowedMethods() const;
		const bool &getAutoindex() const;
		const string &getUploadPath() const;
		const string &getDefaultFile() const;
		const string &getRedirPath() const;
		const vector<string> &getCgiExtensions() const;
};

#endif
