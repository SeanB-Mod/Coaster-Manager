import ctypes,json,os,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parent
s=(root.parent/'first_write.inc').read_text();start=s.index('static unsigned write_hash(');s=s[start:s.index('static void write_log(',start)]
prefix=r"""
#include <stdint.h>
typedef uintptr_t UINT_PTR;typedef void* HMODULE;
static unsigned inputRva;static unsigned char*input;static unsigned inputSize;
static HMODULE GetModuleHandleW(const unsigned short*n){(void)n;return (void*)(uintptr_t)0x10000000;}
static int orientation_code(void*m,void**code,unsigned char*b,unsigned n){if(!m||n>inputSize)return 0;*code=(void*)((uintptr_t)0x10000000+inputRva);for(unsigned i=0;i<n;++i)b[i]=input[i];return 1;}
"""
post=r"""
__declspec(dllexport) int Check(unsigned rva,unsigned n,unsigned expected,unsigned actualRva,unsigned char*b,unsigned size){inputRva=actualRva;input=b;inputSize=size;return write_code_guard((void*)1,rva,n,expected);}
"""
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'guard.c').write_text(prefix+s+post);llvm=Path(os.environ.get('LLVM_BIN','C:/Tools/llvm-mingw/bin'))
 subprocess.run([str(llvm/'clang.exe'),'--driver-mode=cl','--target=x86_64-pc-windows-msvc','/nologo','/c','/TC','/W4','/WX','/O1','/GS-','/Zl','/Fo:'+str(p/'guard.obj'),str(p/'guard.c')],check=True)
 subprocess.run([str(llvm/'ld.lld.exe'),'-flavor','link','/dll','/noentry','/machine:x64','/nodefaultlib','/out:'+str(p/'guard.dll'),str(p/'guard.obj')],check=True)
 d=ctypes.CDLL(str(p/'guard.dll'));d.Check.argtypes=[ctypes.c_uint]*4+[ctypes.c_void_p,ctypes.c_uint]
 for row in json.loads((root/'native_compat_fixtures.json').read_text())+json.loads((root/'current_native_fixtures.json').read_text()):
  raw=bytes.fromhex(row['bytes']);buf=ctypes.create_string_buffer(raw)
  def check(rva=row['newRva'],n=row['size'],expected=row['oldHash'],size=len(raw)):return d.Check(row['oldRva'],n,expected,rva,buf,size)
  assert check(),row['name']
  assert not check(rva=row['newRva']+1),row['name']
  assert not check(expected=row['oldHash']^1),row['name']
  assert not check(size=len(raw)-1),row['name']
  buf[0]=bytes([raw[0]^1]);assert not check(),row['name']
 ctypes.windll.kernel32.FreeLibrary.argtypes=[ctypes.c_void_p];ctypes.windll.kernel32.FreeLibrary(d._handle);del d
print('PASS: all 24 old/new reviewed native compatibility fingerprints accept current bytes; moved pointers, changed bytes, short reads and wrong guard identities reject')
