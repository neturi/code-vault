#ifndef vcommsessioninfo_h
#define vcommsessioninfo_h

#include "vcommsessionenums.h"
#include "vcommsession.h"

/**
Contains the information regarding VCommSession. This class also contains a reference to the VCommSession object.
This is created to add extended properties and contracts - some of which are platform-specific - to 
VCommSession (which is derived by SPARCSClientSession and S3QuerySession) but keep the VCommSession platform-independent.
*/
class VCommSessionInfo {
public:
    /**
    C'tor

    name - 
    Display-friendly name for this session.

    commSession - 
    Reference to the actual VCommSession object.

    setAsConnected - 
    Indicates if the session is already connected or not.
    Useful to represent sessions that have deferred connections (such as outgoing connections).

    continueToReceiveAfterDisconnection - 
    Usually, any WSA_RECV events - after disconnection - are ignored by CommMessageReceiver for the session.
    But, in some cases (example: StandardSyncClientSession), we follow the send-and-disconnect model. The remote hosts for these sessions open a new session, send a message 
    out to XPS, and close the session immediately. So, with the current threading model, the session could be disconnected even before the session has had a chance to read the 
    message. This flag - if set to 'true' - instructs the CommMessageReceiver to continue to process the WSA_RECV event even if the session had disconnected.

    continueToDispatchAfterDisconnection -
    Same as the parameter 'continueToReceiveAfterDisconnection'. CommMessageReceiver does not dispatch messages to the a session (for handling) - if the session is disconnected.
    But, the sessions such as StandardSyncClientSession get disconnected by the remote host as soon as the message is sent out to XPS. 
    If this flag is set to 'true' CommMessageReceiver will to continue to dispatch the message to the session for handling.
    */
    VCommSessionInfo(const std::string& name, VCommSession* commSession, SessionConnectionState connectionState, MessageReceptionAfterDisconnection continueToReceiveAfterDisconnection, 
                        MessageProcessingAfterDisconnection continueToDispatchAfterDisconnection);

    virtual ~VCommSessionInfo();

    /**
    A unique identifier representing this session. Consider this as an hash code.
    */
    boost::uuids::uuid Id() const;

    std::string IdAsString() const;

    std::string Name() const;

    VCommSession* CommSession() const;

    TaskExecutionMode MessageReceptionMode() const;

    /**
    Returns the raw socket id - retrieved from the VCommSession object.
    */
    VSocketID Socket() const;

    /**
    Returns the socket event that will be used to listen for I/O or close events for session's socket.

    This method is platform-specific.
    */
    WSAEVENT SocketEvent() const;

    /**
    Indicates if this session is connected or not.
    */
    SessionConnectionState ConnectionState() const;

    bool SetAsConnected();

    bool SetAsDisconnected();

    /**
    Our socket events are manual-reset events. Meaning, the Windows does not reset them automatically after the event is signalled 
    for any I/O or close events. We'll have to reset them manually after each set (or, signal) by Windows.

    Resets the WSAEVENT related to this session's socket.
    */
    bool ResetSocketEvent();

    MessageReceptionAfterDisconnection SupportForMessageReceptionAfterDisconnection() const;

    MessageProcessingAfterDisconnection SupportForMessageProcessingAfterDisconnection() const;

    SessionOperationState MessageReceptionState() const;

    SessionOperationState MessageProcessingState() const;

    Vu32 IncrementMessagesWaitingToBeProcessed();

    Vu32 DecrementMessagesWaitingToBeProcessed();

    Vu32 GetNumberOfMessagesWaitingToBeProcessed() const;

    /**
    Display-friendly representation of this object.
    */
    std::string ToString() const;

private:
    /**
    Creates a WSAEVENT for this session's socket and registers the event to be notified whenever a READ or CLOSE event occurs 
    on this socket. 
    We are not interested in WRITE events so, we don't register for them.
    */
    void CreateAndRegisterSocketEvent();

    /**
    Returns the logger prefix.
    */
    std::string LoggerPrefix();

private:
    boost::uuids::uuid                                  id;

    std::string                                         name;

    VCommSession*                                       commSession;

    std::atomic<SessionConnectionState>                 connectionState;

    WSAEVENT                                            socketEvent;

    std::atomic<MessageReceptionAfterDisconnection>     messageReceptionAfterDisconnection;
    std::atomic<MessageProcessingAfterDisconnection>    messageProcessingAfterDisconnection;

    std::atomic<Vu32>                                   messagesWaitingToBeProcessed;
};

using VCommSessionInfoSharedPtr = std::shared_ptr<VCommSessionInfo>;
using VCommSessionInfoWeakPtr = std::weak_ptr<VCommSessionInfo>;

using VCommSessionInfoSharedPtrVector = std::vector<VCommSessionInfoSharedPtr>;
using VCommSessionInfoSharedPtrVectorIterator = VCommSessionInfoSharedPtrVector::const_iterator;

/**
Comparer object that is used to compare two VCommSessionInfo objects.
Currently, two VCommSessionInfo objects are considered equal if their UUIDs are equal.

Other members are not considered for comparison as most are mutable.
*/
struct VCommSessionInfoComparer : public std::unary_function<VCommSessionInfoSharedPtr, bool> {
public:
    explicit VCommSessionInfoComparer(const VCommSessionInfoSharedPtr& sessionInfo);
    bool operator() (const VCommSessionInfoSharedPtr& other);

private:
    VCommSessionInfoSharedPtr sessionInfo;
};
#endif
