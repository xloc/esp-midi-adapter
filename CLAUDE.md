# Code Style

Build the simplest thing that works, with these constraints:

## Best Practices

Follow established best practices when they make the code or project simpler. Ignore them when they add complexity without benefit.

## Core Principles

- **No Tailwind styling.** Don't add Tailwind classes when writing components. Structure HTML semantically so the user can apply styles themselves.
- **Single source of truth.** Define data, types, and configuration in one place. Derive everything else from the source. Use Pinia for shared state across components.
- **Direct mapping from input → actions.** Avoid intermediate representations unless they reduce total code and duplication.
- **Avoid encode/decode round-trips.** Don't transform data just to reconstruct it later. Keep information in the form that will be consumed.
- **Use recursion only when it simplifies code.**
- **Don't add structure for structure's sake.** Avoid unnecessary abstractions, boilerplate, and configuration layers. Components and modules are fine when they serve the UI.
- **No premature "robustness".** Skip heavy validation, performance optimizations, and edge-case handling unless asked. Let runtime errors throw naturally.
- **Keep control flow obvious.** Minimal state, minimal branching, minimal helpers. Don't refactor for elegance if it adds indirection.
- **Fewer concepts over "best practices".** Output should be short and simple; prioritize fewer moving parts over convention.
- **If unsure between two implementations**, pick the one with fewer moving parts and less duplication, even if it's less general.
- **If adding a helper, encoding step, validation, or optimization doesn't remove more code than it adds, don't add it.**
- **TypeScript types at declaration site.** Define types at function signatures, API boundaries, and state initialization. Let TypeScript infer derived values.

See [design/00-code-style-examples.md](design/00-code-style-examples.md) for examples.
