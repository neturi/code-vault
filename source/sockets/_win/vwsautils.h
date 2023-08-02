#ifndef vwsautils_h
#define vwsautils_h

#include <winsock2.h>
#include "vtypes.h"

/**
A utility class related to WinSock.
*/
class WSAUtils {
public:
    WSAUtils() = delete;
    WSAUtils(const WSAUtils& other) = delete;
    WSAUtils& operator=(const WSAUtils& other) = delete;

    ~WSAUtils();

    /**
    Given an error code - mostly retrieved by calling ::WSAGetLastError - this method 
    returns the error message.

    error - 
    The Windows (or, WinSock) error code for which the error message is needed.
    */
    static std::string ErrorMessage(DWORD error);
};
#endif
