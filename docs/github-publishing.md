<!--
SPDX-License-Identifier: MIT

Copyright (c) 2025 Dylan Hong

This file is released under the MIT License.
For conditions of distribution and use, see the LICENSE file.
-->

GitHub Pages and Release Publishing
===================================

[Main README](../README.md) | [Documentation index](README.md)

Automation
----------
The repository provides two GitHub Actions workflows:

| Workflow | Trigger | Result |
| --- | --- | --- |
| `Documentation` | Push to `main` or manual dispatch | Builds the warning-clean Doxygen HTML tree and deploys it to GitHub Pages. |
| `Release` | Push of a tag matching `v[0-9]*` | Builds native Linux x86_64 and AArch64 packages, source and documentation archives, checksums, attestations, and a GitHub Release. |

The expected Pages URL is:

  https://github.com/KOR-DYLAN/simple-graphics

The release workflow requires the tag to exactly match `v` followed by the
version in `SGL_VERSION`. For example, version `1.0.0.0` must use tag
`v1.0.0.0`. A mismatch stops every packaging job before publication.

One-Time Repository Settings
----------------------------
Complete these settings after the workflow files reach the default branch:

1. Open **Settings > Pages** and set **Build and deployment > Source** to
   **GitHub Actions**.
2. Confirm GitHub Actions are enabled under **Settings > Actions > General**.
   The workflows declare minimum job permissions themselves; no personal
   access token or repository secret is required.
3. Open **Settings > Environments > github-pages** after the first Pages run.
   Add deployment branch restrictions or required reviewers only when that
   approval delay is intentional.
4. Protect `main` and require the `Documentation / Build documentation` check
   if documentation errors should block merges.
5. Consider enabling immutable releases after the first successful release.
   The workflow creates a draft, uploads every asset, and publishes only after
   all uploads succeed, which is compatible with release immutability.

Release Procedure
-----------------
1. Update every field in `SGL_VERSION` and update release-facing documentation.
2. Run the local checks:

       python3 -m unittest discover -s test/release -p 'test_*.py' -v
       python3 script/release/version.py --check-tag v1.0.0.0
       make docs
       make package
       make package-source
       make package-docs

3. Commit and push the release commit to `main`.
4. Create and push an annotated tag for the same commit. A signed tag is
   preferred when a signing key is configured:

       git tag -s v1.0.0.0 -m "SGL 1.0.0.0"
       git push origin main
       git push origin v1.0.0.0

   Use `git tag -a` instead of `git tag -s` when tag signing is unavailable.
5. Monitor the `Release` workflow. It first creates a draft release, attaches
   all artifacts, and then publishes it as the latest release with generated
   release notes.

Release Assets
--------------
A successful release contains:

| Asset | Purpose |
| --- | --- |
| `sgl-<version>-linux-x86_64.{tar.gz,zip}` | Native x86_64 Linux headers, library, pkg-config, and CMake package files. |
| `sgl-<version>-linux-aarch64.{tar.gz,zip}` | Native AArch64 Linux package with the same installed layout. |
| `sgl-<version>-Source.{tar.gz,zip}` | Architecture-independent source package. |
| `sgl-docs-<version>.{tar.gz,zip}` | Self-contained Doxygen user manual and API reference. |
| `sgl-docs-<version>-SHA256SUMS.txt` | Checksums generated with the documentation archives. |
| `SHA256SUMS.txt` | Checksums for every uploaded release asset. |

The publish job also records GitHub build-provenance attestations for the
assets. With GitHub CLI installed, verify a published release or one downloaded
asset using:

    gh release verify v1.0.0.0 --repo KOR-DYLAN/simple-graphics
    gh release verify-asset v1.0.0.0 ./sgl-1.0.0.0-linux-aarch64.tar.gz \
      --repo KOR-DYLAN/simple-graphics

Failure Handling
----------------
- A build failure creates no release because publication depends on every
  architecture and portable-artifact job.
- A failure during the final publish step may leave a draft release. Inspect
  and remove that draft before rerunning the workflow.
- Do not move a tag after a release is published. Correct the issue, increment
  `SGL_VERSION`, and publish a new tag.
- Pages keeps the previous successful deployment when a new documentation
  build fails.
