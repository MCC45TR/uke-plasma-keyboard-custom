# Future image default

Merge the fragment into `/etc/xdg/kwinrc` only when composing a new Uke image
with the qualified keyboard RPM installed. Do not run a user-session setter
from the RPM transaction. Existing per-user `kwinrc` selections take precedence.
The original `plasma-keyboard` must remain installed for recovery. First-session,
lock-screen and physical tablet acceptance are prerequisites for image use.
