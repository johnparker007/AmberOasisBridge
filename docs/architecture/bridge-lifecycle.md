# Bridge lifecycle

The supported policy is one active emulator instance per process. `Create("jpm-system6")` loads and validates the core, returning an opaque handle; a concurrent second create returns `AMBER_INSTANCE_LIMIT`.

The lifecycle is `Created -> Initialised -> Running`, with `Reset` returning Running to Initialised, then `Shutdown -> Destroyed`. Run is also repeatable in Running. Initialise twice, Run before Initialise, Reset after Destroy, Destroy while Initialised/Running, and reuse of a destroyed handle are rejected. An initialisation failure enters Error; Shutdown performs core cleanup before Destroy. Calls for a process must be serialized by the frontend.

Initialise first invokes JPM `Initialise`. When program paths are present it invokes `LoadROM`; sound paths are optional and invoke `LoadSoundROM`. V1 mirrors the JPM four-slot path model. The current JPM implementation requires its valid program ROM combination; the bridge does not reinterpret ROM contents.
