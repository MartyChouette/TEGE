// Compile every WGSL shader the web renderer ships, and report what a browser
// would say about it.
//
// The WGSL in WebShaderData.h is compiled by the browser at run time, so a
// successful C++ or Emscripten build proves nothing about it: a syntax error, a
// type error or a uniformity-analysis violation shows up as a black canvas in
// front of a player, not as a build failure. This closes that gap by running the
// same compiler (Dawn) over the same source, from Node.
//
// Usage:
//   cd tools && npm install --no-save webgpu
//   node check_wgsl.mjs [path/to/WebShaderData.h]
//
// Exits non-zero if any shader fails to compile.

import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, resolve } from 'node:path';

const here = dirname(fileURLToPath(import.meta.url));
const headerPath = process.argv[2]
  ? resolve(process.argv[2])
  : resolve(here, '..', 'Engine', 'include', 'Enjin', 'Renderer', 'WebGPU', 'WebShaderData.h');

// Each shader is `static const char* NAME = R"( ... )";` — a C++ raw string with
// an empty delimiter, so the body ends at the first `)"`.
function extractShaders(source) {
  const out = [];
  const decl = /static\s+const\s+char\*\s+(\w+)\s*=\s*R"\(/g;
  let m;
  while ((m = decl.exec(source)) !== null) {
    const name = m[1];
    const start = m.index + m[0].length;
    const end = source.indexOf(')"', start);
    if (end === -1) {
      out.push({ name, code: null, error: 'unterminated raw string' });
      continue;
    }
    out.push({ name, code: source.slice(start, end) });
    decl.lastIndex = end;
  }
  return out;
}

const source = readFileSync(headerPath, 'utf8');
const shaders = extractShaders(source);
if (shaders.length === 0) {
  console.error(`no WGSL found in ${headerPath}`);
  process.exit(2);
}

const { create } = await import('webgpu');
const gpu = create([]);
const adapter = await gpu.requestAdapter();
if (!adapter) {
  console.error('no WebGPU adapter available');
  process.exit(2);
}
const device = await adapter.requestDevice();

// A device error that is not tied to one createShaderModule call still has to be
// reported, or a validation failure could pass silently.
let uncaptured = 0;
device.addEventListener?.('uncapturederror', (e) => {
  uncaptured++;
  console.error(`  uncaptured device error: ${e.error?.message ?? e}`);
});

let failed = 0;
for (const s of shaders) {
  if (s.code === null) {
    console.error(`FAIL  ${s.name}: ${s.error}`);
    failed++;
    continue;
  }
  device.pushErrorScope('validation');
  const mod = device.createShaderModule({ code: s.code, label: s.name });
  const info = await mod.getCompilationInfo();
  const scoped = await device.popErrorScope();

  const errors = info.messages.filter((x) => x.type === 'error');
  const warnings = info.messages.filter((x) => x.type === 'warning');

  if (errors.length || scoped) {
    failed++;
    console.error(`FAIL  ${s.name}  (${s.code.split('\n').length} lines)`);
    for (const e of errors) {
      console.error(`  ${e.lineNum}:${e.linePos}  ${e.message}`);
    }
    if (scoped) console.error(`  device: ${scoped.message}`);
  } else {
    const note = warnings.length ? `  (${warnings.length} warning${warnings.length > 1 ? 's' : ''})` : '';
    console.log(`ok    ${s.name}${note}`);
    for (const w of warnings) {
      console.log(`  warning ${w.lineNum}:${w.linePos}  ${w.message}`);
    }
  }
}

console.log(`\n${shaders.length - failed}/${shaders.length} shaders compile`);
if (uncaptured) console.error(`${uncaptured} uncaptured device error(s)`);
process.exit(failed || uncaptured ? 1 : 0);
