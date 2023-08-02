#include "vwaittokensource.h"

VWaitTokenSource::VWaitTokenSource(bool startWaiting) : waitFlag(new AtomicBoolean(startWaiting)),
                                                        waitCondition(new std::condition_variable()),
                                                        waitConditionMutex(new std::mutex()) {
}

VWaitTokenSource::~VWaitTokenSource() {
    Continue();
}

VWaitToken VWaitTokenSource::Token() const {
    return VWaitToken(this->waitFlag, this->waitCondition, this->waitConditionMutex);
}

bool VWaitTokenSource::Waiting() const {
    return *this->waitFlag;
}

bool VWaitTokenSource::Wait() {
    bool waiting = false;

    {
        std::lock_guard<std::mutex> lock(*(this->waitConditionMutex));

        waiting = this->waitFlag->compare_exchange_strong(waiting, true);
    }

    return waiting;
}

bool VWaitTokenSource::Continue() {
    bool continuing = true;

    {
        std::lock_guard<std::mutex> lock(*(this->waitConditionMutex));

        continuing = this->waitFlag->compare_exchange_strong(continuing, false);
    }

    if (continuing) {
        this->waitCondition->notify_all();
    }

    return continuing;
}
