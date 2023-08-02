/*
Copyright (c) 2016. All rights reserved.
*/

#include "vhighresolutiontimehelper.h"

/**
VHighResolutionTimeHelper
*/

const VDouble VHighResolutionTimeHelper::NANOSECONDS_PER_SECOND          = 1E9;
const VDouble VHighResolutionTimeHelper::NANOSECONDS_PER_MILLISECOND     = 1E6;
const VDouble VHighResolutionTimeHelper::NANOSECONDS_PER_MICROSECOND     = 1E3;

const VDouble VHighResolutionTimeHelper::MICROSECONDS_PER_SECOND         = 1E6;
const VDouble VHighResolutionTimeHelper::MICROSECONDS_PER_MILIISECOND    = 1E3;
const VDouble VHighResolutionTimeHelper::MICROSECONDS_PER_NANOSECOND     = 1E-3;

const VDouble VHighResolutionTimeHelper::MILLISECONDS_PER_SECOND         = 1E3;
const VDouble VHighResolutionTimeHelper::MILLISECONDS_PER_MICROSECOND    = 1E-3;
const VDouble VHighResolutionTimeHelper::MILLISECONDS_PER_NANOSECOND     = 1E-6;

// Static 
VDouble VHighResolutionTimeHelper::ConvertNanosecondsToSeconds(const VDouble& nanoseconds) {
    return (nanoseconds / VHighResolutionTimeHelper::NANOSECONDS_PER_SECOND);
}

// Static 
VDouble VHighResolutionTimeHelper::ConvertNanosecondsToMilliseconds(const VDouble& nanoseconds) {
    return (nanoseconds / VHighResolutionTimeHelper::NANOSECONDS_PER_MILLISECOND);
}

// Static 
VDouble VHighResolutionTimeHelper::ConvertNanoSecondsToMicroSeconds(const VDouble& nanoseconds) {
    return (nanoseconds / VHighResolutionTimeHelper::NANOSECONDS_PER_MICROSECOND);
}

// Static 
VDouble VHighResolutionTimeHelper::ConvertMicrosecondsToSeconds(const VDouble& microseconds) {
    return (microseconds / VHighResolutionTimeHelper::MICROSECONDS_PER_SECOND);
}

// Static 
VDouble VHighResolutionTimeHelper::ConvertMicrosecondsToMilliseconds(const VDouble& microseconds) {
    return (microseconds / VHighResolutionTimeHelper::MICROSECONDS_PER_MILIISECOND);
}

// Static 
VDouble VHighResolutionTimeHelper::ConvertMicroSecondsToNanoseconds(const VDouble& microseconds) {
    return (microseconds / VHighResolutionTimeHelper::MICROSECONDS_PER_NANOSECOND);
}

// Static 
VDouble VHighResolutionTimeHelper::ConvertMillisecondsToSeconds(const VDouble& milliseconds) {
    return (milliseconds / VHighResolutionTimeHelper::MILLISECONDS_PER_SECOND);
}

// Static 
VDouble VHighResolutionTimeHelper::ConvertMilliSecondsToMicroseconds(const VDouble& milliseconds) {
    return (milliseconds / VHighResolutionTimeHelper::MILLISECONDS_PER_MICROSECOND);
}

// Static 
VDouble VHighResolutionTimeHelper::ConvertMillisecondsToNanoseconds(const VDouble& milliseconds) {
    return (milliseconds / VHighResolutionTimeHelper::MILLISECONDS_PER_NANOSECOND);
}

// Static 
VDouble VHighResolutionTimeHelper::ConvertSecondsToMilliseconds(const VDouble& seconds) {
    return (seconds * VHighResolutionTimeHelper::MILLISECONDS_PER_SECOND);
}

// Static 
VDouble VHighResolutionTimeHelper::ConvertSecondsToMicroseconds(const VDouble& seconds) {
    return (seconds * VHighResolutionTimeHelper::MICROSECONDS_PER_SECOND);
}

// Static 
VDouble VHighResolutionTimeHelper::ConvertSecondsToNanoseconds(const VDouble& seconds) {
    return (seconds * VHighResolutionTimeHelper::NANOSECONDS_PER_SECOND);
}
