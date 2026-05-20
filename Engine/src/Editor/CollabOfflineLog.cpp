#include "Enjin/Editor/CollabOfflineLog.h"
#include "Enjin/Editor/CollaborativeEditing.h"
#include "Enjin/Logging/Log.h"
#include <filesystem>
#include <cstring>

namespace {
// Local helper — deserialize a VectorClock from binary data.
// Matches the format written by VectorClock::Serialize().
bool DeserializeVectorClock(const Enjin::u8* data, Enjin::u32& pos, Enjin::u32 maxSize,
                            Enjin::Editor::VectorClock& out) {
    out = Enjin::Editor::VectorClock{};
    if (pos >= maxSize) return false;
    Enjin::u8 entryCount = data[pos++];
    if (entryCount > Enjin::Editor::VectorClock::MAX_SITES) return false;
    for (Enjin::u8 i = 0; i < entryCount; ++i) {
        if (pos + 9 > maxSize) return false;
        Enjin::u8 sid = data[pos++];
        if (sid >= Enjin::Editor::VectorClock::MAX_SITES) return false;
        Enjin::u64 v = 0;
        for (int b = 0; b < 8; ++b) {
            v |= static_cast<Enjin::u64>(data[pos++]) << (b * 8);
        }
        // Use Increment to set the clock — increment sid times to reach v.
        // Actually, we need direct access. Use a workaround: merge with a clock
        // that has only this site set.
        Enjin::Editor::VectorClock temp;
        for (Enjin::u64 j = 0; j < v; ++j) temp.Increment(sid);
        out.Merge(temp);
    }
    return true;
}
} // anonymous namespace

namespace Enjin {
namespace Editor {

CollabOfflineLog::~CollabOfflineLog() = default;

void CollabOfflineLog::SetPath(const std::string& scenePath) {
    if (scenePath.empty()) {
        m_LogPath.clear();
        return;
    }
    m_LogPath = scenePath + ".enjincollab";
}

void CollabOfflineLog::AppendOperation(const EditOperation& op) {
    if (m_LogPath.empty()) return;

    // Open in append mode. Create file with header if it doesn't exist.
    bool isNew = !std::filesystem::exists(m_LogPath);

    std::ofstream out(m_LogPath, std::ios::binary | std::ios::app);
    if (!out.is_open()) {
        ENJIN_LOG_ERROR(Editor, "CollabOfflineLog: Failed to open %s for append", m_LogPath.c_str());
        return;
    }

    if (isNew) {
        WriteHeader(out);
    }

    // Serialize the operation using the same binary format as the wire protocol.
    // For the offline log we store: u32 payloadSize + payload bytes.
    // This lets us read entries back without knowing sizes in advance.
    std::vector<u8> payload;

    // Type (1 byte)
    payload.push_back(static_cast<u8>(op.type));

    // EntityId (8 bytes LE)
    u64 eid = op.entityId;
    for (int b = 0; b < 8; ++b) { payload.push_back(static_cast<u8>(eid & 0xFF)); eid >>= 8; }

    // SequenceId (8 bytes LE)
    u64 seq = op.sequenceId;
    for (int b = 0; b < 8; ++b) { payload.push_back(static_cast<u8>(seq & 0xFF)); seq >>= 8; }

    // AuthorId (1 byte)
    payload.push_back(op.authorId);

    // Timestamp (8 bytes, IEEE 754 double LE)
    {
        u64 ts;
        std::memcpy(&ts, &op.timestamp, sizeof(ts));
        for (int b = 0; b < 8; ++b) { payload.push_back(static_cast<u8>(ts & 0xFF)); ts >>= 8; }
    }

    // Vector clock
    op.vclock.Serialize(payload);

    // Transform fields (always, 36 bytes: 3x Vector3)
    auto writeF32 = [&](f32 v) {
        u32 bits; std::memcpy(&bits, &v, sizeof(bits));
        for (int b = 0; b < 4; ++b) { payload.push_back(static_cast<u8>(bits & 0xFF)); bits >>= 8; }
    };
    writeF32(op.position.x); writeF32(op.position.y); writeF32(op.position.z);
    writeF32(op.rotation.x); writeF32(op.rotation.y); writeF32(op.rotation.z);
    writeF32(op.scale.x);    writeF32(op.scale.y);    writeF32(op.scale.z);

    // String fields: componentKey, dataJson, previousJson, authorName
    auto writeStr = [&](const std::string& s) {
        u32 len = static_cast<u32>(s.size());
        for (int b = 0; b < 4; ++b) { payload.push_back(static_cast<u8>(len & 0xFF)); len >>= 8; }
        payload.insert(payload.end(), s.begin(), s.end());
    };
    writeStr(op.componentKey);
    writeStr(op.dataJson);
    writeStr(op.previousJson);
    writeStr(op.authorName);

    // Write length-prefixed entry
    u32 payloadSize = static_cast<u32>(payload.size());
    out.write(reinterpret_cast<const char*>(&payloadSize), 4);
    out.write(reinterpret_cast<const char*>(payload.data()), payloadSize);
    out.flush();

    // Update tracking
    m_LatestClock.Merge(op.vclock);
    ++m_EntryCount;
}

std::vector<EditOperation> CollabOfflineLog::LoadAll() const {
    std::vector<EditOperation> result;
    if (m_LogPath.empty() || !std::filesystem::exists(m_LogPath)) return result;

    std::ifstream in(m_LogPath, std::ios::binary);
    if (!in.is_open()) return result;

    if (!ValidateHeader(in)) {
        ENJIN_LOG_WARN(Editor, "CollabOfflineLog: Invalid header in %s", m_LogPath.c_str());
        return result;
    }

    while (in.good() && !in.eof()) {
        u32 payloadSize = 0;
        in.read(reinterpret_cast<char*>(&payloadSize), 4);
        if (in.gcount() < 4 || payloadSize == 0 || payloadSize > 64 * 1024 * 1024) break;

        std::vector<u8> payload(payloadSize);
        in.read(reinterpret_cast<char*>(payload.data()), payloadSize);
        if (static_cast<u32>(in.gcount()) < payloadSize) break;

        // Deserialize
        u32 pos = 0;
        if (pos >= payloadSize) continue;

        EditOperation op;
        op.type = static_cast<EditOpType>(payload[pos++]);

        // EntityId
        if (pos + 8 > payloadSize) continue;
        op.entityId = 0;
        for (int b = 0; b < 8; ++b) op.entityId |= static_cast<u64>(payload[pos++]) << (b * 8);

        // SequenceId
        if (pos + 8 > payloadSize) continue;
        op.sequenceId = 0;
        for (int b = 0; b < 8; ++b) op.sequenceId |= static_cast<u64>(payload[pos++]) << (b * 8);

        // AuthorId
        if (pos >= payloadSize) continue;
        op.authorId = payload[pos++];

        // Timestamp
        if (pos + 8 > payloadSize) continue;
        u64 tsRaw = 0;
        for (int b = 0; b < 8; ++b) tsRaw |= static_cast<u64>(payload[pos++]) << (b * 8);
        std::memcpy(&op.timestamp, &tsRaw, sizeof(op.timestamp));

        // Vector clock
        DeserializeVectorClock(payload.data(), pos, payloadSize, op.vclock);

        // Transform (36 bytes)
        auto readF32 = [&]() -> f32 {
            if (pos + 4 > payloadSize) return 0.0f;
            u32 bits = 0;
            for (int b = 0; b < 4; ++b) bits |= static_cast<u32>(payload[pos++]) << (b * 8);
            f32 v; std::memcpy(&v, &bits, sizeof(v));
            return v;
        };
        op.position.x = readF32(); op.position.y = readF32(); op.position.z = readF32();
        op.rotation.x = readF32(); op.rotation.y = readF32(); op.rotation.z = readF32();
        op.scale.x = readF32();    op.scale.y = readF32();    op.scale.z = readF32();

        // Strings
        auto readStr = [&]() -> std::string {
            if (pos + 4 > payloadSize) return "";
            u32 len = 0;
            for (int b = 0; b < 4; ++b) len |= static_cast<u32>(payload[pos++]) << (b * 8);
            if (len > 64 * 1024 || pos + len > payloadSize) { pos = payloadSize; return ""; }
            std::string s(payload.begin() + pos, payload.begin() + pos + len);
            pos += len;
            return s;
        };
        op.componentKey = readStr();
        op.dataJson = readStr();
        op.previousJson = readStr();
        op.authorName = readStr();

        op.lamportClock = op.vclock.MaxComponent();
        result.push_back(std::move(op));
    }

    return result;
}

std::vector<EditOperation> CollabOfflineLog::GetOpsSince(const VectorClock& peerClock) const {
    auto all = LoadAll();
    std::vector<EditOperation> result;
    for (auto& op : all) {
        if (!peerClock.DominatesOrEquals(op.vclock)) {
            result.push_back(std::move(op));
        }
    }
    return result;
}

void CollabOfflineLog::Compact(const VectorClock& allAckedClock) {
    auto all = LoadAll();
    std::vector<EditOperation> remaining;
    for (auto& op : all) {
        if (!allAckedClock.DominatesOrEquals(op.vclock)) {
            remaining.push_back(std::move(op));
        }
    }

    // Rewrite
    Clear();
    for (const auto& op : remaining) {
        AppendOperation(op);
    }
}

void CollabOfflineLog::Clear() {
    if (!m_LogPath.empty() && std::filesystem::exists(m_LogPath)) {
        std::filesystem::remove(m_LogPath);
    }
    m_LatestClock = VectorClock{};
    m_EntryCount = 0;
}

bool CollabOfflineLog::Exists() const {
    return !m_LogPath.empty() && std::filesystem::exists(m_LogPath);
}

void CollabOfflineLog::WriteHeader(std::ofstream& out) const {
    out.write(MAGIC, 4);
    out.put(static_cast<char>(LOG_VERSION));
    out.put(static_cast<char>(m_SiteId));
}

bool CollabOfflineLog::ValidateHeader(std::ifstream& in) const {
    char magic[4] = {};
    in.read(magic, 4);
    if (in.gcount() < 4) return false;
    if (std::memcmp(magic, MAGIC, 4) != 0) return false;

    char version = 0;
    in.get(version);
    if (static_cast<u8>(version) > LOG_VERSION) return false;

    char siteId = 0;
    in.get(siteId);  // Read but don't validate — any site could have written this log

    return true;
}

} // namespace Editor
} // namespace Enjin
