# release-P3 results: versioning + release automation

Status: DONE 2026-07-30 (P9.3 in plans.md).  The machinery is in place
and verified as far as it can be without pushing a tag; the first real
release exercises the pipeline end to end.

## The scheme

- Semver, pre-1.0 reading (0.MINOR may break, PATCH may not),
  documented in CHANGELOG.md's header.
- The version lives in exactly two files — `package.json` (what
  buck-dist.sh stamps into the tarball/dist-info) and
  `packaging/npm/package.json` (what pins the wrapper's download tag)
  — plus the git tag.  Nothing else carries a version; the release
  pipeline refuses a tag where the three disagree.
- CHANGELOG.md is Keep-a-Changelog-shaped, newest first, with an
  Unreleased section that must be non-empty to cut a release (an empty
  entry means the release story wasn't written).  Seeded with the
  first-release Unreleased content.

## Cutting a release

`./packaging/prepare-release.sh 0.1.0` (local, offline): clean-tree
check, rolls Unreleased into `## [0.1.0] - <date>`, stamps both
package.jsons via `npm version --no-git-tag-version` (which also
updates the lockfile's mirrored version), commits `release: v0.1.0`,
makes the annotated tag.  It deliberately does NOT push — pushing the
tag is the human act that starts the pipeline.

## The pipeline (.github/workflows/release.yml, on v<semver> tags)

1. **version-check** — tag == both package.jsons, CHANGELOG section
   exists.
2. **bootstrap** — ci.yml's jobs were refactored into a reusable
   `bootstrap.yml` (`on: workflow_call`); CI and Release both call it,
   so "a release is a green matrix" is literally the same workflow:
   stage ladder + test-eir, dist tarballs + //:test-dist (which
   includes the installer smoke), and the release-P2 package smokes on
   all three platforms.
3. **publish** — downloads the three tarball artifacts, generates the
   homebrew formula against the hosted asset URL (sha from the real
   tarball), `npm pack`s the wrapper, extracts the tag's CHANGELOG
   section as notes, and creates a **draft** GitHub release carrying
   tarballs + formula + wrapper tgz.  Publishing the draft is the
   go-live act — draft asset URLs aren't public, so the formula and
   the npm postinstall only resolve after that click.  Two
   shell-gated legs, each loudly skipped when unconfigured rather
   than breaking the release: the formula push to
   `toshok/homebrew-echojs` (iff `HOMEBREW_TAP_TOKEN` is set — a git
   push needs a credential), and `npm publish` via **OIDC trusted
   publishing** (docs.npmjs.com/trusted-publishers): no token at all —
   the job has `id-token: write`, npm ≥ 11.5.1 exchanges the GitHub
   OIDC token for short-lived credentials, and provenance
   attestations are generated automatically.  Gated on the
   `NPM_TRUSTED_PUBLISHING` repo *variable* being `true`, flipped
   after the trusted publisher is configured on npmjs.com.  Two
   load-bearing details: the publisher config matches owner/repo +
   the workflow *filename* (`release.yml` — renaming the file breaks
   publishing; the publish step must also live in this workflow, not
   a reusable one, since validation checks the calling workflow), and
   the wrapper's `repository` field must match the repo exactly
   (`git+https://github.com/toshok/echojs.git` + `directory:
   packaging/npm`).  The publish uses the package directory, not the
   tgz, so provenance sees the build context; `publishConfig.access:
   public` is baked into the wrapper's package.json.
4. **smoke-linux / smoke-macos** — the clean-machine proof the plan
   asked for: a bare `ubuntu:24.04` container (both arches) and a
   fresh macos runner that never see the repo install only the
   tarball plus the README's documented prerequisites (apt.llvm.org
   llvm-22 + build-essential + libuv/libunwind dev on linux; `brew
   install llvm` on macos), run `install.sh`, and compile + run a
   program through the installed layout.

## npm name

`echojs` is taken on the registry (an unrelated 0.1.4), so the wrapper
is `@pirouette/echojs` under the @pirouette npm org (bin is still
`ejs`); the release-P2 follow-on is resolved.  CI smoke globs updated
for the scoped pack filename (`pirouette-echojs-*.tgz`).  A relative
`EJS_NPM_TARBALL` resolves against `INIT_CWD` (where `npm install` was
invoked), since postinstall's cwd is the package directory.

## Verified locally

- prepare-release.sh dry-run in a scratch clone: stamps all four
  files, rolls the changelog, tags v0.1.0; a second cut correctly
  refuses on the now-empty Unreleased section.
- All three workflows parse and pass `actionlint` (only intentional
  SC2016 infos remain: single-quoted JS template literals).
- make-formula.sh `--url` mode produces the hosted-URL formula with
  the local tarball's sha256.

## First-release checklist (for whoever pushes the button)

1. `./packaging/prepare-release.sh 0.1.0` && `git push origin HEAD v0.1.0`
2. wait for the Release workflow: green matrix + draft release + smokes
3. publish the draft release (this makes formula/npm URLs real)
4. optional, once: create `toshok/homebrew-echojs` and set
   `HOMEBREW_TAP_TOKEN` — until then the formula rides on the release
   page
5. optional, once, for npm: on npmjs.com, add a trusted publisher to
   `@pirouette/echojs` (org `toshok`, repo `echojs`, workflow
   `release.yml`, allowed action `npm publish`), then set the repo
   variable `NPM_TRUSTED_PUBLISHING=true`.  If npmjs won't accept a
   trusted publisher for a never-published package, do the first
   `npm publish --access public` locally as an @pirouette member,
   then configure it — every later release publishes via OIDC

## Follow-ons

- The publish job re-runs are not idempotent (`gh release create`
  fails if the draft already exists) — delete the draft before
  re-running, or teach the step `gh release view || create`.
- The linux smoke pins apt.llvm.org's llvm-22 spelling; when the
  toolchain major moves, dist-info already carries it — the smoke
  could read EJS_LLVM_MAJOR from the tarball instead of hardcoding.
- P9.4 (getting-started surface) should point the README at the
  released packages instead of the repo build.
