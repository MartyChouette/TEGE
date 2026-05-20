#pragma once

// Compatibility header — NetworkTransport is now UDPTransport implementing INetworkTransport.
// Existing code that includes this header continues to compile unchanged.
#include "Enjin/Networking/UDPTransport.h"

namespace Enjin {
namespace Networking {
    using NetworkTransport = UDPTransport;
} // namespace Networking
} // namespace Enjin
