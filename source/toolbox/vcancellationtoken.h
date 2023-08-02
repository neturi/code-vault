#ifndef cancellationtoken_h
#define cancellationtoken_h

#include "vtypes.h"

class VCancellationTokenSource;

/**
An inexpensive token that could be obtained from a VCancellationTokenSource (parent) object and passed around.
See documentation on VCancellationTokenSource for more details.

The lifetime of VCancellationToken is indepent of the lifetime of VWaitTokenSource.

This implementation is thread-safe.
*/
struct VCancellationToken {
    friend class VCancellationTokenSource;

public:
    ~VCancellationToken();

    /**
    Returns 'true' if the parent object's (VCancellationTokenSource) is set to 'cancelled'.
    */
    bool Cancelled() const;

protected:
    VCancellationToken(const AtomicBooleanSharedPtr& cancellationFlag);

private:
    AtomicBooleanWeakPtr cancellationFlag;
};
#endif