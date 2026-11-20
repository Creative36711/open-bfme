#!/usr/bin/env bash
# Apply an OpenBFME update as an explicit three-way tree merge without making
# the upstream commit a parent of the downstream branch.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
PIN="$REPO/OPENBFME_UPSTREAM"
REMOTE="${OPENBFME_REMOTE:-upstream}"
BRANCH="${OPENBFME_BRANCH:-main}"
MODE="apply"

usage() {
  cat <<'EOF'
usage: tools/arena/sync_upstream.sh [--check]

Environment:
  OPENBFME_REMOTE  remote name (default: upstream)
  OPENBFME_BRANCH  branch to fetch (default: main)
EOF
}
case "${1:-}" in
  "") ;;
  --check) MODE="check" ;;
  -h|--help) usage; exit 0 ;;
  *) usage >&2; exit 2 ;;
esac

cd "$REPO"
[ -f "$PIN" ] || { echo "missing $PIN" >&2; exit 1; }
old=$(tr -d '[:space:]' < "$PIN")
[[ "$old" =~ ^[0-9a-f]{40}$ ]] || { echo "invalid upstream pin: $old" >&2; exit 1; }
git remote get-url "$REMOTE" >/dev/null 2>&1 || {
  echo "missing remote '$REMOTE'; add Open-BFME/openbfme-godot first" >&2
  exit 1
}

git fetch --no-tags "$REMOTE" "$BRANCH"
new=$(git rev-parse "FETCH_HEAD^{commit}")
printf 'OpenBFME base: %s\nOpenBFME next: %s\n' "$old" "$new"
if [ "$old" = "$new" ]; then
  echo "Already current."
  exit 0
fi

# The explicit old/ours/new merge retains downstream-only files while applying
# upstream additions, edits and removals exactly as a normal three-way merge.
if [ "$MODE" = "check" ]; then
  git diff --stat "$old" "$new"
  exit 0
fi

[ -z "$(git status --porcelain=v1)" ] || {
  echo "working tree must be clean before an upstream sync" >&2
  exit 1
}
[ ! -e .git/MERGE_HEAD ] || { echo "a merge is already in progress" >&2; exit 1; }

git read-tree -m -u "$old" HEAD "$new" || true
if git ls-files -u | grep -q .; then
  cat >&2 <<'EOF'
OpenBFME sync has conflicts. Resolve the staged 1/2/3 entries, then run the LFS
pointer check below before updating OPENBFME_UPSTREAM and committing. To abort,
reset the index/worktree to HEAD.
EOF
  git status --short >&2
  exit 1
fi

# A regular GitHub repository cannot inherit another repository's LFS storage.
# Never commit a new pointer whose payload the destination repository does not
# own; that would make the push fail with GH008.
pointers=$(git grep --cached -Il \
  -e '^version https://git-lfs.github.com/spec/v1$' -- . 2>/dev/null || true)
if [ -n "$pointers" ]; then
  printf 'Refusing sync: the new tree contains Git LFS pointers:\n%s\n' "$pointers" >&2
  printf 'Reset to HEAD and resolve the assets through an approved transport.\n' >&2
  exit 1
fi

printf '%s\n' "$new" > "$PIN"
git add "$PIN"
cat <<EOF
Upstream tree merge is staged and OPENBFME_UPSTREAM now pins $new.
Review and run the update gate, then commit it as a normal downstream commit.
No upstream commit/history will become a parent of this branch.
EOF
git diff --cached --stat
