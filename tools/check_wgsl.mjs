// Compile every WGSL shader the web renderer ships, and report what a browser
// would say about it.
//
// The WGSL in WebShaderData.h is compiled by the browser at run time, so a
// successful C++ or Emscripten build proves nothing about it: a syntax error, a
// type error or a uniformity-analysis violation shows up as a black canvas in
// front of a player, not as a build failure. This closes that gap by running the
// same compiler (Dawn) over the same source, from Node.
//
// It scans every file that embeds WGSL, not just WebShaderData.h. Systems that
// carry their own shader string in a .cpp were invisible to this tool: the
// vegetation shader was edited and shipped without ever being compiled here,
// which is exactly the gap this exists to close.
//
// Usage:
//   cd tools && npm install --no-save webgpu
//   node check_wgsl.mjs [file ...]
//
// Exits non-zero if any shader fails to compile.

import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, resolve } from 'node:path';

const here = dirname(fileURLToPath(import.meta.url));
const repo = resolve(here, '..');

// Every file that embeds WGSL. Add one here when a new system starts carrying
// its own shader string, or it ships uncompiled.
const DEFAULT_SOURCES = [
  'Engine/include/Enjin/Renderer/WebGPU/WebShaderData.h',
  'Engine/src/Renderer/WebGPU/WebGPUVegetationSystem.cpp',
  'Engine/src/Renderer/WebGPU/WebGPUParticleSystem.cpp',
];

const sourcePaths = process.argv.length > 2
  ? process.argv.slice(2).map((p) => resolve(p))
  : DEFAULT_SOURCES.map((p) => resolve(repo, p));

// Each shader is `static const char* NAME = R"DELIM( ... )DELIM";` - a C++ raw
// string. The delimiter is usually empty but not always (the particle and
// vegetation systems use R"WGSL(...)WGSL"), so it is captured and used to find
// the matching terminator.
function extractShaders(source) {
  const out = [];
  const decl = /static\s+const\s+char\*\s+(\w+)\s*=\s*R"(\w*)\(/g;
  let m;
  while ((m = decl.exec(source)) !== null) {
    const name = m[1];
    const term = ')' + m[2] + '"';
    const start = m.index + m[0].length;
    const end = source.indexOf(term, start);
    if (end === -1) {
      out.push({ name, code: null, error: 'unterminated raw string' });
      continue;
    }
    out.push({ name, code: source.slice(start, end) });
    decl.lastIndex = end;
  }
  return out;
}

// A .cpp may embed non-WGSL raw strings (HTML, JS). Only compile what declares
// itself a shader.
function looksLikeWGSL(code) {
  return /@(vertex|fragment|compute)\b/.test(code);
}

const shaders = [];
for (const path of sourcePaths) {
  const found = extractShaders(readFileSync(path, 'utf8'))
    .filter((s) => s.code === null || looksLikeWGSL(s.code));
  const rel = path.startsWith(repo) ? path.slice(repo.length + 1) : path;
  for (const s of found) shaders.push({ ...s, file: rel });
}
if (shaders.length === 0) {
  console.error(`no WGSL found in ${sourcePaths.join(', ')}`);
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

// --- Vertex contract: what PBR_VS declares vs what the pipeline supplies -----
//
// WGSL treats a vertex shader's declared inputs as a contract. A pipeline whose
// vertex layout omits even one of them does not draw wrong, it fails to CREATE,
// and Dawn reports that by handing back an invalid pipeline object. Nothing
// notices until something sets it on a pass -- then the entire command buffer is
// invalid and the frame is black, over an error that names a slot number and
// nothing else.
//
// Compiling PBR_VS on its own cannot catch this, because the shader is
// perfectly valid; it is the C++ beside it that is out of step. So the two are
// read together here: every @location the vertex entry point consumes must
// appear in MakePBRVertexLayout, which is the one place a layout for this
// shader is built.
function checkVertexContract() {
  const shaderSrc = readFileSync(resolve(repo, 'Engine/include/Enjin/Renderer/WebGPU/WebShaderData.h'), 'utf8');
  const cppSrc = readFileSync(resolve(repo, 'Engine/src/ECS/Systems/RenderSystem.cpp'), 'utf8');

  // The VertexInput struct PBR_VS takes: the first one declared in the file,
  // which belongs to PBR_WGSL.
  const structStart = shaderSrc.indexOf('struct VertexInput {');
  if (structStart === -1) return { ok: false, why: 'no VertexInput struct in WebShaderData.h' };
  const structEnd = shaderSrc.indexOf('};', structStart);
  const declared = [...shaderSrc.slice(structStart, structEnd).matchAll(/@location\((\d+)\)/g)]
    .map((m) => Number(m[1]));

  const fnStart = cppSrc.indexOf('MakePBRVertexLayout() {');
  if (fnStart === -1) return { ok: false, why: 'MakePBRVertexLayout not found in RenderSystem.cpp' };
  const fnEnd = cppSrc.indexOf('return vl;', fnStart);
  // Each attribute is {format, offset, location}; the location is the last.
  const supplied = [...cppSrc.slice(fnStart, fnEnd).matchAll(/,\s*(\d+)\s*\}/g)]
    .map((m) => Number(m[1]));

  const missing = declared.filter((loc) => !supplied.includes(loc));
  if (missing.length) {
    return {
      ok: false,
      why: `PBR_VS reads @location(${missing.join(', ')}) but MakePBRVertexLayout does not supply `
         + `${missing.length > 1 ? 'them' : 'it'}. Every pipeline using this shader fails to create, `
         + 'and the frame renders black.',
    };
  }

  // A layout may legitimately carry more than the shader reads today, so extras
  // are not an error, only a silence worth breaking.
  const extra = supplied.filter((loc) => !declared.includes(loc));
  return { ok: true, declared: declared.length, extra };
}

const contract = checkVertexContract();
if (contract.ok) {
  const note = contract.extra.length ? `  (layout also carries @location(${contract.extra.join(', ')}), unread)` : '';
  console.log(`ok    PBR vertex contract: ${contract.declared} attributes supplied${note}`);
} else {
  failed++;
  console.error('FAIL  PBR vertex contract');
  console.error(`  ${contract.why}`);
}

console.log(`\n${shaders.length - failed}/${shaders.length} shaders compile`);
if (uncaptured) console.error(`${uncaptured} uncaptured device error(s)`);
process.exit(failed || uncaptured ? 1 : 0);
