#ifndef vwaittokensource_h
#define vwaittokensource_h

#include "vwaittoken.h"

/**
A simple signalling source that could be used to hold execution of one or more threads (unlimited) 
until signalled.

The difference between this class and VCancellationTokenSource is:
VCancellationTokenSuorce is used to signal the 'end' of an execution unit or task. 
VWaitTokenSource is used to 'hold' or make one or more threads wait until signalled to continue.

VWaitTokenSource acts as the signal source. 
You could obtain as many tokens from an instance of VWaitTokenSource and pass it on to one or more threads.
Calling VWaitTokenSource::Wait will change the source' state to 'waiting' and the threads with the tokens 
could check and wait (see VWaitToken:: WaitUntilContinuation) until VWaitTokenSource::Continue is called.

NOTE: It is the threads' responsibility to check and wait for signals. Calling VWaitTokenSource::Wait will not automatically 
'suspend' or hold the threads.

VWaitToken objects can out-live VWaitTokenSource - from which they are obtained.

This class is thread-safe.
*/
class VWaitTokenSource {
public:
    VWaitTokenSource(bool startWaiting);

    VWaitTokenSource(const VWaitTokenSource& other) = delete;

    VWaitTokenSource& operator=(const VWaitTokenSource& other) = delete;

    ~VWaitTokenSource();

    /**
    Returns a VWaitToken object that could be used to check if VWaitTokenSource::Wait 
    had been signalled to wait or signalled to continue.
    */
    VWaitToken Token() const;

    /**
    Returns 'true' if VWaitTokenSource::Wait had been called. Otherwise, returns 'false'.
    */
    bool Waiting() const;

    /**
    Changes the internal state to 'waiting'. Also the state change is propagated to all VWaitToken objects
    obtained from this instance.

    Returns 'true' if state is successfully changed. Otherwise returns 'false'.
    NOTE: Calling VWaitTokenSource::Wait - while the state is already 'waiting' - will also return 'false'.
    */
    bool Wait();

    /**
    Resets the internal state to 'not waiting' - if the current state is 'waiting'. The state change is 
    propagated to all VWaitToken objects obtained from this instance.

    Returns 'true' if - and only if - the state is successfully changed. Otherwise returns 'false'.
    */
    bool Continue();

private:
    AtomicBooleanSharedPtr waitFlag;

    ConditionVariableSharedPtr waitCondition;
    MutexSharedPtr waitConditionMutex;
};
#endif