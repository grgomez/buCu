#!/bin/sh
# board.sh <issue> <status>: move the issue's item on project 4 to a Status option.
# Exit 0 moved, 1 gh failed or the item is not on the board, 2 usage or unknown status.
set -u
owner=grgomez
repo=grgomez/buCu
project=4
usage() { echo "usage: board.sh <issue-number> <status>" >&2; exit 2; }
fail() { echo "board.sh: $1" >&2; exit 1; }

[ $# -eq 2 ] || usage
issue=$1
status=$2
case $issue in '' | *[!0-9]*) usage ;; esac
case $status in '' | *[!A-Za-z' ']*) echo "board.sh: unknown status '$status'" >&2; exit 2 ;; esac

project_id=$(gh project view "$project" --owner "$owner" --format json --jq .id) ||
    fail "could not read project $project"
field_id=$(gh project field-list "$project" --owner "$owner" --format json \
    --jq '.fields[] | select(.name == "Status") | .id') ||
    fail "could not read the fields of project $project"
option_id=$(gh project field-list "$project" --owner "$owner" --format json \
    --jq ".fields[] | select(.name == \"Status\") | .options[] | select(.name == \"$status\") | .id") ||
    fail "could not read the Status options of project $project"
[ -n "$field_id" ] || fail "project $project has no Status field"
if [ -z "$option_id" ]; then
    echo "board.sh: unknown status '$status'; use a Status option of project $project" >&2
    exit 2
fi
item_id=$(gh project item-list "$project" --owner "$owner" --format json --limit 1000 \
    --jq ".items[] | select(.content.number == $issue and .content.repository == \"$repo\") | .id") ||
    fail "could not list the items of project $project"
[ -n "$item_id" ] || fail "issue #$issue was not found on project $project; move it by hand"
case $item_id in
    *'
'*) fail "more than one item on project $project is issue #$issue; move it by hand" ;;
esac

gh project item-edit --project-id "$project_id" --id "$item_id" --field-id "$field_id" \
    --single-select-option-id "$option_id" >/dev/null || fail "could not move issue #$issue"
echo "issue #$issue moved to $status"
