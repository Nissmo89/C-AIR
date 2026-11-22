# C-AIR Roadmap

This roadmap follows the current C11, Raven, and CMake architecture. Keep UI
widgets in separate `.c` and `.h` files, keep non-UI logic independently
testable, and add tests for every behavior change as required by `RULES.md`.

## 1. Output panel

- [ ] Add an output panel widget in its own `src/widgets/output_panel.c` and
  `.h` files, with a clear, readable layout that follows the active Raven theme.
- [ ] Add a separate output model/API for appending messages with a source,
  severity, and timestamp where useful; keep rendering out of the model.
- [ ] Show compiler and application messages in the panel. Keep normal output,
  warnings, and errors distinguishable and readable.
- [ ] Support clearing output and placing focus or navigation on a selected
  diagnostic when it maps to a source location.
- [ ] Bound retained output and handle long lines and large bursts without
  unbounded memory use or UI stalls.
- [ ] Add unit tests for append, clear, ordering, empty input, long messages,
  capacity limits, and allocation/error paths.
- [ ] Add Raven null-backend integration tests for visibility, theme behavior,
  and diagnostic navigation.

## 2. Integrated terminal

- [ ] Define the terminal's initial scope: interactive shell session, working
  directory, command input, output, exit status, and close/restart behavior.
- [ ] Keep process management in a platform module and terminal presentation in
  a separate widget module. Use native C APIs and avoid adding a C++ runtime.
- [ ] On Windows, investigate ConPTY for interactive terminal behavior; keep
  process handles, pipes, buffers, resizing, and shutdown ownership explicit.
- [ ] Stream output without blocking the Raven UI thread. Handle encoding,
  partial reads/writes, large output, process exit, and user-initiated close.
- [ ] Add tests for process start failure, working directory, input/output,
  exit codes, rapid output, resize, shutdown, and repeated open/close cycles.
- [ ] Keep terminal tests deterministic and isolated. Use fixed harmless test
  commands and time limits; never execute arbitrary project source in CTest.

## 3. libtcc compiler integration

- [ ] Confirm supported libtcc versions, licensing, and build/distribution
  requirements for each supported toolchain before adding the dependency.
- [ ] Create a separate compiler service module with a small C API for compiler
  setup, build requests, diagnostics, and cleanup. Keep UI code out of it.
- [ ] Determine how C-AIR will provide include paths, library paths, target
  settings, and source file contents without relying on the user's shell setup.
- [ ] Send compiler diagnostics, including file and line locations, to the
  output panel and connect navigable diagnostics to the editor.
- [ ] Separate compilation from execution. Run generated programs only after an
  explicit user action; make the behavior and platform limits clear in the UI.
- [ ] Add compiler tests for valid C, syntax/type errors, missing includes,
  multiple source files, symbols/libraries, repeated builds, and cleanup after
  each failure path.
- [ ] Use small, trusted fixture programs for tests. Keep normal tests from
  executing arbitrary workspace or user source code.
- [ ] Add bounded fuzz/property tests for compiler input handling where the
  libtcc API and test environment support safe, reproducible runs.

## 4. Project explorer redesign

- [ ] Review the existing project tree model and explorer widget before changing
  their responsibilities or visual structure.
- [ ] Improve hierarchy, spacing, selection and hover states, file/folder
  affordances, long-name handling, and dark/light theme consistency.
- [ ] Keep rendering and interaction in the explorer widget; keep scanning,
  ownership, and path logic in the project model.
- [ ] Define and test each supported operation: select, expand/collapse, open a
  file, refresh/rescan, and report inaccessible or missing paths.
- [ ] Add operation-level integration tests using a temporary project tree,
  including empty folders, nested folders, long names, missing files, and
  repeated refresh/open cycles.
- [ ] Add accessibility and keyboard navigation behavior consistent with
  Raven's existing widget patterns.

## 5. Testing and reliability work

- [ ] Maintain unit, Raven integration, end-to-end startup, regression, and
  sanitizer/static-analysis coverage as described in `RULES.md`.
- [ ] For each public operation, cover normal use, invalid input, empty state,
  boundary values, failure handling, repeated use, and cleanup/ownership.
- [ ] Add regression tests for every crash, stale handle, wrong visibility
  state, incorrect diagnostic, and resource leak fixed in the project.
- [ ] Keep file/process tests isolated in unique temporary locations and clean
  them up on success and failure.
- [ ] Add stress tests for large project trees, high-volume output, many open
  tabs, repeated tab close/reopen, and terminal start/stop cycles.
- [ ] Run CTest after each feature, plus AddressSanitizer, UBSan, compiler
  warnings, and a static analyzer where supported.
- [ ] Keep all test commands deterministic, bounded by time/resource limits,
  and independent from the developer's current project files.

## Suggested implementation order

1. Expand explorer operation tests and complete the explorer redesign.
2. Add the output model and panel, then connect it to the existing UI.
3. Add the compiler service and diagnostics; test compilation without running
   arbitrary source.
4. Add explicit run behavior for compiled programs with clear lifecycle and
   platform boundaries.
5. Add the terminal backend and widget, then test process lifecycle and output.
6. Review the full test matrix, memory ownership, packaging, licensing, and
   fresh-clone build instructions.
