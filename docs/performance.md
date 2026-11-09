# Performance

How BuCu's performance is measured and which budgets it aims for. Results live in [`benchmarks/README.md`](../benchmarks/README.md); rerun them with `scripts/bench.sh` (added by #7). When to read this document and how it changes: `CLAUDE.md`.

## 1. Rules

- Measure before optimizing. A change made for speed or memory names the benchmark that justified it, with before and after numbers.
- Profile before adding caches, custom allocators, or complex data structures (architecture §8).
- Network operations are never part of a measured core operation (constitution principle 8).

## 2. What is measured (harness by #7, full suite by #20)

| Measure | How |
| --- | --- |
| Cold-start time | `bucu --version`, and `bucu list` on an empty database |
| Add | One `bucu add` on a database of each size |
| List | 1k, 10k, and 100k bookmarks, output to a file |
| Search latency | Keyword, tag, substring, and regex searches on each dataset |
| Memory | Peak RSS of each command above; steady state of a long import |
| Import and export throughput | HTML and Markdown, bookmarks per second |
| Binary size | Release build, stripped |

Datasets are generated, representative, and reproducible from a seed (#7). Each run records the machine, compiler, build type, and BuCu version next to its numbers. The full suite also runs Python Buku on the same data for comparison (#20).

## 3. Budgets

Set by #20 from the first full measurement. Until then, BuCu's goal is lower startup latency and memory use than Python Buku on the same machine and data.
