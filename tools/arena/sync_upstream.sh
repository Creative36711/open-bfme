#!/usr/bin/env bash
# Apply an OpenBFME update as an explicit three-way tree merge without making
# the upstream commit a parent of the downstream branch.
#
# OPENBFME_UPSTREAM is a key=value pin, not a bare SHA: commit= and tag= are one
# exact pair. The tool peels the declared tag on the declared repository before
# it trusts the pin, and an update rewrites only commit=/tag=/pinned_at= while
# keeping the record's shape (repository=, license= and the header stay).
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

The pin file OPENBFME_UPSTREAM must carry repository=, tag= and commit= (40
hex); tag= must peel to commit= on repository= before anything is fetched.
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

# --- read the key=value pin ---------------------------------------------------
pin_field() { # $1=key -> value of the last key= line, empty when absent
  local key=$1 line value=""
  while IFS= read -r line || [ -n "$line" ]; do
    case "$line" in
      "$key="*) value=${line#*=} ;;
    esac
  done < "$PIN"
  printf '%s' "$value"
}
old=$(pin_field commit)
tag=$(pin_field tag)
repository=$(pin_field repository)
[[ "$old" =~ ^[0-9a-f]{40}$ ]] || { echo "invalid upstream pin commit: '$old'" >&2; exit 1; }
[ -n "$tag" ] || { echo "upstream pin has no tag=" >&2; exit 1; }
[ -n "$repository" ] || { echo "upstream pin has no repository=" >&2; exit 1; }

# --- the fetched remote must be the pinned repository -------------------------
git remote get-url "$REMOTE" >/dev/null 2>&1 || {
  echo "missing remote '$REMOTE'; add $repository first" >&2
  exit 1
}
remote_url=$(git remote get-url "$REMOTE")
norm_url() { # the pin's https spelling; optional .git and trailing slash compare equal
  printf '%s' "$1" | sed -E 's#\.git$##; s#/$##'
}
if [ "$(norm_url "$remote_url")" != "$(norm_url "$repository")" ]; then
  echo "remote '$REMOTE' points at '$remote_url', pin declares '$repository'" >&2
  exit 1
fi

# --- fetch the branch and the declared tag ------------------------------------
git fetch --no-tags "$REMOTE" "$BRANCH"
new=$(git rev-parse 'FETCH_HEAD^{commit}')
git fetch --no-tags "$REMOTE" "refs/tags/$tag" || {
  echo "pin integrity: declared tag '$tag' is missing on $remote_url" >&2
  exit 1
}
tag_commit=$(git rev-parse 'FETCH_HEAD^{commit}')
if [ "$tag_commit" != "$old" ]; then
  echo "pin integrity: tag '$tag' peels to $tag_commit, pin commit= is $old" >&2
  exit 1
fi
printf 'OpenBFME base: %s (tag %s)\nOpenBFME next: %s\n' "$old" "$tag" "$new"
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

# The pin always records one exact tag/commit pair, so the new commit must be
# tagged upstream too; an untagged tip is refused instead of silently unpairing
# tag= from commit=.
resolve_tag() { # $1=commit -> tag peeling to it, or empty; stdout only
  local want=$1 sha name stripped annotated="" light=""
  while IFS=$'\t' read -r sha name; do
    [ -n "$name" ] || continue
    case "$name" in
      refs/tags/*^{})
        stripped=${name#refs/tags/}
        stripped=${stripped%'^{}'}
        if [ "$sha" = "$want" ]; then annotated=$stripped; fi
        ;;
      refs/tags/*)
        stripped=${name#refs/tags/}
        if [ "$sha" = "$want" ]; then light=$stripped; fi
        ;;
    esac
  done < <(git ls-remote --tags "$REMOTE")
  if [ -n "$annotated" ]; then printf '%s\n' "$annotated"
  elif [ -n "$light" ]; then printf '%s\n' "$light"
  fi
}
tag_new=$(resolve_tag "$new")
if [ -z "$tag_new" ]; then
  echo "refusing sync: $BRANCH tip $new carries no tag; the pin records one exact tag/commit pair" >&2
  exit 1
fi
git fetch --no-tags "$REMOTE" "refs/tags/$tag_new" || {
  echo "cannot fetch tag '$tag_new' from $REMOTE" >&2
  exit 1
}
[ "$(git rev-parse 'FETCH_HEAD^{commit}')" = "$new" ] || {
  echo "tag '$tag_new' does not peel to $new" >&2
  exit 1
}

# Rewrite only commit=/tag=/pinned_at=; keep the header, repository=, license=
# and every other line byte-identical so the pin keeps its key=value shape.
update_pin() { # $1=commit $2=tag $3=pinned_at (YYYY-MM-DD)
  local commit=$1 tag_value=$2 pinned=$3 line
  local seen_commit=0 seen_tag=0 seen_pinned=0 tmp
  tmp=$(mktemp "$PIN.tmp.XXXXXX")
  while IFS= read -r line || [ -n "$line" ]; do
    case "$line" in
      commit=*) printf 'commit=%s\n' "$commit"; seen_commit=1 ;;
      tag=*) printf 'tag=%s\n' "$tag_value"; seen_tag=1 ;;
      pinned_at=*) printf 'pinned_at=%s\n' "$pinned"; seen_pinned=1 ;;
      *) printf '%s\n' "$line" ;;
    esac
  done < "$PIN" > "$tmp"
  if [ "$seen_commit" -ne 1 ] || [ "$seen_tag" -ne 1 ] || [ "$seen_pinned" -ne 1 ]; then
    rm -f "$tmp"
    echo "pin is missing commit=/tag=/pinned_at=; refusing to rewrite it" >&2
    exit 1
  fi
  cat "$tmp" > "$PIN" # keep the file's inode and mode
  rm -f "$tmp"
}
update_pin "$new" "$tag_new" "$(date -u +%F)"
git add "$PIN"
cat <<EOF
Upstream tree merge is staged and OPENBFME_UPSTREAM now pins $new (tag $tag_new).
Review and run the update gate, then commit it as a normal downstream commit.
No upstream commit/history will become a parent of this branch.
EOF
git diff --cached --stat
