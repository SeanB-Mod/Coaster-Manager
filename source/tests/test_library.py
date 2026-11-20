import ctypes, json, hashlib, struct, copy, sys, os, tempfile, random
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from Prepare_Blueprint import prepare
D=ctypes.CDLL(str(Path(__file__).with_name('test_library.dll').resolve()))
D.PrepareLibrary.argtypes=[ctypes.c_char_p,ctypes.c_uint,ctypes.c_void_p];D.PrepareLibrary.restype=ctypes.c_uint
D.ParseLibrary.argtypes=[ctypes.c_char_p,ctypes.c_uint];D.ParseLibrary.restype=ctypes.c_int
D.HashLibrary.argtypes=[ctypes.c_char_p,ctypes.c_uint,ctypes.c_void_p]
D.ImageLibrary.argtypes=[ctypes.c_char_p,ctypes.c_uint];D.ImageLibrary.restype=ctypes.c_int
D.ScanLibrary.argtypes=[ctypes.c_longlong];D.ScanLibrary.restype=ctypes.c_uint
D.ScanCachedLibrary.argtypes=[ctypes.c_longlong];D.ScanCachedLibrary.restype=ctypes.c_uint
D.DirectLibrary.argtypes=[ctypes.c_uint,ctypes.c_longlong];D.DirectLibrary.restype=ctypes.c_int
D.LoadLibrary.argtypes=[ctypes.c_uint,ctypes.c_longlong];D.LoadLibrary.restype=ctypes.c_int
D.NameLibrary.argtypes=[ctypes.c_uint];D.NameLibrary.restype=ctypes.c_char_p
out=ctypes.create_string_buffer(64+512*72)
def native(doc):
 b=json.dumps(doc,ensure_ascii=False).encode();n=D.PrepareLibrary(b,len(b),out);return out.raw[:n]
def parse(b):return bool(D.ParseLibrary(b,len(b)))
fixture=json.loads(Path(__file__).with_name('library_fixture.json').read_text())
fixture['sourceStationDefinitionId']=-1616826962
assert native(fixture)==prepare(fixture)
# Nonzero base elevations preserve native bytes; mixed base heights still reject.
for elevation in [0.5, -2.0, 12.0]:
 doc=copy.deepcopy(fixture)
 doc['stationFrameProbe']['transformWorldPosition'][1]=elevation
 for support in doc['TrackSupportData']:
  for field in ['Position','WorldPosition']:
   support[field][1]=elevation
   support[field+'Float32Bits'][1]='0x%08x'%struct.unpack('<I',struct.pack('<f',elevation))[0]
 assert native(doc)==prepare(doc)
 doc['TrackSupportData'][0]['Position'][1]=elevation+1
 doc['TrackSupportData'][0]['WorldPosition'][1]=elevation+1
 for field in ['Position','WorldPosition']:
  doc['TrackSupportData'][0][field+'Float32Bits'][1]='0x%08x'%struct.unpack('<I',struct.pack('<f',elevation+1))[0]
 assert not native(doc)
# Differential parity with the portable fixture, independent of user data.
passed,rejected=1,0
assert native(fixture)==prepare(fixture)
# Each write-relevant family must reject malformed input before dispatch.
mutations=[('schema','other'),('schemaVersion',3),('sourceStationId','9223372036854775808'),('sourceStationDefinitionId',0)]
for key,value in mutations:
 doc=copy.deepcopy(fixture);doc[key]=value;assert not native(doc);rejected+=1
for key,value in [('Valid',False),('Height',True),('Height',0),('SectionIndex',-1),('listIndex',1),('PositionFloat32Bits',['0x7f800000']*3),('FloorAreaInstance',{'_id':'0'}),('LocalRotation',999)]:
 doc=copy.deepcopy(fixture);doc['TrackSupportData'][0][key]=value;assert not native(doc),(key,value);rejected+=1
for key,value in [('TrackPieceDefinitionID','9223372036854775808'),('TrackPieceDefinitionID',False),('TrackPieceDecorationDefinition',{'_id':'1','other':'2'}),('DecorationPositionFloat32Bits','0x7fc00000')]:
 doc=copy.deepcopy(fixture);doc['TrackPieceData'][0][key]=value;assert not native(doc);rejected+=1
for b in [b'{"x":1,"x":2}',b'{"x":1,"\\u0078":2}',b'{} trailing',b'[1,]',b'{"a":1,}',b'01',b'NaN',b'Infinity',b'"\\ud800"',b'"\\udc00"',b'"\xc0\x80"',b'"\xed\xa0\x80"',b'"\xf4\x90\x80\x80"',b'"\\u0000"',b'['*34+b'0'+b']'*34]:assert not parse(b),b
for b in [b'{}',b'[null,true,false,-1,1.2e-3]',b'"\\ud83c\\udfa2"',b'\xef\xbb\xbf{}','"Coaster 🎢"'.encode()]:assert parse(b),b
for n in [0,1,55,56,63,64,65,127,128,1000,9280]:
 b=random.Random(n).randbytes(n);buf=ctypes.create_string_buffer(65);D.HashLibrary(b,n,buf);assert buf.value.decode()==hashlib.sha256(b).hexdigest()
# Header bounds prevent large decoded textures; Unity validates compressed image contents.
def png(w,h):return b'\x89PNG\r\n\x1a\n'+struct.pack('>I',13)+b'IHDR'+struct.pack('>II',w,h)
for w,h,ok in [(1,1,1),(4096,4096,1),(4097,1,0),(0,1,0),(0xffffffff,10,0)]:
 b=png(w,h);assert D.ImageLibrary(b,len(b))==ok
for b in [b'',b'\xff\xd8\xff\xe0\xff\xff',b'not image']:assert not D.ImageLibrary(b,len(b))
for v in [1e-50,1e-8]:
 doc=copy.deepcopy(fixture);doc['floorFrameProbe']['worldRotation'][0]=v;assert not native(doc)
with tempfile.TemporaryDirectory() as tmp:
 os.environ['USERPROFILE']=tmp
 root=Path(tmp)/'AppData/LocalLow/Two Point Studios/TPM Coaster Library';root.mkdir(parents=True)
 for i,name,type in [(1,'Z layout',-1616826962),(2,'A layout 🎢',-1616826962),(3,'Other coaster',99)]:
  id=f'{i:032x}';f=root/id;f.mkdir();(f/'entry.json').write_text(json.dumps(dict(schemaVersion=1,id=id,name=name,stationDefinitionId=str(type),screenshot='preview.png',planSha256='a'*64),ensure_ascii=False),encoding='utf-8')
 assert D.ScanLibrary(-1616826962)==2
 assert D.NameLibrary(0).decode()=='A layout 🎢' and D.NameLibrary(1)==b'Z layout'
 assert D.ScanLibrary(99)==1
 assert D.LoadLibrary(0,-1616826962)==0
 id='1'*32;f=root/id;f.mkdir();plan=prepare(fixture)
 metadata=dict(schemaVersion=1,id=id,name='Validated layout',stationDefinitionId='-1616826962',screenshot='preview.png',planSha256=hashlib.sha256(plan).hexdigest())
 (f/'entry.json').write_text(json.dumps(metadata));(f/'blueprint.json').write_text(json.dumps(fixture))
 assert D.ScanLibrary(-1616826962)==3
 # Sorted order A, Validated, Z: direct dispatch must retain chosen bytes without a selected-plan file.
 assert D.DirectLibrary(1,-1616826962)==1
 changed=copy.deepcopy(fixture);changed['TrackSupportData'][0]['Height']+=1
 (f/'blueprint.json').write_text(json.dumps(changed))
 assert D.LoadLibrary(1,-1616826962)==0
 metadata['screenshot']='../elsewhere.png';(f/'entry.json').write_text(json.dumps(metadata))
 assert D.ScanLibrary(-1616826962)==2
 (root/('4'*32)).mkdir();(root/('4'*32)/'entry.json').write_text('{"schemaVersion":1,"schemaVersion":2}')
 assert D.ScanLibrary(-1616826962)==2
print(f'PASS: native library parser matches {passed} portable fixture plans byte-for-byte; {rejected} malformed layouts, duplicate/Unicode/depth/trailing JSON rejection, SHA-256, image bounds, LocalLow discovery/filter/order.')

# Native virtual-list boundaries: empty/small libraries, top/bottom, clamped input.
_lib_ui=ctypes.CDLL(str(Path(__file__).with_name("test_library.dll")))
_lib_ui.ListFirst.argtypes=[ctypes.c_uint,ctypes.c_float]
_lib_ui.ListFirst.restype=ctypes.c_uint
for _count in range(513):
    for _position in [-1.0,0.0,0.25,0.5,0.75,1.0,2.0,float("nan")]:
        _first=_lib_ui.ListFirst(_count,_position)
        assert 0<=_first<=max(0,_count-6)
    assert _lib_ui.ListFirst(_count,1.0)==0
    assert _lib_ui.ListFirst(_count,0.0)==max(0,_count-6)
# Every row intersecting a fractional four-row viewport has a live slot,
# including reverse scrolling where the preceding row must already be bound.
import math
for _count in range(513):
    for _step in range(1001):
        _normal=ctypes.c_float(_step/1000).value
        _first=_lib_ui.ListFirst(_count,_normal)
        _offset=(1-_normal)*max(0,_count-4)
        _top=int(math.floor(_offset+1e-5))
        _bottom=min(_count,int(math.ceil(_offset+4-1e-5)))
        assert all(_first<=row<_first+6 for row in range(_top,_bottom))
print("PASS: all 0-512 entry lists have live slots for every fractional viewport position, with reverse-scroll buffer and reachable endpoints.")

D.StatsLabel.argtypes=[ctypes.c_char_p,ctypes.c_uint,ctypes.c_void_p,ctypes.c_uint]
D.CostText.argtypes=[ctypes.c_int,ctypes.c_void_p]
for stats, expected in [({},"Cost: ?\nBuzz: ?    Thrill: ?\nFerocity: ?    Nausea: ?"),
 ({"cost":"$35,000","buzz":"0","thrill":"<color=red>123</color>","ferocity":"40%","nausea":None},"Cost: $35,000\nBuzz: 0    Thrill: 123\nFerocity: 40%    Nausea: ?"),
 ({"cost":123,"buzz":"?","thrill":"123<broken","ferocity":"<bad<nested>1","nausea":"1>"},"Cost: ?\nBuzz: ?    Thrill: ?\nFerocity: ?    Nausea: ?")]:
 data=json.dumps({"displayStats":stats}).encode();buf=ctypes.create_string_buffer(400);assert D.StatsLabel(data,len(data),buf,len(buf));assert buf.value.decode()==expected
for value, expected in [(-1,""),(0,"$0"),(999,"$999"),(1000,"$1,000"),(35000,"$35,000"),(2147483647,"$2,147,483,647")]:
 buf=ctypes.create_string_buffer(48);D.CostText(value,buf);assert buf.value.decode()==expected
print("PASS: optional display stats preserve known zero, show ? for missing/null/invalid fields, strip UI tags, and format total cost.")

D.TemplateTotalCost.argtypes=[ctypes.c_char_p,ctypes.c_int,ctypes.c_int,ctypes.c_void_p,ctypes.c_uint]
D.TemplateTotalCost.restype=ctypes.c_int
def check_total(track,station,components,expected):
    buf=ctypes.create_string_buffer(48)
    result=D.TemplateTotalCost(track.encode(),station,components,buf,len(buf))
    assert (buf.value.decode() if result else None)==expected,(track,station,components,result,buf.value,expected)
check_total("$51,670",45000,8500,"$105,170")
check_total("£51,670",45000,8500,"£105,170")
check_total("$82,270",45000,0,"$127,270")
check_total("$0",35000,0,"$35,000")
check_total("$2147483647",0,0,"$2,147,483,647")
for raw,price,extra in [("?",45000,8500),("Cost $50,000",45000,0),("$12,34",45000,0),("$9,999,",45000,0),("$51,670",-1,8500),("$51,670",45000,-1),("$2147483647",1,0),("$-1",45000,0)]:
    check_total(raw,price,extra,None)
D.ComponentsCost.argtypes=[ctypes.c_uint,ctypes.c_uint,ctypes.POINTER(ctypes.c_int)]
D.ComponentsCost.restype=ctypes.c_int
for supports,decs,expected in [(16,1,8500),(11,0,5500),(0,0,0),(0,1,None),(2,3,None),(1,1,1000)]:
    c=ctypes.c_int()
    ok=D.ComponentsCost(supports,decs,ctypes.byref(c))
    assert (c.value if ok else None)==expected,(supports,decs,ok,c.value,expected)
print("PASS: template display-only station + track + supports/decorations total, £/$, malformed/overflow rejection.")


D.RatingNumber.argtypes=[ctypes.c_char_p,ctypes.POINTER(ctypes.c_float),ctypes.c_void_p]
for raw,expected in [("143 / 178",143),("286",286),("0",0),("0 / 38",0),("12.5 / 40",12.5),(" 7 ",7)]:
 value=ctypes.c_float();number=ctypes.create_string_buffer(32)
 assert D.RatingNumber(raw.encode(),ctypes.byref(value),number)==1 and value.value==expected,(raw,value.value)
for raw in ["Perfect (143)","Bad (0)","?","","NaN","Inf","-1","1 / ?","1 / 2 extra","1.","1 / 2.","1000001","77%"]:
 value=ctypes.c_float();number=ctypes.create_string_buffer(32)
 assert D.RatingNumber(raw.encode(),ctypes.byref(value),number)==0,raw
print("PASS: raw Buzz/Thrill parsing handles zero, decimals, current/max; preserves labelled text and rejects ambiguous/invalid values.")

# Complete entry loading in a generated profile replaces live library discovery.
with tempfile.TemporaryDirectory() as tmp:
 os.environ['USERPROFILE']=tmp
 root=Path(tmp)/'AppData/LocalLow/Two Point Studios/TPM Coaster Library'
 identity='c'*32; folder=root/identity;folder.mkdir(parents=True)
 (folder/'blueprint.json').write_text(json.dumps(fixture),encoding='utf-8')
 metadata=dict(schemaVersion=1,id=identity,name='Fixture 中文 🎢',stationDefinitionId=str(fixture['sourceStationDefinitionId']),screenshot='preview.png',planSha256=hashlib.sha256(prepare(fixture)).hexdigest(),previewCurve=[[0,1,2],[3,4,5],[6,7,8]])
 (folder/'entry.json').write_text(json.dumps(metadata,ensure_ascii=False),encoding='utf-8')
 (folder/'preview.png').write_bytes(png(1,1))
 assert D.ScanLibrary(fixture['sourceStationDefinitionId'])==1
 assert D.LoadLibrary(0,fixture['sourceStationDefinitionId'])==1
 assert D.NameLibrary(0).decode()=='Fixture 中文 🎢'
 print('PASS: generated LocalLow fixture loads in an isolated profile, with Chinese and emoji preserved.')

D.ColourLibrary.argtypes=[ctypes.c_char_p,ctypes.c_uint,ctypes.POINTER(ctypes.c_float)]
rgba=(ctypes.c_float*4)()
def colour(doc):
 b=json.dumps(doc).encode();return D.ColourLibrary(b,len(b),rgba)
assert colour({})==0
for custom in (False,True):
 assert colour({'trackColour':{'custom':custom,'rgba':[0,0.25,0.5,1]}})==(2 if custom else 1)
 assert list(rgba)==[0,0.25,0.5,1]
for bad in [None,[],{}, {'custom':1,'rgba':[0,0,0,1]}, {'custom':'true','rgba':[0,0,0,1]}, {'custom':True,'rgba':[0,0,1]}, {'custom':True,'rgba':[0,0,0,1,1]}, {'custom':True,'rgba':[-0.1,0,0,1]}, {'custom':True,'rgba':[0,1.1,0,1]}, {'custom':True,'rgba':[0,None,0,1]}]:
 assert colour({'trackColour':bad})==-1,bad
assert colour({})==0 and list(rgba)==[0,0,0,0] # no stale tint from previous selection
print('PASS: optional track colour accepts custom/default RGBA, preserves channels, rejects malformed/out-of-range data and resets between selections.')

# Exercise production catalogue wrapper against actual changed files, not a cache model.
with tempfile.TemporaryDirectory() as tmp:
 os.environ['USERPROFILE']=tmp
 root=Path(tmp)/'AppData/LocalLow/Two Point Studios/TPM Coaster Library'
 identity='d'*32; folder=root/identity;folder.mkdir(parents=True)
 plan=folder/'blueprint.json'; plan.write_text(json.dumps(fixture),encoding='utf-8')
 metadata=dict(schemaVersion=1,id=identity,name='Before',stationDefinitionId=str(fixture['sourceStationDefinitionId']),screenshot='preview.png',planSha256=hashlib.sha256(prepare(fixture)).hexdigest())
 entry=folder/'entry.json';entry.write_text(json.dumps(metadata),encoding='utf-8')
 (folder/'preview.png').write_bytes(png(1,1))
 kind=fixture['sourceStationDefinitionId']
 D.InvalidateCatalogue();assert D.ScanCachedLibrary(kind)==1 and D.NameLibrary(0)==b'Before'
 metadata['name']='After';entry.write_text(json.dumps(metadata),encoding='utf-8')
 D.DropDisplayedCatalogue() # UI rebuild drops visible rows, not file-only snapshot
 assert D.ScanCachedLibrary(kind)==1 and D.NameLibrary(0)==b'Before'
 plan.write_bytes(b'{}');assert D.LoadLibrary(0,kind)==0 # display cache never bypasses validation
 plan.write_text(json.dumps(fixture),encoding='utf-8')
 D.ExpireCatalogue();assert D.ScanCachedLibrary(kind)==1 and D.NameLibrary(0)==b'After'
 metadata['name']='Invalidated';entry.write_text(json.dumps(metadata),encoding='utf-8')
 D.InvalidateCatalogue();assert D.ScanCachedLibrary(kind)==1 and D.NameLibrary(0)==b'Invalidated'
 assert D.ScanCachedLibrary(kind+1)==0
 assert D.ScanCachedLibrary(kind)==1
print('PASS: production display catalogue reuse, expiration, mutation invalidation, type switching and fresh plan validation after external file changes.')

with tempfile.TemporaryDirectory() as tmp:
 os.environ['USERPROFILE']=tmp
 root=Path(tmp)/'AppData/LocalLow/Two Point Studios/TPM Coaster Library';root.mkdir(parents=True)
 for i in range(2049):
  identity=f'{i+1:032x}';folder=root/identity;folder.mkdir()
  (folder/'entry.json').write_text(json.dumps(dict(schemaVersion=1,id=identity,name=f'Large {i:04d}',stationDefinitionId=str(kind),screenshot='preview.png',planSha256='a'*64)))
 assert D.ScanLibrary(kind)==512 and D.LocalEntriesSeen()==2048
 assert D.NameLibrary(511) is not None and D.NameLibrary(512) is None
print('PASS: 2049-entry local fixture discovers 2048 identities, displays exactly 512, and rejects out-of-range row access.')

D.ScanBytesFit.argtypes=[ctypes.c_uint,ctypes.c_uint];D.ScanBytesFit.restype=ctypes.c_int
MiB=1024*1024
assert D.ScanBytesFit(127*MiB,2*MiB)==1 # exceeds the previous cumulative budget
assert D.ScanBytesFit(487*MiB,25*MiB)==1
assert D.ScanBytesFit(511*MiB,MiB)==1
assert D.ScanBytesFit(511*MiB,MiB+1)==0
assert D.ScanBytesFit(512*MiB,1)==0
assert D.ScanBytesFit(513*MiB,0)==0
assert D.ScanBytesFit(0xffffffff,1)==0
assert D.ScanBytesFit(0,25*MiB)==1 and D.ScanBytesFit(0,25*MiB+1)==0
print('PASS: shared Workshop/Manual512MiB cumulative budget accepts above128MiB and exact512MiB, rejects overflow/oversize, retains25MiB per-package cap.')

for count in [128,129,512,513]:
 doc=copy.deepcopy(fixture)
 for field in ['TrackSupportData','TrackPieceData']:
  seed=copy.deepcopy(doc[field][0]);doc[field]=[]
  for i in range(count):
   record=copy.deepcopy(seed);record['listIndex']=record['SectionIndex']=i;doc[field].append(record)
 if count<=512:
  assert native(doc)==prepare(doc) and len(native(doc))==64+count*72
 else:
  assert native(doc)==b''
  try:prepare(doc)
  except ValueError:pass
  else:raise AssertionError('513 supports accepted')
print('PASS: native/Python support plan parity at128/129/512 and513 rejection; expanded plan buffers exercised.')
