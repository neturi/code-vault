#ifndef vcommsessionevent_h
#define vcommsessionevent_h

#include "vcommtypes.h"
#include "vcommsessioninfo.h"

/**
Base class for all comm session event types.
*/
class VCommSessionEvent {
protected:
    VCommSessionEvent();

    /**
    C'tor 

    eventType - 
    The comm session event type (Read, Close, etc.).

    sessions - 
    The list of sessions that have generated the specified event (eventType).
    */
    VCommSessionEvent(CommEventType eventType, const VCommSessionInfoSharedPtrVector& sessions);

public:
    virtual ~VCommSessionEvent();

    /**
    Returns the unique Id of this event object. The Id exists for debugging purposes.
    */
    boost::uuids::uuid Id() const;

    CommEventType EventType() const;

    VCommSessionInfoSharedPtrVector Sessions() const;

private:
    boost::uuids::uuid id;

    CommEventType eventType;

    VCommSessionInfoSharedPtrVector sessions;
};
#endif
