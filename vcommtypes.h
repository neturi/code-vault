#ifndef vcommtypes_h
#define vcommtypes_h

#include "vtaskscheduler.h"
#include "vgenerictaskscheduler.h"

/**
The type of event received by the VCommSessionEventProducer for a socket.

As Comm Session Event Producer only interested in Read and Close events, it only listens for Read and Close events.
*/
enum class CommEventType {
    None    = 0,
    Read    = 1,
    Close   = 2
};

/**
The type of notification that Session Lifetime Manager receives from the listener thread (incoming new connections) 
and the Comm Session Event Producer.

Created - 
A new comm session has been created due to a new incoming connection.

Closed - 
Comm session had disconnected or shut down.
*/
enum class SessionLifetimeManagementAction {
    Unspecified     = 0,
    Created         = 1,
    Deleted         = 2
};

/**
Some meaningful type names for the task schedulers we create and use.
*/
using BroadcastMessagePublishTaskScheduler              = VTaskScheduler<bool>;
using BroadcastMessagePublishTaskSchedulerSharedPtr     = std::shared_ptr<BroadcastMessagePublishTaskScheduler>;
using BroadcastMessagePublishTaskSchedulerWeakPtr       = std::weak_ptr<BroadcastMessagePublishTaskScheduler>;

using TxMessageDispatchTaskScheduler                    = VTaskScheduler<bool>;
using TxMessageDispatchTaskSchedulerSharedPtr           = std::shared_ptr<TxMessageDispatchTaskScheduler>;
using TxMessageDispatchTaskSchedulerWeakPtr             = std::weak_ptr<TxMessageDispatchTaskScheduler>;

using RxMessageReceptionTaskScheduler                   = VTaskScheduler<bool>;
using RxMessageReceptionTaskSchedulerSharedPtr          = std::shared_ptr<RxMessageReceptionTaskScheduler>;
using RxMessageReceptionTaskSchedulerWeakPtr            = std::weak_ptr<RxMessageReceptionTaskScheduler>;

using RxMessageDispatchTaskScheduler                    = VTaskScheduler<bool>;
using RxMessageDispatchTaskSchedulerSharedPtr           = std::shared_ptr<RxMessageDispatchTaskScheduler>;
using RxMessageDispatchTaskSchedulerWeakPtr             = std::weak_ptr<RxMessageDispatchTaskScheduler>;

using SessionLifetimeManagementTaskScheduler            = VTaskScheduler<bool>;
using SessionLifetimeManagementTaskSchedulerSharedPtr   = std::shared_ptr<SessionLifetimeManagementTaskScheduler>;
using SessionLifetimeManagementTaskSchedulerWeakPtr     = std::weak_ptr<SessionLifetimeManagementTaskScheduler>;

using N4BentoResponseTaskScheduler                      = VGenericTaskScheduler<int>;
using N4BentoResponseTaskSchedulerSharedPtr             = std::shared_ptr<N4BentoResponseTaskScheduler>;
using N4BentoResponseTaskSchedulerWeakPtr               = std::weak_ptr<N4BentoResponseTaskScheduler>;
#endif
