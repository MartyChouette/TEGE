#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <fstream>
#include <string>
#include <vector>

namespace Enjin {
namespace Editor {

// R1: dependency-free GIF89a recorder. Feed it RGBA frames (the game-view
// readback); it downscales, quantizes to a per-frame 256-color palette
// (popularity in a 32x32x32 histogram), LZW-encodes, and streams straight to
// disk - memory use stays flat no matter how long the recording runs.
//
// Fidelity: downscale is a shift (0 = full res, 1 = half, 2 = quarter);
// capture cadence is the CALLER's job (call AddFrame at the rate you want,
// pass the real elapsed time as delayMs - GIF time base is 10ms).
class ENJIN_API GifRecorder {
public:
    // Starts a recording. width/height = INCOMING frame size (pre-downscale).
    // Returns false if the file can't be opened.
    bool Start(const std::string& path, u32 width, u32 height, u32 downscaleShift);

    // rgba = width*height*4 bytes (the size given to Start). delayMs = time
    // this frame stays on screen. Frames after the first must match the size.
    void AddFrame(const u8* rgba, f32 delayMs);

    // Finalizes the file. Safe to call when not recording.
    void Stop();

    bool IsRecording() const { return m_File.is_open(); }
    u32 FrameCount() const { return m_Frames; }
    const std::string& Path() const { return m_Path; }

private:
    void WriteFrame(const std::vector<u8>& indices, const u8* palette, f32 delayMs);

    std::ofstream m_File;
    std::string m_Path;
    u32 m_SrcW = 0, m_SrcH = 0;    // incoming frame size
    u32 m_W = 0, m_H = 0;          // output (downscaled) size
    u32 m_Shift = 0;
    u32 m_Frames = 0;
};

} // namespace Editor
} // namespace Enjin
