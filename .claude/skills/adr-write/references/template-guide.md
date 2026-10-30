# ADR template guide (section-by-section)

The canonical structure lives in [`docs/adr/template.md`](/docs/adr/template.md).
This guide explains what each section must contain and how the git/issue context
the skill gathers in Step 2 maps onto it. Read `template.md` for the exact
headings; read this for how to fill them.

## Filename & header

- Filename: `docs/adr/NNNN-<kebab-title>.md`, `NNNN` zero-padded, one higher than
  the current maximum in `docs/adr/`.
- H1: `# NNNN - Title` (space-dash-space, matching the template — not `NNNN.`).
- Status starts **Proposed**. Only a human/architect flips it to `Accepted`.
  `Superseded` / `Deprecated` are set later, with a link to the ADR that replaces it.

## Context

*What forces this decision.* This is where the seeded material goes:

- `git log --oneline -10` → the recent commits that surfaced the problem.
- `gh issue list --state open --limit 10` → any open issue the decision resolves;
  link it in **Links → Roadmap story/task**, don't just paraphrase it.
- State the constraints and forces plainly. No solution yet.

## Decision

The change being adopted, in the present tense ("We will …"). One clear choice.
If it is still genuinely open, the ADR is premature — write it once the
`app-architect` has a direction, since this skill only scaffolds.

## Alternatives Considered

A **table** (`| Alternative | Pros | Cons | Reason rejected |`). Include the chosen
option's serious rivals — an ADR with an empty or single-row table reads as
undecided. Every rejected row needs a concrete *Reason rejected*, not just "worse".

## Consequences

Split **Positive** and **Negative** (the template's two sub-headings). Negative is
the honest part: migration cost, new failure modes, lock-in. Call out anything
touching this project's core safety/compliance invariants or security explicitly —
those are the consequences a reviewer will look for first.

## Validation / Rollout

How we confirm the decision works and how we ship it. Name the follow-up work and
who owns it: implementation → `cpp-expert`, tests → `testing-expert`,
doc/runbook updates → `docs-updater`. This is where downstream tasks get routed.

## Links

- **Roadmap story/task:** the `docs/roadmap/...` story or task or GitHub issue that drove this.
- **Supporting specs / diagrams:** anything under `docs/specs/` or `assets/`.
- **Supersedes / Superseded by:** prior ADRs in the same decision lineage.

## Anti-patterns to avoid

- Pasting a divergent copy of the template into the ADR — always mirror
  `docs/adr/template.md` so the corpus stays uniform.
- Marking a fresh ADR `Accepted` — sign-off is a human step.
- An empty Alternatives table or an all-positive Consequences section — both signal
  the decision wasn't really weighed.
