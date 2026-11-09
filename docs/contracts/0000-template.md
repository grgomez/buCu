# <Kind> <NNN>: <Title>

- Kind: Database | Export | Module
- Version: <the new version> (from <the previous version>)
- Status: Proposed | Accepted
- Date: YYYY-MM-DD
- Issue: #<number>
- Previous record: [`<kind>-<NNN>`](<file>) | none
- ADR: [ADR NNNN](../adr/<file>) | none

_How to use this template:_

- One record for each version change of one contract. The rules of each contract, and the index of the three kinds, are in `docs/architecture.md` §5.
- File name: `<kind>-<NNN>-<short-title>.md`, with `<kind>` `db`, `export`, or `module`. For `db`, `<NNN>` is the `PRAGMA user_version` the record introduces; for the others, the next number of that kind, from `001`.
- The Plan part lists the record and Implement writes it with `Status: Proposed`, then sets `Accepted` when every task is checked. After merge, never edit a record, except to fix a broken link. A later change is a new record.
- Write only the sections of the record's kind, and delete the others. Do not copy the full shape from the code: link to it.

## Summary

What changed and why, in one or two sentences.

## Database (kind `db`)

### Tables

#### <Table> (new | changed | removed)

| Column | Type | Rules |
| --- | --- | --- |
| <name> | <SQLite type> | <NOT NULL, UNIQUE, default, foreign key> (new \| changed \| removed) |

### Indexes

- <index>: <columns>, <why>

### Migration

- Migration from version <n> to <n+1>: <what it does to existing rows>. Migrations are forward-only and never drop user data (`docs/database-schema.md` §2).
- Test: <test file and test name>.
- Exact shape: <the migration source for version n+1>.

### Export effect

How the change affects the export formats. A format change writes its own `export` record.

## Export format (kind `export`)

- Format: <html | md | ...>.
- Change: <elements, fields, or rules added, changed, or removed>.
- Older files: how an import reads earlier versions of the format.
- Sample: <the sample file for this version>.
- Test: <test file and test names that read every sample>.

## Module contract (kind `module`)

- **Contract:** <name, for example the bookmark repository>.
- **Owner:** the module that provides it; header in `include/bucu/<module>/`.
- **Callers:** the modules that use it.
- **Inputs:** names, types, and allowed values.
- **Outputs:** shape and types, including error results.
- **Errors:** which error types, and where they are handled.
- **Tests:** the test file and test names that lock the contract.
- **Change from the previous record:** <what changed>, or "first version".
