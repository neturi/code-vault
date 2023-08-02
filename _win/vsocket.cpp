/*
Copyright c1997-2011 Trygve Isaacson. All rights reserved.
This file is part of the Code Vault version 3.3
http://www.bombaydigital.com/
*/

/** @file */

#include "vsocket.h"
#include "vtypes_internal.h"

#include "vexception.h"
#include "vutils.h"
#include "vwsautils.h"

#ifdef XPS_SERVER
#include <libssh/libssh.h>
#endif

V_STATIC_INIT_TRACE

bool VSocketManager::Initialized = false;

VSocketManager::VSocketManager() {
}

VSocketManager::~VSocketManager() {
}

void VSocketManager::Initialize() {
    // NOTE: Method not thread-safe.
    if (VSocketManager::Initialized) {
        return;
    }

    WORD    versionRequested;
    WSADATA wsaData;
    int     err;

    // Currently using WinSock version 2.2. 
    // Previously, we were choosing version 2.0 (even though we were loading 2.2 library).
    versionRequested = MAKEWORD(VSocket::WINSOCK_MAJOR_VERSION, VSocket::WINSOCK_MINOR_VERSION);

    err = ::WSAStartup(versionRequested, &wsaData);

    VString errorMessage = VSTRING_FORMAT("VSocketManager::Initialize - Failed to initialize Windows Sockets. ::WSAStartup failed with error (%d): ",
                                            err);

    // WSAGetLastError() won't work for WSAStartup.
    // Since this is platform-specific validation, this block cannot be moved to an utility method.
    if (err != 0) {
        switch (err) {
        case WSASYSNOTREADY:
            errorMessage += "The underlying network subsystem is not ready for network communication";
            break;

        case WSAVERNOTSUPPORTED:
            errorMessage += "The version of Windows Sockets support requested is not provided by this particular Windows Sockets implementation";
            break;

        case WSAEINPROGRESS:
            errorMessage += "A blocking Windows Sockets 1.1 operation is in progress";
            break;

        case WSAEPROCLIM:
            errorMessage += "A limit on the number of tasks supported by the Windows Sockets implementation has been reached";
            break;

        case WSAEFAULT:
            errorMessage += "The 'lpWSAData' parameter is not a valid pointer";
            break;

        default:
            errorMessage += "Unknown Error";
            break;
        }

        throw VException(errorMessage);
    }

    VSocketManager::Initialized = true;

    VLOGGER_LEVEL(VLoggerLevel::INFO, "Sockets initialized.");
}

bool VSocketManager::IsInitialized() {
    // NOTE: Method not thread-safe.
    return VSocketManager::Initialized;
}

bool VSocketManager::Deinitialize(bool passiveMode) {
    // NOTE: Method not thread-safe.
    if (!VSocketManager::Initialized) {
        return true;
    }

    // Currently, we don't have any mechanism to close/cleanup all the sockets before we cleanup/unload WinSock.
    int result = ::WSACleanup();

    if (result == SOCKET_ERROR) {
         DWORD errorCode = ::WSAGetLastError();

         VString errorMessage = VSTRING_FORMAT("VSocketManager::Deinitialize - Failed to clean-up. ::WSACleanup returned error: %s", WSAUtils::ErrorMessage(errorCode).c_str());

        if (passiveMode) {
            VLOGGER_LEVEL(VLoggerLevel::ERROR, errorMessage);

            return false;
        } else {
            throw VException(errorMessage);
        }
    } else {
        VSocketManager::Initialized = false;

        VLOGGER_LEVEL(VLoggerLevel::INFO, "Sockets de-initialized.");
    }

    return true;
}

static const int MAX_ADDRSTRLEN = V_MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN);

VNetworkInterfaceList VSocketBase::enumerateNetworkInterfaces() {
    VNetworkInterfaceList interfaces;
    SOCKET sock = ::WSASocketW(AF_INET, SOCK_DGRAM, 0, 0, 0, 0);

    if (sock == SOCKET_ERROR) {
        DWORD errorCode = ::WSAGetLastError();

        throw VException(VSTRING_FORMAT("VSocketBase::enumerateNetworkInterfaces - ::WSASocket failed with error: %s.", WSAUtils::ErrorMessage(errorCode).c_str()));
    }

    INTERFACE_INFO interfaceInfo[20];
    unsigned long numBytesReturned;
    int result = ::WSAIoctl(sock, SIO_GET_INTERFACE_LIST, 0, 0, &interfaceInfo, sizeof(interfaceInfo), &numBytesReturned, 0, 0);
    if (result == SOCKET_ERROR) {
        DWORD errorCode = ::WSAGetLastError();

        throw VException(VSTRING_FORMAT("VSocketBase::enumerateNetworkInterfaces - ::WSAIoctl failed with error: %s.", WSAUtils::ErrorMessage(errorCode).c_str()));
    }

    int numInterfaces = (numBytesReturned / sizeof(INTERFACE_INFO));

    for (int i = 0; i < numInterfaces; ++i) {
        // Filter out 127.x.x.x (loopback addresses).
		// Disabling this warning locally as fixing the warning would cause VC8 build to fail.
#pragma warning(push)
#pragma warning(disable:4996)
        char* tempAddress = ::inet_ntoa(((sockaddr_in*) &(interfaceInfo[i].iiAddress))->sin_addr);
#pragma warning(pop)

		if (tempAddress == NULL) {
            DWORD errorCode = ::WSAGetLastError();

            throw VException(VSTRING_FORMAT("VSocketBase::enumerateNetworkInterfaces - ::inet_ntoa failed with error: %s.", WSAUtils::ErrorMessage(errorCode).c_str()));
        }

        VString address(tempAddress);

        if (! address.startsWith("127.")) {
            VNetworkInterfaceInfo info;
            info.mAddress = address;
            interfaces.push_back(info);
        }
    }

    return interfaces;
}

// static
VString VSocketBase::addrinfoToIPAddressString(const VString& hostName, const struct addrinfo* info) {
    VString result;
    result.preflight(MAX_ADDRSTRLEN);
    
    // WSAAddressToString() works for both IPv4 and IPv6 and is available on "older" versions of Windows.
    DWORD bufferLength = MAX_ADDRSTRLEN;

    int resultCode = ::WSAAddressToStringW(info->ai_addr, (DWORD)info->ai_addrlen, NULL, WideCharacterToVStringConverter(result, bufferLength), &bufferLength);

    if (resultCode != 0) {
        DWORD errorCode = ::WSAGetLastError();

        throw VException(static_cast<int>(errorCode), 
            VSTRING_FORMAT("VSocketBase::addrinfoToIPAddressString(%s) - ::WSAAddressToString() failed. Error=%s.", hostName.chars(), WSAUtils::ErrorMessage(errorCode).c_str()));
    }

    return result;
}

const int VSocket::PEEK_MESSAGE_BUFFER_LENGTH = 4;

const int VSocket::WINSOCK_MAJOR_VERSION = 2;
const int VSocket::WINSOCK_MINOR_VERSION = 2;

VSocket::VSocket(VSocketID id)
    : VSocketBase(id), mReadShutDown(true), mWriteShutDown(true)
    {
}

VSocket::VSocket(const VString& hostName, int portNumber)
    : VSocketBase(hostName, portNumber), mReadShutDown(true), mWriteShutDown(true)
    {
}

VSocket::~VSocket() {
	// Base does cleanup. But, it only closes the socket and discards the socket Id. 
	// We need to shutdown Rx & Tx *before* the socket is closed.
#ifdef XPS_SERVER
	sshDeleteSession = true;
#endif
	close();
}

int VSocket::available() {
    u_long numBytesAvailable = 0;
    u_long actualBytesReturned = 0;

    // Query the amount of data available (queued) for read
    int result = ::WSAIoctl(mSocketID, FIONREAD, NULL, 0, &numBytesAvailable, sizeof(u_long), &actualBytesReturned, NULL, NULL);

    if (result == SOCKET_ERROR) {
        DWORD errorCode = ::WSAGetLastError();

        _throwSocketError(false, "VSocket::available - ::WSAIoctl for FIONREAD failed.", mSocketID, errorCode, result);
    }

    if (numBytesAvailable == 0) {
        //set the socket to be non-blocking.
        u_long argp = 1;
        result = ::WSAIoctl(mSocketID, FIONBIO, &argp, sizeof(u_long), NULL, 0, &actualBytesReturned, NULL, NULL);

        if (result == SOCKET_ERROR) {
            DWORD errorCode = ::WSAGetLastError();

            _throwSocketError(false, "VSocket::available - ::WSAIoctl failed to set socket as non-blocking.", mSocketID, errorCode, result);
        }

        WSABUF dataBuffer;
        char buffer[VSocket::PEEK_MESSAGE_BUFFER_LENGTH];

        u_long recieveFlags = MSG_PEEK;

        dataBuffer.buf = buffer;
        dataBuffer.len = VSocket::PEEK_MESSAGE_BUFFER_LENGTH;

        int receiveResult = ::WSARecv(mSocketID, &dataBuffer, 1, &numBytesAvailable, &recieveFlags, NULL, NULL);
        int receiveError = (receiveResult == SOCKET_ERROR ? ::WSAGetLastError() : 0);

        //restore the blocking 
        argp = 0;   // mohanla - The input argument was previously set to 1 (non-blocking). 
                    // That is, we were not restoring the blocking (contrary to the comment).
                    // Was it a typo? Or, was it intentional? 
        result = ::WSAIoctl(mSocketID, FIONBIO, &argp, sizeof(u_long), NULL, 0, &actualBytesReturned, NULL, NULL);

        if (result == SOCKET_ERROR) {
            // Failed to reset (or, cancel) the non-blocking.
            DWORD errorCode = ::WSAGetLastError();

            _throwSocketError(false, "VSocket::available - ::WSAIoctl failed to reset socket as blocking.", mSocketID, errorCode, result);
        }

        if (receiveResult == SOCKET_ERROR) {
            if (receiveError == WSAEWOULDBLOCK) {
                // WSAEWOULDBLOCK is normal when there is no data queued. 
                // We get this error a lot and the caller - tWinSockConnection::_doTCPConnectionReceive - processes only the error code for WSAEWOULDBLOCK.
                // So, avoiding VSTRING_FORMAT.
                throw VSocketException(receiveError, "VSocket::available - WSAEWOULDBLOCK");
            } else {
                _throwSocketError(false, "VSocket::available - Failed to peek at queued data.", mSocketID, receiveError, receiveResult);
            }
        }
    }
    return (int) numBytesAvailable;
}

int VSocket::read(Vu8* buffer, int numBytesToRead) {
    const static auto MAX_RETRY_ATTEMPTS_FOR_WSAWOULDBLOCK_ERROR = 10;
    const static auto RETRY_INTERVAL_FOR_WSAWOULDBLOCK_ERROR = 10; // Milliseconds

    if (mSocketID == kNoSocketID) {
        throw VSocketException(ExceptionErrorCodes::SocketErrors::SOCKET_ERROR_INVALID_SOCKET, VSTRING_FORMAT("VSocket::read with invalid mSocketID %d", mSocketID));
    }

    VInstant now;

    int       bytesRemainingToRead = numBytesToRead;
    Vu8*      nextBufferPositionPtr = buffer;
    int       result;
    int       numBytesRead = 0;
    int       numBytesReadDuringThisReceive = 0;

    // List to contain sockets for which 'read' statuses are to be determined.
    fd_set    readset;

    WSABUF    dataBuffer;
    u_long    recieveFlags = 0;
    u_long    actualBytesRead = 0;

#ifdef XPS_SERVER
    int bytesReadSSH = 0;
    bool isSSHSocket = false;

    // search if the required socket is present in the SSH map. If so, proceed with SSH read
    // Else, default back to normal read
    ssh_channel sshChannel = NULL;
    ssh_session sshSession = NULL;
    auto mapFind = sshSessionMap.find(mSocketID);
    if (mapFind != sshSessionMap.end()) {
        isSSHSocket = true;
        std::pair<ssh_session, ssh_channel> temp = mapFind->second;
        sshChannel = temp.second;
        sshSession = temp.first;
    }

    if (isSSHSocket == true) {

        if (ssh_is_connected(sshSession) == 0) {
            ssh_disconnect(sshSession);
            ssh_free(sshSession);
            VLOGGER_WARN(VSTRING_FORMAT(NAV_LITERAL("SSH Server :: Session on socket %d closed : client disconnected"), mSocketID));
            sshSessionMap.erase(mapFind->first);
            return 0;
        }
        
        dataBuffer.buf = (CHAR*)nextBufferPositionPtr;
        dataBuffer.len = bytesRemainingToRead;

        bytesReadSSH = ssh_channel_read(sshChannel, dataBuffer.buf, dataBuffer.len, 0);
        bytesRemainingToRead -= bytesReadSSH;

        return (numBytesToRead - bytesRemainingToRead);

    }

#endif
        
    bool isCapturingNetworkStatistics = NetworkMonitor::isCapturingNetworkStatistics();

    NetworkRxTransactionLog log;

    auto currentRetryCountForWSAWOULDBLOCKError = 1;

    while (bytesRemainingToRead > 0) {
        // Note that unlike on Unix, verifying mSocketID <= FD_SETSIZE here is inappropriate because
        // of different Winsock fd_set internals, ID range, and select() API behavior. mSocketID may well be
        // larger than FD_SETSIZE, and that is OK.

        // Initialize the sockets list that would contain the list of sockets that we are querying the statuses for.
        FD_ZERO(&readset);

        // Add (just) our socket to the list.
        FD_SET(mSocketID, &readset);

        // The first argument for ::select is included only for compatibility with BSD. So, create a dummy argument.
        int nfds = (int)(mSocketID + 1);

        // Query the 'read' status of our socket.
        result = ::select(nfds, &readset, NULL, NULL, (mReadTimeOutActive ? &mReadTimeOut : NULL));

        if (result < 0) {
            DWORD errorCode = ::WSAGetLastError();

            if (errno == EINTR) {
                // Debug message: read was interrupted but we will cycle around and try again...
                continue;
            }

            // In case of SOCKET_ERROR
            _throwSocketError(false, "VSocket::read - ::select failed.", mSocketID, errorCode, result);
        }
        else if (result == 0) {
            VString errorMessage = VSTRING_FORMAT("VSocket::read - ::select timed out on socket %d.", mSocketID);

            throw VSocketReadTimedOutException(errorMessage, numBytesToRead, numBytesReadDuringThisReceive);
        }

        if (!FD_ISSET(mSocketID, &readset)) {
            DWORD errorCode = ::WSAGetLastError();

            _throwSocketError(false, "VSocket::read - ::select set to FD_ISSET false.", mSocketID, errorCode);
        }

        /////////////////////////////////////////////////
        // ARGO-60089 - babune
        // This is to check for BUFFER full condition
        // and log every time interval condition.
        if (bytesRemainingToRead >= kDefaultBufferSize){    // && (now - mLastEventTime >= VDuration::MINUTE())) {
            VLOGGER_LEVEL(VLoggerLevel::INFO,
                VSTRING_FORMAT("[PerfStats]-Buffer size is full on socket %d recv of length %d", mSocketID, bytesRemainingToRead));
        }
        ////////////////////////////////////////////////////

        dataBuffer.buf = (CHAR*)nextBufferPositionPtr;
        dataBuffer.len = bytesRemainingToRead;

        if (isCapturingNetworkStatistics) {
            // See comments in VSocket::write
            log.startTransaction();
        }

        result = ::WSARecv(mSocketID, &dataBuffer, 1, &actualBytesRead, &recieveFlags, NULL, NULL);

        if (isCapturingNetworkStatistics) {
            log.completeTransaction(actualBytesRead);
        }

        numBytesRead = (int)actualBytesRead;

        if (result == SOCKET_ERROR) {
            DWORD errorCode = ::WSAGetLastError();

            if (errorCode == WSAEWOULDBLOCK) {
                if (currentRetryCountForWSAWOULDBLOCKError < MAX_RETRY_ATTEMPTS_FOR_WSAWOULDBLOCK_ERROR) {
                    currentRetryCountForWSAWOULDBLOCKError++;

                    VThread::sleep(VDuration::MILLISECOND() * RETRY_INTERVAL_FOR_WSAWOULDBLOCK_ERROR);

                    auto errorMessagePrefix = VSTRING_FORMAT("VSocket::read - ::WSARecv failed to receive data on error %d after %d retry attempt(s). bytesReadSoFar=%d", static_cast<int>(errorCode), 
                                                                    currentRetryCountForWSAWOULDBLOCKError, numBytesReadDuringThisReceive);

                    VLOGGER_LEVEL(VLoggerLevel::WARN, errorMessagePrefix);

                    continue;
                }

                VLOGGER_LEVEL(VLoggerLevel::ERROR, VSTRING_FORMAT("VSocket::read - Exhausted all retry attempts (%d) to read from socket", MAX_RETRY_ATTEMPTS_FOR_WSAWOULDBLOCK_ERROR));
            }

            auto errorMessagePrefix = VSTRING_FORMAT("VSocket::read - ::WSARecv failed to receive data. bytesReadSoFar=%d,", numBytesReadDuringThisReceive);

            bool shouldLogAsError = !_isCommonSocketTeardownError(errorCode);

            _throwSocketError(shouldLogAsError, errorMessagePrefix, mSocketID, errorCode, result);
        }

        if (actualBytesRead == 0) {
            if (mRequireReadAll) {
                DWORD errorCode = ::WSAGetLastError();

                VString errorMessagePrefix = VSTRING_FORMAT("VSocket::read - Reached EOF unexpectedly while receiving data. bytesRemainingToRead: %d,", bytesRemainingToRead);

                VString errorMessage;

                _frameErrorMessage(errorMessagePrefix, mSocketID, errorCode, _getNativeErrorCode(errorCode), errorMessage);

                throw VEOFException(errorCode, errorMessage);
            }
            else {
                break;    // got successful but partial read, caller will have to keep reading
            }
        }

        bytesRemainingToRead -= numBytesRead;
        nextBufferPositionPtr += numBytesRead;

        numBytesReadDuringThisReceive += numBytesRead;
        mNumBytesRead += numBytesRead;
    }

    if (isCapturingNetworkStatistics) {
        _addRxTransactionLog(log);
    }

    mLastEventTime.setNow();

    return (numBytesToRead - bytesRemainingToRead);

}

int VSocket::write(const Vu8* buffer, int numBytesToWrite) {
    if (mSocketID == kNoSocketID) {
        throw VSocketException(ExceptionErrorCodes::SocketErrors::SOCKET_ERROR_INVALID_SOCKET, VSTRING_FORMAT("VSocket::write with invalid mSocketID %d", mSocketID));
    }

    VInstant now;

    const Vu8*  nextBufferPositionPtr = buffer;
    int         bytesRemainingToWrite = numBytesToWrite;
    int         result;
    int         numBytesWritten;
    int         numBytesWrittenDuringThisSend = 0;

    fd_set      writeset;

    WSABUF    dataBuffer;
    u_long    actualBytesWritten = 0;

    bool isCapturingNetworkStatistics = NetworkMonitor::isCapturingNetworkStatistics();

    NetworkTxTransactionLog log;

#ifdef XPS_SERVER

    bool isSSHSocket = false;

    // search if the required socket is present in the SSH map. If so, proceed with SSH write
    // Else, default back to normal write
    ssh_channel sshChannel = NULL;
    ssh_session sshSession = NULL;
    auto mapFind = sshSessionMap.find(mSocketID);
    if (mapFind != sshSessionMap.end()) {
        isSSHSocket = true;
        std::pair<ssh_session, ssh_channel> temp = mapFind->second;
        sshChannel = temp.second;
        sshSession = temp.first;
    }

    if (isSSHSocket == true) {

        if (ssh_is_connected(sshSession) == 0) {
            ssh_disconnect(sshSession);
            ssh_free(sshSession);
            VLOGGER_WARN(VSTRING_FORMAT(NAV_LITERAL("SSH Server :: Session on socket %d closed : client disconnected"), mSocketID));
            sshSessionMap.erase(mapFind->first);
            return 0;
        }

        int bytesWrittenSSH = 0;

        dataBuffer.buf = (CHAR*)nextBufferPositionPtr;
        dataBuffer.len = bytesRemainingToWrite;

        bytesWrittenSSH = ssh_channel_write(sshChannel, dataBuffer.buf, dataBuffer.len);
        bytesRemainingToWrite -= bytesWrittenSSH;

        return (numBytesToWrite - bytesRemainingToWrite);
    }

#endif

    while (bytesRemainingToWrite > 0) {
        // See comment at same location in read() above, regarding mSocketID and FD_SETSIZE.

        FD_ZERO(&writeset);
        FD_SET(mSocketID, &writeset);

        int nfds = (int)(mSocketID + 1);

        result = ::select(nfds, NULL, &writeset, NULL, (mWriteTimeOutActive ? &mWriteTimeOut : NULL));

        if (result < 0) {
            DWORD errorCode = ::WSAGetLastError();

            if (errno == EINTR) {
                // Debug message: write was interrupted but we will cycle around and try again...
                continue;
            }

            // In case of SOCKET_ERROR
            _throwSocketError(false, "VSocket::write - ::select failed.", mSocketID, errorCode, result);
        }
        else if (result == 0) {
            throw VSocketException(ExceptionErrorCodes::SocketErrors::SOCKET_ERROR_WRITE_TIMED_OUT, VSTRING_FORMAT("VSocket::write select timed out on socket %d", mSocketID));
        }

        /////////////////////////////////////////////////
        // ARGO-60089 - babune
        // This is to check for BUFFER full condition
        // and log every time interval condition.
        if (bytesRemainingToWrite >= kDefaultBufferSize){// && (now - mLastEventTime >= VDuration::MINUTE())){
            VLOGGER_LEVEL(VLoggerLevel::INFO, VSTRING_FORMAT("[PerfStats]-Buffer size is full on socket %d send of length %d", mSocketID, bytesRemainingToWrite));

            mLastEventTime = now;
        }
        ////////////////////////////////////////////////////

        dataBuffer.buf = (CHAR*)nextBufferPositionPtr;
        dataBuffer.len = bytesRemainingToWrite;

        if (isCapturingNetworkStatistics) {
            // We are just logging the bytes we send out from the app layer. The lower layers would add to the count which is not captured here.
            // This is not the ideal way to calculate the 'time' either. The time logged here would just be the time taken to copy our data
            // to the lower (network) stack's send buffer.
            // Better way to do this is to make our Rx/Tx calls overlapped (setting a chunk size) and use the completion callback.
            log.startTransaction();
        }

        result = ::WSASend(mSocketID, &dataBuffer, 1, &actualBytesWritten, 0, NULL, NULL);

        if (isCapturingNetworkStatistics) {
            log.completeTransaction(actualBytesWritten);
        }

        numBytesWritten = (int)actualBytesWritten;

        if (result == SOCKET_ERROR) {
            DWORD errorCode = ::WSAGetLastError();

            VString errorMessagePrefix = VSTRING_FORMAT("VSocket::write - ::WSASend failed to send data. bytesWrittenSoFar=%d.", numBytesWrittenDuringThisSend);

            bool shouldLogAsError = !_isCommonSocketTeardownError(errorCode);

            _throwSocketError(shouldLogAsError, errorMessagePrefix, mSocketID, errorCode, result);
        }

        if (numBytesWritten != bytesRemainingToWrite) {
            // Debug message: write was only partially completed so we will cycle around and write the rest...
        }
        else {
            // This is where you could put debug/trace-mode socket write logging output....
        }

        bytesRemainingToWrite -= numBytesWritten;
        nextBufferPositionPtr += numBytesWritten;

        numBytesWrittenDuringThisSend += numBytesWritten;
        mNumBytesWritten += numBytesWritten;
    }

    if (isCapturingNetworkStatistics) {
        _addTxTransactionLog(log);
    }

    return (numBytesToWrite - bytesRemainingToWrite);

}

void VSocket::discoverHostAndPort() {
    VSocklenT           namelen = sizeof(struct sockaddr_in);
    struct sockaddr_in  info;

    int result = ::getpeername(mSocketID, (struct sockaddr*) &info, &namelen);

    if (result == SOCKET_ERROR) {
        DWORD errorCode = ::WSAGetLastError();

        _throwSocketError(true, "VSocket::discoverHostAndPort - ::getpeername failed.", mSocketID, errorCode, result);
    }

    int portNumber = (int) V_BYTESWAP_NTOH_S16_GET(static_cast<Vs16>(info.sin_port));

	// Disabling this warning locally as fixing the warning would cause VC8 build to fail.
#pragma warning(push)
#pragma warning(disable:4996)
    const char* name = ::inet_ntoa(info.sin_addr);
#pragma warning(pop)

    if (name == NULL) {
        DWORD errorCode = ::WSAGetLastError();

        _throwSocketError(false, "VSocket::discoverHostAndPort - ::inet_ntoa failed.", mSocketID, errorCode, result);
    }

    this->setHostAndPort(VSTRING_COPY(name), portNumber);
}

void VSocket::closeRead() {
    if (mReadShutDown) {
        return;
    }

    mReadShutDown = true;

    // Shut down the Rx.
    int result = ::shutdown(mSocketID, SHUT_RD);

    if (result == SOCKET_ERROR) {
        DWORD errorCode = ::WSAGetLastError();

        _throwSocketError(false, "VSocket::closeRead - ::shutdown (Read) failed. Unable to shut down read operations.", mSocketID, errorCode, result);
    }
}

void VSocket::closeWrite() {
    if (mWriteShutDown) {
        return;
    }

    mWriteShutDown = true;

    // Shut down the Tx.
    int result = ::shutdown(mSocketID, SHUT_WR);

    if (result == SOCKET_ERROR) {
        DWORD errorCode = ::WSAGetLastError();

        _throwSocketError(false, "VSocket::closeWrite - ::shutdown (Write) failed. Unable to shut down write operations.", mSocketID, errorCode, result);
    }
}

void VSocket::setSockOpt(int level, int name, void* valuePtr, int valueLength) {
    int result = ::setsockopt(mSocketID, level, name, (char*) valuePtr, valueLength);

    if (result == SOCKET_ERROR) {
        DWORD errorCode = ::WSAGetLastError();

        _throwSocketError(false, "VSocket::setSockOpt - ::setsockopt failed. Unable to set socket options.", mSocketID, errorCode, result);
    }
}

void VSocket::_connect() {

    // Choosing 'Overlapped' mode to allow multiple, simultaneous operations. 
    // We don't do overlapped IO anywhere but, since we were using ::socket API before - which defaults to overlapped mode - we retain the overlapped mode.
    VSocketID socketID = ::WSASocketW(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (socketID == kNoSocketID) {
        // Previous logic handled this the same way
        DWORD errorCode = ::WSAGetLastError();
        VString errorMessage = VSTRING_FORMAT("VSocket::connect - Unable to create a socket Address: >%s<, error code: >%d<", mHostName.chars(), errorCode);
        VLOGGER_LEVEL(VLoggerLevel::ERROR, errorMessage);

        mSocketID = socketID;

        mReadShutDown = false;
        mWriteShutDown = false;
        return;
    }

    // TODO sockaddr_in does not support IPv6
    struct sockaddr_in  address;

    ::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = (u_short)V_BYTESWAP_HTON_S16_GET(static_cast<Vs16>(mPortNumber));

    VStringVector names = VSocketBase::resolveHostName(mHostName);
    if (names.empty()) {
        VString errorHostName = VSTRING_FORMAT("VSocket::connect - ::resolveHostName failed to convert from string to ip address. Address: %s,", mHostName.chars());
        _throwSocketError(false, errorHostName, kNoSocketID, 0);
    }

    for (VStringVector::const_iterator i = names.begin(); i != names.end(); ++i) {

        const char* hostName = (const char*)(*i);

        // Disabling this warning locally as fixing the warning would cause VC8 build to fail.
#pragma warning(push)
#pragma warning(disable:4996)
        unsigned long binaryAddress = ::inet_addr(hostName);
#pragma warning (pop)

        if (binaryAddress == INADDR_NONE || binaryAddress == INADDR_ANY) {
            // all the IPv6s come here.
            continue;
        }

        address.sin_addr.s_addr = binaryAddress;

        int length = sizeof(struct sockaddr_in);

        // If we are not supporting Windows systems earlier to Vista, we could use WSAConnectByList or WSAConnectByName
        if (::WSAConnect(socketID, (const sockaddr*)&address, length, NULL, NULL, NULL, NULL) == SOCKET_ERROR) {
            DWORD errorCode = ::WSAGetLastError();

            // Cleanup on failure
            ::closesocket(socketID);

            VString errorMessage = VSTRING_FORMAT("VSocket::connect - Unable to connect to %s:%d. Code %d", mHostName.chars(), mPortNumber, errorCode);
            VLOGGER_LEVEL(VLoggerLevel::ERROR, errorMessage);

            // This ip address failed - the next one may pass
            continue;
        }

        mSocketID = socketID;

        mReadShutDown = false;
        mWriteShutDown = false;
        return;
    }

    VString errorHostName = VSTRING_FORMAT("VSocket::connect - no ip address could be connected to for host >%s<", mHostName.chars());
    _throwSocketError(false, errorHostName, kNoSocketID, 0);
}

void VSocket::_listen(const VString& bindAddress, int backlog) {
    VSocketID           listenSockID = kNoSocketID;
    struct sockaddr_in  info;
    int                 infoLength = sizeof(info);
    const int           on = 1;
    int                 result;

#ifdef XPS_SERVER

    if (mPortNumber == XPS_DEBUG_PORT_SSH) {

        sshbind = ssh_bind_new();

        VString hostIPAddress = NULL;
        const void* address;

        if (bindAddress.isEmpty()) {
            address = 0;
        }

        else {                  // get the default IP so that we can use "localhost"
            char ipAddr[64];
#pragma warning(disable:4996)
            unsigned long binaryAddress = ::inet_addr(bindAddress);
            sprintf(ipAddr, "%ld", binaryAddress);
            address = ipAddr;
        }

        const void* keyFolder = "C:\\ProgramData\\Navis\\SSH\\navis_ssh_key";     // set directory for SSH Keys

        ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDADDR, address);
        ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDPORT, &mPortNumber);
        ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_RSAKEY, keyFolder);

        VLOGGER_INFO("SSH Server :: Listening for incoming connecitons...");

        int r = ssh_bind_listen(sshbind);
        if (r < 0) {
            VLOGGER_WARN(VSTRING_FORMAT(NAV_LITERAL("SSH Server :: Error listening to socket: %s"), ssh_get_error(sshbind)));
            std::string exceptionString("SSH Server :: SSHSocketError : Failed to listen on sshbind");
            VLOGGER_WARN("SSH Server :: SSHSocketError : Failed to listen on sshbind");
            throw exceptionString;
        }
        mSocketID = ssh_bind_get_fd(sshbind);
        sshSessionMap.insert(std::make_pair(mSocketID, std::make_pair((ssh_session)NULL, (ssh_channel)NULL)));   // insert bind SocketID into map
        return;
    }

#endif

    ::memset(&info, 0, sizeof(info));
    info.sin_family = AF_INET;
    info.sin_port = (u_short)V_BYTESWAP_HTON_S16_GET(static_cast<Vs16>(mPortNumber));

    if (bindAddress.isEmpty()) {
        info.sin_addr.s_addr = INADDR_ANY;
    }
    else {
        // Disabling this warning locally as fixing the warning would cause VC8 build to fail.
#pragma warning(push)
#pragma warning(disable:4996)
        unsigned long binaryAddress = ::inet_addr(bindAddress);
#pragma warning(pop)

        if (binaryAddress == INADDR_NONE || binaryAddress == INADDR_ANY) {
            DWORD errorCode = ::WSAGetLastError();

            VString errorMessagePrefix = VSTRING_FORMAT("VSocket::connect - ::InetPton failed to convert from string to numeric address. Address=%s,", bindAddress.chars());

            _throwSocketError(false, errorMessagePrefix, kNoSocketID, errorCode);
        }

        info.sin_addr.s_addr = binaryAddress;
    }

    // AF_INET                - Use IPv4
    // SOCK_STREAM            - Connection-oriented stream.
    // 0                      - We don't specify the protocol. We would use the default protocol defined by the network service provider.
    // NULL                   - No protocol info is specified.
    // 0                      - No socket group
    // WSA_FLAG_OVERLAPPED    - Allows multiple, simultaneous operations on the socket.
    listenSockID = ::WSASocketW(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

    if (listenSockID == kNoSocketID) {
        DWORD errorCode = ::WSAGetLastError();

        _throwSocketError(false, "VSocket::listen - ::WSASocket() failed.", listenSockID, errorCode);
    }

    // SOL_SOCKET    - Setting options at socket level.
    // SO_REUSEADDR  - Since we set this before calling ::bind, we allow multiple sockets to bind to same address.
    // on            - Choosing Boolean socket options (by setting 'optVal' (3rd) parameter of ::setsockopt to non-zero integer).
    result = ::setsockopt(listenSockID, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));
    if (result == SOCKET_ERROR) {
        // Setting options failed.
        DWORD error = ::WSAGetLastError();

        ::closesocket(listenSockID);

        _throwSocketError(false, "VSocket::listen - ::setsockopt() failed.", listenSockID, error, result);
    }

    result = ::bind(listenSockID, (const sockaddr*)&info, infoLength);
    if (result == SOCKET_ERROR) {
        // Bind failed.
        DWORD errorCode = ::WSAGetLastError();

        ::closesocket(listenSockID);

        VString errorMessagePrefix = VSTRING_FORMAT("VSocket::listen - ::bind() for port %d failed.", mPortNumber);

        _throwSocketError(false, errorMessagePrefix, listenSockID, errorCode, result);
    }

    result = ::listen(listenSockID, backlog);
    if (result == SOCKET_ERROR) {
        // Listen failed.
        DWORD errorCode = ::WSAGetLastError();

        ::closesocket(listenSockID);

        VString errorMessagePrefix = VSTRING_FORMAT("VSocket::listen - ::listen() for port %d failed.", mPortNumber);

        _throwSocketError(false, errorMessagePrefix, listenSockID, errorCode, result);
    }

    mSocketID = listenSockID;

}

void VSocket::close() {
    if (mSocketID != kNoSocketID) {
        closeRead();
        closeWrite();
    }

    // Base would close the socket and disown the socket Id.
    VSocketBase::close();
}

int VSocket::_getNativeErrorCode(DWORD errorCode) const {
    int nativeErrorCode = static_cast<int>(errorCode);

    switch (errorCode) {
    case WSAENOTSOCK:
        nativeErrorCode = ExceptionErrorCodes::SocketErrors::SOCKET_ERROR_INVALID_SOCKET; break;

    case WSAECONNABORTED:
        nativeErrorCode = ExceptionErrorCodes::SocketErrors::SOCKET_ERROR_CONNECTION_ABORTED_BY_REMOTE_HOST; break;
    }

    return nativeErrorCode;
}

bool VSocket::_isCommonSocketTeardownError(DWORD errorCode) {
    return ((errorCode == WSAENOTSOCK) || (errorCode == WSAEINTR) || (errorCode == WSAECONNABORTED) || (errorCode == WSAESHUTDOWN));
}

void VSocket::_frameErrorMessage(const VString& messagePrefix, VSocketID socketID, DWORD errorCode, int nativeErrorCode, /*[OUT]*/ VString& errorMessage) {
    errorMessage = VSTRING_FORMAT("%s Socket: %s, Error: %s, Native Error Code: %d.",
                                    messagePrefix.chars(),
                                    (socketID == kNoSocketID ? "[N/A]" : (boost::lexical_cast<std::string>(socketID)).c_str()),
                                    WSAUtils::ErrorMessage(errorCode).c_str(), nativeErrorCode);
}

void VSocket::_frameErrorMessage(const VString& messagePrefix, VSocketID socketID, DWORD errorCode, int nativeErrorCode, int result, /*[OUT]*/ VString& errorMessage) {
    errorMessage = VSTRING_FORMAT("%s Socket: %s, Result: %d, Error: %s, Native Error Code: %d",
                                    messagePrefix.chars(),
                                    (socketID == kNoSocketID ? "[N/A]" : (boost::lexical_cast<std::string>(socketID)).c_str()),
                                    result,
                                    WSAUtils::ErrorMessage(errorCode).c_str(), nativeErrorCode);
}

void VSocket::_throwSocketError(bool logAsError, const VString& messagePrefix, VSocketID socketID, DWORD errorCode) {
    int nativeErrorCode = _getNativeErrorCode(errorCode);

    VString errorMessage;

    _frameErrorMessage(messagePrefix, socketID, errorCode, nativeErrorCode, errorMessage);

    if (logAsError) {
        VLOGGER_ERROR(errorMessage);
    } else {
        VLOGGER_INFO(errorMessage);
    }

    throw VSocketException(nativeErrorCode, errorMessage);
}

void VSocket::_throwSocketError(bool logAsError, const VString& messagePrefix, VSocketID socketID, DWORD errorCode, int result) {
    int nativeErrorCode = _getNativeErrorCode(errorCode);

    VString errorMessage;

    _frameErrorMessage(messagePrefix, socketID, errorCode, nativeErrorCode, result, errorMessage);

    if (logAsError) {
        VLOGGER_ERROR(errorMessage);
    } else {
        VLOGGER_INFO(errorMessage);
    }

    throw VSocketException(nativeErrorCode, errorMessage);
}

/*
Function calculate checksum
*/
unsigned short VSocket::_in_cksum(unsigned short* ptr, int nbytes) {
    register long sum;
    u_short oddbyte;
    register u_short answer;

    sum = 0;
    while (nbytes > 1) {
        sum += *ptr++;
        nbytes -= 2;
    }

    if (nbytes == 1) {
        oddbyte = 0;
        *((u_char*)&oddbyte) = *(u_char*)ptr;
        sum += oddbyte;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    answer = ~sum;

    return (answer);
}

int VSocket::checkIsValidPort(char* ip, int port) {
    WSADATA wsa;
    SOCKET s;
    struct sockaddr_in server;

    if (IS_RESERVERVED_PORT(port))
        return icmpReservePort;

    //Initialising Winsock...
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        //Failed. Error Code WSAGetLastError());
        return icmpStartUpErr;
    }

    //Create a socket
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
    {
        //Could not create socket 
        return icmpStartUpErr;
    }

    //Socket created
    inet_pton(AF_INET, ip, &(server.sin_addr));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    //Connect to remote server
    /*if (connect(s, (struct sockaddr*)&server, sizeof(server)) < 0)
    {
        //connect error
        closesocket(s);
        return icmpPortInvalid;
    }*/
    try {
        setHostAndPort(ip, port);
        connect(); // throws VException if can't connect
    }
    catch (const VException& ex) {
        closesocket(s);
        VString errorMessage =
            VSTRING_FORMAT(NAV_LITERAL("Unable to connect to server at %s:%d. Network error message: %s"),
                ip, port, ex.what());
        VLOGGER_LEVEL(VLoggerLevel::ERROR, errorMessage);
        return SSL_ERROR;
    }

    closesocket(s);
    return icmpPortValid;
}

int VSocket::getIPStatus(VString ipAddr, int port) {

    char* packet, * data = NULL;
    SOCKET s;
    int packet_size, payload_size = 32;
    struct icmphdr* icmph = NULL;
    struct sockaddr_in dest, from;

    //Initialise Winsock
    WSADATA wsock;

    if (WSAStartup(MAKEWORD(2, 2), &wsock) != 0)
    {
        return icmpStartUpErr;
    }

    //Create Raw ICMP Packet
    if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == SOCKET_ERROR)
    {
        return icmpCreateSockErr;
    }

    dest.sin_family = AF_INET;
    inet_pton(AF_INET, ipAddr.chars(), &(dest.sin_addr));
    //dest.sin_addr.s_addr = inet_addr(ipAddr);

    packet_size = sizeof(struct icmphdr) + payload_size;
    packet = (char*)malloc(packet_size);

    //zero out the packet buffer
    memset(packet, 0, packet_size);

    icmph = (struct icmphdr*)packet;
    icmph->type = ICMP_ECHO;
    icmph->code = 0;
    icmph->un.echo.sequence = rand();
    icmph->un.echo.id = rand();

    // Initialize the TCP payload to some rubbish
    data = packet + sizeof(struct icmphdr);
    memset(data, '^', payload_size);

    //checksum
    icmph->checksum = 0;
    icmph->checksum = _in_cksum((unsigned short*)icmph, packet_size);

    int recvLen = 32;
    char* recvBuf = NULL;
    struct timeval tv;
    fd_set rfds;
    int retval = 0, res = 0;
    int retryCount = 0;

    while (1)
    {
        if (retryCount == MAX_RETRY_COUNT) {
            return icmpHostNoReply;
        }

        if (sendto(s, packet, packet_size, 0, (struct sockaddr*)&dest, sizeof(dest)) == SOCKET_ERROR)
        {
            return icmpSendErr;
        }

        FD_ZERO(&rfds);
        FD_SET(s, &rfds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        recvBuf = (char*)malloc(packet_size);

        retval = select(s + 1, &rfds, NULL, NULL, &tv);

        if (retval > 0) {
            res = recvfrom(s, recvBuf, packet_size, 0, (struct sockaddr*)&from, &recvLen);

            //Got a response... So ip is valid. Check is the port is valid & is listening 
            int result = checkIsValidPort((char*)ipAddr.chars(), port);

            if (result == icmpPortValid)
                return icmpSuccess;
            else
                return result;
        }

        retryCount++;

        ::Sleep(100);
    }

    return 0;
}

int VSocket::connectToHttpsServer(const char* ip, const char* p, SSL** assl) {
    SOCKET s = 0;
    WORD wVersionRequested;
    WSADATA wsaData;
    int err_t = 0;
    u_short port;
    SSL* ssl;
    int sock;

    port = (u_short)atoi(p);
    wVersionRequested = MAKEWORD(1, 1);
    err_t = WSAStartup(wVersionRequested, &wsaData);
    if (err_t != 0) {
        return SSL_ERROR;
    }
    s = ::WSASocketW(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
#pragma warning(disable:4996)
    sa.sin_addr.s_addr = inet_addr(ip);
    sa.sin_port = htons(port);
    //socklen_t socklen = sizeof(sa);
    /*if (connect(s, (struct sockaddr*)&sa, socklen)) {
        return SSL_ERROR;
    }*/
    try {
        setHostAndPort(ip, port);
        connect(); // throws VException if can't connect
    }
    catch (const VException& ex) {
        closesocket(s);
        VString errorMessage = 
            VSTRING_FORMAT(NAV_LITERAL("Unable to connect to server at %s:%d. Network error message: %s"),
                ip, port, ex.what());
        VLOGGER_LEVEL(VLoggerLevel::ERROR, errorMessage);
        return SSL_ERROR;
    }
    
    SSL_library_init();
    SSLeay_add_ssl_algorithms();
    SSL_load_error_strings();
    const SSL_METHOD* meth = TLSv1_2_client_method();
    SSL_CTX* ctx = SSL_CTX_new(meth);
    *assl = SSL_new(ctx);
    ssl = *assl;
    if (!ssl) {
        return SSL_ERROR;
    }

    sock = SSL_get_fd(ssl);
    SSL_set_fd(ssl, s);

    int err = SSL_connect(ssl);
    if (err <= 0) {
        return SSL_ERROR;
    }
    return 0;
}
