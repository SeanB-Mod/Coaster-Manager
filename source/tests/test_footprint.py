import ctypes,math,random
from pathlib import Path
D=ctypes.CDLL(str(Path(__file__).with_name('test_footprint.dll').resolve()))
F=ctypes.c_float
D.Bounds.argtypes=[ctypes.POINTER(F)]*3+[ctypes.c_int,ctypes.POINTER(F)]
D.LocalPoint.argtypes=[ctypes.POINTER(F)]*4
r=random.Random(42)
for _ in range(1000):
 yaw=r.uniform(-math.pi,math.pi);c,s=math.cos(yaw),math.sin(yaw);origin=[r.uniform(-100,100),5,r.uniform(-100,100)];local=[[r.uniform(-50,50),r.uniform(-50,50)] for i in range(20)]
 world=[v for x,z in local for v in (origin[0]+c*x+s*z,5,origin[2]-s*x+c*z)]
 out=(F*4)();D.Bounds((F*3)(*origin),(F*4)(0,math.sin(yaw/2),0,math.cos(yaw/2)),(F*len(world))(*world),20,out)
 for i,(x,z) in enumerate(local):
  point=(F*3)(*world[3*i:3*i+3]);result=(F*2)();D.LocalPoint((F*3)(*origin),(F*4)(0,math.sin(yaw/2),0,math.cos(yaw/2)),point,result)
  assert abs(result[0]-x)<.0001 and abs(result[1]-z)<.0001
 expected=[min([0]+[p[0] for p in local]),max([0]+[p[0] for p in local]),min([0]+[p[1] for p in local]),max([0]+[p[1] for p in local])]
 assert all(abs(a-b)<.0001 for a,b in zip(out,expected)),(list(out),expected)
print('PASS: 20000 route points and 1000 translated/rotated layouts yield station-local footprint bounds, including station origin.')

D.RailSides.argtypes=[ctypes.POINTER(F),ctypes.c_int,ctypes.POINTER(F)]
for points in ([[0,i,0] for i in range(20)],[[10*math.cos(i*2*math.pi/80),5*math.sin(i*2*math.pi/80),10*math.sin(i*2*math.pi/80)] for i in range(80)]):
 n=len(points);flat=[v for p in points for v in p];out=(F*(3*n))();D.RailSides((F*(3*n))(*flat),n,out)
 for i in range(n):
  v=list(out[3*i:3*i+3]);assert all(math.isfinite(x) for x in v) and abs(sum(x*x for x in v)-1)<.00002
  assert v[1]==0
  if i:assert sum(a*b for a,b in zip(v,out[3*(i-1):3*i]))>=0
print('PASS: paired-rail side/gauge remains finite, normalised and continuous for vertical and rotated elevated paths.')

D.SupportTop.argtypes=[ctypes.POINTER(F),ctypes.c_uint,ctypes.POINTER(F),ctypes.POINTER(F)]
points=(F*12)(0,2,0,10,6,0,10,6,10,0,2,10)
for base,expected in [([3,0,0],3.2),([10,0,7],6),([3,0,10],3.2)]:
 out=F();assert D.SupportTop(points,4,(F*3)(*base),ctypes.byref(out))==1;assert abs(out.value-expected)<.0001
for base in ([5,0,5],[3,9,0]):
 out=F();assert D.SupportTop(points,4,(F*3)(*base),ctypes.byref(out))==0
print('PASS: saved support anchors interpolate sloped track height and reject distant/below-ground matches.')

D.Smooth.argtypes=[ctypes.POINTER(F)]*4+[F,ctypes.POINTER(F)]
for t,expected in [(0,1),(.5,1.5),(1,2)]:
 out=(F*3)();D.Smooth((F*3)(0,0,0),(F*3)(1,2,3),(F*3)(2,4,6),(F*3)(3,6,9),t,out)
 assert all(abs(out[k]-expected*(k+1))<.00001 for k in range(3))
D.RotateOffset.argtypes=[ctypes.POINTER(F)]*3
for _ in range(1000):
 yaw=r.uniform(-math.pi,math.pi);v=[r.uniform(-10,10) for k in range(3)];out=(F*3)();D.RotateOffset((F*4)(0,math.sin(yaw/2),0,math.cos(yaw/2)),(F*3)(*v),out)
 expected=[math.cos(yaw)*v[0]+math.sin(yaw)*v[2],v[1],-math.sin(yaw)*v[0]+math.cos(yaw)*v[2]]
 assert all(abs(a-b)<.00001 for a,b in zip(out,expected))
print('PASS: closed-curve interpolation preserves endpoints/straight segments; 1000 native support attachment offsets rotate correctly.')
