# Repository guidance

- `Upstream/` is immutable imported reference material. Do not edit it during normal integration or development work.
- Import a new upstream delivery in a dedicated commit containing only the upstream replacement and import metadata.
- Port behavioural changes into maintained code outside `Upstream/`; do not maintain imported files directly.
- Separate refactoring from behavioural changes where practical.
- Add regression or contract tests for emulator changes where feasible.
- Never commit generated build artefacts or machine-local files.
