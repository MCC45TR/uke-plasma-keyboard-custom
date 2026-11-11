# Uke keyboard development

- The user explicitly authorized this keyboard fork. Do not fork KWin, Plasma,
  Qt or another KDE package. Keep upstream attribution and original providers.
- Native application code is C++20 and Qt/QML. No project Python tools or
  runtime dependencies. Build native code in COPR, never on the tablet.
- Keep the original keyboard installed and preserve the user's input-method,
  display, lock, refresh and session preferences. Do not enable a panel.
- Never publish a physical acceptance claim based only on a source/build test.
- Update the external Desktop tracker and docs/acceptance.md after material
  results. Do not log entered text, clipboard contents or private identities.
