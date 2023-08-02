/*
Copyright (c) 2016. All rights reserved.
*/

#ifndef networkmonitor_h
#define networkmonitor_h

#include "vmutex.h"
#include "vmutexlocker.h"
#include <boost/container/vector.hpp>
#include <boost/container/map.hpp>

// Warning disabled: C4503 - decorated name length exceeded, name was truncated...
// Could not find a better way to localize this suppression.
#pragma warning(disable:4503)

#define LOCK_MUTEX(MutexInstance)   { VMutexLocker locker(&MutexInstance)
#define RELEASE_MUTEX(MutexInstance) }

/**
For Network Monitoring
*/

#define DECLARE_SHARED_PTR(Type)            typedef boost::shared_ptr<Type> Type##SharedPtr
#define DECLARE_WEAK_PTR(Type)              typedef boost::weak_ptr<Type> Type##WeakPtr

#define FORWARD_DECLARE_SHARED_PTR(Type)    class Type; DECLARE_SHARED_PTR(Type)
#define FORWARD_DECLARE_WRAK_PTR(Type)      class Type; DECLARE_WEAK_PTR(Type)

/**
enum SessionType

Defines the type of the session. Currently, we have the following types
Client Session
Query
Standard

*/
// TODO: Do the following when we move on from VC8.
// 1. Change to 'enum class'.
// 2. Use qualified names for enum values wherever they are used.
// 3. Change SessionType_Unavailable to Unavailable.
enum SessionType {
    SessionType_Unavailable = 0,
    ClientSession = 1,
    Query = 2,
    Standard = 3
};

/**
enum SessionDirection

Incoming - Current application is the server.
Outgoing - Current application is the client.

*/
// TODO: Do the following when we move on from VC8.
// 1. Change to 'enum class'.
// 2. Use qualified names for enum values wherever they are used.
// 3. Change SessionDirection_Unavailable to Unavailable.
enum SessionDirection {
    SessionDirection_Unavailable = 0,
    Incoming = 1,
    Outgoing = 2
};

/**
enum TransactionDirection

Rx - Incoming data.
Tx - Outgoing data.

*/
// TODO: Do the following when we move on from VC8.
// 1. Change to 'enum class'.
// 2. Use qualified names for enum values wherever they are used.
// 3. Change TransactionDirection_Unavailable to Unavailable.
enum TransactionDirection {
    TransactionDirection_Unavailable = 0,
    Rx = 1,
    Tx = 2
};

/**
class NetworkTransactionLog

Logs the transacted bytes for one Rx/Tx burst and the time it took for the transaction.

*/
// Doesn't have to be thread-safe.
class NetworkTransactionLog {
public:
    TransactionDirection getTransactionDirection() const;

    Vu64 getTransactedBytes() const;

    VDouble getTransactionTimeInNanoSeconds() const;

    bool isTransacting();

    void startTransaction();
    void completeTransaction(const Vu64& bytesTransacted);

    virtual ~NetworkTransactionLog();

protected:
    /**
    Creates an instance but doesn't start tracking the transaction.
    Useful if the transaction is being done in multiple iterations.
    */
    NetworkTransactionLog(const TransactionDirection& transactionDirection);

    /**
    Creates an instance and starts tracking immediately.
    */
    NetworkTransactionLog(const TransactionDirection& transactionDirection, const bool& startLogging);

private:
    void validateInitialization();

    TransactionDirection  _channel;

    Vu64                _transactedBytes;
    VDouble             _totalTransactionTimeInNS; // NS - Nano-seconds.

    VDouble             _transactionStartTimeInNS;

    bool                _isTransacting;
};

DECLARE_SHARED_PTR(NetworkTransactionLog);
DECLARE_WEAK_PTR(NetworkTransactionLog);

typedef boost::container::vector<NetworkTransactionLogSharedPtr>    TransactionLogsSharedPtrList;
typedef TransactionLogsSharedPtrList::const_iterator                TransactionLogsSharedPtrListIterator;

typedef boost::container::vector<NetworkTransactionLogWeakPtr>      TransactionLogsWeakPtrList;
typedef TransactionLogsWeakPtrList::const_iterator                  TransactionLogsWeakPtrListIterator;

/**
class NetworkRxTransactionLog
*/
class NetworkRxTransactionLog : public NetworkTransactionLog {
public:
    NetworkRxTransactionLog();
    NetworkRxTransactionLog(const bool& startLogging);

    ~NetworkRxTransactionLog();
};

DECLARE_SHARED_PTR(NetworkRxTransactionLog);
DECLARE_WEAK_PTR(NetworkRxTransactionLog);

/**
class NetworkTxTransactionLog
*/
class NetworkTxTransactionLog : public NetworkTransactionLog {
public:
    NetworkTxTransactionLog();
    NetworkTxTransactionLog(const bool& startLogging);

    ~NetworkTxTransactionLog();
};

DECLARE_SHARED_PTR(NetworkTxTransactionLog);
DECLARE_WEAK_PTR(NetworkTxTransactionLog);

typedef boost::container::vector<NetworkRxTransactionLogSharedPtr>  RxTransactionLogsSharedPtrList;
typedef boost::container::vector<NetworkTxTransactionLogSharedPtr>  TxTransactionLogsSharedPtrList;

typedef RxTransactionLogsSharedPtrList::const_iterator              RxTransactionLogsSharedPtrListIterator;
typedef RxTransactionLogsSharedPtrList::reverse_iterator            RxTransactionLogsSharedPtrListReverseIterator;
typedef TxTransactionLogsSharedPtrList::const_iterator              TxTransactionLogsSharedPtrListIterator;
typedef TxTransactionLogsSharedPtrList::reverse_iterator            TxTransactionLogsSharedPtrListReverseIterator;

typedef boost::container::vector<NetworkRxTransactionLogWeakPtr>    RxTransactionLogsWeakPtrList;
typedef boost::container::vector<NetworkTxTransactionLogWeakPtr>    TxTransactionLogsWeakPtrList;

typedef RxTransactionLogsWeakPtrList::const_iterator                RxTransactionLogsWeakPtrListIterator;
typedef RxTransactionLogsWeakPtrList::reverse_iterator              RxTransactionLogsWeakPtrListReverseIterator;
typedef TxTransactionLogsWeakPtrList::const_iterator                TxTransactionLogsWeakPtrListIterator;
typedef TxTransactionLogsWeakPtrList::reverse_iterator              TxTransactionLogsWeakPtrListReverseIterator;

/**
class NetworkSession

Identifies one socket session.
There could be multiple Rx/Tx transactions in a session.

The number of transactions retained in memory is limited by MAX_LOGS_HISTORY.
When the number of transactions exceeds MAX_LOGS_HISTORY, older transactions would be discarded.
NOTE: We don't purge all the transactions when the limit is reached. We only remove older logs and
the list size is maintained to be MAX_LOGS_HISTORY.
*/
class NetworkSession {
public:
    NetworkSession(const VString& sessionId, const SessionType& sessionType, const SessionDirection& sessionDirection,
        const VString& endPointAddres, const Vu16& endPointPort);

    ~NetworkSession();

    VString             getSessionId() const;
    SessionType         getSessionType() const;
    SessionDirection    getSessionDirection() const;
    VString             getEndPointAddress() const;
    Vu16                getEndPointPort() const;

    RxTransactionLogsWeakPtrList getRxTransactionLogs();
    TxTransactionLogsWeakPtrList getTxTransactionLogs();

    TransactionLogsWeakPtrList getTransactionLogs(const TransactionDirection& transactionDirection);

    void addRxTransactionLog(const NetworkRxTransactionLog& log);
    void addTxTransactionLog(const NetworkTxTransactionLog& log);

private:
    void validateInitialization();

    VString             _sessionId;
    SessionType         _sessionType;
    SessionDirection    _sessionDirection;
    VString             _endPointAddress;
    Vu16                _endPointPort;
    // Add more info related to the session, such as start time & end time - if needed.

    RxTransactionLogsSharedPtrList  _rxLogs;
    TxTransactionLogsSharedPtrList  _txLogs;

    VMutex                          _rxLogsMutex;
    VMutex                          _txLogsMutex;

public:
    static const Vu16               MAX_LOGS_HISTORY;
};

DECLARE_SHARED_PTR(NetworkSession);
DECLARE_WEAK_PTR(NetworkSession);

typedef boost::container::vector<NetworkSessionSharedPtr>   NetworkSessionsSharedPtrList;
typedef NetworkSessionsSharedPtrList::const_iterator        NetworkSessionsSharedPtrListIterator;
typedef NetworkSessionsSharedPtrList::reverse_iterator      NetworkSessionsSharedPtrListReverseIterator;
typedef boost::shared_ptr<NetworkSessionsSharedPtrList>     PointerToNetworkSessionsSharedPtrList;

typedef boost::container::vector<NetworkSessionWeakPtr>     NetworkSessionsWeakPtrList;
typedef NetworkSessionsWeakPtrList::const_iterator          NetworkSessionsWeakPtrListIterator;
typedef NetworkSessionsWeakPtrList::reverse_iterator        NetworkSessionsWeakPtrListReverseIterator;
typedef boost::shared_ptr<NetworkSessionsWeakPtrList>       PointerToNetworkSessionsWeakPtrList;

typedef boost::container::map<VString, PointerToNetworkSessionsSharedPtrList>   NetworkSessionsSharedPtrMap;
typedef boost::container::map<VString, NetworkSessionsWeakPtrList>              NetworkSessionsWeakPtrMap;

typedef boost::container::map<VString, PointerToNetworkSessionsSharedPtrList>::const_iterator   NetworkSessionsSharedPtrMapIterator;
typedef boost::container::map<VString, NetworkSessionsWeakPtrList>::const_iterator              NetworkSessionsWeakPtrMapIterator;

/**
class NetworkMonitor
*/
FORWARD_DECLARE_SHARED_PTR(NetworkMonitor);
DECLARE_WEAK_PTR(NetworkMonitor);

/**
class NetworkMonitor

A singleton that collects and manages the list of sessions for every client (end-point)

The number of sessions maintained for each client is limited by MAX_SESSIONS_HISTORY.

NOTE: Once the singleton instance is destroyed by calling NetworkMonitor::destroyInstance,
it cannot be re-created.
*/
class NetworkMonitor {
    // TODO: Harden singleton implementation
public:
    ~NetworkMonitor();

    NetworkSessionWeakPtr createNetworkSession(const VString& clientId, const VString& sessionId, const SessionType& sessionType, 
                                                const SessionDirection& sessionDirection, const VString& endPointAddres, 
                                                const Vu16& endPointPort);

    NetworkSessionsWeakPtrList getSessionsList();
    NetworkSessionsWeakPtrMap getSessionsMap();

    void clearClientLogs(const VString& clientId);
    void clearAllLogs();

    // Add more actions as needed.

    // Configuration
    static bool isCapturingNetworkStatistics();

    // Helpers
    static VString toString(const SessionType& sessionType);
    static VString toString(const SessionDirection& sessionDirection);
    static VString toString(const TransactionDirection& transactionDirection);

    static SessionType convertToSessionType(const VString& sessionTypeStr, bool passiveMode);
    static SessionDirection convertToSessionDirection(const VString& sessionDirectionStr, bool passiveMode);
    static TransactionDirection convertToTransactionDirection(const VString& transactionDirectionStr, bool passiveMode);

    // Instance management.
    // TODO: For C++ 11 - Use call_once to simplify instance creation.
    static NetworkMonitorWeakPtr getInstance();
    static void destroyInstance();

private:

    NetworkMonitor();

    // Copying and assignment are disabled by poisoning the copy c'tor and the assignment operator.
    NetworkMonitor(NetworkMonitor& other);
    NetworkMonitor& operator= (NetworkMonitor& other);

public:
    static const Vu16               MAX_SESSIONS_HISTORY;

private:
    NetworkSessionsSharedPtrMap     _sessions;

    VMutex                          _sessionsMapMutex;

    static boost::optional<bool>    instanceCreated;

    static NetworkMonitorSharedPtr  instance;

    static VMutex                   instanceMutex;
};

#endif