#ifndef vwsaeventproducer_h
#define vwsaeventproducer_h

#include "vcancellationtokensource.h"
#include "vwaittokensource.h"
#include "vcommsessioninfo.h"
#include "vcommsessioneventproducer.h"

/**
This is the concrete implemenation - specific to Linux - of VCommSessionEventProducer.

As with the Windows implementation, this component needs to be notified of newly-created sessions (incoming/outgoing connections)
so that the associated sockets can be monitored for I/O events. Similarly, disconnected sessions are automatically removed
internally, but, to be safe, it is recommended to notify this component with the discarded/sessions for cleanup purposes.

This component uses the epoll mechanism to monitor socket connections. The epoll instance is created when the event producer is started.

As new socket connections are added, they are registered as sessions and this event handler is notified of their creation. The
session info is locally maintained for reference, but the key operation is that the socket reference, known in Linux as a file
descriptor (fd), is added to the epoll instance using epoll_ctl.

There is then a single polling thread that controls the monitoring of all registered sockets using epoll_wait. As opposed to the
Windows implementation, Linux has no limit on the number of sockets that can be monitored on a thread, and since XPS is sequentially
executed despite being multi-threaded, there is no advantage to having multiple event listener threads. In fact, the management of
epoll over multiple threads can be complex and error-prone.

This component presumes the use of level-triggered epoll monitoring using the EPOLLONESHOT flag. This means that every incoming message
disables the associated socket in epoll. This is needed to allow for caching and processing of incoming messages. After a successful
read event, the ReArmSession method should be called to re-enable the epoll listener for that socket.

The polling thread raises notifications whenever it detects 'read' and/or 'close' events. For better performance, this is done for
multiple sockets (or comm sessions). Any component that is interested in getting notified of these read and close events can subscribe
with this component. 'Write' events are not managed as outgoing message processing is already managed and socket characteristics are
need no manipulation.
*/
class VEventProducer : public VCommSessionEventProducer {
public:
    /**
    C'tor

    name -
    A display-friendly name for this component.

    maximumEventsPerPollingThread -
    Unused in Linux context, kept to maintain function signature.
    See VWSAEventProducer for relevance in a Windows context. (Short answer is Winodws can only support up to 64 sockets on a single thread)

    minimumPollingThreads -
    Unused in Linux context, kept to maintain function signature.
    See VWSAEventProducer for relevance in a Windows context. (Short answer is limit on window sockets per thread means more than one thread needed)
    */
    VEventProducer(const std::string& name, unsigned int maximumEventsPerPollingThread, unsigned int minimumPollingThreads);

    VEventProducer(const VEventProducer& other) = delete;

    VEventProducer& operator=(const VEventProducer& other) = delete;

    ~VEventProducer();

    /**
    Starts this component. Also starts the polling threads.

    Returns 'true' if the component is started successfully. If the component is already started, returns 'false'.
    Throws std::excpetion if you try to start the component after it's been stopped/cancelled.
    Throws std::exception if any of the polling threads could not be started.
    */
    bool Start();

    /**
    Stops the component if it has been started.

    Returns 'true' if the component is not started or, if it stopped/cancelled already.

    NOTE: Once stopped, this component cannot be restarted. This is by design.
    */
    bool Stop();

    bool Started() const;

    bool CanStart() const;

    /**
    Updates the list of sessions that are being monitored for I/O events.

    newSessions -
    List of newly-created sessions that needs to be monitored for I/O events (read and closed).

    closedSessions -
    Disconnected sessions that should be removed from the list of 'monitored' sessions.
    */
    void UpdateSessions(const VCommSessionInfoSharedPtrVector& newSessions, const VCommSessionInfoSharedPtrVector& closedSessions);

    /**
    Update epoll with EPOLL_CTL_MOD for the file descriptor for inSession so that subsequent EPOLLIN events are captured
    */
    void ReArmSession(const VCommSessionInfoSharedPtr& inSession);

private:
    /**
    The polling thread execution method. Listens for incoming I/O events for the specified list of sessions.
    Notifies the subscribed listeners whenever a read or close events are detected on one or more sessions.

    inSessions -
    The map of sessions that are currently being monitored by this component, indexed by socket ID.
    */
    void ListenAndProduceEvents(const std::shared_ptr<VCommSessionInfoSharedPtrMap>& inSessions);

private:
    AtomicBoolean started;
    std::mutex startStopMutex;

    VCancellationTokenSource cancellationSource;

    std::shared_ptr<VCommSessionInfoSharedPtrMap> sessions;

    VSocketID epoll_fd;

    boost::thread_group threadGroup;

    static const Vu32 LISTENER_THREAD_IO_WAIT_TIMEOUT = 100; // 100 ms
    static const Vu32 EPOLL_WAIT_IMMEDIATE_RETURN = 0;

    static const Vu32 MAX_EPOLL_EVENTS = 1024;
};
#endif
