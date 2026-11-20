import ctypes, json, os, struct, tempfile, zipfile, zlib, hashlib, io, copy
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from Prepare_Blueprint import prepare
D=ctypes.CDLL(str(Path(__file__).with_name('test_library.dll').resolve()))
D.ScanLibrary.argtypes=[ctypes.c_longlong];D.NameLibrary.restype=ctypes.c_char_p
D.ReadEntryLibrary.argtypes=[ctypes.c_uint,ctypes.c_int,ctypes.c_void_p,ctypes.c_uint]
D.DirectLibrary.argtypes=[ctypes.c_uint,ctypes.c_longlong]
fixture=json.loads(Path(__file__).with_name('library_fixture.json').read_text());typ=-1616826962;fixture['sourceStationDefinitionId']=typ
plan=json.dumps(fixture).encode();sha=hashlib.sha256(prepare(fixture)).hexdigest()
def chunk(k,d):return struct.pack('>I',len(d))+k+d+struct.pack('>I',zlib.crc32(k+d))
png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',1,1,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(b'\0\xff\0\0'))+chunk(b'IEND',b'')
meta={'trackColour':{'custom':True,'rgba':[0.125,0.25,0.5,1]},'previewCurve':[[0,1,2],[3,4,5],[6,7,8]],'schemaVersion':1,'id':'b'*32,'name':'Manual 中文 coaster','stationDefinitionId':str(typ),'screenshot':'preview.png','planSha256':sha,'displayStats':{'buzz':'Perfect (143)'}}
def package(m=None,p=plan,image=png,manifest=None,extra=None,compression=zipfile.ZIP_STORED):
 b=io.BytesIO()
 with zipfile.ZipFile(b,'w',compression=compression) as z:
  z.writestr('package.json',json.dumps(manifest or {'format':'TPM.Coaster','version':1}))
  z.writestr('entry.json',json.dumps(m or meta,ensure_ascii=False));z.writestr('blueprint.json',p);z.writestr('preview.png',image)
  if extra:z.writestr(extra,b'bad')
 return b.getvalue()
with tempfile.TemporaryDirectory() as tmp:
 os.environ['USERPROFILE']=tmp
 parent=Path(tmp)/'AppData/LocalLow/Two Point Studios';parent.mkdir(parents=True)
 root=parent/'TPM Coaster Library';manual=root/'Manual'
 assert D.ScanLibrary(typ)==0 and manual.is_dir() # First run creates canonical folders.
 path=manual/'one.coaster';original=package();path.write_bytes(original)
 assert D.ScanLibrary(typ)==1 and D.NameLibrary(0).decode()==meta['name'] and D.ReadOnlyLibrary(0)==1
 buf=ctypes.create_string_buffer(5*1024*1024)
 for photo,expected in [(0,plan),(1,png)]:
  n=D.ReadEntryLibrary(0,photo,buf,len(buf)-1);assert buf.raw[:n]==expected
 assert D.DirectLibrary(0,typ)==1 and D.CurveLibrary()==3 # Existing validated native placement handoff.
 assert path.read_bytes()==original and sorted(x.name for x in root.iterdir())==['Manual']
 # Duplicate source wins by ID, even if the filename/name differs. Removing local exposes manual.
 local=root/meta['id'];local.mkdir();m=copy.deepcopy(meta);m['name']='Local wins'
 (local/'entry.json').write_text(json.dumps(m));(local/'blueprint.json').write_bytes(plan);(local/'preview.png').write_bytes(png)
 assert D.ScanLibrary(typ)==1 and D.NameLibrary(0)==b'Local wins' and D.ReadOnlyLibrary(0)==0
 for f in local.iterdir():f.unlink()
 local.rmdir();assert D.ScanLibrary(typ)==1 and D.NameLibrary(0).decode()==meta['name']
 duplicate=manual/'aaa.coaster';m['name']='Alphabetical manual winner';duplicate.write_bytes(package(m=m))
 assert D.ScanLibrary(typ)==1 and D.NameLibrary(0)==b'Alphabetical manual winner';duplicate.unlink()
 assert D.ScanLibrary(typ)==1
 # Changed bytes are rejected after selection, then accepted only by a new scan.
 m['name']='Replaced package';path.write_bytes(package(m=m));assert D.ReadEntryLibrary(0,0,buf,len(buf)-1)==0
 assert D.ScanLibrary(typ)==1 and D.NameLibrary(0)==b'Replaced package'
 path.unlink();assert D.ReadEntryLibrary(0,0,buf,len(buf)-1)==0 and D.ScanLibrary(typ)==0
 # Different IDs retain equal layouts/names; wrong type does not appear in this menu.
 path.write_bytes(original);m=copy.deepcopy(meta);m['id']='c'*32;duplicate.write_bytes(package(m=m));assert D.ScanLibrary(typ)==2
 assert D.ScanLibrary(123)==0;duplicate.unlink()
 # ZIP CRC, bounds, traversal, compression, version, metadata, SHA and preview failures.
 badmeta=copy.deepcopy(meta);badmeta['planSha256']='0'*64
 cases=[original[:-1],b'not zip',package(extra='../escape'),package(compression=zipfile.ZIP_DEFLATED),package(manifest={'format':'TPM.Coaster','version':2}),package(m=badmeta),package(p=b'{}'),package(image=png[:-1])]
 changed=bytearray(original);changed[45]^=1;cases.append(bytes(changed))
 for bad in cases:
  path.write_bytes(bad);assert D.ScanLibrary(typ)==0;assert path.read_bytes()==bad
 assert list(root.iterdir())==[manual] and not (Path(tmp)/'escape').exists()
print('PASS: manual discovery/create, Unicode, validated build handoff and previews, ID priority/fallback, stable manual duplicate winner, changed/missing source rejection, version/CRC/traversal/compression/SHA/image validation; source packages untouched.')

