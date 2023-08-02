#include "vcancellationtokensource.h"

VCancellationTokenSource::VCancellationTokenSource() : cancellationFlag(new AtomicBoolean(false)) {
}

VCancellationTokenSource::~VCancellationTokenSource() {
}

VCancellationToken VCancellationTokenSource::Token() const {
    return VCancellationToken(this->cancellationFlag);
}

bool VCancellationTokenSource::Cancelled() const {
    return *this->cancellationFlag;
}

bool VCancellationTokenSource::Cancel() {
    bool notCancelled = false;

    return (std::atomic_compare_exchange_strong(&(*(this->cancellationFlag)), &notCancelled, true));
}
