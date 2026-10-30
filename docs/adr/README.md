# Architecture Decision Records

ADRs for libbios use the [MADR](https://adr.github.io/madr/) (Markdown Any Decision Records) template.
Each record lives in this directory as `000N-slug.md`. Mermaid diagrams referenced by ADRs
are in `assets/`.

## Inventory

| ADR        | Title                          | Status | Date |
|------------|--------------------------------|--------|------|
| _none yet_ | Add a row per ADR as they land |        |      |

## Template

Use `template.md` when creating a new ADR:

```bash
cp docs/adr/template.md docs/adr/0001-short-title.md
```

Replace the template placeholders with the record's number, title, date, and status.

## Naming conventions

- Filename: `000N-kebab-slug.md` - sequential, zero-padded to four digits.
- Status values: `Proposed` | `Accepted` | `Implemented` | `Superseded` | `Deprecated`.
- Superseded ADRs keep their file; add a `Superseded by: [000N](...)` line to their header.
