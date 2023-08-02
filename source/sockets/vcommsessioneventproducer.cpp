#include "vcommsessioneventproducer.h"

VCommSessionEventProducer::VCommSessionEventProducer(const std::string& name) : name(name) {
}

VCommSessionEventProducer::~VCommSessionEventProducer() {
}

std::string VCommSessionEventProducer::Name() const {
    return this->name;
}

bool VCommSessionEventProducer::SubscribeToReadEvents(const VCommSessionReadEventHandlerSharedPtr& handler) {
    std::lock_guard<std::mutex> lock(this->readEventHandlersMutex);

    auto handlerId = handler->HandlerId();

    auto existingHandler = this->readEventHandlers.find(handlerId);

    if (existingHandler == this->readEventHandlers.end()) {
        this->readEventHandlers[handlerId] = VCommSessionReadEventHandlerWeakPtr(handler);

        return true;
    }

    return false;
}

bool VCommSessionEventProducer::UnsubscribeFromReadEvents(const VCommSessionReadEventHandlerSharedPtr& handler) {
    std::lock_guard<std::mutex> lock(this->readEventHandlersMutex);

    auto handlerId = handler->HandlerId();

    auto existingHandler = this->readEventHandlers.find(handlerId);

    if (existingHandler != this->readEventHandlers.end()) {
        this->readEventHandlers.erase(handlerId);

        return true;
    }

    return false;
}

bool VCommSessionEventProducer::SubscribeToClosedEvents(const VCommSessionClosedEventHandlerSharedPtr& handler) {
    std::lock_guard<std::mutex> lock(this->closedEventHandlersMutex);

    auto handlerId = handler->HandlerId();

    auto existingHandler = this->closedEventHandlers.find(handlerId);

    if (existingHandler == this->closedEventHandlers.end()) {
        this->closedEventHandlers[handlerId] = VCommSessionClosedEventHandlerWeakPtr(handler);

        return true;
    }

    return false;
}

bool VCommSessionEventProducer::UnsubscribeFromClosedEvents(const VCommSessionClosedEventHandlerSharedPtr& handler) {
    std::lock_guard<std::mutex> lock(this->closedEventHandlersMutex);

    auto handlerId = handler->HandlerId();

    auto existingHandler = this->closedEventHandlers.find(handlerId);

    if (existingHandler != this->closedEventHandlers.end()) {
        this->closedEventHandlers.erase(handlerId);

        return true;
    }

    return false;
}

void VCommSessionEventProducer::RaiseReadEvent(const VCommSessionReadEventSharedPtr& eventArgs) {
    std::vector<VCommSessionReadEventHandlerWeakPtr> handlers;

    {
        std::lock_guard<std::mutex> lock(this->readEventHandlersMutex);

        auto iterBegin = this->readEventHandlers.begin();

        auto iterEnd = this->readEventHandlers.end();

        for (; iterBegin != iterEnd;) {
            auto handler = iterBegin->second.lock();

            if (!handler) {
                this->readEventHandlers.erase(iterBegin++);
            } else {
                handlers.push_back(iterBegin->second);

                ++iterBegin;
            }
        }
    }

    for (auto iter = handlers.begin(); iter != handlers.end(); iter++) {
        auto handler = (*iter).lock();

        if (handler) {
            handler->HandleEvent(eventArgs);
        }
    }
}

void VCommSessionEventProducer::RaiseClosedEvent(const VCommSessionClosedEventSharedPtr& eventArgs) {
    std::vector<VCommSessionClosedEventHandlerWeakPtr> handlers;

    {
        std::lock_guard<std::mutex> lock(this->closedEventHandlersMutex);

        auto iterBegin = this->closedEventHandlers.begin();

        auto iterEnd = this->closedEventHandlers.end();

        for (; iterBegin != iterEnd;) {
            auto handler = iterBegin->second.lock();

            if (!handler) {
                this->closedEventHandlers.erase(iterBegin++);
            } else {
                handlers.push_back(iterBegin->second);

                ++iterBegin;
            }
        }
    }

    for (auto iter = handlers.begin(); iter != handlers.end(); iter++) {
        auto handler = (*iter).lock();

        if (handler) {
            handler->HandleEvent(eventArgs);
        }
    }
}
