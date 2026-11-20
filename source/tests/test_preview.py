import ctypes, pathlib
lib=ctypes.CDLL(str(pathlib.Path(__file__).with_name('test_preview.dll')))
f=lib.ResizePreview;f.argtypes=[ctypes.c_void_p,ctypes.c_uint,ctypes.c_uint,ctypes.c_void_p];f.restype=ctypes.c_int
out=ctypes.create_string_buffer(960*540*4)
def resize(data,w,h):
 src=ctypes.create_string_buffer(data);assert f(src,w,h,out);return out.raw
# Solid input must stay exact, including alpha, at every output pixel.
for w,h in [(1,1),(16,9),(32,9),(9,32),(960,540)]:
 assert resize(bytes([37,91,163,207])*(w*h),w,h)==bytes([37,91,163,207])*(960*540)
# Side bands should be cropped away, preserving the centre of a wide view.
w,h=48,9;data=b''.join(bytes([0,255,0,255]) if 12<=x<36 else bytes([255,0,0,255]) for y in range(h) for x in range(w));assert resize(data,w,h)==bytes([0,255,0,255])*(960*540)
# Top/bottom bands should be cropped away in a tall view.
w,h=16,27;data=b''.join(bytes([0,0,255,255]) if 7<=y<20 else bytes([255,0,0,255]) for y in range(h) for x in range(w));assert resize(data,w,h)==bytes([0,0,255,255])*(960*540)
# Gradient keeps orientation and interpolates the centre without stretching.
data=b''.join(bytes([x*16,y*28,0,255]) for y in range(9) for x in range(16));result=resize(data,16,9);assert result[0:4]==bytes([0,0,0,255]);assert result[-4:]==bytes([240,224,0,255]);mid=(270*960+480)*4;assert 119<=result[mid]<=121 and 111<=result[mid+1]<=113
assert not f(None,10,10,out);assert not f(out,0,10,out);assert not f(out,4097,1,out)
print('PASS: 960x540 RGBA output, constant/alpha preservation, wide/tall centre crops, gradient orientation/interpolation and invalid bounds.')

bg=lib.BackgroundIndex;bg.argtypes=[ctypes.c_void_p];bg.restype=ctypes.c_uint
flat=lib.FlattenBlack;flat.argtypes=[ctypes.c_void_p,ctypes.c_uint];flat.restype=ctypes.c_int
def backdrop(value):
 a=(ctypes.c_ushort*33)(*[ord(c) for c in value],0);return bg(a)
ids=[f"{i:032x}" for i in range(1024)]
choices=[backdrop(value) for value in ids]
assert set(choices)==set(range(8))
assert choices==[backdrop(value) for value in ids]
assert all(backdrop(value)==backdrop(value.upper()) for value in ids)
for alpha in range(256):
 original=bytes([0,129,255,alpha]);a=ctypes.create_string_buffer(original)
 assert bool(flat(a,1))==(alpha!=255)
 assert a.raw[:4]==bytes([(c*alpha+127)//255 for c in original[:3]]+[255])
 assert not flat(a,1)
# Opaque black/dark rails are preserved, never keyed out by RGB colour.
a=ctypes.create_string_buffer(bytes([0,0,0,255,1,2,3,255,250,10,80,0]))
assert flat(a,3) and a.raw[:12]==bytes([0,0,0,255,1,2,3,255,0,0,0,255])
print('PASS: 1024 UUIDs have stable case-insensitive backdrop choices covering all 8 themes; all 256 alpha levels flatten exactly to black, preserve opaque dark rails and are idempotent.')

mask_alpha=lib.GeometryAlpha
mask_alpha.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_uint]
mask_alpha.restype=ctypes.c_int
# All RGB, including perfectly black rail pixels, survives the mask merge.
colour=bytes([0,0,0,255,1,2,3,255,240,100,23,255,21,91,70,255])*256
alphas=([0]*16+[255]*16+list(range(256)))*4
mask=bytes(c for a in alphas[:1024] for c in (77,88,99,a))
a=ctypes.create_string_buffer(colour);m=ctypes.create_string_buffer(mask)
assert mask_alpha(a,m,1024)
assert all(a.raw[i:i+3]==colour[i:i+3] and a.raw[i+3]==mask[i+3] for i in range(0,4096,4))
for alpha in (0,128,255):
 a=ctypes.create_string_buffer(colour);m=ctypes.create_string_buffer(bytes([0,0,0,alpha])*1024)
 assert not mask_alpha(a,m,1024) and a.raw[:4096]==colour
assert not mask_alpha(None,m,1024)
assert not mask_alpha(a,None,1024)
assert not mask_alpha(a,m,31)
print('PASS: separate geometry alpha preserves all RGB and dark rails; empty/full/invalid masks reject without mutating colour.')
