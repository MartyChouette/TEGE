# TEGE Editor MCP Server

The editor can host a [Model Context Protocol](https://modelcontextprotocol.io)
server - MCP is an open, published protocol with many independent client
implementations - so ANY MCP-speaking assistant or tool can drive a **running
editor**: inspect the scene, read and write any component, create entities,
control play mode, and capture screenshots of the game view. No vendor
account, no cloud service, no telemetry: a plain JSON-RPC endpoint on your own
machine, over an open protocol, speaking the engine's own open scene-JSON.

## Enable it

Settings → System → **MCP Server** → *Enable MCP Server*. The status line shows
the endpoint (default `http://127.0.0.1:8971/mcp`). Localhost-only by design;
off by default.

## Connect

Point any streamable-HTTP MCP client at the endpoint. For example:

```bash
# Claude Code
claude mcp add --transport http tege http://127.0.0.1:8971/mcp

# or any other MCP client - it is one HTTP URL
```

## Tools

| Tool | What it does |
|---|---|
| `scene_info` | Project path, play state, entity count. |
| `list_entities` | Entities with names and their component keys. |
| `find_entity` | Entity id by name. |
| `create_entity` / `destroy_entity` | Entity lifecycle (destroy is deferred one frame). |
| `get_component` | One component as scene-JSON. |
| `set_component` | Write component fields from JSON (replaces; adds if missing). |
| `add_component` / `remove_component` | Default-add / remove by key. |
| `registered_component_keys` | Every key the engine can serialize (~160). |
| `play_control` | `play` / `pause` / `resume` / `stop`. |
| `capture_view` | Save the game view as PNG, returns the path. |
| `spawn_prefab` | Instantiate a `.enjprefab` (project-relative path) at an optional position. |
| `build_game` | Start an async build (`target`: `desktop`/`web`, `run` to launch on success); returns immediately, poll `scene_info` for progress. |

Component JSON uses the exact same schema as `.enjin` scene files — whatever the
serializer writes, `set_component` accepts.

## How it works

The HTTP listener runs on a worker thread but **executes nothing there**: tool
calls queue and run on the editor main thread once per frame (the engine's ECS
allows structural mutation only from the owner thread — adr-0004). A busy or
modal editor answers with a timeout error after 10 seconds instead of hanging
the client.

## Example session

```
> list the entities in my scene
> set the sun's light intensity to 2.5
> enter play mode, wait, capture the view
> add a scatter component to the terrain entity and generate 200 trees
```

Each of those is one or two tool calls — the assistant reads a component with
`get_component`, edits the JSON, writes it back with `set_component`, and can
verify visually with `capture_view`.
