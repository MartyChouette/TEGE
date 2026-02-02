# Sample 01: Hello World

A minimal Enjin scene demonstrating the basics: one entity with a mesh and a light.

## What This Demonstrates

- Creating an entity with `TransformComponent` and `MeshComponent`
- Adding a `MaterialComponent` with PBR properties
- Setting up a `LightComponent` for scene illumination
- Camera positioning for viewing the scene

## Scene Contents

| Entity | Components | Purpose |
|--------|-----------|---------|
| Cube | Transform, Mesh, Material, Name | A simple cube at the origin |
| Light | Transform, Light, Name | Directional light illuminating the scene |
| Camera | Transform, Camera, Name | View camera |

## How to Use

1. Open Enjin Editor
2. **File > Open Scene** and select `hello_world.enjin`
3. Press **Play** to enter play mode

## Key Concepts

### Entities and Components

Everything in Enjin is an **entity** (a u64 ID) with **components** (data structs) attached. The editor's **Hierarchy** panel lists entities; the **Inspector** panel shows their components.

### Transform

Every visible entity needs a `TransformComponent`:
- **Position**: World-space location (x, y, z)
- **Rotation**: Euler angles in degrees
- **Scale**: Size multiplier per axis

### Materials

`MaterialComponent` controls appearance:
- `baseColor` (RGB) - Surface color
- `metallic` (0-1) - Metal vs dielectric
- `roughness` (0-1) - Surface smoothness
