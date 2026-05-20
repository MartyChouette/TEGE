#include "Enjin/Editor/CRDTState.h"
#include "Enjin/Editor/CollaborativeEditing.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace Editor {

// LogEntry definition — kept in .cpp to break circular include with EditOperation
struct CRDTDocument::LogEntry {
    EditOperation op;
    VectorClock clockAtTime;
};

CRDTDocument::CRDTDocument() = default;
CRDTDocument::~CRDTDocument() = default;

// ============================================================================
// CRDTDocument
// ============================================================================

EntityCRDTState& CRDTDocument::GetOrCreate(u64 entityId) {
    return m_Entities[entityId];
}

const EntityCRDTState* CRDTDocument::Get(u64 entityId) const {
    auto it = m_Entities.find(entityId);
    return it != m_Entities.end() ? &it->second : nullptr;
}

bool CRDTDocument::ApplyRemoteOp(const EditOperation& op) {
    // Merge remote clock into local
    m_LocalClock.Merge(op.vclock);

    auto& state = GetOrCreate(op.entityId);
    bool changed = false;

    switch (op.type) {
        case EditOpType::CreateEntity: {
            changed = state.exists.Merge(true, op.vclock, op.authorId);
            // Also merge name if provided
            if (!op.dataJson.empty()) {
                state.name.Merge(op.dataJson, op.vclock, op.authorId);
            }
            break;
        }
        case EditOpType::DeleteEntity: {
            changed = state.exists.Merge(false, op.vclock, op.authorId);
            break;
        }
        case EditOpType::RenameEntity: {
            changed = state.name.Merge(op.dataJson, op.vclock, op.authorId);
            break;
        }
        case EditOpType::SetComponent: {
            auto& reg = state.components[op.componentKey];
            changed = reg.Merge(op.dataJson, op.vclock, op.authorId);
            break;
        }
        case EditOpType::RemoveComponent: {
            // Removing = setting to empty string
            auto& reg = state.components[op.componentKey];
            changed = reg.Merge("", op.vclock, op.authorId);
            break;
        }
        case EditOpType::ModifyTransform: {
            bool posChanged = state.position.Merge(op.position, op.vclock, op.authorId);
            bool rotChanged = state.rotation.Merge(op.rotation, op.vclock, op.authorId);
            bool sclChanged = state.scale.Merge(op.scale, op.vclock, op.authorId);
            changed = posChanged || rotChanged || sclChanged;
            break;
        }
        case EditOpType::SetParent: {
            u64 parentId = 0;
            if (!op.dataJson.empty()) {
                try { parentId = std::stoull(op.dataJson); } catch (...) {}
            }
            changed = state.parentId.Merge(parentId, op.vclock, op.authorId);
            break;
        }
        case EditOpType::LockEntity:
        case EditOpType::UnlockEntity:
            // Locks are advisory, always apply
            changed = true;
            break;
    }

    // Log for catch-up
    m_OpLog.push_back(std::make_unique<LogEntry>(LogEntry{op, m_LocalClock}));
    TrimOpLog();

    return changed;
}

void CRDTDocument::RecordLocalOp(EditOperation& op) {
    m_LocalClock.Increment(m_LocalSiteId);
    op.vclock = m_LocalClock;
    op.authorId = m_LocalSiteId;

    auto& state = GetOrCreate(op.entityId);

    switch (op.type) {
        case EditOpType::CreateEntity:
            state.exists.SetLocal(true, m_LocalClock, m_LocalSiteId);
            if (!op.dataJson.empty()) {
                state.name.SetLocal(op.dataJson, m_LocalClock, m_LocalSiteId);
            }
            break;
        case EditOpType::DeleteEntity:
            state.exists.SetLocal(false, m_LocalClock, m_LocalSiteId);
            break;
        case EditOpType::RenameEntity:
            state.name.SetLocal(op.dataJson, m_LocalClock, m_LocalSiteId);
            break;
        case EditOpType::SetComponent:
            state.components[op.componentKey].SetLocal(op.dataJson, m_LocalClock, m_LocalSiteId);
            break;
        case EditOpType::RemoveComponent:
            state.components[op.componentKey].SetLocal(std::string(""), m_LocalClock, m_LocalSiteId);
            break;
        case EditOpType::ModifyTransform:
            state.position.SetLocal(op.position, m_LocalClock, m_LocalSiteId);
            state.rotation.SetLocal(op.rotation, m_LocalClock, m_LocalSiteId);
            state.scale.SetLocal(op.scale, m_LocalClock, m_LocalSiteId);
            break;
        case EditOpType::SetParent: {
            u64 parentId = 0;
            if (!op.dataJson.empty()) {
                try { parentId = std::stoull(op.dataJson); } catch (...) {}
            }
            state.parentId.SetLocal(parentId, m_LocalClock, m_LocalSiteId);
            break;
        }
        default:
            break;
    }

    m_OpLog.push_back(std::make_unique<LogEntry>(LogEntry{op, m_LocalClock}));
    TrimOpLog();
}

std::vector<EditOperation> CRDTDocument::GetOpsSince(const VectorClock& peerClock) const {
    std::vector<EditOperation> result;
    for (const auto& entry : m_OpLog) {
        if (!entry) continue;
        if (!peerClock.DominatesOrEquals(entry->clockAtTime)) {
            result.push_back(entry->op);
        }
    }
    return result;
}

void CRDTDocument::Clear() {
    m_Entities.clear();
    m_OpLog.clear();
    // Don't reset m_LocalClock or m_LocalSiteId — those persist across scene loads
}

void CRDTDocument::TrimOpLog() {
    if (m_OpLog.size() > MAX_OP_LOG) {
        usize excess = m_OpLog.size() - MAX_OP_LOG;
        m_OpLog.erase(m_OpLog.begin(), m_OpLog.begin() + static_cast<i64>(excess));
    }
}

} // namespace Editor
} // namespace Enjin
