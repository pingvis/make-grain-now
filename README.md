# Make Grain Now

Creative grain, warm halation and neutral bloom for **Premiere Pro and After Effects**, with NVIDIA CUDA acceleration. Windows x64 alpha release, MIT licensed.

Twelve complete looks, an overall strength control and saved look/variation exploration. Detailed controls and module presets live under collapsed Advanced. Each module can be disabled independently. The effect reads the image supplied by Adobe at its position in the effect stack.

## Download and install

Get **Setup.exe** or the ZIP fallback from the [Windows 0.2 test release](https://github.com/pingvis/make-grain-now/releases/tag/v0.2.0-alpha.1).

Both installation methods install the same plugin for Premiere Pro and After Effects. Close both apps and background renders before installing, updating or uninstalling.

### Setup.exe (recommended)

Double-click **MakeGrainNow-...-Setup.exe**, approve the Windows administrator prompt and follow the wizard. Setup includes the required Microsoft Visual C++ runtime and installs it only if missing. The alpha installer is unsigned, so Windows may show an unknown-publisher prompt.

To update, run the newer Setup.exe. To remove it, use **Windows Settings > Apps > Installed apps > MakeGrainNow > Uninstall**.

### PowerShell (ZIP or development)

Extract the ZIP, open PowerShell **as Administrator** in the extracted folder and run:

```powershell
& .\Install.ps1
```

This copies the plugin and checks its checksum. The script does not install Microsoft's runtime or register a Windows uninstaller. For a script-only installation, remove the installed `MakeGrainNow.aex` file manually with Adobe closed.

Developers can link Adobe to their local build instead of copying it. From the repository root, after building, run in Administrator PowerShell:

```powershell
& .\scripts\install.ps1 -DevelopmentLink
```

The development link survives restarts and follows updates to the local build. Keep Adobe closed while replacing that build. Running Setup.exe replaces the link with an independent copy; rerun the development-link command to restore automatic local updates. Use one installation method at a time; both target the shared location below.

See [the tester guide](TESTING_GUIDE.md) for requirements, manual copying and test suggestions.

Installation location:

```text
C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\MakeGrainNow\MakeGrainNow.aex
```

Requirements: Windows x64, NVIDIA GPU with compute capability 7.5 or newer (RTX 20 series or newer), a driver supporting CUDA 12.8-generated PTX, and Microsoft Visual C++ v14 x64 runtime 14.44 or newer. The CUDA toolkit is not needed for the binary. AMD/Intel GPUs and a pure CPU fallback are not supported.

## Use

Look for **Film Texture > Make Grain Now**. Place it after the conversion LUT/Lumetri when the graded image should drive the texture. On an adjustment layer, Adobe supplies the composite beneath it. The plugin does not apply camera LUTs or reopen source media.

Choose a **Look**, then adjust **Overall strength**. **New look** chooses another complete look; **New variation** makes bounded changes to the current settings. These actions are saved and do not randomize on every frame. **Grain type** offers Gaussian (fast) and Simulated (slow), with Simulated selected for new instances.

Expand **Advanced** for separate grain/halation/bloom presets, amounts, size, color, radii, response, encoding and seeds. Settings locks constrain New look and New variation; explicit preset selections and manual edits still work. Shared highlight threshold/softness are preserved when either glow module is locked. New grain pattern changes only the grain seed; New variation keeps that pattern. Zero-strength is exact bypass. At partial strength, the effect blends its final RGB result with the incoming image and preserves alpha. See [the look guide](docs/LOOKS.md).

Grain size and glow radii retain their existing scale relative to a 1080-pixel image height. Older projects retain their saved renderer and settings; the new default applies to newly added effects.

Choose display gamma for converted Rec.709 footage, or Linear RGB only when the incoming pixels are linear. Light spreading uses a gamma 2.4 approximation. Automatic working-space detection and gamut-aware HDR/ACES processing are not implemented. Float rendering preserves negative values and values above 1; integer formats clamp to their native ranges.

## Grain models and validation

**Gaussian (fast)** is interpolated Gaussian noise with variance compensation. **Simulated (slow)**, the new-instance default, uses the existing experimental particle model: Poisson-distributed equal-radius discs, bounded counts and eight optical samples. Preferring its appearance is a creative choice; it costs substantially more GPU time and neither model is film-stock calibrated or a full implementation of the Newson paper.

Preset-first 0.2 passed targeted actual AE tests using synthetic 640x360/50 fps float exports: older project parameter/model/animation migration, neutral strength on upgrade, new-instance Simulated default, legacy render comparison, exact zero-strength bypass, partial-strength blending, exploration locks, Undo, pattern-only changes, and save/reopen persistence. The prior 0.1 binary separately passed 14 actual AE 1920x1080/50 fps float stacking/alpha exports; those older results do not certify 0.2. Systematic Premiere rendering, Lumetri/LUT stacking, host ROI/Multi-Frame Rendering, playback and wider compatibility remain unqualified. The CPU-visible route still uses CUDA with CPU/GPU transfers.

Standalone checks cover native buffers, crop/ROI, HDR, alpha, concurrent rendering, native AE/Premiere entry points with mocked callbacks and an independent particle reference. They are separate from actual Adobe host tests. See [performance measurements](docs/PERFORMANCE.md) and [recorded verification](docs/verification/).

## Build

See [BUILDING.md](docs/BUILDING.md) and [Windows setup builds](docs/SETUP_BUILD.md). Adobe SDKs and NVIDIA build dependencies must be obtained separately and are not redistributed. Build scripts accept Python/CMake paths, and isolated build directories are supported.

## License

Original source and plugin: [MIT](LICENSE). External SDKs/tools retain their own terms; see [THIRD_PARTY.md](THIRD_PARTY.md). No footage, Sony LUT, SDK archive/header/tool, private development notes or Adobe project is included.
