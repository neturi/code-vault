#include "vcommsessionreadevent.h"

VCommSessionReadEvent::VCommSessionReadEvent(const VCommSessionInfoSharedPtrVector& sessions) : VCommSessionEvent(CommEventType::Read, sessions) {
}

VCommSessionReadEvent::~VCommSessionReadEvent() {
}
