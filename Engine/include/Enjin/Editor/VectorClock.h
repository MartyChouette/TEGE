#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <array>
#include <vector>
#include <algorithm>

namespace Enjin {
namespace Editor {

// Vector clock for causality tracking in collaborative editing.
// Fixed-size array for up to 16 concurrent peers (no heap allocation).
class VectorClock {
public:
    static constexpr u8 MAX_SITES = 16;

    VectorClock() { m_Clocks.fill(0); }

    // Increment the clock for a site (happens on every local operation)
    void Increment(u8 siteId) {
        if (siteId < MAX_SITES) {
            ++m_Clocks[siteId];
            if (siteId >= m_MaxSite) m_MaxSite = siteId + 1;
        }
    }

    // Merge with a remote clock (take element-wise max)
    void Merge(const VectorClock& other) {
        for (u8 i = 0; i < MAX_SITES; ++i) {
            m_Clocks[i] = std::max(m_Clocks[i], other.m_Clocks[i]);
        }
        m_MaxSite = std::max(m_MaxSite, other.m_MaxSite);
    }

    // Get the clock value for a specific site
    u64 Get(u8 siteId) const {
        return siteId < MAX_SITES ? m_Clocks[siteId] : 0;
    }

    // Compare two clocks:
    //  -1 = this happened-before other (this < other)
    //   0 = concurrent (neither dominates)
    //  +1 = this happened-after other (this > other)
    i32 Compare(const VectorClock& other) const {
        bool thisGreater = false;
        bool otherGreater = false;
        u8 limit = std::max(m_MaxSite, other.m_MaxSite);
        for (u8 i = 0; i < limit; ++i) {
            if (m_Clocks[i] > other.m_Clocks[i]) thisGreater = true;
            if (m_Clocks[i] < other.m_Clocks[i]) otherGreater = true;
            if (thisGreater && otherGreater) return 0;  // Concurrent — early exit
        }
        if (thisGreater && !otherGreater) return 1;
        if (otherGreater && !thisGreater) return -1;
        return 1;  // Equal clocks — treat as "this >= other"
    }

    // True if this clock dominates or equals the other (every component >= other's)
    bool DominatesOrEquals(const VectorClock& other) const {
        for (u8 i = 0; i < MAX_SITES; ++i) {
            if (m_Clocks[i] < other.m_Clocks[i]) return false;
        }
        return true;
    }

    // True if all components are zero
    bool IsZero() const {
        for (u8 i = 0; i < MAX_SITES; ++i) {
            if (m_Clocks[i] != 0) return false;
        }
        return true;
    }

    // Collapse to a single scalar (max across all sites) — for log ordering display
    u64 MaxComponent() const {
        u64 mx = 0;
        for (u8 i = 0; i < MAX_SITES; ++i) mx = std::max(mx, m_Clocks[i]);
        return mx;
    }

    // Binary serialization: u8 count + (u8 siteId, u64 clock) pairs for non-zero entries
    void Serialize(std::vector<u8>& out) const {
        u8 count = 0;
        for (u8 i = 0; i < m_MaxSite; ++i) {
            if (m_Clocks[i] != 0) ++count;
        }
        out.push_back(count);
        for (u8 i = 0; i < m_MaxSite; ++i) {
            if (m_Clocks[i] != 0) {
                out.push_back(i);
                u64 v = m_Clocks[i];
                for (int b = 0; b < 8; ++b) {
                    out.push_back(static_cast<u8>(v & 0xFF));
                    v >>= 8;
                }
            }
        }
    }

    // TODO: Deserialize — will be added when the wire format is upgraded
    // to include vector clocks (currently still uses Lamport for backward compat).

    bool operator==(const VectorClock& other) const { return m_Clocks == other.m_Clocks; }
    bool operator!=(const VectorClock& other) const { return m_Clocks != other.m_Clocks; }

private:
    std::array<u64, MAX_SITES> m_Clocks;
    u8 m_MaxSite = 0;  // Optimization: only iterate up to the highest known site
};

} // namespace Editor
} // namespace Enjin
