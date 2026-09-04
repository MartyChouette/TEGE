#include "Enjin/Editor/EditorProtocol.h"
#include "Enjin/Logging/Log.h"
#include <cstring>
#include <deque>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cctype>
#include <cerrno>
#endif

namespace Enjin {
namespace Editor {

// Shared memory ring buffer transport for same-machine editor↔runtime IPC.
// Uses a memory-mapped file (Windows) or POSIX shm (Linux) with a lock-free
// single-producer single-consumer ring buffer.
//
// For cross-machine or browser connections, use WebSocketTransport instead.
class SharedMemoryTransport : public IEditorTransport {
public:
    SharedMemoryTransport() = default;
    ~SharedMemoryTransport() override { Disconnect(); }

    bool Connect(const std::string& endpoint) override {
#ifdef _WIN32
        // Create or open named shared memory
        m_MapName = "Local\\EnjinEditorBridge_" + endpoint;
        m_MapHandle = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr,
                                          PAGE_READWRITE, 0, BUFFER_SIZE, m_MapName.c_str());
        if (!m_MapHandle) {
            ENJIN_LOG_ERROR(Editor, "SharedMemoryTransport: CreateFileMapping failed (%lu)", GetLastError());
            return false;
        }

        m_Buffer = static_cast<u8*>(MapViewOfFile(m_MapHandle, FILE_MAP_ALL_ACCESS, 0, 0, BUFFER_SIZE));
        if (!m_Buffer) {
            ENJIN_LOG_ERROR(Editor, "SharedMemoryTransport: MapViewOfFile failed");
            CloseHandle(m_MapHandle);
            m_MapHandle = nullptr;
            return false;
        }

        // Initialize ring buffer header (first 16 bytes)
        auto* header = reinterpret_cast<RingHeader*>(m_Buffer);
        if (header->magic != MAGIC) {
            // First connector initializes the buffer
            memset(m_Buffer, 0, BUFFER_SIZE);
            header->magic = MAGIC;
            header->writePos = 0;
            header->readPos = 0;
        }

        m_Connected = true;
        ENJIN_LOG_INFO(Editor, "SharedMemoryTransport: connected to '%s' (%u KB)",
                       m_MapName.c_str(), BUFFER_SIZE / 1024);
        return true;
#else
        // POSIX shared memory. A name must start with a single slash and contain
        // no others, and the endpoint comes from a caller, so anything outside
        // [A-Za-z0-9_-] becomes an underscore rather than a path separator or a
        // way out of /dev/shm.
        m_MapName = "/EnjinEditorBridge_";
        for (char c : endpoint) {
            const unsigned char u = static_cast<unsigned char>(c);
            m_MapName += (std::isalnum(u) || c == '_' || c == '-') ? c : '_';
        }
        // POSIX caps the name at NAME_MAX; keep well under it.
        if (m_MapName.size() > 200) m_MapName.resize(200);

        // O_EXCL first tells us whether we are the one creating it, which
        // decides who sizes it and who unlinks it at the end.
        m_OwnsSegment = true;
        int fd = shm_open(m_MapName.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
        if (fd < 0 && errno == EEXIST) {
            m_OwnsSegment = false;
            fd = shm_open(m_MapName.c_str(), O_RDWR, 0600);
        }
        if (fd < 0) {
            ENJIN_LOG_ERROR(Editor, "SharedMemoryTransport: shm_open('%s') failed (errno %d)",
                            m_MapName.c_str(), errno);
            return false;
        }
        if (m_OwnsSegment && ftruncate(fd, static_cast<off_t>(BUFFER_SIZE)) != 0) {
            ENJIN_LOG_ERROR(Editor, "SharedMemoryTransport: ftruncate failed (errno %d)", errno);
            close(fd);
            shm_unlink(m_MapName.c_str());
            return false;
        }
        // The peer may have created the object but not sized it yet; mapping a
        // short segment would fault on first touch rather than fail here.
        struct stat st = {};
        if (fstat(fd, &st) != 0 || static_cast<u64>(st.st_size) < BUFFER_SIZE) {
            ENJIN_LOG_ERROR(Editor, "SharedMemoryTransport: segment is %lld bytes, need %u",
                            static_cast<long long>(st.st_size), BUFFER_SIZE);
            close(fd);
            if (m_OwnsSegment) shm_unlink(m_MapName.c_str());
            return false;
        }

        void* mapped = mmap(nullptr, BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        // The mapping keeps the segment alive on its own, so the descriptor is
        // not needed past this point.
        close(fd);
        if (mapped == MAP_FAILED) {
            ENJIN_LOG_ERROR(Editor, "SharedMemoryTransport: mmap failed (errno %d)", errno);
            if (m_OwnsSegment) shm_unlink(m_MapName.c_str());
            return false;
        }
        m_Buffer = static_cast<u8*>(mapped);

        auto* header = reinterpret_cast<RingHeader*>(m_Buffer);
        if (header->magic != MAGIC) {
            // Whoever gets here first initializes the ring, same as Windows.
            memset(m_Buffer, 0, BUFFER_SIZE);
            header->magic = MAGIC;
            header->writePos = 0;
            header->readPos = 0;
        }

        m_Connected = true;
        ENJIN_LOG_INFO(Editor, "SharedMemoryTransport: connected to '%s' (%u KB)",
                       m_MapName.c_str(), BUFFER_SIZE / 1024);
        return true;
#endif
    }

    void Disconnect() override {
#ifdef _WIN32
        if (m_Buffer) { UnmapViewOfFile(m_Buffer); m_Buffer = nullptr; }
        if (m_MapHandle) { CloseHandle(m_MapHandle); m_MapHandle = nullptr; }
#else
        if (m_Buffer) { munmap(m_Buffer, BUFFER_SIZE); m_Buffer = nullptr; }
        // Unlink only from the side that created it. A POSIX shared-memory
        // object outlives every process that mapped it, so without this a
        // crashed editor leaves a megabyte in /dev/shm until reboot. Unlinking
        // removes the name; a peer that still has it mapped keeps working.
        if (m_OwnsSegment && !m_MapName.empty()) {
            shm_unlink(m_MapName.c_str());
            m_OwnsSegment = false;
        }
#endif
        m_Connected = false;
    }

    bool IsConnected() const override { return m_Connected; }

    bool Send(const EditorMessage& msg) override {
        if (!m_Connected || !m_Buffer) return false;

        // Serialize: header + payload
        usize totalSize = sizeof(EditorMessageHeader) + msg.payload.size();
        if (totalSize > DATA_SIZE) {
            ENJIN_LOG_WARN(Editor, "SharedMemoryTransport: message too large (%zu bytes)", totalSize);
            return false;
        }

        auto* header = reinterpret_cast<RingHeader*>(m_Buffer);
        u8* data = m_Buffer + sizeof(RingHeader);

        // Simple linear write (not a true ring buffer yet — sufficient for prototype)
        u32 writePos = header->writePos;
        if (writePos + totalSize + 4 > DATA_SIZE) {
            writePos = 0; // Wrap
        }

        // Write length prefix + message
        u32 len = static_cast<u32>(totalSize);
        memcpy(data + writePos, &len, 4);
        memcpy(data + writePos + 4, &msg.header, sizeof(EditorMessageHeader));
        if (!msg.payload.empty()) {
            memcpy(data + writePos + 4 + sizeof(EditorMessageHeader),
                   msg.payload.data(), msg.payload.size());
        }

        header->writePos = writePos + 4 + static_cast<u32>(totalSize);
        return true;
    }

    bool Receive(EditorMessage& outMsg) override {
        if (!m_Connected || !m_Buffer) return false;

        auto* header = reinterpret_cast<RingHeader*>(m_Buffer);
        u8* data = m_Buffer + sizeof(RingHeader);

        if (header->readPos >= header->writePos) return false; // Nothing to read

        u32 readPos = header->readPos;
        u32 len = 0;
        memcpy(&len, data + readPos, 4);
        if (len == 0 || readPos + 4 + len > DATA_SIZE) {
            header->readPos = 0; // Reset on corruption
            return false;
        }

        memcpy(&outMsg.header, data + readPos + 4, sizeof(EditorMessageHeader));
        usize payloadSize = len - sizeof(EditorMessageHeader);
        if (payloadSize > 0) {
            outMsg.payload.resize(payloadSize);
            memcpy(outMsg.payload.data(), data + readPos + 4 + sizeof(EditorMessageHeader), payloadSize);
        } else {
            outMsg.payload.clear();
        }

        header->readPos = readPos + 4 + len;
        return true;
    }

    bool HasPendingMessages() const override {
        if (!m_Connected || !m_Buffer) return false;
        auto* header = reinterpret_cast<const RingHeader*>(m_Buffer);
        return header->readPos < header->writePos;
    }

private:
    static constexpr u32 BUFFER_SIZE = 1024 * 1024; // 1 MB shared memory
    static constexpr u32 DATA_SIZE = BUFFER_SIZE - sizeof(u32) * 4; // Header overhead
    static constexpr u32 MAGIC = 0x454E4A42; // "ENJB"

    struct RingHeader {
        u32 magic;
        u32 writePos;
        u32 readPos;
        u32 _pad;
    };

    bool m_Connected = false;
    u8* m_Buffer = nullptr;
    std::string m_MapName;

#ifdef _WIN32
    HANDLE m_MapHandle = nullptr;
#else
    bool m_OwnsSegment = false;   // only the creator unlinks the name
#endif
};

// Factory function for creating the platform-appropriate transport
std::unique_ptr<IEditorTransport> CreateSharedMemoryTransport() {
    return std::make_unique<SharedMemoryTransport>();
}

} // namespace Editor
} // namespace Enjin
