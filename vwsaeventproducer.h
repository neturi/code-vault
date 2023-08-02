#ifndef vwsaeventproducer_h
#define vwsaeventproducer_h

#include "vpollingthreadinfo.h"
#include "vcancellationtokensource.h"
#include "vwaittokensource.h"
#include "vcommsessioninfo.h"
#include "vcommsessioneventproducer.h"

/**
This is the concrete implemenation - specific to Windows - of VCommSessionEventProducer.

This class spawns polling threads - threads that subscribe and listen to a specified number of Windows Sockets for I/O events.
Currently, the polling threads only listen for 'read' and 'close' events. They don't listen for 'write' events.

The polling threads raise notifications whenever they detect 'read' and/or 'close' events. For better performance, they try to raise 
the 'read' and 'close' events for multiple sockets (or comm sessions).

Any component that is interested in getting notified of these events (read/close) can subscribe with this component.

This class needs to be notified of newly-created sessions (incoming/outgoing connections) so that the new sessions' sockets could be monitored 
for I/O events. 
Disconnected sessions are automatically removed from the list - but, to be safe, it is recommended to notify this component with 
the discarded/sessions for cleanup purposes.
*/
class VWSAEventProducer : public VCommSessionEventProducer {
public:
    /**
    C'tor 

    name - 
    A display-friendly name for this component.

    maximumEventsPerPollingThread - 
    The maximum number of sockets that would be monitored by one polling thread for I/O events. 
    Windows has limits on the maximum number of sockets a thread can listen (for I/O events) to. This limit is 64 per thread right now.
    Setting the value to Windows' limit will decrease the response time of the thread with respect to socket events.
    When you are processing an I/O event on a socket, one or more sockets could raise events concurrently. We handle that by checking
    all the sockets for events after processing one event. This avoids the number of calls we make through Windows APIs.
    When there is spurious I/O activity on the sockets, the thread will have to loop through a huge list - decreasing the response time for
    each event as well as changing the order of incoming events.
    Key is to keep the value at possible minimum but increase the number of threads.

    minimumPollingThreads - 
    The polling threads - that listen for socket I/O events - have a pre-determined number of sockets to listen to.
    The number of polling threads would increase or decrease dynamically depending upon the number of active connections/sessions.
    This setting is to set the minimum number of polling threads that will always be running. Setting this to the right value - suiting
    the customer's environment - will avoid overhead in creation and destruction of polling threads.
    
    This setting works closely with 'minimumPollingThreads'.
    For example, on average, if you have about 40 clients connecting concurrently, you'd set minimumPollingThreads
    to 16 and this setting to 3 (16 x 3 = 48).
    Or, minimumPollingThreads could be set to 8 and this setting to 5 or 6.

    NOTE: Setting maximumEventsPerPollingThread & minimumPollingThreads to 1 is the same as having one input thread per session.
    */
    VWSAEventProducer(const std::string& name, unsigned int maximumEventsPerPollingThread, unsigned int minimumPollingThreads);

    VWSAEventProducer(const VWSAEventProducer& other) = delete;

    VWSAEventProducer& operator=(const VWSAEventProducer& other) = delete;

    ~VWSAEventProducer();

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

private:
    /**
    The polling threads' execution method. Listens for incoming I/O events for the specified list of sessions.
    Notifies the subscribed listeners whenever a read or close events are detected on one or more sessions.

    pollingThreadInfo - 
    Contains the required information for each polling thread to listen for I/O events. 
    See VPollingThreadInfo for more information.

    sessions - 
    The list of sessions that are currently being monitored by this component. Note that this list is shared by all the polling threads.
    Each polling thread monitors sessions a subset of this list. This is done to avoid unnecessary copying of sessions list.
    */
    void ListenAndProduceEvents(const VPollingThreadInfoSharedPtr& pollingThreadInfo, const std::shared_ptr<VCommSessionInfoSharedPtrVector>& inSessions);

    bool SetAbortIOWaitEvent();

    bool ResetAbortIOWaitEvent();

private:
    unsigned int minimumPollingThreads;
    unsigned int maximumEventsPerPollingThread;

    AtomicBoolean started;
    std::mutex startStopMutex;

    VCancellationTokenSource cancellationSource;

    std::shared_ptr<VCommSessionInfoSharedPtrVector> sessions;

    VPollingThreadInfoPtrVector pollingThreads;

    VWaitTokenSource waitTokenSource;

    WSAEVENT abortIOWaitEvent;

    AtomicBoolean abortIOWaitEventSet;

    boost::thread_group threadGroup;

    static const DWORD POLLING_THREAD_JOIN_TIMEOUT = 100; // 100 ms

    static const DWORD LISTENER_THREAD_IO_WAIT_TIMEOUT = 100; // 100 ms
};
#endif
