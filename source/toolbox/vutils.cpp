#include "vutils.h"
#include "vexception.h"
#include "vhighresolutiontimehelper.h"
#include "stack_crawler.h"
#ifdef __linux__
#include <cxxabi.h>
#endif

//
// Class STLUtils
//
std::string STLUtils::CleanRawTypeName(const std::string& rawTypeName) {
    // TODO: Better to use regex. Currently, if the class name or struct name contains class or struct as part of the name itself, 
    // this routine would remove the text. 
    // For example: 'class MyUtilityclass' (note the case) would result in 'class MyUtility'.

    std::string typeName = rawTypeName;

#ifdef __linux__
    std::size_t len = 0;
    int status = 0;
    std::unique_ptr<char, decltype(&std::free)> ptr(__cxxabiv1::__cxa_demangle(typeName.c_str(), nullptr, &len, &status), &std::free);
    typeName = ptr.get();
#endif

    Replace(typeName, TOKEN_CLASS, TOKEN_EMPTY);

    Replace(typeName, TOKEN_STRUCT, TOKEN_EMPTY);

    Replace(typeName, TOKEN_ENUM, TOKEN_EMPTY);

    Replace(typeName, TOKEN_SPACE, TOKEN_EMPTY);

    return typeName;
}

bool STLUtils::Replace(std::string& str, const std::string& toReplace, const std::string& replaceWith) {
    if (str.length() == 0 || toReplace.length() == 0) {
        return false;
    }

    VSizeType startPosition = str.find(toReplace);

    bool found = false;

    while(startPosition != std::string::npos) {
        found |= true;

        str.replace(startPosition, toReplace.length(), replaceWith);

        startPosition = str.find(toReplace, (startPosition + replaceWith.length()));
    }

    return found;
}

const std::string STLUtils::TOKEN_CLASS     = "class ";
const std::string STLUtils::TOKEN_STRUCT    = "struct ";
const std::string STLUtils::TOKEN_ENUM      = "enum ";
const std::string STLUtils::TOKEN_SPACE     = " ";
const std::string STLUtils::TOKEN_EMPTY     = "";
const std::string STLUtils::TOKEN_NEWLINE   = "\r\n";

//
// Struct FreezeDumpInfo
//
FreezeDumpInfo::FreezeDumpInfo() : DumpCreated(false), DumpCreatorId(""), DumpCreationTime(0.0) {
}

FreezeDumpInfo::~FreezeDumpInfo() {
}

void FreezeDumpInfo::Reset() {
    this->DumpCreated = false;

    this->DumpCreatorId = "";

    this->DumpCreationTime = 0.0;

    this->DumpFileName = "";
}

//
// Class SlowThroughputMonitor
//

std::shared_ptr<SlowThroughputMonitor> SlowThroughputMonitor::SingletonInstance;
std::once_flag SlowThroughputMonitor::InstanceCreated;

SlowThroughputMonitor::SlowThroughputMonitor() : freezeDumpInfo(), throughputMap() {
}

SlowThroughputMonitor::~SlowThroughputMonitor() {
}

// static
SlowThroughputMonitor& SlowThroughputMonitor::Instance() {
    std::call_once(SlowThroughputMonitor::InstanceCreated, []() {
        SlowThroughputMonitor::SingletonInstance.reset(new SlowThroughputMonitor());
    });

    return *SlowThroughputMonitor::SingletonInstance;
}

bool SlowThroughputMonitor::MonitorThroughput(const std::string& componentId, VDouble processingRate) {
    if (!StackCrawl::isAutoDumpOnPossibleFreezeEnabled()) {
        return false;
    }
    const VDouble SECONDS_PER_MINUTE = 60.0;

    const VDouble ZERO_THROUGHPUT_TOLERANCE_IN_SECONDS = (StackCrawl::getZeroThroughputToleranceInMinutes() * SECONDS_PER_MINUTE);

    VDouble currentTime = VHighResolutionTimeHelper::GetTimeInSeconds();

    bool dumpCreated = false;

    std::lock_guard<std::mutex> lock(this->dataSynchronizer);

    auto  existingComponent = this->throughputMap.find(componentId);

    if (existingComponent == this->throughputMap.end()) {
        this->throughputMap[componentId] = boost::none;
    } 

    OptionalVDouble slowThroughputStartTime = this->throughputMap[componentId];

    if (!slowThroughputStartTime.is_initialized()) {
        // Seeing zero throughput. Start tracking...
        if (processingRate <= 0.0) {
            this->throughputMap[componentId] = currentTime;
        }
    } else if (processingRate > 0.0) {
        // We have recovered from zero throughput. 
        VDouble zeroThroughputPeriod = (currentTime - slowThroughputStartTime.value());

        VLOGGER_INFO(VSTRING_FORMAT("SlowThroughputMonitor::MonitorThroughput - Component %s seems to have recovered from 0%% througput after %u minutes.", componentId.c_str(),
                                            static_cast<Vu32>(zeroThroughputPeriod / SECONDS_PER_MINUTE)));

        this->throughputMap[componentId] = boost::none;

        ResetDumpCreationStatusIfPossible();
    } else if (!this->freezeDumpInfo.DumpCreated) {
        VDouble zeroThroughputPeriod = (currentTime - slowThroughputStartTime.value());

        VLOGGER_INFO(VSTRING_FORMAT("ZERO_THROUGHPUT %f", zeroThroughputPeriod));

        if (zeroThroughputPeriod >= ZERO_THROUGHPUT_TOLERANCE_IN_SECONDS) {
            // 0% throughput continues over the specified tolerance period. Create a memory dump.
            VString fileNamePrefix = VSTRING_FORMAT("%s_%s", StackCrawl::DEFAULT_FREEZE_DUMP_FILE_NAME_PREFIX.chars(), componentId.c_str());

            DumpType dumpType = StackCrawl::getCurrentCrashDumpType();

            VString dumpFile = StackCrawl::generateLiveDmp(dumpType, fileNamePrefix);

            VLOGGER_WARN(VSTRING_FORMAT("SlowThroughputMonitor::MonitorThroughput - POSSIBLE FREEZE!!! Component %s reported 0%% throughput for over %u minutes. Created %s dump: %s",
                componentId.c_str(), static_cast<Vu32>(zeroThroughputPeriod / SECONDS_PER_MINUTE), DumpTypeConverter::ToString(dumpType).c_str(), dumpFile.chars()));
            this->freezeDumpInfo.DumpCreated = true;

            this->freezeDumpInfo.DumpCreatorId = componentId;

            this->freezeDumpInfo.DumpCreationTime = VHighResolutionTimeHelper::GetTimeInSeconds();

            this->freezeDumpInfo.DumpFileName = std::string(dumpFile.chars());

            dumpCreated = true;
        }
    }

    return dumpCreated;
}

bool SlowThroughputMonitor::StopMonitoringComponent(const std::string& componentId) {
    bool removed = false;

    std::lock_guard<std::mutex> lock(this->dataSynchronizer);

    if (this->throughputMap.size() == 0) {
        return false;
    }

    auto  existingComponent = this->throughputMap.find(componentId);

    if (existingComponent != this->throughputMap.end()) {
        this->throughputMap.erase(existingComponent);

        ResetDumpCreationStatusIfPossible();

        removed = true;
    }

    return removed;
}

FreezeDumpInfo SlowThroughputMonitor::GetFreezeDumpInfo() {
    std::lock_guard<std::mutex> lock(this->dataSynchronizer);

    return this->freezeDumpInfo;
}

void SlowThroughputMonitor::ResetDumpCreationStatusIfPossible() {
    if (this->freezeDumpInfo.DumpCreated) {
        auto freezingComponent = std::find_if(this->throughputMap.begin(), this->throughputMap.end(), [](const std::pair<std::string, OptionalVDouble>& componentInfo) {
            return (componentInfo.second.is_initialized());
        });

        if (freezingComponent == this->throughputMap.end()) {
            VLOGGER_INFO("SlowThroughputMonitor::ResetDumpCreationStatusIfPossible - No component seems to suffer from 0% throughput currently. Resetting monitor to create dump on potential freeze...");

            this->freezeDumpInfo.Reset();
        }
    }
}

//
// Class TaskQueueDiagnostics
//
TaskQueueDiagnostics::TaskQueueDiagnostics(const std::string& loggerName, Vu16 logFrequencyInMinutes, Vu16 minimumProcessingRate, bool enableDiagnostics, bool enableFreezeMonitoring) :
                                                                                                    loggerName(loggerName),
                                                                                                    logFrequencyInMinutes(logFrequencyInMinutes), 
                                                                                                    diagnosticsStartTime(0.0),
                                                                                                    itemsQueuedSinceStartTime(0),
                                                                                                    itemsProcessedSinceStartTime(0),
                                                                                                    minimumProcessingRate(minimumProcessingRate),
                                                                                                    diagnosticsEnabled(enableDiagnostics),
                                                                                                    freezeMonitoringEnabled(enableFreezeMonitoring) {
}

TaskQueueDiagnostics::TaskQueueDiagnostics(const TaskQueueDiagnostics& inObj) {
    loggerName = inObj.loggerName;
    logFrequencyInMinutes = inObj.logFrequencyInMinutes;
    minimumProcessingRate = inObj.minimumProcessingRate;
    diagnosticsEnabled = inObj.diagnosticsEnabled.load();
    freezeMonitoringEnabled = inObj.freezeMonitoringEnabled;
    diagnosticsStartTime = inObj.diagnosticsStartTime;
    itemsQueuedSinceStartTime = inObj.itemsQueuedSinceStartTime;
    itemsProcessedSinceStartTime = inObj.itemsProcessedSinceStartTime.load();
}

TaskQueueDiagnostics::~TaskQueueDiagnostics() {
    if (this->freezeMonitoringEnabled) {
        SlowThroughputMonitor::Instance().StopMonitoringComponent(this->loggerName);
    }
}

std::string TaskQueueDiagnostics::LoggerName() {
    std::lock_guard<std::mutex> lock(this->updateLoggerNameMutex);

    return this->loggerName;
}

bool TaskQueueDiagnostics::Enabled() const {
    return this->diagnosticsEnabled;
}

void TaskQueueDiagnostics::Enable() {
    this->diagnosticsEnabled = true;
}

void TaskQueueDiagnostics::Disable() {
    this->diagnosticsEnabled = false;

    this->itemsQueuedSinceStartTime = 0;
    this->itemsProcessedSinceStartTime = 0;

    this->diagnosticsStartTime = 0.0;
}

bool TaskQueueDiagnostics::RegisterItemsQueued(size_t numberOfItemsInserted, size_t currentQueueSize, bool measureProcessingRate) {
    VDouble processingRate = 0.0;

    return RegisterItemsQueued(numberOfItemsInserted, currentQueueSize, measureProcessingRate, processingRate);
}

bool TaskQueueDiagnostics::RegisterItemsQueued(size_t numberOfItemsInserted, size_t currentQueueSize, bool measureProcessingRate, /*OUT*/ VDouble& processingRate) {
    if (!this->diagnosticsEnabled && !this->freezeMonitoringEnabled) { // 'freezeMonitoringEnabled' overrides 'diagnosticsEnabled'.
        return false;
    }

    auto result = false;

    if (this->diagnosticsStartTime == 0.0) {
        this->diagnosticsStartTime = VHighResolutionTimeHelper::GetTimeInNanoSeconds();
    } else if (this->diagnosticsStartTime > 0.0) {
        VDouble minutes = (VHighResolutionTimeHelper::ConvertNanosecondsToSeconds(VHighResolutionTimeHelper::GetTimeInNanoSeconds() - this->diagnosticsStartTime) / 60.0);

        if (minutes >= static_cast<VDouble>(this->logFrequencyInMinutes)) {
            VDouble queuesPerMinute = ((1.0 / minutes) * static_cast<VDouble>(this->itemsQueuedSinceStartTime));

            this->itemsQueuedSinceStartTime = 0;

            this->diagnosticsStartTime = VHighResolutionTimeHelper::GetTimeInNanoSeconds();

            if (measureProcessingRate || this->freezeMonitoringEnabled) {
                VDouble processingPerMinute = ((1.0 / minutes) * static_cast<VDouble>(this->itemsProcessedSinceStartTime));

                processingRate = ((static_cast<VDouble>(processingPerMinute) / queuesPerMinute) * 100.0);

                VLOGGER_INFO(VSTRING_FORMAT("[COMM] TaskQueueDiagnostics[%s]::RegisterItemsQueued - Queues/Minute: %f, Processing/minute: %f (%f%%), Current Queue Size: %llu", LoggerName().c_str(), queuesPerMinute, processingPerMinute, processingRate, currentQueueSize));

                if (processingRate <= this->minimumProcessingRate) {
                    VLOGGER_WARN(VSTRING_FORMAT("[COMM] TaskQueueDiagnostics[%s]::RegisterItemsQueued - WARNING - Slow Throughput!. Processing rate (%f%%) is slower (%f/min) than queue rate (%f/min). %llu", LoggerName().c_str(), processingRate, processingPerMinute, queuesPerMinute, this->itemsProcessedSinceStartTime.load()));
                }

                if (this->freezeMonitoringEnabled) {
                    SlowThroughputMonitor::Instance().MonitorThroughput(this->loggerName, processingRate);
                }
            } else {
                VLOGGER_INFO(VSTRING_FORMAT("[COMM] TaskQueueDiagnostics[%s]::RegisterItemsQueued - Queues/Minute: %f, Current Queue Size: %llu", LoggerName().c_str(), queuesPerMinute, currentQueueSize));
            }

            this->itemsProcessedSinceStartTime = 0;

            result = true;
        }
    }

    this->itemsQueuedSinceStartTime += numberOfItemsInserted;

    return result;
}

void TaskQueueDiagnostics::RegisterItemsProcessed(size_t itemsProcessed) {
    if (!this->diagnosticsEnabled) {
        return;
    }

    this->itemsProcessedSinceStartTime += itemsProcessed;
}

void TaskQueueDiagnostics::UpdateLoggerName(const std::string& inName) {
    std::lock_guard<std::mutex> lock(this->updateLoggerNameMutex);

    if (this->freezeMonitoringEnabled) {
        SlowThroughputMonitor::Instance().StopMonitoringComponent(this->loggerName);
    }

    this->loggerName = inName;
}

#ifdef VPLATFORM_WIN
// 
// Class WideCharacterToVStringConverter
// 

const int WideCharacterToVStringConverter::MAX_LENGTH_OF_SOURCE_STRING = ((4 * 1024) + 1); // 4KB.

WideCharacterToVStringConverter::WideCharacterToVStringConverter(VString& string, int requiredLength) :
destinationString(string), requiredLength(requiredLength) {
    wideCharBuffer = new WCHAR[requiredLength];
}

WideCharacterToVStringConverter::~WideCharacterToVStringConverter() {
    WideCharacterToVStringConverter::wideCharToVString(this->wideCharBuffer, this->destinationString);

    delete this->wideCharBuffer;
}

WideCharacterToVStringConverter::operator LPWSTR () {
    return this->wideCharBuffer;
}

void WideCharacterToVStringConverter::wideCharToVString(const LPWSTR sourceString, VString &destinationString) {
    destinationString = "";

    if (sourceString == NULL) {
        return;
    }

    int multiByteStringLength;

    size_t actualWideStringLength = ::wcslen(sourceString);

    if (actualWideStringLength >= MAX_LENGTH_OF_SOURCE_STRING) {
        throw VException(VSTRING_FORMAT("WideCharacterToVStringConverter::wideCharToVString - Length of the source string (%llu) is greater than the allowed value (%d).",
            actualWideStringLength, MAX_LENGTH_OF_SOURCE_STRING));
    }

    int wideStringLength = (int)actualWideStringLength;

    // The actual length could be different from 'requiredLength'. So, re-calculate.
    multiByteStringLength = ::WideCharToMultiByte(CP_ACP, 0, sourceString, wideStringLength, 0, 0, 0, 0);

    destinationString.preflight(multiByteStringLength);

    ::WideCharToMultiByte(CP_ACP, 0, sourceString, wideStringLength, destinationString.buffer(), multiByteStringLength, 0, 0);

    destinationString.postflight(multiByteStringLength);
}

WindowsUtils::WindowsUtils() {
}

WindowsUtils::~WindowsUtils() {
}

/**
Static
*/
VString WindowsUtils::GetMessageForError(DWORD error) {
    LPVOID lpMsgBuf;

    DWORD result = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                                    error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);

    std::string errorMessage = boost::str(boost::format{ "(%1%) [N/A]" } % error);

    if (result) {
        errorMessage = boost::str(boost::format{ "(%1%) %2%" } % error % ((char*)lpMsgBuf));

        ::LocalFree(lpMsgBuf);

        STLUtils::Replace(errorMessage, STLUtils::TOKEN_NEWLINE, STLUtils::TOKEN_EMPTY);
    }

    return VString(errorMessage.c_str());
}
#endif
