# Copilot Instructions for Image Processing Laboratory

## Project Context
- This is a C++20 image-processing playground built with CMake and Conan.
- Main entry point is `main.cpp`; source files are primarily under `src/`.
- The executable target is `opengl_template`.

## Coding Rules
- Prefer small, localized changes and preserve current public APIs unless explicitly asked to refactor.
- Match the existing C++ style and naming in nearby code.
- Keep effect logic deterministic and avoid hidden side effects.
- Use existing helpers/utilities before introducing new abstractions.
- Add concise comments only for non-obvious logic.

## Build and Validation
- After code changes, verify with the existing workspace flow when possible:
  1. `Conan Install` task (if dependencies/config changed)
  2. `CMake: Configure` task (if build config changed)
  3. `CMake: Build` task
- Prioritize fixing compile errors introduced by the current change.

## Image Processing App Conventions
- Keep UI/control behavior consistent with `ControlsManager` and slider parameter semantics.
- For parameterized effects, preserve `min/max/default/neutral/enabled` behavior.
- Use `getEffective(...)` when ON/OFF state should affect the result.
- Keep bounds handling robust for trackbar-backed parameters.

## File and Asset Expectations
- Do not remove or rename expected assets/folders under `assets/` unless requested.
- Prefer exporting generated outputs to existing export paths and utilities.

## Safety
- Do not run destructive git operations unless explicitly requested.
- Do not revert unrelated user changes.
