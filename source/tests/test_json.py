import ctypes, struct, random, math, json
from pathlib import Path
d=ctypes.CDLL(str(Path(__file__).with_name('test_json.dll').resolve()))
d.FormatFloat.argtypes=[ctypes.c_float];d.FormatFloat.restype=ctypes.c_char_p
d.FormatBits.argtypes=[ctypes.c_float];d.FormatBits.restype=ctypes.c_char_p
d.TransformPoint.argtypes=[ctypes.POINTER(ctypes.c_float)]*4
d.BufferBoundary.restype=ctypes.c_int
d.FormatIds.restype=ctypes.c_char_p
def roundtrip(bits):
 f=struct.unpack('<f',struct.pack('<I',bits))[0]
 text=d.FormatFloat(f)
 if math.isfinite(f):
  assert text is not None
  back=struct.pack('<f',float(text))
  assert back==struct.pack('<f',f),(hex(bits),text,back)
  assert json.loads(d.FormatBits(f))==f'0x{bits:08x}'
 else:assert text is None
for b in [0,0x80000000,1,0x007fffff,0x00800000,0x7f7fffff,0xff7fffff,0x3f800000,0x7f800000,0xff800000,0x7fc00000]:roundtrip(b)
r=random.Random(400)
for _ in range(100000):roundtrip(r.getrandbits(32))
assert d.BufferBoundary()==1
ids=json.loads(d.FormatIds());assert ids==[-9223372036854775808,18446744073709551615,'quote" slash\\ line\n']
def transform(p,o,q):
 a=(ctypes.c_float*3)(*p);b=(ctypes.c_float*3)(*o);c=(ctypes.c_float*4)(*q);out=(ctypes.c_float*3)();d.TransformPoint(a,b,c,out);return list(out)
assert transform([11,4,8],[10,2,5],[0,0,0,1])==[1,2,3]
v=transform([10,2,4],[10,2,5],[0,math.sqrt(.5),0,math.sqrt(.5)])
assert all(abs(a-b)<1e-6 for a,b in zip(v,[1,0,0])),v
for _ in range(1000):
 angle=r.uniform(-math.pi,math.pi);q=[0,math.sin(angle/2),0,math.cos(angle/2)];origin=[r.uniform(-100,100) for _ in range(3)];local=[r.uniform(-30,30) for _ in range(3)]
 world=[origin[0]+math.cos(angle)*local[0]+math.sin(angle)*local[2],origin[1]+local[1],origin[2]-math.sin(angle)*local[0]+math.cos(angle)*local[2]]
 result=transform(world,origin,q)
 assert max(abs(a-b) for a,b in zip(result,local))<3e-5
print('PASS: 100,011 float round-trips/nonfinite rejections; raw bits; signed/unsigned IDs; escaping; 4 MiB boundary; 1,002 coordinate transforms.')
d.RelativeRotation.argtypes=[ctypes.POINTER(ctypes.c_float)]*3
d.RelativeRotation.restype=ctypes.c_int
def relative(s,r):
 a=(ctypes.c_float*4)(*s);b=(ctypes.c_float*4)(*r);out=(ctypes.c_float*4)()
 return d.RelativeRotation(a,b,out),list(out)
def mul(a,b):
 x,y,z,w=a;u,v,t,h=b
 return [w*u+x*h+y*t-z*v,w*v-x*t+y*h+z*u,w*t+x*v-y*u+z*h,w*h-x*u-y*v-z*t]
def normalized(q):
 n=math.sqrt(sum(x*x for x in q));return [x/n for x in q]
for _ in range(10000):
 s=normalized([r.uniform(-1,1) for _ in range(4)])
 local=normalized([r.uniform(-1,1) for _ in range(4)])
 world=mul(s,local);ok,computed=relative(s,world)
 assert ok and max(abs(a-b) for a,b in zip(computed,local))<2e-7
 rebuilt=mul(s,computed)
 assert max(abs(a-b) for a,b in zip(rebuilt,world))<2e-7
ok,q=relative([0,1,0,0],[0,1,0,0]);assert ok and q==[0,0,0,1]
ok,q=relative([0,1,0,0],[0,0,0,1]);assert ok and q==[0,-1,0,0]
for bad in [[0,0,0,0],[0,0,0,2],[float('nan'),0,0,1],[0,float('inf'),0,1]]:
 assert relative([0,0,0,1],bad)[0]==0
 assert relative(bad,[0,0,0,1])[0]==0
print('PASS: 10,000 mixed-axis orientation compositions, known 180-degree cases, invalid quaternion rejection')
