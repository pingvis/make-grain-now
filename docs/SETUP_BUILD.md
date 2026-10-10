# Windows installer

The recommended tester delivery is one `MakeGrainNow-...-Setup.exe`.
It installs the verified .aex into the shared Adobe MediaCore folder used by
both Premiere Pro and After Effects, registers MakeGrainNow in Windows
Installed apps, and includes the standard Inno Setup uninstaller.
Documentation and uninstall metadata live in Program Files/MakeGrainNow.

## Build

Run from the repository root:

```powershell
& .\scripts\prepare_setup_tools.ps1
& .\scripts\package.ps1 -FriendTest
& .\scripts\build_setup.ps1
```

The official Inno Setup 6.7.3 compiler is extracted in portable mode under
ignored `.deps/setup-tools`, with no compiler uninstall entry or shortcuts.
Download hashes and vendor signatures are checked. The included, unmodified
Microsoft Visual C++ x64 Redistributable is version 14.51.36247.0. The Microsoft
download URL is mutable: if it changes, review its version/signature and update
the pinned hash in both preparation and build scripts deliberately.

The build refuses a stale plugin or stale packaged verification evidence.
It produces the EXE, a SHA256 companion and a JSON build report in `dist/`.
The Inno source has an explicit nine-file product allowlist plus the runtime.
No source projects, footage, LUTs, SDKs or private project notes are embedded.
The PowerShell/manual-copy ZIP remains available as a fallback.

## Runtime behavior

Setup requests administrator rights. It refuses to modify files while
AfterFX.exe, AfterFX.com, Adobe Premiere Pro.exe or aerender.exe is running.
The uninstaller checks at launch and immediately before removal. Neither
program closes applications or forces a restart.

Setup verifies the extracted plugin checksum before changing the old plugin.
It installs the shared Microsoft runtime only when version 14.44 or newer
is missing. The runtime payload is hash checked before execution, and runtime
failure leaves the prior plugin in place. If Microsoft returns reboot-required
code 3010, Setup asks the user to restart and rerun it, preserving the prior
plugin; Setup does not schedule an automatic reboot. The shared runtime remains installed
when MakeGrainNow is removed, because other applications can depend on it.

An existing plugin file or development link is renamed before the new copy is
installed. Failed setup restores the prior entry. Successful setup removes the
old backup; a failed backup cleanup is logged and the backup is retained.
The installer never copies through an existing link to its source. Unknown
files in the product directories survive uninstall. There are no recursive
wildcard delete rules, shell helpers, telemetry or network requests in Setup.

The EXE is unsigned. A real code-signing certificate is a separate release
step; the package must not claim a verified MakeGrainNow publisher yet.
GPU/driver requirements are stated in the wizard and tester guide; setup does
not certify hardware compatibility or install NVIDIA drivers.

## Disposable tests

```powershell
& .\scripts\build_setup.ps1 -TestFixtureRoot "$PWD\tests\outputs\setup-test-unique"
& .\tests\setup_installer_tests.ps1 -FixtureRoot "$PWD\tests\outputs\setup-test-unique"
```

Use a fresh folder. The fixture compiler rejects destinations outside
`tests/outputs`. Fixture installers are visibly labeled TEST FIXTURE, built
outside `dist`, run without elevation, use a unique test-only HKCU Installed
apps identity, and disable shared runtime installation. Real Windows process
snapshots are tested using disposable executables with a test-only name prefix;
real Adobe applications are never closed or modified by these tests.

Tests cover fresh installation, regular-file upgrades, injected failure after
backup, link source preservation, all four host guards for install/uninstall,
unrelated-file preservation, registration removal and reinstall. A hard link
is used if Windows denies symbolic-link creation; the report records which
kind was actually tested. Symbolic-link handling, production administrator
installation and installing a missing runtime must not be described as tested
on this machine unless separate evidence is obtained. The plugin binary is
unchanged: installer tests do not certify new Adobe rendering or playback.

References: [Inno Setup](https://jrsoftware.org/isinfo.php),
[portable compiler mode](https://jrsoftware.org/ishelp/topic_technotes.htm),
[Microsoft runtime redistribution and switches](https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files).
