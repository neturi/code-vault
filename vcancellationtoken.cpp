#include "vcancellationtoken.h"

VCancellationToken::VCancellationToken(const AtomicBooleanSharedPtr& cancellationFlag) : cancellationFlag(AtomicBooleanWeakPtr(cancellationFlag)) {
}

VCancellationToken::~VCancellationToken() {
}

bool VCancellationToken::Cancelled() const {
    if (auto cancellationFlagSharedPtr = this->cancellationFlag.lock()) {
        return *cancellationFlagSharedPtr;
    }

    return true;
}
