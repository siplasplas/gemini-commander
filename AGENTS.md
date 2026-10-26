# Repository guidance for coding assistants

## Language policy

- Write project documentation in English.
- Write code comments in English.
- Write texts, messages, helps in code in English.
- Write Git commit subjects and bodies in English.
- Communicate with the user in their preferred natural language, including progress updates, questions, and final responses.

Determine the conversation language in this order:

1. Follow an explicit language request from the user.
2. When continuing or resuming a session, preserve the language established in that conversation, including language preferences recorded in its continuation summary. English repository files or an English summary do not by themselves change the conversation language.
3. For a new conversation, infer the language from the user's opening words and messages. Do not treat quoted text, code, or technical terms as a language switch.
4. If the current conversation provides no clear signal, use a known language preference from other sessions when that context is available. Do not assume access to unavailable session history.
5. If no preference can be inferred, ask briefly which language the user prefers.

The conversation language does not change the English-language requirements for tracked documentation, code comments, or Git commit messages.

Exceptions to English documentation:

- Handoff notes (`HANDOFF*.md`) and any other notes or documents not kept in Git may be in any language.
- A tracked document may be in another language when its file name carries the language code before `.md` (for example `spec.pl.md`) and the English version exists next to it as `spec.md` or `spec.en.md`. Keep both versions in step when either changes.

## Git

- AI assistants may create local Git commits, but must never run `git push` or use another tool to push commits, branches or tags to a remote repository. A human performs all pushes.
- Handoff notes (`HANDOFF*.md`) are working notes between sessions: never commit them and never delete them; leave them untracked, and the user removes them when no longer needed.
- Hand a turn's work over to the user, and commit it, only when the project builds (`cmake --build build` with no errors, also for the test targets when they are configured). If something out of the ordinary keeps the project from building, such as a missing system package or a broken toolchain, do not present the work as done: tell the user what fails and why.
- The repository is public. Before every commit, check the staged changes (`git diff --cached`) for anything private: API keys, tokens or passwords, personal paths such as home directories, e-mail addresses, private notes, logs, settings files, or the content of the user's conversations. Leave such things out or replace them with neutral examples, and tell the user what was found.

## Size of a turn

- Split a larger task into several smaller turns. Each turn takes only as much work as makes one coherent commit, then hands it over to the user; the next part follows in the next turn.

## Tests

- When a change would benefit from tests, propose them briefly (what they would cover and roughly how) and add them once the user agrees; do not add tests without that agreement.
- Fast tests may be committed and run with `ctest --test-dir build --output-on-failure`: they take seconds and need no network, accounts or API keys.
- Long-running tests (network, accounts, or many seconds each) are not part of the committed suite. Run them at most once, when a feature is handed over, not after every change.
- While working, run only the one or two tests that cover the change, and say which tests were not run.
- Some tools and especially tests can take long or hang. Run anything that might hang with a moderate time limit (for example `timeout 120 ctest …`, or the tool's own timeout option), not with a long or no limit; when the limit is hit, report it and look for the cause instead of waiting longer.

## Code style

- Match the surrounding code style. User-visible behavior changes go into `README.md`.
