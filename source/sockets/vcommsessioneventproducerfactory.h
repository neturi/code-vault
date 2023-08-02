#ifndef vcommsessioneventproducerfactory_h
#define vcommsessioneventproducerfactory_h

#include "vcommsessioneventproducer.h"

/**
A factory class (singleton) that creates and provides an instance of Comm Session Event Producer (VCommSessionEventProducer).

Why is it needed? 
Implementation of Comm Session Event Producers would be platform-specific. 
This class provides a layer of abstraction and hides the actual implementation of the Comm Session Event Producer.

The implementation of this class would be platform-specific as well.
*/
class VCommSessionEventProducerFactory {
public:
    VCommSessionEventProducerFactory(const VCommSessionEventProducerFactory& other) = delete;

    VCommSessionEventProducerFactory& operator = (const VCommSessionEventProducerFactory& other) = delete;

    ~VCommSessionEventProducerFactory();

    /**
    Returns the singleton instance of this class. If the instance doesn't exist, this method creates one.
    */
    static VCommSessionEventProducerFactory& Instance();

    /**
    Creates an instance of the platform-specific Comm Session Event Producer. If an instance had already been created, 
    this method throws an error.

    maximumEventsPerPollingThread - 
    minimumPollingThreads - 
    Regardless of the platform, the method to subscribe and listen for events on sockets could be the same.
    Some platforms - like Windows - limit the maximum number of sockets a thread can subscribe and listen to (for events).
    For performance reasons, the platform-specific implementation could reduce the limit further.
    This parameter defines the application-set maximum sockets per thread.

    Polling threads subscribe and listen to events on sockets. 
    This parameter specifies the minimum number of threads that would listen for socket events. 

    Note that no maximum limit is set. Setting a limit on this value would directly affect the number of concurrent client connections
    the application could support at any given time. We aren't setting any limits on the number of concurrent client connections today 
    so, limit on the polling threads is not needed.
    */
    VCommSessionEventProducerWeakPtr CreateCommSessionEventProducer(const Vu32 maximumEventsPerPollingThread, const Vu32 minimumPollingThreads);

    /**
    Returns the platform-specific Comm Session Event Producer instance.
    */
    VCommSessionEventProducerWeakPtr CommSessionEventProducer() const;

private:
    VCommSessionEventProducerFactory();

private:
    VCommSessionEventProducerSharedPtr                          commSessionEventProducer;

    static std::shared_ptr<VCommSessionEventProducerFactory>    SingletonInstance;
    static std::once_flag                                       InstanceCreated;

    static const std::string                                    COMM_SESSION_EVENT_PRODUCER_NAME;
};
#endif
