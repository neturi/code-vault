#ifndef vutils_h
#define vutils_h

#include "vtypes.h"
#include "vstring.h"

/**
A utility class containing helpers to deal with STL objects.
*/
class STLUtils {
public:
    STLUtils() = delete;

    STLUtils(const STLUtils& other) = delete;

    STLUtils& operator=(const STLUtils& other) = delete;

    /**
    Cleans the type name (or Id) obtained through RTTI APIs.
    For now, it removes the 'class' prefix and removes spaces.

    rawTypeName - The string that needs to be cleaned up.
    */
    static std::string CleanRawTypeName(const std::string& rawTypeName);

    /**
    Returns a new string with all the occurrences of a specified string in specified string instance replaced with another specified string.

    str - 
    The source string. This string is not modified.
    
    toReplace - 
    The string to search for within 'str'

    replaceWith - 
    String to replace all occurences of 'toReplace' with.
    */
    static bool Replace(std::string& str, const std::string& toReplace, const std::string& replaceWith);

public:
    static const std::string TOKEN_CLASS;
    static const std::string TOKEN_STRUCT;
    static const std::string TOKEN_ENUM;
    static const std::string TOKEN_SPACE;
    static const std::string TOKEN_EMPTY;
    static const std::string TOKEN_NEWLINE;
};

/**
Stores the information regarding the auto-dump on freeze.
*/
struct FreezeDumpInfo {
public:
    FreezeDumpInfo();
    ~FreezeDumpInfo();

    /**
    Resets the state to 'no dump created'.
    */
    void Reset();

public:
    bool        DumpCreated;        // 'true' if an auto-dump was created. Default is 'false'.
    std::string DumpCreatorId;      // The 'freezing' component that triggered the auto-dump creation.
    VDouble     DumpCreationTime;   // The time the dump was created.
    std::string DumpFileName;       // Full path of the dump file.
};

using ComponentsThroughputMap = std::map<std::string, OptionalVDouble>;

/**
A simple monitor that observes the reporting components for freezes/hangs.

Any component that reports 0% throughput for over 'N' minutes (tolerance period) will be considered as a candidate for potential freeze.
In such an event, SlowThroughputMonitor will generate a dump file automatically. The type of the dump file (tiny/full) 
will be determined by the type that is set as the application's current dump type.

Once an auto-dump is created, SlowThroughputMonitor will avoid creating dumps further - even if components continue to report 0% 
throughput. But, if all of the components recover from the freeze (somehow), SlowThroughputMonitor will reset itself to create 
an auto-dump in case a freeze occurs again. 

This component activates and deactivates monitoring depending on the following global method.
StackCrawl::isAutoDumpOnPossibleFreezeEnabled()
So, it is possible - through telnet commands - to start & stop monitoring at any point during the application's lifetime. 
See MemoryDumpCommandHandler class for more details on this feature.

The dump type and the tolerance period are obtained from the following global methods in StackCrawl namepsace.
StackCrawl::getCurrentCrashDumpType()
StackCrawl::getZeroThroughputToleranceInMinutes()
*/
class SlowThroughputMonitor : public std::enable_shared_from_this<SlowThroughputMonitor> {
public:
    SlowThroughputMonitor(const SlowThroughputMonitor& other) = delete;

    SlowThroughputMonitor& operator = (const SlowThroughputMonitor& other) = delete;

    ~SlowThroughputMonitor();

    /**
    Returns the singleton instance of the SlowThroughputMonitor.
    */
    static SlowThroughputMonitor& Instance();

    /**
    Components report their throughput rate through this method. The throughput should be 0% or below for SlowThroughputMonitor
    to start monitoring the component for potential freeze.

    If monitoring is disabled, this method will not have any effect. That is, the component will not be monitored for freeze.

    NOTE: How the processing rate or throughput rate is calculated is entirely left to the reporting component. So is the frequency 
    of the reports.

    componentId - 
    The unique ID of the component reporting the throughput rate.

    processingRate - 
    The current throughput rate of the reporting component. 

    Returns 'true' if SlowThroughputMonitor created an auto-dump file on detection of potential freeze.
    Otherwise, returns 'false'.
    */
    bool MonitorThroughput(const std::string& componentId, VDouble processingRate);

    /**
    Stops monitoring a component for 0% throughput. This method could be called even if monitoring is disabled.

    componentId - 
    The unique ID of the component that had previously registered for monitoring.

    Returns 'true' if the component is unregistered successfully. 
    Returns 'false' if the component was not registered.
    */
    bool StopMonitoringComponent(const std::string& componentId);

    /**
    Returns the current status of dump-creation. 
    */
    FreezeDumpInfo GetFreezeDumpInfo();

private:
    SlowThroughputMonitor();

    void ResetDumpCreationStatusIfPossible();

private:
    FreezeDumpInfo                                  freezeDumpInfo;

    ComponentsThroughputMap                         throughputMap;

    std::mutex                                      dataSynchronizer;

    std::once_flag                                  configurationInitialized;

    static std::shared_ptr<SlowThroughputMonitor>   SingletonInstance;
    static std::once_flag                           InstanceCreated;
};

/**
A very simple utility class to track and log diagnostic information related to task queues.

Doesn't have the overhead of timers or threads. The diagnostic information is printed out only when a queueing activity 
happens. Idea is, if the activity doesn't happen very frequently, it would mean the system/component is not under load and so, 
diagnostics is generally not needed.
*/
class TaskQueueDiagnostics {
public:
    /**
    C'tor

    loggerName - 
    The prefix that will used in the log statements. This also indicates the source (Class::Method) of the logs.

    logFrequencyInMinutes - 
    The frequency - in minutes - at which the diagnostic information will be logged.

    minimumProcessingRate - 
    The threshold that's set as the preferred minimum rate at which items are processed per specified 'logFrequencyInMinutes'. 
    This value is specified as percentage.
    For example, 'logFrequencyInMinutes' is set to '1'. 'minimumProcessingRate' is set to 25%, the current queues/minute is 100 and current 
    items processed per minute is 23, a warning would be logged (items processed per minute is 23% - less than preferred threshold of 25%).

    enableDiagnostics - 
    Enables the diagnostics - if set to 'true'. If set to 'false', the diagnostics is disabled.
    */
    TaskQueueDiagnostics(const std::string& loggerName, Vu16 logFrequencyInMinutes, Vu16 minimumProcessingRate, bool enableDiagnostics, bool enableFreezeMonitoring);

    TaskQueueDiagnostics(const TaskQueueDiagnostics&);

    ~TaskQueueDiagnostics();

    std::string LoggerName();

    /**
    Returns 'true' if the diagnosis is enabled. Otherwise, returns 'false'.
    If the diagnosis is not enabled, no tracking/logging is done.
    */
    bool Enabled() const;

    /**
    Enables the diagnosis.
    */
    void Enable();

    /**
    Disables the diagnosis.
    */
    void Disable();

    /**
    Registers a queueing activity.

    Returns 'true' if one minute has passed since the last queues/minute calculation and the queues/minute is calculated.
    Returns 'false' if one minute hasn't passed since the last calculation.

    numberOfItemsInserted - 
    The total number of items inserted into the queue during this activity.
    
    currentQueueSize - 
    The size of the queue after inserting the items (i.e., size including 'numberOfItemsInserted').

    measureProcessingRate - 
    Set to 'true' if the diagnostics should include the information on items-processed/minute. Set to 'false' otherwise.
    If set to 'false', the parameters 'itemsProcessed' and 'minimumProcessingRate' are ignored.
    */
    bool RegisterItemsQueued(size_t numberOfItemsInserted, size_t currentQueueSize, bool measureProcessingRate);


    /**
    See documentation for the overload (above).

    processingRate -
    This is an out parameter. When the function returns, this parameter will hold the current processing rate.
    */
    bool RegisterItemsQueued(size_t numberOfItemsInserted, size_t currentQueueSize, bool measureProcessingRate, /*OUT*/ VDouble& processingRate);

    /**
    Registers the number of items processed.

    itemsProcessed - 
    Items processed 'since last registration'.
    */
    void RegisterItemsProcessed(size_t itemsProcessed);

    /**
    Updates the logger name. This is required as some components change their names based on their states (example: VClientSession).

    loggerName - The new logger name.
    */
    void UpdateLoggerName(const std::string& inName);

private:
    std::string         loggerName;
    Vu16                logFrequencyInMinutes;

    VDouble             diagnosticsStartTime;
    size_t              itemsQueuedSinceStartTime;
    std::atomic<size_t> itemsProcessedSinceStartTime;
    Vu16                minimumProcessingRate;

    AtomicBoolean       diagnosticsEnabled;

    bool                freezeMonitoringEnabled;

    std::mutex          updateLoggerNameMutex;
};

#ifdef VPLATFORM_WIN
/**
A simple utility class to convert WSTR to VString.
*/
class WideCharacterToVStringConverter {
public:
    WideCharacterToVStringConverter(VString& string, int requiredLength);

    ~WideCharacterToVStringConverter();

    operator LPWSTR ();

    // Specifies the maximum allowed length of the source (input) string.
    static const int MAX_LENGTH_OF_SOURCE_STRING;

private:
    // Discourage copying and assignment by poisoning the copy c'tor and assignment operator.
    WideCharacterToVStringConverter(WideCharacterToVStringConverter& other);
    WideCharacterToVStringConverter& operator= (WideCharacterToVStringConverter& other);

    static void wideCharToVString(const LPWSTR sourceString, VString &destinationString);

    VString &destinationString;
    WCHAR *wideCharBuffer;
    int requiredLength;
};

class WindowsUtils {
public:
    WindowsUtils(const WindowsUtils& other) = delete;

    WindowsUtils& operator = (const WindowsUtils& other) = delete;

    ~WindowsUtils();

    static VString GetMessageForError(DWORD error);

private:
    WindowsUtils();
};
#endif // VPLATFORM_WIN

#endif