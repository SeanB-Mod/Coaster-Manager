import ctypes, math, random
from pathlib import Path
lib=ctypes.CDLL(str(Path(__file__).with_name('test_import_rotation.dll').resolve()))
lib.ImportTransform.argtypes=[ctypes.POINTER(ctypes.c_float)]*4+[ctypes.c_float,ctypes.POINTER(ctypes.c_float)]
lib.ImportTransform.restype=ctypes.c_int
F=lambda xs:(ctypes.c_float*len(xs))(*xs)
Q=lambda a:F([0,math.sin(math.radians(a)/2),0,math.cos(math.radians(a)/2)])
rng=random.Random(53)
for i in range(10000):
    source,target,support=[rng.uniform(-360,360) for _ in range(3)]
    if i<8:source,target,support=45,[0,45,90,135,180,225,270,315][i],270
    p=[rng.uniform(-1000,1000),0,rng.uniform(-1000,1000)]
    out=F([0]*8)
    assert lib.ImportTransform(Q(source),Q(target),F(p),Q(support),support%360,out)
    d=math.radians(target-source)
    expected=[math.cos(d)*p[0]+math.sin(d)*p[2],0,-math.sin(d)*p[0]+math.cos(d)*p[2]]
    assert max(abs(a-b) for a,b in zip(out,expected))<.0005,(source,target,list(out),expected)
    expectedq=Q(support+target-source)
    assert abs(sum(a*b for a,b in zip(out[3:7],expectedq)))>.999999
    angle=(support+target-source)%360
    assert abs((out[7]-angle+180)%360-180)<.002,(out[7],angle)
print('PASS: 10,000 import yaw transforms, native world rotations and floor-local angles, including 45-degree snap headings.')

# Imported support rotations may contain banking; station rotations remain yaw-only.
for i in range(1000):
    source,target=[rng.uniform(-360,360) for _ in range(2)]
    raw=[rng.uniform(-1,1) for _ in range(4)]; length=math.sqrt(sum(v*v for v in raw)); supportq=F([v/length for v in raw])
    out=F([0]*8);angle=rng.uniform(0,360)
    assert lib.ImportTransform(Q(source),Q(target),F([3,0,7]),supportq,angle,out)
    b,w=math.sin(math.radians(target-source)/2),math.cos(math.radians(target-source)/2)
    x,y,z,v=supportq
    expected=[w*x+b*z,w*y+b*v,w*z-b*x,w*v-b*y]
    assert abs(sum(a*b for a,b in zip(out[3:7],expected)))>.999999
print('PASS: 1,000 banked/tilted support quaternions under destination yaw.')
