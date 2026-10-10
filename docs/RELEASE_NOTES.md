# v0.1.0-alpha.1

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
