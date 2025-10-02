#include "../include/Location.hpp"

Location::Location() {}

Location::Location(string name, Parse::ServerBlock serverBlock)
{
	this->name = name;

	for (vector<string>::const_iterator it = serverBlock._allowed_methods[name].begin(); it != serverBlock._allowed_methods[name].end(); it++)
		allowedMethods.push_back(*it);

	map<string, string>::iterator it = serverBlock._autoindex.find(name);
	if (it != serverBlock._autoindex.end())
		autoindex = it->second == "on";
	else
		autoindex = false;

	it = serverBlock._upload_paths.find(name);
	if (it != serverBlock._upload_paths.end())
		uploadPath = it->second;
	else
		uploadPath = "";

	it = serverBlock._defaultfile.find(name);
	if (it != serverBlock._defaultfile.end())
		defaultFile = it->second;
	else
		defaultFile = "";

	it = serverBlock._returndir.find(name);
	if (it != serverBlock._returndir.end())
		redirpath = it->second;
	else
		redirpath = "";

	map<string, vector<string> >::iterator it2 = serverBlock._cgi_extensions.find(name);
	if (it2 != serverBlock._cgi_extensions.end())
	{
		vector<string> temp = it2->second;
		for (vector<string>::iterator it3 = temp.begin(); it3 != temp.end(); it3++)
			cgiExtensions.push_back(*it3);
	}
}

const string &Location::getName() const { return name; }
const vector<string> &Location::getAllowedMethods() const { return allowedMethods; }
const bool &Location::getAutoindex() const { return autoindex; }
const string &Location::getUploadPath() const { return uploadPath; }
const string &Location::getDefaultFile() const { return defaultFile; }
const string &Location::getRedirPath() const { return redirpath; }
const vector<string> &Location::getCgiExtensions() const { return cgiExtensions; }
