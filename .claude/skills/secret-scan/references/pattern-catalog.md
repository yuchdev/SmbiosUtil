# Secret-scan pattern catalog

Documents the exact detection engine shared by the `/secret-scan` skill and the
`PreToolUse(Write|Edit|MultiEdit)` hook (`.claude/hooks/secret_scan.py`). Both use
this same table, so the manual sweep and the automatic block agree. When the hook
source changes, update this file (drift here means the catalog lies about what is
actually blocked).

> Note: this file deliberately describes secret *shapes* in prose and uses
> `${VAR}`/placeholder forms in every example — a literal credential here would be
> blocked by the very hook it documents.

## Detected shapes → per-type remediation

| Type                       | Shape (described, not literal)                                                   | Remediation                                                                     |
|----------------------------|----------------------------------------------------------------------------------|---------------------------------------------------------------------------------|
| AWS access key id          | `AKIA` followed by 16 uppercase alphanumerics                                    | Delete; rotate in IAM; use an instance role or `${AWS_ACCESS_KEY_ID}`.          |
| AWS secret access key      | an `aws_secret_access_key` assignment to a 40-char base64 value                  | Rotate the key pair; never store the secret in-repo.                            |
| Anthropic API key          | prefix `sk-ant-` then 20+ chars                                                  | Rotate in the Anthropic console; read from `${ANTHROPIC_API_KEY}`.              |
| OpenAI API key             | prefix `sk-` or `sk-proj-` then 20+ chars                                        | Rotate in the OpenAI dashboard; read from `${OPENAI_API_KEY}`.                  |
| Google API key             | prefix `AIza` then 35 chars                                                      | Rotate in GCP; restrict the key; use env/secret manager.                        |
| GitHub token               | prefixes `ghp_`/`gho_`/`ghu_`/`ghs_`/`ghr_` then 36+ chars                       | Revoke the PAT; use `${GITHUB_TOKEN}` via `.mcp.json` expansion.                |
| Slack token                | prefixes `xoxb-`/`xoxa-`/`xoxp-`/`xoxr-`/`xoxs-` then 10+ chars                  | Revoke in Slack app admin; env var.                                             |
| Private key block          | a `BEGIN … PRIVATE KEY` PEM header                                               | Remove; rotate the key; store in a secrets manager, never in-repo.              |
| Generic assigned secret    | `password`/`passwd`/`secret`/`token`/`api_key` assigned a quoted 8+ char literal (hex memory addresses exempt - see below) | Replace the literal with an env/`${VAR}` reference.                             |
| Postgres URL with password | a `postgres://` DSN carrying inline `user:password@` credentials                 | Move credentials to env; build the DSN as `postgres://user:${PGPASSWORD}@host`. |

## Value-scoped exemption: hex memory addresses

`Generic assigned secret` is keyword-driven, so it also matches a *pointer or offset*
that happens to live in a field called `token`, `secret`, `password`, or `api_key` -
common in debugger, profiler, and crash-analysis code:

    token   = "0x00007fff5fbff8c0"     # 64-bit address, not a credential
    secret  = "0xFFFFF80000000000ULL"  # kernel base with a C integer suffix
    api_key = "0x00401000"             # 32-bit module base

The scanner therefore skips a finding when the **captured value alone** is a hex
memory address: `0x`/`0X`, then **1-16 hex digits**, then an optional `u`/`U`/`l`/`L`
suffix; `_` digit separators are stripped first.

Two properties make this safe, and both matter:

1. **The 16-digit ceiling is the security boundary, not cosmetic.** A 64-bit address
   is at most 16 hex digits, while every hex-encoded credential worth catching is
   longer - AES-128 is 32 digits, AES-256 and Ethereum private keys are 64. So
   `token = "0x<64 hex digits>"` is still blocked.
2. **It is scoped to the matched value, never the line.** A line carrying an address
   *and* a real credential still fails, e.g.
   `token = "0x00007fff5fbff8c0 AKIA................"` is caught as an AWS key.

This is why the exemption lives in the pattern engine rather than in the placeholder
allowlist below: the allowlist matches the whole line, which would let one address
mention wave through everything else on it.

## Placeholder allowlist (why a line is *not* flagged)

A line is skipped if it contains any of these case-insensitive substrings:
`example`, `placeholder`, `your-`, `your_`, `changeme`, `dummy`, `xxxx`, `${`,
`<your`, `redacted`, `fake`, `test`.

This is why `API_KEY=${OPENAI_API_KEY}` and `token="your-token-here"` pass — they
read as obvious non-secrets. Consequence: the correct fix for a real hit is to
convert it into an allowlisted form (a `${VAR}` reference), which also makes the
scanner pass.

## Skipped file types

Binary/large artifacts are not scanned: `.lock`, `.png`, `.jpg`, `.jpeg`, `.gif`,
`.pdf`, `.dmp`, `.db`. Note `.dmp` (crash dumps) are skipped by the scanner — but
they can still contain sensitive data (memory contents, credentials, PII); never
commit them regardless.

## Reporting rules

- Print `file:line: <type>: <short excerpt>` — **never** the full secret value.
- On any hit: instruct the user to (1) remove the literal, (2) **rotate** it if it
  ever reached a remote, (3) replace with an env/`${VAR}` reference (see `.mcp.json`)
  or a secrets manager.
- If the secret was already committed, recommend `git filter-repo` / history rewrite
  — deletion in a new commit does not remove it from history.

## False-positive handling

If a real detection is genuinely not a secret (e.g. a test fixture), prefer making
it *look* like a placeholder (add `example`/`dummy`/`test` context or a `${VAR}`)
rather than widening the allowlist — the allowlist is shared with the blocking hook,
so loosening it weakens the automatic gate for everyone.
