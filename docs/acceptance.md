# Acceptance status

11 November 2026. Work in progress; no physical acceptance is claimed.

| Area | Source | Build/package | Physical |
|---|---|---|---|
| Fork and independent identity | Implemented | COPR pending | Not applicable |
| Static surface and KWin routing | Initial implementation | Pending | Pending |
| Physical size, key groups and settings | Initial implementation | Pending | Pending |
| Touchpad gestures, buttons, typing guard | Initial implementation | Pending | Pending |
| Language/variant selection, clipboard, emoji | Initial implementation | Pending | Pending |
| Fixed refresh profiles and restoration | Initial implementation | Pending | Pending |
| Native 30–60 VRR | Existing kernel interface reviewed; gated | Not qualified | Pending |
| Dynamic laptop mode | Later phase | Not implemented | Not tested |

Tests must separately cover Fedora 44/45/Rawhide aarch64, original-package file
coexistence, install/reinstall/upgrade/removal/rollback, native plugin ABI failure,
lock, closed cover, disabled panel, mirroring, hot unplug, process disappearance,
pointer exclusion, window placement and focus. Check 1×/2×/2.65× and rotations
with a physical ruler; TR Q/F, EN, RTL and CJK; Qt/GTK/browser/terminal/XWayland;
multitouch, gestures, modifier latches and release cleanup; SELinux enforcing,
password/unknown contexts, idle CPU/wakeups/PSS and sustained memory use.

The tablet currently has its cover closed and internal panel disabled. That is
an ineligible static-mode case, not permission to enable the panel or to claim
successful touch/keyboard acceptance. Original kernel and boot selection remain
outside this package's delivery scope.

## COPR 11309616 — source worker failure

Source `20a2d674feed5eca42c6b63659dcd08ac0202eeb`. The source worker checked out
the exact revision and invoked the reviewed Makefile, then failed because its
minimal chroot did not contain `git`. No native compilation or binary RPM was
produced. The recipe now installs its explicit source tools with native DNF5.
The package-definition API also requires its actual `package_name` form field;
that producer was created as 3281444 after HTTP 400 responses, without replaying
an ambiguous operation. GitHub Actions is disabled: the token lacks workflow
scope and no SSH identity is available. Native builds remain COPR-only.

## COPR 11309623 — configuration failure

The SRPM source worker succeeded with verified dependency archives. All three
aarch64 buildroots reached CMake and found the missing explicit `epoxy` build
dependency required by the original KWin export. No binary RPM was produced.
Epoxy, DRM and Vulkan development requirements are now explicit. This corrects
the package recipe without changing a distribution provider.

The next source adds native QML startup validation, dictionary presence gates,
single-active-language caches, exact XKB variant preservation and compositor
notifications for long-touch detection. Healthy-idle clipboard, gamepad and
binary-update polling were removed. Crash recovery blocks on a pidfd and file
notifications. These remain source changes until their COPR tests pass.
