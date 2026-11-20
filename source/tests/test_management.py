import ctypes,json,os,tempfile,struct,zlib
from pathlib import Path
D=ctypes.CDLL(str(Path(__file__).with_name('test_management.dll').resolve()))
D.ManageSelect.argtypes=[ctypes.c_longlong,ctypes.c_uint];D.ManageRename.argtypes=[ctypes.c_char_p];D.ManagePhotoPath.restype=ctypes.c_wchar_p

def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data))
png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',1,1,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(b'\0\xff\0\0'))+chunk(b'IEND',b'')
newpng=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',1,1,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(b'\0\0\xff\0'))+chunk(b'IEND',b'')
with tempfile.TemporaryDirectory() as tmp:
 os.environ['USERPROFILE']=tmp
 root=Path(tmp)/'AppData/LocalLow/Two Point Studios/TPM Coaster Library';root.mkdir(parents=True)
 folder=root/('a'*32);folder.mkdir();entry=folder/'entry.json';source=folder/'blueprint.json';photo=folder/'preview.png'
 metadata={'schemaVersion':1,'id':'a'*32,'stationDefinitionId':'-123','name':'Original','screenshot':'preview.png','planSha256':'1'*64,'coasterName':'Test Coaster','extra':{'keep':[1,2,'\u00e9']}}
 def reset(**changes):
  m=dict(metadata);m.update(changes);entry.write_text(json.dumps(m,ensure_ascii=False),encoding='utf-8');source.write_bytes(b'original immutable source');photo.write_bytes(png);return m
 def select():assert D.ManageSelect(-123,0)==1
 reset()
 def sized_entry(size):
  m=dict(metadata,padding='');base=json.dumps(m,ensure_ascii=False).encode();m['padding']='x'*(size-len(base));raw=json.dumps(m,ensure_ascii=False).encode();assert len(raw)==size;entry.write_bytes(raw)
 sized_entry(50000);select();assert D.ManageRename(b'Large metadata')==1
 assert len(json.loads(entry.read_text(encoding='utf-8'))['padding'])>49000
 sized_entry(65536);select();assert D.ManageRename(b'Original')==1
 before=entry.read_bytes();select();assert D.ManageRename(b'A longer name')==0 and entry.read_bytes()==before
 sized_entry(65537);assert D.ManageSelect(-123,0)==0
 reset();select();snapshot=source.read_bytes();oldpng=photo.read_bytes()
 name='  New \U0001f3a2 "quoted" \\ name  '
 assert D.ManageRename(name.encode())==1
 result=json.loads(entry.read_text(encoding='utf-8'));assert result['name']==name.strip() and result['extra']==metadata['extra'] and result['coasterName']=='Test Coaster'
 assert source.read_bytes()==snapshot and photo.read_bytes()==oldpng and not list(folder.glob('.*'))
 select()
 for name in [b'',b'   ',b'line\nbreak',b'\xff',b'x'*101]:assert D.ManageRename(name)==0
 reset();entry.write_text(entry.read_text(encoding='utf-8').replace('"name":','"na\\u006de":'),encoding='utf-8');select()
 assert D.ManageRename('Unicode \u4e2d\u6587'.encode())==1;assert json.loads(entry.read_text(encoding='utf-8'))['name']=='Unicode \u4e2d\u6587'
 for marker in [{'readOnly':True},{'origin':'workshop'},{'source':'steam-workshop'},{'workshopItemId':'123456'},{'workshopPublishedFileId':'123456'}]:
  reset();select();m=dict(metadata);m.update(marker);entry.write_text(json.dumps(m),encoding='utf-8');before={p.name:p.read_bytes() for p in folder.iterdir()}
  assert D.ManageRename(b'forbidden')==0 and D.ManageDelete()==0 and D.ManageStartPhoto()==0
  assert before=={p.name:p.read_bytes() for p in folder.iterdir()}
  select();assert D.ManageReadOnly()!=0
 reset();select();m=dict(metadata);m['name']='Changed elsewhere';entry.write_text(json.dumps(m),encoding='utf-8');assert D.ManageRename(b'overwrite')==0
 reset();select();(folder/'.entry-update.tmp').write_bytes(b'foreign operation');assert D.ManageRename(b'blocked')==0;assert (folder/'.entry-update.tmp').read_bytes()==b'foreign operation';(folder/'.entry-update.tmp').unlink()
 reset();select();assert D.ManageStartPhoto()==1;temp=Path(D.ManagePhotoPath());assert temp.exists()
 assert D.ManageStartPhoto()==0;temp.write_bytes(png[:-1]);assert D.ManageFinishPhoto()==0 and photo.read_bytes()==oldpng
 D.ManageDiscardPhoto();assert not temp.exists() and photo.read_bytes()==oldpng
 assert D.ManageStartPhoto()==1;temp=Path(D.ManagePhotoPath());temp.write_bytes(newpng);assert D.ManageFinishPhoto()==1
 assert not temp.exists() and photo.read_bytes()==newpng and source.read_bytes()==snapshot and json.loads(entry.read_text(encoding='utf-8'))['name']=='Original'
 select();assert D.ManageStartPhoto()==1;temp=Path(D.ManagePhotoPath());temp.write_bytes(png);m=dict(metadata);m['origin']='workshop';entry.write_text(json.dumps(m),encoding='utf-8');assert D.ManageFinishPhoto()==-1;D.ManageDiscardPhoto()
 reset(screenshot='preview.jpg');photo.unlink();(folder/'preview.jpg').write_bytes(b'old jpeg');select();assert D.ManageStartPhoto()==1
 Path(D.ManagePhotoPath()).write_bytes(png);assert D.ManageFinishPhoto()==1;assert json.loads(entry.read_text(encoding='utf-8'))['screenshot']=='preview.png' and photo.read_bytes()==png
 assert not (folder/'preview.jpg').exists()
 reset();select();(folder/'keep.txt').write_bytes(b'extra file');assert D.ManageDelete()==0 and source.exists() and entry.exists() and photo.exists();(folder/'keep.txt').unlink()
 nested=folder/'nested';nested.mkdir();assert D.ManageDelete()==0 and source.exists();nested.rmdir()
 assert D.ManageDelete()==1 and not folder.exists() and [p.name for p in root.iterdir()]==['Manual']
print('PASS: atomic Unicode rename preserves unknown metadata/source/preview; invalid and stale edits reject; Workshop entries are read-only at open and commit; retake preserves old preview on incomplete capture; validated PNG replaces atomically; permanent deletion has no backup and refuses extra files/directories.')

D.RetakeIdentity.argtypes=[ctypes.c_ulonglong,ctypes.c_longlong,ctypes.c_ulonglong,ctypes.c_longlong,ctypes.c_uint,ctypes.c_uint]
for index,generation in [(10,1),(900,7),(1,0xffffffff)]:
 assert D.RetakeIdentity(123,-456,123,-456,index,generation)==1
for values in [(124,-456,9,1),(123,-457,9,1),(123,-456,0,1),(123,-456,9,0)]:
 assert D.RetakeIdentity(123,-456,*values)==0
assert not D.RetakeIdentity(0,-456,0,-456,1,1)
print('PASS: Retake keeps persistent station/definition identity across regenerated ECS handles and rejects other stations/types/dead handles.')
