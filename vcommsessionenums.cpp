#include "vcommsessionenums.h"

//
// Enum SessionConnectionStateConverter
//
SessionConnectionStateConverter::SessionConnectionStateConverter() {
}

const std::string& SessionConnectionStateConverter::ToString(SessionConnectionState enumValue) {
    std::call_once(SessionConnectionStateConverter::MapsInitialized, []() {
        EnumToStringMap = std::map<SessionConnectionState, std::string> {{ SessionConnectionState::NotConnected, "NotConnected"},
        { SessionConnectionState::Connected, "Connected" },
        { SessionConnectionState::Disconnected, "Disconnected" }};
    });

    return EnumToStringMap[enumValue];
}

std::map<SessionConnectionState, std::string> SessionConnectionStateConverter::EnumToStringMap;
std::once_flag SessionConnectionStateConverter::MapsInitialized;

//
// Enum MessageReceptionAfterDisconnectionConverter
//
MessageReceptionAfterDisconnectionConverter::MessageReceptionAfterDisconnectionConverter() {
}

const std::string& MessageReceptionAfterDisconnectionConverter::ToString(MessageReceptionAfterDisconnection enumValue) {
    std::call_once(MessageReceptionAfterDisconnectionConverter::MapsInitialized, []() {
        EnumToStringMap = std::map<MessageReceptionAfterDisconnection, std::string> { { MessageReceptionAfterDisconnection::NotSupported, "NotSupported"},
        { MessageReceptionAfterDisconnection::Supported, "Supported" }};
    });

    return EnumToStringMap[enumValue];
}

std::map<MessageReceptionAfterDisconnection, std::string> MessageReceptionAfterDisconnectionConverter::EnumToStringMap;
std::once_flag MessageReceptionAfterDisconnectionConverter::MapsInitialized;

//
// Enum MessageProcessingAfterDisconnectionConverter
//
MessageProcessingAfterDisconnectionConverter::MessageProcessingAfterDisconnectionConverter() {
}

const std::string& MessageProcessingAfterDisconnectionConverter::ToString(MessageProcessingAfterDisconnection enumValue) {
    std::call_once(MessageProcessingAfterDisconnectionConverter::MapsInitialized, []() {
        EnumToStringMap = std::map<MessageProcessingAfterDisconnection, std::string> { { MessageProcessingAfterDisconnection::NotSupported, "NotSupported"},
        { MessageProcessingAfterDisconnection::Supported, "Supported" }};
    });

    return EnumToStringMap[enumValue];
}

std::map<MessageProcessingAfterDisconnection, std::string> MessageProcessingAfterDisconnectionConverter::EnumToStringMap;
std::once_flag MessageProcessingAfterDisconnectionConverter::MapsInitialized;

