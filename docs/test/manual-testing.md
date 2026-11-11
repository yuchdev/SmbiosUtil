# Manual Test Plan - `smbios_gui`

Most of this plan is now automated (milestone 0011, Story 04.0). Run the
automated tiers first; the checks that still need a person are listed in
[Automation status](#automation-status) and are what this plan is for. See
[test-subsystem.md](test-subsystem.md) for the suites themselves, and
`docs/roadmap/0011-gui-visualization-tool/status.md` for the recorded
testing-scope calls.

**Already automated, do not re-test by hand:**

- The view model, the display sanitiser (`display_text()`), value formatting
  and the "Copy as..." rendering logic (`libsmbios_gui_model`), covered by
  `tests/unit/gui_*_test.cpp` (`GuiViewModelUnitTests`,
  `GuiFieldTextUnitTests`, `GuiCopyAsUnitTests`), including hostile strings,
  malformed tables, duplicate handles and the 1 MiB copy cap.
- The real ImGui widgets in a headless context with synthetic tables
  (`smbios_gui_ci_test`, `ctest -L gui-ci`, also run by CI on every push).
- With a display and the live table, on a developer machine only
  (`ctest -L gui-local`, never on CI): a hidden-window GLFW + OpenGL3 smoke,
  every "Copy as..." format against the CLI, the OS clipboard round trip,
  GUI/CLI parity, and the launch/exit/no-stray-output check of the test twin.

## Recording results

Report **only discrepancies**, in the short form `expected -> actual`, one per
line. Say nothing for checks that pass. A clean run is one line:
`<platform>: all checks OK`. Never paste real serial numbers, UUIDs or asset
tags into a report - counts and structure types only.

```
M-06 hex toggle: 0x1F -> 31 (no change on click)
M-09 Copy json: equals `smbios --format json` entry -> trailing newline differs
```

## Automation status

Which checks the automated tiers cover. `gui-ci` = `smbios_gui_ci_test`
(headless, synthetic tables, runs on CI); `gui-local` = the local-only cases
`smbios_gui_local_test.<Case>` (real display, live table, developer machine
only). A check is listed as automated only because its cases pass on macOS;
see the caveats below for Linux and Windows.

| Check | Status | By |
|-------|--------|-----|
| M-01 launch and exit | Partly automated: launch, rendering and exit status `0` | `gui-local` `LaunchAutoExitTwin`, `AutoExitTwinSmoke`, `HiddenWindowSmoke`. **Manual:** the Escape key and the title-bar close (steps 2-3) |
| M-02 node list vs CLI | Automated | `gui-local` `LiveParityRegisteredNodesMatchCli`: registered-node count, per-node type order and each entry's JSON bytes equal `smbios --format json` |
| M-03 unregistered types | Automated | `gui-ci` `RegisteredAndUnknownNodesRender`, `NoCopyControlsForEmptyFieldNode`; `gui-local` parity prints the GUI-only count (`GUI-only unregistered nodes: N`) |
| M-04 field table vs CLI | Automated | `gui-ci` `FieldRowsMatchFieldValueText` (rows on screen = the node's fields); `gui-local` `CopyAsBytesMatchCliEveryNodeAndFormat` (every live entry's `json` = the CLI's) |
| M-05 value kinds | Automated (rendering); look and feel manual | `gui-ci` `FieldRowsMatchFieldValueText`, `DecodeFailedNodeShowsMarker`. **Manual:** that a disabled checkbox and a hex dump look right |
| M-06 hex toggle | Automated | `gui-ci` `HexToggleSwitchesNumericRendering` |
| M-07 selection and scrolling | Partly automated | `gui-ci` selection clamp, `NodeClickChangesSelection`, the two clipper cases. **Manual:** mouse wheel/scrollbar feel, flicker, highlight |
| M-08 Copy as dropdown | Automated | `gui-ci` `GuiCopyAsHeadlessTests` |
| M-09 Copy json vs CLI | Automated | `gui-ci` `CopyClickPutsRenderEntryAsBytesOnClipboard` (click path, in-memory clipboard); `gui-local` `CopyAsBytesMatchCliEveryNodeAndFormat` (every node, in memory) and `ClipboardRoundTripMatchesCli` (real OS clipboard, one identifier-free entry) |
| M-10 Copy other formats | Automated | `gui-local` `CopyAsBytesMatchCliEveryNodeAndFormat` (`text`/`xml`/`yaml`, every node) and `ClipboardRoundTripMatchesCli` (one write per format); `gui-ci` `CopyRefusesOversizedOutput` for the 1 MiB refusal |
| M-11 no readable table | Partly automated | `gui-ci` `EmptyNodesShowsNoTableText`. **Manual:** the startup failure path (one stderr line, exit `1`) |
| M-12 root warning | Manual | Never automated: every `gui-local` case skips when run as root |
| M-13 window behaviour | Manual | Resize, minimise/restore, multi-monitor |
| M-14 stray output and files | Automated (output and files) | `gui-local` `LaunchAutoExitTwin`: 0 bytes on stdout+stderr and an empty working directory after 120 frames. "No text inputs" is enforced by review and grep (G-11) |

So a manual pass now needs only: **M-01** steps 2-3 (Escape, title-bar
close), **M-05**'s look, **M-07**'s feel, **M-11**'s startup exit-1 path,
**M-12** and **M-13** - plus anything the automated tiers report.

The `gui-local` cases launch a test twin (`smbios_gui_autoexit`), not the
shipped `smbios_gui`: it is the same sources and link line plus one
compile-time define. A regression specific to the shipped artifact would
only show in a manual M-01.

### Running the local tier

```
ctest --test-dir build/<os>-gui -L gui-local --output-on-failure
```

- **Never as root** (no `sudo`, no elevated shell): every case then prints
  `SKIP: <case>: local GUI tests do not run as root`. On Linux, where the
  sysfs table is root-only, the live-table cases skip with
  `no readable SMBIOS table` - that is expected, not a reason to elevate.
- **Close screen shares and recordings first.** The twin's window shows
  live data, including identifiers, for about 2 s per launching case.
- Output follows the same rule as this plan: nothing but counts. Parity and
  copy-as failures print one `<what>: expected -> actual` line per
  difference (CLI = expected, GUI = actual) with node indices, type codes
  and handles only.

### Clipboard caveats (`ClipboardRoundTripMatchesCli`)

The case overwrites the OS clipboard text 4 times (one write per format) and
then restores it. It prints
`NOTICE: ClipboardRoundTripMatchesCli overwrites then restores the clipboard text (one write per format)`
before the first write and one `NOTICE:` line after restoring.

- **Only an identifier-free entry is written:** BIOS Information (type 0),
  or failing that the first registered entry of a type other than
  1/2/3/4/11/17 with no field name containing `serial`, `uuid` or `asset`.
  Every other node is checked in memory only. If no such entry exists, the
  case skips and M-09/M-10's OS-clipboard leg stays manual on that machine
  (accepted gap; the in-memory case still covers every node).
- **Compare before restore:** if the clipboard no longer holds the test's
  last write (you or a clipboard manager changed it), it is left alone, the
  notice says so and the case fails - re-run it. An empty or non-text saved
  clipboard is restored as an empty string.
- **Non-text clipboard content** (images, files, rich text) cannot be saved
  and is lost.
- **Crash residue:** a crash, a CTest `TIMEOUT` kill or Ctrl-C skips the
  restore, so the BIOS Information text stays on the clipboard and your
  previous text is lost.
- **X11/Wayland ownership:** on Linux the writing process owns the
  clipboard; once the test exits, the restored text disappears unless a
  clipboard manager keeps it.
- **Clipboard history and sync record every write:** Windows Clipboard
  History/Cloud Clipboard, macOS Universal Clipboard and clipboard managers
  (Raycast, Alfred, Maccy, Paste, Klipper, GPaste) keep the 4 written
  entries after the restore. Clear them if that matters to you.

### Platform caveats

The local tier has only been run on macOS. Its Windows code paths (the
`CreateProcessW` + Job Object spawn helper and the temporary directory) have
never been compiled or run, and the Linux run is pending. Treat a first run
on either as untested code and report what it finds.

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

- Before closing any change to `smbios_gui/` (rendering, copy action, `main`):
  `ctest -L gui` on the `<os>-gui` build first, then the manual-only checks
  listed under [Automation status](#automation-status).
- On each platform at least once per release that touches the GUI.
- After changing the view model or `display_text()`, run the unit suite first
  (`ctest --test-dir build/<preset> -R unit`), then `ctest -L gui-local`.
