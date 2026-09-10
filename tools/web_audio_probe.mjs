// Ask a running web build what state its AudioContext is in.
//
// A browser starts every AudioContext SUSPENDED and will not let it run until
// the page has seen a real user gesture. Nothing in the engine resumes one, so
// the question "does a web game make any sound before the player clicks" has a
// yes/no answer that no screenshot can show and no desktop test can stand in
// for. This asks the page directly.
//
// Usage:
//   node web_audio_probe.mjs <url> [--frames N] [--timeout MS] [--click]
//
// --click dispatches a real mouse click partway through, so the two runs
// together say whether a gesture is what the audio was waiting for.

import { launch } from 'puppeteer-core';
import { existsSync } from 'node:fs';

const args = process.argv.slice(2);
const url = args[0];
if (!url) {
    console.error('usage: node web_audio_probe.mjs <url> [--frames N] [--timeout MS] [--click]');
    process.exit(2);
}
const numArg = (flag, dflt) => {
    const i = args.indexOf(flag);
    return i >= 0 && args[i + 1] ? Number(args[i + 1]) : dflt;
};
const waitMs = numArg('--timeout', 30000);
const settleMs = numArg('--settle', 9000);
const doClick = args.includes('--click');

const CHROME_CANDIDATES = [
    'C:/Program Files/Google/Chrome/Application/chrome.exe',
    'C:/Program Files (x86)/Google/Chrome/Application/chrome.exe',
    '/usr/bin/google-chrome',
    '/usr/bin/chromium',
];
const chromePath = process.env.CHROME_PATH || CHROME_CANDIDATES.find(existsSync);
if (!chromePath) {
    console.error('no Chrome found; set CHROME_PATH');
    process.exit(2);
}

// NOT --mute-audio: muting is a different thing from suspending, and a muted
// context still reports "running". The probe would answer its own question.
const browser = await launch({
    executablePath: chromePath,
    headless: true,
    args: [
        '--enable-unsafe-webgpu',
        '--enable-unsafe-swiftshader',
        '--enable-features=Vulkan',
        '--hide-scrollbars',
        '--autoplay-policy=document-user-activation-required',  // Chrome's default
    ],
});

let exitCode = 0;
try {
    const page = await browser.newPage();
    await page.setViewport({ width: 900, height: 600 });

    // Wrap the constructor before any page script runs, so every context the
    // engine makes is recorded whoever made it.
    await page.evaluateOnNewDocument(() => {
        window.__ctxs = [];
        for (const name of ['AudioContext', 'webkitAudioContext']) {
            const Orig = window[name];
            if (!Orig) continue;
            window[name] = function (...a) {
                const c = new Orig(...a);
                window.__ctxs.push(c);
                return c;
            };
            window[name].prototype = Orig.prototype;
        }
    });

    page.on('console', (m) => {
        const t = m.text();
        if (/audio|Audio|AUDIO/.test(t)) console.log('  [log]', t);
    });

    await page.goto(url, { waitUntil: 'domcontentloaded', timeout: waitMs });
    await new Promise((r) => setTimeout(r, settleMs));

    const report = async (label) => {
        const s = await page.evaluate(() => ({
            count: (window.__ctxs || []).length,
            states: (window.__ctxs || []).map((c) => c.state),
            times: (window.__ctxs || []).map((c) => c.currentTime),
        }));
        console.log(`${label}: ${s.count} AudioContext(s) ` +
                    (s.count ? `state=[${s.states.join(', ')}] currentTime=[${s.times.join(', ')}]`
                             : '(none created)'));
        return s;
    };

    const before = await report('before gesture');

    if (doClick) {
        await page.mouse.click(450, 300);
        await new Promise((r) => setTimeout(r, 3000));
        const after = await report('after click  ');
        if (before.count && after.count) {
            // Only LIVE contexts count. miniaudio leaves closed ones behind
            // while probing devices, and "every context is running" is false
            // forever once one of them is closed -- which made this verdict
            // contradict the states printed directly above it.
            const live = (states) => states.filter((s) => s !== 'closed');
            const wokeUp = live(before.states).some((s) => s !== 'running') &&
                           live(after.states).length > 0 &&
                           live(after.states).every((s) => s === 'running');
            console.log(wokeUp
                ? 'VERDICT: the context was waiting on a gesture and a click released it'
                : 'VERDICT: the click did not change the context state');
        }
    }

    if (before.count === 0) {
        console.log('VERDICT: the engine never created an AudioContext on this page');
    } else if (before.states.filter((s) => s !== 'closed').every((s) => s === 'running')) {
        console.log('VERDICT: audio is running without a gesture');
    } else {
        console.log('VERDICT: audio is SUSPENDED before any gesture - anything played now is silent');
    }
} catch (e) {
    console.error('probe failed:', e.message);
    exitCode = 1;
} finally {
    await browser.close();
}
process.exit(exitCode);
