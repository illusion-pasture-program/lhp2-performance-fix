# v0.1.0

Initial public package.

## Included Fixes

- Chains Direct3D 9 through DXVK x32, avoiding the NVIDIA legacy DX9 `Present` hitch.
- Caps the LEGO Harry Potter Years 5-7 main frame-limiter `Sleep` callsite at `0x004B43FC` to 2ms.

## Tested Result

On the test system, the game went from severe intermittent hitches and complex-area drops to stable 165 FPS in areas that previously dropped hard.

## Package Contents

- `d3d9.dll` — LHP2 fix proxy
- `dxvk_d3d9.dll` — DXVK 2.7.1 x32 `d3d9.dll`, renamed
- `lhp2_fix.ini` — Sleep cap config
- `dxvk.conf` — DXVK defaults
- `install.ps1` / `uninstall.ps1`
