#ifndef vcommsession_h
#define vcommsession_h

#include "vtypes.h"
#include "vsocketbase.h"
#include "vmessage.h"
#include <boost/uuid/uuid.hpp>
#include <boost/asio/io_service.hpp>

/**
The mode by which a task can be executed.

Sequential -
These tasks needs to be executed in a specific order. For XPS, this is the order by which a message is received for processing.
Example: Updates from Bridge or XPS-Clients.

Concurrent - 
Tasks that don't need to be executed in a specific order. Generally, these tasks are mostly requests from XPS-Clients. 
Since no specific order is required, these tasks can usually be executed in parallel.
*/
enum class TaskExecutionMode {
    Sequential = 0,
    Concurrent = 1
};

/**
Represents the state of a session (usually, a client session).

Stopped -
The session is stopped and cannot process any messages currently or in the future.

Ready - 
The session is ready to process a message.

Busy - 
The session is currently processing a message and cannot process any other message.
*/
enum class SessionOperationState {
    Stopped = 0,
    Ready   = 1,
    Busy    = 2
};

/**
Represents a comm or client session (VClientSession).
Or, this class is a facade/wrapper of the VClientSession.

Why is this needed?
Changing the VClientSession with the new contracts (methods) would have forced us to change a lot of code.
This class reduces the changes and contains - mostly - within the new comm components.
*/
class VCommSession {
protected:
    /**
    C'tor

    name - 
    The display-friendly name of this session. 
    */
    VCommSession(const std::string& name, SessionOperationState initialMessageReceptionState, SessionOperationState initialMessageProcessingState);

public:
    virtual ~VCommSession();

    /**
    Returns the unique Id of this session. Exists for debugging purposes.
    The unique Id is automatically created during the construction of an instance.
    */
    boost::uuids::uuid Id() const;

    std::string IdAsString() const;

    std::string Name() const;

    /**
    Returns the logged-in user's name for the session.
    */
    std::string UserName();

    /**
    The mode that this session currently supports for message reception. 
    See the documentation for TaskExecutionMode for more information on the modes.

    The mode return is 'current' and could change in the future. 

    Returns TaskExecutionMode::Sequential if the incoming messages have to received one-by-one.
    Returns TaskExecutionMode::Concurrent if the incoming messages can be received concurrently.
    */
    TaskExecutionMode MessageReceptionMode() const;

    /**
    Returns the raw, underlying socket for this session.
    */
    virtual VSocketID Socket() const = 0;

    /**
    Called when a 'read' event is pending/waiting for this session.
    
    This method should handle the read event by constructing the message object (VMessage), read the data from the socket and
    return the fully constructed message object.

    NOTE: All non-fatal exceptions related to read operation should be handled by this method.
    */
    virtual VMessage* ReceiveIncomingMessage(/*OUT*/ TaskExecutionMode& messageProcessingMode) = 0;

    /**
    Called when a message is read and ready to be dispatched by the Rx Message Dispatcher.

    This method should process/handle the message.

    message - 
    The fully-constructed message object that needs to be handled/processed.

    NOTE: All non-fatal exceptions are specific to this session. So, they should be contained within this method.
    */
    virtual void HandleRxMessage(VMessage* message) = 0;

    /**
    NOT USED CURRENTLY

    Just a placeholder for now.
    */
    virtual void HandleTxMessage(VMessage* message) = 0;

    /**
    Disconnects and cleans up this comm session.
    */
    virtual void Disconnect(bool socketDisconnected) = 0;

    /**
    Increments the reference count of this object. When another component wants to hold a reference object 
    and keep this object alive until needed, the component may call this method.

    The VClientSession and its derived classes are not smart-pointer compatible yet.
    This forces us to use the 'reference count' mechanism that is being used for non-smart-pointer types 
    in XPS (example: SPARCSClientSessionRefPtr). 

    VClientSession supports reference counting and instances of VClientSession will not be deleted unless the 
    reference count is 0.

    See documentation for VClientSession and SPARCSClientSessionRefPtr for more information.

    Not ideal, but, helps us to keep the client session objects in scope until VCommSessionInfo is alive.
    */
    virtual void IncrementRefCount() = 0;

    /**
    Decrements the reference count of this object. When another component - holding a reference object 
    to this object - is done with this object, the component may call this method.
    */
    virtual void DecrementRefCount() = 0;

    /**
    Returns the current reference count of this object. 
    */
    virtual Vu64 CurrentRefCount() = 0;

    /**
    Returns the current state with respect to receiving incoming messages.
    See SessionOperationState for more information on the states.

    Returns SessionOperationState::Stopped if the session is stopped and cannot receive any incoming messages currently or in the future.
    Returns SessionOperationState::Ready if the session is ready to receive an incoming message.
    Returns SessionOperationState::Busy if the session is currently receiving an incoming message.
    */
    SessionOperationState MessageReceptionState() const;

    /**
    Returns the current state with respect to processing a message/request.
    See SessionOperationState for more information on the states.

    Returns SessionOperationState::Stopped if the session is stopped and cannot process any messages/updates currently or in the future.
    Returns SessionOperationState::Ready if the session is ready to process a message/update.
    Returns SessionOperationState::Busy if the session is currently processing a message/update.
    */
    SessionOperationState MessageProcessingState() const;

protected:
    /**
    Exists because the VClientSession's name changes as its state changes (logged-in, logged-out etc.).
    The name of this class should be in sync with the VClientSession for logging purposes.

    updatedName - 
    The new name of this comm session.
    */
    void UpdateUserName(const std::string& updatedUserName);

    void SetMessageReceptionMode(TaskExecutionMode inMode);

    void SetMessageReceptionState(SessionOperationState currentState);

    void SetMessageProcessingState(SessionOperationState currentState);

private:
    boost::uuids::uuid                  id;

    std::string                         name;

    std::string                         userName;

    std::mutex                          updateUserNameMutex;

    std::atomic<TaskExecutionMode>      messageReceptionMode;

    std::atomic<SessionOperationState>  messageReceptionState;

    std::atomic<SessionOperationState>  messageProcessingState;

    static const std::string            UNINITIALIZED_USER_NAME;
};

using VCommSessionPtr = std::shared_ptr<VCommSession>;
#endif
