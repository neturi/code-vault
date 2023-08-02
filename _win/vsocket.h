/*
Copyright c1997-2011 Trygve Isaacson. All rights reserved.
This file is part of the Code Vault version 3.3
http://www.bombaydigital.com/
*/

#ifndef vsocket_h
#define vsocket_h

/** @file */

#include "vsocketbase.h"
#include <openssl/ssl.h>

#pragma comment(lib, "libssl.lib")
#pragma comment(lib, "libcrypto.lib")
#pragma comment (lib, "ws2_32.lib")

#define SSL_ERROR   (-1)

enum IcmpStatCode {
    icmpSuccess,
    icmpStartUpErr,
    icmpCreateSockErr,
    icmpSendErr,
    icmpRecvErr,
    icmpHostNoReply,
    icmpReservePort,
    icmpPortValid,
    icmpPortInvalid
};

struct icmphdr
{
    unsigned char type;     /* message type */
    unsigned char code;     /* type sub-code */
    unsigned short checksum;
    union
    {
        struct
        {
            unsigned short    id;
            unsigned short    sequence;
        } echo;            /* echo datagram */
        unsigned int    gateway;  /* gateway address */
        struct
        {
            unsigned short    __unused;
            unsigned short    mtu;
        } frag;          /* path mtu discovery */
    } un;
};


#define ICMP_ECHO       8    /* Echo Request   */
#define MAX_RETRY_COUNT 3

#define IS_RESERVERVED_PORT(port) (port == 80 || port == 10080 )?true:false

/*
There are a couple of Unix APIs we call that take a socklen_t parameter.
On Windows the parameter is defined as an int. The cleanest way of dealing
with this is to have our own conditionally-defined type here and use it there.
We do something similar in the _unix version of this file for HP-UX, which
also uses int instead of socklen_t.
*/
typedef int VSocklenT;

/**
    @ingroup vsocket
*/

/**
This is the WinSock2 implementation of VSocket. It derives
from VSocketBase and overrides the necessary methods to provide socket
communications using the WinSock2 APIs. The implementations of this class
for other socket platforms are located in the parallel directory for the
particular platform. If you are seeing this in the Doxygen pages, it is
because Doxygen was run with macro definitions for this platform, not
the other platforms.

@see    VSocketBase
*/
class VSocket : public VSocketBase {
    public:

        /**
        Constructs the object with an already-opened low-level
        socket connection identified by its ID.
        */
        VSocket(VSocketID id);
        /**
        Constructs the object; does NOT open a connection.
        */
        VSocket(const VString& hostName, int portNumber);
        /**
        Destructor, cleans up by closing the socket.
        */
        virtual ~VSocket();

        // --------------- These are the implementations of the pure virtual
        // methods that only a platform subclass can implement.

        /**
        Returns the number of bytes that are available to be read on this
        socket. If you do a read() on that number of bytes, you know that
        it will not block.
        @return the number of bytes currently available for reading
        */
        virtual int available();
        /**
        Reads data from the socket.

        If you don't have a read timeout set up for this socket, then
        read will block until all requested bytes have been read.

        @param    buffer            the buffer to read into
        @param    numBytesToRead    the number of bytes to read from the socket
        @return    the number of bytes read
        */
        virtual int read(Vu8* buffer, int numBytesToRead);
        /**
        Writes data to the socket.

        If you don't have a write timeout set up for this socket, then
        write will block until all requested bytes have been written.

        @param    buffer            the buffer to read out of
        @param    numBytesToWrite    the number of bytes to write to the socket
        @return    the number of bytes written
        */
        virtual int write(const Vu8* buffer, int numBytesToWrite);
        /**
        Sets the host name and port number properties of this socket by
        asking the lower level services to whom the socket is connected.
        */
        virtual void discoverHostAndPort();
        /**
        Shuts down just the read side of the connection.
        */
        virtual void closeRead();
        /**
        Shuts down just the write side of the connection.
        */
        virtual void closeWrite();
        /**
        Sets a specified socket option.
        @param    level        the option level
        @param    name        the option name
        @param    valuePtr    a pointer to the new option value data
        @param    valueLength    the length of the data pointed to by valuePtr
        */
        virtual void setSockOpt(int level, int name, void* valuePtr, int valueLength);

        /**
        gets IP Status or error code.
        @param    ipAddr      ipAddr
        @param    port        the port number
        */
        int getIPStatus(VString ipAddr, int port);

        /**
        checks if its a Valid Port.
        @param    ip      ipAddr
        @param    port        the port number
        */
        int checkIsValidPort(char* ip, int port);

        /**
        connect To Server using Https protocol
        @param    ip      ipAddr
        @param    p       port number
        @param    assl    SSL object
        */
        int connectToHttpsServer(const char* ip, const char* p, SSL** assl);
        /**
        In Windows, the send & receive should be properly shut down before closing the socket. 
        Overriding VSocketBase::close would give us a chance to shut down the send & receive.
        */
        virtual void close();

    protected:

        /**
        Connects to the server.
        */
        virtual void _connect();
        /**
        Starts listening for incoming connections. Only useful to call
        with a VListenerSocket subclass, but needed here for class
        hierarchy implementation reasons (namely, it is implemented by
        the VSocket platform-specific class that VListenerSocket
        derives from).
        @param  bindAddress if empty, the socket will bind to INADDR_ANY (usually a good
                                default); if a value is supplied the socket will bind to the
                                supplied IP address (can be useful on a multi-homed server)
        @param  backlog     the backlog value to supply to the ::listen() function
        */
        virtual void _listen(const VString& bindAddress, int backlog);

    private:
        /**
        internal method to verify the check sum
        return the answer
        */
        unsigned short _in_cksum(unsigned short* ptr, int nbytes);
        /**
        Tries and converts the WSA error codes to native error codes (specific to XPS) as an attempt to 
        abstract platform-specific error codes from the layers above VSocket.
        Currently, not all the WinSock error codes have native equivalents.

        errorCode - 
        The WinSock-specific error code that needs to be converted to native error code.
        */
        int _getNativeErrorCode(DWORD errorCode) const;

        /**
        STAF scripts shuts down the clients abruptly without waiting for the data transactions to complete.
        In this scenario, it is common for a socket to be disconnected/closed while we attempt to send/receive data over the socket. 
        This cannot be avoided unless we have a well-defined shutdown protocol. 
        
        Logging common errors - due to tearing down of connection on either side - causes STAF scripts to report them as diffs in the script results.
        Almost all of these errors are handled by the layers above VSocket. So, when we encounter common teardown errors, we don't log them as errors but log them as information.

        We treat the following errors as common reasons for disconnections (during Rx or Tx). 
        WSAENOTSOCK     (10038L) - An operation was attempted on something that is not a socket. 
        WSAEINTR        (10004L) - A blocking operation was interrupted by a call to WSACancelBlockingCall. 
        WSAECONNABORTED (10053L) - An established connection was aborted by the software in your host machine.
        WSAESHUTDOWN    (10058L) - A request to send or receive data was disallowed because the socket had already been shut down in that direction with a previous shutdown call.

        NOTE: 
        We only reduce the severity level of the above-mentioned errors in *this layer*. We DON'T SUPPRESS the errors. 
        The errors would still be thrown to upper/calling layers and these layers still have the option to handle the errors as they see fit.
        */
        bool _isCommonSocketTeardownError(DWORD errorCode);

        /**
        Retrieves the error string for the specified Windows error code and frames an error message with all the given 
        information.

        messagePrefix - 
        Any string or message that should prefix the socket information and Windows' error message.

        socketID - 
        The raw socket Id.

        errorCode - 
        The error code returned by ::WSAGetLastError() API - if any. If there is no error code, set to '0'.

        errorMessage - 
        Output parameter. 
        The resulting message string that is framed with all the given information.
        */
        void _frameErrorMessage(const VString& messagePrefix, VSocketID socketID, DWORD errorCode, int nativeErrorCode, /*[OUT]*/ VString& errorMessage);

        /**
        Similar to the overload above. This overload takes the value returned by a WinSock API - if applicable.

        result - 
        The value returned by a WinSock API - if applicable. If there is no result, specify your own value what would clearly indicate 
        the case in the logs.
        */
        void _frameErrorMessage(const VString& messagePrefix, VSocketID socketID, DWORD errorCode, int nativeErrorCode, int result, /*[OUT]*/ VString& errorMessage);

        /**
        Constructs an error message with the specified information and throws a VSocketException with the constructed error message.

        logAsError - 
        If set to 'true', logs the error message first as 'ERROR' and then throws. If set to 'false', logs the error message as 'INFO' and throws.

        messagePrefix -
        Any string or message that should prefix the socket information and Windows' error message.

        socketID -
        The raw socket Id.

        errorCode -
        The error code returned by ::WSAGetLastError() API - if any. If there is no error code, set to '0'.
        */
        void _throwSocketError(bool logAsError, const VString& messagePrefix, VSocketID socketID, DWORD errorCode);

        /**
        Similar to the overload above. This overload takes the value returned by a WinSock API - if applicable.

        result -
        The value returned by a WinSock API - if applicable. If there is no result, specify your own value what would clearly indicate
        the case in the logs.
        */
        void _throwSocketError(bool logAsError, const VString& messagePrefix, VSocketID socketID, DWORD errorCode, int result);

    private:
        // Not thread-safe. This class & base should be thread-safe.
        bool mReadShutDown;
        bool mWriteShutDown;

        static const int PEEK_MESSAGE_BUFFER_LENGTH;

    public:
        // Holds the major WinSock version that we would use.
        static const int WINSOCK_MAJOR_VERSION;
        // Holds the minor WinSock version that we would use.
        static const int WINSOCK_MINOR_VERSION;
};

#endif /* vsocketbase_h */