#include "vlogger.h"
#include "vwsautils.h"
#include "vpollingthreadinfo.h"

VPollingThreadInfo::VPollingThreadInfo(unsigned int threadId, unsigned int groupOffset, unsigned int numberOfSockets,
                                        VWaitToken threadWaitToken, WSAEVENT abortIOWaitEvent, boost::thread* pollingThread) :
                                                id(threadId), groupOffset(groupOffset),
                                                numberOfSockets(numberOfSockets),
                                                threadWaitToken(threadWaitToken),
                                                abortIOWaitEvent(abortIOWaitEvent),
                                                isJoinEventSet(false),
                                                pollingThread(pollingThread),
                                                pollingThreadJoinEvent(NULL),
                                                threadExited(false) {
    this->pollingThreadJoinEvent = ::CreateEvent(NULL, true, false, VSTRING_FORMAT("PTJ-%d", this->id).chars());

    if (this->pollingThreadJoinEvent == NULL) {
        std::string errorMessage = std::string("[COMM] Failed to create VPollingThreadInfo instance: %d") + WSAUtils::ErrorMessage(::GetLastError());

        throw std::exception(errorMessage.c_str()); // Should stop app?
    }
}

VPollingThreadInfo::~VPollingThreadInfo() {
    if (this->pollingThreadJoinEvent != NULL) {
        ::CloseHandle(this->pollingThreadJoinEvent);
    }
}

unsigned int VPollingThreadInfo::Id() const {
    return this->id;
}

unsigned int VPollingThreadInfo::GroupOffset() const {
    return this->groupOffset;
}

void VPollingThreadInfo::GroupOffset(unsigned int inGroupOffset) {
    this->groupOffset = inGroupOffset;
}

unsigned int VPollingThreadInfo::NumberOfSockets() const {
    return this->numberOfSockets;
}

void VPollingThreadInfo::NumberOfSockets(unsigned int inNumberOfSockets) {
    this->numberOfSockets = inNumberOfSockets;
}

VWaitToken VPollingThreadInfo::ThreadWaitToken() const {
    return this->threadWaitToken;
}

WSAEVENT VPollingThreadInfo::AbortIOWaitEvent() const {
    return this->abortIOWaitEvent;
}

boost::thread* VPollingThreadInfo::PollingThread() const {
    return this->pollingThread;
}

void VPollingThreadInfo::PollingThread(boost::thread* inPollingThread) {
    this->pollingThread = inPollingThread;
}

HANDLE VPollingThreadInfo::PollingThreadJoinEvent() const {
    return this->pollingThreadJoinEvent;
}

bool VPollingThreadInfo::SetPollingThreadJoinEvent() {
    bool eventIsSet = false;

    // If the event is already set...
    if (!this->isJoinEventSet.compare_exchange_strong(eventIsSet, true)) {
        return true;
    }

    if (!::SetEvent(this->pollingThreadJoinEvent)) {
        std::string errorMessage = std::string("Failed to set polling thread's 'join' event: ") + WSAUtils::ErrorMessage(::GetLastError());

        VLOGGER_ERROR(VSTRING_FORMAT("[COMM] VPollingThreadInfo[%d]::SetPollingThreadJoinEvent - %s", this->id, errorMessage.c_str()));

        this->isJoinEventSet = false;

        return false;
    }

    return true;
}

bool VPollingThreadInfo::ResetPollingThreadJoinEvent() {
    bool eventIsSet = true;

    // If the event is already reset...
    if (!this->isJoinEventSet.compare_exchange_strong(eventIsSet, false)) {
        return true;
    }

    if (!::ResetEvent(this->pollingThreadJoinEvent)) {
        std::string errorMessage = std::string("Failed to reset polling thread's 'join' event: ") + WSAUtils::ErrorMessage(::GetLastError());

        VLOGGER_ERROR(VSTRING_FORMAT("[COMM] VPollingThreadInfo[%d]::ResetPollingThreadJoinEvent - %s", this->id, errorMessage.c_str()));

        this->isJoinEventSet = true;

        return false;
    }

    return true;
}

bool VPollingThreadInfo::IsPollingThreadJoinEventSet() const {
    return this->isJoinEventSet;
}

bool VPollingThreadInfo::ThreadExited() const {
    return this->threadExited;
}

bool VPollingThreadInfo::SetThreadExited() {
    if (this->threadExited) {
        this->threadExited = true;

        return false;
    }

    return false;
}

bool VPollingThreadInfo::Cancelled() const {
    return this->pollingCancellationSource.Cancelled();
}

bool VPollingThreadInfo::Cancel() {
    return this->pollingCancellationSource.Cancel();
}

std::string VPollingThreadInfo::ToString() const {
    return boost::str(boost::format{ "{Id: %1%, Group-Offset: %2%, Number-Of-Sockets: %3%, Waiting: %4%, Cancelled: %5%" } % id % groupOffset % numberOfSockets % threadWaitToken.Waiting() % Cancelled());
}
