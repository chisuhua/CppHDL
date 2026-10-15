# No-op capability (placeholder for cleanup-orphan-code)

This is a placeholder delta for the `cleanup-orphan-code` change, which
performs source-file and documentation cleanup without introducing any
capability-level behaviour change. Per the `fix-ns-pollution` precedent,
the archive step leaves `openspec/specs/` unchanged.

## ADDED Requirements

### Requirement: No-op placeholder for archive

The change MUST NOT introduce any new capability-level behaviour.

#### Scenario: archive leaves openspec/specs/ untouched

- **WHEN** `openspec archive cleanup-orphan-code` runs
- **THEN** the tool reports "no spec changes"
- **AND** `openspec/specs/` is unchanged
