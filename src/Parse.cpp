#include "../include/Parse.hpp"
#include <string>

Parse::Parse() {}

Parse::~Parse() {}

bool Parse::isDirectoryFormat(const string &folder)
{
	if (folder.size() < 3 || folder[0] != '.' || folder[1] != '/')
		return false;

	for (size_t i = 2; i < folder.size(); ++i)
	{
		char c = folder[i];
		if (c == ',' || c == ':' || c == '*' || c == '.' || c == '#')
		{
			return false;
		}
	}
	return true;
}

void Parse::handlesameport_host()
{
	for (size_t i = 0; i < _serverBlocks.size(); ++i)
	{
		for (size_t j = i + 1; j < _serverBlocks.size();)
		{
			if (_serverBlocks[i]._port == _serverBlocks[j]._port && _serverBlocks[i]._host == _serverBlocks[j]._host)
				_serverBlocks.erase(_serverBlocks.begin() + j);
			else
				++j;
		}
	}
}

void Parse::loadConfig(const string &filename)
{

	ifstream config_file(filename.c_str());

	if (!config_file.is_open())
	{
		throw runtime_error("Could not open config file: " + filename);
	}

	string line;
	ServerBlock current_server; // Local server block to be filled
	while (getline(config_file, line))
	{
		line = line.substr(0, line.find('#')); // Remove comments
		_trim(line);

		if (line.empty())
			continue;

		if (line.find("server") == 0)
		{
			current_server.clear(); // Clear previous server block
			_parseServerBlock(config_file, current_server);
			_serverBlocks.push_back(current_server); // Store the fully parsed server block
		}
		else
		{
			_parseLine(line, current_server); // Global parsing for non-server-specific lines
		}
	}
	for (size_t i = 0; i < _serverBlocks.size(); ++i)
	{
		if (_serverBlocks[i]._client_max_body_size == 0)
		{
			_serverBlocks[i]._client_max_body_size = 1024 * 1024; /// default client_max_body_size value
		}
	}
	handlesameport_host();
}

void Parse::_parseLine(const string &line, ServerBlock &current_server)
{
	istringstream iss(line);
	string directive;
	iss >> directive;

	if (directive == "listen")
	{ // port
		string port_s;
		int port;

		if (!(iss >> port_s))
			throw runtime_error("Error : specify port number");

		for (size_t i = 0; i < port_s.length(); i++)
		{
			if (!isdigit(port_s[i]))
				throw runtime_error("Invalid port number");
		}
		port = Utils::stoi(port_s);
		if (port < 1 || port > 65535)
		{
			throw runtime_error("Invalid port number");
		}
		current_server._port = port; /// port of the block
	}
	else if (directive == "host")
	{
		string host;
		if (!(iss >> host))
			throw runtime_error("empty host");
		else
		{
			_cleanValue(host);
			current_server._host = host;
		}
	}
	else if (directive == "server_name")
	{ // server name
		string name;
		iss >> name;
		_cleanValue(name);
		current_server._server_name = name; // Store a single server name instead of multiple names
	}
	else if (directive == "error_page")
	{ // error pages
		int code;
		string path;
		iss >> code >> path;
		_cleanValue(path);
		current_server._error_pages[code] = path; // Store  error code and the corresponding directory
	}
	else if (directive == "client_max_body_size")
	{ // client_max_body_size
		string size_str;
		iss >> size_str;
		_cleanValue(size_str);

		if (!size_str.empty() && size_str[size_str.size() - 1] == ';')
		{
			size_str.erase(size_str.size() - 1);
		}

		current_server._client_max_body_size = _parseSize(size_str);
	}
	else if (directive == "root")
	{ // root
		string path;
		iss >> path;
		_cleanValue(path);
		current_server._root_directory = path;
	}

	if (!current_server._port) // check if no method found throw error
		throw runtime_error("specify a port");
}

void Parse::_parseServerBlock(ifstream &config_file, ServerBlock &current_server)
{
	string line;
	while (getline(config_file, line))
	{
		line = line.substr(0, line.find('#'));
		_trim(line);

		if (line.empty())
			continue;
		if (line == "}") // the end of the block
			break;
		if (line.find("location") == 0)
		{
			_parseLocationBlock(config_file, line, current_server);
		}
		else
		{
			_parseLine(line, current_server);
		}
	}
}

size_t Parse::_parseSize(const string &size_str)
{ // client_max_body_size
	if (size_str.empty())
		return 0;

	char unit = size_str[size_str.size() - 1];
	size_t size = 0;

	if (isdigit(unit))
	{
		size = atoi(size_str.c_str());
	}
	else
	{
		size = atoi(size_str.substr(0, size_str.size() - 1).c_str());
		if (unit == 'K' || unit == 'k')
		{
			size *= 1024;
		}
		else if (unit == 'M' || unit == 'm')
		{
			size *= 1024 * 1024;
		}
		else if (unit == 'G' || unit == 'g')
		{
			size *= 1024 * 1024 * 1024;
		}
		else
		{
			throw runtime_error("Invalid size unit");
		}
	}
	return size;
}

void Parse::_parseLocationBlock(ifstream &config_file, const string &locationLine, ServerBlock &current_server)
{
	string line = locationLine;
	string path;
	vector<string> methods;
	string root_path;
	string upload_path;
	string autoindex;
	string returnvalue;
	string cgi_path;
	string defaultfile;
	vector<string> cgi_extensions;

	size_t start = line.find_first_of(" \t");
	size_t end = line.find_first_of(" \t{", start + 1);
	if (start != string::npos)
	{
		if (end == string::npos)
			end = line.size();
		path = line.substr(start + 1, end - start - 1);
		_trim(path);
	}

	while (getline(config_file, line))
	{
		_trim(line);
		if (line.empty())
			continue;
		if (line == "}")
			break;

		istringstream iss(line);
		string directive;
		iss >> directive;

		if (directive == "allowed_methods")
		{
			string method;
			while (iss >> method)
			{
				_cleanValue(method);
				if (method.compare("GET") == 0 || method.compare("POST") == 0 || method.compare("DELETE") == 0) // only methods words
					methods.push_back(method);
				else
					throw runtime_error("error allowed methods syntaxe");
			}
		}
		else if (directive == "root")
		{
			iss >> root_path;
			_cleanValue(root_path);
		}
		else if (directive == "autoindex")
		{
			iss >> autoindex;
			_cleanValue(autoindex);
		}
		else if (directive == "default")
		{
			iss >> defaultfile;
			_cleanValue(defaultfile);
		}
		else if (directive == "return")
		{
			iss >> returnvalue;
			_cleanValue(returnvalue);
		}
		else if (directive == "upload_path")
		{
			iss >> upload_path;
			_cleanValue(upload_path);
		}
		else if (directive == "cgi_path")
		{
			iss >> cgi_path;
			_cleanValue(cgi_path);
		}
		else if (directive == "cgi_extensions")
		{
			string ext;
			while (iss >> ext)
			{
				_cleanValue(ext);
				cgi_extensions.push_back(ext);
			}
		}
	}

	if (!path.empty())
	{
		current_server._allowed_methods[path] = methods; // methods
		if (!autoindex.empty())
			current_server._autoindex[path] = autoindex; // autoindex
		if (!root_path.empty())
			current_server._location_roots[path] = root_path; // root_path
		if (!(defaultfile.empty()))
			current_server._defaultfile[path] = defaultfile; // defaultfile
		if (!(returnvalue.empty()))
			current_server._returndir[path] = returnvalue; // _returndir
		if (!upload_path.empty())
			current_server._upload_paths[path] = upload_path; // upload_path
		if (!cgi_path.empty())
			current_server._cgi_paths[path] = cgi_path; // cgi_path
		if (!cgi_extensions.empty())
			current_server._cgi_extensions[path] = cgi_extensions; // cgi_extensions
	}
	if (methods.empty()) // check if no method found throw error
		throw runtime_error("need allowed methodes");
}

void Parse::_trim(string &str)
{

	size_t start = str.find_first_not_of(" \t\n\r");
	size_t end = str.find_last_not_of(" \t\n\r;");
	if (start != string::npos && end != string::npos)
	{
		str = str.substr(start, end - start + 1);
	}
}

void Parse::_cleanValue(string &value)
{
	_trim(value);
	if (!value.empty() && value[value.size() - 1] == ';')
	{ // strip ;
		value.erase(value.size() - 1);
	}
}
const vector<Parse::ServerBlock> &Parse::getServerBlocks() const
{
	return _serverBlocks;
}
