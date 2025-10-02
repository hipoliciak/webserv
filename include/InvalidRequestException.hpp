#ifndef INVALIDREQUESTEXCEPTION_HPP
#define INVALIDREQUESTEXCEPTION_HPP

# include <string>
# include <stdexcept>

using namespace std;

class InvalidRequestException : public runtime_error
{
public:
	InvalidRequestException(const string message);
};

#endif