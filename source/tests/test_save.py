import ctypes,json,os,tempfile,sys,struct,zlib,copy
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from Prepare_Blueprint import prepare
import hashlib
D=ctypes.CDLL(str(Path(__file__).with_name('test_save.dll').resolve()))
D.OwnerExpired.argtypes=[ctypes.c_ulonglong,ctypes.c_ulonglong,ctypes.c_int]
# Actual scan race: tick time is captured, then a slower scan publishes a newer seen time.
for now in (0,100,10000,2**32-1,2**48):
 for scan_duration in (0,1,16,100,1001):
  assert D.OwnerExpired(now,now+scan_duration,1)==0
  assert D.OwnerExpired(now,now+scan_duration,0)==1
 assert D.OwnerExpired(now+1000,now,1)==0
 assert D.OwnerExpired(now+1001,now,1)==1
print('PASS: Save owner scan timestamp race cannot wrap into false expiry; destroyed owners and real timeouts still expire.')

D.StageSave.argtypes=[ctypes.c_char_p,ctypes.c_uint,ctypes.c_char_p,ctypes.c_ulonglong,ctypes.c_longlong];D.StageSave.restype=ctypes.c_int
for name in ['StagePath','PhotoPath','FinalPath']:getattr(D,name).restype=ctypes.c_wchar_p
D.FinishSave.restype=ctypes.c_int;D.PlanUnchanged.restype=ctypes.c_int;D.CheckPng.argtypes=[ctypes.c_char_p,ctypes.c_uint];D.CheckPng.restype=ctypes.c_int
doc=json.loads(Path(__file__).with_name('library_fixture.json').read_text());doc['sourceStationDefinitionId']=-1616826962
source=int(doc['sourceStationId']);definition=doc['sourceStationDefinitionId']
def stage(document=doc,name='A name 🎢 "quoted" \\ path'):
 data=json.dumps(document).encode();return D.StageSave(data,len(data),name.encode(),source,definition)
def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data))
png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',1,1,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(b'\0\xff\0\0'))+chunk(b'IEND',b'')
assert D.CheckPng(png,len(png))==1
for bad in [png[:-1],png+b'garbage',png[:40]+bytes([png[40]^1])+png[41:],png[:24],png[:8]+struct.pack('>I',0xffffffff)+png[12:]]:assert not D.CheckPng(bad,len(bad))
with tempfile.TemporaryDirectory() as tmp:
 os.environ['USERPROFILE']=tmp
 studios=Path(tmp)/'AppData/LocalLow/Two Point Studios';studios.mkdir(parents=True)
 root=studios/'TPM Coaster Library'
 D.SeedColour(1)
 D.SeedPreviewCurve(1)
 assert stage()==1
 assert D.PlanUnchanged()==1
 staged=Path(D.StagePath());final=Path(D.FinalPath());photo=Path(D.PhotoPath())
 import uuid
 identifier=uuid.UUID(hex=final.name);assert identifier.version==4 and identifier.variant==uuid.RFC_4122
 assert staged.exists() and not final.exists()
 assert D.FinishSave()==0
 metadata=json.loads((staged/'entry.json').read_text(encoding='utf-8'))
 assert metadata['trackColour']=={'custom':True,'rgba':[0.125,0.25,0.5,1]}
 assert metadata['name']=='A name 🎢 "quoted" \\ path'
 assert len(metadata['previewCurve'])==240 and metadata['previewCurve'][17][1]==17
 assert (staged/'entry.json').stat().st_size<=65536
 D.ReadPreviewCurve.argtypes=[ctypes.c_char_p,ctypes.c_uint]
 raw=json.dumps(metadata).encode();assert D.ReadPreviewCurve(raw,len(raw))==240
 for bad in [None,[],[[1,2,3]],[[1,2,3]]*241,[[1,2,'3'],[1,2,3]],[[1,2,3,4],[1,2,3]],[[5000,2,3],[1,2,3]]]:
  raw=json.dumps({'previewCurve':bad}).encode();assert D.ReadPreviewCurve(raw,len(raw))==0
 raw=b'{}';assert D.ReadPreviewCurve(raw,len(raw))==0
 D.SeedPreviewCurve(0)

 assert metadata['displayStats']==dict.fromkeys(['cost','buzz','thrill','ferocity','nausea'])
 assert metadata['planSha256']==hashlib.sha256(prepare(doc)).hexdigest()
 assert json.loads((staged/'blueprint.json').read_text())==doc
 photo.write_bytes(png[:-1]);assert D.FinishSave()==0 and not final.exists()
 photo.write_bytes(png);assert D.FinishSave()==1
 assert final.exists() and not staged.exists() and (final/'preview.png').read_bytes()==png
 D.DiscardSave();assert final.exists() # Completed entries are never cleaned up as staging.
 first=final
 stats={'cost':'$35,000','buzz':'12','thrill':'140','ferocity':'40%','nausea':'0%'}
 data=json.dumps({'displayStats':stats}).encode();D.SetStats.argtypes=[ctypes.c_char_p,ctypes.c_uint];D.SetStats(data,len(data))
 assert stage(name='Second layout')==1;final=Path(D.FinalPath());assert final!=first
 assert json.loads((Path(D.StagePath())/'entry.json').read_text(encoding='utf-8'))['displayStats']==stats
 Path(D.PhotoPath()).write_bytes(png);final.mkdir();(final/'keep.txt').write_text('Do not overwrite')
 assert D.FinishSave()==-1;D.DiscardSave();assert (final/'keep.txt').read_text()=='Do not overwrite'
 assert first.exists()
 assert stage(name='   ')==0
 bad=copy.deepcopy(doc);bad['TrackSupportData'][0]['Valid']=False
 assert stage(bad)==0
 assert D.PlanUnchanged()==1
 assert not list(root.glob('.saving-*'))
print('PASS: native Save stages snapshot/name/SHA atomically; UTF-8 names round-trip; complete PNG chunk/CRC validation; missing/partial capture rejection; repeated saves; no-overwrite publication; fixed-file cleanup; invalid snapshot rejection; import selection unchanged.')

assert D.TestRestoreUi()==1
print("PASS: UI restoration releases every retained canvas, continues after destroyed/throwing canvases, and is idempotent.")
