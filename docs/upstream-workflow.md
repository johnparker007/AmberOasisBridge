# Upstream import and integration workflow

Upstream imports and maintained-code changes are deliberately separated so imported evidence remains reviewable.

## Stage 1: raw import

1. Replace or update only files beneath `Upstream/`.
2. Remove only clearly generated or machine-local output; document conservative retention decisions.
3. Update the snapshot metadata in `Upstream/README.md` without guessing unknown facts.
4. Do not make modern-source changes.
5. Commit the import separately, containing only the upstream replacement and import metadata.

## Stage 2: assessment

1. Compare the new upstream import with the previous import commit.
2. Categorise every relevant change as emulator behaviour, build configuration, generated output, formatting-only, API change, or uncertain.
3. Identify corresponding maintained code outside `Upstream/` using `docs/upstream-map.md`.
4. Produce a reviewed integration plan before changing maintained code.

## Stage 3: integration

1. Port selected behavioural changes into maintained code outside `Upstream/`; never modify the reference snapshot.
2. Add or update regression and contract tests where feasible.
3. Add a dated report beneath `docs/upstream-integrations/` recording integrated, omitted, already-present, and unresolved changes.

## Integration report template

Copy this template to `docs/upstream-integrations/YYYY-MM-DD-<platform>-<description>.md`.

```markdown
# <Platform> integration report — YYYY-MM-DD

## Compared snapshots
- Previous import commit: <commit>
- New import commit: <commit>

## Scope and plan
<summary and links to affected maintained locations>

## Change disposition
| Upstream change | Category | Disposition | Maintained files/tests | Rationale |
| --- | --- | --- | --- | --- |
| <change> | <behaviour/build/generated/format/API/uncertain> | <integrated/omitted/already-present/unresolved> | <paths or TBD> | <reason> |

## Verification
- <commands and results>

## Unresolved items
- <item, owner, and proposed next action>
```

## CI safeguard

After checking out the comparison baseline, CI can run:

```text
python tools/check_upstream_unchanged.py <git-reference>
```

The command exits non-zero and lists paths if the working snapshot differs from the supplied reference. CI policy should choose the reference (for example, the target branch commit); no provider-specific configuration is assumed here.
