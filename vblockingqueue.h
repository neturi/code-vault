#ifndef vblockingqueue_h
#define vblockingqueue_h

#include "vtypes.h"
#include "vutils.h"
#include "vcancellationtoken.h"
#include "vlogger.h"

#include <boost/foreach.hpp>

/**
Queue that could be used to:
1. Retrieve an item from the queue without blocking.
2. Wait and retrieve an item from the queue - when it becomes available. 

This queue is thread-safe.
*/

template <typename DataType>
class VBlockingQueue {
public:
    /**
    C'tor

    queueName - 
    The name of this queue (used in logging).

    cancellationToken - 
    Cancellation token used by this queue to check if the owner has cancelled further activities.

    allowTakeAfterCancellationOrStop - 
    If set to 'true', will process the items remaining in the queue (new items cannot be queued) when
    the operation is cancelled by the owner.
    If set to 'false', will discard any pending items in the queue on cancellation.
    */
    VBlockingQueue(const std::string& queueName, const VCancellationToken& cancellationToken, bool allowTakeAfterCancellationOrStop) : 
                                                                                                typeName(STLUtils::CleanRawTypeName(std::string(typeid(*this).name()))),
                                                                                                name(queueName), cancellationToken(cancellationToken),
                                                                                                allowTakeAfterCancellationOrStop(allowTakeAfterCancellationOrStop),
                                                                                                queueStopped(false) {
        VLOGGER_INFO(VSTRING_FORMAT("[COMM] %s[%s]::c'tor - Queue is created. Allow take after cancellation or stop: %d", this->typeName.c_str(), 
                                        this->name.c_str(), this->allowTakeAfterCancellationOrStop));
    }

    /**
    C'tor 
    If the default type name is not desirable, use this overload and supply your own type name.

    typeName - 
    The type name of this queue (used in logging).
    */
    VBlockingQueue(const std::string& queueName, const std::string& typeName, const VCancellationToken& cancellationToken, bool allowTakeAfterCancellationOrStop) :
                                                                                                typeName(typeName),
                                                                                                name(queueName), cancellationToken(cancellationToken),
                                                                                                allowTakeAfterCancellationOrStop(allowTakeAfterCancellationOrStop),
                                                                                                queueStopped(false) {
        VLOGGER_INFO(VSTRING_FORMAT("[COMM] %s[%s]::c'tor - Queue is created. Allow take after cancellation or stop: %d", this->typeName.c_str(),
                                        this->name.c_str(), this->allowTakeAfterCancellationOrStop));
    }

    ~VBlockingQueue() {
        VLOGGER_INFO(VSTRING_FORMAT("[COMM] %s[%s]::d'tor - Queue size on destruction: %d", this->typeName.c_str(), this->name.c_str(), this->jobQueue.size()));
    }

    VBlockingQueue(const VBlockingQueue& other) = delete;

    VBlockingQueue& operator=(const VBlockingQueue& other) = delete;

    std::string Name() const {
        return this->name;
    }

    /**
    Non-blocking call to try and retrieve an item from the queue.

    If the queue is empty, returns 'false' and the value of 'data' is undefined. 
    If the queue is not empty and if we have a successful pop from the queue, returns 'true'.

    data - 
    Out parameter. If the queue is not empty, will contain the dequeued item. If the queue is empty, value is undefined.

    consumerId - 
    The name/Id of the caller. Used for debugging. 
    */
    bool TryTake(DataType& data, const std::string& /*consumerId*/) {
        // If we are NOT set to allow 'taking' of data after stop or cancellation...
        if (this->cancellationToken.Cancelled() || this->queueStopped) {
            if (!allowTakeAfterCancellationOrStop) {
                return false;
            } else {
                std::call_once(cancellationMessageLogged, [this]() {
                    VLOGGER_INFO(VSTRING_FORMAT("[COMM] %s[%s]::TryTake - Cancellation/stop detected. Will stop queueing. But, will allow 'take' until queue empties out as it is configured to allow 'take' after stop/cancellation...", this->typeName.c_str(), this->name.c_str()));
                });
            }
        }

        {
            std::lock_guard<std::mutex> lock(this->mutex);

            if (this->jobQueue.size() > 0) {
                data = this->jobQueue.front();

                this->jobQueue.pop();

                return true;
            }

            return false;
        }
    }

    /**
    Blocking call to try and retrieve an item from the queue.

    This method will block until one of the following conditions are met.
    1. The queue is stopped - by calling Stop() method.
    2. An item is queued - that is, the queue's status changes from empty to non-empty.

    If the queue is empty, or if the operation is cancelled by the owner, returns 'false' and the value of 'data' is undefined.
    If we have a successful pop from the queue, returns 'true'. 

    data - 
    Out parameter. If the dequeue was successful, will contain the dequeued item. 
    If the dequeue was not successful, value is undefined.
    
    consumerId - 
    The name/Id of the caller. Used for debugging.
    */
    bool WaitAndTake(DataType& data, const std::string& /*consumerId*/) {
        auto proceedToTake = [this]() {
            if (this->cancellationToken.Cancelled() || this->queueStopped) {
                // If we are NOT set to allow 'taking' of data after stop or cancellation...
                if (!allowTakeAfterCancellationOrStop) {
                    return false;
                } else {
                    std::call_once(cancellationMessageLogged, [this]() {
                        VLOGGER_INFO(VSTRING_FORMAT("[COMM] %s[%s]::TryTake - Cancellation detected. Will stop queueing. But, will allow 'take' until queue empties out as it is configured to allow 'take' after stop/cancellation...", this->typeName.c_str(), this->name.c_str()));
                    });
                }
            }

            return true;
        };

        if (!proceedToTake()) {
            return false;
        }

        {
            std::unique_lock<std::mutex> lock(this->mutex);

            // Lock until an item is queued in the queue or until the queue is stopped.
            this->condition.wait(lock, [this] { return (!this->jobQueue.empty() || this->cancellationToken.Cancelled() || this->queueStopped); });

            if ((this->cancellationToken.Cancelled() || this->queueStopped) && (!proceedToTake() || this->jobQueue.empty())) {
                Stop();

                lock.unlock();

                return false;
            }

            data = this->jobQueue.front();

            this->jobQueue.pop();

            lock.unlock();
        }

        return true;
    }

    /**
    Queues an item in the queue.

    Returns 'true' if the queueing is successful. If not, returns 'false'.

    data - 
    The item that should be queued.
    */
    bool Enqueue(const DataType& data) {
        if (this->cancellationToken.Cancelled() || this->queueStopped) {
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(this->mutex);

            this->jobQueue.push(data);
        }

        this->condition.notify_one();

        return true;
    }

    /**
    Queues multiple items in the queue.

    Returns 'true' if the queueing is successful. If not, returns 'false'.

    items - 
    A collection of items that needs to be queued.
    */
    bool EnqueueMultiple(const std::vector<DataType>& items) {
        if (this->cancellationToken.Cancelled() || this->queueStopped) {
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(this->mutex);

			BOOST_FOREACH(const DataType& item, items) {
                this->jobQueue.push(item);
            }
        }

        this->condition.notify_all();

        return true;
    }

    /**
    Clears/empties the queue.

    Doesn't mean we are stopped/cancelled. We're just being asked to empty the queue...
    */
    void Clear() {
        std::unique_lock<std::mutex> lock(this->mutex);

        if (this->jobQueue.size() > 0) {
            std::queue<DataType> emptyQueue;

            std::swap(this->jobQueue, emptyQueue);
        }
    }

    /**
    Returns the current size (items queued) of the queue.
    
    Very expensive - if called too often.
    */
    size_t Size() const {
        std::unique_lock<std::mutex> lock(this->mutex);

        return this->jobQueue.size();
    }

    /**
    Stops the queue. Unblocks all VBlockingQueue::WaitAndTake calls. 
    Items cannot be queued after a queue is stopped.

    Once stopped, queue cannot be restarted (for now at least).
    */
    void Stop() {
        std::call_once(stopCalled, [this]() {
            this->queueStopped = true;

            // We can't just notify_one if we have multiple threads blocked (waiting to take).
            this->condition.notify_all();

            VLOGGER_INFO(VSTRING_FORMAT("[COMM] %s[%s]::Stop - Stopped. Is cancelled?: %d, Allowing 'take' after stop/cancellation?: %d", this->typeName.c_str(), this->name.c_str(), this->cancellationToken.Cancelled(), this->allowTakeAfterCancellationOrStop));
        });
    }

    bool Stopped() const {
        return this->queueStopped;
    }

    bool Cancelled() const {
        return this->cancellationToken.Cancelled();
    }

private:
    std::string                 typeName;
    std::string                 name;

    VCancellationToken		    cancellationToken;

    std::queue<DataType>        jobQueue;

    std::condition_variable	    condition;
    mutable std::mutex			mutex;

    std::once_flag              cancellationMessageLogged;

    std::once_flag			    stopCalled;

    AtomicBoolean               queueStopped;

    bool					    allowTakeAfterCancellationOrStop;
};
#endif
