# Database 001: Schema version 1

- Kind: Database
- Version: 1 (from 0)
- Status: Accepted
- Date: 2026-11-11
- Issue: #6
- Previous record: none
- ADR: none

## Summary

The first schema: bookmarks, tags, and the links between them, with timestamps in Unix seconds (UTC) and cascading foreign keys. A new or empty file is upgraded from version 0 to version 1.

## Database

### Tables

#### bookmark (new)

| Column | Type | Rules |
| --- | --- | --- |
| id | INTEGER | Primary key, no `AUTOINCREMENT`; the reuse rule is #13 (new) |
| url | TEXT | NOT NULL, UNIQUE (new) |
| title | TEXT | NOT NULL, default empty (new) |
| description | TEXT | NOT NULL, default empty (new) |
| flags | INTEGER | NOT NULL, default 0 (new) |
| created_at | INTEGER | NOT NULL, Unix seconds UTC (new) |
| updated_at | INTEGER | NOT NULL, Unix seconds UTC (new) |

#### tag (new)

| Column | Type | Rules |
| --- | --- | --- |
| id | INTEGER | Primary key (new) |
| name | TEXT | NOT NULL, UNIQUE (new) |

#### bookmark_tag (new)

`WITHOUT ROWID`.

| Column | Type | Rules |
| --- | --- | --- |
| bookmark_id | INTEGER | NOT NULL, references `bookmark(id)` ON DELETE CASCADE, part of the primary key (new) |
| tag_id | INTEGER | NOT NULL, references `tag(id)` ON DELETE CASCADE, part of the primary key (new) |

### Indexes

- `bookmark_tag_tag_id`: `bookmark_tag(tag_id)`, serves tag-to-bookmark lookups and the tag cascade.

### Migration

- Migration from version 0 to 1: creates the three tables and the index in one transaction and sets `user_version` to 1. A version 0 file that already has tables is refused, not migrated. Migrations are forward-only and never drop user data (`docs/database-schema.md` §2).
- Test: `tests/db/migration_test.cpp`: "an empty file is upgraded to version 1", "a new database is at version 1 with the version 1 tables and no rows"; `tests/db/schema_test.cpp`: the cascade, link, and duplicate tests.
- Exact shape: `src/db/migrations.cpp`, step 1.

### Export effect

None: no export format exists yet (#17).
