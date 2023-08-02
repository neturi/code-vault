#include "vexception.h"
#include "vcommsessioneventproducerfactory.h"
#include "vwsaeventproducer.h"

VCommSessionEventProducerFactory::VCommSessionEventProducerFactory() {
}

VCommSessionEventProducerFactory::~VCommSessionEventProducerFactory() {
}

VCommSessionEventProducerFactory& VCommSessionEventProducerFactory::Instance() {
    std::call_once(VCommSessionEventProducerFactory::InstanceCreated, []() {
        VCommSessionEventProducerFactory::SingletonInstance.reset(new VCommSessionEventProducerFactory());
    });

    return *VCommSessionEventProducerFactory::SingletonInstance;
}

VCommSessionEventProducerWeakPtr VCommSessionEventProducerFactory::CreateCommSessionEventProducer(const Vu32 minimumPollingThreads,
                                                                                                    const Vu32 maximumEventsPerPollingThread) {
    if (this->commSessionEventProducer) {
        throw VException("An instance of Comm Session Event Producer is already created");
    }

    auto csep = std::make_shared<VWSAEventProducer>(COMM_SESSION_EVENT_PRODUCER_NAME, minimumPollingThreads, maximumEventsPerPollingThread);

    this->commSessionEventProducer = csep;

    return VCommSessionEventProducerWeakPtr(this->commSessionEventProducer);
}

VCommSessionEventProducerWeakPtr VCommSessionEventProducerFactory::CommSessionEventProducer() const {
    if (this->commSessionEventProducer) {
        return VCommSessionEventProducerWeakPtr(this->commSessionEventProducer);
    }

    return VCommSessionEventProducerWeakPtr();
}

std::shared_ptr<VCommSessionEventProducerFactory> VCommSessionEventProducerFactory::SingletonInstance;
std::once_flag VCommSessionEventProducerFactory::InstanceCreated;

const std::string VCommSessionEventProducerFactory::COMM_SESSION_EVENT_PRODUCER_NAME = "WSACommSessionEventProducer";
