# Wind release activation fix

This note records the MikuMikuDance 9.31 x64 evidence used by WindTool when
waking a rigid body after wind writes or release cleanup.

## Frame order

`analysis/reports/architecture/frame_pipeline_calls.csv` places the MME/D3D
`BeginScene` callback before these MMD physics stages:

1. `MMD_SyncKinematicRigidBodiesFromBones`
2. `btDiscreteDynamicsWorld::stepSimulation`
3. `MMD_ApplyRigidBodyTransformsToBones`

WindTool therefore prepares forces and release state before Bullet advances the
next simulation step.

## Activation functions

MMD 9.31 x64 SHA-256:

```text
2C9414C21619B4AD85D9C2EF76836F3C34DB7A8ABD07BD6C6176D385F7EFDFB4
```

The binary contains two adjacent Bullet functions:

- `0x1400ED310`, `btCollisionObject::forceActivationState(int)`
  - changes `body + 0xEC`
  - does not change the deactivation timer
- `0x1400ED330`, `btCollisionObject::activate(bool)`
  - changes `body + 0xEC` to the active state for eligible bodies
  - writes zero to `body + 0xF0` (`m_deactivationTime`)

The relevant `activate(bool)` instructions are:

```text
1400ED330  test dl, dl
1400ED334  test byte ptr [rcx+0xE0], 3
1400ED34A  mov r8d, 1
1400ED357  mov [rcx+0xEC], eax
1400ED35D  mov dword ptr [rcx+0xF0], 0
```

WindTool uses RVA `0xED330` with `forceActivation=false`. Wind targets are
already filtered to dynamic MMD rigid bodies, and the function itself preserves
Bullet's static/kinematic guard.

## Why this matters during release

Release cleanup clears accumulated force plus solver and interpolation
velocities. Calling only `forceActivationState(1)` can leave an old
`m_deactivationTime`; Bullet may put the zero-velocity body back to sleep while
it is still displaced by the former wind. Calling `activate(false)` also resets
that timer, allowing MMD gravity and joint constraints to move the body back
toward its normal pose.

## Why WindTool does not call MMD's full reset

`MMD_ResetRigidBodiesToBonePose` at `0x1400C45C0` reconstructs transforms from
bone poses and resets every dynamic body in the model. Using it for a local
WindTool target would also snap unrelated hair, cloth, and accessories, so the
release path deliberately leaves transforms to MMD's normal physics solver.
