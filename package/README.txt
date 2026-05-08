LEGO Harry Potter Years 5-7 Performance Fix
===========================================

Install:
  Right-click install.ps1 -> Run with PowerShell

If PowerShell blocks it, open PowerShell and run:
  powershell -ExecutionPolicy Bypass -File ".\install.ps1"

Uninstall:
  powershell -ExecutionPolicy Bypass -File ".\uninstall.ps1"

Files installed into the game folder:
  d3d9.dll       = LHP2 fix proxy
  dxvk_d3d9.dll  = DXVK x32 d3d9.dll
  lhp2_fix.ini   = frame limiter Sleep cap config
  dxvk.conf      = DXVK config, only if you did not already have one

The fix is reversible. It does not modify harry2.exe or save files.
