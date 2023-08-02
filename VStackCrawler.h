/*
* Copyright (c) 1989-2017 Navis LLC. All rights reserved.
* $Id: VStackCrawler.h,v 1.0 2017-03-29 16:30 mohanla Exp $
*/

#ifndef _V_STACK_CRAWLER
#define _V_STACK_CRAWLER

// Prevent spurious VC8 warnings about "deprecated"/"unsafe" boost std c++ lib use.
#if _MSC_VER >= 1400
    #pragma warning(push)
    #pragma warning(disable : 4996)
#endif

#include <string>

#if _MSC_VER >= 1400
    #pragma warning(pop)
#endif

struct ApplicationInfo {
public:
    ApplicationInfo() : ApplicationName("XPS-Client"), VersionString("N/A"), SVNRevision("N/A"), BuildTimeStamp("N/A") {
    }

public:
    std::string ApplicationName;
    std::string VersionString;
    std::string SVNRevision;
    std::string BuildTimeStamp;
};

// Need to be defined by the application.
extern ApplicationInfo GetApplicationInfo();

extern void RegisterApplicationExceptionHandler(const std::string& prefixForTinyDumpFileName, const std::string& prefixForFullDumpFileName);

extern void RegisterApplicationExceptionHandlerLinux(int signum);

#endif