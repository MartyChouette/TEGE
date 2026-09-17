#include "Enjin/Effects/DrawBudget.h"

#include <algorithm>

namespace Enjin {
namespace Effects {

usize DrawBudgetShare(usize budget, usize consumers) {
    if (consumers == 0) return budget;
    return std::max<usize>(1, budget / consumers);
}

usize DrawStride(usize wanted, usize budget) {
    if (budget == 0) return 1;
    if (wanted <= budget) return 1;
    return (wanted + budget - 1) / budget;   // ceil, so the result always fits
}

f32 PoolDemandScale(f64 demand, usize capacity) {
    if (capacity == 0) return 0.0f;
    if (demand <= static_cast<f64>(capacity) || demand <= 0.0) return 1.0f;
    return static_cast<f32>(static_cast<f64>(capacity) / demand);
}

} // namespace Effects
} // namespace Enjin
