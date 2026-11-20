"""Compile the production UI installer with bounded Win32/runtime mocks."""
import ctypes,subprocess,tempfile,os
from pathlib import Path
root=Path(__file__).resolve().parent
source=(root.parent/'placement_entry.inc').read_text()
installer=source[source.index('static int install_placement_entry(void) {'):]
prefix=r"""
#include <stdint.h>
typedef uintptr_t SIZE_T; typedef unsigned long DWORD;
typedef void (*native_ui_lateupdate)(void*,void*);
static native_ui_lateupdate placement_original;
static int mode,changes;
static void old(void*a,void*b){(void)a;(void)b;}
static void observed_placement_lateupdate(void*a,void*b){(void)a;(void)b;}
static void *slot;
static void *domain(void){return (void*)1;}
static void *(*p_domain_get)(void)=domain;
static void *findclass(void*d,const char*n,const char*c){(void)d;(void)n;(void)c;return (void*)1;}
static void *pf_method(void*c,const char*n,const char*r,unsigned z,const char**p,int i){(void)c;(void)n;(void)r;(void)z;(void)p;(void)i;return mode==1?0:&slot;}
static void logtext(const char*s){(void)s;}
static void *GetCurrentProcess(void){return (void*)1;}
static int ReadProcessMemory(void*p,void*m,void*out,SIZE_T n,SIZE_T*got){(void)p;if(mode==2)return 0;*(void**)out=*(void**)m;*got=mode==3?0:n;return 1;}
static int is_executable_game_pointer(void*p){return p && mode!=4;}
#define PAGE_READWRITE 4
static int VirtualProtect(void*p,SIZE_T n,DWORD flags,DWORD*oldflags){(void)p;(void)n;(void)flags;*oldflags=4;if(mode==5)return 0;if(mode==6 && !changes++)slot=(void*)(uintptr_t)7;return 1;}
"""
post=r"""
__declspec(dllexport) int Run(int m){mode=m;changes=0;slot=(void*)old;placement_original=0;int result=install_placement_entry();if(m==0)return result && slot==(void*)observed_placement_lateupdate && placement_original==old;if(m==6)return !result && slot==(void*)(uintptr_t)7;return !result && slot==(void*)old;}
"""
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.c').write_text(prefix+installer+post)
 llvm=Path(os.environ.get('LLVM_BIN','C:/Tools/llvm-mingw/bin'))
 subprocess.run([str(llvm/'clang.exe'),'--driver-mode=cl','--target=x86_64-pc-windows-msvc','/nologo','/c','/TC','/W4','/WX','/O1','/GS-','/Zl','/Fo:'+str(p/'test.obj'),str(p/'test.c')],check=True)
 subprocess.run([str(llvm/'ld.lld.exe'),'-flavor','link','/dll','/noentry','/machine:x64','/nodefaultlib','/out:'+str(p/'test.dll'),str(p/'test.obj')],check=True)
 d=ctypes.CDLL(str(p/'test.dll'))
 for mode in range(7):assert d.Run(mode),mode
 ctypes.windll.kernel32.FreeLibrary.argtypes=[ctypes.c_void_p]
 ctypes.windll.kernel32.FreeLibrary(d._handle)
 del d
print('PASS UI installer: relocated executable callback accepted; missing signature, read failure, short read, invalid code, protection failure, and concurrent slot replacement rejected')
