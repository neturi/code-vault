#ifndef vcommsessioneventhandler_h
#define vcommsessioneventhandler_h

#include "vcommsessionreadevent.h"
#include "vcommsessionclosedevent.h"
#include <boost/uuid/random_generator.hpp>

/**
Contract that the comm session event observers/listeners should meet if they want to subscribe and handle 
comm session events (VCommSessionEvent).
*/
template <typename EventType>
class VCommSessionEventHandler {
    // Fail if the 'EventType' is not derived from VCommSessionEvent.
    BOOST_STATIC_ASSERT((std::is_base_of<VCommSessionEvent, EventType>::value));

public:
    /**
    Returns the unique Id of this handler.
    */
    boost::uuids::uuid HandlerId() const {
        return this->id;
    }

    /**
    Observers/listeners should implement this method to be notified of the events.
    */
    virtual void HandleEvent(const std::shared_ptr<EventType>& eventArgs) = 0;

    virtual ~VCommSessionEventHandler() {
    }

protected:
    /**
    C'tor

    Unique Id is automatically generated.
    */
    VCommSessionEventHandler() : id(boost::uuids::random_generator()()) {
    }

private:
    boost::uuids::uuid id;
};

using VCommSessionReadEventHandlerSharedPtr     = std::shared_ptr<VCommSessionEventHandler<VCommSessionReadEvent>>;
using VCommSessionReadEventHandlerWeakPtr       = std::weak_ptr<VCommSessionEventHandler<VCommSessionReadEvent>>;

using VCommSessionClosedEventHandlerSharedPtr   = std::shared_ptr<VCommSessionEventHandler<VCommSessionClosedEvent>>;
using VCommSessionClosedEventHandlerWeakPtr     = std::weak_ptr<VCommSessionEventHandler<VCommSessionClosedEvent>>;
#endif
