#ifndef vcommsessionreadevent_h
#define vcommsessionreadevent_h

#include "vcommsessionevent.h"

/**
Raised by Comm Session Event Producer (VCommSessionEventProducer) to interested components (observers)
whenever we have a read event for one or more comm sessions.

NOTE: This event only notifies the observers that data is pending to be read for the comm session.
It doesn't mean that the incoming data (message) has been read.
*/
class VCommSessionReadEvent : public VCommSessionEvent {
public:
    /**
    C'tor 

    sessions - 
    Contains the list of comm sessions for which read events were detected.
    */
    VCommSessionReadEvent(const VCommSessionInfoSharedPtrVector& sessions);

    ~VCommSessionReadEvent();
};

using VCommSessionReadEventSharedPtr = std::shared_ptr<VCommSessionReadEvent>;
#endif
