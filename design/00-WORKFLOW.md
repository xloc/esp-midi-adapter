# Agent-Assisted Development Workflow

## Overview

A structured approach to transforming ideas into working software with code agents. The core principle: minimize wasted work by giving agents hard specs and fast feedback loops.

## File Structure

```
project/
├── design/
│   ├── 00-WORKFLOW.md           # This process doc (copy to new projects)
│   ├── 00-build-contract.md     # Project requirements (what to build)
│   ├── 01-first-feature.md      # First vertical slice
│   └── 02-*.md                  # Additional slices
└── src/
    └── ...
```

## The Process

If you want "idea → working software" as fast as possible with today's code agents, the most efficient pattern is:

1. Turn the idea into a narrow executable slice (a tiny product that runs end-to-end)
2. Give the agent a hard spec + a feedback loop
3. Iterate in short cycles where you only do: decide, evaluate, and adjust

Here's a practical workflow that consistently minimizes wasted work.

### 1. Convert the idea into a 1-page "build contract"

Agents are fast at producing code, but they're slow at guessing what you mean. A short contract eliminates most churn.

**A. One-sentence goal**

"Build X that lets a user do Y, producing Z."

**B. 3–7 "non-negotiable" user stories**

"As a user, I can …"

Keep them concrete and testable.

**C. Definition of done**

A checklist that can be verified locally:
- `make test` passes
- Can run `docker compose up` and use the UI
- Imports data file type A
- Exports artifact B
- etc.

**D. Constraints**

Tech stack constraints (or explicitly "choose the simplest").
OS/environment, performance constraints, offline/online, etc.

**E. "Not doing" list**

Prevents the agent from building extras.

This doc is the single highest ROI step because it stops "beautiful wrong code".

### 2. Start with an end-to-end vertical slice, not architecture

Most people (and agents) waste time by overbuilding scaffolding.

Pick the smallest workflow that touches all layers you care about:

> UI → API → storage → one real algorithm → output

Even if everything is ugly/hard-coded.

Example (generic):
- One screen
- One API route
- One data model/table
- One "core function"
- One output format

Once this slice works, you now have:
- A running integration testbed
- Real constraints
- A place to plug in "the real version"

### 3. Make the agent produce tests + code, not just code

The best way to keep agents aligned is to anchor them to executable truth.

Ask for:
- A minimal test plan (unit + integration)
- Golden files / fixtures for known inputs/outputs
- Smoke test script (one command that proves it works)

Then your loop becomes:
1. Agent changes code
2. You run tests
3. You paste failures back
4. Agent fixes

This is dramatically faster than reading diffs.

### 4. Use a "tight loop" execution environment

Your goal is: agent writes code → you run it in seconds → agent sees results.

Common choices:
- Devcontainer / Docker compose so the environment is reproducible.
- One-command bootstrap: `just dev` / `make dev` / `task dev`
- Fast tests:
  - Unit tests run in <10s
  - Integration tests maybe <60s

If tests take 5 minutes, you'll iterate 10× slower.

### 5. Delegate in modules, but integrate daily

Agents do best when tasks have clear boundaries.

**Good task chunking:**
- "Implement data model + migrations + CRUD"
- "Implement parsing for format X with tests"
- "Implement UI for workflow Y using existing API"
- "Add logging + error handling + basic observability"

**Bad chunking:**
- "Build the backend"
- "Design the architecture"
- "Make it scalable"

**Rule:** Merge something runnable every day (or every session). Otherwise you accumulate mismatched pieces.

### 6. Force explicit decisions early (so you don't refactor forever)

For speed, choose defaults unless you truly care:
- One language, one framework, one DB
- Monorepo
- Boring deployment (even if just local)
- Minimal auth (or none if not needed)
- Minimal UI styling

Agents are excellent at building "reasonable defaults" fast—as long as you allow them to decide.

### 7. Use "spec prompts" that agents can execute without guesswork

A good agent request includes:
- **Context:** what exists (repo tree, current state)
- **Goal:** what to add/change
- **Interfaces:** inputs/outputs, endpoints, data schemas
- **Acceptance tests:** what you will run to verify

Template you can reuse:

> "Read the repo. Implement X. Keep changes minimal. Add tests. Update docs. Ensure … command passes. If you need to choose, prefer simplest."

(When you paste logs/errors back, your throughput jumps.)

### 8. Keep a "decision log" to avoid re-litigating choices

One markdown file:
- What you chose (stack, folder layout, conventions)
- Why (one line)
- What you're punting (future work)

This stops both you and the agent from undoing yesterday.

### 9. The fastest overall loop (summary)

**Day 0**
- 1-page build contract
- Repo scaffold + dev environment + CI
- One vertical slice working end-to-end

**Days 1–N**

Each iteration:
1. Pick one user story
2. Ask agent: implement + tests
3. Run tests
4. Paste failures
5. Merge when green
