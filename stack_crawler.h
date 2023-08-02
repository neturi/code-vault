/*
 * Copyright (c) 1989-2007 Navis LLC. All rights reserved.
 * $Id: stack_crawler.h,v 1.5 2008-08-16 00:33:47 abarr Exp $
 */

#ifndef navis_stack_crawler
#define navis_stack_crawler

#include "vstring.h"
#include "vlogger.h"

#ifdef VPLATFORM_WIN
// BEGIN WINDOWS-SPECIFIC IMPLEMENTATION -----------------------------------------------
#include <iostream>
#include <fstream>
#include <map>
#include <windows.h>

#define _NO_CVCONST_H

#include "DbgHelp.h"
#include <sys/timeb.h>
#include <time.h>
#include <sstream>

#endif

#ifndef VPLATFORM_WIN
#define BOOST_STACKTRACE_USE_BACKTRACE
#define BOOST_STACKTRACE_USE_ADDR2LINE

#include <signal.h>     // ::signal, ::raise
#include <iostream>
#include <boost/stacktrace.hpp>
#include <boost/filesystem.hpp>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <unistd.h>

static const Vu16     MAX_STACK_TRACE_BUFFER_SIZE = 8192;
static const VString  DEFAULT_DUMP_FILE_NAME_WITH_PATH = "./XPS-Crash-Dump.dump";
#endif

enum class DumpType {
    None = 0,
    Tiny = 1,
    Full = 2,
};

class DumpTypeConverter {
private:
    DumpTypeConverter();

public:
    DumpTypeConverter(const DumpTypeConverter& other) = delete;
    DumpTypeConverter& operator=(const DumpTypeConverter& other) = delete;

    static int ToInteger(DumpType dumpType);

    static DumpType FromInteger(int dumpTypeAsInteger);

    static std::string ToString(DumpType dumpType);

    static DumpType FromString(const std::string& dumpTypeAsString);
};

// This namespace defines the public API for managing the crash handling / stack crawl facility.
// Note that the implementation only does what it can for the target platform.
namespace StackCrawl {
	/**
	Following method is not thread-safe.
	*/
	extern void configureCrashHandler(bool generateStdlog, DumpType dumpTypeToGenerate, const VString& stdlogViewerApp);
	extern const VString& getStdlogViewerApp();
	extern DumpType getCurrentCrashDumpType();
	extern DumpType getDefaultCrashDumpType();
	/**
	Following method is not thread-safe.
	*/
	extern DumpType setCrashDumpType(DumpType dumpType);
	extern void setScriptCommandAndLineNumberForStackCrawl(const VString& inCommand, int inLine); // 2009.04.13 sjain ARGO-15351
	extern VString generateLiveDmp(DumpType dumpType, const VString& fileNamePrefix);

	/**
	Following method is not thread-safe.
	*/
	extern void configureAutoDumpOnPossibleFreeze(bool autoDumpOnPossibleFreezeEnabled, Vu16 zeroThroughputToleranceInMinutes, const VString& freezeDumpFileNamePrefix);
	extern bool isAutoDumpOnPossibleFreezeEnabled();
	extern Vu16 getZeroThroughputToleranceInMinutes();
	extern VString getFreezeDumpFileNamePrefix();
	/**
	Following method is not thread-safe.
	*/
	extern void enableOrDisableAutoDumpOnPossibleFreeze(bool enable);

	extern const VString DEFAULT_TINY_DUMP_FILE_NAME_PREFIX;
	extern const VString DEFAULT_FULL_DUMP_FILE_NAME_PREFIX;
	extern const VString DEFAULT_ON_DEMAND_DUMP_FILE_NAME_PREFIX;

	extern const DumpType DEFAULT_DUMP_TYPE;

	extern const VString DEFAULT_DUMP_TYPE_AS_STRING;

	extern const VString DEFAULT_FREEZE_DUMP_FILE_NAME_PREFIX;
	extern const bool DEFAULT_AUTO_DUMP_ON_POSSIBLE_FREEZE_ENABLED;
	extern const Vu16 DEFAULT_ZERO_THROUGHPUT_TOLERANCE_IN_MINUTES;
	extern const Vu16 MINIMUM_ZERO_THROUGHPUT_MINUTES;
	extern const Vu16 MAXIMUM_ZERO_THROUGHPUT_MINUTES;
}

struct DumpCreationResult {
public:
    DumpCreationResult() : DumpTypeCreated(DumpType::None), DumpFileName("") {
    }

    DumpCreationResult(DumpType dumpTypeCreated, const VString& dumpFileName) : DumpTypeCreated(dumpTypeCreated),
        DumpFileName(dumpFileName) {
    }

public:
    DumpType    DumpTypeCreated;
    VString     DumpFileName;
};

#ifdef VPLATFORM_WIN
// BEGIN WINDOWS-SPECIFIC IMPLEMENTATION -----------------------------------------------

class StackCrawlThreadInfo { 
public:
    StackCrawlThreadInfo(DWORD startingStackFrame);

    ~StackCrawlThreadInfo();

    DWORD getStartingStackFrame() const;

    bool isInExceptionHandler() const;

    void enteringExceptionHandler();

    void leavingExceptionHandler();

    bool hasSymbols() const;

    void setHasSymbols(bool state);

private:
    DWORD fStartingStackFrame;
    bool fIsInExceptionHandler;
    bool fHasSymbols;
};

enum StackCrawlerPrimitiveType {
    SC_NoType          = 0,
    SC_Void            = 1,
    SC_Char            = 2,
    SC_WChar           = 3,
    SC_Int             = 6,
    SC_UInt            = 7,
    SC_Float           = 8,
    SC_BCD             = 9,
    SC_Bool            = 10,
    SC_Long            = 13,
    SC_ULong           = 14,
    SC_Currency        = 25,
    SC_Date            = 26,
    SC_Variant         = 27,
    SC_Complex         = 28,
    SC_Bit             = 29,
    SC_BSTR            = 30,
    SC_Hresult         = 31
};

struct StackCrawlerParameterInfo {
public:
    int                         FunctionId;
    DWORD64                     Address;
    VString                     Name;
    VString                     TypeName;
    VString                     ValueAsString;

    static const VString        UNKNOWN_TYPE_OR_VALUE;
};

typedef boost::container::vector<StackCrawlerParameterInfo> ParameterInfoList;

typedef boost::shared_ptr<ParameterInfoList> ParameterInfoListPtr;

class StackCrawlerEx;

// Passed as user argument to ::SymEnumSymbols API.
// Contains the context & information necessary to continue with the parsing when the API returns.
struct EnumerateSymbolsCallbackParam {
public:
    EnumerateSymbolsCallbackParam(int functionId, StackCrawlerEx* instance, const STACKFRAME64& stackFrame, ParameterInfoListPtr parameters);

    int                     FunctionId;

    StackCrawlerEx*         StackCrawlerInstance;
    
    ParameterInfoListPtr    Parameters;
    STACKFRAME64            StackFrame;
};

// A wrapper to define the stack cralwer settings.
// Avoids the number of arguments that we'd need to pass around.
struct StackCrawlerExSettings {
public:
    bool                        IsOnDemand;

    VNamedLoggerPtr Logger;

    HANDLE                      ProcessHandle;
    HANDLE                      ThreadHandle;

    _EXCEPTION_POINTERS*        Exceptions;

    boost::optional<CONTEXT>    Context;

    DumpType                    DumpTypeToGenerate;

    VString                     TinyDumpFileNamePrefix;

    VString                     FullDumpFileNamePrefix;

    bool                        HasSymbols;

    bool                        DisplayProlog;
    bool                        DisplayMachineInfo;
    bool                        DisplayRegisters;
    bool                        DisplayStackFrames;
    bool                        DisplayParametersInfo;

    DumpCreationResult          Result;

public:
    bool CrashDumpEnabled() const;

    std::string GetDumpFileNamePrefix() const;
};

// Encapsulated most of the stack-crawling functionalities into this class.
// This would help us to extend the functionality easily in the future.
// Also, we could modify this class to just get the stack frame information and log/print the information
// later - at the user's discretion - and avoid expensive logging calls. This would be useful especially
// when we are collecting stack info while debugging time-critical issues.
// This class doesn't have to be thread-safe. The function _writeCrashDumpToStandardLogFile already synchronizes
// the stack crawling and the crawling is done through a new instance of StackCrawlerEx.
class StackCrawlerEx {
public:
    StackCrawlerEx(const StackCrawlerExSettings& settings);
    ~StackCrawlerEx();

    static bool InitDbgSymbols();
    static void CleanupDbgSymbols();

    static HANDLE GetOSVersionInfo(OSVERSIONINFOEX& osvi);

    static void LogFatal(const VString& message);

    StackCrawlerExSettings Settings() const;

    StackCrawlerExSettings WriteCrashDumpInfo(const VString& headerMessage);

private:
    //StackCrawlerExSettings _generateStackCrawlWithRegisters(const VString& headerMessage); NOT IN USE - FOR NOW.

    void _writeCrashDumpProlog(DumpCreationResult dumpCreationResult);
    void _writeCrashDumpMachineInfo();
    void _writeCrashDumpRegisters();

    // For Stack frames
    void _writeCrashDumpStackFrames();

    bool _parseFunctionInfo(int functionId, DWORD machineType, STACKFRAME64& stackFrame, ParameterInfoListPtr parameters, bool continueStackWalk);

    bool _parseStackParameters(int functionId, STACKFRAME64 stackFrame, ParameterInfoListPtr parameters);

    bool _handleEnumeratedParameter(int functionId, STACKFRAME64 stackFrame, PSYMBOL_INFO pSymbolInfo, ParameterInfoListPtr parameters);

    bool _parseParameterTypeAndValue(const STACKFRAME64& stackFrame, PSYMBOL_INFO pSymbolInfo, bool resolvingPointerType, DWORD pointerType, 
                                        StackCrawlerParameterInfo& parameterInfo, enum SymTagEnum& tag);

    static int __stdcall EnumerateSymbolsCallback(PSYMBOL_INFO pSymbolInfo, unsigned long size, void* pParam);

    void _getParameterTypeAndValue(const StackCrawlerPrimitiveType& primitiveType, const STACKFRAME64& stackFrame, bool isPointerType, enum SymTagEnum tag, 
                                    ULONG64 parameterSize, ULONG64 symbolAddress, StackCrawlerParameterInfo& parameterInfo);
    
    void _parseIntegerValue(void* pAddress, ULONG64 parameterSize, bool isSigned, bool ignoreValue, StackCrawlerParameterInfo& parameterInfo);
    void _parseFloatValue(void* pAddress, ULONG64 parameterSize, bool ignoreValue, StackCrawlerParameterInfo& parameterInfo);

    static void _convertContextToStackFrame(const CONTEXT& context, STACKFRAME64& stackFrame);

    // For tiny/full dump
    DumpCreationResult          _writeMiniDumpToNewFile();
    MINIDUMP_USER_STREAM*   _allocateCustomMiniDumpStrings(const VStringVector& elements);

public:
    static const VString                    POINTER_FORMATTER;

private:
    StackCrawlerExSettings                  _settings;
};

// END WINDOWS-SPECIFIC IMPLEMENTATION -----------------------------------------------
#else

class StackCrawlThreadInfo {
public:
    StackCrawlThreadInfo(Vu64 startingStackFrame);

    ~StackCrawlThreadInfo();

    Vu64 getStartingStackFrame() const;

    bool isInExceptionHandler() const;

    void enteringExceptionHandler();

    void leavingExceptionHandler();

    bool hasSymbols() const;

    void setHasSymbols(bool state);

private:
    Vu64 fStartingStackFrame;
    bool fIsInExceptionHandler;
    bool fHasSymbols;
};

struct StackCrawlerParameterInfo {
public:
    int                         FunctionId;
    Vu64                        Address;
    VString                     Name;
    VString                     TypeName;
    VString                     ValueAsString;

    static const VString        UNKNOWN_TYPE_OR_VALUE;
};

typedef boost::container::vector<StackCrawlerParameterInfo> ParameterInfoList;

typedef boost::shared_ptr<ParameterInfoList> ParameterInfoListPtr;

class StackCrawlerEx;

// A wrapper to define the stack cralwer settings.
// Avoids the number of arguments that we'd need to pass around.
struct StackCrawlerExSettings {
public:
    bool                        IsOnDemand;

    VNamedLoggerPtr Logger;

    DumpType                    DumpTypeToGenerate;

    VString                     TinyDumpFileNamePrefix;

    VString                     FullDumpFileNamePrefix;

    bool                        HasSymbols;

    bool                        DisplayProlog;
    bool                        DisplayMachineInfo;
    bool                        DisplayStackFrames;
    bool                        DisplayParametersInfo;

    DumpCreationResult          Result;

public:
    bool CrashDumpEnabled() const;

    std::string GetDumpFileNamePrefix() const;
};

// Encapsulated most of the stack-crawling functionalities into this class.
// This would help us to extend the functionality easily in the future.
// Also, we could modify this class to just get the stack frame information and log/print the information
// later - at the user's discretion - and avoid expensive logging calls. This would be useful especially
// when we are collecting stack info while debugging time-critical issues.
// This class doesn't have to be thread-safe. The function _writeCrashDumpToStandardLogFile already synchronizes
// the stack crawling and the crawling is done through a new instance of StackCrawlerEx.
class StackCrawlerEx {
public:
    StackCrawlerEx(const StackCrawlerExSettings& settings);
    ~StackCrawlerEx();

    static void LogFatal(const VString& message);

    StackCrawlerExSettings Settings() const;

    StackCrawlerExSettings WriteCrashDumpInfo(const VString& headerMessage);

private:

    void _writeCrashDumpProlog(DumpCreationResult dumpCreationResult);
    void _writeCrashDumpMachineInfo();

    // For Stack frames
    void _writeCrashDumpStackFrames();

    // For tiny/full dump
    DumpCreationResult          _writeMiniDumpToNewFile();

public:
    static const VString                    POINTER_FORMATTER;

private:
    StackCrawlerExSettings                  _settings;
};

#endif

#endif /* include guard */
