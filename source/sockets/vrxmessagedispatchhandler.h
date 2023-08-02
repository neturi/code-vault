#ifndef vrxmessagedispatchhandler_h
#define vrxmessagedispatchhandler_h

#include "vcommsessioninfo.h"
#include "vmessage.h"

/**
A DTO that contains information related to a dispatch operation in XPS Server.
A dispatch operation is typically the processing of a message/request and it happens after a message/request is received from a component external to XPS Server. Example: XPS Client.

- CommSessionInfo
A managed reference to the Comm Session that would be handling the message/request.

- ReceivedTime
The time - in nanoseconds - the message/request was received by XPS Server.

- Priority
The priority with which the message/request will be processed by XPS Server.

- QueuedTime
The time - in nanoseconds - the message/request was queued for processing in XPS Server.

NOTE: The ReceivedTime and QueuedTime are used to measure the latency in message reception and message queueing (for processing). 
*/
struct DispatchInfo {
public:
    DispatchInfo() : ReceivedTime(0.0), Message(NULL), ProcessingMode(TaskExecutionMode::Sequential), QueuedTime(0.0) {
    }

    DispatchInfo(VCommSessionInfoSharedPtr commSessionInfo, VDouble receivedTime, VMessage* message, TaskExecutionMode processingMode, VDouble queuedTime) :  
                    CommSessionInfo(commSessionInfo), ReceivedTime(receivedTime), Message(message), ProcessingMode(processingMode), QueuedTime(queuedTime) {
    }

    ~DispatchInfo() {
        CommSessionInfo.reset();

        ReceivedTime = 0.0;

        Message = NULL;

        ProcessingMode = TaskExecutionMode::Sequential;

        QueuedTime = 0.0;
    }

public:
    VCommSessionInfoSharedPtr   CommSessionInfo;
    VDouble                     ReceivedTime;
    VMessage*                   Message;
    TaskExecutionMode           ProcessingMode;
    VDouble                     QueuedTime;
};

using DispatchInfoVector    = std::vector<DispatchInfo>;

/**
Dispatches the incoming messages for processing to the related comm sessions.
*/
class VRxMessageDispatchHandler {
protected:
    VRxMessageDispatchHandler();

public:
    virtual ~VRxMessageDispatchHandler();

    /**
    Called when an incoming message is read and is ready to be dispatched to the appropriate handler (VCommSession).
    The concrete implementation should dispatch the messages to the appropriate comm session for handling.

    dispatchInfo - 
    Contains the message object (VMessage*) and information about comm session (VCommSessionInfoSharedPtr) that needs to handle the message.

    NOTE: This method would be called by the Rx Message Dispatcher. If Rx Message Dispatcher stops, message dispatch for *all*
    the comm sessions would stop. So, this method should handle all possible errors appropriately and should only propagate
    irrecoverable errors - causing application to shut down - up to the caller.
    */
    virtual bool DispatchIncomingMessage(const DispatchInfo& dispatchInfo) = 0;
};

using VRxMessageDispatchHandlerSharedPtr = std::shared_ptr<VRxMessageDispatchHandler>;
#endif
