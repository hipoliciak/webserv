#ifndef PARSE_HPP
#define PARSE_HPP

# include <algorithm>
# include <fstream>
# include <iostream>
# include <map>
# include <sstream>
# include <string>
# include <vector>

# include "Utils.hpp"

using namespace std;

class Parse
{
	public:
		struct ServerBlock
		{
			int _port; // Only one port per server block
			string _server_name;
			string _host;
			map<int, string> _error_pages; // Map for error code and its directory
			map<string, vector<string> > _routes;
			map<string, vector<string> > _allowed_methods;
			size_t _client_max_body_size; // Only one client max body size per server block
			string _root_directory;
			map<string, string> _returndir; // return
			map<string, string> _autoindex; // add autoindex
			map<string, string> _location_roots;
			map<string, string> _upload_paths;
			map<string, string> _defaultfile;
			map<string, string> _cgi_paths;
			map<string, vector<string> > _cgi_extensions;

			void clear()
			{
				_error_pages.clear();
				_allowed_methods.clear();
				_client_max_body_size = 0;
				_root_directory.clear();
				_autoindex.clear();
				_location_roots.clear();
				_upload_paths.clear();
				_defaultfile.clear();
				_cgi_paths.clear();
				_cgi_extensions.clear();
			}
		};

	private:
		vector<ServerBlock> _serverBlocks;

	public:
		Parse();
		~Parse();

		void loadConfig(const string &filename);
		const vector<ServerBlock> &getServerBlocks() const;

		// Parsing and utility functions
		void _trim(string &str);
		void _parseServerBlock(ifstream &config_file, ServerBlock &current_server);
		void _parseLine(const string &line, ServerBlock &current_server);
		void _cleanValue(string &value);
		size_t _parseSize(const string &size_str);
		void _parseLocationBlock(ifstream &config_file, const string &locationLine, ServerBlock &current_server);
		bool isDirectoryFormat(const string &folder);
		void handlesameport_host();
};

#endif