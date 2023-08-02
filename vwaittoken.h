#ifndef vwaittoken_h
#define vwaittoken_h

#include "vtypes.h"

class VWaitTokenSource;

/**
Token representing a VWaitTokenSource object. 

An instance of this class can only be obtained from a VWaitTokenSource (parent) object. 
The obtained instance can then be used to check the state of the VWaitTokenSource object (waiting or not-waiting).

The lifetime of this object is independent of the VWaitTokenSource object (from where this is obtained).

This class is thread-safe. But, it is recommended to create one instance per thread (copies are cheap).

NOTE: VWaitToken objects can only be used to check the wait status or to block until wait status is reset. VWaitToken objects
cannot be used to change the status of VWaitTokenSource (wait or continue).
*/
struct VWaitToken {
    friend class VWaitTokenSource;

public:
    ~VWaitToken();

    /**
    Returns 'true' if the 'parent' VWaitTokenSource object is in 'wait' state. If not, returns 'false'.
    */
    bool Waiting() const;

    /**
    If the parent VWaitTokenSource is in 'waiting' state - will block the caller until the parent's state is set to 
    'not-waiting' (by calling VWaitTokenSource::Continue).

    source - 
    Identity of the caller - used for debugging purposes (by adding some logs).
    */
    void WaitUntilContinuation(const std::string& source);

    /**
    Similar to VWaitToken::WaitUntilContinuation except this API times out if the parent's state is not set to 'not-waiting' within
    the specified time period.

    maxTimeToWaitInMilliseconds - 
    The maximum time (in milliseconds) the API would wait (blocking) for paren'ts state change before timing out.

    Returns 'true' if the parent changes its state to 'not-waiting' before 'maxTimeToWaitInMilliseconds' expires.
    Otherwise, returns 'false'.
    */
    bool WaitUntilContinuationOrTimeout(const std::string& source, Vu32 maxTimeToWaitInMilliseconds);

protected:
    VWaitToken(const AtomicBooleanSharedPtr& waitFlag, const ConditionVariableSharedPtr& waitCondition, const MutexSharedPtr& waitConditionMutex);

private:
    AtomicBooleanWeakPtr waitFlag;

    ConditionVariableSharedPtr waitCondition;
    MutexSharedPtr waitConditionMutex;
};
#endif