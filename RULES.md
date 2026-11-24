# C Application Rules

These rules apply to every C application in this workspace. Keep this file
current when the project adopts a new tool, platform, subsystem, or test layer.

## Language and build

- Write application and UI code in C11. Add C++ or Rust only when a concrete
  requirement cannot be met cleanly in C, and isolate that code behind a C ABI.
- Build with CMake and link Raven through its exported `raven::raven` target.
- Keep warnings enabled. Fix warnings instead of suppressing them globally.
- Keep generated files, executables, and build trees out of source directories.
- Use explicit ownership and cleanup for every allocation and OS resource.
- Check return values from allocations, file operations, system calls, and Raven
  APIs that can fail. Validate sizes before copying or formatting into buffers.
- Avoid global mutable state when a small context structure can own the state.
- Keep platform code behind a narrow module boundary and document supported
  platforms and assumptions.

## Modularity

- Give each UI widget or cohesive widget group its own `.c` and `.h` pair under
  `src/widgets/`.
- A widget module owns its construction, state, and widget-specific behavior.
  Other modules use its public functions instead of reaching into its internals.
- Keep application composition in `src/app.c`; keep `main.c` limited to argument
  handling and starting the application.
- Put non-UI data structures and logic in separate model modules. Model code
  should not depend on Raven so unit tests can run without a GUI backend.
- Keep public headers small, use include guards, and prefix public names with
  `cair_`.

## Required testing

Every behavior change must add or update tests. The minimum test layers are:

1. **Unit tests:** exercise model and utility behavior, including invalid input,
   empty data, boundary sizes, and failure paths.
2. **Integration tests:** construct real module combinations and verify their
   contracts together. Use Raven's null backend for UI modules when possible.
3. **End-to-end smoke tests:** launch the application in a noninteractive test
   mode and check initialization, composition, and clean shutdown.
4. **Regression tests:** preserve a test for every fixed defect or crash.
5. **Fuzz/property tests:** add them for parsers, file formats, compiler input,
   and other code that accepts broad or untrusted input. Keep seeds and limits
   reproducible.
6. **Memory and static checks:** run AddressSanitizer or the platform's closest
   available memory checker, plus compiler warnings and a static analyzer.
   Record unavailable checks and the reason in the change summary.

Keep tests deterministic and isolated from user files. Tests that create files
must use a unique temporary directory and remove it on success and failure.
Never make the normal test suite execute arbitrary project source code.

CTest is the source of truth for automated tests:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="C:/Users/khali/Code/Raven/install"
cmake --build build
ctest --test-dir build --output-on-failure
```

Add each test executable with `add_test`. Keep unit tests independent of the
Raven runtime; use `R_BACKEND_NULL` for Raven integration and startup smoke
tests. Enable the sanitizer build with
`-DCAIR_ENABLE_SANITIZERS=ON` when using a supported GCC or Clang toolchain.

## Change review

- A change is ready when the implementation is modular, the relevant test
  layers pass, and any unsupported test layer is called out with a reason.
- Keep behavior and test names specific enough that a failed CTest result points
  to the affected subsystem.
- Update `BUILDING_C_APPS.md` when build prerequisites or commands change.
- Follow all commit documentation and message requirements specified in `RULE.md`.
  Every commit must include full context, problem explanation, Q&A, and a step-by-step
  walkthrough.
