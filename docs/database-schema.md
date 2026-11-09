# Database Schema

The rules for BuCu's SQLite database: tables, versioning, migrations, and how it differs from Buku's. Records of each version change: [`docs/contracts/`](contracts/) (`db-NNN`). When to read this document and how it changes: `CLAUDE.md`.

Normative words (MUST, SHOULD, MAY): [constitution §6](constitution.md#6-normative-language).

## 1. Connection rules (added by #6)

- `PRAGMA foreign_keys = ON` on every connection.
- WAL journal mode where the file system supports it; otherwise the default rollback journal.
- A busy timeout, so a second process waits briefly instead of failing at once.
- Every repeated statement is prepared once per command; bulk writes run in one transaction.

## 2. Versioning and migrations (added by #6)

- The schema version is `PRAGMA user_version`. Version 0 is an empty file.
- Migrations are forward-only: each one moves version n to n+1, inside a transaction, and never drops user data.
- BuCu MUST refuse to open a database whose version is newer than it knows, and say which BuCu version is needed.
- Each version change has a `db-NNN` record and a migration test from every earlier version.

## 3. Schema version 1 (draft; settled by #6)

```text
bookmark
  id           INTEGER PRIMARY KEY
  url          TEXT NOT NULL UNIQUE      -- normalized (product §4)
  title        TEXT NOT NULL DEFAULT ''
  description  TEXT NOT NULL DEFAULT ''
  flags        INTEGER NOT NULL DEFAULT 0
  created_at   INTEGER NOT NULL          -- storage form settled by #6
  updated_at   INTEGER NOT NULL

tag
  id    INTEGER PRIMARY KEY
  name  TEXT NOT NULL UNIQUE              -- normalized (product §4)

bookmark_tag                              -- WITHOUT ROWID
  bookmark_id  INTEGER NOT NULL REFERENCES bookmark(id) ON DELETE CASCADE
  tag_id       INTEGER NOT NULL REFERENCES tag(id) ON DELETE CASCADE
  PRIMARY KEY (bookmark_id, tag_id)
  INDEX on (tag_id)
```

## 4. Differences from Buku

Buku keeps one `bookmarks` table with tags in one comma-delimited text column, and compacts IDs when a bookmark is deleted. BuCu normalizes tags into their own tables, and its ID rule is settled by #13. Importing a Buku database therefore maps its schema rather than copying it (#23).

## 5. Portability and merging

The database is one file that any SQLite 3 tool can read, on any platform. Merging two databases is not supported yet; the limits and the path to it are #31.
