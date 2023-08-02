#ifndef vcommsessionclosedevent_h
#define vcommsessionclosedevent_h

#include "vcommsessionevent.h"

/**
Raised by Comm Session Event Producer (VCommSessionEventProducer) to interested components (observers)
whenever one or more comm sessions had disconnected/closed.

NOTE: This event generated only when a session disconnects or closes its socket. This not an event
indicating the 'intent' to disconnect/close.
*/
class VCommSessionClosedEvent : public VCommSessionEvent {
public:
    /**
    C'tor

    sessions - The list of comm sessions that had closed/disconnected.
    */
    VCommSessionClosedEvent(const VCommSessionInfoSharedPtrVector& sessions);

    ~VCommSessionClosedEvent();
};

using VCommSessionClosedEventSharedPtr = std::shared_ptr<VCommSessionClosedEvent>;
#endif