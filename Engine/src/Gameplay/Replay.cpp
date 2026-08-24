#include "Enjin/Gameplay/Replay.h"
#include <nlohmann/json.hpp>
#include <cstring>

using json = nlohmann::json;

namespace Enjin {
namespace Gameplay {

std::string SerializeReplay(const ReplayData& replay) {
    json j;
    j["tege_replay"] = replay.version;
    j["engineVersion"] = replay.engineVersion;
    j["fixedDt"] = replay.fixedDt;
    if (replay.rngSeed != 0) j["rngSeed"] = replay.rngSeed;
    j["scene"] = replay.sceneJson;
    json frames = json::array();
    for (const auto& f : replay.frames) {
        json fj;
        if (!f.keysDown.empty()) fj["k"] = f.keysDown;
        if (f.mouseMask != 0) fj["b"] = f.mouseMask;
        fj["m"] = json::array({f.mouseX, f.mouseY});
        fj["d"] = f.dt;
        frames.push_back(std::move(fj));
    }
    j["frames"] = std::move(frames);
    if (!replay.endState.empty()) {
        json ends = json::array();
        for (const auto& e : replay.endState) {
            ends.push_back(json{
                {"n", e.name},
                {"p", {e.position.x, e.position.y, e.position.z}},
                {"q", {e.rotX, e.rotY, e.rotZ, e.rotW}},
            });
        }
        j["endState"] = std::move(ends);
    }
    if (!replay.bookmarks.empty()) {
        json marks = json::array();
        for (const auto& b : replay.bookmarks) {
            marks.push_back(json{{"f", b.frame}, {"t", b.time}, {"l", b.label}});
        }
        j["bookmarks"] = std::move(marks);
    }
    return j.dump();
}

bool ParseReplay(const std::string& text, ReplayData& out) {
    json j;
    try {
        j = json::parse(text);
    } catch (const std::exception&) {
        return false;
    }
    if (!j.contains("tege_replay") || !j.contains("frames") || !j["frames"].is_array())
        return false;
    out.version = j.value("tege_replay", 1u);
    out.engineVersion = j.value("engineVersion", std::string());
    out.fixedDt = j.value("fixedDt", 1.0f / 60.0f);
    if (out.fixedDt <= 0.0f || out.fixedDt > 1.0f) out.fixedDt = 1.0f / 60.0f;
    out.rngSeed = j.value("rngSeed", 0u);
    out.sceneJson = j.value("scene", std::string());
    out.frames.clear();
    out.frames.reserve(j["frames"].size());
    for (const auto& fj : j["frames"]) {
        ReplayFrame f;
        if (fj.contains("k") && fj["k"].is_array())
            for (const auto& k : fj["k"])
                if (k.is_number_unsigned() && k.get<u32>() < 512u)
                    f.keysDown.push_back(static_cast<u16>(k.get<u32>()));
        f.mouseMask = static_cast<u8>(fj.value("b", 0u));
        if (fj.contains("m") && fj["m"].is_array() && fj["m"].size() >= 2) {
            f.mouseX = fj["m"][0].get<f32>();
            f.mouseY = fj["m"][1].get<f32>();
        }
        f.dt = fj.value("d", out.fixedDt);
        if (f.dt <= 0.0f || f.dt > 1.0f) f.dt = out.fixedDt;
        out.frames.push_back(std::move(f));
    }
    out.endState.clear();
    if (j.contains("endState") && j["endState"].is_array()) {
        for (const auto& ej : j["endState"]) {
            ReplayEndEntity e;
            e.name = ej.value("n", std::string());
            if (e.name.empty()) continue;
            if (ej.contains("p") && ej["p"].is_array() && ej["p"].size() >= 3) {
                e.position = Math::Vector3(ej["p"][0].get<f32>(), ej["p"][1].get<f32>(), ej["p"][2].get<f32>());
            }
            if (ej.contains("q") && ej["q"].is_array() && ej["q"].size() >= 4) {
                e.rotX = ej["q"][0].get<f32>(); e.rotY = ej["q"][1].get<f32>();
                e.rotZ = ej["q"][2].get<f32>(); e.rotW = ej["q"][3].get<f32>();
            }
            out.endState.push_back(std::move(e));
        }
    }
    out.bookmarks.clear();
    if (j.contains("bookmarks") && j["bookmarks"].is_array()) {
        for (const auto& bj : j["bookmarks"]) {
            ReplayBookmark b;
            b.frame = bj.value("f", 0u);
            b.time = bj.value("t", 0.0f);
            b.label = bj.value("l", std::string());
            if (b.time < 0.0f) b.time = 0.0f;
            out.bookmarks.push_back(std::move(b));
        }
    }
    return true;
}

void ReplayFrameFromBuffers(ReplayFrame& out, const bool* keys512,
                            const bool* mouse8, Math::Vector2 mousePos) {
    out.keysDown.clear();
    for (u16 k = 0; k < 512; ++k)
        if (keys512[k]) out.keysDown.push_back(k);
    out.mouseMask = 0;
    for (u8 b = 0; b < 8; ++b)
        if (mouse8[b]) out.mouseMask |= static_cast<u8>(1u << b);
    out.mouseX = mousePos.x;
    out.mouseY = mousePos.y;
}

void ReplayFrameToBuffers(const ReplayFrame& frame, bool* keys512,
                          bool* mouse8, Math::Vector2& mousePos) {
    std::memset(keys512, 0, 512);
    std::memset(mouse8, 0, 8);
    for (u16 k : frame.keysDown)
        if (k < 512) keys512[k] = true;
    for (u8 b = 0; b < 8; ++b)
        if (frame.mouseMask & (1u << b)) mouse8[b] = true;
    mousePos = Math::Vector2(frame.mouseX, frame.mouseY);
}

} // namespace Gameplay
} // namespace Enjin
