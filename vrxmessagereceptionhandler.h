#ifndef vrxmessagereceptionhandler_h
#define vrxmessagereceptionhandler_h

#include "vcommsessioninfo.h"

/**
Handles all the notifications related to incoming messages (Rx) on a socket.
*/
class VRxMessageReceptionHandler {
protected:
    VRxMessageReceptionHandler();

public:
    virtual ~VRxMessageReceptionHandler();

    /**
    Called whenever an incoming message (read event) is detected on a socket.
    The concrete implementation should read and construct the message objects (VMessage) on notification.

    sessions -
    Contains a list of VCommSession objects whose associated sockets had raised the read events.

    NOTE: This method would be called by the Rx Message Receiver. If Rx Message Receiver stops, message reception for *all*
    the comm sessions would stop. So, this method should handle all possible errors appropriately and should only propagate 
    irrecoverable errors - causing application to shut down - up to the caller. 
    */
    virtual bool ReceiveIncomingMessages(const VCommSessionInfoSharedPtrVector& sessions) = 0;
};

using VRxMessageReceptionHandlerSharedPtr = std::shared_ptr<VRxMessageReceptionHandler>;
#endif
