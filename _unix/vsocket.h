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

#ifndef PP_Target_Carbon
    // Unix platform includes needed for platform socket implementation.
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
#endif

#include "vmutex.h"

#include <netdb.h>
#include <arpa/inet.h>
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


/**
If the enhanced BSD multi-home APIs are available, you can define the
preprocessor symbol V_BSD_ENHANCED_SOCKETS, which will cause a slightly
different set of code to be used for listen() and connect(), which
takes advantage of those APIs. One thing the BSD APIs let you do is listen on
multiple interfaces or an interface other than the default one. The
IEEE 1003.1 APIs don't give you control over this, so you can only listen on
the default interface.
*/

/*
There are a couple of Unix APIs we call that take a socklen_t parameter.
Well, on HP-UX the parameter is defined as an int. The cleanest way of dealing
with this is to have our own conditionally-defined type here and use it there.
*/
#ifdef VPLATFORM_UNIX_HPUX
    typedef int VSocklenT;
#else
    typedef socklen_t VSocklenT;
#endif

/**
    @ingroup vsocket
*/


/**
This is the standard Unix sockets implementation of VSocket. It derives
from VSocketBase and overrides the necessary methods to provide socket
communications using the Unix APIs. The implementations of this class
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
        @param    level         the option level
        @param    name          the option name
        @param    valuePtr      a pointer to the new option value data
        @param    valueLength   the length of the data pointed to by valuePtr
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

#ifdef V_BSD_ENHANCED_SOCKETS

        // These are further BSD-specific methods used in the V_BSD_ENHANCED_SOCKETS
        // versions of connect() and listen().
        void        _tcpGetAddrInfo(struct addrinfo** res);                                             ///< Calls low-level getaddrinfo()
        int         _getAddrInfo(struct addrinfo* hints, struct addrinfo** res, bool useHostName = true); ///< Calls low-level _getAddrInfo()
        VSocketID   _tcpConnectWAddrInfo(struct addrinfo* const resInput);                              ///< Calls low-level socket() and connect()

        static VMutex gAddrInfoMutex;   ///< getaddrinfo() is not thread-safe, so we have to protect ourselves.

#endif /* V_BSD_ENHANCED_SOCKETS */
};

#endif /* vsocketbase_h */

