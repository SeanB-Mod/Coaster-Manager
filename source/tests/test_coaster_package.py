import ctypes, hashlib, json, os, struct, sys, tempfile, zipfile, zlib
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from Prepare_Blueprint import prepare
D=ctypes.CDLL(str(Path(__file__).with_name('test_management.dll').resolve()))
D.ManageSelect.argtypes=[ctypes.c_longlong,ctypes.c_uint]
D.ManageExport.argtypes=[ctypes.c_wchar_p]
D.ExportResult.restype=ctypes.c_wchar_p
def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data))
png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',1,1,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(b'\0\xff\0\0'))+chunk(b'IEND',b'')
fixture=json.loads(Path(__file__).with_name('library_fixture.json').read_text())
fixture['sourceStationDefinitionId']=-1616826962
plan=json.dumps(fixture).encode();sha=hashlib.sha256(prepare(fixture)).hexdigest()
with tempfile.TemporaryDirectory() as tmp:
 os.environ['USERPROFILE']=tmp
 root=Path(tmp)/'AppData/LocalLow/Two Point Studios/TPM Coaster Library';root.mkdir(parents=True)
 folder=root/('a'*32);folder.mkdir();desktop=Path(tmp)/'Redirected Desktop';desktop.mkdir()
 metadata={'schemaVersion':1,'id':'a'*32,'name':'Gearbox: 中文 / "test"','stationDefinitionId':str(fixture['sourceStationDefinitionId']),'screenshot':'preview.png','planSha256':sha,'displayStats':{'buzz':'Perfect (143)'},'extra':{'preserve':True}}
 (folder/'entry.json').write_text(json.dumps(metadata,ensure_ascii=False),encoding='utf-8');(folder/'blueprint.json').write_bytes(plan);(folder/'preview.png').write_bytes(png)
 def select():assert D.ManageSelect(fixture['sourceStationDefinitionId'],0)==1
 metadata['largeUnknownField']='x'*50000
 (folder/'entry.json').write_text(json.dumps(metadata,ensure_ascii=False),encoding='utf-8')
 before={p.name:p.read_bytes() for p in folder.iterdir()};select()
 assert D.ManageExport(str(desktop))==1
 first=Path(D.ExportResult());assert first.exists() and first.suffix=='.coaster' and 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa' in first.name and '中文' in first.name and ':' not in first.name
 with zipfile.ZipFile(first) as z:
  assert z.testzip() is None and set(z.namelist())=={'package.json','entry.json','blueprint.json','preview.png'}
  assert json.loads(z.read('package.json'))=={'format':'TPM.Coaster','version':1}
  assert json.loads(z.read('entry.json'))==metadata and z.read('blueprint.json')==plan and z.read('preview.png')==png
 original=first.read_bytes();assert D.ManageExport(str(desktop))==1
 second=Path(D.ExportResult());assert second!=first and '(2)' in second.name and first.read_bytes()==original
 assert before=={p.name:p.read_bytes() for p in folder.iterdir()}
 # Round-trip an exact native export into the package-backed library, without extraction.
 manual=root/'Manual';manual.mkdir(exist_ok=True);imported=manual/'roundtrip.coaster';imported.write_bytes(original)
 hidden=root/'_hidden_fixture';folder.rename(hidden)
 select();assert D.ManageReadOnly()==1
 assert D.ManageRename(b'blocked')==0 and D.ManageStartPhoto()==0
 library=ctypes.CDLL(str(Path(__file__).with_name('test_library.dll').resolve()))
 library.ScanLibrary.argtypes=[ctypes.c_longlong];library.DirectLibrary.argtypes=[ctypes.c_uint,ctypes.c_longlong]
 library.ReadEntryLibrary.argtypes=[ctypes.c_uint,ctypes.c_int,ctypes.c_void_p,ctypes.c_uint]
 assert library.ScanLibrary(fixture['sourceStationDefinitionId'])==1 and library.DirectLibrary(0,fixture['sourceStationDefinitionId'])==1
 buffer=ctypes.create_string_buffer(1024);size=library.ReadEntryLibrary(0,1,buffer,1023);assert buffer.raw[:size]==png
 assert imported.read_bytes()==original
 # Changed packages cannot be deleted using an earlier confirmation selection.
 modified=bytearray(original);modified[50]^=1;imported.write_bytes(modified)
 assert D.ManageDelete()==0 and imported.read_bytes()==modified
 imported.write_bytes(original)
 # Same metadata/layout with changed preview bytes also rejects (whole-package digest).
 with zipfile.ZipFile(first) as z:members={n:z.read(n) for n in z.namelist()}
 changed=desktop/'changed.zip'
 with zipfile.ZipFile(changed,'w') as z:
  for n,b in members.items():z.writestr(n,b)
 imported.write_bytes(changed.read_bytes());assert D.ManageDelete()==0 and imported.exists()
 imported.write_bytes(original);select()
 neighbour=manual/'zzz-duplicate.coaster';neighbour.write_bytes(original)
 assert D.ManageDelete()==1 and not imported.exists() and neighbour.read_bytes()==original
 assert library.ScanLibrary(fixture['sourceStationDefinitionId'])==1 # Lower duplicate becomes visible.
 assert D.ManageDelete()==0 # Repeated confirmation cannot delete another file.
 hidden.rename(folder);neighbour.unlink();select()
 print('PASS: native export round-trip, manual build/preview, protected rename/retake; stale package deletion rejected; exact selected package deleted, duplicate untouched and exposed on rescan.')
 # Incomplete preview, mismatched layout and stale/read-only metadata must never publish.
 for invalid in [png[:-1],b'not an image']:
  (folder/'preview.png').write_bytes(invalid);select();assert D.ManageExport(str(desktop))==0 and len(list(desktop.glob('*.coaster')))==2 and not list(desktop.glob('*.tmp'))
 (folder/'preview.png').write_bytes(png);(folder/'blueprint.json').write_bytes(b'{}');select();assert D.ManageExport(str(desktop))==0
 (folder/'blueprint.json').write_bytes(plan);select();metadata['name']='Changed';(folder/'entry.json').write_text(json.dumps(metadata),encoding='utf-8');assert D.ManageExport(str(desktop))==0
 metadata['readOnly']=True;(folder/'entry.json').write_text(json.dumps(metadata),encoding='utf-8');select();assert D.ManageExport(str(desktop))==0
 assert len(list(desktop.glob('*.coaster')))==2 and not list(desktop.glob('*.tmp'))
print('PASS: portable .coaster ZIP CRC/content/identity, Unicode filename sanitisation, no-overwrite repeat export, source preservation and invalid/stale/read-only rejection; no real Desktop writes.')
