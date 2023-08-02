#include "vutils.h"
#include "vwsautils.h"

std::string WSAUtils::ErrorMessage(DWORD error) {
    LPVOID lpMsgBuf;

    DWORD result = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
        error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);

    std::string errorMessage = boost::str(boost::format{ "(%1%) [N/A]" } % error);

    if (result) {
        errorMessage = boost::str(boost::format{ "(%1%) %2%" } % error % ((char*)lpMsgBuf));

        ::LocalFree(lpMsgBuf);

        STLUtils::Replace(errorMessage, STLUtils::TOKEN_NEWLINE, STLUtils::TOKEN_EMPTY);
    }

    return errorMessage;
}
