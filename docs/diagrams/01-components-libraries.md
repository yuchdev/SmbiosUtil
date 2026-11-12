# 1. Components: libraries and link dependencies

Build-time view of every CMake target and how it links. Sources of truth:
the root `CMakeLists.txt`, each `<dir>/CMakeLists.txt`, and
[ADR 0001](../adr/0001-per-os-smbios-source-libraries.md). Mermaid, not
rendered locally; checked by hand against the grammar.

## 1a. Per-platform selection (CMake time, never `#if`)

The root `CMakeLists.txt` branch sets `SMBIOS_PLATFORM_LIB` and
`SMBIOS_LEGACY_LIB`; `libsmbios` links both `PUBLIC`.

```mermaid
flowchart TD
    root{"root CMakeLists.txt<br/>platform branch"}
    root -->|WIN32| win["libsmbios_win<br/>+ libsmbios_win_legacy"]
    root -->|APPLE| mac["libsmbios_macos<br/>+ libsmbios_posix_legacy"]
    root -->|Linux| lin["libsmbios_linux<br/>+ libsmbios_posix_legacy"]
    root -->|other nix| oth["libsmbios_posix_legacy only<br/>SMBIOS_PLATFORM_LIB empty"]
    win --> core["libsmbios<br/>links PLATFORM_LIB and LEGACY_LIB"]
    mac --> core
    lin --> core
    oth --> fb["fallback_source_chain.cpp<br/>added to libsmbios sources"]
    fb --> core
```

## 1b. Link graph of the products

Per-OS and legacy libraries are shown as one node each: exactly one
flavor of each is linked per build (see 1a). `libsmbios_macos` also links
the IOKit and CoreFoundation frameworks; the legacy libraries also link
Boost.

```mermaid
flowchart LR
    sys["sys_interface<br/>header-only INTERFACE"]
    legacy["legacy flavor<br/>win_legacy or posix_legacy"]
    perOS["per-OS library<br/>win, linux or macos"]
    core["libsmbios<br/>static"]
    cli["smbios<br/>CLI executable"]
    cabi["smbios_c<br/>SHARED C ABI"]
    model["libsmbios_gui_model<br/>static, no imgui"]
    gui["smbios_gui<br/>exe, SMBIOS_BUILD_GUI=ON"]
    twin["smbios_gui_autoexit<br/>local-only test twin"]

    legacy --> sys
    perOS --> sys
    perOS --> legacy
    core --> sys
    core --> legacy
    core --> perOS
    cli -->|whole-archive| core
    cabi -->|whole-archive| core
    model --> core
    gui -->|whole-archive| core
    gui --> model
    twin -.->|same sources plus SMBIOS_GUI_TEST_HOOKS| gui
```

`smbios_gui` and `smbios_gui_autoexit` also link `imgui::imgui`, `glfw`
and `OpenGL::GL` (omitted above to keep the picture small).
`smbios_gui_autoexit` compiles `smbios_gui`'s `main.cpp` again with its
own private definition; it is never created on CI.

## 1c. Test executables and what they link

Tiers are described in [diagram 6](06-test-and-ci-flow.md).

```mermaid
flowchart LR
    core["libsmbios"]
    model["libsmbios_gui_model"]
    cabi["smbios_c"]
    unit["smbios_unit_test"]
    mock["smbios_mock_test"]
    integ["smbios_integration_test"]
    perf["smbios_perf_test"]
    cabit["smbios_c_* tests"]
    guici["smbios_gui_ci_test"]
    guiloc["smbios_gui_local_test"]

    unit --> core
    unit --> model
    mock --> core
    integ --> core
    perf --> core
    cabit --> cabi
    cabit --> core
    guici --> model
    guici --> core
    guiloc --> model
    guiloc --> core
```

`smbios_unit_test`, `smbios_gui_ci_test`, `smbios_gui_local_test` and
the `smbios_c_*` tests that need entries link `libsmbios` as a whole
archive. The two GUI test executables also link `imgui::imgui`; the local
one adds `glfw` and `OpenGL::GL`. `smbios_c_*` tests link `libsmbios`
directly only for some cases (`tests/c_abi/CMakeLists.txt`).
