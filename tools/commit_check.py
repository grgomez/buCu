#!/usr/bin/env python3
"""Check a commit message subject: usage `commit_check.py <message-file>`.

Accepted: `T<NNN> <summary>` (task), `R<n> <summary>` (review feedback), and the
subjects git makes itself: Merge, Revert, fixup!, squash!.
"""

import re
import sys

_ACCEPTED = re.compile(
    r"^(?:T\d{3} \S|R\d+ \S|Merge |Revert \"|fixup! \S|squash! \S)"
)
_HELP = (
    "Commit subject rejected. Start it with a task ID, `T<NNN> <summary>`, or a "
    "review finding ID, `R<n> <summary>` (constitution principle 10)."
)


def subject_of(message):
    """Return the first line that is not empty and not a comment."""
    for line in message.splitlines():
        if line.strip() and not line.startswith("#"):
            return line
    return ""


def valid_subject(subject):
    return bool(_ACCEPTED.match(subject))


def main(argv):
    if len(argv) != 2:
        sys.stderr.write("usage: commit_check.py <message-file>\n")
        return 2
    with open(argv[1], encoding="utf-8") as handle:
        subject = subject_of(handle.read())
    if valid_subject(subject):
        return 0
    sys.stderr.write(f"{_HELP}\nGot: {subject!r}\n")
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
