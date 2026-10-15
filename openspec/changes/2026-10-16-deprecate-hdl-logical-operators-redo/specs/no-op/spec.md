# No-op capability (placeholder for deprecate-hdl-logical-operators)

This is a placeholder delta for the `deprecate-hdl-logical-operators` change,
which marks 8 `&&` / `||` overloads `[[deprecated]]` and migrates internal
call sites to `&` / `|`. It does not introduce any capability-level
behaviour change. Per the `fix-ns-pollution` precedent, the archive step
leaves `openspec/specs/` unchanged.

## ADDED Requirements

### Requirement: No-op placeholder for archive

The change MUST NOT introduce any new capability-level behaviour.

#### Scenario: archive leaves openspec/specs/ untouched

- **WHEN** `openspec archive deprecate-hdl-logical-operators` runs
- **THEN** the tool reports "no spec changes"
- **AND** `openspec/specs/` is unchanged
