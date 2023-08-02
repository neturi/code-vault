/*
Copyright (c) 2016. All rights reserved.
*/

// START - For Network Monitor
#ifndef vhighresolutiontimehelper_h
#define vhighresolutiontimehelper_h

#include "vtypes.h"

/**
VHighResolutionTimeHelper
*/
class VHighResolutionTimeHelper {
public:
    static VDouble GetTimeInNanoSeconds();
    static VDouble GetTimeInMicroSeconds();
    static VDouble GetTimeInMilliSeconds();
    static VDouble GetTimeInSeconds();

    static VDouble ConvertNanosecondsToSeconds(const VDouble& nanoseconds);
    static VDouble ConvertNanosecondsToMilliseconds(const VDouble& nanoseconds);
    static VDouble ConvertNanoSecondsToMicroSeconds(const VDouble& nanoseconds);

    static VDouble ConvertMicrosecondsToSeconds(const VDouble& microseconds);
    static VDouble ConvertMicrosecondsToMilliseconds(const VDouble& microseconds);
    static VDouble ConvertMicroSecondsToNanoseconds(const VDouble& microseconds);

    static VDouble ConvertMillisecondsToSeconds(const VDouble& milliseconds);
    static VDouble ConvertMilliSecondsToMicroseconds(const VDouble& milliseconds);
    static VDouble ConvertMillisecondsToNanoseconds(const VDouble& milliseconds);

    static VDouble ConvertSecondsToMilliseconds(const VDouble& seconds);
    static VDouble ConvertSecondsToMicroseconds(const VDouble& seconds);
    static VDouble ConvertSecondsToNanoseconds(const VDouble& seconds);

private:
    VHighResolutionTimeHelper();

    // Use this static method to initialize the constant 'TicksPerSecond'.
    static Vu64 InitializeTicksPerSecond();

public:
    static const VDouble NANOSECONDS_PER_SECOND;
    static const VDouble NANOSECONDS_PER_MILLISECOND;
    static const VDouble NANOSECONDS_PER_MICROSECOND;

    static const VDouble MICROSECONDS_PER_SECOND;
    static const VDouble MICROSECONDS_PER_MILIISECOND;
    static const VDouble MICROSECONDS_PER_NANOSECOND;

    static const VDouble MILLISECONDS_PER_SECOND;
    static const VDouble MILLISECONDS_PER_MICROSECOND;
    static const VDouble MILLISECONDS_PER_NANOSECOND;

    static const Vu64 TicksPerSecond;
};

#endif