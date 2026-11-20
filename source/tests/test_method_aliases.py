from pathlib import Path
import ctypes,os,subprocess,tempfile
root=Path(__file__).resolve().parent;s=(root.parent/'station_preflight.inc').read_text();a=s.index('static int pf_method_name(');s=s[a:s.index('static void *pf_method(',a)]
prefix=r"""
static int equals(const char*a,const char*b){if(!a||!b)return 0;while(*a&&*a==*b){a++;b++;}return *a==*b;}
static int pf_class(void*c,const char*n){return equals((const char*)c,n);}
"""
post=r"""
__declspec(dllexport) int Check(const char*c,const char*a,const char*r){return pf_method_name((void*)c,a,r);}
"""
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'alias.c').write_text(prefix+s+post);llvm=Path(os.environ.get('LLVM_BIN','C:/Tools/llvm-mingw/bin'))
 subprocess.run([str(llvm/'clang.exe'),'--driver-mode=cl','--target=x86_64-pc-windows-msvc','/nologo','/c','/TC','/W4','/WX','/O1','/GS-','/Zl','/Fo:'+str(p/'alias.obj'),str(p/'alias.c')],check=True)
 subprocess.run([str(llvm/'ld.lld.exe'),'-flavor','link','/dll','/noentry','/machine:x64','/nodefaultlib','/out:'+str(p/'alias.dll'),str(p/'alias.obj')],check=True)
 dll=ctypes.CDLL(str(p/'alias.dll'));dll.Check.argtypes=[ctypes.c_char_p]*3
 cls=b'TPS.Game.ECPTrackRide.<>c__DisplayClass15_0'
 for actual,requested in [(b'<SetComponents>g__Snapshot|32',b'<SetComponents>g__Snapshot|30'),(b'<SetComponents>g__Revert|33',b'<SetComponents>g__Revert|31'),(b'<SetComponents>g__OnTrackRevert|14',b'<SetComponents>g__OnTrackRevert|13')]:
  assert dll.Check(cls,actual,requested);assert not dll.Check(b'Other.Closure',actual,requested)
 assert dll.Check(b'TPS.Game.ECPTrackRide',b'<SetComponents>g__ToggleTrackSupportArrow|15_28',b'<SetComponents>g__ToggleTrackSupportArrow|20_26')
 assert not dll.Check(cls,b'<SetComponents>g__Snapshot|99',b'<SetComponents>g__Snapshot|30')
 ctypes.windll.kernel32.FreeLibrary.argtypes=[ctypes.c_void_p];ctypes.windll.kernel32.FreeLibrary(dll._handle)
print('PASS: production scoped alias resolver accepts current Snapshot32/Revert33/support15_28; wrong owners and unknown suffixes refuse')
