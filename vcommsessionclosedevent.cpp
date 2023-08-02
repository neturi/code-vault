#include "vcommsessionclosedevent.h"

VCommSessionClosedEvent::VCommSessionClosedEvent(const VCommSessionInfoSharedPtrVector& sessions) : 
                                                        VCommSessionEvent(CommEventType::Close, sessions) {
}

VCommSessionClosedEvent::~VCommSessionClosedEvent() {
}
