#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "webserv.hpp"
#include <fstream>

class Client {
	public:
		Client();
		Client(int fd);
		~Client();

		// --- STATE MACHINE & BODY FILE ---
		enum ClientState {
			STATE_READING_HEADERS,
			STATE_HEADERS_COMPLETE,
			STATE_READING_BODY,
			STATE_READING_CHUNK_SIZE,
			STATE_READING_CHUNK_DATA,
			STATE_REQUEST_COMPLETE
		};
		ClientState getState() const;

		// Data handling
		bool readData();
		bool isRequestComplete() const;
		void clearRequest();
		
		// Reading control
		void stopReading();
		bool shouldStopReading() const;
		bool areHeadersComplete() const;
		
		// Getters/Setters
		int getFd() const;
		size_t getContentLength() const;
		bool isChunked() const;
		const std::string& getRequest() const;
		const std::string& getBodyFilePath() const;
		const std::string& getBuffer() const;
		time_t getLastActivity() const;
		void updateActivity();
		void beginReadingBody(size_t maxBodySize);
		void markForCloseAfterWrite();
		bool shouldCloseAfterWrite() const;
		bool parseRequest();

	private:
		int _fd;
		std::string _buffer;
		time_t _lastActivity;
		bool _stopReading;

		ClientState _state;
		std::string _headers;
		std::string _bodyFilePath;
		std::ofstream* _bodyFile;
		size_t _contentLength;
		size_t _maxBodySize;
		size_t _bodyBytesReceived;
		size_t _currentChunkSize;
		bool _isChunked;
		bool _requestComplete;
		bool _closeConnectionAfterWrite;
		std::string createTempFile();
		bool openBodyFile();
		bool parseHeadersFromBuffer();
		bool handleBodyRead();
		bool handleChunkRead();
};

#endif