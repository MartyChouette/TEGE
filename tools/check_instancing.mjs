// Does firstInstance actually index the shared ObjectData buffer?
//
// The web scene pass now uploads every visible object's ObjectData once, in
// draw order, into ONE storage buffer, and each draw says which instance it
// starts at. That rests on a claim I had not tested: that
// wgpuRenderPassEncoderDrawIndexed's firstInstance reaches the shader as
// @builtin(instance_index), so objects.data[instance_index] lands on the right
// entry.
//
// If that is wrong, objects render with each other's transforms. This renders
// four quads from one buffer using four separate draws with firstInstance
// 0..3, each entry carrying a distinct colour and X offset, and checks the
// pixels land where the buffer says.
//
// Run: node check_instancing.mjs
import { create, globals } from 'webgpu';
Object.assign(globalThis, globals);

const W = 400, H = 100;
const N = 4;

const WGSL = `
struct ObjectData { offsetX : f32, r : f32, g : f32, b : f32, };
struct ObjectDataArray { data : array<ObjectData>, };
@group(0) @binding(0) var<storage, read> objects : ObjectDataArray;

struct VSOut { @builtin(position) pos : vec4<f32>, @location(0) color : vec3<f32>, };

@vertex
fn vs_main(@builtin(vertex_index) vid : u32,
           @builtin(instance_index) inst : u32) -> VSOut {
    // A quad covering the left QUARTER of clip space, shifted by the object's X.
    var xs = array<f32, 6>(-1.0, -0.5, -1.0, -0.5, -0.5, -1.0);
    var ys = array<f32, 6>(-1.0, -1.0,  1.0, -1.0,  1.0,  1.0);
    let o = objects.data[inst];
    var out : VSOut;
    out.pos = vec4<f32>(xs[vid] + o.offsetX, ys[vid], 0.0, 1.0);
    out.color = vec3<f32>(o.r, o.g, o.b);
    return out;
}

@fragment
fn fs_main(in : VSOut) -> @location(0) vec4<f32> {
    return vec4<f32>(in.color, 1.0);
}
`;

const gpu = create([]);
const adapter = await gpu.requestAdapter();
if (!adapter) { console.error('no WebGPU adapter'); process.exit(2); }
const device = await adapter.requestDevice();
device.addEventListener?.('uncapturederror', e => console.error('DEVICE ERROR:', e.error?.message));

const mod = device.createShaderModule({ code: WGSL });
for (const m of (await mod.getCompilationInfo()).messages) {
  if (m.type === 'error') { console.error(`shader ${m.lineNum}: ${m.message}`); process.exit(1); }
}

const layout = device.createBindGroupLayout({
  entries: [{ binding: 0, visibility: GPUShaderStage.VERTEX, buffer: { type: 'read-only-storage' } }],
});

const pipeline = device.createRenderPipeline({
  layout: device.createPipelineLayout({ bindGroupLayouts: [layout] }),
  vertex: { module: mod, entryPoint: 'vs_main' },
  fragment: { module: mod, entryPoint: 'fs_main', targets: [{ format: 'rgba8unorm' }] },
  primitive: { topology: 'triangle-list' },
});

// Four objects: each shifted a quarter of the screen further right, each a
// distinct primary. Entry i must paint band i.
const expect = [
  { x: 0.0,  rgb: [255, 0, 0] },
  { x: 0.5,  rgb: [0, 255, 0] },
  { x: 1.0,  rgb: [0, 0, 255] },
  { x: 1.5,  rgb: [255, 255, 0] },
];
const data = new Float32Array(N * 4);
expect.forEach((e, i) => {
  data[i * 4 + 0] = e.x;
  data[i * 4 + 1] = e.rgb[0] / 255;
  data[i * 4 + 2] = e.rgb[1] / 255;
  data[i * 4 + 3] = e.rgb[2] / 255;
});

const objBuf = device.createBuffer({
  size: data.byteLength,
  usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST,
});
device.queue.writeBuffer(objBuf, 0, data);

// ONE bind group for the whole buffer, bound once - the thing the change relies on.
const bg = device.createBindGroup({ layout, entries: [{ binding: 0, resource: { buffer: objBuf } }] });

const tex = device.createTexture({
  size: [W, H], format: 'rgba8unorm',
  usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_SRC,
});

const enc = device.createCommandEncoder();
const pass = enc.beginRenderPass({
  colorAttachments: [{
    view: tex.createView(),
    clearValue: { r: 0, g: 0, b: 0, a: 1 },
    loadOp: 'clear', storeOp: 'store',
  }],
});
pass.setPipeline(pipeline);
pass.setBindGroup(0, bg);
// Four separate draws, each one instance, each starting at a different index.
// This is exactly the per-entity path.
for (let i = 0; i < N; i++) pass.draw(6, 1, 0, i);
pass.end();

const bpr = Math.ceil(W * 4 / 256) * 256;
const rb = device.createBuffer({ size: bpr * H, usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ });
enc.copyTextureToBuffer({ texture: tex }, { buffer: rb, bytesPerRow: bpr }, [W, H]);
device.queue.submit([enc.finish()]);
await rb.mapAsync(GPUMapMode.READ);
const px = new Uint8Array(rb.getMappedRange());

// Sample the middle of each expected band.
let failed = 0;
const y = H >> 1;
expect.forEach((e, i) => {
  const cx = Math.floor((i + 0.5) * (W / N));
  const o = y * bpr + cx * 4;
  const got = [px[o], px[o + 1], px[o + 2]];
  const ok = got.every((v, k) => Math.abs(v - e.rgb[k]) <= 2);
  if (!ok) failed++;
  console.log(`  instance ${i}: expected rgb(${e.rgb}) at x=${cx}, got rgb(${got})  ${ok ? 'ok' : 'MISMATCH'}`);
});

// The batched path: one draw, N instances, firstInstance 0 - every entry used.
console.log('\nbatched draw (one call, 4 instances):');
{
  const enc2 = device.createCommandEncoder();
  const p2 = enc2.beginRenderPass({
    colorAttachments: [{
      view: tex.createView(), clearValue: { r: 0, g: 0, b: 0, a: 1 },
      loadOp: 'clear', storeOp: 'store',
    }],
  });
  p2.setPipeline(pipeline);
  p2.setBindGroup(0, bg);
  p2.draw(6, N, 0, 0);
  p2.end();
  const rb2 = device.createBuffer({ size: bpr * H, usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ });
  enc2.copyTextureToBuffer({ texture: tex }, { buffer: rb2, bytesPerRow: bpr }, [W, H]);
  device.queue.submit([enc2.finish()]);
  await rb2.mapAsync(GPUMapMode.READ);
  const p = new Uint8Array(rb2.getMappedRange());
  expect.forEach((e, i) => {
    const cx = Math.floor((i + 0.5) * (W / N));
    const o = y * bpr + cx * 4;
    const got = [p[o], p[o + 1], p[o + 2]];
    const ok = got.every((v, k) => Math.abs(v - e.rgb[k]) <= 2);
    if (!ok) failed++;
    console.log(`  instance ${i}: expected rgb(${e.rgb}), got rgb(${got})  ${ok ? 'ok' : 'MISMATCH'}`);
  });
}

console.log(failed === 0
  ? '\nPASS - firstInstance indexes the shared buffer correctly, both paths'
  : `\nFAIL - ${failed} mismatch(es)`);
process.exit(failed === 0 ? 0 : 1);
