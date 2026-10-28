# NEngine mini sample games

These are **native, console-driven minimum game examples**, not completed graphical
Vulkan projects or Android APKs. Both use the shipped NEngine `PlayerRuntime`
and the same real Scene serialization, Input.ninput authoring data, Player export,
fixed-step physics and a finish-line rule. The 2D game also packages and
samples a real `.nanim` sprite animation clip.

Build with `-DNENGINE_BUILD_TESTS=ON -DNENGINE_BUILD_PLAYER=ON`.
Executable names: `NEngineMiniRunner2D` and `NEngineMiniRunner3D`.

- `NEngineMiniRunner2D --self-test` generates and exports a 2D project,
  loads it in PlayerRuntime, presses mapped MoveRight (D), and verifies
  animation, 2D physics and a win after simulated ticks.
- `NEngineMiniRunner3D --self-test` runs the same gameplay lifecycle with
  a 3D Rigidbody and MeshRenderer.
- `NEngineMiniRunner2D --play` or `NEngineMiniRunner3D --play` lets a
  human type `d` to move right, `a` to move left or `q` to quit.
  Each input is one fixed tick; moving right enough ticks reaches the goal.

Both are built and executed on Windows and Ubuntu in the CTest matrix.
The generated project is temporary and is removed after the program exits.

**Important scope:** This sample demonstrates input, Scene/Player parity and
gameplay logic, but it does not show textured graphics or an interactive Vulkan
window; those remain independent acceptance requirements.

C# gameplay tutorial and toolchain limitations: [managed/README.md](managed/README.md) and [managed/NavQuickstart.cs](managed/NavQuickstart.cs). Both follow the current NEngine 0.9.0-dev API; see the canonical docs checklist for exact acceptance scope.
