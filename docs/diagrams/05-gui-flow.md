# 5. Process: `smbios_gui` flow

From startup to "Copy as". Sources: `smbios_gui/src/main.cpp`,
`view_model.cpp`, `entry_tree_view.cpp`, `field_text.cpp`, `copy_as.cpp`,
`copy_as_action.cpp`. The view-model layer (`libsmbios_gui_model`) has no
Dear ImGui dependency; the rendering files do. Mermaid, not rendered
locally.

## 5a. Startup and per-frame rendering

```mermaid
sequenceDiagram
    participant M as main
    participant B as SMBios
    participant V as build_entry_tree
    participant R as render_entry_browser
    participant T as field_text
    participant G as ImGui

    M->>B: make_unique SMBios
    M->>V: build_entry_tree(bios)
    loop each DMIHeader
        V->>V: SMBiosEntryFactory create, then get_fields
        Note over V: exception gives decode_failed node, no fields
    end
    V-->>M: vector of EntryNode
    M->>G: glfwInit, window, ImGui context
    loop until window closes or Escape
        M->>G: NewFrame
        M->>R: render_main_window to render_entry_browser
        R->>T: entry_node_label, field_value_text
        T->>T: display_text on every firmware string
        R->>G: TextUnformatted(text, text_end)
        M->>G: Render, RenderDrawData, SwapBuffers
    end
```

Firmware strings reach ImGui only as already sanitized `display_text()`
output passed to `TextUnformatted` (never as a format string, label or
ID).

## 5b. "Copy as" (when the user presses Copy)

```mermaid
sequenceDiagram
    participant U as user
    participant A as render_copy_as_action
    participant C as copy_as
    participant F as SMBiosEntryFactory
    participant O as output formatter registry
    participant G as ImGui

    U->>A: pick format, press Copy
    A->>O: get_output_formatter(name)
    A->>C: create_entry_for_node(bios, index, node)
    C->>F: create(header, version, table_end)
    F-->>C: entry or nullptr
    A->>C: render_entry_as(entry, format)
    C->>O: get_output_formatter(name), render(entry)
    O-->>C: text
    alt rendered over 1 MiB
        C-->>A: nullopt, status TooLarge
    else text contains NUL
        A->>A: status Failed
    else ok
        A->>G: SetClipboardText
        A->>A: status Copied
    end
```

The format list comes from `registered_output_formatter_names()`; the
default is `json`. Any format other than `json` shows a note that the
output may contain raw control characters.
