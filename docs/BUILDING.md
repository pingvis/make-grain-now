# Build from source (Windows x64)

Install Visual Studio 2022 C++ Build Tools and a Windows SDK, CMake and Python
3.12 or newer. Use an **x64 Native Tools Command Prompt for VS 2022** or an
equivalent developer environment so `cl`, `rc`, `cmake` and Python are available.
NVIDIA CUDA hardware/driver is required to run verification, including CPU-visible tests.

Download the official Adobe SDKs separately, accepting their licenses:

- AE 26.5: `vendor/ae-sdk/AfterEffectsSDK_26.5_win/Examples/`
- Premiere 26.0: `vendor/pr-sdk/Premiere Pro 26.0 C++ SDK/Examples/`

CMakeLists.txt currently targets those directory names. Adjust AE_ROOT and
PR_ROOT if using another compatible SDK. SDK archives, headers and PiPLTool
are excluded from the repository and distribution.

Install the build/test dependencies locally:

```powershell
python -m pip install --target .deps nvidia-cuda-nvrtc-cu12==12.8.93 nvidia-cuda-runtime-cu12==12.8.90
python -m pip install numpy
.\scripts\build.ps1
.\scripts\verify.ps1
```

NVRTC emits compute_75 PTX with CUDA 12.8. The built .aex embeds that PTX and
loads NVIDIA's system driver. It does not ship the NVRTC/runtime DLLs.

Custom executables and isolated builds:

```powershell
.\scripts\build.ps1 -BuildDir build-candidate -PythonExe 'C:\Python312\python.exe' -CMakeExe 'C:\Tools\CMake\bin\cmake.exe'
.\scripts\verify.ps1 -BuildDir build-candidate -OutputDir tests\outputs\candidate -PythonExe 'C:\Python312\python.exe'
```

Outputs are generated under the build directory and tests/outputs. The native
harness uses mocked Adobe callbacks with real CUDA; it is not host qualification.
The original 14-frame host report is published separately under docs/verification.
Host-testing scripts and media are local development fixtures and are excluded
from this source snapshot.

Close Adobe before installing or rebuilding a development-linked binary:

```powershell
.\scripts\install.ps1
# Or, once, for development with administrator privileges:
.\scripts\install.ps1 -DevelopmentLink
```

The development link depends on keeping this checkout at its installed path.
