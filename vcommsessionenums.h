#ifndef vcommsessionenums_h
#define vcommsessionenums_h

#include "vault.h"

/**
Represents a session's connection state.
*/
enum class SessionConnectionState {
    /**
    Represents the state before a connection is established.
    */
    NotConnected = 0,

    /**
    State after a successful connection is established.
    */
    Connected = 1,

    /**
    Respresents the state after an active connection is torn down.
    */
    Disconnected = 2
};

class SessionConnectionStateConverter {
private:
    SessionConnectionStateConverter();

public:
    SessionConnectionStateConverter(const SessionConnectionStateConverter& other) = delete;
    SessionConnectionStateConverter& operator=(const SessionConnectionStateConverter& other) = delete;

    static const std::string& ToString(SessionConnectionState connectionState);

    // Add FromString if needed.

private:
    static std::map<SessionConnectionState, std::string> EnumToStringMap;

    static std::once_flag MapsInitialized;
};

/**
Indicates if a session would continue to receive messages even after disconnection.
Most of our sessions - especially ad hoc sessions such as S3_QUERY - send a message and disconnect immediately after the send.
So, most of our sessions would support message reception after disconnection.
*/
enum class MessageReceptionAfterDisconnection {
    NotSupported = 0,
    Supported = 1
};

class MessageReceptionAfterDisconnectionConverter {
private:
    MessageReceptionAfterDisconnectionConverter();

public:
    MessageReceptionAfterDisconnectionConverter(const MessageReceptionAfterDisconnectionConverter& other) = delete;
    MessageReceptionAfterDisconnectionConverter& operator=(const MessageReceptionAfterDisconnectionConverter& other) = delete;

    static const std::string& ToString(MessageReceptionAfterDisconnection connectionState);

private:
    static std::map<MessageReceptionAfterDisconnection, std::string> EnumToStringMap;

    static std::once_flag MapsInitialized;
};

/**
Indicates a session's willingness to handle/process messages even after disconnection.
Most of our sessions - especially ad hoc sessions such as S3_QUERY - send a message and disconnect immediately after the send.
So, most of our sessions would support message processing even after disconnection.
*/
enum class MessageProcessingAfterDisconnection {
    NotSupported = 0,
    Supported = 1
};

class MessageProcessingAfterDisconnectionConverter {
private:
    MessageProcessingAfterDisconnectionConverter();

public:
    MessageProcessingAfterDisconnectionConverter(const MessageProcessingAfterDisconnectionConverter& other) = delete;
    MessageProcessingAfterDisconnectionConverter& operator=(const MessageProcessingAfterDisconnectionConverter& other) = delete;

    static const std::string& ToString(MessageProcessingAfterDisconnection connectionState);

private:
    static std::map<MessageProcessingAfterDisconnection, std::string> EnumToStringMap;

    static std::once_flag MapsInitialized;
};

#endif