#pragma once

#include "Enjin/Platform/Platform.h"
#include <string>

namespace Enjin {
namespace ECS {

// Notes component - simple text notes for developer documentation
// Attach to any entity to add comments, reminders, or documentation
struct ENJIN_API NotesComponent {
    std::string notes;

    NotesComponent() = default;
    NotesComponent(const std::string& text) : notes(text) {}
};

} // namespace ECS
} // namespace Enjin
