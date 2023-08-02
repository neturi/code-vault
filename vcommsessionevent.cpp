#include "vcommsessionevent.h"
#include <boost/uuid/random_generator.hpp>

VCommSessionEvent::VCommSessionEvent() : id(boost::uuids::random_generator()()) {
}

VCommSessionEvent::VCommSessionEvent(CommEventType eventType, const VCommSessionInfoSharedPtrVector& sessions) : 
                                                                id(boost::uuids::random_generator()()), 
                                                                eventType(eventType), sessions(sessions) {
}

VCommSessionEvent::~VCommSessionEvent() {
}

boost::uuids::uuid VCommSessionEvent::Id() const {
    return this->id;
}

CommEventType VCommSessionEvent::EventType() const {
    return this->eventType;
}

VCommSessionInfoSharedPtrVector VCommSessionEvent::Sessions() const {
    return this->sessions;
}
