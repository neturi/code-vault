#ifndef vsessionlifetimemanagementhandler_h
#define vsessionlifetimemanagementhandler_h

#include "vcommtypes.h"
#include "vcommsessioninfo.h"

using VSessionLifetimeManagementInfo        = std::tuple<VCommSessionInfoSharedPtr, SessionLifetimeManagementAction>;
using VSessionLifetimeManagementInfoVector  = std::vector<VSessionLifetimeManagementInfo>;

/**
Contract for the class that would handle notifications affecting the lifetime of VCommSession objects.
*/
class VSessionLifetimeManagementHandler {
protected:
    VSessionLifetimeManagementHandler();

public:
    virtual ~VSessionLifetimeManagementHandler();

    /**
    Called whenever a change in the lifetime of VCommSession objects are detected.
    
    info - 
    Contains a list of VCommSession objects and the type of action (created or deleted) that affects each session's lifetime.
    This method takes a list to avoid spurious/frequent calls when large number sessions connect/disconnect.
    */
    virtual bool ManageSessionsLifetime(const VSessionLifetimeManagementInfoVector& info) = 0;
};

using VSessionLifetimeManagementHandlerSharedPtr = std::shared_ptr<VSessionLifetimeManagementHandler>;
#endif
