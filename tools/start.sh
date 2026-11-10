#!/bin/sh
# start.sh <issue>: switch to the issue's branch (creating it from origin/master when none
# exists) and move its board item to In progress.
# Exit 0 ready, 1 failed, 2 usage, 3 branch ready but the board item was not moved.
set -u
repo=grgomez/buCu
usage() { echo "usage: start.sh <issue-number>" >&2; exit 2; }
fail() { echo "start.sh: $1" >&2; exit 1; }

[ $# -eq 1 ] || usage
issue=$1
case $issue in '' | *[!0-9]*) usage ;; esac

[ -z "$(git status --porcelain)" ] || fail "uncommitted changes; commit or stash them first"
git fetch -q origin || fail "git fetch origin failed"

# An existing branch of the issue, local first, then on origin: (feat|fix)/issue-<n>-*.
branch=$(git for-each-ref --format='%(refname:short)' \
    "refs/heads/feat/issue-$issue-*" "refs/heads/fix/issue-$issue-*" | head -n 1)
if [ -z "$branch" ]; then
    branch=$(git for-each-ref --format='%(refname:short)' \
        "refs/remotes/origin/feat/issue-$issue-*" "refs/remotes/origin/fix/issue-$issue-*" |
        head -n 1)
    branch=${branch#origin/}
fi

if [ -n "$branch" ]; then
    git switch -q "$branch" || fail "could not switch to $branch"
else
    info=$(gh issue view "$issue" --repo "$repo" --json title,labels \
        --jq '.title, (.labels[].name)') || fail "could not read issue #$issue"
    title=$(printf '%s\n' "$info" | sed -n 1p)
    names=$(printf '%s\n' "$info" | sed -n '2,$p')
    kind=feat
    if printf '%s\n' "$names" | grep -qx -e bug -e escaped; then kind=fix; fi
    slug=$(printf '%s' "$title" | tr 'A-Z' 'a-z' | sed 's/[^a-z0-9][^a-z0-9]*/-/g; s/^-//; s/-$//')
    if [ ${#slug} -gt 40 ]; then
        head=$(printf '%s' "$slug" | cut -c1-41)
        case $head in
            *-) slug=${head%-} ;;
            *-*) slug=${head%-*} ;;
            *) slug=$(printf '%s' "$slug" | cut -c1-40) ;;
        esac
    fi
    [ -n "$slug" ] || slug=work
    branch=$kind/issue-$issue-$slug
    git switch -q --no-track -c "$branch" origin/master || fail "could not create $branch"
fi

echo "on branch $branch"
if ! "$(dirname "$0")/board.sh" "$issue" "In progress"; then
    echo "start.sh: the branch is ready, but move the board item by hand" >&2
    exit 3
fi
