#include "vwsautils.h"
#include "vwsaeventproducer.h"
#include "vhighresolutiontimehelper.h"

const Vu32 VCommSessionEventProducer::MAX_SOCKETS_PER_POLLING_THREAD = 63;
const Vu32 VCommSessionEventProducer::DEFAULT_SOCKETS_PER_POLLING_THREAD = 32;

VWSAEventProducer::VWSAEventProducer(const std::string& name, unsigned int minimumPollingThreads, unsigned int maximumEventsPerPollingThread) :
                                                                                VCommSessionEventProducer(name),
                                                                                maximumEventsPerPollingThread(maximumEventsPerPollingThread),
                                                                                minimumPollingThreads(minimumPollingThreads),
                                                                                abortIOWaitEvent(WSA_INVALID_EVENT), 
                                                                                abortIOWaitEventSet(false),
                                                                                waitTokenSource(false), started(false),
                                                                                sessions(std::make_shared<VCommSessionInfoSharedPtrVector>()) {
    VLOGGER_INFO(VSTRING_FORMAT("[COMM] VWSAEventProducer::c'tor - Configuration -> Minimum-Polling-Threads: %u, Maximum-Events-Per-Polling-Thread: %u", this->minimumPollingThreads, this->maximumEventsPerPollingThread));
}

VWSAEventProducer::~VWSAEventProducer() {
}

bool VWSAEventProducer::Start() {
    std::lock_guard<std::mutex> lock(this->startStopMutex);

    VLOGGER_INFO("[COMM] VWSAEventProducer::Start - Starting");

    if (this->cancellationSource.Cancelled()) {
        throw std::exception("[COMM] VWSAEventProducer::Start - WSA Comm Event Producer is stopped and cannot be restarted");
    }

    if (this->started) {
        return false;
    }

    this->abortIOWaitEvent = ::WSACreateEvent();

    if (this->abortIOWaitEvent == WSA_INVALID_EVENT) {
        // Should stop app
        throw std::exception(boost::str(boost::format{ "[COMM] VWSAEventProducer::Start - Failed to start: %1%" } % WSAUtils::ErrorMessage(::WSAGetLastError())).c_str());
    }

    for (unsigned int i = 1; i <= this->minimumPollingThreads; i++) {
        auto pollingThreadInfo = std::make_shared<VPollingThreadInfo>(i, 0, 0, this->waitTokenSource.Token(), this->abortIOWaitEvent, (boost::thread*)NULL);

        boost::thread* thread = threadGroup.create_thread(boost::bind(&VWSAEventProducer::ListenAndProduceEvents, this, pollingThreadInfo, this->sessions));

        pollingThreadInfo->PollingThread(thread);

        this->pollingThreads.push_back(pollingThreadInfo);
    }

    this->started = true;

    VLOGGER_INFO("[COMM] VWSAEventProducer::Start - Started");

    return true;
}

bool VWSAEventProducer::Stop() {
    if (!this->started || this->cancellationSource.Cancelled()) {
        return false;
    }

    this->started = false;

    this->cancellationSource.Cancel();

    std::lock_guard<std::mutex> lock(this->startStopMutex);

    VLOGGER_INFO(VSTRING_FORMAT("[COMM] VWSAEventProducer[%s]::Stop - Stopping...", VCommSessionEventProducer::Name().c_str()));

    // Stop all threads.
    BOOST_FOREACH(VPollingThreadInfoSharedPtr pollingThreadInfo, this->pollingThreads) {
        pollingThreadInfo->Cancel();
    }

    // Unblock all threads from waiting for socket I/O events.
    if (SetAbortIOWaitEvent()) {
        // Wait for all threads to terminate.
        this->threadGroup.join_all();
    } else {
        VLOGGER_ERROR(VSTRING_FORMAT("[COMM] VWSAEventProducer[%s]::Stop - Failed to set the abort event on stop", VCommSessionEventProducer::Name().c_str()));
    }

    this->pollingThreads.clear();

    this->sessions->clear();

    VLOGGER_INFO(VSTRING_FORMAT("[COMM] VWSAEventProducer[%s]::Stop - Stopped", VCommSessionEventProducer::Name().c_str()));

    return true;
}

bool VWSAEventProducer::Started() const {
    return (this->started && !this->cancellationSource.Cancelled());
}

bool VWSAEventProducer::CanStart() const {
    return (!this->started && !this->cancellationSource.Cancelled());
}

// To update the sessions, we need to pause/wait the polling threads that are waiting for events on the sessions' sockets.
// Until the update is completed, the polling threads should wait. 
// Once update is completed, the polling threads should continue. 
// We need to make sure that *all* the polling threads continue. If we don't, a call to WSAEventProducer::UpdateSessions in quick
// succession could put a polling thread back into waiting (extremely rare occurence but, possible).
//
// We do the sessions update in 7 steps.
// 1. Signal the threads to pause and unblock the polling threads that are waiting for socket events.
// 2. Wait for all the threads to pause.
// 3. Update the sessions' list (add newly connected sessions, remove disconnected ones). 
// 4. Redistribute the sessions among the polling threads evenly. 
// 5. Retire any excess threads - if required (happens if sessions disconnect).
// 6. Create new threads - if required (happens when we have new connections).
// 7. Signal the threads to continue and wait for all the threads (except newly created) to notify that they are continuing.
//
// Ground rules: 
// 'abort-wait' event   - Only WSAEventProducer::UpdateSessions sets and resets.
// 'join' event         - WSAEventProducer::UpdateSessions only resets. WSAEventProducer::ListenAndProduceEvents only sets.
void VWSAEventProducer::UpdateSessions(const VCommSessionInfoSharedPtrVector& newSessions, const VCommSessionInfoSharedPtrVector& closedSessions) {
    auto waitForAllJoinEvents = [](VPollingThreadInfoPtrVector& pollingThreads) {
        if (pollingThreads.size() == 0) {
            return;
        }

        std::vector<HANDLE> joinEvents;

        std::transform(pollingThreads.begin(), pollingThreads.end(), std::back_inserter(joinEvents), [](const VPollingThreadInfoSharedPtr& threadInfo) {
            return threadInfo->PollingThreadJoinEvent();
        });

        const HANDLE* pJoinEvents = &joinEvents[0];

        while (true) {
            DWORD waitResult = ::WaitForMultipleObjects(static_cast<DWORD>(joinEvents.size()), pJoinEvents, TRUE, POLLING_THREAD_JOIN_TIMEOUT);

            if (waitResult == WAIT_TIMEOUT) {
                // Check if all the threads have joined.

                bool allSignalled = true;

                BOOST_FOREACH(VPollingThreadInfoSharedPtr pollingThreadInfo, pollingThreads) {
                    allSignalled &= pollingThreadInfo->IsPollingThreadJoinEventSet();
                }

                if (allSignalled) {
                    break;
                }
            }

            if (waitResult == WAIT_FAILED) {
                std::string errorMessage = std::string("[COMM] VWSAEventProducer::UpdateSessions::waitForAllJoinEvents - Failed waiting for polling threads to join. Error: ") + WSAUtils::ErrorMessage(::GetLastError());

                throw std::exception(errorMessage.c_str());
            }

            break;
        }
    };

    if (!this->started) {
        if (this->cancellationSource.Cancelled()) {
            throw std::exception("[COMM] VWSAEventProducer::UpdateSessions - WSA Event Producer is stopped and cannot be used to manage sessions");
        } else {
            throw std::exception("[COMM] VWSAEventProducer::UpdateSessions - WSA Event Producer is not started and cannot be used to manage sessions");
        }
    }

    if (this->cancellationSource.Cancelled()) {
        return;
    }

    std::lock_guard<std::mutex> lock(this->startStopMutex);

    // Step 1
    // We can't update the sessions while the polling threads are running. 
    // Unblock the threads and signal them to pause.

    // Signal the threads to pause. 
    this->waitTokenSource.Wait();

    // Unblock the threads by setting the 'abort-waiting' event - if they are waiting for socket I/O events.
    if (!SetAbortIOWaitEvent()) {
        throw std::exception("[COMM] VWSAEventProducer::UpdateSessions - Failed to set abort-IO-wait event for pausing threads");
    }

    // Step 2
    // Wait for all the polling threads to notify us that they have paused.
   
    waitForAllJoinEvents(this->pollingThreads);

    // Step 3
    // At this point all the threads should have paused and we are free to update the sessions.

    // Remove the sessions that have been disconnected or if they have been removed.
    auto shouldRemoveSession = [&closedSessions](const VCommSessionInfoSharedPtr& sessionInfo) {
        bool removed = (sessionInfo->ConnectionState() != SessionConnectionState::Connected || 
                        (std::find_if(closedSessions.begin(), closedSessions.end(), VCommSessionInfoComparer(sessionInfo)) != std::end(closedSessions)));

        return removed;
    };

    this->sessions->erase(std::remove_if(this->sessions->begin(), this->sessions->end(), [shouldRemoveSession](const VCommSessionInfoSharedPtr& sessionInfo) {
        return shouldRemoveSession(sessionInfo);
    }), this->sessions->end());

    // Add new sockets - if there're any.
    if (newSessions.size() > 0) {
        this->sessions->insert(this->sessions->end(), newSessions.begin(), newSessions.end());
    }

    // Step 4
    // Try and redistribute the sockets among the threads. This is for faster response to socket I/O events from Windows.

    int numberOfSockets = static_cast<int>(this->sessions->size());

    std::div_t result = std::div(numberOfSockets, this->maximumEventsPerPollingThread);

    // Calculate the required number of sockets.
    unsigned int requiredSocketGroups = result.quot + (result.rem > 0 ? 1 : 0);

    if (requiredSocketGroups < this->minimumPollingThreads) {
        requiredSocketGroups = this->minimumPollingThreads;
    }

    // Calculate the number of sockets per group.
    result = std::div(numberOfSockets, requiredSocketGroups);

    // Step 5
    // Discard excess/retired threads.

    VPollingThreadInfoPtrVector removedThreads;

    if (requiredSocketGroups < this->pollingThreads.size()) {
        removedThreads.insert(removedThreads.end(), (this->pollingThreads.begin() + requiredSocketGroups), this->pollingThreads.end());
    }

    if (!removedThreads.empty()) {
        BOOST_FOREACH(VPollingThreadInfoSharedPtr pollingThreadInfo, removedThreads) {
            pollingThreadInfo->Cancel();

            boost::thread* pollingThread = pollingThreadInfo->PollingThread();

            this->threadGroup.remove_thread(pollingThread);

            // It is OK - the boost::thread object and the thread's lifetime are not dependent of each other.
            delete pollingThread;
        }
    }

    // Remove the threads that are not needed from our list.
    if (this->pollingThreads.size() > requiredSocketGroups) {
        this->pollingThreads.erase((this->pollingThreads.begin() + requiredSocketGroups), this->pollingThreads.end());
    }

    // Step 6
    // Update the existing (remaining) threads with the latest sessions' info.

    int offset = 0;
    int remainder = result.rem;
    int groupsCreated = 0;
    int threadId = 1;

    // Update existing threads' configuration/information.
    BOOST_FOREACH(VPollingThreadInfoSharedPtr pollingThreadInfo, this->pollingThreads) {
        int threadSockets = result.quot + (remainder-- > 0 ? 1 : 0);

        pollingThreadInfo->GroupOffset(offset);

        pollingThreadInfo->NumberOfSockets(threadSockets);

        pollingThreadInfo->ResetPollingThreadJoinEvent();

        offset += threadSockets;

        groupsCreated++;

        threadId++;

        // We'd signal the threads (below) to continue once the update is complete.
    }

    // Create any new threads (and start them) - if required.
    for (unsigned int i = (groupsCreated + 1); i <= requiredSocketGroups; i++, threadId++) {
        int groupSockets = result.quot + (remainder-- > 0 ? 1 : 0);

        auto pollingThreadInfo = std::make_shared<VPollingThreadInfo>(threadId, offset, groupSockets, this->waitTokenSource.Token(), this->abortIOWaitEvent, (boost::thread*)NULL);

        boost::thread* thread = threadGroup.create_thread(boost::bind(&VWSAEventProducer::ListenAndProduceEvents, this, pollingThreadInfo, this->sessions));

        pollingThreadInfo->PollingThread(thread);

        this->pollingThreads.push_back(pollingThreadInfo);

        offset += groupSockets;
    }

    VLOGGER_INFO(VSTRING_FORMAT("[COMM] VWSAEventProducer::UpdateSessions - Sessions: %d, PollingThreads: %d", this->sessions->size(), this->pollingThreads.size()));

    // Step 7
    // Update is complete. Signal the paused threads to continue and make sure that all
    // of them have unblocked.
    //

    // We need to reset the 'abort-waiting' event before signalling the threads to continue.
    if (!ResetAbortIOWaitEvent()) {
        throw std::exception("[COMM] VWSAEventProducer::UpdateSessions - Failed to set abort-IO-wait event for resuming threads");
    }

    // Signal the threads to continue.
    this->waitTokenSource.Continue();

    // Wait for all the polling threads to continue. They'll notify us through the 'join' events.
    waitForAllJoinEvents(this->pollingThreads);

    // Don't forget to reset the 'join' events.
    BOOST_FOREACH(VPollingThreadInfoSharedPtr pollingThreadInfo, this->pollingThreads) {
        pollingThreadInfo->ResetPollingThreadJoinEvent();
    }
}

void VWSAEventProducer::ListenAndProduceEvents(const VPollingThreadInfoSharedPtr& pollingThreadInfo, const std::shared_ptr<VCommSessionInfoSharedPtrVector>& inSessions) {
    std::string logPrefix = boost::str(boost::format{ "[COMM] VWSAEventProducer[Thread-%1%]::ListenAndProduceEvents" } % pollingThreadInfo->Id());

    VWaitToken threadWaitToken = pollingThreadInfo->ThreadWaitToken();

    WSAEVENT aiowEvent = pollingThreadInfo->AbortIOWaitEvent();

    WSAEVENT* pWSAEvents = NULL;

    DWORD totalEvents = 0;

    DWORD eventIndex = 0;

    VCommSessionInfoSharedPtrVector localSessions;
    std::vector<WSAEVENT> eventsToWaitFor;

    bool refreshSessionsList = true;

    boost::optional<bool> isNewThreadSynchronized;

    int debug_JoinEventSetCount = 0;
    boost::optional<bool> debug_IsWaitingForContinuation;
    bool debug_IsAbortIOEventSet;

    VLOGGER_INFO(VSTRING_FORMAT("%s - Started", logPrefix.c_str()));

    auto threadCancelled = [&logPrefix, &pollingThreadInfo, this]() -> bool {
        // If the producer is stopped...
        if (this->cancellationSource.Cancelled()) {
            VLOGGER_INFO(VSTRING_FORMAT("%s - Exiting as VWSAEventProducer is cancelled", logPrefix.c_str()));

            return true;
        }

        // If this polling thread is stopped...
        if (pollingThreadInfo->Cancelled()) {
            VLOGGER_INFO(VSTRING_FORMAT("%s - PollingThreadInfo is cancelled. Exiting...", logPrefix.c_str()));

            return true;
        }

        return false;
    };

    auto resetDebugFlags = [&]() {
        debug_JoinEventSetCount = 0;
        debug_IsWaitingForContinuation = boost::none;
        debug_IsAbortIOEventSet = false;
    };

    while (!threadCancelled()) {
        if (refreshSessionsList) {
            VLOGGER_TRACE(VSTRING_FORMAT("%s - PollingThreadInfo -> %s", logPrefix.c_str(), pollingThreadInfo->ToString().c_str()));

            refreshSessionsList = false;

            localSessions.clear();

            eventsToWaitFor.clear();

            if (threadCancelled()) {
                break;
            }

            // Collect the events to wait for. The first event would be the 'abort-wait' event.
            eventsToWaitFor.push_back(aiowEvent);

            // Get events for all the sockets allocated to this thread.
            auto startPosition = (inSessions->begin() + pollingThreadInfo->GroupOffset());

            auto endPosition = (startPosition + pollingThreadInfo->NumberOfSockets());

            std::transform(startPosition, endPosition, std::back_inserter(eventsToWaitFor), [](const VCommSessionInfoSharedPtr& session) {
                return session->SocketEvent();
            });

            std::transform(startPosition, endPosition, std::back_inserter(localSessions), [](const VCommSessionInfoSharedPtr& session) {
                return session;
            });

            pWSAEvents = &eventsToWaitFor[0];

            totalEvents = static_cast<DWORD>(eventsToWaitFor.size());
        }

        if (!isNewThreadSynchronized.is_initialized()) {
            isNewThreadSynchronized = false;

            // Wait till we are signalled to continue. 
            threadWaitToken.WaitUntilContinuation(logPrefix);

            // Notify that we are synchronized and continuing.
            if (!pollingThreadInfo->SetPollingThreadJoinEvent()) {
                VLOGGER_WARN(VSTRING_FORMAT("%s - Failed to set the polling thread's join event on sync.", logPrefix.c_str()));
            }

            isNewThreadSynchronized = true;
        }

        // Wait for socket I/O events from Windows.
        // If the sessions need to be updated, the WSAEventProducer::UpdateSessions would set the 'abort-wait' event to unblock us.

        bool expectedToJoinOnAbortIOWaitEvent = false;

        bool stopThreadOnWaitError = false;

        while (true) {
            eventIndex = ::WSAWaitForMultipleEvents(static_cast<DWORD>(eventsToWaitFor.size()), pWSAEvents, FALSE, LISTENER_THREAD_IO_WAIT_TIMEOUT, FALSE);

            if (threadCancelled()) {
                break;
            }

            if (eventIndex == WSA_WAIT_FAILED) {
                VLOGGER_ERROR(VSTRING_FORMAT("%s - ::WSAEWSAWaitForMultipleEvents failed with error: %s", logPrefix.c_str(), WSAUtils::ErrorMessage(::WSAGetLastError()).c_str()));

                stopThreadOnWaitError = true;

                break;
            } else if (eventIndex == WSA_WAIT_TIMEOUT) {
                if (this->abortIOWaitEventSet) {
                    expectedToJoinOnAbortIOWaitEvent = true;

                    break;
                }

                continue;
            } 

            // We have IO event(s) on socket(s) or, the abort-IO-wait event is set.
            eventIndex = (eventIndex - WSA_WAIT_EVENT_0);

            break;
        }

        if (stopThreadOnWaitError || threadCancelled()) {
            break;
        }

        // Unlikely to happen...
        if (!expectedToJoinOnAbortIOWaitEvent && eventIndex >= totalEvents) { // If we timed out, 'eventIndex' would be WSA_WAIT_TIMEOUT (258)
            VLOGGER_ERROR(VSTRING_FORMAT("%s - ::WSAEWSAWaitForMultipleEvents returned invalid index (%d). Total events: %d", logPrefix.c_str(), eventIndex, totalEvents));

            continue;
        }

        if (!expectedToJoinOnAbortIOWaitEvent) {
            expectedToJoinOnAbortIOWaitEvent = (eventsToWaitFor[eventIndex] == aiowEvent);
        }

        // If 'abort-wait' is set by WSAEventProducer::UpdateSessions, 
        if (expectedToJoinOnAbortIOWaitEvent) {
            resetDebugFlags();

            debug_IsAbortIOEventSet = true;

            // Notify that we have aborted and waiting.
            if (!pollingThreadInfo->SetPollingThreadJoinEvent()) {
                VLOGGER_ERROR(VSTRING_FORMAT("%s - Failed to set the polling thread's join event. TERMINATING this polling thread.", logPrefix.c_str()));

                break;
            }

            debug_JoinEventSetCount++;

            debug_IsWaitingForContinuation = true;

            // Wait till we are signalled to continue. 
            threadWaitToken.WaitUntilContinuation(logPrefix);

            debug_IsWaitingForContinuation = false;

            // Notify that we are continuing.
            if (!pollingThreadInfo->SetPollingThreadJoinEvent()) {
                VLOGGER_ERROR(VSTRING_FORMAT("%s - Failed to set the polling thread's join event. TERMINATING this polling thread.", logPrefix.c_str()));

                break;
            }

            debug_JoinEventSetCount++;

            // The sessions would have been updated. So, we need to start fresh.
            refreshSessionsList = true;

            continue;
        }

        WSANETWORKEVENTS networkEvents = {};

        std::vector<VCommSessionInfoSharedPtr> readingSessions;
        std::vector<VCommSessionInfoSharedPtr> closedSessions;

        // Find out what sockets are signalled for read/close. 
        // Looping through all the sockets will save waiting for notifications from Windows.
        for (DWORD index = eventIndex; index < eventsToWaitFor.size(); index++) {
            // TODO: mohanla - Check for thread cancellation?

            // Since the first event in the 'eventsToWaitFor' is 'abort-event', skip it.
            VCommSessionInfoSharedPtr session = localSessions[index - 1];

            eventIndex = ::WSAWaitForMultipleEvents(1, &eventsToWaitFor[index], TRUE, 0, FALSE);

            if (eventIndex == WSA_WAIT_FAILED) {
                continue;
            }

            // If the session has already disconnected, ignore the event.
            if (session->ConnectionState() != SessionConnectionState::Connected) {
                continue;
            }

            session->ResetSocketEvent();

            VSocketID socket = session->Socket();

            bool closeSocket = (socket == INVALID_SOCKET); // Means, socket has disconnected.

            if (!closeSocket) {
                int result = ::WSAEnumNetworkEvents(socket, eventsToWaitFor[index], &networkEvents);

                if (result == SOCKET_ERROR) {
                    int errorCode = ::WSAGetLastError();

                    if (errorCode != WSAENOTSOCK) {
                        VLOGGER_ERROR(VSTRING_FORMAT("%s - ::WSAEnumNetworkEvents returned error while enumerating events for session %s. Error: %s",
                            logPrefix.c_str(), session->ToString().c_str(), WSAUtils::ErrorMessage(errorCode).c_str()));

                        continue;
                    }

                    closeSocket = true;
                }
            }

            if (!closeSocket && networkEvents.lNetworkEvents & FD_READ) { // We have a read event.
                if (networkEvents.iErrorCode[FD_READ_BIT] != 0) {
                    VLOGGER_ERROR(VSTRING_FORMAT("%s - Read bit indicates error '%d' for session %s", logPrefix.c_str(), networkEvents.iErrorCode[FD_READ_BIT], session->ToString().c_str()));

                    continue;
                }

                readingSessions.push_back(session);
            } else if (closeSocket || networkEvents.lNetworkEvents & FD_CLOSE) { // We have a close event.
                if (!closeSocket && networkEvents.iErrorCode[FD_CLOSE_BIT] != 0) {
                    VLOGGER_ERROR(VSTRING_FORMAT("%s - Closed bit indicates error '%d' for session %s. Disconnecting session...", logPrefix.c_str(), networkEvents.iErrorCode[FD_CLOSE_BIT], session->ToString().c_str()));
                }

                session->SetAsDisconnected();

                closedSessions.push_back(session);

                VLOGGER_INFO(VSTRING_FORMAT("%s - REMOVED session %s", logPrefix.c_str(), session->ToString().c_str()));
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

    // Notify that we have exited.
    if (pollingThreadInfo && !pollingThreadInfo->SetPollingThreadJoinEvent()) {
        VLOGGER_ERROR(VSTRING_FORMAT("%s - Failed to set the polling thread's join event.", logPrefix.c_str()));
    }

    VLOGGER_INFO(VSTRING_FORMAT("%s - Stopped", logPrefix.c_str()));
}

bool VWSAEventProducer::SetAbortIOWaitEvent() {
    bool eventIsSet = false;

    // If the event is already set...
    if (!this->abortIOWaitEventSet.compare_exchange_strong(eventIsSet, true)) {
        return true;
    }

    if (!::WSASetEvent(this->abortIOWaitEvent)) {
        std::string errorMessage = std::string("Failed to set abort-IO-wait event: ") + WSAUtils::ErrorMessage(::WSAGetLastError());

        VLOGGER_ERROR(VSTRING_FORMAT("[COMM] VWSAEventProducer::SetAbortIOWaitEvent - %s", errorMessage.c_str()));

        this->abortIOWaitEventSet = false;

        return false;
    }

    return true;
}

bool VWSAEventProducer::ResetAbortIOWaitEvent() {
    bool eventIsSet = true;

    // If the event is already reset...
    if (!this->abortIOWaitEventSet.compare_exchange_strong(eventIsSet, false)) {
        return true;
    }

    if (!::WSAResetEvent(this->abortIOWaitEvent)) {
        std::string errorMessage = std::string("Failed to reset abort-IO-wait event: ") + WSAUtils::ErrorMessage(::WSAGetLastError());

        VLOGGER_ERROR(VSTRING_FORMAT("[COMM] VWSAEventProducer::ResetAbortIOWaitEvent - %s", errorMessage.c_str()));

        this->abortIOWaitEventSet = true;

        return false;
    }

    return true;
}
