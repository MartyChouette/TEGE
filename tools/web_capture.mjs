// Render a web build in a real browser and save what it drew.
//
// This exists because the web renderer had no verification loop at all. WGSL
// compiling through Dawn (check_wgsl.mjs) proves the shaders are legal and
// nothing else: the two bugs that actually reached a browser -- a mouse
// position scaled by devicePixelRatio twice, and a palette clock that latched
// after one frame -- were both C++ integration bugs that compiled perfectly and
// were invisible to every check that could be run without a GPU. They were
// found by a person opening the page and saying it looked wrong.
//
// Headless Chrome DOES give a WebGPU adapter, with --enable-unsafe-swiftshader.
// That was previously believed impossible here, which is why this did not exist.
//
// Usage:
//   node web_capture.mjs <url> <out.png> [--frames N] [--timeout MS] [--show-log] [--click]
//                        [--allow-splash]
//
// Exit code is non-zero when the page never rendered, so this can gate CI.

import { launch } from 'puppeteer-core';
import { existsSync } from 'node:fs';
import { writeFile } from 'node:fs/promises';
import path from 'node:path';

const args = process.argv.slice(2);
const url = args[0];
const outPath = args[1];
if (!url || !outPath) {
    console.error('usage: node web_capture.mjs <url> <out.png> [--frames N] [--timeout MS] [--show-log] [--click] [--allow-splash]');
    process.exit(2);
}
const flag = (name, fallback) => {
    const i = args.indexOf(name);
    return i >= 0 && args[i + 1] ? Number(args[i + 1]) : fallback;
};
// Frames to wait for BEFORE capturing. The default is not 1: a first frame
// catches the engine mid-setup, with defaults still in place -- the scene
// palette is a white table until its first upload, so an early capture shows
// blank white materials and looks like the feature is broken when it is only
// unfinished. 120 frames is about two seconds of animation, past every
// first-frame default and far enough in for cycling to have moved.
// A COMMA LIST captures at each point: --frames 30,90,240,500 writes
// <out>.f0030.png and so on. One value keeps the original behaviour and writes
// the path given, because CI and the demo-room checker call it that way.
//
// More than one because a single picture cannot tell a still scene from a
// stopped one, which is the failure this whole harness exists to catch.
const frameList = (() => {
    const i = args.indexOf('--frames');
    if (i < 0 || !args[i + 1]) return [120];
    return args[i + 1].split(',').map((s) => Number(s.trim()))
        .filter((n) => Number.isFinite(n) && n > 0).sort((a, b) => a - b);
})();
const wantFrames = frameList[frameList.length - 1];
const timeoutMs = flag('--timeout', 120000);
const showLog = args.includes('--show-log');
const doClick = args.includes('--click');
// Photographing the "Click to Play" card is a capture of nothing, and it does
// not look like a failure: the file is written, the byte count is plausible,
// and the draw-call count is non-zero because the shell itself draws. Two
// captures of two DIFFERENT builds both came back as the title card and still
// differed in size, which "confirmed" a renderer fix that had not been
// exercised at all. Pass --allow-splash when the card is what you actually want.
const allowSplash = args.includes('--allow-splash');

const CHROME_CANDIDATES = [
    process.env.CHROME_PATH,
    'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe',
    'C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe',
    '/usr/bin/google-chrome',
    '/usr/bin/chromium',
    '/usr/bin/chromium-browser',
    '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
].filter(Boolean);

const chromePath = CHROME_CANDIDATES.find((p) => existsSync(p));
if (!chromePath) {
    console.error('no Chrome found. Set CHROME_PATH to a Chrome/Chromium binary.');
    process.exit(2);
}

// Software rendering is a different renderer, not a slower one. SwiftShader
// accepts things a real driver rejects and vice versa, so a capture that only
// ever runs on it can report a scene as fine while the machine in front of a
// person shows black. --gpu runs headed on the actual adapter, which is the
// only way to see what a player sees.
const useGPU = args.includes('--gpu');

// Click the "Click to Play" card away and wait for the preloader it lives in to
// actually go. Safe to call repeatedly: it does nothing when there is no gate.
//
// It waits on the PARENT, not the gate. The shell's click handler fades and
// hides #preloader and never touches #click-to-play, so the gate element stays
// display:block forever and is only ever hidden by its ancestor.
async function dismissGate(page, budgetMs) {
    const deadline = Date.now() + budgetMs;
    let dismissed = false;
    while (Date.now() < deadline) {
        const state = await page.evaluate(() => {
            const vis = (el) => !!el && (typeof el.checkVisibility === 'function'
                ? el.checkVisibility({ checkOpacity: true, checkVisibilityCSS: true })
                : el.getClientRects().length > 0);
            return {
                preUp: vis(document.getElementById('preloader')),
                gateUp: vis(document.getElementById('click-to-play')),
            };
        }).catch(() => null);

        if (!state || !state.preUp) return dismissed;   // nothing in the way

        if (state.gateUp) {
            // TWO clicks, and they do different jobs.
            //
            // The coordinate click is a trusted gesture, which is what grants
            // the page user activation -- a browser holds every AudioContext
            // suspended until it sees one, and this engine holds CLIP LOADING
            // with it, so without this an audio-dependent capture measures a
            // game that was never allowed to start.
            //
            // It does not DISMISS the gate. Measured on a real export: the
            // element is unoccluded (elementFromPoint at its centre returns
            // #click-to-play), it is not moving (tegeBreathe animates
            // text-shadow only), and a hundred coordinate clicks over thirty
            // seconds left the preloader at display:flex -- while a single
            // el.click() took it to display:none immediately. Something in the
            // page swallows the real event before it reaches the listener; the
            // synthetic dispatch goes straight to the element and runs it.
            await page.click('#click-to-play').catch(() => {});
            await page.evaluate(() => {
                const g = document.getElementById('click-to-play');
                if (g) g.click();
            }).catch(() => {});
            dismissed = true;
        }
        await new Promise((r) => setTimeout(r, 250));
    }
    if (dismissed) console.error('  note: clicked the gate but the preloader is still up');
    return dismissed;
}

const browser = await launch({
    executablePath: chromePath,
    headless: !useGPU,
    args: useGPU
        ? [
            '--enable-unsafe-webgpu',
            '--hide-scrollbars',
            '--mute-audio',
            '--window-size=940,700',
          ]
        : [
            // WebGPU in headless. Without the swiftshader flag requestAdapter
            // returns null on a machine with no usable GPU in the headless
            // sandbox, and the engine boots into a canvas that never draws.
            '--enable-unsafe-webgpu',
            '--enable-unsafe-swiftshader',
            '--enable-features=Vulkan',
            '--hide-scrollbars',
            '--mute-audio',
            // STEALS FOCUS WITHOUT THIS, even fully headless. Asking for
            // Vulkan makes Chrome spawn a separate GPU-info-collection
            // process on Windows, and that process creates a window, takes
            // the foreground, and is destroyed again in under 25ms.
            //
            // Measured during a 42-project sweep with a 25ms foreground-window
            // logger: the window is gone before its owner can even be
            // resolved, so it logs as proc=Idle, and focus then falls through
            // to Program Manager -- the DESKTOP -- rather than back to
            // whatever the person was using. That is why it reads as "my
            // cursor was grabbed and I had to alt-tab": input goes to the
            // desktop. Nothing appears on screen long enough to identify by
            // eye, and a 400ms poll misses it entirely and reports silence.
            '--disable-gpu-process-for-dx12-vulkan-info-collection',
            // NOT --no-startup-window: it stops Puppeteer getting a target at
            // all and every capture dies with "Timed out after waiting 30000ms".
          ],
});

let exitCode = 0;
try {
    const page = await browser.newPage();
    await page.setViewport({ width: 900, height: 600, deviceScaleFactor: 1 });

    const logLines = [];
    page.on('console', (m) => logLines.push(`[${m.type()}] ${m.text()}`));
    page.on('pageerror', (e) => logLines.push(`[pageerror] ${e.message}`));
    // The URL, not just "Failed to load resource". A console 404 carries no URL, so a
    // checker reading these could only ever say "1 fetch failure" and leave whoever saw
    // it to guess -- and on this demo room the answer was favicon.ico every time, which
    // is browser noise and not a deployment problem.
    page.on('response', (r) => {
        if (r.status() >= 400) logLines.push(`[http${r.status()}] ${r.url()}`);
    });
    page.on('requestfailed', (r) => {
        logLines.push(`[requestfailed] ${r.url()} ${r.failure()?.errorText ?? ''}`);
    });

    await page.goto(url, { waitUntil: 'domcontentloaded', timeout: timeoutMs });

    // Wait for the engine's own account of itself rather than for a timer --
    // that is the difference between a capture that means something and one
    // that happened to land after the right delay.
    //
    // TWO signals, because neither alone covers every scene.
    //
    // The entity count is really the size of the per-entity RENDER DATA array,
    // which only entities with their own GPU buffers populate. A sprite-only
    // scene draws entirely from instance buffers and never adds a row to it, so
    // its count is legitimately zero forever. Waiting on that alone reported
    // "the engine never loaded a scene" for scenes that had loaded perfectly and
    // were on screen -- which is exactly the kind of harness that makes you
    // debug the wrong thing.
    //
    // So the scene-loaded log line counts too. It is the player saying, in its
    // own words, that the thing we are waiting for has happened.
    let frames = 0;
    let sceneLoaded = false;
    page.on('console', (m) => {
        if (/Loaded scene:/.test(m.text())) sceneLoaded = true;
    });
    await page.waitForFunction(
        () => typeof Module !== 'undefined' && Module._getEntityCount &&
              Module._getEntityCount() > 0,
        { timeout: timeoutMs, polling: 250 },
    ).catch(async () => {
        if (sceneLoaded) return;   // loaded, just nothing with per-entity render data
        // Say what the page looked like instead of just failing. "Never drew a
        // frame" has several very different causes -- no adapter, a wasm abort,
        // a missing export -- and they are indistinguishable without this.
        const state = await page.evaluate(() => ({
            module: typeof Module !== 'undefined',
            entityExport: typeof Module !== 'undefined' && !!Module._getEntityCount,
            entities: (typeof Module !== 'undefined' && Module._getEntityCount)
                ? Module._getEntityCount() : null,
            canvas: (() => {
                const c = document.getElementById('game-canvas');
                return c ? `${c.width}x${c.height}` : 'missing';
            })(),
        })).catch(() => null);
        console.error('  page state: ' + JSON.stringify(state));
        for (const l of logLines.slice(-12)) console.error('  ' + l);
        throw new Error('the engine never loaded a scene');
    });

    // --click: one real gesture, dispatched AFTER the readiness wait above and
    // BEFORE the frame wait below. Order is the whole point.
    //
    // A browser holds every AudioContext suspended until the page has seen a
    // user gesture, and this engine holds CLIP LOADING with it, so an
    // audio-dependent capture without a click measures a game that was never
    // allowed to start: Audio_GetLength returns -1 and Audio_Seek returns false
    // for the whole run.
    //
    // Clicking BEFORE readiness does not work and is worth not rediscovering:
    // the click lands before the canvas is wired, the gesture is lost, and the
    // readiness check then fails with "the engine never loaded a scene" -- so
    // the capture looks broken rather than un-clicked. Clicking here means the
    // frame wait below is time the page spends ALIVE after the gesture, which is
    // what a script waiting on audio needs.
    if (doClick) {
        // A generated export gates itself behind a "Click to Play" overlay that
        // sits ON TOP of the canvas, so clicking the canvas hits the overlay's
        // backdrop and the game never starts. Captures of a real export were a
        // perfect photograph of the play button until this was added.
        //
        // The hand-written demo shells in web-demo/ have no such gate, which is
        // why this went unnoticed: the tool was only ever pointed at those.
        // Dismissing the gate is not a one-shot, because WHEN it appears is not
        // knowable from here.
        //
        // Readiness above is `_getEntityCount() > 0`, true the moment a scene
        // loads. The gate is revealed much later and by a different mechanism:
        // the Module calls hidePreloader() when the engine finishes
        // initializing, which only sets ready=true, and showClickToPlay() then
        // waits for an EASED progress bar to reach 0.995. The gap between those
        // two is however long that project takes to initialize -- measured at
        // over eight seconds on Ropes -- so any fixed wait is a guess, and a
        // guess that is too short skips the click, reports "no gate", and
        // photographs the card that appears straight afterwards.
        //
        // So: poll until the preloader is actually gone, clicking the gate every
        // time it is visible. A shell with no gate at all just falls out of the
        // loop when the deadline passes, having cost nothing.
        await dismissGate(page, 30000);

        const target = await page.$('#game-canvas');
        // Twice: the engine notices a gesture by polling its own Input each
        // frame, and a single press/release can fall between two polls.
        for (let i = 0; i < 2; i++) {
            if (target) await target.click();
            else await page.mouse.click(450, 300);
            await new Promise((r) => setTimeout(r, 300));
        }
    }

    // Then let it run on, stopping at each requested frame count to photograph
    // it. rAF is the honest clock here: it counts frames the browser actually
    // presented.
    const canvas = await page.$('#game-canvas');
    if (!canvas) throw new Error('no #game-canvas on the page');

    const multi = frameList.length > 1;
    const stem = outPath.replace(/\.png$/i, '');

    // Count SIMULATION frames when the build can report them, rAF ticks
    // otherwise.
    //
    // rAF ticks start at page load, and on web everything before the first
    // counted frame -- the preloader, the "Click to Play" gate, this tool's own
    // click choreography -- is time the game may already have been running. So
    // "capture at frame 30" meant "capture an unknown distance into the
    // simulation", varying with machine load, and a desktop capture at frame 30
    // and a web capture at frame 30 were not photographs of the same moment.
    // That is how Examples/FixedTimestep -- a demo about simulation timing --
    // read as FROZEN on web: by the time counting began its one falling body had
    // landed, so every capture was the same settled scene, and comparing two of
    // them proved only that nothing was moving any more.
    //
    // _getSimFrame counts frames in which gameplay actually ran. An older build
    // does not export it and falls back to rAF, which is the behaviour this
    // replaces rather than a second thing to maintain.
    const hasSimClock = await page.evaluate(
        () => typeof Module !== 'undefined' && typeof Module._getSimFrame === 'function'
    ).catch(() => false);
    if (!hasSimClock) {
        console.error('  note: this build does not export _getSimFrame; ' +
                      'counting browser frames, which start before the game does');
    }

    const dest0 = (st, t, m, o) =>
        m ? `${st}.f${String(t).padStart(4, '0')}.png` : o;

    let waited = 0;
    for (const target of frameList) {
        const step = target - waited;
        if (step > 0) {
            waited = hasSimClock
                ? await page.evaluate((t) => new Promise((resolve) => {
                    // ABSOLUTE, not relative. `target` means "the target'th
                    // frame the simulation has run", so it is the same moment a
                    // desktop capture at that frame photographs -- a relative
                    // wait would just re-add whatever offset this tool's own
                    // startup happened to cost.
                    const tick = () => {
                        const now = Module._getSimFrame();
                        if (now >= t) resolve(now);
                        else requestAnimationFrame(tick);
                    };
                    requestAnimationFrame(tick);
                }), target)
                : await page.evaluate((n, from) => new Promise((resolve) => {
                    let i = 0;
                    const tick = () => {
                        if (++i >= n) resolve(from + i);
                        else requestAnimationFrame(tick);
                    };
                    requestAnimationFrame(tick);
                }), step, waited);
        }
        frames = waited;
        if (hasSimClock && waited > target + 5) {
            console.error(`  warning: ${dest0(stem, target, multi, outPath)} is LATE -- asked for ` +
                          `simulation frame ${target}, got ${waited}. The page spent that long ` +
                          `getting to a running game, so this is not the same moment a desktop ` +
                          `capture at frame ${target} photographs.`);
        }
        const dest = multi
            ? `${stem}.f${String(target).padStart(4, '0')}.png`
            : outPath;

        // One more attempt right before photographing. A multi-frame run
        // captures at f30, f90, f240 and f500, and the gate can surface between
        // any two of them -- it is revealed by the preloader's own animation,
        // not by anything this tool controls. Short budget: by here it is a
        // late arrival, not a slow boot.
        if (doClick) await dismissGate(page, 3000);

        // Is the play gate still up? Checked at CAPTURE time rather than once at
        // the start, because a multi-frame run photographs repeatedly and the
        // gate is dismissed partway through a --click run.
        const gated = await page.evaluate(() => {
            const el = document.querySelector('#click-to-play');
            if (!el) return null;

            // checkVisibility, not getComputedStyle. An element's computed
            // style is its OWN: after the shell hides the gate by hiding an
            // ANCESTOR, #click-to-play still computes display:block opacity:1
            // while rendering nothing at all. Reading it that way refused every
            // capture including the ones that had correctly clicked through.
            // getClientRects is the fallback for a browser without
            // checkVisibility: an unrendered element has no boxes.
            const shown = typeof el.checkVisibility === 'function'
                ? el.checkVisibility({ checkOpacity: true, checkVisibilityCSS: true })
                : el.getClientRects().length > 0;
            if (!shown) return null;

            const cs = getComputedStyle(el);
            const r = el.getBoundingClientRect();
            return { display: cs.display, opacity: cs.opacity,
                     size: `${Math.round(r.width)}x${Math.round(r.height)}` };
        }).catch(() => null);

        if (gated && !allowSplash) {
            // The style goes in the message because "still up" is a conclusion
            // and these are the facts behind it -- an overlay left in the DOM
            // at opacity 0 is not up, and telling those apart from the outside
            // is the whole difficulty.
            console.error(
                `refusing to capture ${dest}: the "Click to Play" gate is still up ` +
                `(${JSON.stringify(gated)}), so this would photograph the title card rather ` +
                `than the scene. Pass --click to get past it, or --allow-splash if the card ` +
                `is what you want.`);
            await browser.close();
            process.exit(2);
        }

        const shot = await canvas.screenshot({ type: 'png' });
        await writeFile(path.resolve(dest), shot);

        // The same numbers the desktop capture writes beside its image, from the
        // engine's own exports. Pixels cannot say WHY a frame is wrong: a scene
        // whose meshes all failed to draw still fills the canvas with sky, and a
        // draw-call count of zero settles that in one line.
        //
        // Field names match the desktop sidecar exactly so one claim reads both.
        // entityRenderSlots keeps the desktop name rather than the export's:
        // getEntityCount returns the size of the per-entity render-data array,
        // which is a high-water mark indexed by entity id and not a count of
        // anything, and calling it a count on one runtime and a slot count on
        // the other is how a phantom parity difference gets invented.
        const stats = await page.evaluate(() => ({
            drawCalls: (typeof Module !== 'undefined' && Module._getDrawCallCount)
                ? Module._getDrawCallCount() : null,
            entityRenderSlots: (typeof Module !== 'undefined' && Module._getEntityCount)
                ? Module._getEntityCount() : null,
        })).catch(() => ({ drawCalls: null, entityRenderSlots: null }));
        await writeFile(path.resolve(dest.replace(/\.png$/i, '.json')),
                        JSON.stringify({ frame: target, ...stats }, null, 2) + '\n');

        console.log(`captured ${dest} after ${frames} frames (${shot.length} bytes, ` +
                    `${stats.drawCalls} draw calls)`);
    }
    if (showLog) {
        for (const l of logLines) console.log('  ' + l);
    } else {
        const errs = logLines.filter((l) => l.startsWith('[error]') || l.startsWith('[pageerror]'));
        for (const e of errs.slice(0, 10)) console.log('  ' + e);
    }
} catch (err) {
    console.error('capture failed: ' + err.message);
    exitCode = 1;
} finally {
    await browser.close();
}
process.exit(exitCode);
