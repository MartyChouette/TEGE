I'll start by reading the project's CLAUDE.md and getting a lay of the land.

---

Now let me survey the scope directories and start hunting.

---

There's a worktree copy polluting searches. Let me exclude it and re-run.

---

Let me fan out on the big directories while I verify specifics myself.

---

Found something significant. Let me verify it.

---

Strong results. Let me independently verify the top claims.

---

Let me re-verify the zero-reference claims with looser matching — my word boundaries may have hidden derived names.

---

I have enough verified material. Let me do a final consistency check on the dead-code totals.

---

# Editor / Build / Assets / Tooling Audit

Every claim below I verified by reading the code and running a repo-wide caller search (excluding `.claude/worktrees/`, which holds stale full copies of the repo and poisons naive greps). I discarded several candidates that didn't survive verification — noted at the end.

---

## Rank 1 — Broken behaviour users will hit

### 1.1 The web build silently empties every VisualScript, BehaviorTree and QuestFlow graph
`Engine/src/AI/NodeGraphStub.cpp:83-84`

```cpp
nlohmann::json NodeGraphData::ToJson() const { return nlohmann::json{{"nodes",array()},{"links",array()}}; }
void NodeGraphData::FromJson(const nlohmann::json&) { Clear(); }
```

The web CMake branch excludes `src/Editor/*.cpp` (`Engine/CMakeLists.txt:92`), which drops the real `NodeGraphData` implementation in `Engine/src/Editor/NodeGraph.cpp:182-283`. `NodeGraphStub.cpp` re-implements the data methods for web — its header comment claims "Full implementations" — but `ToJson`/`FromJson` are stubs.

`SceneSerializer.cpp:4900, 4978, 5241, 5311` calls `graph.FromJson(...)` when loading FlowNode, VisualScript, BehaviorTree and QuestFlow components, and it compiles on web. `Player/src/web_main.cpp:340-341, 894-895, 1551-1557` fully initialises and ticks `VisualScriptSystem` and `BehaviorTreeSystem` every frame. `VisualScriptExecutor.cpp:90, 578, 631` and `BehaviorTreeExecutor.cpp:102-121` then walk a graph that has zero nodes.

**Cost:** visual scripting and behaviour-tree AI are inert in every web export. No error, no log line. Compounding it, `web_main.cpp` never sets any of the `s_VisualScript*` system pointers that `NodeRegistry.cpp:58-73` declares and the desktop player sets at `Player/src/main.cpp:2351+`, so even a populated graph would no-op on save/weather/water/UI/audio nodes.

**Fix:** `NodeGraph.cpp` lines 1-295 are pure data — no ImGui until line 297. Move them to `Engine/src/VisualScript/NodeGraphData.cpp` (compiled on all platforms) and delete `NodeGraphStub.cpp`. Same split on the header (see 4.5). Then wire the `s_VisualScript*` pointers in `web_main.cpp`.

### 1.2 Web build reports "Build complete!" and opens a browser when no WASM was produced
`Engine/src/Build/BuildPipeline.cpp:85-88`

```cpp
if (!HTML5Exporter::InvokeEmscriptenBuild(config.outputDir, pakPath)) {
    AddMessage(MessageSeverity::Warning, "Emscripten build failed — WASM output may be missing...");
}
```

Warning only. Phase 5 (`:113-129`) verifies `game.enjpak` and nothing else, then `:135` sets `m_Result.success = true`. `EditorLayerDialogs.cpp:1597-1607` shows a green "Build complete!" toast, starts the dev server and calls `OpenUrlPreferChromium` unconditionally.

`InvokeEmscriptenBuild` (`HTML5Exporter.cpp:202-305`) can only succeed inside a source checkout or with `emcmake` on PATH — its own comment says so.

**Cost:** `File > Build Game` with target Web on an installed editor → green success toast → browser opens on a page that 404s `EnjinPlayer.js`/`.wasm`. The desktop *run* path was already fixed to be honest (`:1628-1636`); the web branch was not.

**Fix:** Phase 5 should verify `EnjinPlayer.js` + `EnjinPlayer.wasm` (web) / the exe (desktop) and fail the build otherwise. `CopyPlayer` failure at `BuildPipeline.cpp:611` has the identical problem.

### 1.3 `m_FeedbackTab` is written from 13 places and read from none
`Engine/include/Enjin/Editor/EditorLayer.h:1718`

The Feedback panel's tab bar (`EditorLayerPanels.cpp:6019-6050`) writes `m_FeedbackTab` to record the active tab, and 6 other sites write it to *request* a tab — `EditorLayerMenuBar.cpp:1216` (Help > Send Feedback…), `EditorLayerPanels.cpp:5533, 5541, 6063, 6381, 6619`, and `:6631` where a "Go to Settings" button's entire body is `m_FeedbackTab = FeedbackTab::GitHubSettings;`. No `BeginTabItem` is ever passed `ImGuiTabItemFlags_SetSelected`.

**Cost:** every "jump to this tab" action in the bug-report/feedback system is a no-op. "Send Feedback…" opens the panel on Bug Reports. The GitHub-not-configured empty state tells the user to go to Settings and hands them a button that does nothing.

**Fix:** add `FeedbackTab m_FeedbackTabRequest`, pass `ImGuiTabItemFlags_SetSelected` when it matches, clear after. ~10 lines.

### 1.4 Template Marketplace "Install" fabricates an empty template that then wipes the open scene
`Engine/src/Editor/TemplateMarketplace.cpp:251-292`

`Install` downloads nothing. It writes `templates/<id>/meta.json` from the hardcoded catalog and `scene.enjin` containing `{ "entities": [] }`, then returns `true`. `EditorLayerProjectHub.cpp:5850-5851` reports `Installed: <name>` as Success. The catalog (`TemplateMarketplace.cpp:57-152`) advertises author, version, license, size, download counts and star ratings, and reuses the ids of the real code-generated templates (`EditorLayer::ApplyTemplate`, `EditorLayerProjectHub.cpp:2360`).

The installed dir is then picked up by `TemplateCreator::ScanTemplates` and appears under Custom Templates. Its **Load** button (`EditorLayerProjectHub.cpp:5598-5610`) reaches `SceneSerializer::Load(..., clearExisting=true)`, which calls `m_World->Clear()` at `SceneSerializer.cpp:9077` *before* parsing.

**Cost:** the user's open scene is destroyed and replaced by nothing, and the console prints `[Template] Loaded: <name>`.

**Fix:** the marketplace has no backend. Either remove the panel and the menu entry (`EditorLayerMenuBar.cpp:646-651`) or make `Install` copy from real template content.

### 1.5 Hub and editor cannot see each other's projects
`Hub/src/HubApplication.cpp:248` writes the manifest as literally `.enjinproject`; `:181` scans for `filename() == ".enjinproject"`.

The editor writes `<ProjName>.enjinproject` (`EditorLayerScene.cpp:243`, `EditorLayerDialogs.cpp:1712`) and detects projects with `entry.path().extension() == ".enjinproject"` (`EditorLayerScene.cpp:150, 173, 447`). For a file named `.enjinproject`, `std::filesystem::path::extension()` returns `""` — a leading-dot name with no other period has no extension.

**Cost:** total mutual invisibility. Hub-created projects are never auto-detected by the editor; editor-created projects never appear in the Hub's list. Hub is `ENJIN_BUILD_HUB=OFF` by default, which is why this has gone unnoticed.

### 1.6 Scene List "Load" reports success after the world is already cleared
`Engine/src/Editor/EditorLayerPanels.cpp:1382-1404` — three call sites (double-click, `Load`, `Load Additive`) discard the `bool` from `SceneManager::LoadScene`/`LoadSceneAdditive`, which has four failure returns (`SceneManager.cpp:527-565`), and unconditionally show a green `Scene loaded: <name>`. `SceneSerializer::Load` clears the world before it can fail.

**Cost:** a moved or corrupt `.enjin` gives an empty viewport plus a success toast. Every other scene-load path in the editor checks the result (`EditorLayerScene.cpp:388, 392`, `EditorLayerMenuBar.cpp:295-303`) — this panel is the exception.

---

## Rank 2 — The same problem solved N times

### 2.1 "Run a child process and wait" is hand-rolled 10 times
`Desktop.h` centralised *launch and forget*. Run-and-wait (and run-and-capture) was not, and each site carries its own Windows/POSIX pair:

| Site | Shape |
|---|---|
| `Engine/src/Build/HTML5Exporter.cpp:171` | `RunProcess`, wait, 5-min timeout on Win / none on POSIX |
| `Engine/src/Editor/EditorLayerGit.cpp:135` | `RunGitCommand`, wait + capture stdout |
| `Engine/src/Editor/EditorLayerProjectHub.cpp:2277-2295` | git init, wait, **exit status discarded** on both branches |
| `Engine/src/Editor/EditorLayerComponents.cpp:6835-6849` | IDE launch, fire-and-forget |
| `Engine/src/Editor/EditorLayerComponents.cpp:6886-6889` | VS Code goto-line, fire-and-forget |
| `Engine/src/Platform/FileDialog.cpp:212` | `ExecuteCommand`, posix_spawn + pipe (macOS) |
| `Engine/src/Platform/FileDialog.cpp:368` | `ExecuteCommand`, byte-identical **except** it checks `WIFEXITED`/`WEXITSTATUS` (Linux) |
| `Engine/src/Platform/FileDialog.cpp:421` | inline spawn |
| `Engine/src/Editor/EditorBridge.cpp:222/236` | `CreateProcessA` / `fork+execlp` |
| `Hub/src/HubApplication.cpp:397` | **`std::system(cmd + " &")`** |

The Hub line is the last remaining violation of the documented no-`std::system` rule. It is also wrong twice over: `system("cmd &")` returns the shell's status, which is 0 as soon as the job backgrounds, so it returns `true` even when `EnjinEditor` doesn't exist; and the path is interpolated into `/bin/sh` unquoted.

`Core/include/Enjin/Platform/LinuxPlatform.h:57` already declares exactly the right function — `ProcessResult LaunchProcess(...)` with capture and a timeout — but it is Linux-only, so nothing portable can use it. Its **only** caller in the whole tree is the dead `AppImageBuilder` (see 3.1).

**Fix:** promote it to `Enjin/Platform/Process.h` alongside `Desktop.h` (`RunProcess(exe, args, workingDir, captureOutput, timeoutMs) -> {exitCode, output}`) and collapse the ten copies. Same pattern that worked for `Desktop.h` and `ShaderSearchPaths`.

### 2.2 `ShellEscape` — five byte-identical copies
`EditorLayer.cpp:168`, `EditorLayerComponents.cpp:155`, `EditorLayerGit.cpp:131`, `FileDialog.cpp:201`, `FileDialog.cpp:357`. All inside POSIX guards, all identical. Belongs next to the process helper in Core.

### 2.3 "Walk up from a scene to find its `.enjinproject`" — three copies, three different depths
`EditorLayerScene.cpp:147-162` (depth < 3), `:168-190` (depth < 3), `:441-456` (depth < **4**). All three also hand-roll the "is this path under root" test with `lexically_relative` + `rfind("..", 0)` (`:144`, `:440`, plus `EditorLayerComponents.cpp:6916, 6969` and `EditorLayerPanels.cpp:673`) when `Platform::MakeRelativeToRoot` exists for it.

**Cost:** a scene four directories below its project root gets a "this scene belongs to another project" warning from `FindMismatchedProjectForScene` while `AutoDetectProjectForScene` gives up and offers to create a *new* project for it. The two functions disagree by construction.

### 2.4 The image-extension list — twelve copies, already divergent
`ThumbnailGenerator.cpp:78`, `EditorLayerPanels.cpp:744`, `EditorLayerComponents.cpp:820, 960, 1158, 4038, 4059`, `EditorLayerRendering.cpp:693`, `EditorLayerViewport.cpp:258`, `EditorLayerScene.cpp:806`, plus `:1019, 1168` for the `.png|.tga` alpha test. `EditorLayerScene.cpp:806` already omits `.svg` that the other eleven include. Two of the twelve are already named predicate functions; nothing shares them.

### 2.5 `GetComponentIcon` — two identical 24-entry tables
`EditorLayerComponents.cpp:167` and `EditorLayerHierarchy.cpp:123`. Currently in sync; any new component's icon must be added twice or the hierarchy and inspector disagree.

### 2.6 Resource lookups that miss the installed layout (same class as the 19 shader arrays)
- **User manual:** `EditorLayerPanels.cpp:3463` and `:3541` each carry an identical four-entry array `docs/`, `../docs/`, `../../docs/`, `../../../docs/` plus an identical read loop. `Application::InitializeEngine` sets CWD to the exe dir, so from `<prefix>/bin` none of them reach `<prefix>/share/doc/enjin/USER_MANUAL.md`, which is where `install(DIRECTORY docs/ ...)` (`CMakeLists.txt:209`) puts it. The panel shows "Manual Not Found" in every installed build — honest, but wrong. It only works from a build tree.
- **Templates:** eight sites resolve a bare relative `"templates"` (`EditorLayerMenuBar.cpp:649`, `EditorLayerProjectHub.cpp:5427, 5464, 5493, 5533, 5541, 5601, 5618`). Installed templates go to `<prefix>/share/enjin/templates` (`CMakeLists.txt:214`) and nothing looks there. There is also no `templates/` directory in the repo at all, so the `if(EXISTS)` guard means it never installs and the Custom Templates list is always empty.

**Fix:** a `Platform::ResourceSearchPaths(subdir, fileName)` mirroring `Renderer::ShaderSearchPaths`, with `../share/enjin/` and `../share/doc/enjin/` in it.

---

## Rank 3 — Dead code

### 3.1 ~5,060 lines of `.cpp` with zero callers anywhere in Engine/Core/Editor/Player/Hub/Tests

Verified with loose substring greps (not word-boundary — that is what produced my own two near-misses, see the bottom).

| File | Lines | Note |
|---|---|---|
| `Engine/src/Build/AppImageBuilder.cpp` | 400 | Confirmed zero callers. `BuildTargetPlatform` (`BuildReport.h:11`) is only `Desktop`/`Web`; Linux ships via CPack DEB. Sole consumer of `LinuxPlatform::LaunchProcess` / `IsCommandAvailable` — and `LaunchProcessAsync`, `OpenWithDefault`, `GetDesktopEnvironment`, `GetDisplayServerType`, `GetXDGRuntimeDir` have zero callers at all |
| `Engine/src/Platform/SteamDeck.cpp` | 369 | class `SteamDeckSupport`, 0 refs, no UI mentions it |
| `Engine/src/Platform/SteamInput.cpp` | 313 | class `SteamInputManager`, 0 refs |
| `Engine/src/Platform/SwitchPlatform.cpp` | 145 | class `SwitchPlatform`, 0 refs |
| `Engine/src/Editor/SymbolLibrary.cpp` | 1160 | class `SymbolLibrary`, 0 refs (`FlashTimelineEditor::DrawSymbolLibrary` is an unrelated method name) |
| `Engine/src/Editor/CollaborativeEditingUI.cpp` | 952 | **duplicate** of the live `EditorLayer::DrawCollaborationPanel` (`EditorLayerPanels.cpp:5122`); its 8 `ApplyXxx` remote-op handlers are a second copy of the logic in `EditorLayer.cpp:516-570` |
| `Engine/src/Editor/CollabMergeUI.cpp` | 276 | 0 refs |
| `Engine/src/Editor/EditorBridge.cpp` | 267 | 0 refs |
| `Engine/src/Editor/SharedMemoryTransport.cpp` | 252 | reachable only via `CreateSharedMemoryTransport()`, called only from `Tests/Unit/Editor/TestSharedMemoryTransport.cpp` |
| `Engine/src/GUI/InventoryUI.cpp` | 326 | class `GUI::InventoryUI`, 0 refs — unrelated to the live `InventoryComponent` |
| `Engine/src/GUI/MinimapRenderer.cpp` | 230 | 0 refs — unrelated to the live HUD `WidgetType::Minimap` |
| `Engine/src/GUI/ShaderGUI.cpp` | 144 | 0 refs |
| `Engine/src/GUI/DialogueAsset.cpp` | 111 | 0 refs |
| `Engine/src/Editor/EditorProtocol.cpp` | 46 | test-only |
| `Engine/src/Editor/P2PAuthority.cpp` | 40 | `CollaborativeEditingSystem::SetAuthority` (`CollaborativeEditing.h:295`) is never called — `m_Authority` is permanently null |
| `Engine/src/Editor/ServerAuthority.cpp` | 29 | same |

All are compiled into `EnjinEngine` by `file(GLOB_RECURSE src/*.cpp)`.

### 3.2 `WebStubs.cpp` looks like it handles web's null system pointers; it doesn't
`Engine/src/Scripting/WebStubs.cpp:18-32` defines 12 `void* s_VisualScript*` in namespace `Enjin::VisualScript`. The real globals (`NodeRegistry.cpp:58-73`) are at **global scope** with typed pointers. Nothing references the namespaced ones. A maintainer reading this file concludes web's system pointers are handled; they are not (see 1.1).

---

## Rank 4 — Wasted work and small broken paths

### 4.1 OIT allocates ~19 MB of VRAM plus a pipeline on every desktop run and is never used
`RenderSystem::Initialize` (`RenderSystem.cpp:4272-4277`) unconditionally constructs and initialises `OITManager`, which creates an RGBA16F accumulation image + R8 revealage image (`OITManager.cpp:682, 724`), a render pass, framebuffer, composite pipeline and descriptor sets. `m_OITManager->` appears exactly twice in the whole tree: that `Initialize` and the `Shutdown` at `:4616`. `BeginTransparentPass`, `EndTransparentPass` and `CompositePass` are never called.

`m_OITEnabled` (`RenderSystem.h:2411`) gates nothing — its only three references are the checkbox at `EditorLayerRendering.cpp:2654-2656` and `EditorLayerPanels.cpp:7928`, which then prints **`OIT: Enabled`**. At 1920×1080 that's ~18.7 MB held for the process lifetime in the editor *and* every exported desktop game.

### 4.2 Per-frame `stat()` in draw code
- `EditorLayerProjectHub.cpp:495` — one `filesystem::exists` per recent project, every frame the Project Hub is visible. 20 recents at 60 fps ≈ 1200 syscalls/s; a recent project on an unreachable network path stalls the UI.
- `EditorLayerComponents.cpp:1469-1473` — 1-2 `exists` per frame per selected Text entity (font-missing warning).
- `EditorLayerSettings.cpp:2583-2609` — `DrawSettingsSection_BuildScenes` re-scans `scenes/` with a `directory_iterator` and an O(n·m) compare **every frame** the section is expanded, calling `SceneManager::AddScene` from inside a draw function.

Cache these behind the same dirty-flag pattern the asset browser already uses correctly.

### 4.3 Gamepad radial menu: release-to-select cannot fire
`EditorLayerGamepad.cpp:139-142` — on bumper release it calls `DrawRadialMenu(type)` "to resolve the action", but `DrawRadialMenu` only executes at `:348` when `IsGamepadButtonPressed(GamepadButton::A)` is also true that frame. The documented "hold to open, release to select" never happens. Separately, `m_RadialMenuAngle` (`:159`) is a dead store — `DrawRadialMenu:296-305` recomputes the angle itself.

### 4.4 Empty handlers on enabled controls
- `EditorLayerPanels.cpp:5684-5687` — "Export as SVG" in the SWF Import tab; the handler body is a single `ImGui::TextDisabled` call. `VectorDrawingEditor::ExportSVG` exists and is never reached from here.
- `FlashTimeline.cpp:430-432` — layer context-menu "Rename", empty body, sits between working Delete/Move Up/Move Down.
- `Hub/src/HubApplication.cpp:638-640` — `File > Open Project… (Ctrl+O)`, zero-statement body.
- `Hub/src/HubApplication.cpp:539-550` — `LoadSettings` opens nothing while `SaveSettings` (`:498-536`) writes a real JSON file with theme, directories and the whole recent-projects list. The user clicks "Save Settings" (`:921`), a file appears, and everything is gone on restart.
- `Hub/src/HubApplication.cpp:403-410` — `CreateFromTemplate` is a `std::cout` that returns `true`; all 43 templates advertised in `LoadTemplates()` (`:412-461`) produce a blank project.
- `Engine/src/Editor/SymbolLibrary.cpp:1087-1099` — `GenerateThumbnail` writes a 0-byte `thumbnail.png` and stores its path in the persisted catalog. (Moot while the class is dead, per 3.1.)

### 4.5 Editor coupling — concrete cost
`Engine/src/Editor` is 83,653 lines (27% of `Engine/src`) across 59 of 406 TUs, compiled into `EnjinEngine`, which `EnjinPlayer` links. Two specific costs:

- **`Enjin/Editor/NodeGraph.h` includes `<imgui.h>` and `<nlohmann/json.hpp>`** and is pulled in by `ECS/Components/VisualScript.h:6`, `AI/BehaviorTree.h:6`, `Gameplay/QuestFlow.h:7` and `VisualScript/NodeDefinition.h:6`. **41 TUs** — including runtime AI, quest and visual-script code — parse all of ImGui for a data struct that doesn't use it. The 1.1 fix (splitting `NodeGraphData` out) removes this at the same time.
- **`ECS/Systems/RenderSystem.h:92` includes `Enjin/Editor/FlashTimeline.h`** for one struct, `Editor::OnionSkinGhost`. 25 TUs include `RenderSystem.h`, so every edit to the Flash timeline editor rebuilds the renderer. Moving `OnionSkinGhost` into a renderer header is a one-struct change.
- `GUI/GameMenus.h:7` and `GUI/ImGuiLayer.h:8` include `Enjin/Editor/EditorSettings.h` — that one is cheap (no heavy includes), but it means the player's game menus name an editor type.

---

## What I did *not* find

Category 4 (wasted work) is genuinely thinner than the brief anticipated, and I'd rather say so than pad. The asset browser is properly cached behind `m_AssetBrowserCacheDirty` (`EditorLayerPanels.cpp:602-604`). `World::GetEntitiesWithComponent` returns `const&`, so the 40+ calls in draw code allocate nothing. Every `ifstream` I traced inside a `Draw*` function turned out to be behind a button. The four items in 4.1-4.2 are the real ones.

**Two claims I withdrew during verification**, both from word-boundary greps that hid a derived name — the exact failure mode of the previous audit:
- `Engine/src/Editor/ProceduralGraph.cpp` (1,241 lines) looked dead; it is live via `ProceduralGraphEditor m_ProcGraphEditor` (`EditorLayer.h:1419`).
- `Engine/src/Editor/CRDTState.cpp` (166 lines) looked dead; class `CRDTDocument` is live via `m_CRDTDoc` (`CollaborativeEditing.cpp:132, 361, 420, 724`).
- `Assets::CopyToProjectAssets` also looked orphaned until I widened the search — it has three callers in the editor.

One background search on editor dead code has not returned; the dead-code list above is my own independently verified work and does not depend on it.

## Suggested order

1. Split `NodeGraphData` out of the editor (fixes 1.1 and half of 4.5 in one change; low risk, no behaviour change on desktop).
2. `m_FeedbackTab` request flag (1.3) — ~10 lines, obvious win.
3. Check the return values at `EditorLayerPanels.cpp:1382-1404` (1.6) and escalate the build verification in `BuildPipeline.cpp:85-88, 611` (1.2).
4. Delete the 5,060 dead lines (3.1) and the misleading `WebStubs.cpp` block (3.2).
5. `Platform::RunProcess` + `ShellEscape` in Core; collapse the ten copies (2.1, 2.2).
6. Remove or implement the Template Marketplace (1.4) and decide whether the Hub is a product (1.5, 4.4).

---

The dead-code search returned. Let me verify its two highest-value additions before including them.

---

The second search has returned and corroborates my dead-code list. Here is what it adds, verified.

## Addendum

### New finding — `GitIntegration.h` is a 189-line header with no implementation file at all
`Engine/include/Enjin/Editor/GitIntegration.h:54` declares `class ENJIN_API GitIntegration` with ~20 out-of-line methods (`Initialize`, `Refresh`, `GetDiff`, `StageFile`, `StageAll`, `Commit`, `Push`, `Pull`, `Fetch`, `SwitchBranch`, `CreateBranch`, `InitRepository`, `DrawGitPanel`, `RunGitCommand`, `ParseStatusOutput`, `ParseLogOutput`, …).

There is no `GitIntegration.cpp` — `ls Engine/src/Editor | grep -i git` returns only `EditorLayerGit.cpp`, and `grep -rn "GitIntegration::" --include=*.cpp` over the whole tree returns **nothing**. Every declared method is undefined; anyone who includes this header and calls a method gets a link error.

The working git panel is `EditorLayer::DrawGitIntegrationPanel` (`Engine/src/Editor/EditorLayerGit.cpp:400`), which shells out directly. The only other hits on the token are the unrelated `EditorPanel::GitIntegration` bit flag (`EditorLayer.h:139`). This is a booby-trapped header, not merely dead code — delete it.

### Method-level dead code on live classes (each verified at exactly 2 hits: declaration + definition, no call site)

| Method | Location | ~LOC |
|---|---|---|
| `SWFConverter::ConvertToEntities` | `Engine/src/Assets/SWFConverter.cpp:549` | 120 |
| `TelemetrySystem::UploadToDiscord` | `Engine/src/Editor/TelemetrySystem.cpp:212` | 77 |
| `SWFLoader::ConvertToSVG` | `Engine/src/Assets/SWFLoader.cpp:1016` | 50 |
| `CollaborativeEditingSystem::DetectConflict` | `Engine/src/Editor/CollaborativeEditing.cpp:430` | 25 |
| `SpriteSheetImporter::CreateAnimation` | `Engine/src/Editor/SpriteSheetImporter.cpp:156` | 23 |
| `FeedbackManager::ExportAllAsJson` / `SearchBugReports` | `Engine/src/Editor/FeedbackSystem.cpp:508` / `:455` | 32 |
| `SWFLoader::ExtractBitmaps` | `Engine/src/Assets/SWFLoader.cpp:1151` | 16 |
| `PrefabManager::CreateFromEntities` / `UnregisterPrefab` | `Engine/src/Assets/Prefab.cpp:391` / `:721` | 15 |
| `CollaborativeEditingSystem::GoOffline` / `AttemptReconnect` | `Engine/src/Editor/CollaborativeEditing.cpp:194` / `:207` | 23 |
| `AssetLibrary::FilterByStyle` / `GetAssetPath` | `Engine/src/Assets/AssetLibrary.cpp:39` / `:75` | 12 |
| `FlashTimelineEditor::ImportFromSWFSprite` | `Engine/src/Editor/FlashTimeline.cpp:954` | 3 (already a stub — parameter commented out) |

The `GoOffline`/`AttemptReconnect`/`DetectConflict` cluster being dead is consistent with `CollabMergeUI` and `CollaborativeEditingUI` being dead — the offline/merge half of collaborative editing was never wired up.

### One claim from that search I checked and rejected
It reported that `FeedbackManager`'s "entire persistence layer is dead — bug reports are collected but never written." **That is wrong.** `SaveAll` and `LoadAll` have nine call sites (`EditorLayer.cpp:781, 1504`, `EditorLayerDialogs.cpp:2689, 2769, 2795`, `EditorLayerPanels.cpp:6349, 6369, 6570, 6585, 6615, 6776`). Only the per-item `SaveBugReport`/`SaveFeedback` and the two query/export methods are uncalled. Bug reports do persist.

### Net effect on the totals

Dead `.cpp` rises from ~5,060 to ~5,450 lines once the Tier-2 methods are counted, plus ~1,500 lines of dead headers, of which `GitIntegration.h` (189 lines, no implementation) is the one that will actively break someone.

Nothing in the addendum changes the ranking in my main report — items 1.1 through 1.6 remain the ones with user-visible consequences.

---

# Editor / Build / Assets / Tooling Audit — Main Findings

All claims verified by reading the code and running repo-wide caller searches, excluding `.claude/worktrees/` (stale full copies of the repo that poison naive greps).

---

## Rank 1 — Broken behaviour users hit

### 1.1 Web builds silently empty every VisualScript, BehaviorTree and QuestFlow graph
`Engine/src/AI/NodeGraphStub.cpp:83-84`
```cpp
nlohmann::json NodeGraphData::ToJson() const { return nlohmann::json{{"nodes",array()},{"links",array()}}; }
void NodeGraphData::FromJson(const nlohmann::json&) { Clear(); }
```
The web CMake branch excludes `src/Editor/*.cpp` (`Engine/CMakeLists.txt:92`), dropping the real implementation at `Engine/src/Editor/NodeGraph.cpp:182-283`. `NodeGraphStub.cpp` replaces it — its header comment claims "Full implementations" — but these two are stubs. `SceneSerializer.cpp:4900, 4978, 5241, 5311` calls `graph.FromJson(...)` on web; `web_main.cpp:340-341, 894-895, 1551-1557` ticks both systems every frame; `VisualScriptExecutor.cpp:90, 578, 631` and `BehaviorTreeExecutor.cpp:102-121` then walk a zero-node graph.

**Cost:** visual scripting and behaviour-tree AI are inert in every web export. No error, no log. Compounding it, `web_main.cpp` never sets the `s_VisualScript*` globals that `NodeRegistry.cpp:58-73` declares and desktop sets at `Player/src/main.cpp:2351+`.

**Fix:** `NodeGraph.cpp` lines 1-295 are pure data (no ImGui until line 297). Move to `Engine/src/VisualScript/NodeGraphData.cpp`, compiled on all platforms; delete `NodeGraphStub.cpp`. Wire the `s_VisualScript*` pointers in `web_main.cpp`.

### 1.2 Web build reports "Build complete!" and opens a browser when no WASM exists
`Engine/src/Build/BuildPipeline.cpp:85-88` — `InvokeEmscriptenBuild` failure is a **Warning** only. Phase 5 (`:113-129`) verifies `game.enjpak` and nothing else, then `:135` sets `success = true`. `EditorLayerDialogs.cpp:1597-1607` shows a green toast, starts the dev server and calls `OpenUrlPreferChromium` unconditionally. `InvokeEmscriptenBuild` (`HTML5Exporter.cpp:202-305`) can only succeed in a source checkout — its own comment says so.

**Cost:** on an installed editor, `File > Build Game` (Web) gives a success toast then opens a browser on a page that 404s `EnjinPlayer.js`/`.wasm`. The desktop *run* path was already fixed to be honest (`:1628-1636`); the web branch was not. `CopyPlayer` failure at `BuildPipeline.cpp:611` has the same shape.

**Fix:** Phase 5 must verify `EnjinPlayer.js` + `.wasm` (web) / the exe (desktop) and fail the build otherwise.

### 1.3 `m_FeedbackTab` is written from 13 places and read from none
`Engine/include/Enjin/Editor/EditorLayer.h:1718`. The tab bar (`EditorLayerPanels.cpp:6019-6050`) writes it to record the active tab; six other sites write it to *request* a tab — `EditorLayerMenuBar.cpp:1216` (Help > Send Feedback…), `EditorLayerPanels.cpp:5533, 5541, 6063, 6381, 6619`, and `:6631`, where a "Go to Settings" button's entire body is `m_FeedbackTab = FeedbackTab::GitHubSettings;`. No `BeginTabItem` is ever passed `ImGuiTabItemFlags_SetSelected`.

**Cost:** every "jump to this tab" action is a no-op. "Send Feedback…" lands on Bug Reports. The GitHub-not-configured empty state tells the user to go to Settings and gives them a button that does nothing.

**Fix:** add `m_FeedbackTabRequest`, pass `ImGuiTabItemFlags_SetSelected` when it matches, clear after. ~10 lines.

### 1.4 Template Marketplace "Install" fabricates an empty template that then wipes the open scene
`Engine/src/Editor/TemplateMarketplace.cpp:251-292` — `Install` downloads nothing. It writes `meta.json` from a hardcoded catalog plus `scene.enjin` containing `{ "entities": [] }`, returns `true`; `EditorLayerProjectHub.cpp:5850-5851` reports Success. The catalog (`:57-152`) advertises author, version, license, size, download counts and star ratings, and reuses the ids of the real code-generated templates (`EditorLayer::ApplyTemplate`, `EditorLayerProjectHub.cpp:2360`). The result appears under Custom Templates, and its **Load** button (`:5598-5610`) reaches `SceneSerializer::Load(..., clearExisting=true)`, which calls `m_World->Clear()` at `SceneSerializer.cpp:9077` *before* parsing.

**Cost:** the user's open scene is destroyed and replaced by nothing, with `[Template] Loaded: <name>` in the console.

**Fix:** there is no backend. Remove the panel and its menu entry (`EditorLayerMenuBar.cpp:646-651`), or make `Install` copy real content.

### 1.5 Hub and editor cannot see each other's projects
`Hub/src/HubApplication.cpp:248` writes the manifest as literally `.enjinproject`; `:181` scans for `filename() == ".enjinproject"`. The editor writes `<ProjName>.enjinproject` (`EditorLayerScene.cpp:243`, `EditorLayerDialogs.cpp:1712`) and detects via `entry.path().extension() == ".enjinproject"` (`EditorLayerScene.cpp:150, 173, 447`). For a leading-dot filename with no other period, `std::filesystem::path::extension()` returns `""`.

**Cost:** total mutual invisibility in both directions. Masked because `ENJIN_BUILD_HUB` defaults OFF.

**Fix:** Hub writes and scans `<name>.enjinproject`.

### 1.6 Scene List "Load" reports success after the world is already cleared
`Engine/src/Editor/EditorLayerPanels.cpp:1382-1404` — three call sites (double-click, `Load`, `Load Additive`) discard the `bool` from `SceneManager::LoadScene`/`LoadSceneAdditive`, which has four failure returns (`SceneManager.cpp:527-565`), and unconditionally show green `Scene loaded: <name>`. The serializer clears the world before it can fail.

**Cost:** a moved or corrupt `.enjin` gives an empty viewport plus a success toast. Every other scene-load path checks the result (`EditorLayerScene.cpp:388, 392`, `EditorLayerMenuBar.cpp:295-303`) — this panel is the exception.

**Fix:** check the return; three lines.

---

## Rank 2 — The same problem solved N times

### 2.1 "Run a child process and wait" hand-rolled 10 times
`Desktop.h` centralised *launch and forget*; run-and-wait was not. Each site carries its own Windows/POSIX pair:

`HTML5Exporter.cpp:171` (wait, 5-min timeout on Win / none on POSIX) · `EditorLayerGit.cpp:135` (wait + capture) · `EditorLayerProjectHub.cpp:2277-2295` (git init, **exit status discarded on both branches**) · `EditorLayerComponents.cpp:6835-6849` and `:6886-6889` · `FileDialog.cpp:212` (macOS) · `FileDialog.cpp:368` (Linux — byte-identical **except** it checks `WIFEXITED`/`WEXITSTATUS`, a fix that landed in one copy only) · `FileDialog.cpp:421` · `EditorBridge.cpp:222/236` · `Hub/src/HubApplication.cpp:397` — **`std::system(cmd + " &")`**, the last violation of the documented no-`std::system` rule, which returns the shell's status (always 0 once backgrounded, so it reports success when `EnjinEditor` doesn't exist) and interpolates an unquoted path into `/bin/sh`.

`Core/include/Enjin/Platform/LinuxPlatform.h:57` already declares exactly the right function — `ProcessResult LaunchProcess(...)` with capture and timeout — but Linux-only, so nothing portable can use it. Its only caller is the dead `AppImageBuilder`.

**Fix:** promote it to `Enjin/Platform/Process.h` next to `Desktop.h`; collapse the ten copies. Same pattern that worked for `Desktop.h` and `ShaderSearchPaths`.

### 2.2 `ShellEscape` — five byte-identical copies
`EditorLayer.cpp:168`, `EditorLayerComponents.cpp:155`, `EditorLayerGit.cpp:131`, `FileDialog.cpp:201`, `FileDialog.cpp:357`. All inside POSIX guards. Belongs beside the process helper in Core.

### 2.3 "Walk up from a scene to its `.enjinproject`" — three copies, three depths
`EditorLayerScene.cpp:147-162` (depth < 3), `:168-190` (depth < 3), `:441-456` (depth < **4**). All three also hand-roll the containment test with `lexically_relative` + `rfind("..", 0)` (`:144`, `:440`, plus `EditorLayerComponents.cpp:6916, 6969`, `EditorLayerPanels.cpp:673`) when `Platform::MakeRelativeToRoot` exists for it.

**Cost:** a scene four levels below its project root gets a "belongs to another project" warning from one function while the other gives up and offers to create a *new* project. They disagree by construction.

### 2.4 The image-extension list — twelve copies, already divergent
`ThumbnailGenerator.cpp:78`, `EditorLayerPanels.cpp:744`, `EditorLayerComponents.cpp:820, 960, 1158, 4038, 4059`, `EditorLayerRendering.cpp:693`, `EditorLayerViewport.cpp:258`, `EditorLayerScene.cpp:806`, plus `:1019, 1168`. `EditorLayerScene.cpp:806` already omits `.svg` that the other eleven include. Two are already named predicates; nothing shares them.

### 2.5 `GetComponentIcon` — two identical 24-entry tables
`EditorLayerComponents.cpp:167` and `EditorLayerHierarchy.cpp:123`. In sync today; a new component's icon must be added twice or hierarchy and inspector disagree.

### 2.6 Resource lookups that miss the installed layout (same class as the 19 shader arrays)
- **User manual:** `EditorLayerPanels.cpp:3463` and `:3541` each carry an identical four-entry array (`docs/`, `../docs/`, `../../docs/`, `../../../docs/`) plus an identical read loop. `Application::InitializeEngine` sets CWD to the exe dir, so from `<prefix>/bin` none reach `<prefix>/share/doc/enjin/USER_MANUAL.md` where `CMakeLists.txt:209` installs it. The panel shows "Manual Not Found" in every installed build; it only works from a build tree.
- **Templates:** eight sites resolve a bare relative `"templates"` (`EditorLayerMenuBar.cpp:649`, `EditorLayerProjectHub.cpp:5427, 5464, 5493, 5533, 5541, 5601, 5618`). Installed templates go to `<prefix>/share/enjin/templates` (`CMakeLists.txt:214`) and nothing looks there. There is also no `templates/` directory in the repo at all, so the `if(EXISTS)` guard means it never installs and Custom Templates is always empty.

**Fix:** a `Platform::ResourceSearchPaths(subdir, fileName)` mirroring `Renderer::ShaderSearchPaths`, including `../share/enjin/` and `../share/doc/enjin/`.

---

## Rank 3 — Dead code (~5,060 lines of `.cpp`, zero callers)

Verified with loose substring greps, not word boundaries — that is what produced my own near-misses (below).

`Build/AppImageBuilder.cpp` 400 · `Platform/SteamDeck.cpp` 369 · `Platform/SteamInput.cpp` 313 · `Platform/SwitchPlatform.cpp` 145 · `Editor/SymbolLibrary.cpp` 1160 · `Editor/CollaborativeEditingUI.cpp` 952 · `Editor/CollabMergeUI.cpp` 276 · `Editor/EditorBridge.cpp` 267 · `Editor/SharedMemoryTransport.cpp` 252 (test-only) · `Editor/EditorProtocol.cpp` 46 (test-only) · `Editor/P2PAuthority.cpp` 40 · `Editor/ServerAuthority.cpp` 29 · `GUI/InventoryUI.cpp` 326 · `GUI/MinimapRenderer.cpp` 230 · `GUI/ShaderGUI.cpp` 144 · `GUI/DialogueAsset.cpp` 111.

Notes: `BuildTargetPlatform` (`BuildReport.h:11`) is only `Desktop`/`Web` — there is no AppImage target; Linux ships via CPack DEB. `CollaborativeEditingUI.cpp` is a **duplicate** of the live `EditorLayer::DrawCollaborationPanel` (`EditorLayerPanels.cpp:5122`), and its eight `ApplyXxx` remote-op handlers duplicate `EditorLayer.cpp:516-570`. `CollaborativeEditingSystem::SetAuthority` (`CollaborativeEditing.h:295`) is never called, so `m_Authority` is permanently null and both authority implementations are inert. All are compiled by `file(GLOB_RECURSE src/*.cpp)`.

**Also:** `Engine/src/Scripting/WebStubs.cpp:18-32` defines 12 `void* s_VisualScript*` in namespace `Enjin::VisualScript`, while the real globals (`NodeRegistry.cpp:58-73`) are typed and at **global** scope. Nothing references the namespaced ones. A maintainer reading this file concludes web's system pointers are handled; they are not (see 1.1).

---

## Rank 4 — Wasted work and small broken paths

**4.1 OIT allocates ~19 MB of VRAM plus a pipeline on every desktop run and is never used.** `RenderSystem.cpp:4272-4277` unconditionally constructs `OITManager`, creating an RGBA16F accumulation image + R8 revealage image (`OITManager.cpp:682, 724`), a render pass, framebuffer, composite pipeline and descriptor sets. `m_OITManager->` appears exactly twice in the tree: that `Initialize` and `Shutdown` at `:4616`. `BeginTransparentPass`/`EndTransparentPass`/`CompositePass` are never called. `m_OITEnabled` (`RenderSystem.h:2411`) gates nothing — its only references are the checkbox at `EditorLayerRendering.cpp:2654-2656` and `EditorLayerPanels.cpp:7928`, which prints **`OIT: Enabled`**. At 1920×1080 that is ~18.7 MB held for process lifetime in the editor *and* every exported desktop game.

**4.2 Per-frame `stat()` in draw code.** `EditorLayerProjectHub.cpp:495` — one `filesystem::exists` per recent project every frame the Project Hub is visible (20 recents at 60 fps ≈ 1200 syscalls/s; one unreachable network path stalls the UI). `EditorLayerComponents.cpp:1469-1473` — 1-2 stats/frame per selected Text entity. `EditorLayerSettings.cpp:2583-2609` — `DrawSettingsSection_BuildScenes` re-scans `scenes/` with a `directory_iterator` and an O(n·m) compare **every frame** the section is expanded, calling `SceneManager::AddScene` from inside a draw function. Cache behind the dirty-flag pattern the asset browser already uses correctly.

**4.3 Gamepad radial menu: release-to-select cannot fire.** `EditorLayerGamepad.cpp:139-142` calls `DrawRadialMenu(type)` on bumper release "to resolve the action", but `DrawRadialMenu` only executes at `:348` when `GamepadButton::A` is *also* pressed that frame. The documented "hold to open, release to select" never happens. `m_RadialMenuAngle` (`:159`) is additionally a dead store — `:296-305` recomputes it.

**4.4 Empty handlers on enabled controls.** `EditorLayerPanels.cpp:5684-5687` "Export as SVG" — body is one `ImGui::TextDisabled` call; `VectorDrawingEditor::ExportSVG` exists and is never reached. `FlashTimeline.cpp:430-432` layer "Rename" — empty, between working Delete/Move Up/Move Down. `Hub/src/HubApplication.cpp:638-640` `File > Open Project… (Ctrl+O)` — zero statements. `Hub/src/HubApplication.cpp:539-550` `LoadSettings` opens nothing while `SaveSettings` (`:498-536`) writes a real JSON file with theme, directories and recents — the user saves, a file appears, everything is gone on restart. `Hub/src/HubApplication.cpp:403-410` `CreateFromTemplate` is a `std::cout` returning `true`; all 43 templates advertised at `:412-461` produce a blank project.

**4.5 Editor coupling — concrete cost.** `Engine/src/Editor` is 83,653 lines (27% of `Engine/src`) across 59 of 406 TUs, compiled into `EnjinEngine`, which `EnjinPlayer` links. Two specific costs: `Enjin/Editor/NodeGraph.h` includes `<imgui.h>` and `<nlohmann/json.hpp>` and is pulled in by `ECS/Components/VisualScript.h:6`, `AI/BehaviorTree.h:6`, `Gameplay/QuestFlow.h:7`, `VisualScript/NodeDefinition.h:6` — **41 TUs**, including runtime AI and quest code, parse all of ImGui for a data struct that does not use it (the 1.1 fix removes this too). And `ECS/Systems/RenderSystem.h:92` includes `Enjin/Editor/FlashTimeline.h` for one struct, `Editor::OnionSkinGhost`; 25 TUs include `RenderSystem.h`, so every Flash-timeline edit rebuilds the renderer. Moving that struct to a renderer header is a one-struct change.

---

## What I did not find, and what I withdrew

**Category 4 is genuinely thinner than the brief anticipated** and I would rather say so than pad. The asset browser is correctly cached behind `m_AssetBrowserCacheDirty` (`EditorLayerPanels.cpp:602-604`). `World::GetEntitiesWithComponent` returns `const&`, so the 40+ calls in draw code allocate nothing. Every `ifstream` I traced inside a `Draw*` function was behind a button. Items 4.1-4.2 are the real ones.

**Three claims withdrawn during verification**, two from word-boundary greps hiding a derived name — the exact failure mode of the previous audit: `Editor/ProceduralGraph.cpp` (1,241 lines) is live via `ProceduralGraphEditor m_ProcGraphEditor` (`EditorLayer.h:1419`); `Editor/CRDTState.cpp` (166 lines) is live via `m_CRDTDoc` (`CollaborativeEditing.cpp:132, 361, 420, 724`); `Assets::CopyToProjectAssets` looked orphaned until I widened the search — three editor callers.

## Suggested order

1. Split `NodeGraphData` out of the editor — fixes 1.1 and half of 4.5 in one change, no desktop behaviour change.
2. `m_FeedbackTab` request flag (1.3); check returns at `EditorLayerPanels.cpp:1382-1404` (1.6). Both trivial.
3. Escalate build verification in `BuildPipeline.cpp:85-88, 611` (1.2).
4. Delete the dead code (Rank 3) and the misleading `WebStubs.cpp` block — plus `GitIntegration.h` from the addendum, which is the one that will actively break someone.
5. `Platform::RunProcess` + `ShellEscape` in Core; collapse the ten copies (2.1, 2.2).
6. Decide whether the Template Marketplace (1.4) and the Hub (1.5, 4.4) are products or should be removed.