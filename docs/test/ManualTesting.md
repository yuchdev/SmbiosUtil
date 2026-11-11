# Manual Test Plan - `smbios_gui`

The automated suites cannot drive a window, so everything below is checked
by a person at a real display. It is the manual half of milestone 0011's
testing scope (see [test-subsystem.md](test-subsystem.md) for what *is*
automated, and `docs/roadmap/0011-gui-visualization-tool/status.md` for the
recorded testing-scope call).

**What is already automated** (do not re-test by hand): the view model, the
display sanitiser (`display_text()`), value formatting, and the "Copy as..."
rendering logic live in the ImGui-free `libsmbios_gui_model` library and are
covered by `tests/unit/gui_*_test.cpp` (`GuiViewModelUnitTests`,
`GuiFieldTextUnitTests`, `GuiCopyAsUnitTests`), including hostile strings,
malformed tables, duplicate handles and the 1 MiB copy cap. This plan covers
only what needs eyes, a window and a clipboard.

## Recording results

Report **only discrepancies**, in the short form `expected -> actual`, one per
line. Say nothing for checks that pass. A clean run is one line:
`<platform>: all checks OK`. Never paste real serial numbers, UUIDs or asset
tags into a report - counts and structure types only.

```
M-06 hex toggle: 0x1F -> 31 (no change on click)
M-09 Copy json: equals `smbios --format json` entry -> trailing newline differs
```

## Prerequisites

- A build of the GUI (it is opt-in; see the root `CLAUDE.md`, "Building the
  GUI"):
  ```
  cmake --preset <os>-gui          # windows-gui | linux-gui | macos-gui
  cmake --build --preset <os>-gui
  ```
  The binary is `build/<os>-gui/smbios_gui/bin/smbios_gui` (under a `Debug/`
  subfolder with the Visual Studio generator on Windows). Linux needs the
  GLFW system packages `libxinerama-dev libxcursor-dev xorg-dev
  libglu1-mesa-dev pkg-config`.
- The CLI from the same build, for comparison:
  `build/<os>-gui/smbios/bin/smbios`.
- A desktop session with OpenGL 3.x (3.2 core on macOS). The GUI reads the
  live machine only; there is no file or dump loading.
- **Privileges:** macOS and Windows need none. On Linux the sysfs table is
  typically root-only; an unprivileged run falls through the source chain and
  may show the "no table" view (see M-11). **Do not run the GUI with `sudo`
  as a workaround** - the GUI warns against it (M-12); use the CLI under
  `sudo` or fix table permissions instead.

## Platform matrix

Fill in one column per machine tested. Mark `-` for not run.

| Check | macOS | Linux | Windows |
|-------|-------|-------|---------|
| M-01 launch and exit |  |  |  |
| M-02 node list vs CLI |  |  |  |
| M-03 unregistered types |  |  |  |
| M-04 field table vs CLI |  |  |  |
| M-05 value kinds |  |  |  |
| M-06 hex toggle |  |  |  |
| M-07 selection and scrolling |  |  |  |
| M-08 Copy as dropdown |  |  |  |
| M-09 Copy json vs CLI |  |  |  |
| M-10 Copy other formats |  |  |  |
| M-11 no readable table |  |  |  |
| M-12 root warning |  |  |  |
| M-13 window behaviour |  |  |  |
| M-14 stderr and files |  |  |  |

## Checks

### M-01 Launch and clean exit

1. Run `smbios_gui`. One window titled for the app opens with a left list and
   a right pane.
2. Press **Escape**. The window closes and the process exits with status `0`
   (`echo $?` / `$LASTEXITCODE`).
3. Run it again and close the window with the title-bar control. Status `0`.

Expected: no crash, no console output (see M-14).

### M-02 Node list matches the CLI

Registered structure types must match `smbios --format text` in **count and
order** (the milestone's parity gate, as ruled 2026-11-11):

1. `smbios --format text | grep -c 'Header type'` gives N.
2. In the GUI, count list entries whose label is **not** `Unknown (0xNN)`.
   Expected: N, in the same order as the CLI's `Header type` lines.
3. Each entry reads `<type name> (handle 0xHHHH)`; the handles are the CLI's
   handles.

Reference: on the maintainer's Mac this was 26 = 26 (the GUI shows 32 nodes;
see M-03).

### M-03 Unregistered types are shown, not hidden

Structure types with no registered entry class (typically OEM types 0x80+)
appear as `Unknown (0xNN) (handle 0x...)` nodes. The CLI skips them, so the
GUI may legitimately show **more** nodes than `smbios` - that is not a
discrepancy. Selecting one shows an empty field table, no crash.

### M-04 Field table matches the CLI

For at least three structures (suggest BIOS Information, System Information,
one Memory Device): select it and compare the right pane's `Field` / `Value`
rows with `smbios --format json` for the same handle. Names, values and order
must match.

### M-05 Every value kind renders

Across the structures you inspect, confirm each kind appears and renders
sensibly:

- text (string) fields;
- number fields;
- boolean fields - shown as **disabled** checkboxes (clicking does nothing);
- byte-array fields (e.g. a UUID) - shown as space-separated hex pairs, cut
  at 1024 bytes with a `... [N more bytes]` note if longer.

A structure that cannot be decoded shows an orange `[decode failed]` marker in
the list and a fixed explanation instead of a table. It should be rare;
report any occurrence on real hardware.

### M-06 Hex / decimal toggle

In a structure with numeric fields, tick **Hex**: numbers switch to `0x...`.
Untick: back to decimal. Switch to another structure and back - the toggle
keeps working.

### M-07 Selection and scrolling

Click several entries; the right pane follows the selection. Scroll a long
list (a machine with many structures) and a long field table with the mouse
wheel and scrollbar; no flicker, no blank rows, no wrong row highlighted.

### M-08 Copy as dropdown

With a structure that has fields selected, find the **Copy as** dropdown and
**Copy** button.

- The dropdown lists exactly the formatters registered in the GUI binary,
  sorted: `json`, `text`, `xml`, `yaml` (there is **no** `csv` - that
  formatter is compiled into the `smbios` CLI only).
- The default selection is `json`.
- Selecting `text`, `xml` or `yaml` shows the note "Output may contain raw
  control characters - paste into terminals with care."; `json` does not.
- A structure with no fields (an `Unknown` or `[decode failed]` node) shows
  no Copy controls.

### M-09 Copy `json` equals the CLI

1. Select a structure, leave `json`, click **Copy**. The status line shows
   "Copied to clipboard."
2. Paste into a text editor.
3. Run `smbios --format json`, locate the same structure (same type/handle).
   The pasted text must equal that structure's JSON object byte for byte
   (the CLI adds array framing around entries; the single copy does not).

### M-10 Copy other formats

Repeat M-09 for `text`, `xml` and `yaml`, comparing with `smbios --format
<name>`. Expected: identical per-entry text. (The status line may also read
"Entry too large to copy (over 1 MiB)." for an unusually large structure;
report it with the structure type.)

### M-11 No readable table

Provoke a failed read (Linux, unprivileged, sysfs table root-only and no other
source available; or any host where no source succeeds). Expected: the window
still opens and shows "No SMBIOS table could be read (this source may need
elevated privileges)." with no list and no crash. If the table cannot even be
read at startup the process prints one fixed line to stderr and exits `1`.

### M-12 Root warning (Linux/macOS)

Running as root (e.g. a throwaway VM, **not** as a convenience for M-11)
shows a banner at the top: "Warning: smbios_gui is running as root. Running a
desktop GUI with root privileges is not recommended." It must not suggest
using `sudo`. Not applicable on Windows.

### M-13 Window behaviour

Resize larger and smaller, maximise, **minimise and restore**, move between
monitors if available. Expected: layout follows, no crash, no busy spinning
while minimised (CPU near idle), closing from the minimised state still works.

### M-14 No stray output or files

- While running and on exit, nothing is printed to stdout/stderr on a normal
  run (only fixed one-line messages on startup failure, M-11).
- No `imgui.ini` (or any other file) appears in the working directory.
- The window cannot edit anything: there are no text inputs, and the only
  controls are the Hex toggle, the format dropdown and Copy.

## Hostile-data note

The display sanitiser (control/bidi characters, invalid UTF-8, oversized
strings, `%`-sequences and `##` in firmware text) is verified by unit tests
rather than by eye, because the GUI has no way to load a crafted table. If you
have a VM that exposes a deliberately odd SMBIOS table, you may additionally
confirm that its strings display as plain text (control characters shown as
`U+FFFD`, long strings cut with `[N bytes truncated]`) and that nothing
beyond the text changes in the window.

## When to run

- Before closing any change to `smbios_gui/` (rendering, copy action, `main`).
- On each platform at least once per release that touches the GUI.
- After changing the view model or `display_text()`, run the unit suite first
  (`ctest --test-dir build/<preset> -R unit`), then M-02 to M-05.
