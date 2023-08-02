/*
Copyright c1997-2011 Trygve Isaacson. All rights reserved.
This file is part of the Code Vault version 3.3
http://www.bombaydigital.com/
*/

/** @file */

#include "vsocket.h"
#include "vtypes_internal.h"

#include <signal.h>

#ifdef sun
#include <sys/proc.h>
#endif

#include <sys/ioctl.h>
#include <ifaddrs.h>

#include "vexception.h"
#include "vmutexlocker.h"

// _Linux_ _SSH_
#ifdef XPS_SERVER
#include <libssh/libssh.h>
#include <boost/filesystem.hpp>
/*
 * DATABUF structure definition
 */
typedef struct _DATABUF {
    Vu64 len;  /* the length of the buffer  */
    char* buf; /* the pointer to the buffer */
} DATABUF;

#endif
V_STATIC_INIT_TRACE

// On Mac OS X, we disable SIGPIPE in VSocketBase::setDefaultSockOpt().
// For other Unix platforms, we specify it in the flags of each send()/recv() call via this parameter.
#ifdef VPLATFORM_MAC
#define VSOCKET_DEFAULT_SEND_FLAGS 0
#define VSOCKET_DEFAULT_RECV_FLAGS 0
#else
#define VSOCKET_DEFAULT_SEND_FLAGS MSG_NOSIGNAL
#define VSOCKET_DEFAULT_RECV_FLAGS MSG_NOSIGNAL
#endif

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

    //lint -e421 -e923 " Caution -- function 'signal(int, void (*)(int))' is considered dangerous [MISRA Rule 123]"
    ::signal(SIGPIPE, SIG_IGN);

    // Should log the error if the call failed.

    VSocketManager::Initialized = true;

    VLOGGER_LEVEL(VLoggerLevel::INFO, "Sockets initialized.");

    return;
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

    // TODO: Do BSD-specific cleanup here.

    return true;
}

// This is the one VSocketBase function that must implemented per platform, so we do it here.
// This kind of highlights that the original VSocketBase <- VSocket hierarchy is probably due
// for restructuring so that we simply break the implementation of 1 class into two .cpp files
// where the second file contains the per-platform implementation, but named by the same class,
// as we do now with, for example, VFSNode and its platform-specific parts.
// static
VNetworkInterfaceList VSocketBase::enumerateNetworkInterfaces() {
    VNetworkInterfaceList interfaces;
    struct ifaddrs* interfacesDataPtr = NULL;
    int result = ::getifaddrs(&interfacesDataPtr);
    if (result != 0) {
        throw VStackTraceException(VSTRING_FORMAT("VSocketBase::enumerateNetworkInterfaces: getifaddrs returned %d, errno = %d (%s)", result, errno, ::strerror(errno)));
    }

    struct ifaddrs* intfPtr = interfacesDataPtr;
    while (intfPtr != NULL) {
        if (intfPtr->ifa_addr != NULL) {
            VNetworkInterfaceInfo info;
            info.mFamily = intfPtr->ifa_addr->sa_family;
            info.mName.copyFromCString(intfPtr->ifa_name);
            // AF_INET6 will work just fine here, too. But hold off until we can verify callers can successfully use IPV6 address strings to listen, connect, etc.
            if ((info.mFamily == AF_INET) && (info.mName != "lo0")) { // Internet interfaces only, and skip the loopback address.
                info.mAddress.preflight(255);
                char* buffer = info.mAddress.buffer();
                ::inet_ntop(intfPtr->ifa_addr->sa_family, &((struct sockaddr_in*)intfPtr->ifa_addr)->sin_addr, buffer, 255);
                info.mAddress.postflight((int) ::strlen(buffer));

                // Check for "lo0" above should filter out 127.x.x.x (loopback addresses), but in case it doesn't, check it.
                if (!info.mAddress.startsWith("127.")) {
                    interfaces.push_back(info);
                }
            }
        }

        intfPtr = intfPtr->ifa_next;
    }

    ::freeifaddrs(interfacesDataPtr);
    return interfaces;
}

static const int MAX_ADDRSTRLEN = V_MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN);
// Another platform-specific implementation of a function defined by the base class API.
// static
VString VSocketBase::addrinfoToIPAddressString(const VString& hostName, const struct addrinfo* info) {
    void* addr;
    if (info->ai_family == AF_INET) {
        addr = (void*)&(((struct sockaddr_in*)info->ai_addr)->sin_addr);
    }
    else if (info->ai_family == AF_INET6) {
        addr = (void*)&(((struct sockaddr_in6*)info->ai_addr)->sin6_addr);
    }
    else {
     // We don't know how to access the addr for other family types. They could conceivably be added.
        throw VException(VSTRING_FORMAT("VSocketBase::addrinfoToIPAddressString(%s): An invalid family (%d) other than AF_INET or AF_INET6 was specified.", hostName.chars(), info->ai_family));
    }

    VString result;
    result.preflight(MAX_ADDRSTRLEN);
    const char* buf = ::inet_ntop(info->ai_family, addr, result.buffer(), MAX_ADDRSTRLEN);
    if (buf == NULL) {
        throw VException(errno, VSTRING_FORMAT("VSocketBase::addrinfoToIPAddressString(%s): inet_ntop() failed. Error='%s'.", hostName.chars(), ::strerror(errno)));
    }
    result.postflight((int) ::strlen(buf));

    return result;
}

VSocket::VSocket(VSocketID id)
    : VSocketBase(id)
{
}

VSocket::VSocket(const VString& hostName, int portNumber)
    : VSocketBase(hostName, portNumber)
{
}

VSocket::~VSocket() {
    // Note: base class destructor does a close() of the socket if it is open.
    // _Linux_ _SSH_
#ifdef XPS_SERVER
    sshDeleteSession = true;
#endif
}

int VSocket::available() {
    int numBytesAvailable = 0;

    int result = ::ioctl(mSocketID, FIONREAD, &numBytesAvailable);

    if (result == -1) {
        throw VStackTraceException(errno, VSTRING_FORMAT("VSocket[%s] available: Ioctl failed. Result=%d. Error='%s'.", mSocketName.chars(), result, ::strerror(errno)));
    }

    return numBytesAvailable;
}

int VSocket::read(Vu8* buffer, int numBytesToRead) {
    if (mSocketID < 0) {
        throw VStackTraceException(VSTRING_FORMAT("VSocket[%s] read: Invalid socket ID %d.", mSocketName.chars(), mSocketID));
    }

    int     bytesRemainingToRead = numBytesToRead;
    Vu8* nextBufferPositionPtr = buffer;
    int     theNumBytesRead = 0;
    DATABUF dataBuffer;

 // _Linux_ _SSH_
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
            VLOGGER_WARN(VSTRING_FORMAT(("SSH Server :: Session on socket %d closed : client disconnected"), mSocketID));
            sshSessionMap.erase(mapFind->first);
            return 0;
        }

        dataBuffer.buf = (char*)nextBufferPositionPtr;
        dataBuffer.len = bytesRemainingToRead;

        bytesReadSSH = ssh_channel_read(sshChannel, dataBuffer.buf, dataBuffer.len, 0);
        bytesRemainingToRead -= bytesReadSSH;

        return (numBytesToRead - bytesRemainingToRead);

    }

#endif

    while (bytesRemainingToRead > 0) {
        theNumBytesRead = (int) ::recv(mSocketID, (char*)nextBufferPositionPtr, (VSizeType)bytesRemainingToRead, VSOCKET_DEFAULT_RECV_FLAGS);

        if (theNumBytesRead < 0) {
            if (errno == EPIPE) {
                VLOGGER_ERROR(VSTRING_FORMAT("VSocket[%s] read: EPIPE <%s>", mSocketName.chars(), ::strerror(errno)));
                throw VSocketClosedException(errno, VSTRING_FORMAT("VSocket[%s] read: Socket has closed (EPIPE).", mSocketName.chars()));
            }
            else {
                VLOGGER_ERROR(VSTRING_FORMAT("VSocket[%s] read: other recv <%s>", mSocketName.chars(), ::strerror(errno)));
                throw VException(errno, VSTRING_FORMAT("VSocket[%s] read: Recv failed. Result=%d. Error='%s'.", mSocketName.chars(), theNumBytesRead, ::strerror(errno)));
            }
        }
        else if (theNumBytesRead == 0) {
            if (mRequireReadAll) {
                VLOGGER_WARN(VSTRING_FORMAT("VSocket[%s] read: closed <%s>", mSocketName.chars(), ::strerror(errno)));
                throw VSocketClosedException(0, VSTRING_FORMAT("VSocket[%s] read: Socket has closed.", mSocketName.chars()));
            }
            else {
                VLOGGER_WARN(VSTRING_FORMAT("VSocket[%s] read: partial <%s>", mSocketName.chars(), ::strerror(errno)));
                break;    // got successful but partial read, caller will have to keep reading
            }
        }
        else {
            VLOGGER_TRACE(VSTRING_FORMAT("VSocket[%s] read: recv <%d> bytes", mSocketName.chars(), theNumBytesRead));
        }

        bytesRemainingToRead -= theNumBytesRead;
        nextBufferPositionPtr += theNumBytesRead;

        mNumBytesRead += theNumBytesRead;
    }

    mLastEventTime.setNow();

    return (numBytesToRead - bytesRemainingToRead);
}

int VSocket::write(const Vu8* buffer, int numBytesToWrite) {
    if (mSocketID < 0) {
        throw VStackTraceException(VSTRING_FORMAT("VSocket[%s] write: Invalid socket ID %d.", mSocketName.chars(), mSocketID));
    }

    const Vu8* nextBufferPositionPtr = buffer;
    int         bytesRemainingToWrite = numBytesToWrite;
    int         theNumBytesWritten;
    DATABUF     dataBuffer;

// _Linux_ _SSH_
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
            VLOGGER_WARN(VSTRING_FORMAT(("SSH Server :: Session on socket %d closed : client disconnected"), mSocketID));
            sshSessionMap.erase(mapFind->first);
            return 0;
        }

        int bytesWrittenSSH = 0;

        dataBuffer.buf = (char*)nextBufferPositionPtr;
        dataBuffer.len = bytesRemainingToWrite;

        bytesWrittenSSH = ssh_channel_write(sshChannel, dataBuffer.buf, dataBuffer.len);
        bytesRemainingToWrite -= bytesWrittenSSH;

        return (numBytesToWrite - bytesRemainingToWrite);
    }

#endif

    while (bytesRemainingToWrite > 0) {
        theNumBytesWritten = (int) ::send(mSocketID, nextBufferPositionPtr, (VSizeType)bytesRemainingToWrite, VSOCKET_DEFAULT_SEND_FLAGS);

        if (theNumBytesWritten <= 0) {
            if (errno == EPIPE) {
                VLOGGER_ERROR(VSTRING_FORMAT("VSocket[%s] write: EPIPE <%s>", mSocketName.chars(), ::strerror(errno)));
                throw VSocketClosedException(errno, VSTRING_FORMAT("VSocket[%s] write: Socket has closed (EPIPE).", mSocketName.chars()));
            }
            else {
                VLOGGER_ERROR(VSTRING_FORMAT("VSocket[%s] write: other <%s>", mSocketName.chars(), ::strerror(errno)));
                throw VException(errno, VSTRING_FORMAT("VSocket[%s] write: Send failed. Error='%s'.", mSocketName.chars(), ::strerror(errno)));
            }
        }
        else if (theNumBytesWritten != bytesRemainingToWrite) {
            VLOGGER_WARN(VSTRING_FORMAT("VSocket[%s] write: <%d> written != <%d> remaining", mSocketName.chars(), theNumBytesWritten, bytesRemainingToWrite));
            // Debug message: write was only partially completed so we will cycle around and write the rest...
        }
        else {
            VLOGGER_TRACE(VSTRING_FORMAT("VSocket[%s] write: send <%d> bytes", mSocketName.chars(), theNumBytesWritten));
        }

        bytesRemainingToWrite -= theNumBytesWritten;
        nextBufferPositionPtr += theNumBytesWritten;

        mNumBytesWritten += theNumBytesWritten;
    }

    return (numBytesToWrite - bytesRemainingToWrite);
}

void VSocket::discoverHostAndPort() {
    struct sockaddr_in  info;
    VSocklenT           infoLength = sizeof(info);

    int result = ::getpeername(mSocketID, (struct sockaddr*)&info, &infoLength);
    if (result != 0) {
        throw VStackTraceException(errno, VSTRING_FORMAT("VSocket[%s] discoverHostAndPort: Getpeername failed. Error='%s'.", mSocketName.chars(), ::strerror(errno)));
    }

    int portNumber = (int)V_BYTESWAP_NTOH_S16_GET(static_cast<Vs16>(info.sin_port));

    const char* name = ::inet_ntoa(info.sin_addr); // addr2ascii is preferred but is not yet standard:
    //const char* name = ::addr2ascii(AF_INET, info.sin_addr, sizeof(info.sin_addr), NULL);

    this->setHostAndPort(VSTRING_COPY(name), portNumber);
}

void VSocket::closeRead() {
    int result = ::shutdown(mSocketID, SHUT_RD);

    if (result < 0) {
        throw VException(VSTRING_FORMAT("VSocket[%s] closeRead: Unable to shut down socket.", mSocketName.chars()));
    }
}

void VSocket::closeWrite() {
    int result = ::shutdown(mSocketID, SHUT_WR);

    if (result < 0) {
        throw VException(VSTRING_FORMAT("VSocket[%s] closeWrite: Unable to shut down socket.", mSocketName.chars()));
    }
}

void VSocket::setSockOpt(int level, int name, void* valuePtr, int valueLength) {
    (void) ::setsockopt(mSocketID, level, name, valuePtr, valueLength);
}

#ifndef V_BSD_ENHANCED_SOCKETS

void VSocket::_connect() {

    // windows does overlapped 
    VSocketID socketID = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socketID <= kNoSocketID) {
        // Previous logic handled this the same way
        VString errorMessage = VSTRING_FORMAT("VSocket::connect - Unable to create a socket Address: >%s<, error code: >%s<", mHostName.chars(), ::strerror(errno));
        VLOGGER_LEVEL(VLoggerLevel::ERROR, errorMessage);

        mSocketID = socketID;
        return;
    }

    // TODO sockaddr_in does not support IPv6
    struct sockaddr_in  address;
    int infoLength = sizeof(address);

    ::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = (in_port_t)V_BYTESWAP_HTON_S16_GET(static_cast<Vs16>(mPortNumber));

    VStringVector names = VSocketBase::resolveHostName(mHostName);
    if (names.empty()) {
        throw VException(errno, VSTRING_FORMAT("VSocket::connect - ::resolveHostName failed to convert from string to ip address. Address: >%s< Error >%s<,", mSocketName.chars(), ::strerror(errno)));
    }

    for (VStringVector::const_iterator i = names.begin(); i != names.end(); ++i) {

        const char* hostName = (const char*)(*i);
        unsigned long binaryAddress = ::inet_addr(hostName);
        if (binaryAddress == INADDR_NONE || binaryAddress == INADDR_ANY) {
            // all the IPv6s come here.
            continue;
        }

        address.sin_addr.s_addr = binaryAddress;

        int result = ::connect(socketID, (const sockaddr*)&address, infoLength);

        if (result != 0) {
            // Cleanup on failure
            vault::close(socketID);

            VString errorMessage = VSTRING_FORMAT("VSocket::connect - Unable to connect to %s:%d. Code %d", mHostName.chars(), mPortNumber, ::strerror(errno));
            VLOGGER_LEVEL(VLoggerLevel::ERROR, errorMessage);

            // This ip address failed - the next one may pass
            continue;
        }

        // the good leg
        mSocketID = socketID;
        return;
    }

    throw VException(errno, VSTRING_FORMAT("VSocket::connect - no ip address could be connected to for host >%s< Error >%s<", mSocketName.chars(), ::strerror(errno)));
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
            unsigned long binaryAddress = ::inet_addr(bindAddress);
            sprintf(ipAddr, "%ld", binaryAddress);
            address = ipAddr;
        }

        // Get the current directory for relative path instead of hard coded one(windows)
        // Which has been "C:\\ProgramData\\Navis\\SSH\\navis_ssh_key";
        std::string currPath = (".");// by default to the current directory of executable
        try {
            currPath = boost::filesystem::current_path().c_str();
        }
        catch (boost::filesystem::filesystem_error& e)
        {
            std::cerr << e.what() << '\n';
            currPath = (".");
        }

        currPath += "/SSH/navis_ssh_key";
        const char* cPath = currPath.c_str();
        const void* keyFolder = static_cast<const void*>(cPath);     // set directory for SSH Keys

        ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDADDR, address);
        ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDPORT, &mPortNumber);
        ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_RSAKEY, keyFolder);

        VLOGGER_INFO("SSH Server :: Listening for incoming connections...");

        int r = ssh_bind_listen(sshbind);
        if (r < 0) {
            VLOGGER_WARN(VSTRING_FORMAT("SSH Server :: Error listening to socket: %s", ssh_get_error(sshbind)));
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
    info.sin_port = (in_port_t)V_BYTESWAP_HTON_S16_GET(static_cast<Vs16>(mPortNumber));

    if (bindAddress.isEmpty()) {
        info.sin_addr.s_addr = INADDR_ANY;
    }
    else {
        info.sin_addr.s_addr = inet_addr(bindAddress);
    }

    listenSockID = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenSockID < 0) {
        throw VStackTraceException(errno, VSTRING_FORMAT("VSocket[%s] listen: Socket failed. Result=%d. Error='%s'.", mSocketName.chars(), listenSockID, ::strerror(errno)));
    }

    result = ::setsockopt(listenSockID, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if (result != 0) {
        throw VStackTraceException(errno, VSTRING_FORMAT("VSocket[%s] listen: Setsockopt failed. Result=%d. Error='%s'.", mSocketName.chars(), result, ::strerror(errno)));
    }

    int flags = ::fcntl(listenSockID, F_GETFL, 0);
    result = ::fcntl(listenSockID, F_SETFL, flags | O_NONBLOCK);
    if (result != 0) {
        throw VStackTraceException(errno, VSTRING_FORMAT("VSocket[%s] listen: Set non-blocking failed. Result=%d. Error='%s'.", mSocketName.chars(), result, ::strerror(errno)));
    }

    result = ::bind(listenSockID, (const sockaddr*)&info, infoLength);
    if (result != 0) {
        vault::close(listenSockID);
        throw VStackTraceException(errno, VSTRING_FORMAT("VSocket[%s] listen: Bind failed. Result=%d. Error='%s'.", mSocketName.chars(), result, ::strerror(errno)));
    }

    (void) ::listen(listenSockID, backlog);
    if (result != 0) {
        vault::close(listenSockID);
        throw VStackTraceException(errno, VSTRING_FORMAT("VSocket[%s] listen: Listen failed. Result=%d. Error='%s'.", mSocketName.chars(), result, ::strerror(errno)));
    }

    mSocketID = listenSockID;
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
    struct sockaddr_in server;

    if (IS_RESERVERVED_PORT(port))
        return icmpReservePort;

    //Initializing ...
    if (!VSocketManager::IsInitialized()) {
        VSocketManager::Initialize();
        if (!VSocketManager::IsInitialized()) {
            return icmpStartUpErr;
        }
    }

    //Create a socket
    char tempStr[256];
    VSocketID   sockID = V_NO_SOCKET_ID_CONSTANT;
    sockID = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sockID < 0) {
        sprintf(tempStr, "VSocket[%s] listen: Socket failed. Result=%d. Error='%s'.", ip, sockID, ::strerror(errno));
        return icmpCreateSockErr;
    }

    //Socket created
    inet_pton(AF_INET, ip, &(server.sin_addr));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    try {
        setHostAndPort(ip, port);
        connect(); // throws VException if can't connect
    }
    catch (const VException& ex) {
        vault::close(sockID);
        VString errorMessage =
            VSTRING_FORMAT("Unable to connect to server at %s:%d. Network error message: %s",
                ip, port, ex.what());
        VLOGGER_LEVEL(VLoggerLevel::ERROR, errorMessage);
        return SSL_ERROR;
    }

    vault::close(sockID);
    return icmpPortValid;
}

int VSocket::getIPStatus(VString ipAddr, int port) {

    char* packet, * data = NULL;
    int packet_size, payload_size = 32;
    struct icmphdr* icmph = NULL;
    struct sockaddr_in dest;

    if (!VSocketManager::IsInitialized()) {
        VSocketManager::Initialize();
        if (!VSocketManager::IsInitialized()) {
            return icmpStartUpErr;
        }
    }

    char tempStr[256];
    VSocketID   sockID = V_NO_SOCKET_ID_CONSTANT;
    sockID = ::socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockID < 0) {
        sprintf(tempStr, "VSocket[%s] listen: Socket failed. Result=%d. Error='%s'.", ipAddr.chars(), sockID, ::strerror(errno));
        return icmpCreateSockErr;
    }

    dest.sin_family = AF_INET;
    inet_pton(AF_INET, ipAddr.chars(), &(dest.sin_addr));

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

    char* recvBuf = NULL;
    struct timeval tv;
    fd_set rfds;
    int retval = 0;
    int retryCount = 0;

    while (1)
    {
        if (retryCount == MAX_RETRY_COUNT) {
            return icmpHostNoReply;
        }

        int         bytesRemainingToWrite = packet_size;
        int         theNumBytesWritten;
        theNumBytesWritten = (int) ::send(sockID, packet, (VSizeType)bytesRemainingToWrite, VSOCKET_DEFAULT_SEND_FLAGS);
        if (theNumBytesWritten <= 0) {
            return icmpSendErr;
        }

        FD_ZERO(&rfds);
        FD_SET(sockID, &rfds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        recvBuf = (char*)malloc(packet_size);

        retval = select(sockID + 1, &rfds, NULL, NULL, &tv);

        if (retval > 0) {
            int theNumBytesRead = (int) ::recv(sockID, recvBuf, (VSizeType)packet_size, VSOCKET_DEFAULT_RECV_FLAGS);
            if (theNumBytesRead < 0) {
                if (errno == EPIPE) {
                    VLOGGER_ERROR(VSTRING_FORMAT("VSocket[%s] read: EPIPE <%s>", mSocketName.chars(), ::strerror(errno)));
                    throw VSocketClosedException(errno, VSTRING_FORMAT("VSocket[%s] read: Socket has closed (EPIPE).", mSocketName.chars()));
                }
            }

            //Got a response... So ip is valid. Check is the port is valid & is listening 
            int result = checkIsValidPort((char*)ipAddr.chars(), port);

            if (result == icmpPortValid)
                return icmpSuccess;
            else
                return result;
        }

        retryCount++;

        ::sleep(100);
    }

    return 0;
}

int VSocket::connectToHttpsServer(const char* ip, const char* p, SSL** assl) {
    SSL* ssl;
    u_short port = (u_short)atoi(p);

    //Initializing ...
    if (!VSocketManager::IsInitialized()) {
        VSocketManager::Initialize();
        if (!VSocketManager::IsInitialized()) {
            return SSL_ERROR;
        }
    }

    VSocketID socketID = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socketID <= kNoSocketID) {
        // Previous logic handled this the same way
        VString errorMessage 
            = VSTRING_FORMAT("VSocket::connect - Unable to create a socket Address: >%s<, error code: >%s<",
                mHostName.chars(), ::strerror(errno));
        VLOGGER_LEVEL(VLoggerLevel::ERROR, errorMessage);

        mSocketID = socketID;
        return SSL_ERROR;
    }

    try {
        setHostAndPort(ip, port);
        connect(); // throws VException if can't connect
    }
    catch (const VException& ex) {
        // Cleanup on failure
        vault::close(socketID);
        VString errorMessage =
            VSTRING_FORMAT("Unable to connect to server at %s:%d. Network error message: %s",
                ip, port, ex.what());
        VLOGGER_LEVEL(VLoggerLevel::ERROR, errorMessage);
        return SSL_ERROR;
    }

    SSL_library_init();
    SSLeay_add_ssl_algorithms();
    SSL_load_error_strings();
    const SSL_METHOD* meth = TLS_client_method();
    SSL_CTX* ctx = SSL_CTX_new(meth);
    *assl = SSL_new(ctx);
    ssl = *assl;
    if (!ssl) {
        return SSL_ERROR;
    }

    SSL_get_fd(ssl);
    SSL_set_fd(ssl, socketID);

    int err = SSL_connect(ssl);
    if (err <= 0) {
        return SSL_ERROR;
    }
    return 0;
}

#else /* V_BSD_ENHANCED_SOCKETS version follows */

void VSocket::_connect() {
    VSocketID           socketID = kNoSocketID;
    struct addrinfo* res = NULL;

    this->_tcpGetAddrInfo(&res);

    try {
        socketID = this->_tcpConnectWAddrInfo(res);
    }
    catch (const VException& ex) {
        ::freeaddrinfo(res);
        throw;
    }

    ::freeaddrinfo(res);

    mSocketID = socketID;
}

void VSocket::_listen(const VString& bindAddress, int backlog) {
    VSocketID           listenSockID;
    int                 result;
    const int           on = 1;
    struct addrinfo     hints;
    struct addrinfo* res;
    struct addrinfo* ressave;
    VString             lastError;

    ::memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    result = this->_getAddrInfo(&hints, &res, false);
    if (result != 0) {
        throw VStackTraceException(errno, VSTRING_FORMAT("VSocket[%s] listen: GetAddrInfo failed. Result=%d. Error='%s'.", mSocketName.chars(), result, ::gai_strerror(errno)));
    }

    ressave = res;

    do {
        listenSockID = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);

        if (listenSockID < 0) {
            // Error on this interface, we'll try the next one.
            lastError.format("VSocket[%s] listen: Socket failed. ID=%d. Error='%s'.", mSocketName.chars(), listenSocketID, ::strerror(errno));
            continue;
        }

        result = ::setsockopt(listenSockID, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
        if (result != 0) {
            // Error on this interface, we'll try the next one.
            lastError.format("VSocket[%s] listen: Setsockopt failed. Result=%d. Error='%s'.", mSocketName.chars(), result, ::strerror(errno));
            continue;
        }

        result = ::bind(listenSockID, res->ai_addr, res->ai_addrlen);

        if (result == 0) {
            // Success -- break out of loop to continue setting up listen.
            break;
        }

        // Error on this interface, we'll try the next one.
        lastError.format("VSocket[%s] listen: Bind failed. Result=%d. Error='%s'.", mSocketName.chars(), result, ::strerror(errno));
        vault::close(listenSockID);

    } while ((res = res->ai_next) != NULL);

    if (res == NULL) {
        throw VStackTraceException(errno, lastError);
    }

    if (lastError.length() != 0) {
        VString s(VSTRING_ARGS("VSocket[%s] listen: Bind succeeded after earlier error: ", mSocketName.chars()));
        std::cout << s << lastError << std::endl;
    }

    result = ::listen(listenSockID, backlog);

    ::freeaddrinfo(ressave);

    if (result != 0) {
        vault::close(listenSockID);
        throw VException(errno, VSTRING_FORMAT("VSocket[%s] listen: Listen failed. Result=%d. Error='%s'.", mSocketName.chars(), result, ::strerror(errno)));
    }

    mSocketID = listenSockID;
}

void VSocket::_tcpGetAddrInfo(struct addrinfo** res) {
    struct addrinfo hints;
    int             result;

    ::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    result = this->_getAddrInfo(&hints, res);

    if (result != 0) {
        throw VException(errno, VSTRING_FORMAT("VSocket[%s] _tcpGetAddrInfo: GetAddrInfo failed. Result=%d. Error='%s'.", mSocketName.chars(), result, ::gai_strerror(errno)));
    }
}

int VSocket::_getAddrInfo(struct addrinfo* hints, struct addrinfo** res, bool useHostName) {
    VMutexLocker    locker(&gAddrInfoMutex, "VSocket::_getAddrInfo()");
    VString         portAsString(VSTRING_FORMATTER_INT, mPortNumber);

    return ::getaddrinfo(useHostName ? mHostName.chars() : NULL, portAsString.chars(), hints, res);
}

VSocketID VSocket::_tcpConnectWAddrInfo(struct addrinfo* const resInput) {
    struct addrinfo* res = resInput;
    VSocketID           socketID;

    do {
        socketID = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);

        if (socketID < 0) {
            // Debug message: socket open failed but we will try again if more available...
            continue;
        }
        else if (socketID > FD_SETSIZE) { // FD_SETSIZE is max num open sockets, sockid over that is sign of a big problem
         // Debug message: socket id appears to be out of range...
        }

        if (::connect(socketID, res->ai_addr, res->ai_addrlen) == 0) {
            break; /* success */
        }

        vault::close(socketID);
    } while ((res = res->ai_next) != NULL);

    if (res == NULL) {
        throw VException(errno, VSTRING_FORMAT("VSocket[%s] _tcpConnectWAddrInfo: Socket/Connect failed. Error='%s'.", mSocketName.chars(), ::gai_strerror(errno)));
    }

    return socketID;
}

// The BSD call to getaddrinfo() is not threadsafe so we protect it with a global mutex.
VMutex VSocket::gAddrInfoMutex("VSocket::gAddrInfoMutex");

#endif /* V_BSD_ENHANCED_SOCKETS */

