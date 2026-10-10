# Make Grain Now

Creative grain, warm halation and neutral bloom for **Premiere Pro and After Effects**, with NVIDIA CUDA acceleration. Windows x64 alpha release, MIT licensed.

Six presets, saved preset randomization, simple primary sliders and expandable advanced controls. Each module can be disabled independently. The effect reads the image supplied by Adobe at its position in the effect stack.

## Download and install

Download the Windows ZIP from the repository's Releases page. Extract it, close Premiere/After Effects (including background renders), then run `Install.ps1` in Administrator PowerShell. See [the tester guide](TESTING_GUIDE.md) for manual installation and troubleshooting.

Installation location:

```text
C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\MakeGrainNow\MakeGrainNow.aex
```

Requirements: Windows x64, NVIDIA GPU with compute capability 7.5 or newer (RTX 20 series or newer), a driver supporting CUDA 12.8-generated PTX, and Microsoft Visual C++ v14 x64 runtime 14.44 or newer. The CUDA toolkit is not needed for the binary. AMD/Intel GPUs and a pure CPU fallback are not supported.

## Use

Look for **Film Texture > Make Grain Now**. Place it after the conversion LUT/Lumetri when the graded image should drive the texture. On an adjustment layer, Adobe supplies the composite beneath it. The plugin does not apply camera LUTs or reopen source media.

Choose a preset or Randomize preset. Grain, halation and bloom amounts independently control the modules. Grain size and glow radii scale relative to a 1080-pixel image height. Advanced controls expose color grain, shadow/highlight response, seed, glow thresholds/radii, input encoding and renderer.

Choose display gamma for converted Rec.709 footage, or Linear RGB only when the incoming pixels are linear. Light spreading uses a gamma 2.4 approximation. Automatic working-space detection and gamut-aware HDR/ACES processing are not implemented. Float rendering preserves negative values and values above 1; integer formats clamp to their native ranges.

## Grain models and validation

The default is interpolated Gaussian noise with variance compensation: a fast creative approximation. The experimental particle mode uses Poisson-distributed equal-radius discs, bounded counts and eight optical samples. Neither is calibrated to a film stock or a full implementation of the Newson paper.

The current binary passed 14 actual AE 32-bit-float exports at 1920x1080/50 fps on an RTX 3060 Ti: clip/adjustment stacking, bypass, repeatability, temporal changes, upstream Exposure changes, CUDA/CPU-visible parity and partial alpha. Systematic Premiere rendering, Lumetri/LUT stacking, host ROI/Multi-Frame Rendering, playback and wider compatibility remain unqualified. The CPU-visible route still uses CUDA with CPU/GPU transfers.

Standalone checks cover native buffers, crop/ROI, HDR, alpha, concurrent rendering, native AE/Premiere entry points with mocked callbacks and an independent particle reference. They are separate from actual Adobe host tests. See [performance measurements](docs/PERFORMANCE.md) and [recorded verification](docs/verification/).

## Build

See [BUILDING.md](docs/BUILDING.md). Adobe SDKs and NVIDIA build dependencies must be obtained separately and are not redistributed. Build scripts accept Python/CMake paths, and isolated build directories are supported.

## License

Original source and plugin: [MIT](LICENSE). External SDKs/tools retain their own terms; see [THIRD_PARTY.md](THIRD_PARTY.md). No footage, Sony LUT, SDK archive/header/tool, private development notes or Adobe project is included.
