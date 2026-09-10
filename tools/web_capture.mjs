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
//   node web_capture.mjs <url> <out.png> [--frames N] [--timeout MS] [--show-log]
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
    console.error('usage: node web_capture.mjs <url> <out.png> [--frames N] [--timeout MS] [--show-log]');
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
const wantFrames = flag('--frames', 120);
const timeoutMs = flag('--timeout', 120000);
const showLog = args.includes('--show-log');

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
          ],
});

let exitCode = 0;
try {
    const page = await browser.newPage();
    await page.setViewport({ width: 900, height: 600, deviceScaleFactor: 1 });

    const logLines = [];
    page.on('console', (m) => logLines.push(`[${m.type()}] ${m.text()}`));
    page.on('pageerror', (e) => logLines.push(`[pageerror] ${e.message}`));

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

    // Then let it run on for the requested number of frames. rAF is the honest
    // clock here: it counts frames the browser actually presented.
    frames = await page.evaluate((n) => new Promise((resolve) => {
        let i = 0;
        const tick = () => { if (++i >= n) resolve(i); else requestAnimationFrame(tick); };
        requestAnimationFrame(tick);
    }), wantFrames);

    const canvas = await page.$('#game-canvas');
    if (!canvas) throw new Error('no #game-canvas on the page');
    const shot = await canvas.screenshot({ type: 'png' });
    await writeFile(path.resolve(outPath), shot);

    console.log(`captured ${outPath} after ${frames} frames (${shot.length} bytes)`);
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
