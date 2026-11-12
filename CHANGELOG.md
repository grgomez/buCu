# Changelog

All notable changes to BuCu are listed here, newest first.

## Unreleased

- Build skeleton: CMake presets, vendored SQLite, Catch2, and CLI11, and the strict warning set.
- `bucu --version` and `bucu --help` (also `-h`), with exit codes `0`, `1`, and `2` as in the product rules.
- Repository converted from the Buku fork into BuCu: the Python implementation is removed, and `NOTICE` and `README.md` state the origin and licence.

## Origin

BuCu is a C++ port of [Buku](https://github.com/jarun/buku) by Arun Prakash Jana. BuCu's history diverges from Buku at upstream commit `c57a1ab`; the earlier commits are Buku's. Buku's release history is in its [changelog](https://github.com/jarun/buku/blob/master/CHANGELOG).
