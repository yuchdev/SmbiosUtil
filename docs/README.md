# Documentation Registry

| Path | Purpose |
|------|---------|
| [README.md](README.md) | This registry: inventory of every document under `docs/`. |
| [adr/README.md](adr/README.md) | How libbios records Architecture Decision Records (MADR format, `000N-slug.md` naming). |
| [adr/template.md](adr/template.md) | MADR template that `/adr-write` fills in for a new ADR. |
| [agent/tutorial.md](agent/tutorial.md) | How to work with the agents, skills and hooks, including which agent owns which part of libbios. |
| [agent/agents.md](agent/agents.md) | Generated reference of installed agents (model, description). Do not hand-edit. |
| [agent/skills.md](agent/skills.md) | Generated reference of installed skills and their invocation. Do not hand-edit. |
| [agent/hooks.md](agent/hooks.md) | Generated reference of installed hooks and their triggers. Do not hand-edit. |
| [dev/cpp_coding_standard.md](dev/cpp_coding_standard.md) | libbios C++ style guide that `cpp-expert`, `testing-expert` and the reviewers follow. |
| [roadmap/README.md](roadmap/README.md) | Roadmap index and the Milestone → Story → Task hierarchy; six-milestone program taking libbios to a portable, UEFI-capable C++20 library. |
| [roadmap/0001-portable-build-system-and-dependencies/plan.md](roadmap/0001-portable-build-system-and-dependencies/plan.md) | Milestone 0001: vcpkg manifest, CMake presets, CI matrix - replaces manually-pointed Boost installs. |
| [roadmap/0002-cpp20-modernization-and-bugfixes/plan.md](roadmap/0002-cpp20-modernization-and-bugfixes/plan.md) | Milestone 0002: raise to C++20, `-Werror`/sanitizers, fix the `SMBiosVersion` comparison and `unix_bios.cpp` bugs. |
| [roadmap/0003-portable-byte-level-smbios-parsing/plan.md](roadmap/0003-portable-byte-level-smbios-parsing/plan.md) | Milestone 0003: endian-safe, bounds-checked field accessors replacing `reinterpret_cast` onto versioned structs. |
| [roadmap/0004-entry-model-and-factory-modernization/plan.md](roadmap/0004-entry-model-and-factory-modernization/plan.md) | Milestone 0004: self-registering factory; the eight remaining SMBIOS structure types. |
| [roadmap/0005-uefi-and-native-source-abstraction/plan.md](roadmap/0005-uefi-and-native-source-abstraction/plan.md) | Milestone 0005: `ISmbiosSource` strategy interface; real Linux sysfs/EFI sources; new macOS UEFI source. |
| [roadmap/0006-testing-hardening-and-documentation/plan.md](roadmap/0006-testing-hardening-and-documentation/plan.md) | Milestone 0006: hermetic unprivileged test layer, boundary/fuzz tests, documentation pass. |
| [roadmap/0007-c-abi-shim-for-cross-language-interop/plan.md](roadmap/0007-c-abi-shim-for-cross-language-interop/plan.md) | Milestone 0007: C ABI shim (opaque handles, exception-to-error-code boundary) over the finished C++ core, for Python/Rust/C#/Go FFI. |
| [reviews/README.md](reviews/README.md) | Conventions for `background-reviewer` reports (dependency audits, secret scans, performance, licenses). |
| [reviews/example-report.md](reviews/example-report.md) | Example efficiency-audit report showing the expected shape. |
| [security/README.md](security/README.md) | `security-auditor`'s directory conventions, security rules, and the initial libbios threat-model draft. |
| [test/code_test_coverage.md](test/code_test_coverage.md) | Coverage checklist and the lcov/genhtml recipe for a `--coverage` CMake build. |
