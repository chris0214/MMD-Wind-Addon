# WindTool Architecture

## Runtime chain

WindTool uses a small `MMEffect.dll` forwarding loader. The loader preserves the
standard MME exports, forwards each call to `MMEffect.original.dll`, and invokes
WindTool at the audited begin-scene boundary on the MMD UI thread.

```text
MikuMikudance.exe
  -> MMEffect.dll (WindTool forwarder)
       -> MMEffect.original.dll
       -> PhysicsControlStudio/MmdPhysicsControlStudio.dll
```

The internal `PhysicsControlStudio` file names are retained for compatibility
with existing installations and track files. The visible product name is
WindTool.

## Modules

- `src/host_plugin.cpp`: host validation, model/rigid-body discovery, Win32
  overlay UI, target selection, frame callback and guarded Bullet writes.
- `src/wind.cpp`: deterministic wind-field and noise evaluation.
- `src/physics_track.cpp`: keyframe storage and interpolation.
- `src/track_json.cpp`: version-tolerant JSON track and target-group storage.
- `src/core.cpp`: capability, validation, command and transaction primitives.
- `forwarder/`: MME forwarding loader and WindTool frame bridge.
- `sdk/`: the minimal MMD 9.31 ABI declarations required to compile WindTool.

## Physics backend

The supported MMD build stores a direct `btRigidBody` pointer in each dynamic
rigid-body runtime record. WindTool reads current world position and writes
validated velocity/damping fields using the following version-locked offsets:

```text
world position                +0x40
interpolation linear velocity +0x90
activation state              +0xEC
solver linear velocity        +0x150
linear damping                +0x1F0
angular damping               +0x1F4
```

Wind acceleration is integrated using a high-resolution real-time clock rather
than the MMD timeline frame number. This keeps force magnitude stable across
viewport frame rates and allows physics to continue while the timeline is
paused.

Target gravity is implemented as a per-frame velocity compensation against
MMD's native gravity. WindTool does not replace or own the global Bullet world.

## Write safety

Before a frame can write physics state, WindTool verifies:

1. Exact `MikuMikudance.exe` name, size and SHA-256.
2. Execution on the captured MMD UI thread.
3. Model-table and rigid-body count bounds.
4. Dynamic-body mode and non-null runtime object.
5. Readable source ranges and writable destination ranges.
6. A finite, validated control snapshot.

Every multi-body update records the previous value. A failed write rolls back
values already changed during that frame. Unsupported hosts fail closed.

## Track format

WindTool deliberately does not modify PMM files. Keyframes and named target
groups are stored in `PhysicsControlStudio.json` beside the installed plugin.
Numeric controls interpolate linearly; switches, field types and target sets use
step interpolation.

## Extension boundaries

The core command model reserves room for additional material properties,
constraint rebuilds and bone writeback policies. These are not exposed until
their live Bullet/MMD ownership rules and rollback paths are verified for the
supported host build.
