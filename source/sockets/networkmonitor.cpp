/*
Copyright (c) 2016. All rights reserved.
*/

#include "vhighresolutiontimehelper.h"
#include "vexception.h"
#ifdef VAULT_APP_CONFIG_SUPPORTED
#include "biz_config.h"
#endif
#include "networkmonitor.h"

/**
For Network Monitoring
*/

/*
NetworkTransactionLog
*/

NetworkTransactionLog::NetworkTransactionLog(const TransactionDirection& transactionChannel) : _channel(transactionChannel),
                                                                                                _transactedBytes(0),
                                                                                                _totalTransactionTimeInNS(0.0),
                                                                                                _transactionStartTimeInNS(0.0),
                                                                                                _isTransacting(false) {
    validateInitialization();
}

NetworkTransactionLog::NetworkTransactionLog(const TransactionDirection& transactionChannel, 
                                                                const bool& startTransacting) : _channel(transactionChannel),
                                                                                                _transactedBytes(0),
                                                                                                _totalTransactionTimeInNS(0.0),
                                                                                                _transactionStartTimeInNS(0.0),
                                                                                                _isTransacting(startTransacting) {
    validateInitialization();

    if (_isTransacting) {
        startTransaction();
    }
}

NetworkTransactionLog::~NetworkTransactionLog() {
    completeTransaction(0);
}

TransactionDirection NetworkTransactionLog::getTransactionDirection() const {
    return _channel;
}

Vu64 NetworkTransactionLog::getTransactedBytes() const {
    return _transactedBytes;
}

VDouble NetworkTransactionLog::getTransactionTimeInNanoSeconds() const {
    return _totalTransactionTimeInNS;
}

bool NetworkTransactionLog::isTransacting() {
    return _isTransacting;
}

void NetworkTransactionLog::startTransaction() {
    if (_isTransacting) {
        return;
    }

    _isTransacting = true;

    _transactionStartTimeInNS = VHighResolutionTimeHelper::GetTimeInNanoSeconds();
}

void NetworkTransactionLog::completeTransaction(const Vu64& bytesTransacted) {
    VDouble transactionCompletionTimeInNS = VHighResolutionTimeHelper::GetTimeInNanoSeconds();

    if (!_isTransacting) {
        return;
    }

    _isTransacting = false;

    if (bytesTransacted > 0) {
        _transactedBytes += bytesTransacted;

        VDouble transactionTimeInNS = (transactionCompletionTimeInNS - _transactionStartTimeInNS);

        _totalTransactionTimeInNS += transactionTimeInNS;
    }

    _transactionStartTimeInNS = 0.0;
}

void NetworkTransactionLog::validateInitialization() {
    if (_channel == TransactionDirection_Unavailable) {
        throw VException("Invalid transaction channel. Transaction channel cannot be 'TransactionDirection_Unavailable'.");
    }
}

/*
NetworkRxTransactionLog
*/

NetworkRxTransactionLog::NetworkRxTransactionLog() : NetworkTransactionLog(Rx) {
}

NetworkRxTransactionLog::NetworkRxTransactionLog(const bool& startLogging) : NetworkTransactionLog(Rx, startLogging) {
}

NetworkRxTransactionLog::~NetworkRxTransactionLog() {
}

/*
NetworkTxTransactionLog
*/

NetworkTxTransactionLog::NetworkTxTransactionLog() : NetworkTransactionLog(Tx) {
}

NetworkTxTransactionLog::NetworkTxTransactionLog(const bool& startLogging) : NetworkTransactionLog(Tx, startLogging) {
}

NetworkTxTransactionLog::~NetworkTxTransactionLog() {
}

/*
NetworkSession
*/

const Vu16 NetworkSession::MAX_LOGS_HISTORY = 100;      // Alternatively, this could come from a settings file.

NetworkSession::NetworkSession(const VString& sessionId, const SessionType& sessionType, const SessionDirection& sessionDirection,
                                                    const VString& endPointAddres, const Vu16& endPointPort) : _sessionId(sessionId),
                                                                                                                _sessionType(sessionType),
                                                                                                                _sessionDirection(sessionDirection),
                                                                                                                _endPointAddress(endPointAddres),
                                                                                                                _endPointPort(endPointPort) {
    validateInitialization();
}

NetworkSession::~NetworkSession() {
}

VString NetworkSession::getSessionId() const {
    return _sessionId;
}

SessionType NetworkSession::getSessionType() const {
    return _sessionType;
}

SessionDirection NetworkSession::getSessionDirection() const {
    return _sessionDirection;
}

VString NetworkSession::getEndPointAddress() const {
    return _endPointAddress;
}

Vu16 NetworkSession::getEndPointPort() const {
    return _endPointPort;
}

RxTransactionLogsWeakPtrList NetworkSession::getRxTransactionLogs() {
    RxTransactionLogsWeakPtrList rxLogsCopy;

    LOCK_MUTEX(_rxLogsMutex);

    RxTransactionLogsSharedPtrListIterator iterator = _rxLogs.begin();

    for (; iterator != _rxLogs.end(); ++iterator) {
        rxLogsCopy.push_back(NetworkRxTransactionLogWeakPtr(*iterator));
    }

    RELEASE_MUTEX(_rxLogsMutex);

    return rxLogsCopy;
}

TxTransactionLogsWeakPtrList NetworkSession::getTxTransactionLogs() {
    TxTransactionLogsWeakPtrList txLogsCopy;

    LOCK_MUTEX(_txLogsMutex);

    TxTransactionLogsSharedPtrListIterator iterator = _txLogs.begin();

    for (; iterator != _txLogs.end(); ++iterator) {
        txLogsCopy.push_back(NetworkTxTransactionLogWeakPtr(*iterator));
    }

    RELEASE_MUTEX(_txLogsMutex);

    return txLogsCopy;
}

TransactionLogsWeakPtrList NetworkSession::getTransactionLogs(const TransactionDirection& transactionDirection) {
    TransactionLogsWeakPtrList logsListCopy;

    if (transactionDirection == Rx || transactionDirection == TransactionDirection_Unavailable) {
        LOCK_MUTEX(_rxLogsMutex);

        RxTransactionLogsSharedPtrListIterator iterator = _rxLogs.begin();

        for (; iterator != _rxLogs.end(); ++iterator) {
            logsListCopy.push_back(NetworkTransactionLogWeakPtr(*iterator));
        }

        RELEASE_MUTEX(_rxLogsMutex);
    }

    if (transactionDirection == Tx || transactionDirection == TransactionDirection_Unavailable) {
        LOCK_MUTEX(_txLogsMutex);

        TxTransactionLogsSharedPtrListIterator iterator = _txLogs.begin();

        for (; iterator != _txLogs.end(); ++iterator) {
            logsListCopy.push_back(NetworkTransactionLogWeakPtr(*iterator));
        }

        RELEASE_MUTEX(_txLogsMutex);
    }

    return logsListCopy;
}

void NetworkSession::addRxTransactionLog(const NetworkRxTransactionLog& log) {
    if (log.getTransactedBytes() == 0) {
        return;
    }

    // TODO: Avoid copy?
    NetworkRxTransactionLogSharedPtr pLog = boost::make_shared<NetworkRxTransactionLog>(log);

    LOCK_MUTEX(_rxLogsMutex);
    if (_rxLogs.size() >= NetworkSession::MAX_LOGS_HISTORY) {
        _rxLogs.erase(_rxLogs.begin() + 1);
    }

    _rxLogs.push_back(pLog);
    RELEASE_MUTEX(_rxLogsMutex);
}

void NetworkSession::addTxTransactionLog(const NetworkTxTransactionLog& log) {
    if (log.getTransactedBytes() == 0) {
        return;
    }

    // TODO: Avoid copy?
    NetworkTxTransactionLogSharedPtr pLog = boost::make_shared<NetworkTxTransactionLog>(log);

    LOCK_MUTEX(_rxLogsMutex);
    if (_txLogs.size() >= NetworkSession::MAX_LOGS_HISTORY) {
        _txLogs.erase(_txLogs.begin() + 1);
    }

    _txLogs.push_back(pLog);
    RELEASE_MUTEX(_rxLogsMutex);
}

void NetworkSession::validateInitialization() {
    if (_sessionId.isEmpty()) {
        throw VException("Invalid session Id. Session Id cannot be empty.");
    }

    if (this->_sessionType == SessionType_Unavailable) {
        throw VException("Invalid session type. Session type cannot be 'SessionType_Unavailable'.");
    }

    if (_sessionDirection == SessionDirection_Unavailable) {
        throw VException("Invalid session direction. Session direction cannot be 'SessionDirection_Unavailable'.");
    }

    if (_endPointAddress.isEmpty()) {
        throw VException("Invalid end-point address. End-point address cannot be empty.");
    }

    if (_endPointPort == 0) { // TODO: Better range check.
        throw VException("Invalid end-point port. End-point port cannot be 0.");
    }
}

/*
NetworkMonitor
*/

boost::optional<bool> NetworkMonitor::instanceCreated;

NetworkMonitorSharedPtr NetworkMonitor::instance;

VMutex NetworkMonitor::instanceMutex("NetworkMonitorInstanceMutex", true);

const Vu16 NetworkMonitor::MAX_SESSIONS_HISTORY = 100;       // Alternatively, this could come from a settings file.

NetworkMonitor::NetworkMonitor() {
}

NetworkMonitor::~NetworkMonitor() {
}

NetworkSessionWeakPtr NetworkMonitor::createNetworkSession(const VString& clientId, const VString& sessionId, const SessionType& sessionType,
                                                            const SessionDirection& sessionDirection, const VString& endPointAddres,
                                                            const Vu16& endPointPort) {
    NetworkSessionSharedPtr pNetwworkSession = boost::make_shared<NetworkSession>(sessionId, sessionType, sessionDirection, endPointAddres, endPointPort);

    LOCK_MUTEX(_sessionsMapMutex);

    NetworkSessionsSharedPtrMapIterator iterator = _sessions.find(clientId);

    PointerToNetworkSessionsSharedPtrList pSessionsList;

    if (iterator == _sessions.end()) {
        pSessionsList = boost::make_shared<NetworkSessionsSharedPtrList>();

        _sessions[clientId] = pSessionsList;
    } else {
        pSessionsList = iterator->second;
    }

    if (pSessionsList->size() >= NetworkMonitor::MAX_SESSIONS_HISTORY) {
        pSessionsList->erase(pSessionsList->begin() + 1);
    }

    pSessionsList->push_back(pNetwworkSession);

    RELEASE_MUTEX(_sessionsMapMutex);

    return NetworkSessionWeakPtr(pNetwworkSession);
}

NetworkSessionsWeakPtrList NetworkMonitor::getSessionsList() {
    NetworkSessionsWeakPtrList sessionsListCopy;

    LOCK_MUTEX(_sessionsMapMutex);

    // TODO: For C++ 11 - Use for-each with custom converter.
    NetworkSessionsSharedPtrMapIterator mapIterator = _sessions.begin();

    for (; mapIterator != _sessions.end(); ++mapIterator) {
        PointerToNetworkSessionsSharedPtrList pNetworkSessionsList = mapIterator->second;

        NetworkSessionsSharedPtrListIterator sessionsListIterator = pNetworkSessionsList->begin();

        for (; sessionsListIterator != pNetworkSessionsList->end(); ++sessionsListIterator) {
            sessionsListCopy.push_back(NetworkSessionWeakPtr(*sessionsListIterator));
        }
    }

    RELEASE_MUTEX(_sessionsMapMutex);

    return sessionsListCopy;
}

NetworkSessionsWeakPtrMap NetworkMonitor::getSessionsMap() {
    NetworkSessionsWeakPtrMap sessionsMapCopy;

    LOCK_MUTEX(_sessionsMapMutex);

    // TODO: For C++ 11 - Use for-each with custom converter.
    NetworkSessionsSharedPtrMapIterator mapIterator = _sessions.begin();

    for (; mapIterator != _sessions.end(); ++mapIterator) {
        NetworkSessionsWeakPtrList sessionsListCopy;

        PointerToNetworkSessionsSharedPtrList pNetworkSessionsList = mapIterator->second;

        NetworkSessionsSharedPtrListIterator sessionsListIterator = pNetworkSessionsList->begin();

        for (; sessionsListIterator != pNetworkSessionsList->end(); ++sessionsListIterator) {
            sessionsListCopy.push_back(NetworkSessionWeakPtr(*sessionsListIterator));
        }

        sessionsMapCopy[mapIterator->first] = sessionsListCopy;
    }

    RELEASE_MUTEX(_sessionsMapMutex);

    return sessionsMapCopy;
}

void NetworkMonitor::clearClientLogs(const VString& clientId) {
    LOCK_MUTEX(_sessionsMapMutex);

    NetworkSessionsSharedPtrMapIterator mapIterator = _sessions.find(clientId);

    if (mapIterator != _sessions.end()) {
        _sessions.erase(mapIterator);
    } else {
        throw VException(VSTRING_FORMAT("Client with Id '%s' could not be found.", clientId.chars()));
    }

    RELEASE_MUTEX(_sessionsMapMutex);
}

void NetworkMonitor::clearAllLogs() {
    LOCK_MUTEX(_sessionsMapMutex);

    _sessions.clear();

    RELEASE_MUTEX(_sessionsMapMutex);
}

// Static
bool NetworkMonitor::isCapturingNetworkStatistics() {
#ifdef VAULT_APP_CONFIG_SUPPORTED
    return BizConfig::instance()->getConfigBoolean(riappConfigST5_CaptureNetworkStatistics);
#else
    return false;
#endif 
}

// Static 
VString NetworkMonitor::toString(const SessionType& sessionType) {
    VString sessionTypeStr = "SessionType_Unavailable";

    switch (sessionType) {
    case ClientSession:
        sessionTypeStr = "ClientSession"; break;

    case Query:
        sessionTypeStr = "Query"; break;

    case Standard:
        sessionTypeStr = "Standard"; break;

    case SessionType_Unavailable:
        break;
    }

    return sessionTypeStr;
}

// Static 
VString NetworkMonitor::toString(const SessionDirection& sessionDirection) {
    VString sessionDirectionStr = "SessionDirection_Unavailable";

    switch (sessionDirection) {
    case Incoming:
        sessionDirectionStr = "Incoming"; break;

    case Outgoing:
        sessionDirectionStr = "Outgoing"; break;

    case SessionDirection_Unavailable:
        break;
    }

    return sessionDirectionStr;
}

// Static 
VString NetworkMonitor::toString(const TransactionDirection& transactionDirection) {
    VString transactionDirectionStr = "TransactionDirection_Unavailable";

    switch (transactionDirection) {
    case Rx:
        transactionDirectionStr = "Rx"; break;

    case Tx:
        transactionDirectionStr = "Tx"; break;

    case TransactionDirection_Unavailable:
        break;
    }

    return transactionDirectionStr;
}

// Static 
SessionType NetworkMonitor::convertToSessionType(const VString& sessionTypeStr, bool passiveMode) {
    boost::optional<SessionType> sessionType;

    if (toString(ClientSession).compareIgnoreCase(sessionTypeStr) == 0) {
        sessionType = ClientSession;
    } else if (toString(Query).compareIgnoreCase(sessionTypeStr) == 0) {
        sessionType = Query;
    } else if (toString(Standard).compareIgnoreCase(sessionTypeStr) == 0) {
        sessionType = Standard;
    } else if (toString(SessionType_Unavailable).compareIgnoreCase(sessionTypeStr) == 0) {
        sessionType = SessionType_Unavailable;
    }

    if (!sessionType.is_initialized()) {
        if (!passiveMode) {
            throw VException(VSTRING_FORMAT("Failed to convert '%s' to SessionType - Invalid enum value.", sessionTypeStr.chars()));
        }

        sessionType = SessionType_Unavailable;
    }

    return *sessionType;
}

// Static 
SessionDirection NetworkMonitor::convertToSessionDirection(const VString& sessionDirectionStr, bool passiveMode) {
    boost::optional<SessionDirection> sessionDirection;

    if (toString(Incoming).compareIgnoreCase(sessionDirectionStr) == 0) {
        sessionDirection = Incoming;
    } else if (toString(Outgoing).compareIgnoreCase(sessionDirectionStr) == 0) {
        sessionDirection = Outgoing;
    } else if (toString(SessionDirection_Unavailable).compareIgnoreCase(sessionDirectionStr) == 0) {
        sessionDirection = SessionDirection_Unavailable;
    }

    if (!sessionDirection.is_initialized()) {
        if (!passiveMode) {
            throw VException(VSTRING_FORMAT("Failed to convert '%s' to SessionDirection - Invalid enum value.", sessionDirectionStr.chars()));
        }

        sessionDirection = SessionDirection_Unavailable;
    }

    return *sessionDirection;
}

//Static 
TransactionDirection NetworkMonitor::convertToTransactionDirection(const VString& transactionDirectionStr, bool passiveMode) {
    boost::optional<TransactionDirection> transactionDirection;

    if (toString(Rx).compareIgnoreCase(transactionDirectionStr) == 0) {
        transactionDirection = Rx;
    } else if (toString(Tx).compareIgnoreCase(transactionDirectionStr) == 0) {
        transactionDirection = Tx;
    } else if (toString(TransactionDirection_Unavailable).compareIgnoreCase(transactionDirectionStr) == 0) {
        transactionDirection = TransactionDirection_Unavailable;
    }

    if (!transactionDirection.is_initialized()) {
        if (!passiveMode) {
            throw VException(VSTRING_FORMAT("Failed to convert '%s' to TransactionDirection - Invalid enum value.", transactionDirectionStr.chars()));
        }

        transactionDirection = TransactionDirection_Unavailable;
    }

    return *transactionDirection;
}

// Static
NetworkMonitorWeakPtr NetworkMonitor::getInstance() {
    LOCK_MUTEX(instanceMutex);

    // Better to init the static instance and use it everywhere. 
    // But, cannot avoid double-checked locking as we need to check if the instance was destroyed.
    if (!instanceCreated.is_initialized()) {
        instance = NetworkMonitorSharedPtr(new NetworkMonitor());   // Cannot use make_shared as NetworkMonitor::c'tor is private.

        instanceCreated = true;
    } else if (!(*instanceCreated)) {
        // Return an empty weak pointer instead?
        throw VException("NetworkMonitor singleton is destroyed!");
    }

    RELEASE_MUTEX(instanceMutex);

    return NetworkMonitorWeakPtr(instance);
}

// Static
void NetworkMonitor::destroyInstance() {
    LOCK_MUTEX(instanceMutex);

    if (instanceCreated.is_initialized() && *instanceCreated) {
        instance.reset();

        instanceCreated = false;
    }

    RELEASE_MUTEX(instanceMutex);
}
