# Make Grain Now - alpha test build 0.2

Creative grain, halation and bloom for Adobe Premiere Pro and After Effects.
Start with a disposable project or a copy of your project.

## Requirements

- Windows x64; initial test baseline is Premiere Pro 26.5.2 / After Effects 26.5.
- NVIDIA GPU with compute capability 7.5 or newer (for example GeForce RTX 20 series or newer). AMD, Intel and older NVIDIA GPUs are not supported by this build. Other compatible GPUs have not yet been tested.
- A recent NVIDIA driver supporting CUDA 12.8 PTX or newer. Use the [official NVIDIA driver download](https://www.nvidia.com/Download/index.aspx). The CUDA toolkit is not needed: GPU code is embedded in the effect.
- Microsoft Visual C++ v14 x64 runtime, version 14.44 or newer. If the effect fails to load or reports MSVCP140/VCRUNTIME140 missing, install the [official Microsoft x64 redistributable](https://aka.ms/vc14/vc_redist.x64.exe). [Microsoft's runtime guidance](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist).

## Install with Setup.exe (recommended)

1. Close Premiere Pro, After Effects and background renders.
2. Double-click **MakeGrainNow-...-Setup.exe** and approve the Windows administrator prompt.
3. Follow the wizard, then reopen Adobe. Find **Film Texture > Make Grain Now**.

Setup installs one shared plugin for both apps in
`C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\MakeGrainNow`
on a standard Windows installation. It safely replaces an older copy or
development link at that location. The uninstaller and documentation are stored
separately in `C:\Program Files\MakeGrainNow`.

The installer includes Microsoft's x64 runtime and installs it only if the
required version is missing. It never closes Adobe or restarts Windows
automatically. This alpha installer is unsigned, so Windows may show an unknown
publisher warning. Setup itself makes no network requests.

## Install by copying the plugin (ZIP fallback)

1. Extract the entire ZIP to a normal folder.
2. Close Premiere and After Effects, including background renders.
3. Create this folder if it does not exist:
   `C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\MakeGrainNow`
4. Copy **MakeGrainNow.aex** into that folder. Windows will request administrator permission. If updating an earlier Make Grain Now test build, replace only that file. Keep just one installed copy of the effect.
5. Restart Adobe. Find **Film Texture > Make Grain Now** in the Effects panel (Premiere) or Effects & Presets panel (After Effects).

Alternatively, open PowerShell as Administrator in the extracted folder and run:

```powershell
& .\Install.ps1
```

The script checks the package's binary checksum and copies the effect with rollback on replacement failure. It does not install drivers or Microsoft runtimes. If Windows policy blocks the script, use the copy-by-hand instructions above. Do not use the development-link option on a friend's machine.

## Try it

- Use the CUDA renderer: **File > Project Settings > General** in Premiere; **File > Project Settings > Video Rendering and Effects** in AE, then select the Mercury GPU Acceleration (CUDA) option available in your host.
- Place it after your usual Log-to-display conversion and after Lumetri/your grade. The effect processes the image arriving at its position in the stack. It does not include a camera LUT.
- Start with a **Look**, then adjust **Overall strength**. There are twelve complete creative looks. Zero strength bypasses the effect exactly.
- **New look** picks another look; **New variation** makes bounded changes to the current settings. Both save their results, rather than choosing again every frame. New variation keeps the grain pattern; **New grain pattern** under Advanced changes only the seed.
- **Grain type** offers **Gaussian (fast)** and **Simulated (slow)**. Simulated is the default for newly added effects; existing projects keep their saved choice. It is an experimental particle approximation and costs more GPU time. Use Gaussian when you need extra playback headroom.
- **Advanced** contains individual amounts, size, response, color, radii, seeds, encoding and module presets. Locks preserve module settings during New look / New variation; deliberate manual changes and preset selections still work. Locking either glow also preserves their shared threshold/softness.
- Use **Rec.709 / display gamma** for converted display footage. Select **Linear RGB** only if the pixels arriving at the effect are linear. Automatic HDR/ACES handling is not implemented.

## Useful tests and feedback

Try a clip effect and an adjustment layer above graded footage. Scrub, play, export a short section and reopen the saved test project. Change the upstream grade and check that the glow/grain response updates. Try bright lights, dark scenes and different presets. Grain should animate even on a held image. Re-rendering the same frame with unchanged settings should be stable.

Send your feedback directly to the person who gave you this ZIP:

```text
Build: 0.2 test (include the first 8 characters from SHA256.txt)
Adobe application and exact version:
Windows version:
GPU / VRAM / NVIDIA driver version:
Sequence or composition resolution / frame rate:
Preset / renderer / preview resolution:
Effect on clip or adjustment layer; effects before it:
Playback and short export result:
Steps to reproduce any problem / exact error text:
Optional screenshot or a small non-sensitive test sample:
```

The previous 0.1 build passed actual AE float stacking/alpha tests. Consult the package's verification summary for evidence tied to its exact binary; older reports do not certify a newer build. Premiere operation was reported successful by the original tester; systematic Premiere testing and broader hardware/version compatibility are still pending. Standalone GPU timings are not Adobe playback guarantees. This is an alpha build for feedback. Original project code is licensed under MIT; see LICENSE.

## Remove or update

Close the Adobe apps and background renders. If you used Setup.exe, open
**Windows Settings > Apps > Installed apps > MakeGrainNow > Uninstall**.
To update, run the newer Setup.exe with Adobe closed. If you used the ZIP/script
installation, delete only the installed **MakeGrainNow.aex** from its folder
above to remove it. Leave other plugins intact.

The ZIP contains the effect, installer, documentation, checksums and verification summaries. It contains no footage, LUTs, Adobe SDKs, NVIDIA toolkit, Python tools or source projects. The installer makes no network requests.
