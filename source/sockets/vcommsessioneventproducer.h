#ifndef vcommsessioneventproducer_h
#define vcommsessioneventproducer_h

#include "vcommsessioninfo.h"
#include "vcommsessioneventhandler.h"

/**
This class is the source for activity events on the comm sessions' sockets. 
Currently, this class only produces events for Read and Close events. 

Interested components can subscribe to either or both events (Read/Close).
Currently, Comm Message Receiver (CommMessageReceiver) subscribes to both events.

This class only defines the contract for the Comm Session Event Producer. The implementation would be
platform-specific.
*/
class VCommSessionEventProducer {
protected:
    /**
    C'tor

    name - 
    A display-friendly name for this component.
    */
    VCommSessionEventProducer(const std::string& name);

public:
    VCommSessionEventProducer(const VCommSessionEventProducer& other) = delete;

    VCommSessionEventProducer& operator=(const VCommSessionEventProducer& other) = delete;

    virtual ~VCommSessionEventProducer();

    std::string Name() const;

    virtual bool Start() = 0;

    virtual bool Stop() = 0;

    virtual bool Started() const = 0;

    virtual bool CanStart() const = 0;

    virtual void UpdateSessions(const VCommSessionInfoSharedPtrVector& newSessions, const VCommSessionInfoSharedPtrVector& closedSessions) = 0;

    virtual void ReArmSession(const VCommSessionInfoSharedPtr&) {};

    /**
    Subscribes the 'handler' to read events on one or more sockets.

    Returns 'true' if the handler is successfully subscribed. 
    Returns 'false' if the handler is already subscribed.

    Method is thread-safe.
    */
    bool SubscribeToReadEvents(const VCommSessionReadEventHandlerSharedPtr& handler);

    /**
    Unsubscribes the 'handler' from read events on one or more sockets.
    For better performance, always unsubscribe when appropriate.

    Returns 'true' if the handler is successfully unsubscribed. Otherwise, returns 'false'.

    Method is threa-safe.
    */
    bool UnsubscribeFromReadEvents(const VCommSessionReadEventHandlerSharedPtr& handler);

    /**
    Subscribes the 'handler' to close events on one or more sockets.

    Returns 'true' if the handler is successfully subscribed.
    Returns 'false' if the handler is already subscribed.

    Method is thread-safe.
    */
    bool SubscribeToClosedEvents(const VCommSessionClosedEventHandlerSharedPtr& handler);

    /**
    Unsubscribes the 'handler' from close events on one or more sockets.
    For better performance, always unsubscribe when appropriate.

    Returns 'true' if the handler is successfully unsubscribed. Otherwise, returns 'false'.

    Method is threa-safe.
    */
    bool UnsubscribeFromClosedEvents(const VCommSessionClosedEventHandlerSharedPtr& handler);

protected:
    /**
    Raises the read event to the interested observers.

    eventArgs - 
    The read event argument that contains the list of comm sessions that generated the read event.
    */
    void RaiseReadEvent(const VCommSessionReadEventSharedPtr& eventArgs);

    /**
    Raises the close event to the interested observers.

    eventArgs -
    The close event argument that contains the list of comm sessions that generated the close 
    event (due to disconnection by either the server or the client.
    */
    void RaiseClosedEvent(const VCommSessionClosedEventSharedPtr& eventArgs);

private:
    std::string name;

    std::map<boost::uuids::uuid, VCommSessionReadEventHandlerWeakPtr>   readEventHandlers;
    std::map<boost::uuids::uuid, VCommSessionClosedEventHandlerWeakPtr> closedEventHandlers;

    std::mutex readEventHandlersMutex;
    std::mutex closedEventHandlersMutex;

public:
    static const Vu32 MAX_SOCKETS_PER_POLLING_THREAD;

    static const Vu32 DEFAULT_SOCKETS_PER_POLLING_THREAD;
};

using VCommSessionEventProducerSharedPtr    = std::shared_ptr<VCommSessionEventProducer>;
using VCommSessionEventProducerWeakPtr      = std::weak_ptr<VCommSessionEventProducer>;
#endif
