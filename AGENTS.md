# Uke keyboard development

- The user explicitly authorized this keyboard fork. Do not fork KWin, Plasma,
  Qt or another KDE package. Keep upstream attribution and original providers.
- Native application code is C++20 and Qt/QML. No project Python tools or
  runtime dependencies. Build native code in COPR, never on the tablet.
- Keep the original keyboard installed and preserve the user's input-method,
  display, lock, refresh and session preferences. Normal application operation
  must not enable a panel. On 12 November the user explicitly authorized a
  temporary open-cover simulation and panel wake for testing; restore the
  previous state afterward and keep this out of production eligibility policy.
- Never publish a physical acceptance claim based only on a source/build test.
- Latest user steering, 12 November: static mode must open automatically on
  the tablet when all gates pass and remain visible with its touchpad across
  text-focus changes. USB exclusion still applies. Preserve the explicit
  Normal keyboard action; manual dismissal lasts until the next eligibility
  cycle. This supersedes the earlier manual-entry default.
- Update the external Desktop tracker and docs/acceptance.md after material
  results. Do not log entered text, clipboard contents or private identities.
- Latest user steering, 12 November: automatic normal-mode opening must require
  touch input. Mouse focus and text/context updates must not open the keyboard.
  Keep persistent static mode and explicit manual show actions separate.
