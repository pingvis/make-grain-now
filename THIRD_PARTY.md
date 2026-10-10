# Provenance and distribution

The image-processing kernels and host adapters in src/ were authored for this project and are licensed under MIT; see LICENSE. No GPL grain-rendering source has been copied. External SDKs and tools retain their own licenses.

- Adobe After Effects 26.5 and Premiere Pro 26.0 C++ SDKs: obtained from the signed-in official Adobe Developer Console. Used as build-time headers and the PiPL resource compiler. Kept under ignored vendor/. Adobe's SDK license applies; do not redistribute the SDK archives, headers or tools with our source. Review the SDK license before a public release.
- NVIDIA CUDA NVRTC 12.8.93 and runtime/header package 12.8.90: obtained from NVIDIA's official PyPI packages under ignored .deps/. Used to compile CUDA PTX at build time and provide Driver API declarations. The .aex embeds our compiled PTX and uses the installed NVIDIA driver; it does not ship NVRTC or CUDA runtime DLLs. NVIDIA package license files remain in .deps/. Review NVIDIA redistribution terms if packaging these dependencies later.
- Newson et al., *Realistic Film Grain Rendering*, IPOL 2017: https://www.ipol.im/pub/art/2017/192/ . The Boolean-disc concept informs the experimental particle path. Our implementation uses equal radii, bounded Poisson counts and eight Monte Carlo samples. It is not yet a calibrated or fully validated implementation of the paper. The fast default is filtered Gaussian noise and must be described as a creative approximation.
- Sony conversion LUT: used locally for test previews only; not included in this repository or distribution.
- Camera footage: supplied by the user and retained unchanged under tests/fixtures/input/. Never include it in distributable packages or upload it externally without user authorization.
- FFmpeg, Python, NumPy and Pillow: existing local tools used for fixtures and verification. They are not linked into or shipped with the native plugin.

The public source and binary package exclude third-party SDKs, compiler tools, camera footage, conversion LUTs and Adobe test projects.
- psd-tools 1.24.0 (MIT), attrs 26.1.0, Pillow 12.3.0, NumPy 2.5.3 and typing-extensions 4.16.0: installed from PyPI into ignored .deps/psd-python solely for reading actual AE float PSD test exports. Package metadata and licenses are retained there; these are test dependencies and excluded from the native plugin/distribution.
