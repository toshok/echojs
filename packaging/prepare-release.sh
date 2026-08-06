#!/bin/sh
# Cut a release locally (release-P3): roll the CHANGELOG, stamp the
# version everywhere it lives, commit, and tag.  Nothing is pushed —
# pushing the tag is the action that runs the release pipeline
# (.github/workflows/release.yml), so that stays a human decision:
#
#     ./packaging/prepare-release.sh 0.2.0
#     git push origin HEAD "v0.2.0"
#
# The version lives in exactly two files — package.json (the dist
# tarball's source of truth, read by buck-dist.sh) and
# packaging/npm/package.json (the wrapper, whose version pins the
# release tag its postinstall downloads from) — plus the tag itself;
# release.yml's version-check job refuses a tag where they disagree.
set -eu

VERSION="${1:-}"
case "$VERSION" in
    *[!0-9.]*|"") echo "usage: $0 <major.minor.patch>" >&2; exit 1 ;;
esac
echo "$VERSION" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$' || {
    echo "error: '$VERSION' is not a major.minor.patch version" >&2
    exit 1
}

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

test -z "$(git status --porcelain)" || {
    echo "error: working tree not clean" >&2
    exit 1
}
! git rev-parse -q --verify "refs/tags/v$VERSION" > /dev/null || {
    echo "error: tag v$VERSION already exists" >&2
    exit 1
}

# the Unreleased section must exist and have content — an empty
# changelog entry means the release story hasn't been written
grep -q '^## \[Unreleased\]' CHANGELOG.md || {
    echo "error: CHANGELOG.md has no '## [Unreleased]' section" >&2
    exit 1
}
BODY="$(awk '/^## \[Unreleased\]/{f=1; next} /^## /{f=0} f' CHANGELOG.md | grep -cv '^[[:space:]]*$' || true)"
[ "$BODY" -gt 0 ] || {
    echo "error: the Unreleased section of CHANGELOG.md is empty — write the release notes first" >&2
    exit 1
}

TODAY="$(date +%Y-%m-%d)"
awk -v v="$VERSION" -v d="$TODAY" '
    /^## \[Unreleased\]$/ { print; print ""; print "## [" v "] - " d; next }
    { print }
' CHANGELOG.md > CHANGELOG.md.new
mv CHANGELOG.md.new CHANGELOG.md

# npm stamps package.json (and the lockfile's mirrored version) in
# place; --allow-same-version because the tree may already carry the
# to-be-released version (it has since 0.2.0 was pre-stamped)
npm version --no-git-tag-version --allow-same-version "$VERSION" > /dev/null
(cd packaging/npm && npm version --no-git-tag-version --allow-same-version "$VERSION" > /dev/null)

git add CHANGELOG.md package.json package-lock.json packaging/npm/package.json
git commit -q -m "release: v$VERSION"
git tag -a "v$VERSION" -m "echojs $VERSION"

echo "prepared v$VERSION:"
git --no-pager log --oneline -1
echo
echo "next:"
echo "    git push origin HEAD \"v$VERSION\"    # runs the release pipeline"
echo "the pipeline builds, tests, and drafts the GitHub release with all"
echo "assets attached; publishing the draft is the go-live click, after"
echo "which CI pushes the tap formula and npm-publishes the wrapper"
echo "(docs/release-p3-results.md has the details)"
