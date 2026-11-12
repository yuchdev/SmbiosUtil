# 6. Process: test tiers and CI jobs

Sources: `tests/CMakeLists.txt`, `tests/gui/CMakeLists.txt`, the root
`CMakeLists.txt` and `.github/workflows/ci.yml`. Mermaid, not rendered
locally.

## 6a. Which tests are registered, and when

```mermaid
flowchart TD
    ct["ctest, root enable_testing"] --> base["always registered"]
    ct --> guiopt{"SMBIOS_BUILD_GUI=ON?"}
    base --> unit["smbios_unit_test<br/>hermetic, synthetic tables"]
    base --> mock["smbios_mock_test<br/>empty placeholder"]
    base --> cabi["smbios_c_* tests"]
    base --> ex["smbios_example_* and<br/>smbios_cli_unknown_format* tests"]
    base --> hw["smbios_integration_test<br/>smbios_perf_test<br/>live hardware, need root"]
    guiopt -->|yes| ci["smbios_gui_ci_test<br/>label gui, gui-ci<br/>headless ImGui"]
    guiopt -->|yes| ciq{"CI or GITHUB_ACTIONS set?"}
    ciq -->|no| loc["smbios_gui_local_test<br/>smbios_gui_autoexit<br/>label gui, gui-local"]
    ciq -->|yes| off["local tier forced OFF,<br/>never configured"]
```

`smbios_gui_local_test` needs a real display, OpenGL context, OS clipboard
and live table. Run a tier with
`ctest --test-dir build/<os>-gui -L gui-ci|gui-local|gui`.

## 6b. CI jobs (`.github/workflows/ci.yml`, on push and pull request)

```mermaid
flowchart LR
    push["push or pull_request"] --> build["build<br/>linux, windows, macos"]
    push --> san["sanitizers<br/>linux, macos"]
    push --> gb["gui-build<br/>linux-gui, windows-gui, macos-gui"]

    build --> b1["cmake --preset, build"]
    b1 --> b2["unprivileged ctest:<br/>unit, mock, smbios_c_, cli, example"]
    b2 --> b3["best-effort privileged:<br/>functional, formats, integration, perf"]

    san --> s1["configure with<br/>-fsanitize=address,undefined"]
    s1 --> s2["same unprivileged ctest set"]
    s2 --> s3["best-effort privileged tests"]

    gb --> g1["configure and build<br/>per-OS gui preset"]
    g1 --> g2["ctest -L gui-ci"]
    g2 --> g3["assert 0 gui-local tests<br/>and no local binaries"]
```

Notes:

- The Windows unprivileged tests run in `build` because sanitizers are not
  set up for MSVC; Linux and macOS run them in `sanitizers`.
- The `gui-build` job never runs `smbios_gui` itself (no display on hosted
  runners).
- Steps that need elevated privileges are `continue-on-error` or print a
  warning; they complement, not replace, the unprivileged suite.
