/*
 * Copyright (c) 1989-2007 Navis LLC. All rights reserved.
 * $Id: stack_crawler.cpp,v 1.21 2008-11-05 19:21:54 isaacson Exp $
 */

#include "VStackCrawler.h"
#include "stack_crawler.h"
#ifdef VPLATFORM_WIN
#include <Psapi.h>
#endif
#include "vmutexlocker.h"
#include "vsocket.h"
#include "vexception.h"
#include "vthread.h"
#include "vutils.h"

#include <boost/algorithm/string/predicate.hpp>

/*
Some of this functionality is only available (and frankly, needed) on Windows.
This includes the installation of system exception handlers that catch crashes
and try to walk the stack and print diagnostic information. On other platforms
we may only be able to walk the stack (if that) but the system will typically
produce a detailed crash log automatically so that we don't need to do all this
work in the first place.

Regarding 64-bit Windows. Different system APIs/structures (e.g., register data)
are required. Work in progress. See http://www.howzatt.demon.co.uk/articles/DebuggingInWin64.html
for possible stack walking example.
*/

const VString   StackCrawl::DEFAULT_TINY_DUMP_FILE_NAME_PREFIX              = "XPS-Crash-Dump-Tiny";
const VString   StackCrawl::DEFAULT_FULL_DUMP_FILE_NAME_PREFIX              = "XPS-Crash-Dump-Full";
const VString   StackCrawl::DEFAULT_ON_DEMAND_DUMP_FILE_NAME_PREFIX         = "XPS-On-Demand-Dump";

#ifdef VAULT_STACK_CRAWLING_FOR_SERVER
const DumpType  StackCrawl::DEFAULT_DUMP_TYPE                               = DumpType::Full; // Full dump is default for XPS Server.
#else
const DumpType  StackCrawl::DEFAULT_DUMP_TYPE                               = DumpType::Full; // Full dump is default for XPS Client.
#endif

const VString   StackCrawl::DEFAULT_DUMP_TYPE_AS_STRING                     = VString(DumpTypeConverter::ToString(StackCrawl::DEFAULT_DUMP_TYPE).c_str());

const VString   StackCrawl::DEFAULT_FREEZE_DUMP_FILE_NAME_PREFIX            = "XPS-Possible-Freeze";
#ifdef VAULT_STACK_CRAWLING_FOR_SERVER
const bool      StackCrawl::DEFAULT_AUTO_DUMP_ON_POSSIBLE_FREEZE_ENABLED    = true; 
#else
const bool      StackCrawl::DEFAULT_AUTO_DUMP_ON_POSSIBLE_FREEZE_ENABLED    = false; // XPS Client does not support freeze monitoring - at least, not yet.
#endif
const Vu16      StackCrawl::DEFAULT_ZERO_THROUGHPUT_TOLERANCE_IN_MINUTES    = 3;
const Vu16      StackCrawl::MINIMUM_ZERO_THROUGHPUT_MINUTES                 = 1;
const Vu16      StackCrawl::MAXIMUM_ZERO_THROUGHPUT_MINUTES                 = 30;

static bool     gGenerateStdlog     = true; // If true, when we crash we call our code that appends info to STDLOG.TXT.
static DumpType gDumpTypeConfigured = StackCrawl::DEFAULT_DUMP_TYPE;
static DumpType gDumpTypeToGenerate = gDumpTypeConfigured;
static VString  gStdlogViewerApp("NOTEPAD"); // If not empty, after stdlog is written, WinExec is called with this app.
static VString  gCurrentScriptCommand;
static int      gCurrentScriptLineNumber = 0;

// Following globals are not thread-safe.
VString         gFreezeDumpFileNamePrefix           = StackCrawl::DEFAULT_FREEZE_DUMP_FILE_NAME_PREFIX;
AtomicBoolean   gAutoDumpOnPossibleFreezeEnabled	= { StackCrawl::DEFAULT_AUTO_DUMP_ON_POSSIBLE_FREEZE_ENABLED };
Vu16            gZeroThroughputToleranceInMinutes   = StackCrawl::DEFAULT_ZERO_THROUGHPUT_TOLERANCE_IN_MINUTES;

// This namespace defines the public API for managing the crash handling / stack crawl facility.
namespace StackCrawl {
    void configureCrashHandler(bool generateStdlog, DumpType dumpTypeToGenerate, const VString& stdlogViewerApp) {
        gGenerateStdlog = generateStdlog;
        gDumpTypeConfigured = dumpTypeToGenerate;
        gDumpTypeToGenerate = dumpTypeToGenerate;

        VLOGGER_INFO(VSTRING_FORMAT("Configured Dump-Type: %d, %s", dumpTypeToGenerate, DumpTypeConverter::ToString(dumpTypeToGenerate).c_str()));
        gStdlogViewerApp = stdlogViewerApp;
    }

    const VString& getStdlogViewerApp() {
        return gStdlogViewerApp;
    }

    extern DumpType getCurrentCrashDumpType() {
        return gDumpTypeToGenerate;
    }

    extern DumpType getDefaultCrashDumpType() {
        return gDumpTypeConfigured;
    }

    extern DumpType setCrashDumpType(DumpType dumpType) {
        DumpType previousValue = gDumpTypeToGenerate;

        gDumpTypeToGenerate = dumpType;

        return previousValue;
    }

    void setScriptCommandAndLineNumberForStackCrawl(const VString& inCommand, int inLine) {
        gCurrentScriptCommand = inCommand;
        gCurrentScriptLineNumber = inLine;
    }

    void configureAutoDumpOnPossibleFreeze(bool autoDumpOnPossibleFreezeEnabled, Vu16 zeroThroughputToleranceInMinutes, const VString& freezeDumpFileNamePrefix) {
        gAutoDumpOnPossibleFreezeEnabled = autoDumpOnPossibleFreezeEnabled;

        gZeroThroughputToleranceInMinutes = zeroThroughputToleranceInMinutes;

        VString fileNamePrefix = freezeDumpFileNamePrefix;

        fileNamePrefix.trim();

        if (!fileNamePrefix.isNotEmpty()) {
            gFreezeDumpFileNamePrefix = fileNamePrefix;
        }
    }

    bool isAutoDumpOnPossibleFreezeEnabled() {
        return gAutoDumpOnPossibleFreezeEnabled;
    }

    Vu16 getZeroThroughputToleranceInMinutes() {
        return gZeroThroughputToleranceInMinutes;
    }

    VString getFreezeDumpFileNamePrefix() {
        return gFreezeDumpFileNamePrefix;
    }

    void enableOrDisableAutoDumpOnPossibleFreeze(bool enable) {
        gAutoDumpOnPossibleFreezeEnabled = enable;
    }
} // namespace StackCrawl

//
// Class DumpTypeConverter
//
int DumpTypeConverter::ToInteger(DumpType dumpType) {
    return static_cast<int>(dumpType);
}

DumpType DumpTypeConverter::FromInteger(int dumpTypeAsInteger) {
    DumpType dumpType = DumpType::None;

    switch (dumpTypeAsInteger) {
    case 0:
        dumpType = DumpType::None;
        break;

    case 1:
        dumpType = DumpType::Tiny;
        break;

    case 2:
        dumpType = DumpType::Full;
        break;

    default:
        throw VException(VSTRING_FORMAT("No conversion exists for the specified integer value: %d", dumpTypeAsInteger));
    }

    return dumpType;
}

std::string DumpTypeConverter::ToString(DumpType dumpType) {
    std::string dumpTypeAsString = "";

    switch (dumpType) {
    case DumpType::None:
        dumpTypeAsString = "None";
        break;

    case DumpType::Tiny:
        dumpTypeAsString = "Tiny";
        break;

    case DumpType::Full:
        dumpTypeAsString = "Full";
        break;

    default:
        throw VException(VSTRING_FORMAT("No conversion exists for the specified dump type: %d", static_cast<int>(dumpType)));
    }

    return dumpTypeAsString;
}

DumpType DumpTypeConverter::FromString(const std::string& dumpTypeAsString) {
    std::string cleanDumpTypeAsString = dumpTypeAsString;

    STLUtils::Replace(cleanDumpTypeAsString, STLUtils::TOKEN_SPACE, STLUtils::TOKEN_EMPTY);

    DumpType dumpType = DumpType::None;

    if (boost::iequals(cleanDumpTypeAsString, DumpTypeConverter::ToString(DumpType::None))) {
        dumpType = DumpType::None;
    }
    else if (boost::iequals(cleanDumpTypeAsString, DumpTypeConverter::ToString(DumpType::Tiny))) {
        dumpType = DumpType::Tiny;
    }
    else if (boost::iequals(cleanDumpTypeAsString, DumpTypeConverter::ToString(DumpType::Full))) {
        dumpType = DumpType::Full;
    }
    else {
        throw VException(VSTRING_FORMAT("Failed to convert string '%s' to 'DumpType'", dumpTypeAsString.c_str()));
    }

    return dumpType;
}

static VInstant gStartTime; // Initialized to the current time at startup.

static const VString LOG_FILE_NAME("STDLOG.TXT");

// allcate some memory for the SEH handler
void* gSEHEmergencyMemory = 0;

Vs64 gWorkingSetSizeAtStartup = 0;

VString gPrefixForTinyDumpFileName = StackCrawl::DEFAULT_TINY_DUMP_FILE_NAME_PREFIX;

VString gPrefixForFullDumpFileName = StackCrawl::DEFAULT_FULL_DUMP_FILE_NAME_PREFIX;

#ifdef VPLATFORM_WIN
// BEGIN WINDOWS-SPECIFIC IMPLEMENTATION -----------------------------------------------
/*
This section is the Windows implementation of the APIs. Here we do all the work of
being able to install system exception handlers and manually walk the stack.
*/
static bool alreadyHandlingOutOfMemory = false;

typedef std::map<DWORD, StackCrawlThreadInfo*> StackCrawlThreadInfoMap;
static StackCrawlThreadInfoMap gStackCrawlThreadInfoMap;
static VMutex gStackCrawlThreadInfoMapMutex("gStackCrawlThreadInfoMapMutex"); // 2011.04.28 tisaacson ARGO-31680 - Make access to map thread-safe.

static void _registerThreadInfo(DWORD threadId, StackCrawlThreadInfo* info) {
    VMutexLocker locker(&gStackCrawlThreadInfoMapMutex, "_registerThreadInfo");

    // Complain if caller is attempting to register a thread that's already registered.
    if (gStackCrawlThreadInfoMap.find(threadId) != gStackCrawlThreadInfoMap.end()) {
        VLOGGER_ERROR(VSTRING_FORMAT("_registerThreadInfo: threadId %lu is already registered.", threadId));
        return;
    }

    gStackCrawlThreadInfoMap[threadId] = info;
}

static StackCrawlThreadInfo* const _lookupThreadInfo(DWORD threadId) {
    VMutexLocker locker(&gStackCrawlThreadInfoMapMutex, "_lookupThreadInfo");

    StackCrawlThreadInfo* info = NULL;

    auto iter = gStackCrawlThreadInfoMap.find(threadId);

    if (iter != gStackCrawlThreadInfoMap.end()) {
        info = iter->second;
    }

    return info;
}

void _deregisterThreadInfo(DWORD threadId) {
    VMutexLocker locker(&gStackCrawlThreadInfoMapMutex, "_deregisterThreadInfo");
    vault::mapDeleteOneValue(gStackCrawlThreadInfoMap, threadId);
}

//--------------------------------------------------------------------------------

StackCrawlThreadInfo::StackCrawlThreadInfo(DWORD startingStackFrame) :
    fStartingStackFrame(startingStackFrame),
    fIsInExceptionHandler(false),
    fHasSymbols(false) {
}

StackCrawlThreadInfo::~StackCrawlThreadInfo() {
}

DWORD StackCrawlThreadInfo::getStartingStackFrame() const {
    return fStartingStackFrame;
}

bool StackCrawlThreadInfo::isInExceptionHandler() const {
    return fIsInExceptionHandler;
}

void StackCrawlThreadInfo::enteringExceptionHandler() {
    fIsInExceptionHandler = true;
}

void StackCrawlThreadInfo::leavingExceptionHandler() {
    fIsInExceptionHandler = false;
}

bool StackCrawlThreadInfo::hasSymbols() const {
    return fHasSymbols;
}

void StackCrawlThreadInfo::setHasSymbols(bool state) {
    fHasSymbols = state;
}

//--------------------------------------------------------------------------------

static const DWORD INVALID_VARIABLE = 0xcccccccc;
static const int METHOD_NAME_LENGTH = 256;

//---------------------------------------------------------------------

#if (defined(_M_IX86) || defined(_AMD64_) || defined(_M_AMD64))
    #define ENABLE_WINDOWS_STACK_CRAWLER
#else
    #warning "STACK CRAWLER IS DISABLED!!! Stack Crawler is not supported for the targeted platform."
#endif

#ifdef ENABLE_WINDOWS_STACK_CRAWLER
static bool init_dbg_symbols_ok = StackCrawlerEx::InitDbgSymbols();

struct SymInfo {
    IMAGEHLP_SYMBOL symbolInfo;
    CHAR symbolName[METHOD_NAME_LENGTH];
};

const VString StackCrawlerParameterInfo::UNKNOWN_TYPE_OR_VALUE = VString("[N/A]");

EnumerateSymbolsCallbackParam::EnumerateSymbolsCallbackParam(int functionId, StackCrawlerEx* instance, const STACKFRAME64& stackFrame, 
                                ParameterInfoListPtr parameters) : FunctionId(functionId), StackCrawlerInstance(instance), 
                                                                    StackFrame(stackFrame), Parameters(parameters) {
}

template <typename Type>
struct SimpleTypeResolver {
public:
    bool operator()(void* pData, bool isPointer, enum SymTagEnum tag, StackCrawlerParameterInfo& typeInfo) {
        std::stringstream stringStream;

        if (tag == SymTagEnum) {
#ifdef VCOMPILER_64BIT
            // TODO: In x64, retrieving parameters is not as straight forward as in x86. Some parameters are
            // stored in registers and others in stack. Need more work on this.
            stringStream << StackCrawlerParameterInfo::UNKNOWN_TYPE_OR_VALUE;
#else
            // TODO: Needs more work.
            // SMELL: Find the size of the enum and get it's value using ::SymGetTypeInfo.
            stringStream << *(int*)pData << " (?)";
#endif
        } else {
            if (isPointer || std::is_pointer<Type>::value) { // Handle pointer types. We just print the address.
                ULONG64 address;

#ifdef VCOMPILER_64BIT
                address = *(unsigned __int64*)pData;
#else
                address = *(unsigned __int32*)pData;
#endif
                stringStream << boost::format(StackCrawlerEx::POINTER_FORMATTER) % address;
            } else { 
#ifdef VCOMPILER_64BIT
                // TODO: In x64, retrieving parameters is not as straight forward as in x86. Some parameters are
                // stored in registers and others in stack. Need more work on this.
                stringStream << StackCrawlerParameterInfo::UNKNOWN_TYPE_OR_VALUE;
#else
                // Handle special cases like bool, wchar_t etc. as well as the other primitive types.
                boost::typeindex::type_index typeOfType = boost::typeindex::type_id<Type>();

                if (typeOfType == boost::typeindex::type_id<bool>()) {
                    stringStream << ((*(bool*)pData) ? "true" : "false");
                } else if (typeOfType == boost::typeindex::type_id<wchar_t>()) {
                    stringStream << (wchar_t)*(__int16*)pData;
                } else {
                    stringStream << (Type)*(Type*)pData;
                }
#endif
            }
        }

        typeInfo.TypeName = boost::typeindex::type_id<Type>().pretty_name();
        typeInfo.ValueAsString = stringStream.str();

        return true;
    }
};

//
// Struct StackCrawlerExSettings
//
bool StackCrawlerExSettings::CrashDumpEnabled() const {
    return (this->DumpTypeToGenerate != DumpType::None);
}

std::string StackCrawlerExSettings::GetDumpFileNamePrefix() const {
    std::string dumpFileName = "";

    if (this->DumpTypeToGenerate == DumpType::Tiny) {
        dumpFileName = this->TinyDumpFileNamePrefix;
    } else if (this->DumpTypeToGenerate == DumpType::Full) {
        dumpFileName = this->FullDumpFileNamePrefix;
    }

    return dumpFileName;
}

// 
// Class StackCrawlerEx
//
#ifdef VCOMPILER_64BIT
const VString StackCrawlerEx::POINTER_FORMATTER = VString("0x%016X");
#else
const VString StackCrawlerEx::POINTER_FORMATTER = VString("0x%08X        ");
#endif

StackCrawlerEx::StackCrawlerEx(const StackCrawlerExSettings& settings) : _settings(settings) {
}

StackCrawlerEx::~StackCrawlerEx() {
}

/*
* Initialize Debugger Symbols - call windows to load the pdb files
* This gets called at startup to avoid problems in the SEH handler
*/
/*static*/
bool StackCrawlerEx::InitDbgSymbols() {
    VString homePath;

    VFSNode::getExecutableDirectory().getPath(homePath);
    homePath += ";C:/Windows/System32;C:/Windows/SysWOW64";     // add "old" search locations for installer ease

    char homePathChars[2048];

    homePath.copyToBuffer(homePathChars, 2048);

    int sym_init = ::SymInitialize(GetCurrentProcess(), homePathChars, true);

    static int sym_init_error = 0;

    if (!sym_init) {
        sym_init_error = GetLastError();
    }

    return (sym_init != 0);
}

/*static*/
void StackCrawlerEx::CleanupDbgSymbols() {
    ::SymCleanup(GetCurrentProcess());
}

#pragma warning(push)
#pragma warning(disable : 4996)
/*static*/
HANDLE StackCrawlerEx::GetOSVersionInfo(OSVERSIONINFOEX& osvi) {
    //Get operating system info...
    //for more details about the osvi stuff below, see:
    //http://msdn2.microsoft.com/en-us/library/ms724429.aspx
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    if (!GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&osvi))) {
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        if (!GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&osvi))) {
            //_write(logger, "Unable to get Windows version");
        }
    }

    if (osvi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
        return NULL;
    else // NT and later
        return GetCurrentProcess();
}
#pragma warning(pop)

// This function should be used for anything we want to write to both the console
// and as a FATAL level notification as we are processing a crash.
/*static*/
void StackCrawlerEx::LogFatal(const VString& message) {
    std::cout << message << std::endl;
    VLOGGER_FATAL(message);
}

StackCrawlerExSettings StackCrawlerEx::Settings() const {
    return this->_settings;
}

StackCrawlerExSettings StackCrawlerEx::WriteCrashDumpInfo(const VString& headerMessage) {
    VString localTime;
    
    VInstant().getLocalString(localTime);

    VNamedLoggerPtr logger = _settings.Logger;

    logger->emitStackCrawlLine(VSTRING_FORMAT("%s BEGIN OUTPUT ------------------------------------------------------------------", localTime.chars()));

    if (headerMessage.isNotEmpty()) {
        logger->emitStackCrawlLine(headerMessage);
    }

    // To lessen the possibility that logging itself will fail and prevent the minidump,
    // we just collect log output as we go, then write the minidump, and only then do we
    // log the collected log output.
    if (_settings.DumpTypeToGenerate != DumpType::None) {
        _settings.Result = _writeMiniDumpToNewFile();

        if (_settings.IsOnDemand) {
            VInstant().getLocalString(localTime);

            logger->emitStackCrawlLine(VSTRING_FORMAT("%s Created on-demand crash dump:", localTime.chars()));

            logger->emitStackCrawlLine(_settings.Result.DumpFileName);
        }
    }

    if (_settings.DisplayProlog) {
        _writeCrashDumpProlog(_settings.Result);
    }

    if (_settings.DisplayMachineInfo) {
        _writeCrashDumpMachineInfo();
    }

#ifdef ENABLE_WINDOWS_STACK_CRAWLER
    if (_settings.DisplayRegisters) {
        _writeCrashDumpRegisters();
    }

    if (_settings.DisplayStackFrames) {
        _writeCrashDumpStackFrames();
    }
#else
    static const std::string CRAWLER_UNAVAILABLE_ERROR_MESSAGE("Unavailable as stack crawler is disabled for the target platform");

    if (_settings.DisplayRegisters) {
        logger->emitStackCrawlLine((std::string("REGISTERS: ") + CRAWLER_UNAVAILABLE_ERROR_MESSAGE).c_str());
    }

    if (_settings.DisplayStackFrames) {
        logger->emitStackCrawlLine((std::string("SYMBOLS: ") + CRAWLER_UNAVAILABLE_ERROR_MESSAGE).c_str());
        logger->emitStackCrawlLine((std::string("CALL STACK: ") + CRAWLER_UNAVAILABLE_ERROR_MESSAGE).c_str());
        logger->emitStackCrawlLine((std::string("PARAMETERS: ") + CRAWLER_UNAVAILABLE_ERROR_MESSAGE).c_str());
    }
#endif
    VInstant().getLocalString(localTime);
    logger->emitStackCrawlLine("");
    logger->emitStackCrawlLine(VSTRING_FORMAT("%s END OUTPUT ------------------------------------------------------------------", localTime.chars()));
    logger->emitStackCrawlLine("");

    return _settings;
}

void StackCrawlerEx::_writeCrashDumpProlog(DumpCreationResult dumpCreationResult) {
    VNamedLoggerPtr logger = _settings.Logger;

    ApplicationInfo appInfo = GetApplicationInfo();

    // Tell the customer what steps to take.
    logger->emitStackCrawlLine("");
    // 2011.05.02 JHR ARGO-31686 Zebra ESG should be removed from xps stdlog
    logger->emitStackCrawlLine(VSTRING_FORMAT("%s crashed and produced this crash dump information file. Please submit a case through your Navis LLC support contact.", appInfo.ApplicationName.c_str()));
    if (_settings.CrashDumpEnabled() && (dumpCreationResult.DumpTypeCreated != DumpType::None)) {
        logger->emitStackCrawlLine("Attach the relevant log files and the crash dump file (specified below) from this machine from at least one hour before this crash.");
        logger->emitStackCrawlLine("NOTE: If this is a private/non-tagged build, please attach the full set of XPS/XPS-Client binaries.");
    } else {
        logger->emitStackCrawlLine("Attach the relevant log files from this machine from at least one hour before this crash. ");
    }
    logger->emitStackCrawlLine("Depending on the crash, it may also be necessary to obtain a backup of the data from a SPARCS client, along with a copy of the");
    logger->emitStackCrawlLine("XPS data folder and settings.xml.");
    logger->emitStackCrawlLine("");

    if (_settings.CrashDumpEnabled()) {
        logger->emitStackCrawlLine("CRASH DUMP FILE:");
        if (dumpCreationResult.DumpTypeCreated != DumpType::None) {
            logger->emitStackCrawlLine(dumpCreationResult.DumpFileName);
        } else {
            logger->emitStackCrawlLine("Failed to create crash dump file.");
        }
    } else {
        logger->emitStackCrawlLine("Not configured to create dump file. Dump file was NOT created.");
    }

    logger->emitStackCrawlLine("");
    logger->emitStackCrawlLine("GENERAL INFORMATION:");
    logger->emitStackCrawlLine(VSTRING_FORMAT("  Current thread id          : 0x%x", GetCurrentThreadId()));
    logger->emitStackCrawlLine(VSTRING_FORMAT("  Program Version            : %s, %s, build date %s", appInfo.VersionString.c_str(), appInfo.SVNRevision.c_str(), appInfo.BuildTimeStamp.c_str()));

    VInstant now;
    VDuration d = now - gStartTime;
    logger->emitStackCrawlLine(VSTRING_FORMAT("  Local time program started : %s", gStartTime.getLocalString().chars()));
    logger->emitStackCrawlLine(VSTRING_FORMAT("  Current local time         : %s", now.getLocalString().chars()));

    VString timeString(VSTRING_ARGS("%dd %dh %dm %d.%03llds", d.getDurationDays(), )
                                        d.getDurationHours() % 24, d.getDurationMinutes() % 60, d.getDurationSeconds() % 60,
                                        d.getDurationMilliseconds() % CONST_S64(1000));
    logger->emitStackCrawlLine(VSTRING_FORMAT("  Elapsed time               : %s", timeString.chars()));
}

void StackCrawlerEx::_writeCrashDumpMachineInfo() {
    // TODO: Optimize
    VString msg; // used to format output strings below

    VNamedLoggerPtr logger = _settings.Logger;

    OSVERSIONINFOEX osvi;

    StackCrawlerEx::GetOSVersionInfo(osvi);

    logger->emitStackCrawlLine("");
    logger->emitStackCrawlLine("OS and MEMORY:");

    typedef void (WINAPI * PGNSI)(LPSYSTEM_INFO);
    typedef BOOL(WINAPI * PGPI)(DWORD, DWORD, DWORD, DWORD, PDWORD);
    PGPI pGPI;
    DWORD dwType = 0;

    VString platform;
    if (osvi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
        if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 0)
            platform = "95";
        else if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion < 90)
            platform = "98";
        else
            platform = "Me";
    } else {
        if (osvi.dwMajorVersion < 4)
            platform = "NT 3";
        else if (osvi.dwMajorVersion == 4)
            platform = "NT 4";
        else if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 0)
            platform = "2000";
        else if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 1)
            platform = "XP";
        else if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 2)
            platform = "Server 2003";
        else if (osvi.dwMajorVersion >= 6) {
            if (osvi.dwMinorVersion == 0) {
                if (osvi.wProductType == VER_NT_WORKSTATION)
                    platform = "Vista";
                else
                    platform = "Server 2008";
            } else if (osvi.dwMinorVersion == 1) {
                if (osvi.wProductType == VER_NT_WORKSTATION)
                    platform = "7";
                else
                    platform = "Server 2008 R2";
            }
            __pragma(warning(suppress: 6309))  // Analyzer mistakenly thinks argument 1 to macro is NULL
                pGPI = (PGPI)GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), "GetProductInfo");
            pGPI(6, 0, 0, 0, &dwType);
        }
    }

    msg.format("  Platform                     : Windows %s (%ld.%d type %ld) build %ld service pack %ld.%ld", platform.chars(), osvi.dwMajorVersion, osvi.dwMinorVersion,
        dwType == 0 ? osvi.wProductType : dwType, osvi.dwBuildNumber, osvi.wServicePackMajor, osvi.wServicePackMinor);
    logger->emitStackCrawlLine(msg);

    //Write physical, virtual and harddisk memory info
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    GlobalMemoryStatusEx(&statex);

    const int MDIVIDE = 1024 * 1024; //divde bytes to get Mbytes
    const int GDIVIDE = 1024 * 1024 * 1024; //divde bytes to get Gbytes

    // 2009.06.03 sjain ARGO-18949 - Log System IP
    VString ipAddr;
    VSocket::getLocalHostIPAddress(ipAddr);
    msg.format("  System IP Address            : %s", ipAddr.chars());
    logger->emitStackCrawlLine(msg);
    msg.format("  Total physical mem installed : %4.0I64d MB", statex.ullTotalPhys / MDIVIDE);
    logger->emitStackCrawlLine(msg);

    double percentage = 100.0 * (1.0 - static_cast<double>(statex.ullAvailPhys) / static_cast<double>(statex.ullTotalPhys));

    msg.format("  Free physical mem avail      : %4.0I64d MB (%3.1lf%% used)", statex.ullAvailPhys / MDIVIDE, percentage);
    logger->emitStackCrawlLine(msg);

    msg.format("  Total virtual mem            : %4.0I64d MB", statex.ullTotalVirtual / MDIVIDE);
    logger->emitStackCrawlLine(msg);

    percentage = 100.0 * (1.0 - static_cast<double>(statex.ullAvailVirtual) / static_cast<double>(statex.ullTotalVirtual));

    msg.format("  Free virtual mem avail       : %4.0I64d MB (%3.1lf%% used)", statex.ullAvailVirtual / MDIVIDE, percentage);
    logger->emitStackCrawlLine(msg);

    __int64 freeBytesAvailable, totalNumberOfBytes, totalNumberOfFreeBytes;
    if (GetDiskFreeSpaceEx(0, (PULARGE_INTEGER)&freeBytesAvailable, (PULARGE_INTEGER)&totalNumberOfBytes, (PULARGE_INTEGER)&totalNumberOfFreeBytes)) {
        msg.format("  Total harddisk mem installed : %4.0I64d GB", totalNumberOfFreeBytes / GDIVIDE);
        logger->emitStackCrawlLine(msg);
    }

    if ((gCurrentScriptLineNumber != 0) && gCurrentScriptCommand.isNotEmpty()) {
        msg.format("  Last Script Command Executed at (Line#%d) :", gCurrentScriptLineNumber);
        logger->emitStackCrawlLine(msg);
        msg.format("    %s", gCurrentScriptCommand.chars());
        logger->emitStackCrawlLine(msg);
    }
}

void StackCrawlerEx::_writeCrashDumpRegisters() {
    SymInfo symInfo;

    symInfo.symbolInfo.SizeOfStruct = sizeof(symInfo);
    symInfo.symbolInfo.MaxNameLength = sizeof(symInfo.symbolName);

    VNamedLoggerPtr logger = _settings.Logger;
    CONTEXT context = _settings.Context.value();

    logger->emitStackCrawlLine("");
    logger->emitStackCrawlLine("REGISTERS:");

#ifdef VCOMPILER_64BIT
    logger->emitStackCrawlLine(VSTRING_FORMAT("   Rax = 0x%0.16X, Rbx = 0x%0.16X, Rcx = 0x%0.16X", context.Rax, context.Rbx, context.Rcx));
    logger->emitStackCrawlLine(VSTRING_FORMAT("   Rdx = 0x%0.16X, Rsi = 0x%0.16X, Rdi = 0x%0.16X", context.Rdx, context.Rsi, context.Rdi));
    logger->emitStackCrawlLine(VSTRING_FORMAT("   Rip = 0x%0.16X, Rsp = 0x%0.16X, Rbp = 0x%0.16X", context.Rip, context.Rsp, context.Rbp));
    logger->emitStackCrawlLine(VSTRING_FORMAT("   P1  = 0x%0.16X, P2  = 0x%0.16X, P3  = 0x%0.16X", context.P1Home, context.P2Home, context.P3Home));
    logger->emitStackCrawlLine(VSTRING_FORMAT("   P4  = 0x%0.16X, P5  = 0x%0.16X, P6  = 0x%0.16X", context.P4Home, context.P5Home, context.P6Home));

    DWORD64 offset;
    DWORD64 address = context.Rip;
#else
    logger->emitStackCrawlLine(VSTRING_FORMAT("  Eax = %0.8X, Ebx = %0.8X, Ecx = %0.8X", context.Eax, context.Ebx, context.Ecx));
    logger->emitStackCrawlLine(VSTRING_FORMAT("  Edx = %0.8X, Esi = %0.8X, Edi = %0.8X", context.Edx, context.Esi, context.Edi));
    logger->emitStackCrawlLine(VSTRING_FORMAT("  Eip = %0.8X, Esp = %0.8X, Ebp = %0.8X", context.Eip, context.Esp, context.Ebp));

    DWORD offset;
    DWORD address = context.Eip;
#endif
    logger->emitStackCrawlLine("");

    if (_settings.HasSymbols && ::SymGetSymFromAddr(_settings.ProcessHandle, address, &offset, &symInfo.symbolInfo)) {
        logger->emitStackCrawlLine("SYMBOLS AVAILABLE:");
#ifdef VCOMPILER_64BIT
        logger->emitStackCrawlLine(VSTRING_FORMAT("  Rip = 0x%0.16X falls inside function '%s'", context.Rip, symInfo.symbolInfo.Name));
#else
        logger->emitStackCrawlLine(VSTRING_FORMAT("  Eip = %0.8X falls inside function %s", context.Eip, symInfo.symbolInfo.Name));
#endif
    } else {
        logger->emitStackCrawlLine("NO SYMBOLS AVAILABLE -- Please ensure the pdb file is located in the current working directory:");
        logger->emitStackCrawlLine(VFSNode::getCurrentWorkingDirectory().getPath());
        // 2010.09.21 ranstrom ARGO-28381 add note for 64-bit OS
        logger->emitStackCrawlLine("or for 64-bit OS, in /Windows/sysWOW64");
    }
}

void StackCrawlerEx::_writeCrashDumpStackFrames() {
    VNamedLoggerPtr logger = _settings.Logger;

    DWORD originalSymOptions = ::SymGetOptions();

    try {
#ifdef VCOMPILER_64BIT
        // We don't know when the ::SymInitialize was called. So, refresh the modules' list.
        ::SymRefreshModuleList(_settings.ProcessHandle);
#endif
        // Set options for un-decorated name, load source lines etc.
        ::SymSetOptions(SYMOPT_UNDNAME | SYMOPT_CASE_INSENSITIVE | SYMOPT_LOAD_LINES | SYMOPT_OMAP_FIND_NEAREST | SYMOPT_NO_PROMPTS | SYMOPT_DEBUG);

        STACKFRAME64 stackFrame = { 0 };

        _convertContextToStackFrame(_settings.Context.value(), stackFrame);

        PIMAGE_NT_HEADERS pImageNTHeaders = ::ImageNtHeader(::GetModuleHandle(NULL));

        WORD machineType = pImageNTHeaders->FileHeader.Machine;

        logger->emitStackCrawlLine("");
        logger->emitStackCrawlLine("CALL STACK:");
        logger->emitStackCrawlLine("Stack Id Frame              Module               Address           Function");
        logger->emitStackCrawlLine("-------- ------------------ -------------------- ----------------- ----------------------------------");

        ParameterInfoListPtr parameters = ParameterInfoListPtr(new ParameterInfoList()); // TODO: Avoid creating this list.

        _parseFunctionInfo(1, machineType, stackFrame, parameters, false);

        if (::StackWalk64(machineType, _settings.ProcessHandle, _settings.ThreadHandle, &stackFrame, &(_settings.Context.value()), NULL, 
                            &(::SymFunctionTableAccess64), &(::SymGetModuleBase64), NULL)) {
            _parseFunctionInfo(2, machineType, stackFrame, parameters, true);
        }

        if (_settings.DisplayParametersInfo) {
            logger->emitStackCrawlLine("");
            logger->emitStackCrawlLine("PARAMETERS:");
            logger->emitStackCrawlLine("Stack Id Address            Name                             Type                 Value");
            logger->emitStackCrawlLine("-------- ------------------ -------------------------------- -------------------- --------------------------");

            VString printableAddress;
            VString parameterInfoLine;

            ParameterInfoList::iterator iter = parameters->begin();

            for (; iter != parameters->end(); iter++) {
                StackCrawlerParameterInfo parameterInfo = *iter;

                printableAddress.format(POINTER_FORMATTER, reinterpret_cast<void*>(parameterInfo.Address));

                parameterInfoLine.format("%03d      %s %-32s %-20s %s", parameterInfo.FunctionId, printableAddress.chars(), parameterInfo.Name.chars(),
                    parameterInfo.TypeName.chars(), parameterInfo.ValueAsString.chars());
                logger->emitStackCrawlLine(parameterInfoLine);
            }
        }
    } catch (const std::exception& ex) {
        logger->emitStackCrawlLine("Failed to write debug dump info: ");
        logger->emitStackCrawlLine(ex.what());
    } catch (...) {
        logger->emitStackCrawlLine("Failed to write debug dump info: Unknown error");
    }

    ::SymSetOptions(originalSymOptions);
}

bool StackCrawlerEx::_parseFunctionInfo(int functionId, DWORD machineType, STACKFRAME64& stackFrame, ParameterInfoListPtr parameters, bool continueStackWalk) {
    VString logLine;

    VNamedLoggerPtr logger = _settings.Logger;

    bool result = false;

    if (_settings.DisplayParametersInfo) {
        _parseStackParameters(functionId, stackFrame, parameters);
    }

#ifdef VCOMPILER_64BIT
    void* frameAddress = reinterpret_cast<void*>(_settings.Context.value().Rbp);
#else
    void* frameAddress = reinterpret_cast<void*>(_settings.Context.value().Ebp);
#endif

    DWORD64 address = stackFrame.AddrPC.Offset;

    PSYMBOL_INFO pSymbolInfo = (PSYMBOL_INFO)new char[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
    pSymbolInfo->MaxNameLen = MAX_SYM_NAME;
    pSymbolInfo->SizeOfStruct = sizeof(SYMBOL_INFO);

    DWORD64 dwDisplacement64;

    IMAGEHLP_MODULE64 moduleInfo = { 0 };
    moduleInfo.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);

    IMAGEHLP_LINE64 lineInfo;

    DWORD originalOptions = ::SymGetOptions();

    DWORD localOptions = (originalOptions & ~SYMOPT_UNDNAME);

    localOptions = (localOptions | SYMOPT_PUBLICS_ONLY);

    ::SymSetOptions(localOptions);

    VString moduleAndFunctionInfo;
    VString functionLineInfo;
    VString printableAddress;

    if (::SymGetModuleInfo64(_settings.ProcessHandle, address, &moduleInfo)) {
        char pUndecoratedName[MAX_SYM_NAME] = { 0 };

        if (::SymFromAddr(_settings.ProcessHandle, address, &dwDisplacement64, pSymbolInfo)) {
            // TODO: Get function's return type here.

            ::UnDecorateSymbolName(pSymbolInfo->Name, pUndecoratedName, MAX_SYM_NAME, UNDNAME_COMPLETE);

            printableAddress = VSTRING_FORMAT(POINTER_FORMATTER, reinterpret_cast<void*>(address));

            moduleAndFunctionInfo.format(" %-20s%s %s", moduleInfo.ModuleName, printableAddress.chars(), pUndecoratedName);
        }

        DWORD dwDisplacement = 0;

        if (::SymGetLineFromAddr64(_settings.ProcessHandle, address, &dwDisplacement, &lineInfo)) {
            functionLineInfo.format(" at-> %s (%d)", lineInfo.FileName, lineInfo.LineNumber);
        }

        result = true;
    }

    delete[] pSymbolInfo;

    ::SymSetOptions(originalOptions);

    printableAddress.format(POINTER_FORMATTER, frameAddress);

    logLine.format("%03d      %s%s%s", functionId, printableAddress.chars(), moduleAndFunctionInfo.chars(), functionLineInfo.chars());

    logger->emitStackCrawlLine(logLine);

    if (continueStackWalk &&
        ::StackWalk64(machineType, _settings.ProcessHandle, _settings.ThreadHandle, &stackFrame, &(_settings.Context.value()), NULL, 
                        &(::SymFunctionTableAccess64), &(::SymGetModuleBase64), NULL)) {
        result &= _parseFunctionInfo((functionId + 1), machineType, stackFrame, parameters, true);
    }

    return result;
}

bool StackCrawlerEx::_parseStackParameters(int functionId, STACKFRAME64 stackFrame, ParameterInfoListPtr parameters) {
    IMAGEHLP_STACK_FRAME currentStackFrame = { 0 };

    currentStackFrame.InstructionOffset = stackFrame.AddrPC.Offset;

    if (!::SymSetContext(_settings.ProcessHandle, &currentStackFrame, NULL)) {
        return false;
    }

    EnumerateSymbolsCallbackParam* pParam = new EnumerateSymbolsCallbackParam(functionId, this, stackFrame, parameters);

    BOOL result = ::SymEnumSymbols(_settings.ProcessHandle, 0, NULL, EnumerateSymbolsCallback, pParam);

    delete pParam;

    return (result == TRUE);
}

bool StackCrawlerEx::_handleEnumeratedParameter(int functionId, STACKFRAME64 stackFrame, PSYMBOL_INFO pSymbolInfo, ParameterInfoListPtr parameters) {
    bool result = true;

    if (pSymbolInfo == NULL) {
        return false;
    }

    StackCrawlerParameterInfo parameterInfo;

    parameterInfo.FunctionId = functionId;
    parameterInfo.Name = pSymbolInfo->Name;

    enum SymTagEnum tag;

#ifdef VCOMPILER_64BIT
    // TODO: In x64, retrieving parameters is not as straight forward as in x86. Some parameters are
    // stored in registers and others in stack. Need more work on this.
    parameterInfo.Address = 0;
#else
    parameterInfo.Address = pSymbolInfo->Address;
#endif

    result = _parseParameterTypeAndValue(stackFrame, pSymbolInfo, false, 0, parameterInfo, tag);

    parameters->push_back(parameterInfo);

    return result;
}

bool StackCrawlerEx::_parseParameterTypeAndValue(const STACKFRAME64& stackFrame, PSYMBOL_INFO pSymbolInfo, bool resolvingPointerType, DWORD pointerType, 
                                                    StackCrawlerParameterInfo& parameterInfo, enum SymTagEnum& tag) {
    if (!resolvingPointerType) {
        parameterInfo.TypeName = StackCrawlerParameterInfo::UNKNOWN_TYPE_OR_VALUE;
        parameterInfo.ValueAsString = StackCrawlerParameterInfo::UNKNOWN_TYPE_OR_VALUE;
    }

    tag = (enum SymTagEnum)0;

    ULONG typeId = (resolvingPointerType ? pointerType : pSymbolInfo->TypeIndex);

    if (!::SymGetTypeInfo(_settings.ProcessHandle, pSymbolInfo->ModBase, typeId, TI_GET_SYMTAG, &tag)) {
        return false;
    }

    if (tag == SymTagBaseType) { // If primitive data type, try and get the value (for now, works only for x86)
        StackCrawlerPrimitiveType type = (StackCrawlerPrimitiveType)0;

        if (!::SymGetTypeInfo(_settings.ProcessHandle, pSymbolInfo->ModBase, typeId, TI_GET_BASETYPE, &type)) {
            return false;
        }

        ULONG64 length = 0;

        if (!::SymGetTypeInfo(_settings.ProcessHandle, pSymbolInfo->ModBase, typeId, TI_GET_LENGTH, &length)) {
            return false;
        }

        _getParameterTypeAndValue(type, stackFrame, resolvingPointerType, tag, length, pSymbolInfo->Address, parameterInfo);

        if (resolvingPointerType) {
            parameterInfo.TypeName += "*";
        }
    } else if (tag == SymTagPointerType) { // If the parameter is a pointer try and get the pointer type - just get the address and not the value
        DWORD ptrType;

        if (!::SymGetTypeInfo(_settings.ProcessHandle, pSymbolInfo->ModBase, typeId, TI_GET_TYPE, &ptrType) ||
            !_parseParameterTypeAndValue(stackFrame, pSymbolInfo, true, ptrType, parameterInfo, tag)) {
            return false;
        }

        if (resolvingPointerType) {
            parameterInfo.TypeName += "*";
        }
    } else {    // If other complex type (except enums) - just get the address and not the value
        LPWSTR rawTypeName = NULL;

        BOOL ret = ::SymGetTypeInfo(_settings.ProcessHandle, pSymbolInfo->ModBase, pSymbolInfo->TypeIndex, TI_GET_SYMNAME, &rawTypeName);

        if (!ret) {
            return false;
        }

        boost::container::vector<char> typeNameInANSI(::wcslen(rawTypeName) + 1);

        ::WideCharToMultiByte(CP_ACP, 0, rawTypeName, (int)typeNameInANSI.size(), &typeNameInANSI[0], (int)typeNameInANSI.size(), NULL, NULL);

        parameterInfo.TypeName = &typeNameInANSI[0];

        if (resolvingPointerType) {
            parameterInfo.TypeName += "*";

            // Using 'NoType' to indicate that this is a complex type.
            _getParameterTypeAndValue(SC_NoType, stackFrame, resolvingPointerType, tag, 0, pSymbolInfo->Address, parameterInfo);
        }

        ::LocalFree(rawTypeName);
    }

    return true;
}

/*static*/ 
int __stdcall StackCrawlerEx::EnumerateSymbolsCallback(PSYMBOL_INFO pSymbolInfo, unsigned long /*size*/, void* pParam) {
    if ((pSymbolInfo->Flags & SYMFLAG_PARAMETER) != SYMFLAG_PARAMETER) {
        // Possibly, end of parameters.
        return 1;
    }

    EnumerateSymbolsCallbackParam* pCallbackParam = reinterpret_cast<EnumerateSymbolsCallbackParam*>(pParam);

    pCallbackParam->StackCrawlerInstance->_handleEnumeratedParameter(pCallbackParam->FunctionId, pCallbackParam->StackFrame, pSymbolInfo, pCallbackParam->Parameters);

    return 1;
}

void StackCrawlerEx::_getParameterTypeAndValue(const StackCrawlerPrimitiveType& primitiveType, const STACKFRAME64& stackFrame, bool isPointerType, enum SymTagEnum tag,
                                                ULONG64 parameterSize, ULONG64 symbolAddress, StackCrawlerParameterInfo& parameterInfo) {
    ULONG64 address = (stackFrame.AddrFrame.Offset + symbolAddress);

    void* pData = reinterpret_cast<void*>(address);

    if (tag == SymTagEnum) {
#ifdef VCOMPILER_64BIT
        // TODO: In x64, retrieving parameters is not as straight forward as in x86. Some parameters are
        // stored in registers and others in stack. Need more work on this.
        parameterInfo.ValueAsString = StackCrawlerParameterInfo::UNKNOWN_TYPE_OR_VALUE;
#else
        // TODO: Needs more work.
        // SMELL: Find the size of the enum and get it's value using ::SymGetTypeInfo.
        parameterInfo.ValueAsString.format("%d (?)", *(int*)pData);
#endif

        return;
    } 

#ifdef VCOMPILER_64BIT
    // TODO: In x64, retrieving parameters is not as straight forward as in x86. Some parameters are
    // stored in registers and others in stack. Need more work on this.
    bool processValue = false;

#else
    bool processValue = !isPointerType;
#endif

    switch (primitiveType) {
        case SC_Char: 
            parameterInfo.TypeName = "char";

            if (processValue) {
                parameterInfo.ValueAsString.format("%c", (char)*(__int8*)pData);
            }

            break;

        case SC_WChar: 
            parameterInfo.TypeName = "wchar_t";

            if (processValue) {
                parameterInfo.ValueAsString.format("%lc", (wchar_t)*(__int16*)pData);
            }

            break;

        case SC_Int: 
            _parseIntegerValue(pData, parameterSize, true, !processValue, parameterInfo);

            break;

        case SC_UInt: 
            _parseIntegerValue(pData, parameterSize, false, !processValue, parameterInfo);

            break;

        case SC_Float: 
            _parseFloatValue(pData, parameterSize, !processValue, parameterInfo);

            break;

        case SC_Bool: 
            parameterInfo.TypeName = "bool";

            if (processValue) {
                parameterInfo.ValueAsString.format("%s", ((*(bool*)pData) ? "true" : "false"));
            }

            break;

        case SC_Long: 
            parameterInfo.TypeName = "long";

            if (processValue) {
                parameterInfo.ValueAsString.format("%ld", *(long*)pData);
            }

            break;

        case SC_ULong: 
            parameterInfo.TypeName = "unsigned long";

            if (processValue) {
                parameterInfo.ValueAsString.format("%lu", *(unsigned long*)pData);
            }

            break;

        case SC_Hresult: 
        case SC_Void: 
            parameterInfo.TypeName = "void";
            processValue = false; 

        break;

        default:
            if (primitiveType != SC_NoType) {
                parameterInfo.TypeName = StackCrawlerParameterInfo::UNKNOWN_TYPE_OR_VALUE;
            }

            break;
    }

    if (!processValue && isPointerType) {
		ULONG64 addr;

#ifdef VCOMPILER_64BIT
        addr = *(unsigned __int64*)pData;
#else
		addr = *(unsigned __int32*)pData;
#endif
        parameterInfo.ValueAsString.format(POINTER_FORMATTER, addr);
    } else if (!isPointerType) {
        parameterInfo.ValueAsString = StackCrawlerParameterInfo::UNKNOWN_TYPE_OR_VALUE;
    }
}

void StackCrawlerEx::_parseIntegerValue(void* pAddress, ULONG64 parameterSize, bool isSigned, bool ignoreValue, StackCrawlerParameterInfo& parameterInfo) {
    parameterInfo.TypeName = StackCrawlerParameterInfo::UNKNOWN_TYPE_OR_VALUE;
    parameterInfo.ValueAsString = StackCrawlerParameterInfo::UNKNOWN_TYPE_OR_VALUE;

    if (parameterSize == sizeof(__int8)) {
        parameterInfo.TypeName = "byte";

        if (!ignoreValue) {
            if (isSigned) {
                parameterInfo.ValueAsString.format("%d", *(__int8*)pAddress);
            } else {
                parameterInfo.ValueAsString.format("%u", *(unsigned __int8*)pAddress);
            }
        }
    } else if (parameterSize == sizeof(__int16)) {
        parameterInfo.TypeName = "short";

        if (!ignoreValue) {
            if (isSigned) {
                parameterInfo.ValueAsString.format("%d", *(__int16*)pAddress);
            } else {
                parameterInfo.ValueAsString.format("%u", *(unsigned __int16*)pAddress);
            }
        }
    } else if (parameterSize == sizeof(__int32)) {
        parameterInfo.TypeName = "int";

        if (!ignoreValue) {
            if (isSigned) {
                parameterInfo.ValueAsString.format("%ld", *(__int32*)pAddress);
            } else {
                parameterInfo.ValueAsString.format("%lu", *(unsigned __int32*)pAddress);
            }
        }
    } else if (parameterSize == sizeof(__int64)) {
        parameterInfo.TypeName = "__int64"; // Or, long long

        if (!ignoreValue) {
            if (isSigned) {
                parameterInfo.ValueAsString.format("%lld", *(__int64*)pAddress);
            } else {
                parameterInfo.ValueAsString.format("%llu", *(unsigned __int64*)pAddress);
            }
        }
    }

    if (parameterInfo.TypeName != StackCrawlerParameterInfo::UNKNOWN_TYPE_OR_VALUE && !isSigned) {
        parameterInfo.TypeName = VString("unsigned ") + parameterInfo.TypeName;
    }
}

void StackCrawlerEx::_parseFloatValue(void* pAddress, ULONG64 parameterSize, bool ignoreValue, StackCrawlerParameterInfo& parameterInfo) {
    parameterInfo.TypeName = StackCrawlerParameterInfo::UNKNOWN_TYPE_OR_VALUE;
    parameterInfo.ValueAsString = StackCrawlerParameterInfo::UNKNOWN_TYPE_OR_VALUE;

    if (parameterSize == sizeof(float)) {
        parameterInfo.TypeName = "float";

        if (!ignoreValue) {
            parameterInfo.ValueAsString.format("%f", *(float*)pAddress);
        }
    } else if (parameterSize == sizeof(double)) {
        parameterInfo.TypeName = "double";

        if (!ignoreValue) {
            parameterInfo.ValueAsString.format("%f", *(double*)pAddress);
        }
    }
}

/*static*/
void StackCrawlerEx::_convertContextToStackFrame(const CONTEXT& context, STACKFRAME64& stackFrame) {
    stackFrame.AddrReturn.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrStack.Mode = AddrModeFlat;
    stackFrame.AddrBStore.Mode = AddrModeFlat;

    stackFrame.Virtual = TRUE;

    // Not supported (at least for now):
    // ARM x86/x64
    // IA x64

#ifdef _M_IX86 // x86
    stackFrame.AddrPC.Offset = context.Eip;
    stackFrame.AddrReturn.Offset = context.Eip;
    stackFrame.AddrFrame.Offset = context.Ebp;
    stackFrame.AddrStack.Offset = context.Esp;
    stackFrame.AddrBStore = stackFrame.AddrFrame;
#elif (defined(_AMD64_) || defined(_M_AMD64)) // AMD x64
    stackFrame.AddrPC.Offset = context.Rip;
    stackFrame.AddrReturn.Offset = context.Rip;
    stackFrame.AddrFrame.Offset = context.Rbp;
    stackFrame.AddrStack.Offset = context.Rsp;
    stackFrame.AddrBStore = stackFrame.AddrFrame;
#else
    #error "Stack Crawler is not supported for the targeted platform!!!"
#endif
}

/**
Writes a minidump file containing the state of the application.
@param  exceptionPointers   if not null, will be used when calling MiniDumpWriteDump;
if NULL, will pass NULL (you should supply null if this is not a real crash,
because the experimental behavior is that we fail on the pointer obtained in _triggerDumpFile)
@param  fileNamePrefix      the string to prefix the _<timstamp>.dmp part of the file name;
pass "crashdump" or whatever for real crash, "shutdown" etc. otherwise, to distinguish the files
*/
DumpCreationResult StackCrawlerEx::_writeMiniDumpToNewFile() {
    ApplicationInfo appInfo = GetApplicationInfo();

    DumpCreationResult result = DumpCreationResult();

    VString fileNamePrefix(_settings.GetDumpFileNamePrefix().c_str());

    VInstant now;
    VString dumpFileName(VSTRING_ARGS("%s_%s.dmp", fileNamePrefix.chars(), now.getLocalString(true).chars()));
    VString dumpFilePath;
    VFSNode::getExecutableDirectory().getChildPath(dumpFileName, dumpFilePath);

    VStringVector loggerOutput;
    loggerOutput.push_back(VSTRING_FORMAT("_writeMiniDumpToNewFile: Creating %s dump file '%s'...", DumpTypeConverter::ToString(_settings.DumpTypeToGenerate).c_str(), dumpFilePath.chars()));

    HANDLE fileHandle = CreateFile(dumpFilePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if ((fileHandle == NULL) || (fileHandle == INVALID_HANDLE_VALUE)) {
        loggerOutput.push_back(VSTRING_FORMAT("_writeMiniDumpToNewFile: CreateFile failed for file '%s'. Error: %u", dumpFilePath.chars(), GetLastError()));
    } else {
        MINIDUMP_EXCEPTION_INFORMATION mdei;
        mdei.ThreadId = GetCurrentThreadId();
        mdei.ExceptionPointers = _settings.Exceptions;
        mdei.ClientPointers = FALSE;

        VStringVector customStrings;
        VDuration d = now - gStartTime;
        VString timeString(VSTRING_ARGS("%dd %dh %dm %d.%03llds", d.getDurationDays(), )
                                        d.getDurationHours() % 24, d.getDurationMinutes() % 60, d.getDurationSeconds() % 60,
                                        d.getDurationMilliseconds() % CONST_S64(1000));

        customStrings.push_back(VSTRING_FORMAT("Program Version            : %s %s, build date %s", appInfo.VersionString.c_str(), appInfo.SVNRevision.c_str(), appInfo.BuildTimeStamp.c_str()));
        customStrings.push_back(VSTRING_FORMAT("Local time program started : %s", gStartTime.getLocalString().chars()));
        customStrings.push_back(VSTRING_FORMAT("Current local time         : %s", now.getLocalString().chars()));
        customStrings.push_back(VSTRING_FORMAT("Elapsed time               : %s", timeString.chars()));
        // 2010.03.10 ranstrom ARGO-24449  add the common WinDbg command so we do not have to go to confluence page each time.
        customStrings.push_back(VString("Use this WinDbg command    : !analyze -v; !uniqstack   "));

        MINIDUMP_USER_STREAM* mdus = _allocateCustomMiniDumpStrings(customStrings);
        MINIDUMP_USER_STREAM_INFORMATION mdusi;
        mdusi.UserStreamCount = static_cast<ULONG>(customStrings.size());
        mdusi.UserStreamArray = mdus;

        PMINIDUMP_EXCEPTION_INFORMATION pExceptionInfo = ((_settings.Exceptions == NULL) ? 0 : &mdei);

        DWORD dumpTypeFlags = (MiniDumpWithThreadInfo | MiniDumpWithFullMemory | MiniDumpWithFullMemoryInfo | MiniDumpWithHandleData | MiniDumpWithUnloadedModules);

        DumpType dumpToCreate = _settings.DumpTypeToGenerate;

        BOOL dumpSuccessfullyCreated = false;

        if (dumpToCreate == DumpType::Full) {
            dumpSuccessfullyCreated = ::MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), fileHandle, static_cast<MINIDUMP_TYPE>(dumpTypeFlags), pExceptionInfo, &mdusi, 0);

            if (!dumpSuccessfullyCreated) {
                loggerOutput.push_back(VString("_writeMiniDumpToNewFile: Failed to create full dump. Will attempt to create mini dump..."));

                dumpToCreate = DumpType::Tiny; // Might have failed due to disk-space issues. No harm in retrying with tiny dump.
            }
        }

        if (dumpToCreate == DumpType::Tiny) {
            dumpSuccessfullyCreated = ::MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), fileHandle, MiniDumpNormal, pExceptionInfo, &mdusi, 0);
        }

        if (dumpSuccessfullyCreated) {
            loggerOutput.push_back(VSTRING_FORMAT("_writeMiniDumpToNewFile: Wrote file '%s'.", dumpFilePath.chars()));

            result.DumpTypeCreated = dumpToCreate;
            result.DumpFileName = dumpFilePath;
        } else if (dumpToCreate != DumpType::None) {
            loggerOutput.push_back(VSTRING_FORMAT("_writeMiniDumpToNewFile: Failed to write file '%s'. Error: %u", dumpFilePath.chars(), GetLastError()));
        }

        CloseHandle(fileHandle);

        delete[] mdus;
    }

    for (VStringVector::const_iterator i = loggerOutput.begin(); i != loggerOutput.end(); ++i) {
        if (_settings.Exceptions == NULL) { // Not a fatal error, just a requested shutdown dump.
            VLOGGER_INFO(*i);
        } else {
            StackCrawlerEx::LogFatal(*i);
        }
    }

    return result;
}

MINIDUMP_USER_STREAM* StackCrawlerEx::_allocateCustomMiniDumpStrings(const VStringVector& elements) {
    int numElements = (int)elements.size();

    MINIDUMP_USER_STREAM* s = new MINIDUMP_USER_STREAM[elements.size()];

    for (int i = 0; i < numElements; ++i) {
        s[i].Type = CommentStreamA;
        s[i].BufferSize = elements[i].length() + 1;
        s[i].Buffer = (PVOID)elements[i].chars();
    }

    return s;
}

static void _launchCrashDumpViewer(const VString& nativePathOfLogFileToLaunch) { // TODO: mohanla - move to oop
    VString execParam(VSTRING_ARGS("%s %s", gStdlogViewerApp.chars(), nativePathOfLogFileToLaunch.chars()));
    UINT result = WinExec(execParam, SW_SHOWDEFAULT);
    // Kind of bizarre set of return values. Values 0 through 31 are all errors.
    // Docs say you should use CreateProcess() instead of WinExec() if possible.
    if (result <= 31) {
        StackCrawlerEx::LogFatal(VSTRING_FORMAT("_launchCrashDumpViewer: WinExec(%s) returned error code %d.", execParam.chars(), (int)result));
    }
}
#else
static bool init_dbg_symbols_ok = false;
#endif // ENABLE_WINDOWS_STACK_CRAWLER

#ifndef ENABLE_WINDOWS_STACK_CRAWLER
#pragma warning(push)
#pragma warning(disable: 4100 4189)
#endif
static void _writeCrashDumpInfoToLogger(StackCrawlerExSettings& settings, const VString& headerMessage) {
    VNamedLoggerPtr logger = settings.Logger;

    // Protect against recursion should any of our dump code itself "crash":
    // If we have set up StackCrawlThreadInfo in response to an actual crash,
    // mark it "in progress" here; if we see it's already in progress, stop.
    // If we haven't setup StackCrawlThreadInfo, it means we're being called
    // not from an actual crash, but from a command, so proceed.
    StackCrawlThreadInfo* threadInfo = _lookupThreadInfo(GetCurrentThreadId());

    if (threadInfo == NULL) {
        logger->emitStackCrawlLine(VSTRING_FORMAT("UNABLE TO FIND STACK CRAWL INFO FOR CURRENT THREAD ID %d", GetCurrentThreadId()));
        logger->emitStackCrawlLine(headerMessage);
    } 

    // Use DbgHelp.dll to obtain symbolic information.
    if (!threadInfo->hasSymbols()) {
        // symbols should have been initialized, its ok to call multiple times
        int sym_init = StackCrawlerEx::InitDbgSymbols();

        static int sym_init_error = 0;

        if (!sym_init) {
            sym_init_error = GetLastError();
        }

        threadInfo->setHasSymbols(init_dbg_symbols_ok);

        settings.HasSymbols = init_dbg_symbols_ok;
    } else {
        settings.HasSymbols = true;
    }

    StackCrawlerEx stackCrawler(settings);

    stackCrawler.WriteCrashDumpInfo(headerMessage);
}
#ifndef ENABLE_WINDOWS_STACK_CRAWLER
#pragma warning(pop)
#endif

//This is called when an unhandled exception is thrown and generates a stack crawl which is
//written to STDLOG.TXT.
static void _writeCrashDumpToStandardLogFile(StackCrawlerExSettings& settings, const VString& headerMessage, bool launchViewer) {
    static VMutex mutex("crashDumpMutex");

    VMutexLocker lock(&mutex, "writeCrashDumpToStandardLogFile()");

    VStringVector loggerOutput;

    VFSNode crashLogFile;

    VFSNode::getExecutableDirectory().getChildNode(LOG_FILE_NAME, crashLogFile);

    try {
        VBufferedFileStream stream(crashLogFile);

        stream.openReadWrite();
    } catch (const VException& ex) {
        StackCrawlerEx::LogFatal(VSTRING_FORMAT("Unable to open crash log file '%s'. Aborting crash dump: %s", crashLogFile.getPath().chars(), ex.what()));
        return;
    }

    try {
        VLogAppenderPtr appender(new VFileLogAppender("crash-log-appender", VLogAppender::DONT_FORMAT_OUTPUT, crashLogFile.getPath()));

        VNamedLoggerPtr logger(new VNamedLogger("crash-logger", VLoggerLevel::TRACE, VStringVector(), appender));

        settings.Logger = logger;

        StackCrawlThreadInfo* threadInfo = _lookupThreadInfo(GetCurrentThreadId());

        if (threadInfo->isInExceptionHandler()) {
            logger->emitStackCrawlLine("***** _writeCrashDumpToStandardLogFile called while already in progress.");
        } else {
            threadInfo->enteringExceptionHandler();

            loggerOutput.push_back(VSTRING_FORMAT("_writeCrashDumpToStandardLogFile: Writing file '%s'...", crashLogFile.getPath().chars()));

            _writeCrashDumpInfoToLogger(settings, headerMessage);

            loggerOutput.push_back(VSTRING_FORMAT("_writeCrashDumpToStandardLogFile: Wrote file '%s'.", crashLogFile.getPath().chars()));

            threadInfo->leavingExceptionHandler();
        }
    } catch (const std::exception& ex) {
        loggerOutput.push_back(VSTRING_FORMAT("Error writing crash log file '%s': %s", crashLogFile.getPath().chars(), ex.what()));
    }

    for (VStringVector::const_iterator i = loggerOutput.begin(); i != loggerOutput.end(); ++i) {
        StackCrawlerEx::LogFatal(*i);
    }

    // Note that we don't launch the viewer until the VRawFileLogger writing to it has closed.
    // Otherwise some apps complain about the file being open by us.
    if (launchViewer && gStdlogViewerApp.isNotEmpty()) {
        VString crashLogFileNativePath = crashLogFile.getPath();

        VFSNode::denormalizePath(crashLogFileNativePath);

        _launchCrashDumpViewer(crashLogFileNativePath);
    }
}

#ifdef VAULT_USER_STACKCRAWL_SUPPORT
// If toLogger is NULL, we will log to the crash log file, not the logger.
static void _generateStackCrawlWithRegisters(VNamedLoggerPtr toLogger, const VString& headerMessage, bool isOnDemand,
                                             bool displayProlog, bool displayMachineInfo, bool displayRegisters, bool displayStackCrawl, bool displayParametersInfo, bool launchViewer) {
    CONTEXT context;

    // Capture the register values.
#ifdef VCOMPILER_64BIT
    ::ZeroMemory(&context, sizeof(CONTEXT));

    ::RtlCaptureContext(&context);
#else
#ifndef _lint
    _asm mov context.Eax, eax;
    _asm mov context.Ebx, ebx;
    _asm mov context.Ecx, ecx;
    _asm mov context.Edx, edx;
    _asm mov context.Esi, esi;
    _asm mov context.Edi, edi;
    _asm mov context.Esp, esp;
    _asm mov context.Ebp, ebp;
#endif /* _lint */

    context.Eip = *reinterpret_cast<DWORD*>(context.Ebp + 4);
    context.Ebp = *reinterpret_cast<DWORD*>(context.Ebp);
#endif /* VCOMPILER_64BIT */

    StackCrawlerExSettings stackCrawlerSettings = {};

    stackCrawlerSettings.IsOnDemand             = isOnDemand;

    stackCrawlerSettings.Logger                 = toLogger;

    stackCrawlerSettings.ProcessHandle          = ::GetCurrentProcess();
    stackCrawlerSettings.ThreadHandle           = ::GetCurrentThread();

    stackCrawlerSettings.Exceptions             = NULL;

    stackCrawlerSettings.Context                = boost::optional<CONTEXT>(context);

    stackCrawlerSettings.HasSymbols             = false;    // It's OK even if ::SymInitialize is already called.

    stackCrawlerSettings.DisplayProlog          = displayProlog;
    stackCrawlerSettings.DisplayMachineInfo     = displayMachineInfo;
    stackCrawlerSettings.DisplayRegisters       = displayRegisters;
    stackCrawlerSettings.DisplayStackFrames     = displayStackCrawl;
    stackCrawlerSettings.DisplayParametersInfo  = displayParametersInfo;

    if (toLogger == NULL) {
        _writeCrashDumpToStandardLogFile(stackCrawlerSettings, headerMessage, launchViewer);
    } else {
        _writeCrashDumpInfoToLogger(stackCrawlerSettings, headerMessage);
    }
}

std::mutex VThread::logStackCrawlSynchronizer;

void VThread::logStackCrawl(const VString& headerMessage, VNamedLoggerPtr logger, bool /*verbose*/) {
    std::lock_guard<std::mutex> lock(logStackCrawlSynchronizer);

    DWORD threadId = GetCurrentThreadId();

    StackCrawlThreadInfo* threadInfo = _lookupThreadInfo(threadId);

    bool threadInfoAdded = false;

    if (threadInfo == NULL) {
        _registerThreadInfo(GetCurrentThreadId(), new StackCrawlThreadInfo(0));

        threadInfo = _lookupThreadInfo(GetCurrentThreadId());

        threadInfoAdded = true;
    }

    _generateStackCrawlWithRegisters(logger, headerMessage, true, false, false, false, true, false, false /*do not launch Notepad*/);

    if (threadInfoAdded) {
        _deregisterThreadInfo(threadId);
    }
}
#endif

//--------------------------------------------------------------------------------
LONG WINAPI _stackCrawlExceptionFilter(struct _EXCEPTION_POINTERS* exceptionPointers) {
    StackCrawlThreadInfo* threadInfo = _lookupThreadInfo(GetCurrentThreadId());

    if (threadInfo == NULL) {
        _registerThreadInfo(GetCurrentThreadId(), new StackCrawlThreadInfo(0));
    } else if (threadInfo->isInExceptionHandler()) {
        return EXCEPTION_EXECUTE_HANDLER;
    }

    DWORD exceptionCode = exceptionPointers->ExceptionRecord->ExceptionCode;

    // Handle all SEH errors by producing a crash dump and stdlog stack trace

    // deallocate the working memory for low-memory conditions
    ::free(gSEHEmergencyMemory);
    gSEHEmergencyMemory = 0;

	if (exceptionCode == STATUS_NO_MEMORY) {
		if (!alreadyHandlingOutOfMemory) {
			alreadyHandlingOutOfMemory = true;
            StackCrawlerEx::LogFatal("Out of memory exception ");  // re-entered. Is that possible?
			alreadyHandlingOutOfMemory = false;
		}
        exceptionPointers->ExceptionRecord->ExceptionFlags |= EXCEPTION_NONCONTINUABLE;
		return EXCEPTION_CONTINUE_SEARCH;
	}

    // produce our output and terminate.
    VString errorMessage(VSTRING_ARGS("Fatal error 0x%X (%d) caught by _stackCrawlExceptionFilter.", (int) exceptionCode, (int) exceptionCode));

    StackCrawlerEx::LogFatal(errorMessage);

    StackCrawlerExSettings stackCrawlerSettings = {};

    stackCrawlerSettings.IsOnDemand             = false;

    stackCrawlerSettings.ProcessHandle          = ::GetCurrentProcess();
    stackCrawlerSettings.ThreadHandle           = ::GetCurrentThread();

    stackCrawlerSettings.Exceptions             = exceptionPointers;

    stackCrawlerSettings.Context                = boost::optional<CONTEXT>(*(exceptionPointers->ContextRecord));

    stackCrawlerSettings.DumpTypeToGenerate     = gDumpTypeToGenerate;
    stackCrawlerSettings.TinyDumpFileNamePrefix = gPrefixForTinyDumpFileName;
    stackCrawlerSettings.FullDumpFileNamePrefix = gPrefixForFullDumpFileName;

    stackCrawlerSettings.HasSymbols             = false;

    stackCrawlerSettings.DisplayProlog          = true;
    stackCrawlerSettings.DisplayMachineInfo     = true;
    stackCrawlerSettings.DisplayRegisters       = true;
    stackCrawlerSettings.DisplayStackFrames     = true;
    stackCrawlerSettings.DisplayParametersInfo  = true;

    if (gDumpTypeToGenerate == DumpType::None) {
        StackCrawlerEx::LogFatal(VSTRING_FORMAT("Dump type configuration - %s (%d) - prevents generation of dump file. Dump file will not be generated.", DumpTypeConverter::ToString(gDumpTypeToGenerate).c_str(), gDumpTypeToGenerate));
    }

    // TODO: On exception, the state of the current thread is undefined. It's better to do stack crawling in a separate (new) thread.
    if (gGenerateStdlog) {
        _writeCrashDumpToStandardLogFile(stackCrawlerSettings, VString::EMPTY(), true/*launch Notepad*/);
    }

#ifdef VAULT_STACK_CRAWLING_FOR_SERVER
    // 2011.03.22 tisaacson ARGO-30606 - Report abnormal exit to SCM to facilitate auto-restart.
    NotificationDispatcher::instance()->notify(NotificationDispatcher::kRuntimeFailure, errorMessage, exceptionCode);
#endif

    if (!gGenerateStdlog && (gDumpTypeToGenerate == DumpType::None)) {
        StackCrawlerEx::LogFatal("The exception handler is configured to produce neither stdlog nor .dmp file.");
    }

    StackCrawlerEx::CleanupDbgSymbols();

    exceptionPointers->ExceptionRecord->ExceptionFlags |= EXCEPTION_NONCONTINUABLE;

    return EXCEPTION_EXECUTE_HANDLER; // We have written our data, now let the OS (presumably) terminate the app
}

//--------------------------------------------------------------------------------
#ifndef VAULT_SIMPLE_USER_THREAD_MAIN
void* VThread::userThreadMain(void* arg) {
    /*
    All we have to do for a thread is register its base stack frame
    before calling the thread main, and deregister afterwards.
    */
    DWORD startingStackFrame = 0;
#ifndef VCOMPILER_64BIT
#ifndef _lint
    _asm mov startingStackFrame, ebp;
#endif /* _lint */
#endif /* VCOMPILER_64BIT */

    _registerThreadInfo(GetCurrentThreadId(), new StackCrawlThreadInfo(startingStackFrame));
    void* result = VThread::threadMain(arg);
    _deregisterThreadInfo(GetCurrentThreadId());

    return result;
}
#endif

Vs64 GetSetWorkingSetSize() {
    PROCESS_MEMORY_COUNTERS pmc;

    DWORD processID = ::GetCurrentProcessId();

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);

    if (NULL == hProcess) {
        return 0;
    }

    Vs64 workingSet = 0;

    if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
        workingSet = pmc.WorkingSetSize;
    }

    return workingSet;
}

void RegisterApplicationExceptionHandler(const std::string& prefixForTinyDumpFileName, const std::string& prefixForFullDumpFileName) {
    /*
    Should be called once - from the main method.

    For main, we register the starting stack frame, install the global exception
    handler (which will be used in the context of any thread that crashes), and
    then call the wrapped main function. There is no need to deregister since at
    the end of this function the app exits. We also set the Windows "error mode"
    to avoid display of a GUI dialog box in the event of a crash.
    */

    // Here's where to put a breakpoint if you want to attach to XPS at startup
    // It needs to be done before the SEH handler is intalled below
    // DebugBreak();

    gPrefixForTinyDumpFileName = VString(prefixForTinyDumpFileName.c_str());

    gPrefixForFullDumpFileName = VString(prefixForFullDumpFileName.c_str());

    // allocate some memory for the SEH handler in case of low memory conditions
    // in testing, 60M worked reliably so thats the default
    int kEmergencyMemorySize = 60 * 1024 * 1024; // megabytes
    gSEHEmergencyMemory = ::malloc(kEmergencyMemorySize);
    memset(gSEHEmergencyMemory, 0, kEmergencyMemorySize);

    gWorkingSetSizeAtStartup = GetSetWorkingSetSize();

    DWORD startingStackFrame = 0;

#ifndef VCOMPILER_64BIT
#ifndef _lint
    _asm mov startingStackFrame, ebp;
#endif /* _lint */
#endif /* VCOMPILER_64BIT */

    _registerThreadInfo(GetCurrentThreadId(), new StackCrawlThreadInfo(startingStackFrame));
    ::SetUnhandledExceptionFilter(_stackCrawlExceptionFilter);

    SetErrorMode(SEM_NOGPFAULTERRORBOX);
}

/**
We need this delegating function so that it can use a raw C char buffer; the calling function
uses VC++ __try/__catch structured exception handling so it can't use objects with destructors,
like VString.
*/
static void _callDumpFile(EXCEPTION_POINTERS* /*ep*/, bool isOnDemand, DumpType dumpType, const char* fileNamePrefix, size_t dumpFileFullPathBufferSize, /*OUT*/ char*& dumpFileFullPath) {
    VLogAppenderPtr appender(new VCoutLogAppender("crash-dump-log-appender", VLogAppender::DONT_FORMAT_OUTPUT));

    VNamedLoggerPtr logger(new VNamedLogger("crash-dump-logger", VLoggerLevel::TRACE, VStringVector(), appender));

    StackCrawlerExSettings stackCrawlerSettings = {};

    stackCrawlerSettings.IsOnDemand             = isOnDemand;

    stackCrawlerSettings.Logger                 = logger;

    stackCrawlerSettings.ProcessHandle          = ::GetCurrentProcess();
    stackCrawlerSettings.ThreadHandle           = ::GetCurrentThread();

    stackCrawlerSettings.Exceptions             = NULL; // Experimentally, if we supply ep here, the dump file write doesn't work.
                                                        // So now if we pass null it will pass 0 to the library call and we'll get a useful dump file.

    stackCrawlerSettings.DumpTypeToGenerate     = dumpType;
    stackCrawlerSettings.TinyDumpFileNamePrefix = VSTRING_FORMAT("%s_Tiny", fileNamePrefix);
    stackCrawlerSettings.FullDumpFileNamePrefix = VSTRING_FORMAT("%s_Full", fileNamePrefix);

    stackCrawlerSettings.HasSymbols = false;

    stackCrawlerSettings.DisplayProlog = false;
    stackCrawlerSettings.DisplayMachineInfo = false;
    stackCrawlerSettings.DisplayRegisters = false;
    stackCrawlerSettings.DisplayStackFrames = false;
    stackCrawlerSettings.DisplayParametersInfo = false;

    StackCrawlerEx stackCrawler(stackCrawlerSettings);

    StackCrawlerExSettings result = stackCrawler.WriteCrashDumpInfo(VString::EMPTY());

    ::strncpy_s(dumpFileFullPath, dumpFileFullPathBufferSize, result.Result.DumpFileName.chars(), static_cast<size_t>(result.Result.DumpFileName.length()));
}


/**
This function raises an exception so that we can get an exception record suitable
for passing to the dump file writer function. We intentionally declare the file name
prefix as a raw C char buffer because a function using VC++ __try/__catch structured
exceptions cannot use anything that has a destructor (like VString).
@param  fileNamePrefix  the prefix to be used when formatting the .dmp file name;
this allows us to distinguish between real "crash" dumps, and informational "shutdown"
dumps.
*/
#pragma warning(disable: 4706)
static void _triggerDumpFile(DumpType dumpType, const char* fileNamePrefix, size_t dumpFileFullPathBufferSize, /*OUT*/ char*& dumpFileFullPath) {
    EXCEPTION_POINTERS* ep = NULL;

    __try {
        RaiseException(1, 0, 0, NULL); // a continuable exception
    } __except (((ep = GetExceptionInformation()) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_EXECUTE_HANDLER)) {
        _callDumpFile(ep, true, dumpType, fileNamePrefix, dumpFileFullPathBufferSize, dumpFileFullPath);
    }
}
#pragma warning(default: 4706)

#elif defined VPLATFORM_MAC
// BEGIN MAC-SPECIFIC IMPLEMENTATION -----------------------------------------------
/*
This section is the Mac OS X implementation of the APIs. We don't need to install
system exception handlers, etc. because the system will produce crash dumps for us.
We just implement the stack crawl API so that we can log stack crawls at will.
*/

// Headers required to call backtrace() and friends.
#include <execinfo.h>
#include <dlfcn.h>
#include <cxxabi.h>

void VThread::logStackCrawl(const VString& headerMessage, VNamedLoggerPtr logger, bool verbose) {
    VNamedLoggerPtr cout;

    // point "logger" to the console if null was supplied
    if (logger == NULL) {
        VLogAppenderPtr coutAppender(new VCoutLogAppender("logStackCrawl.cout.appender", VLogAppender::DO_FORMAT_OUTPUT));
        cout.reset(new VNamedLogger("logStackCrawl.cout.logger", VLoggerLevel::TRACE, VStringVector(), coutAppender));
        logger = cout;
    }

    if (headerMessage.isNotEmpty())
        logger->emitStackCrawlLine(headerMessage);

    void* callstack[128];
    int frames = backtrace(callstack, 128);
    char** strs = backtrace_symbols(callstack, frames);
    for (int i = 1; i < frames; ++i) { // NOTE: i = 1 so that we skip THIS frame (logStackCrawl), which we don't need to show
        //printf("%s\n", strs[i]); <-- mangled name
        Dl_info info;
        int res = dladdr(callstack[i], &info);
        if (res != 0) {
            size_t length = 0;
            int status = 0;
            char* name = __cxxabiv1::__cxa_demangle(info.dli_sname, NULL, &length, &status);
            ptrdiff_t offset = ((char*)callstack[i]) - ((char*)info.dli_saddr);
            VString line(VSTRING_ARGS("%s + " VSTRING_FORMATTER_SIZE " %s", (0 == status) ? name : info.dli_sname, offset, (verbose ? info.dli_fname : "")));
            if (name != NULL) {
                free(name);
            }
            logger->emitStackCrawlLine(line);
        }
    }
    free(strs);

}

void* VThread::userThreadMain(void* arg) {
    return VThread::threadMain(arg);
}

int main(int argc, char** argv) {
    VMainThread mainThread;
    return mainThread.execute(argc, argv);
}

#else
// BEGIN UNIX-SPECIFIC IMPLEMENTATION -----------------------------------------------
/*
This section is the Unix implementation of the APIs. We don't need to install
system exception handlers, etc. because the system will produce crash dumps for us.
It may turn out that the stack crawl APIs we use above on Mac OS X are also well-defined
on generic Unix, but that still needs to be confirmed. Until then it's a no-op.
*/

void RegisterApplicationExceptionHandlerLinux(int signum) {
	/*
	This is the handler method to be registered which
	will be invoked upon an exception or crash
	*/
	const std::string& dumpFileName = DEFAULT_DUMP_FILE_NAME_WITH_PATH.chars();

	::signal(signum, SIG_DFL);

	// Check if there is any existing file and remove
	if (boost::filesystem::exists(dumpFileName.c_str())) {
		boost::filesystem::remove(dumpFileName.c_str());
	}

	// Capture the stack trace.
	boost::filesystem::path p{ dumpFileName.c_str() };
	boost::filesystem::ofstream outfilestream{ p };
	outfilestream << boost::stacktrace::stacktrace();

	outfilestream.close();

	if (signum != SIGABRT) {
		::signal(SIGABRT, NULL);
		::raise(SIGABRT);
	}
}

typedef std::map<Vu64, StackCrawlThreadInfo*> StackCrawlThreadInfoMap;
static StackCrawlThreadInfoMap gStackCrawlThreadInfoMap;
static VMutex gStackCrawlThreadInfoMapMutex("gStackCrawlThreadInfoMapMutex"); // 2011.04.28 tisaacson ARGO-31680 - Make access to map thread-safe.

static void _registerThreadInfo(Vu64 threadId, StackCrawlThreadInfo* info) {
    VMutexLocker locker(&gStackCrawlThreadInfoMapMutex, "_registerThreadInfo");

    // Complain if caller is attempting to register a thread that's already registered.
    if (gStackCrawlThreadInfoMap.find(threadId) != gStackCrawlThreadInfoMap.end()) {
        VLOGGER_ERROR(VSTRING_FORMAT("_registerThreadInfo: threadId %lu is already registered.", threadId));
        return;
    }

    gStackCrawlThreadInfoMap[threadId] = info;
}

static StackCrawlThreadInfo* const _lookupThreadInfo(Vu64 threadId) {
    VMutexLocker locker(&gStackCrawlThreadInfoMapMutex, "_lookupThreadInfo");

    StackCrawlThreadInfo* info = NULL;

    auto iter = gStackCrawlThreadInfoMap.find(threadId);

    if (iter != gStackCrawlThreadInfoMap.end()) {
        info = iter->second;
    }

    return info;
}

void _deregisterThreadInfo(Vu64 threadId) {
    VMutexLocker locker(&gStackCrawlThreadInfoMapMutex, "_deregisterThreadInfo");
    vault::mapDeleteOneValue(gStackCrawlThreadInfoMap, threadId);
}

//--------------------------------------------------------------------------------

StackCrawlThreadInfo::StackCrawlThreadInfo(Vu64 startingStackFrame) :
    fStartingStackFrame(startingStackFrame),
    fIsInExceptionHandler(false),
    fHasSymbols(false) {
}

StackCrawlThreadInfo::~StackCrawlThreadInfo() {
}

Vu64 StackCrawlThreadInfo::getStartingStackFrame() const {
    return fStartingStackFrame;
}

bool StackCrawlThreadInfo::isInExceptionHandler() const {
    return fIsInExceptionHandler;
}

void StackCrawlThreadInfo::enteringExceptionHandler() {
    fIsInExceptionHandler = true;
}

void StackCrawlThreadInfo::leavingExceptionHandler() {
    fIsInExceptionHandler = false;
}

bool StackCrawlThreadInfo::hasSymbols() const {
    return fHasSymbols;
}

void StackCrawlThreadInfo::setHasSymbols(bool state) {
    fHasSymbols = state;
}

//
// Struct StackCrawlerExSettings
//
bool StackCrawlerExSettings::CrashDumpEnabled() const {
    return (this->DumpTypeToGenerate != DumpType::None);
}

std::string StackCrawlerExSettings::GetDumpFileNamePrefix() const {
    std::string dumpFileName = "";

    if (this->DumpTypeToGenerate == DumpType::Tiny) {
        dumpFileName = this->TinyDumpFileNamePrefix;
    }
    else if (this->DumpTypeToGenerate == DumpType::Full) {
        dumpFileName = this->FullDumpFileNamePrefix;
    }

    return dumpFileName;
}

// 
// Class StackCrawlerEx
//
#ifdef VCOMPILER_64BIT
const VString StackCrawlerEx::POINTER_FORMATTER = VString("0x%016X");
#else
const VString StackCrawlerEx::POINTER_FORMATTER = VString("0x%08X        ");
#endif

StackCrawlerEx::StackCrawlerEx(const StackCrawlerExSettings& settings) : _settings(settings) {
}

StackCrawlerEx::~StackCrawlerEx() {
}

// This function should be used for anything we want to write to both the console
// and as a FATAL level notification as we are processing a crash.
/*static*/
void StackCrawlerEx::LogFatal(const VString& message) {
    std::cout << message << std::endl;
    VLOGGER_FATAL(message);
}

StackCrawlerExSettings StackCrawlerEx::Settings() const {
    return this->_settings;
}

StackCrawlerExSettings StackCrawlerEx::WriteCrashDumpInfo(const VString& headerMessage) {
    VString localTime;

    VInstant().getLocalString(localTime);

    VNamedLoggerPtr logger = _settings.Logger;

    logger->emitStackCrawlLine(VSTRING_FORMAT("%s BEGIN OUTPUT ------------------------------------------------------------------", localTime.chars()));

    if (headerMessage.isNotEmpty()) {
        logger->emitStackCrawlLine(headerMessage);
    }

    // To lessen the possibility that logging itself will fail and prevent the minidump,
    // we just collect log output as we go, then write the minidump, and only then do we
    // log the collected log output.
    if (_settings.DumpTypeToGenerate != DumpType::None) {
        _settings.Result = _writeMiniDumpToNewFile();

        if (_settings.IsOnDemand) {
            VInstant().getLocalString(localTime);

            logger->emitStackCrawlLine(VSTRING_FORMAT("%s Created on-demand crash dump:", localTime.chars()));

            logger->emitStackCrawlLine(_settings.Result.DumpFileName);
        }
    }

    if (_settings.DisplayProlog) {
        _writeCrashDumpProlog(_settings.Result);
    }

    if (_settings.DisplayMachineInfo) {
        _writeCrashDumpMachineInfo();
    }
    
    static const std::string CRAWLER_UNAVAILABLE_ERROR_MESSAGE("Unavailable as stack crawler is disabled for the target platform");
    static const std::string CRAWLER_STACKTRACE_INFO_MESSAGE("Call Stack is avaiable through the dump file created for the target platform");

    logger->emitStackCrawlLine((std::string("REGISTERS: ") + CRAWLER_UNAVAILABLE_ERROR_MESSAGE).c_str());
    if (_settings.DisplayStackFrames) {
        logger->emitStackCrawlLine((std::string("SYMBOLS: ") + CRAWLER_UNAVAILABLE_ERROR_MESSAGE).c_str());
        logger->emitStackCrawlLine((std::string("CALL STACK: ") + CRAWLER_STACKTRACE_INFO_MESSAGE).c_str());
        logger->emitStackCrawlLine((std::string("PARAMETERS: ") + CRAWLER_UNAVAILABLE_ERROR_MESSAGE).c_str());
    }

    VInstant().getLocalString(localTime);
    logger->emitStackCrawlLine("");
    logger->emitStackCrawlLine(VSTRING_FORMAT("%s END OUTPUT ------------------------------------------------------------------", localTime.chars()));
    logger->emitStackCrawlLine("");

    return _settings;
}

void StackCrawlerEx::_writeCrashDumpProlog(DumpCreationResult dumpCreationResult) {
    VNamedLoggerPtr logger = _settings.Logger;

    ApplicationInfo appInfo = GetApplicationInfo();

    // Tell the customer what steps to take.
    logger->emitStackCrawlLine("");
    logger->emitStackCrawlLine(VSTRING_FORMAT("%s crashed and produced this crash dump information file. Please submit a case through your Navis LLC support contact.", appInfo.ApplicationName.c_str()));
    if (_settings.CrashDumpEnabled() && (dumpCreationResult.DumpTypeCreated != DumpType::None)) {
        logger->emitStackCrawlLine("Attach the relevant log files and the crash dump file (specified below) from this machine from at least one hour before this crash.");
        logger->emitStackCrawlLine("NOTE: If this is a private/non-tagged build, please attach the full set of XPS/XPS-Client binaries.");
    }
    else {
        logger->emitStackCrawlLine("Attach the relevant log files from this machine from at least one hour before this crash. ");
    }
    logger->emitStackCrawlLine("Depending on the crash, it may also be necessary to obtain a backup of the data from a SPARCS client, along with a copy of the");
    logger->emitStackCrawlLine("XPS data folder and settings.xml.");
    logger->emitStackCrawlLine("");

    if (_settings.CrashDumpEnabled()) {
        logger->emitStackCrawlLine("CRASH DUMP FILE:");
        if (dumpCreationResult.DumpTypeCreated != DumpType::None) {
            logger->emitStackCrawlLine(dumpCreationResult.DumpFileName);
        }
        else {
            logger->emitStackCrawlLine("Failed to create crash dump file.");
        }
    }
    else {
        logger->emitStackCrawlLine("Not configured to create dump file. Dump file was NOT created.");
    }

    logger->emitStackCrawlLine("");
    logger->emitStackCrawlLine("GENERAL INFORMATION:");
    logger->emitStackCrawlLine(VSTRING_FORMAT("  Current thread id          : 0x%x", pthread_self()));
    logger->emitStackCrawlLine(VSTRING_FORMAT("  Program Version            : %s, %s, build date %s", appInfo.VersionString.c_str(), appInfo.SVNRevision.c_str(), appInfo.BuildTimeStamp.c_str()));

    VInstant now;
    VDuration d = now - gStartTime;
    logger->emitStackCrawlLine(VSTRING_FORMAT("  Local time program started : %s", gStartTime.getLocalString().chars()));
    logger->emitStackCrawlLine(VSTRING_FORMAT("  Current local time         : %s", now.getLocalString().chars()));

    VString timeString(VSTRING_ARGS("%dd %dh %dm %d.%03llds", d.getDurationDays(), )
        d.getDurationHours() % 24, d.getDurationMinutes() % 60, d.getDurationSeconds() % 60,
        d.getDurationMilliseconds() % CONST_S64(1000));
    logger->emitStackCrawlLine(VSTRING_FORMAT("  Elapsed time               : %s", timeString.chars()));
}

void StackCrawlerEx::_writeCrashDumpMachineInfo() {
    VString msg; // used to format output strings below

    VNamedLoggerPtr logger = _settings.Logger;

    logger->emitStackCrawlLine("");
    logger->emitStackCrawlLine("OS and MEMORY:");

    struct utsname name;
    if (uname(&name)) {
        exit(-1);
    }

    struct sysinfo info;
    sysinfo(&info);

    msg.format("  Platform                      : OS is %s and release %s", name.sysname, name.release);
    logger->emitStackCrawlLine(msg);

    //Write physical, virtual and harddisk memory info

    //---
    boost::filesystem::space_info si = boost::filesystem::space(".");
    //---
    unsigned num_cpu = std::thread::hardware_concurrency();
    //---
    std::string cputemp = "/sys/class/thermal/thermal_zone0/temp";
    std::string cpufrequency = "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq";
    boost::filesystem::ifstream cpu_temp(cputemp);
    boost::filesystem::ifstream  cpu_freq(cpufrequency);

    //---
    std::string cpunumber = std::to_string(num_cpu);
    std::string mem_size = std::to_string(((size_t)info.totalram * (size_t)info.mem_unit) / (1024 * 1024)); // in MB
    std::string disk_capacity = std::to_string(si.capacity / (1024 * 1024));
    std::string disk_available = std::to_string(si.available / (1024 * 1024) );
    float available  = (si.available / (1024 * 1024));
    float capacity = (si.capacity / (1024 * 1024));

    float fraction = 0.0;
    if (capacity != 0) {
        fraction = available / capacity;
    }
    std::string fslevel = std::to_string(fraction * 100);
    //--- 

    // Log System IP
    VString ipAddr;
    VSocket::getLocalHostIPAddress(ipAddr);
    msg.format("  System IP Address             : %s", ipAddr.chars());
    logger->emitStackCrawlLine(msg);
    msg.format("  Number of CPU(s)              : %s", cpunumber.c_str());
    logger->emitStackCrawlLine(msg);

    msg.format("  Total RAM Installed           : %s MB", mem_size.c_str());
    logger->emitStackCrawlLine(msg);

    msg.format("  Total Physical Disk space     : %s MB", disk_capacity.c_str());
    logger->emitStackCrawlLine(msg);

    msg.format("  Free Physical Disk space      : %s MB (%s%c used)", disk_available.c_str(), fslevel.c_str(), '%');
    logger->emitStackCrawlLine(msg);

    msg.format("  Total Virtual Mememory        : %lf MB", static_cast<double>((size_t)info.totalswap / (1024 * 1024)));
    logger->emitStackCrawlLine(msg);

    msg.format("  Virtual Memory Used           : %lf MB", static_cast<double>((size_t)(info.totalswap- info.freeswap) / (1024 * 1024)));
    logger->emitStackCrawlLine(msg);

    double virtualMemUsedFraction = 0.0;
    if (info.totalswap != 0) {
        virtualMemUsedFraction = static_cast<double> ((size_t)(info.totalswap - info.freeswap) / (1024 * 1024)) / static_cast<double> (info.totalswap / (1024 * 1024));
    }
    double percentage = 100.0 * (virtualMemUsedFraction);
 
    msg.format("  Free virtual Memory Available  : %lf MB (%lf%c used)", (static_cast<double>((size_t)(info.freeswap)) / (1024 * 1024)), percentage, '%');
    logger->emitStackCrawlLine(msg);

    if ((gCurrentScriptLineNumber != 0) && gCurrentScriptCommand.isNotEmpty()) {
        msg.format("  Last Script Command Executed at (Line#%d) :", gCurrentScriptLineNumber);
        logger->emitStackCrawlLine(msg);
        msg.format("    %s", gCurrentScriptCommand.chars());
        logger->emitStackCrawlLine(msg);
    }
}

/**
Writes a minidump file containing the state of the application.
@param  exceptionPointers   if not null, will be used when calling MiniDumpWriteDump;
if NULL, will pass NULL (you should supply null if this is not a real crash,
because the experimental behavior is that we fail on the pointer obtained in _triggerDumpFile)
@param  fileNamePrefix      the string to prefix the _<timstamp>.dmp part of the file name;
pass "crashdump" or whatever for real crash, "shutdown" etc. otherwise, to distinguish the files
*/
DumpCreationResult StackCrawlerEx::_writeMiniDumpToNewFile() {
    ApplicationInfo appInfo = GetApplicationInfo();

    DumpCreationResult result = DumpCreationResult();

    VString fileNamePrefix(_settings.GetDumpFileNamePrefix().c_str());

    VInstant now;
    VString dumpFileName(VSTRING_ARGS("%s_%s.dmp", fileNamePrefix.chars(), now.getLocalString(true).chars()));
    VString dumpFilePath;
    VFSNode::getExecutableDirectory().getChildPath(dumpFileName, dumpFilePath);

    VStringVector loggerOutput;
    loggerOutput.push_back(VSTRING_FORMAT("_writeMiniDumpToNewFile: Creating %s dump file '%s'...", DumpTypeConverter::ToString(_settings.DumpTypeToGenerate).c_str(), dumpFilePath.chars()));

    // Check if there is any existing file and remove
    if (boost::filesystem::exists(dumpFilePath.chars())) {
        loggerOutput.push_back(VSTRING_FORMAT("_writeMiniDumpToNewFile: Dump file '%s'. already exists", dumpFilePath.chars()));
    }

    boost::filesystem::path p{ dumpFileName.chars() };
    boost::filesystem::ofstream outfilestream{ p };

    {
        VStringVector customStrings;
        VDuration d = now - gStartTime;
        VString timeString(VSTRING_ARGS("%dd %dh %dm %d.%03llds", d.getDurationDays(), )
            d.getDurationHours() % 24, d.getDurationMinutes() % 60, d.getDurationSeconds() % 60,
            d.getDurationMilliseconds() % CONST_S64(1000));

        customStrings.push_back(VSTRING_FORMAT("Program Version            : %s, build date %s", appInfo.VersionString.c_str(), appInfo.BuildTimeStamp.c_str()));
        customStrings.push_back(VSTRING_FORMAT("Local time program started : %s", gStartTime.getLocalString().chars()));
        customStrings.push_back(VSTRING_FORMAT("Current local time         : %s", now.getLocalString().chars()));
        customStrings.push_back(VSTRING_FORMAT("Elapsed time               : %s", timeString.chars()));
        customStrings.push_back(VSTRING_FORMAT("Current Thread Id          : %ld", pthread_self()));
        DumpType dumpToCreate = _settings.DumpTypeToGenerate;

        bool dumpSuccessfullyCreated = false;

        if (dumpToCreate == DumpType::Full || dumpToCreate == DumpType::Tiny) {

            // Write all the customStrings entries to the dump file
            for (VStringVector::const_iterator i = customStrings.begin(); i != customStrings.end(); ++i) {
                if (_settings.IsOnDemand) { // Not a fatal error, just a requested shutdown dump.
                    VLOGGER_INFO(*i);
                }
                else {
                    StackCrawlerEx::LogFatal(*i);
                }
                outfilestream << *i << std::endl;
            }
            dumpSuccessfullyCreated = true;

            // Capture the stack trace.
            outfilestream << boost::stacktrace::stacktrace() << std::endl;

            if (dumpToCreate == DumpType::Full) {
                // Currently No Symbols and Parameters are being generated 
                // as we don't have Stackwalker to walk through each stack frame.
                // Ofcourse we don't have registers here like in windows.

                // TODO: Can explore using Boost Stack Frame dump which may not be 
                // equivalent.
                auto buf = std::array<char, MAX_STACK_TRACE_BUFFER_SIZE>{};
                const auto size = boost::stacktrace::safe_dump_to(buf.data(), buf.size());
                const auto st = boost::stacktrace::stacktrace::from_dump(buf.data(), size);

                // Appends Frames Info the the dump file.
                outfilestream << "Frames Dump :" << st << std::endl;
            }

            if (!dumpSuccessfullyCreated) {
                loggerOutput.push_back(VString("_writeMiniDumpToNewFile: Failed to create full dump. Will attempt to create mini dump..."));
                dumpToCreate = DumpType::Tiny; // Might have failed due to disk-space issues. No harm in retrying with tiny dump.
            }
        }

        if (dumpSuccessfullyCreated) {
            loggerOutput.push_back(VSTRING_FORMAT("_writeMiniDumpToNewFile: Wrote file '%s'.", dumpFilePath.chars()));

            result.DumpTypeCreated = dumpToCreate;
            result.DumpFileName = dumpFilePath;
        }
        else if (dumpToCreate != DumpType::None) {
            loggerOutput.push_back(VSTRING_FORMAT("_writeMiniDumpToNewFile: Failed to write file '%s'. Error: %u", dumpFilePath.chars(), errno));
        }

    }

    for (VStringVector::const_iterator i = loggerOutput.begin(); i != loggerOutput.end(); ++i) {
        if (_settings.IsOnDemand) { // Not a fatal error, just a requested shutdown dump.
            VLOGGER_INFO(*i);
        }
        else {
            StackCrawlerEx::LogFatal(*i);
        }
    }

    return result;
}

static void _writeCrashDumpInfoToLogger(StackCrawlerExSettings& settings, const VString& headerMessage) {
    VNamedLoggerPtr logger = settings.Logger;

    // Protect against recursion should any of our dump code itself "crash":
    // If we have set up StackCrawlThreadInfo in response to an actual crash,
    // mark it "in progress" here; if we see it's already in progress, stop.
    // If we haven't setup StackCrawlThreadInfo, it means we're being called
    // not from an actual crash, but from a command, so proceed.
    StackCrawlThreadInfo* threadInfo = _lookupThreadInfo(pthread_self());

    if (threadInfo == NULL) {
        logger->emitStackCrawlLine(VSTRING_FORMAT("UNABLE TO FIND STACK CRAWL INFO FOR CURRENT THREAD ID %d", pthread_self()));
        logger->emitStackCrawlLine(headerMessage);
    }

    StackCrawlerEx stackCrawler(settings);

    stackCrawler.WriteCrashDumpInfo(headerMessage);
}

//This is called when an unhandled exception is thrown and generates a stack crawl which is
//written to STDLOG.TXT.
static void _writeCrashDumpToStandardLogFile(StackCrawlerExSettings& settings, const VString& headerMessage, bool launchViewer) {
    static VMutex mutex("crashDumpMutex");

    VMutexLocker lock(&mutex, "writeCrashDumpToStandardLogFile()");

    VStringVector loggerOutput;

    VFSNode crashLogFile;

    VFSNode::getExecutableDirectory().getChildNode(LOG_FILE_NAME, crashLogFile);

    try {
        VBufferedFileStream stream(crashLogFile);

        stream.openReadWrite();
    }
    catch (const VException& ex) {
        StackCrawlerEx::LogFatal(VSTRING_FORMAT("Unable to open crash log file '%s'. Aborting crash dump: %s", crashLogFile.getPath().chars(), ex.what()));
        return;
    }

    try {
        VLogAppenderPtr appender(new VFileLogAppender("crash-log-appender", VLogAppender::DONT_FORMAT_OUTPUT, crashLogFile.getPath()));

        VNamedLoggerPtr logger(new VNamedLogger("crash-logger", VLoggerLevel::TRACE, VStringVector(), appender));

        settings.Logger = logger;

        loggerOutput.push_back(VSTRING_FORMAT("_writeCrashDumpToStandardLogFile: Writing file header message  into '%s'...", crashLogFile.getPath().chars()));

        loggerOutput.push_back(VSTRING_FORMAT("_writeCrashDumpToStandardLogFile: Header Messgae: '%s'.", headerMessage.chars()));

        StackCrawlerEx stackCrawler(settings);

        stackCrawler.WriteCrashDumpInfo(headerMessage);
    }
    catch (const std::exception& ex) {
        loggerOutput.push_back(VSTRING_FORMAT("Error writing crash log file '%s': %s", crashLogFile.getPath().chars(), ex.what()));
    }

    for (VStringVector::const_iterator i = loggerOutput.begin(); i != loggerOutput.end(); ++i) {
        StackCrawlerEx::LogFatal(*i);
    }

    // Note that we don't launch the viewer until the VRawFileLogger writing to it has closed.
    // Otherwise some apps complain about the file being open by us.
    if (launchViewer && gStdlogViewerApp.isNotEmpty()) {
        VString crashLogFileNativePath = crashLogFile.getPath();

        VFSNode::denormalizePath(crashLogFileNativePath);

        // Verify that crash dump file exists.
        // Launching the Dump file viewer is windows specific process.
        //_launchCrashDumpViewer(crashLogFileNativePath);
    }
}

// If toLogger is NULL, we will log to the crash log file, not the logger.
static void _generateStackCrawlLinux(VNamedLoggerPtr toLogger, const VString& headerMessage, bool isOnDemand,
    bool displayProlog, bool displayMachineInfo, bool displayStackCrawl, bool displayParametersInfo, DumpType dumpType, const char* fileNamePrefix, bool launchViewer) {

    StackCrawlerExSettings stackCrawlerSettings = {};

    stackCrawlerSettings.DumpTypeToGenerate = dumpType;
    stackCrawlerSettings.TinyDumpFileNamePrefix = VSTRING_FORMAT("%s_Tiny", fileNamePrefix);
    stackCrawlerSettings.FullDumpFileNamePrefix = VSTRING_FORMAT("%s_Full", fileNamePrefix);

    stackCrawlerSettings.IsOnDemand = isOnDemand;
    stackCrawlerSettings.Logger = toLogger;

    stackCrawlerSettings.HasSymbols = false;    // It's OK even if ::SymInitialize is already called.

    stackCrawlerSettings.DisplayProlog = displayProlog;
    stackCrawlerSettings.DisplayMachineInfo = displayMachineInfo;
    stackCrawlerSettings.DisplayStackFrames = displayStackCrawl;
    stackCrawlerSettings.DisplayParametersInfo = displayParametersInfo;

    if (toLogger == NULL || gGenerateStdlog) {
        _writeCrashDumpToStandardLogFile(stackCrawlerSettings, headerMessage, launchViewer);
    }
    else {
        _writeCrashDumpInfoToLogger(stackCrawlerSettings, headerMessage);
    }
}

/**
We need this delegating function so that it can use a raw C char buffer; the calling function
uses C++ __try/__catch structured exception handling so it can't use objects with destructors,
like VString.
*/
static void _callDumpFile(bool isOnDemand, DumpType dumpType, const char* fileNamePrefix, size_t dumpFileFullPathBufferSize, /*OUT*/ char*& dumpFileFullPath) {
    VLogAppenderPtr appender(new VCoutLogAppender("crash-dump-log-appender", VLogAppender::DONT_FORMAT_OUTPUT));

    VNamedLoggerPtr logger(new VNamedLogger("crash-dump-logger", VLoggerLevel::TRACE, VStringVector(), appender));

    StackCrawlerExSettings stackCrawlerSettings = {};

    stackCrawlerSettings.IsOnDemand             = isOnDemand;

    stackCrawlerSettings.Logger                 = logger;

    stackCrawlerSettings.DumpTypeToGenerate     = dumpType;
    stackCrawlerSettings.TinyDumpFileNamePrefix = VSTRING_FORMAT("%s_Tiny", fileNamePrefix);
    stackCrawlerSettings.FullDumpFileNamePrefix = VSTRING_FORMAT("%s_Full", fileNamePrefix);

    stackCrawlerSettings.HasSymbols = false;

    stackCrawlerSettings.DisplayProlog = true;
    stackCrawlerSettings.DisplayMachineInfo = true;
    stackCrawlerSettings.DisplayParametersInfo = false;

    StackCrawlerEx stackCrawler(stackCrawlerSettings);

    StackCrawlerExSettings result = stackCrawler.WriteCrashDumpInfo(VString::EMPTY());

    std::strncpy(dumpFileFullPath, result.Result.DumpFileName.chars(), static_cast<size_t>(result.Result.DumpFileName.length()));
}

std::mutex VThread::logStackCrawlSynchronizer;

void VThread::logStackCrawl(const VString& headerMessage, VNamedLoggerPtr logger, bool /*verbose*/) {
    VNamedLoggerPtr cout;

    // point "logger" to the console if null was supplied
    if (logger == NULL) {
        VLogAppenderPtr coutAppender(new VCoutLogAppender("logStackCrawl.cout.appender", VLogAppender::DO_FORMAT_OUTPUT));
        cout.reset(new VNamedLogger("logStackCrawl.cout.logger", VLoggerLevel::TRACE, VStringVector(), coutAppender));
        logger = cout;
    }

    if (headerMessage.isNotEmpty())
        logger->emitStackCrawlLine(headerMessage);

    VInstant now;
    VString localTime = now.getLocalString();
    VString message(VSTRING_ARGS("[%s] (Stack crawl is partially supported on this platform.) %s", localTime.chars(), headerMessage.chars()));

    logger->emitStackCrawlLine(message);

    std::lock_guard<std::mutex> lock(logStackCrawlSynchronizer);

    Vu64 threadId = pthread_self();

    StackCrawlThreadInfo* threadInfo = _lookupThreadInfo(threadId);

    bool threadInfoAdded = false;

    if (threadInfo == NULL) {
        _registerThreadInfo(pthread_self(), new StackCrawlThreadInfo(0));

        threadInfo = _lookupThreadInfo(pthread_self());

        threadInfoAdded = true;
    }

    _generateStackCrawlLinux(logger, headerMessage, true, true, true, true, true, 
        DumpType::Tiny, StackCrawl::DEFAULT_FREEZE_DUMP_FILE_NAME_PREFIX.chars(), false /*do not launch Notepad*/);

    if (threadInfoAdded) {
        _deregisterThreadInfo(threadId);
    }
}

void* VThread::userThreadMain(void* arg) {
	const std::string& dumpFileName = DEFAULT_DUMP_FILE_NAME_WITH_PATH.chars();

	if (boost::filesystem::exists(dumpFileName)) {
		boost::filesystem::remove(dumpFileName);
	}
	::signal(SIGINT, &RegisterApplicationExceptionHandlerLinux);
	::signal(SIGSEGV, &RegisterApplicationExceptionHandlerLinux);
	::signal(SIGABRT, &RegisterApplicationExceptionHandlerLinux);
	return VThread::threadMain(arg);
}

#endif /* platform specific switching */

namespace StackCrawl {
    VString generateLiveDmp(DumpType dumpType, const VString& fileNamePrefix) {
        VString dumpFileFullPath = "";

        const size_t DUMP_FILE_FULL_PATH_BUFFER_SIZE = 1024;

        char cpDumpFileFullPath[DUMP_FILE_FULL_PATH_BUFFER_SIZE] = { 0 };

        char* outDumpFilePath = &cpDumpFileFullPath[0];

#ifdef VPLATFORM_WIN
        _triggerDumpFile(dumpType, fileNamePrefix, DUMP_FILE_FULL_PATH_BUFFER_SIZE, outDumpFilePath);
#else
        _callDumpFile(true, dumpType, fileNamePrefix, DUMP_FILE_FULL_PATH_BUFFER_SIZE, outDumpFilePath);
        VLOGGER_ERROR(VSTRING_FORMAT("generateLiveDump(%s): Live dump is partially supported on non-windows.", fileNamePrefix.chars()));
#endif

        VLOGGER_INFO(VSTRING_FORMAT("Wrote %s .dmp file.", fileNamePrefix.chars()));

        dumpFileFullPath = VString(cpDumpFileFullPath);

        return dumpFileFullPath;
    }
} // namespace StackCrawl
