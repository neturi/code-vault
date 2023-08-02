#ifndef vpollingthreadinfo_h
#define vpollingthreadinfo_h

#include "vcancellationtokensource.h"
#include "vwaittoken.h"

/**
What is a polling thread?
Polling thread takes a predefined number of Socket objects (socket groups) and registers for READ and CLOSE events on
those Socket objects. Whenever it detects a READ/CLOSE event on a socket, the thread raises a READ/CLOSE 
event. 

This class holds the configuration and information required for a polling thread.
*/
class VPollingThreadInfo {
public:
    /**
    C'tor

    threadId - 
    The polling thread's Id.

    groupOffset - 
    The component that owns the polling threads creates a list of VCommSessionInfo objects that represents each Comm (or client) sessions
    currently active. VCommSessionInfo contains the (raw) socket objects. 
    Each polling thread works with a non-overlapping subset of this list. 
    This parameter specifies the starting index of *this* polling thread's subset in the list. 

    numberOfSockets - 
    The maximum number of sockets that this polling thread would monitor. This, along with the parameter 'groupOffset', defines this 
    polling thread's range in the VCommSessionInfo list.

    threadWaitToken - 
    The WSAEventProducer - which creates and owns the polling threads needs to update the list of VCommSessionInfo and the 
    number of polling threads periodically - as clients connect and disconnect. So, WSAEventProducer will have to hold the execution 
    of the polling threads until the updates are done. This wait token is used to synchronize this update activity.

    abortPollingEvent - 
    Socket event notifications work by creating a WSAEVENT object for each socket, subscribe for the events we are interested in
    (such as READ, CLOSE, WRITE) occurs. Windos API, ::WSAWaitForMultipleEvents - takes a list of WSAEVENT handles and listens 
    for events from Windows. This API is a blocking call and returns when one or more of the WSAEVENT handles is signalled. 
    We'll need to abort this blocking call whenever the sockets' list has to be updated. 'abortPollingEvent' is a non-socket event 
    created and added to the list of socket events. When we need to abort the ::WSAWaitForMultipleEvents, we just signal through 
    'abortPollingEvent'.

    pollingThread - 
    The Boost thread object that represents this polling thread. It is passed as an argument to this polling thread so that 
    when this polling thread exits, it can delete the thread object. Deleting from other components would require unnecessary 
    synchronization. Note that the thread object is not dependent on the thread's lifetime. It is merely a representation of 
    the OS' thread. 
    */
    VPollingThreadInfo(unsigned int threadId, unsigned int groupOffset, unsigned int numberOfSockets,
                        VWaitToken threadWaitToken, WSAEVENT abortPollingEvent, boost::thread* pollingThread);

    ~VPollingThreadInfo();

    unsigned int Id() const;

    unsigned int GroupOffset() const;
    void GroupOffset(unsigned int inGroupOffset);

    unsigned int NumberOfSockets() const;
    void NumberOfSockets(unsigned int inNumberOfSockets);

    VWaitToken ThreadWaitToken() const;

    WSAEVENT AbortIOWaitEvent() const; // TODO: mohanla - Better name?

    boost::thread* PollingThread() const;
    void PollingThread(boost::thread* inPollingThread);

    /**
    WSAEventProducer signals the polling threads to abort waiting on ::WSAWaitForMultipleEvents whenever it needs to
    update the sockets' list. It also needs to wait for all the threads to 'join' before continuing with the update.
    Each polling thread has its own event that it will set (or, signal) to notify WSAEventProducer that it has aborted 
    and is waiting for the update to complete.
    WSAEventProducer will wait for all the polling threads' events to be signalled before continuing with the update.

    Returns the current polling thread's join event.
    */
    HANDLE PollingThreadJoinEvent() const;

    /**
    Sets (or signals) the current polling thread's join event.
    */
    bool SetPollingThreadJoinEvent();

    /**
    Resets the current polling thread's join event.
    */
    bool ResetPollingThreadJoinEvent();

    bool IsPollingThreadJoinEventSet() const;

    /**
    Indicates whether this polling thread has exited/stopped.
    */
    bool ThreadExited() const;

    /**
    Sets an internal flag that indicates if this polling thread has exited/stopped.
    */
    bool SetThreadExited();

    /**
    Returns 'true' if this polling thread has been cancelled (advised to stop/exit).
    */
    bool Cancelled() const;

    /**
    Stops/cancels this polling thread.
    */
    bool Cancel();

    /**
    Display-friendly representation of this object.
    */
    std::string ToString() const;

private:
    unsigned int id;
    unsigned int groupOffset;
    unsigned int numberOfSockets;

    VWaitToken threadWaitToken;

    WSAEVENT abortIOWaitEvent;

    boost::thread* pollingThread;

    HANDLE pollingThreadJoinEvent;

    AtomicBoolean isJoinEventSet;

    AtomicBoolean threadExited;

    VCancellationTokenSource pollingCancellationSource;
};

using VPollingThreadInfoSharedPtr = std::shared_ptr<VPollingThreadInfo>;
using VPollingThreadInfoPtrVector = std::vector<VPollingThreadInfoSharedPtr>;
#endif
