"""Verify Windows load/export ABI and the PiPL resource exists in the linked effect."""
import ctypes as C
import os
from pathlib import Path
root=Path(__file__).resolve().parents[1]
dll=C.WinDLL(str(Path(os.environ.get('MGN_BUILD_DIR',str(root/'build')))/'Release/MakeGrainNow.aex'))
for name in ['EffectMain','PluginDataEntryFunction2','xGPUFilterEntry']:
    assert getattr(dll,name),f'Missing export {name}'
win=C.WinDLL('kernel32',use_last_error=True)
win.FindResourceW.argtypes=[C.c_void_p,C.c_void_p,C.c_wchar_p];win.FindResourceW.restype=C.c_void_p
win.SizeofResource.argtypes=[C.c_void_p,C.c_void_p];win.SizeofResource.restype=C.c_uint32
win.LoadResource.argtypes=[C.c_void_p,C.c_void_p];win.LoadResource.restype=C.c_void_p
win.LockResource.argtypes=[C.c_void_p];win.LockResource.restype=C.c_void_p
resource=win.FindResourceW(dll._handle,16000,'PiPL');assert resource,'Adobe PiPL not linked into native binary'
size=win.SizeofResource(dll._handle,resource);data=C.string_at(win.LockResource(win.LoadResource(dll._handle,resource)),size)
assert b'Make Grain Now' in data and b'MGN Film Texture' in data,'PiPL name/match name missing'
print(f'Native binary loads; Adobe AE/Premiere exports and PiPL resource verified ({size} bytes). This does not verify Adobe rendering.')
