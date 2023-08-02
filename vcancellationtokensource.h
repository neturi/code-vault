#ifndef vcancellationtokensource_h
#define vcancellationtokensource_h

#include "vcancellationtoken.h"

/**
VCancellationTokenSource could be used to propagate 'closure' of a task/operation to multiple, interested component.

This light-weight implementation supports single-point cancellation/stop (usually the owner of the VCancelletionTokenSource).
Any number of VCancellationToken objects could be obtained from a VCancellationTokenSource object and passed around.

When a task/operation is cancelled/stopped - by calling VCancellationTokenSource::Stop() - all VCancellationToken objects obtained from the 
VCancellationTokenSource object reflect the cancellation/stop status.

The lifetime of VCancellationToken is indepent of the lifetime of VWaitTokenSource. 
This class is thread-safe.

NOTE: VCancellationToken objects can only be used to check the stop/cancellation status. They cannot be used to stop/cancel a task/operation.
*/

class VCancellationTokenSource {
public:
    VCancellationTokenSource();

    ~VCancellationTokenSource();

    VCancellationTokenSource(const VCancellationTokenSource& other) = delete;

    VCancellationTokenSource& operator=(const VCancellationTokenSource& other) = delete;

    VCancellationToken Token() const;

    /**
    Returns 'true' if the state is set to 'cancelled'. Returns 'false' otherwise.
    */
    bool Cancelled() const;

    /**
    Changes the state to 'cancelled' - if it is not cancelled already.

    Returns 'true' if we have a successful state change. Otherwise, returns 'false'.
    */
    bool Cancel();

private:
    AtomicBooleanSharedPtr cancellationFlag;
};
#endif