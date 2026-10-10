# v0.2 preset-first update (local test build)

Windows Setup.exe now provides a graphical installation wizard and a standard
uninstaller registered in Windows Installed apps. It includes the Microsoft
x64 runtime, refuses installation/removal while Adobe is running, and preserves
an older plugin on replacement failure. Installer testing uses disposable
workspace destinations; the plugin binary and rendering behavior are unchanged.
The alpha installer is unsigned.

Twelve complete creative looks, Overall strength, New look and bounded New
variation. Detailed amounts, sizes, response, module presets, locks, pattern
and encoding controls live under collapsed Advanced. Grain type is visible:
Gaussian (fast) / Simulated (slow), with Simulated as the new-instance default.
Existing projects retain their saved model/settings; no automatic switching or
grain/halation/bloom algorithm redesign was added.

Binary SHA256:
`E16C26CE9E930517FA89F7E586AA24B7722CE9252202B8D5EC1C51CAC1491C4A`.

Complete standalone/mock-SDK checks pass. Eighty legacy CUDA comparisons pass
at a 1e-6 tolerance (maximum 2.384e-7, alpha exact). Actual AE synthetic
640x360/50 fps float exports verify both legacy models/settings and animated
grain migration, new default, zero/partial strength and legacy appearance
(maximum 3.576e-7). Actual button/lock/Undo/pattern interactions and save/reopen
pass. Host UI dropdown selection through the automation tool remains
unqualified; all preset callback cases pass the native mock harness.
No performance experiments occurred; deferred candidates are recorded locally.
Premiere host rendering, 4K playback, ROI and MFR still need qualification.
This build has not been published by this development step.

# v0.1.0-alpha.1 (previous build)

Windows x64 NVIDIA CUDA alpha for Premiere Pro and After Effects. Creative grain,
halation and bloom; six presets, saved randomization and expandable controls.

Includes the optimized native build, installer, MIT license, third-party notices,
tester instructions, checksums and verification evidence.

Binary SHA256:
`745C8DF9D3A10118F36E4B2C361F740E8D4DBADB499AC236EB2A3EBFDE839251`.

Fourteen actual AE float exports pass stacking, bypass, temporal/upstream changes,
CUDA/CPU-visible parity and alpha preservation. Native SDK callback tests and
standalone CUDA tests pass. Systematic Premiere tests, Lumetri/LUT placement,
host ROI/MFR, playback and broader hardware/host qualification remain pending.

Requires Windows x64, NVIDIA compute capability 7.5+, a CUDA 12.8 PTX-compatible
driver and Microsoft Visual C++ v14 x64 runtime 14.44+. No toolkit is needed
for the prebuilt plugin. See START_HERE.md inside the ZIP for installation.
