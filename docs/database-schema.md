# Database Schema

The rules for BuCu's SQLite database: tables, versioning, migrations, and how it differs from Buku's. Records of each version change: [`docs/contracts/`](contracts/) (`db-NNN`). When to read this document and how it changes: `CLAUDE.md`.

Normative words (MUST, SHOULD, MAY): [constitution §6](constitution.md#6-normative-language).

## 1. Connection rules

- `PRAGMA foreign_keys = ON` on every connection.
- WAL journal mode where the file system supports it; otherwise the default rollback journal.
- A busy timeout of 5 seconds, so a second process waits briefly instead of failing at once.
- Every repeated statement is prepared once per command; bulk writes run in one transaction.

## 2. Versioning and migrations

- The schema version is `PRAGMA user_version`. Version 0 is an empty file.
- Opening a path with no file creates the database at the newest version. A version 0 file that already has tables is another program's database: BuCu MUST refuse it and change nothing.
- A first open that fails after creating the file (for example, its lock wait runs out) may leave an empty version 0 file, which the next open upgrades to version 1. BuCu never deletes a database file.
- Migrations are forward-only: each one moves version n to n+1, inside a transaction, and never drops user data.
- BuCu MUST refuse to open a database whose version is newer than it knows, and say both the file's schema version and the newest version it supports.
- Each version change has a `db-NNN` record and a migration test from every earlier version.

## 3. Schema version 1

```text
bookmark
  id           INTEGER PRIMARY KEY       -- no AUTOINCREMENT; the reuse rule is #13
  url          TEXT NOT NULL UNIQUE      -- normalized (product §4)
  title        TEXT NOT NULL DEFAULT ''
  description  TEXT NOT NULL DEFAULT ''
  flags        INTEGER NOT NULL DEFAULT 0
  created_at   INTEGER NOT NULL          -- Unix seconds, UTC
  updated_at   INTEGER NOT NULL          -- Unix seconds, UTC

tag
  id    INTEGER PRIMARY KEY
  name  TEXT NOT NULL UNIQUE              -- normalized (product §4)

bookmark_tag                              -- WITHOUT ROWID
  bookmark_id  INTEGER NOT NULL REFERENCES bookmark(id) ON DELETE CASCADE
  tag_id       INTEGER NOT NULL REFERENCES tag(id) ON DELETE CASCADE
  PRIMARY KEY (bookmark_id, tag_id)
  INDEX bookmark_tag_tag_id on (tag_id)
```

Bookmarks carry no UUID; #31 adds one with a migration.

## 4. Differences from Buku

Buku keeps one `bookmarks` table with tags in one comma-delimited text column, and compacts IDs when a bookmark is deleted. BuCu normalizes tags into their own tables, and its ID rule is settled by #13. Importing a Buku database therefore maps its schema rather than copying it (#23).

## 5. Portability and merging

The database is one file that any SQLite 3 tool can read, on any platform. Merging two databases is not supported yet; the limits and the path to it are #31.
