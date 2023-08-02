#include "VCommSessionInfo.h"
#include "vwsautils.h"

VCommSessionInfo::VCommSessionInfo(const std::string& name, VCommSession* commSession, SessionConnectionState connectionState, MessageReceptionAfterDisconnection messageReceptionAfterDisconnection,
                                                                        MessageProcessingAfterDisconnection messageProcessingAfterDisconnection) :
                                                                        id(commSession->Id()),
                                                                        name(name),
                                                                        commSession(commSession),
                                                                        connectionState(connectionState),
                                                                        messageReceptionAfterDisconnection(messageReceptionAfterDisconnection),
                                                                        messageProcessingAfterDisconnection(messageProcessingAfterDisconnection),
                                                                        messagesWaitingToBeProcessed(0) {
    commSession->IncrementRefCount();

    if (connectionState == SessionConnectionState::Connected) {
        CreateAndRegisterSocketEvent();
    }
}

VCommSessionInfo::~VCommSessionInfo() {
    ::WSACloseEvent(this->socketEvent);

    commSession->DecrementRefCount();
}

boost::uuids::uuid VCommSessionInfo::Id() const {
    return this->id;
}

std::string VCommSessionInfo::IdAsString() const {
    std::string sessionIdAsString = boost::lexical_cast<std::string>(this->id);

    return sessionIdAsString;
}

std::string VCommSessionInfo::Name() const {
    return this->name;
}

VSocketID VCommSessionInfo::Socket() const {
    return this->commSession->Socket();
}

VCommSession* VCommSessionInfo::CommSession() const {
    return this->commSession;
}

TaskExecutionMode VCommSessionInfo::MessageReceptionMode() const {
    return this->commSession->MessageReceptionMode();
}

WSAEVENT VCommSessionInfo::SocketEvent() const {
    return this->socketEvent;
}

SessionConnectionState VCommSessionInfo::ConnectionState() const {
    return this->connectionState;
}

bool VCommSessionInfo::SetAsConnected() {
    SessionConnectionState expectedState = SessionConnectionState::NotConnected;

    bool stateChanged = this->connectionState.compare_exchange_strong(expectedState, SessionConnectionState::Connected);

    return stateChanged;
}

bool VCommSessionInfo::SetAsDisconnected() {
    SessionConnectionState expectedState = SessionConnectionState::Connected;

    bool stateChanged = this->connectionState.compare_exchange_strong(expectedState, SessionConnectionState::Disconnected);

    return stateChanged;
}

bool VCommSessionInfo::ResetSocketEvent() {
    if (this->socketEvent != WSA_INVALID_EVENT && !::WSAResetEvent(this->socketEvent)) {
        std::string errorMessage = std::string("Failed to reset socket event: ") + WSAUtils::ErrorMessage(::WSAGetLastError());

        VLOGGER_ERROR(VSTRING_FORMAT("[COMM] VCommSessionInfo[%s]::ResetSocketEvent: %s", LoggerPrefix().c_str(), errorMessage.c_str()));

        return false;
    }

    return true;
}

MessageReceptionAfterDisconnection VCommSessionInfo::SupportForMessageReceptionAfterDisconnection() const {
    return this->messageReceptionAfterDisconnection;
}

MessageProcessingAfterDisconnection VCommSessionInfo::SupportForMessageProcessingAfterDisconnection() const {
    return this->messageProcessingAfterDisconnection;
}

SessionOperationState VCommSessionInfo::MessageReceptionState() const {
    return this->commSession->MessageReceptionState();
}

SessionOperationState VCommSessionInfo::MessageProcessingState() const {
    return this->commSession->MessageProcessingState();
}

Vu32 VCommSessionInfo::IncrementMessagesWaitingToBeProcessed() {
    return ++this->messagesWaitingToBeProcessed;
}

Vu32 VCommSessionInfo::DecrementMessagesWaitingToBeProcessed() {
    return --this->messagesWaitingToBeProcessed;
}

Vu32 VCommSessionInfo::GetNumberOfMessagesWaitingToBeProcessed() const {
    return this->messagesWaitingToBeProcessed;
}

std::string VCommSessionInfo::ToString() const {
    return boost::str(boost::format{ "{Id: %1%, Name: %2%, User: %3%, Socket: %4%, Connected?: %5%}" } 
                                        % this->IdAsString() % this->name % this->commSession->UserName().c_str() % Socket() % SessionConnectionStateConverter::ToString(this->connectionState).c_str());
}

void VCommSessionInfo::CreateAndRegisterSocketEvent() {
    this->socketEvent = ::WSACreateEvent();

    if (this->socketEvent == WSA_INVALID_EVENT) {
        std::string errorMessage = boost::str(boost::format{ "[COMM] Failed to create VCommSessionInfo instance - %1%: Invalid socket event" } % LoggerPrefix());

        throw std::invalid_argument(errorMessage.c_str()); // Should stop app?
    }

    int result = ::WSAEventSelect(this->commSession->Socket(), this->socketEvent, (FD_READ | FD_CLOSE));

    if (result == SOCKET_ERROR) {
        std::string errorMessage = boost::str(boost::format{ "[COMM] Failed to create VCommSessionInfo instance - %1%: Failed to set socket's event for read/close: %2%" } 
                                                                % LoggerPrefix() % WSAUtils::ErrorMessage(::WSAGetLastError()));

        throw std::exception(errorMessage.c_str()); // Should stop app?
    }
}

std::string VCommSessionInfo::LoggerPrefix() {
    return boost::str(boost::format{ "%1%/%2%{%3%}" } % this->IdAsString() % this->name % this->commSession->UserName());
}

VCommSessionInfoComparer::VCommSessionInfoComparer(const VCommSessionInfoSharedPtr& sessionInfo) : sessionInfo(sessionInfo) {
}

bool VCommSessionInfoComparer::operator() (const VCommSessionInfoSharedPtr& other) {
    if (!this->sessionInfo && !other) {
        return true;
    }

    if (this->sessionInfo && other) {
        return (this->sessionInfo->Id() == other->Id());
    }

    return false;
}
