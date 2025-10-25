#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "webserv.hpp"

class Client {
private:
    int _fd;
    std::string _buffer;
    std::string _request;
    bool _requestComplete;
    time_t _lastActivity;
    bool _stopReading;
    size_t _lastBufferSize;
    bool _chunkedEncodingLogged;

public:
    Client();
    Client(int fd);
    ~Client();

    // Data handling
    bool readData();
    void appendToBuffer(const std::string& data);
    bool isRequestComplete() const;
    void clearRequest();
    
    // Reading control
    void stopReading();
    bool shouldStopReading() const;
    bool areHeadersComplete() const;
    std::string getHeaders() const;
    
    // Getters/Setters
    int getFd() const;
    const std::string& getRequest() const;
    const std::string& getBuffer() const;
    time_t getLastActivity() const;
    void updateActivity();
    
    // Chunked encoding logging control
    bool hasLoggedChunkedEncoding() const;
    void setChunkedEncodingLogged();
    
    // Request parsing
    bool parseRequest();
};

#endif