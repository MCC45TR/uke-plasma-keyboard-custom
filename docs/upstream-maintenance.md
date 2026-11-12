# Original source and Uke patches

The Uke application remains the separately named `uke-plasma-keyboard-custom`
RPM. Its source starts from KDE Plasma Keyboard6.8.3, rather than using a
complete custom-fork snapshot as Source0. The original Fedora Plasma Keyboard,
KWin and Qt packages remain installed. This source transition was explicitly
requested by the user; candidate45 is not yet native/package/device qualified.

The immutable [manifest](../packaging/upstream.json) records upstream version,
revision, release-archive digest, patch order/digests and every reviewed
production file. The6.8.3 archive matches all478 tracked files at
[`ce74711d`](https://github.com/KDE/plasma-keyboard/tree/ce74711d51a7d8a83399cf39cec189e0c9161d38).
Original SPDX headers and licenses remain. Custom interface/tool additions
retain their mops1k attribution and [documentation](../README.custom-tools.md).

| Patch | Scope |
| --- | --- |
| 0001 | Build settings, independent application identity and settings UI |
| 0002 | Existing keyboard interface, layouts and input policy |
| 0003 | Laptop surface, physical sizing and same-RPM native KWin integration |
| 0004 | Optional tools, themes, translations, data and source maintenance |
| 0005 | Existing and new native regression coverage |

Five unchanged dictionary/model data files are stored once in the source asset
archive, with original Git blob IDs and SHA256 digests. They are not duplicated
inside binary patches. Their notices/licenses are retained, and demand-loading
policy is unchanged. Integration documentation and the package recipe are a
separate source archive. COPR's source worker checks all digests and verifies
that original source plus the series reproduces the reviewed production files.
The RPM then applies the same series with Git before its normal native build.

For a reviewed source edit, refresh and check the series using the exact
official archive recorded in the manifest:

```sh
node tools/patch-series.mjs --refresh /path/to/plasma-keyboard-6.8.3.tar.xz
node tools/patch-series.mjs --verify-base /path/to/plasma-keyboard-6.8.3.tar.xz
bash tests/source.sh
```

For an upstream update, first review the official release changes and active
fixes, verify the new revision/archive, and update the manifest and recipe pin.
Apply the existing series to that clean source and resolve conflicts explicitly.
Bring the reviewed upstream changes into the expanded source, then refresh the
series and verify the round trip. Do not regenerate patches against a new
baseline merely to hide upstream changes or bypass conflicts. The dependency
floors follow the selected upstream release; private Qt/KWin ABI checks remain.

Use one uniquely versioned COPR candidate on Fedora44/45/Rawhide aarch64.
Retain every required native test, verify the actual source worker and published
recipe, then review signatures, dependencies, file coexistence and scriptlets.
Deliver through DNF while preserving preferences. Actual boot/session, pointer,
touch, lock, display and refresh results remain separate acceptance gates.
Compatible fixes can be proposed to KDE; removing the Uke application requires
equivalent original-package features and qualified preference/upgrade/rollback
parity. Updating a source pin does not establish that parity.
