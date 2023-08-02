#include "vcommsessioninfo.h"
#include <errno.h>
#include <unistd.h>
#include <boost/lexical_cast.hpp>
#include <boost/uuid/uuid_io.hpp>

VCommSessionInfo::VCommSessionInfo(const std::string& name, VCommSession* commSession, SessionConnectionState connectionState, MessageReceptionAfterDisconnection messageReceptionAfterDisconnection,
    MessageProcessingAfterDisconnection messageProcessingAfterDisconnection) :
    id(commSession->Id()),
    name(name),
    commSession(commSession),
    connectionState(connectionState),
    messageReceptionAfterDisconnection(messageReceptionAfterDisconnection),
    messageProcessingAfterDisconnection(messageProcessingAfterDisconnection),
    messagesWaitingToBeProcessed(0)
{
    commSession->IncrementRefCount();

    if (connectionState == SessionConnectionState::Connected) {
        ConfigureSocketEvent();
    }
}

VCommSessionInfo::~VCommSessionInfo() {
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

EPOLL_EVENT VCommSessionInfo::SocketEvent() const {
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

void VCommSessionInfo::ConfigureSocketEvent() {
    /*
    * Notes on what these events mean:
    * EPOLLIN - Monitor for incoming data on the socket available to read. We do not monitor for write events, so the complimentary EPOLLOUT flag is not set.
    * EPOLLRDHUP - Monitor for socket peer closing connection. Note that the similar EPOLLHUP flag is set by default and montiors "unexpected" socket close by peer.
    * EPOLLET - (E)dge (T)riggered notification, meaning notification for an event is only provided once when it occurs. As opposed to the default of 
    *   Level Triggered, which will continue notifying as long as data exists in the socket (has not been read). There is no functional impact for message based
    *   communication, but ET is nominally more efficient than LT.
    * EPOLLONESHOT - Disable monitoring for this socket after a event occurs. This is to allow time for the read to be performed by a separate thread before querying
    *   for new events. The socket must be explicitly rearmed using EPOLL_CTL_MOD after reading (see VEventProducer::ReArmSession).
    */
    socketEvent.events = EPOLLIN | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    socketEvent.data.fd = this->commSession->Socket(); // This is technically not needed, but best practice to set if no other use planned
}
