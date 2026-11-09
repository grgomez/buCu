# ADR 0005: libcurl for the optional network fetch

- Status: Accepted
- Date: 2026-11-09
- Issue: #1

## Context

The MVP fetches a page's title and description when a bookmark is added. That needs HTTP with TLS, redirects, timeouts, and a response size limit on every platform, and it must never block core operations (constitution principle 8).

## Considered options

- **libcurl (chosen):** portable, mature, supports every limit needed; system library on Linux and macOS, and a source on Windows to settle.
- **Native HTTP per platform (WinHTTP, NSURLSession, libcurl on Linux):** no extra dependency on two platforms, but three implementations to maintain.
- **No network in the MVP:** smallest surface, but the owner wants the fetch in the MVP.

## Decision

We will implement the `net` interface (architecture §4) with libcurl, behind the CMake option `BUCU_WITH_NET`. Without it, BuCu builds a stub that always reports "offline". How libcurl reaches the Windows MSVC build is settled by a trial in #16's plan.

## Consequences

- libcurl is the largest dependency; it is optional at build time, and every test runs against a fake.
- Release packaging must carry libcurl's licence when it is linked (#22).
