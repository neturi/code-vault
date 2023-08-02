#include "vwaittoken.h"

VWaitToken::~VWaitToken() {
}

bool VWaitToken::Waiting() const {
    if (auto waitFlagSharedPtr = this->waitFlag.lock()) {
        return *waitFlagSharedPtr;
    }

    // If the parent VWaitTokenSource had gone out of scope, we aren't waiting.
    return false;
}

VWaitToken::VWaitToken(const AtomicBooleanSharedPtr& waitFlag, const ConditionVariableSharedPtr& waitCondition, const MutexSharedPtr& waitConditionMutex)
    : waitFlag(AtomicBooleanWeakPtr(waitFlag)),
    waitCondition(waitCondition), waitConditionMutex(waitConditionMutex) {
}

void VWaitToken::WaitUntilContinuation(const std::string& /*source*/) {
    std::unique_lock<std::mutex> lock(*(this->waitConditionMutex));

    this->waitCondition->wait(lock, [this] { return !this->Waiting(); });

    lock.unlock();
}

bool VWaitToken::WaitUntilContinuationOrTimeout(const std::string& /*source*/, Vu32 maxTimeToWaitInMilliseconds) {
    if (maxTimeToWaitInMilliseconds == 0) {
        throw std::invalid_argument("maxTimeToWaitInMilliseconds - Invalid wait time. Should be greater than 0.");
    }

    std::unique_lock<std::mutex> lock(*(this->waitConditionMutex));

    bool signalled = this->waitCondition->wait_for(lock, std::chrono::milliseconds(maxTimeToWaitInMilliseconds), [this] { return !this->Waiting(); });

    lock.unlock();

    return signalled;
}
