# LEGO Harry Potter Years 5-7 Performance Fix

This is a small, reversible fix for two performance problems in the Steam version of **LEGO Harry Potter: Years 5-7** on modern NVIDIA systems.

It does not modify `harry2.exe` or save files.

## What It Fixes

1. **Intermittent NVIDIA DX9 hitch**
   The game can stutter badly every ~45-60 seconds because NVIDIA's legacy DirectX 9 path stalls inside `IDirect3DDevice9::Present`.

   This package uses DXVK so the game renders through Vulkan instead of native DX9.

2. **Complex-area frame drops**
   Some areas trigger a broken frame limiter feedback loop. The game asks Windows to `Sleep` too long, then counts that sleep time as part of the next frame, making ordinary slow frames turn into huge drops.

   This package caps the known frame-limiter `Sleep` callsite to 2ms.

## Files

Installed into the game folder:

```text
d3d9.dll       LHP2 fix proxy
dxvk_d3d9.dll  DXVK x32 d3d9.dll, renamed
lhp2_fix.ini   Sleep cap config
dxvk.conf      DXVK config
```

The proxy is intentionally tiny:

- It forwards D3D9 calls to `dxvk_d3d9.dll`.
- It hooks the game import for `Sleep`.
- It only caps calls returning from `0x004B43FC`, the main frame-limiter callsite found during profiling.

## Install

Download the release zip from:

https://github.com/illusion-pasture-program/lhp2-performance-fix/releases/

Extract it anywhere.

Run:

```powershell
powershell -ExecutionPolicy Bypass -File ".\install.ps1"
```

By default, the installer targets:

```text
C:\Program Files (x86)\Steam\steamapps\common\LEGO Harry Potter Years 5-7
```

For a custom path:

```powershell
powershell -ExecutionPolicy Bypass -File ".\install.ps1" -GameDir "D:\SteamLibrary\steamapps\common\LEGO Harry Potter Years 5-7"
```

## Uninstall

```powershell
powershell -ExecutionPolicy Bypass -File ".\uninstall.ps1"
```

Steam's **Verify integrity of game files** also restores the original game files.
It may not remove extra local wrapper DLLs, so use `uninstall.ps1` when removing this fix.

## Config

`lhp2_fix.ini`:

```ini
enabled = 1
frame_limiter_sleep_cap_ms = 2
debug_log = 0
```

The tested best value is `2`. If you want to experiment, try `3` or `5`.

## Tested Setup

- Windows 11
- Steam version of LEGO Harry Potter: Years 5-7
- NVIDIA RTX 4070 Ti
- NVIDIA driver 576.40
- DXVK 2.7.1 x32 `d3d9.dll`

## Building From Source

Open a normal terminal and run:

```cmd
src\build.bat
```

The build script uses the Visual Studio 2022 x86 compiler environment.

## Credits

DXVK is created by Philip Rebohle and contributors.

This package includes DXVK's 32-bit `d3d9.dll` renamed to `dxvk_d3d9.dll`; see `THIRD_PARTY_NOTICES.md`.
