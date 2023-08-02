#include "veventproducer.h"
#include "vhighresolutiontimehelper.h"

VEventProducer::VEventProducer(const std::string& name, unsigned int /*minimumPollingThreads*/, unsigned int /*maximumEventsPerPollingThread*/) :
    VCommSessionEventProducer(name),
    started(false),
    sessions(std::make_shared<VCommSessionInfoSharedPtrMap>())
{
    VLOGGER_INFO("[COMM] VEventProducer::c'tor");
}

VEventProducer::~VEventProducer() {
}

bool VEventProducer::Start() {
    std::lock_guard<std::mutex> lock(this->startStopMutex);

    VLOGGER_INFO("[COMM] VEventProducer::Start - Starting");

    if (this->cancellationSource.Cancelled()) {
        VLOGGER_FATAL_AND_THROW("[COMM] VEventProducer::Start - Comm Event Producer is stopped and cannot be restarted");
    }

    if (this->started) {
        return false;
    }

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        VLOGGER_FATAL_AND_THROW(VSTRING_FORMAT("[COMM] VEventProducer::Start - epoll_create failed: %s", strerror(errno)));
    }

    threadGroup.create_thread(boost::bind(&VEventProducer::ListenAndProduceEvents, this, this->sessions));

    this->started = true;

    VLOGGER_INFO("[COMM] VEventProducer::Start - Started");

    return true;
}

bool VEventProducer::Stop() {
    if (!this->started || this->cancellationSource.Cancelled()) {
        return false;
    }

    this->started = false;

    this->cancellationSource.Cancel();

    std::lock_guard<std::mutex> lock(this->startStopMutex);

    VLOGGER_INFO(VSTRING_FORMAT("[COMM] VEventProducer[%s]::Stop - Stopping...", VCommSessionEventProducer::Name().c_str()));

    this->threadGroup.join_all();

    this->sessions->clear();

    VLOGGER_INFO(VSTRING_FORMAT("[COMM] VEventProducer[%s]::Stop - Stopped", VCommSessionEventProducer::Name().c_str()));

    return true;
}

bool VEventProducer::Started() const {
    return (this->started && !this->cancellationSource.Cancelled());
}

bool VEventProducer::CanStart() const {
    return (!this->started && !this->cancellationSource.Cancelled());
}

void VEventProducer::UpdateSessions(const VCommSessionInfoSharedPtrVector& newSessions, const VCommSessionInfoSharedPtrVector& closedSessions) {
    if (!this->started) {
        if (this->cancellationSource.Cancelled()) {
            VLOGGER_FATAL_AND_THROW("[COMM] VEventProducer::UpdateSessions - Event Producer is stopped and cannot be used to manage sessions");
        }
        else {
            VLOGGER_FATAL_AND_THROW("[COMM] VEventProducer::UpdateSessions - Event Producer is not started and cannot be used to manage sessions");
        }
    }

    if (this->cancellationSource.Cancelled()) {
        return;
    }

    std::lock_guard<std::mutex> lock(this->startStopMutex);

    BOOST_FOREACH(VCommSessionInfoSharedPtr closedSession, closedSessions) {
        if (this->sessions->erase(closedSession->Socket()) == 0) {
            VLOGGER_ERROR(VSTRING_FORMAT("[COMM] VEventProducer::UpdateSessions - collection erase failed: %s", closedSession->ToString().c_str()));
        }
        else {
            VLOGGER_DEBUG(VSTRING_FORMAT("[COMM] VEventProducer::UpdateSessions - erased from collection: %s", closedSession->ToString().c_str()));
        }
    }

    BOOST_FOREACH(VCommSessionInfoSharedPtr newSession, newSessions) {
        auto addNewResult = this->sessions->emplace(newSession->Socket(), newSession); // TODO: insert_or_assign (C++17)?
        if (!addNewResult.second) {
            VLOGGER_ERROR(VSTRING_FORMAT("[COMM] VEventProducer::UpdateSessions - collection add failed: %s, already contained %s", newSession->ToString().c_str(), sessions->at(newSession->Socket())->ToString().c_str()));
            continue;
        }
        VLOGGER_DEBUG(VSTRING_FORMAT("[COMM] VEventProducer::UpdateSessions - added session: %s", newSession->ToString().c_str()));
        EPOLL_EVENT event = newSession->SocketEvent();
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, newSession->Socket(), &event) == -1) {
            VLOGGER_ERROR(VSTRING_FORMAT("[COMM] VEventProducer::UpdateSessions - epoll add failed: %s - error: %s", newSession->ToString().c_str(), strerror(errno)));
        }
    }
}

void VEventProducer::ReArmSession(const VCommSessionInfoSharedPtr& inSession) {
    EPOLL_EVENT event = inSession->SocketEvent();
    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, inSession->CommSession()->Socket(), &event)) {
        VLOGGER_ERROR(VSTRING_FORMAT("[COMM] VEventProducer::ReArmSession - epoll rearm failed: %s - error: %s", inSession->ToString().c_str(), strerror(errno)));
    }
    VLOGGER_TRACE(VSTRING_FORMAT("[COMM] VEventProducer::ReArmSession - rearmed session: %s", inSession->ToString().c_str()));
}

void VEventProducer::ListenAndProduceEvents(const std::shared_ptr<VCommSessionInfoSharedPtrMap>& inSessions) {
    VLOGGER_INFO("VEventProducer Started");

    EPOLL_EVENT events[MAX_EPOLL_EVENTS];
    int numEvents;

    while (!this->cancellationSource.Cancelled()) {
        while (true) {
            numEvents = epoll_wait(epoll_fd, events, MAX_EPOLL_EVENTS, EPOLL_WAIT_IMMEDIATE_RETURN);
            if (numEvents == -1) {
                VLOGGER_ERROR(VSTRING_FORMAT("[COMM] VEventProducer::ListenAndProduceEvents - epoll_wait failed with error: %s", strerror(errno)));
                break;
            }
            if (numEvents == 0) {
                VLOGGER_TRACE("[COMM] VEventProducer::ListenAndProduceEvents - no events found");
                continue;
            }
            break;
        }

        VLOGGER_TRACE(VSTRING_FORMAT("[COMM] VEventProducer::ListenAndProduceEvents - found %d event(s)", numEvents));

        std::vector<VCommSessionInfoSharedPtr> readingSessions;
        std::vector<VCommSessionInfoSharedPtr> closedSessions;

        for (Vu16 i = 0; i < numEvents; i++) {
            if ((events[i].events & EPOLLRDHUP) || (events[i].events & EPOLLHUP)) { // Query sessions signal EPOLLHUP on close (is this bad?)
                if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, NULL) == -1) {
                    VLOGGER_ERROR(VSTRING_FORMAT("[COMM] VEventProducer::ListenAndProduceEvents - EPOLL_CTL_DEL failed on fd<%d> with error : %s", events[i].data.fd, strerror(errno)));
                }
                auto session = inSessions->at(events[i].data.fd);
                session->SetAsDisconnected();
                closedSessions.push_back(session);
                VLOGGER_TRACE(VSTRING_FORMAT("[COMM] VEventProducer::ListenAndProduceEvents - registered session close: %s", session->ToString().c_str()));
            }
            else if (events[i].events & EPOLLIN) {
                auto session = inSessions->at(events[i].data.fd);
                readingSessions.push_back(session);
                VLOGGER_TRACE(VSTRING_FORMAT("[COMM] VEventProducer::ListenAndProduceEvents - registered session read: %s", session->ToString().c_str()));
            }
            else if (VLogger::isDefaultLogLevelActive(VLoggerLevel::DEBUG)) {
                std::bitset<32> bits(events[i].events);
                std::string bitStr = bits.to_string();
                VLOGGER_DEBUG(VSTRING_FORMAT("[COMM] VEventProducer::ListenAndProduceEvents - event bits: <%s>", bitStr.c_str()));
            }
        }

        if (readingSessions.size() > 0) {
            VCommSessionReadEventSharedPtr readEventArgs = std::make_shared<VCommSessionReadEvent>(readingSessions);
            RaiseReadEvent(readEventArgs);
        }

        if (closedSessions.size() > 0) {
            VCommSessionClosedEventSharedPtr closedEventArgs = std::make_shared<VCommSessionClosedEvent>(closedSessions);
            RaiseClosedEvent(closedEventArgs);
        }
    }

    VLOGGER_INFO("VEventProducer Stopped");
}

// Unused values. Definition required for abstract reference
const Vu32 VCommSessionEventProducer::MAX_SOCKETS_PER_POLLING_THREAD = 100;
const Vu32 VCommSessionEventProducer::DEFAULT_SOCKETS_PER_POLLING_THREAD = 0;