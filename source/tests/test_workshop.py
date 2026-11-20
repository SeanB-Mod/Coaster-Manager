import ctypes,json,os,struct,tempfile,zipfile,zlib,hashlib,io,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from Prepare_Blueprint import prepare
D=ctypes.CDLL(str(Path(__file__).with_name('test_library.dll').resolve()))
M=ctypes.CDLL(str(Path(__file__).with_name('test_management.dll').resolve()))
D.ScanWorkshopLibrary.argtypes=[ctypes.c_wchar_p,ctypes.c_longlong]
D.NameLibrary.restype=ctypes.c_char_p;D.ReadEntryLibrary.argtypes=[ctypes.c_uint,ctypes.c_int,ctypes.c_void_p,ctypes.c_uint]
D.DirectLibrary.argtypes=[ctypes.c_uint,ctypes.c_longlong];D.ResolveWorkshopLibrary.argtypes=[ctypes.c_wchar_p,ctypes.c_void_p]
M.ManageSelectWorkshop.argtypes=[ctypes.c_wchar_p,ctypes.c_longlong,ctypes.c_uint];M.ManageExport.argtypes=[ctypes.c_wchar_p]
fixture=json.loads(Path(__file__).with_name('library_fixture.json').read_text());typ=-1616826962;fixture['sourceStationDefinitionId']=typ
plan=json.dumps(fixture).encode();sha=hashlib.sha256(prepare(fixture)).hexdigest()
def chunk(k,d):return struct.pack('>I',len(d))+k+d+struct.pack('>I',zlib.crc32(k+d))
png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',1,1,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(b'\0\xff\0\0'))+chunk(b'IEND',b'')
meta={'trackColour':{'custom':True,'rgba':[0.125,0.25,0.5,1]},'previewCurve':[[0,1,2],[3,4,5],[6,7,8]],'schemaVersion':1,'id':'d'*32,'name':'Workshop coaster','stationDefinitionId':str(typ),'screenshot':'preview.png','planSha256':sha}
def package(m=None,bad=False):
 b=io.BytesIO()
 with zipfile.ZipFile(b,'w') as z:
  z.writestr('package.json','{"format":"TPM.Coaster","version":1}');z.writestr('entry.json',json.dumps(m or meta));z.writestr('blueprint.json',b'{}' if bad else plan);z.writestr('preview.png',png)
 return b.getvalue()
with tempfile.TemporaryDirectory() as tmp:
 os.environ['USERPROFILE']=str(Path(tmp)/'User')
 local=Path(os.environ['USERPROFILE'])/'AppData/LocalLow/Two Point Studios/TPM Coaster Library';manual=local/'Manual';manual.mkdir(parents=True)
 apps=Path(tmp)/'Steam 中文 Library/steamapps';game=apps/'common/Two Point Museum';game.mkdir(parents=True)
 root=apps/'workshop/content/2185060';item=root/'1234567890';item.mkdir(parents=True)
 out=ctypes.create_unicode_buffer(1024)
 assert D.ResolveWorkshopLibrary(str(game/'TPM.exe'),out)==1 and Path(out.value)==root
 assert D.ResolveWorkshopLibrary(str(Path(tmp)/'Not Steam/TPM.exe'),out)==0
 scan=lambda:D.ScanWorkshopLibrary(str(root),typ)
 assert scan()==0 # Native Museum/non-package content is ignored.
 (item/'museum.bundle').write_bytes(b'irrelevant');(root/'loose.coaster').write_bytes(package())
 invalid=root/'not-an-item';invalid.mkdir();(invalid/'hidden.coaster').write_bytes(package())
 nested=item/'nested';nested.mkdir();(nested/'hidden.coaster').write_bytes(package())
 assert scan()==0
 packagefile=item/'Coaster.coaster';original=package();packagefile.write_bytes(original)
 assert scan()==1 and D.SourceLibrary(0)==2 and D.ReadOnlyLibrary(0)==1 and D.NameLibrary(0)==b'Workshop coaster'
 buf=ctypes.create_string_buffer(5*1024*1024)
 for photo,expected in [(0,plan),(1,png)]:
  size=D.ReadEntryLibrary(0,photo,buf,len(buf)-1);assert buf.raw[:size]==expected
 assert D.DirectLibrary(0,typ)==1 and D.CurveLibrary()==3
 assert M.ManageSelectWorkshop(str(root),typ,0)==1
 assert M.ManageDelete()==0 and M.ManageRename(b'blocked')==0 and M.ManageStartPhoto()==0 and M.ManageExport(tmp)==0
 assert packagefile.read_bytes()==original
 # Full priority and fallback by identity, without modifying lower sources.
 m=dict(meta,name='Manual wins');manualfile=manual/'manual.coaster';manualfile.write_bytes(package(m))
 assert scan()==1 and D.SourceLibrary(0)==1 and D.NameLibrary(0)==b'Manual wins'
 own=local/meta['id'];own.mkdir();(own/'entry.json').write_text(json.dumps(dict(meta,name='User wins')));(own/'blueprint.json').write_bytes(plan);(own/'preview.png').write_bytes(png)
 assert scan()==1 and D.SourceLibrary(0)==0 and D.NameLibrary(0)==b'User wins'
 for f in own.iterdir():f.unlink()
 own.rmdir();assert scan()==1 and D.SourceLibrary(0)==1
 manualfile.unlink();assert scan()==1 and D.SourceLibrary(0)==2 and packagefile.read_bytes()==original
 # Stable Workshop duplicate selection and different-ID coexistence.
 second=root/'2234567890';second.mkdir();other=second/'Another.coaster';other.write_bytes(package(dict(meta,name='Duplicate lower item')))
 assert scan()==1 and D.NameLibrary(0)==b'Workshop coaster'
 packagefile.unlink();assert scan()==1 and D.NameLibrary(0)==b'Duplicate lower item'
 packagefile.write_bytes(package(dict(meta,id='e'*32)));assert scan()==2
 # Missing or changed files cannot alter an already selected build/preview.
 assert D.ReadEntryLibrary(0,0,buf,len(buf)-1)>0
 packagefile.write_bytes(package(dict(meta,id='e'*32,name='Changed')));assert D.ReadEntryLibrary(0,0,buf,len(buf)-1)==0
 packagefile.write_bytes(b'broken zip');assert scan()==1
 other.write_bytes(package(bad=True));assert scan()==0
 assert packagefile.read_bytes()==b'broken zip' and other.exists()
 # Unrelated subscribed museums must not consume the old 256-folder allowance.
 for i in range(320):
  directory=root/str(3000000000+i);directory.mkdir();(directory/'museum.bundle').write_bytes(b'museum')
 late=root/'9999999999';late.mkdir();(late/'Late.coaster').write_bytes(package(dict(meta,id='f'*32,name='Beyond old folder limit')))
 assert scan()==1 and D.NameLibrary(0)==b'Beyond old folder limit'
 assert D.WorkshopFolders()==323
 assert not (Path(tmp)/'escape').exists()
print('PASS: active Steam library resolution (Unicode/custom location), numeric item/direct package filtering, Workshop preview/build, user > Manual > Workshop priority and fallback, stable item duplicates, changed/missing/corrupt rejection, Workshop rename/retake/export/delete blocked; Steam files untouched.')

# Exercise raised package and display caps with real validated stored ZIPs.
with tempfile.TemporaryDirectory() as tmp:
 os.environ['USERPROFILE']=str(Path(tmp)/'User')
 local=Path(os.environ['USERPROFILE'])/'AppData/LocalLow/Two Point Studios/TPM Coaster Library';manual=local/'Manual';manual.mkdir(parents=True)
 root=Path(tmp)/'steamapps/workshop/content/2185060';item=root/'9876543210';item.mkdir(parents=True)
 for i in range(2049):
  (item/f'{i:04d}.coaster').write_bytes(package(dict(meta,id=f'{i+1:032x}',name=f'Capacity {i:04d}')))
 assert D.ScanWorkshopLibrary(str(root),typ)==512
 assert D.WorkshopPackages()==2048 and D.NameLibrary(511) is not None and D.NameLibrary(512) is None
 # Same raised cap applies to Manual packages; their priority must remain higher.
 for i in range(2049):
  (manual/f'{i:04d}.coaster').write_bytes(package(dict(meta,id=f'{i+1:032x}',name=f'Manual Capacity {i:04d}')))
 assert D.ScanWorkshopLibrary(str(root),typ)==512 and D.ManualEntriesSeen()==2048
 assert D.SourceLibrary(0)==1
print('PASS: 2049-package Workshop and Manual fixtures scan up to2048 valid packages, display exactly512 entries and retain source priority/bounded row access.')

with tempfile.TemporaryDirectory() as tmp:
 os.environ['USERPROFILE']=str(Path(tmp)/'User')
 root=Path(tmp)/'steamapps/workshop/content/2185060';item=root/'12345';item.mkdir(parents=True)
 file=item/'Metadata.coaster'
 large=dict(meta,padding='x'*50000);file.write_bytes(package(large))
 assert D.ScanWorkshopLibrary(str(root),typ)==1
 oversized=dict(meta,padding='x'*65536);file.write_bytes(package(oversized))
 assert D.ScanWorkshopLibrary(str(root),typ)==0
print('PASS: Workshop accepts valid metadata above16KiB and rejects metadata exceeding64KiB.')
