#include "vcommsession.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/random_generator.hpp>

VCommSession::VCommSession(const std::string& name, SessionOperationState initialMessageReceptionState, SessionOperationState initialMessageProcessingState) : 
                                                        id(boost::uuids::random_generator()()), name(name), userName(UNINITIALIZED_USER_NAME), messageReceptionMode(TaskExecutionMode::Sequential),
                                                        messageReceptionState(initialMessageReceptionState), messageProcessingState(initialMessageProcessingState) {
}

VCommSession::~VCommSession() {
}

boost::uuids::uuid VCommSession::Id() const {
    return this->id;
}

std::string VCommSession::IdAsString() const {
	std::string sessionIdAsString = boost::uuids::to_string(this->id);

    return sessionIdAsString;
}

std::string VCommSession::Name() const {
    return this->name;
}

std::string VCommSession::UserName() {
    std::lock_guard<std::mutex> lock(this->updateUserNameMutex);

    return this->userName;
}

TaskExecutionMode VCommSession::MessageReceptionMode() const {
    return this->messageReceptionMode;
}

SessionOperationState VCommSession::MessageReceptionState() const {
    return this->messageReceptionState;
}

SessionOperationState VCommSession::MessageProcessingState() const {
    return this->messageProcessingState;
}

void VCommSession::UpdateUserName(const std::string& updatedUserName) {
    std::lock_guard<std::mutex> lock(this->updateUserNameMutex);

    this->userName = updatedUserName;
}

void VCommSession::SetMessageReceptionMode(TaskExecutionMode inMode) {
    this->messageReceptionMode = inMode;
}

void VCommSession::SetMessageReceptionState(SessionOperationState currentState) {
    this->messageReceptionState = currentState;
}

void VCommSession::SetMessageProcessingState(SessionOperationState currentState) {
    this->messageProcessingState = currentState;
}

const std::string VCommSession::UNINITIALIZED_USER_NAME = "?";