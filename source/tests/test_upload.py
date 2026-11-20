import ctypes,json,os,tempfile
from pathlib import Path
D=ctypes.CDLL(str(Path(__file__).with_name('test_upload.dll').resolve()))
D.UploadSentTitle.restype=ctypes.c_char_p
D.UploadTypeTag.restype=ctypes.c_char_p
D.UploadSetType.argtypes=[ctypes.c_char_p]
D.SelectUpload.argtypes=[ctypes.c_longlong];D.UploadTarget.restype=ctypes.c_ulonglong
D.UploadCreatedId.argtypes=[ctypes.c_ulonglong]
D.UploadFailure.argtypes=[ctypes.c_int]*5;D.UploadRename.argtypes=[ctypes.c_char_p]
D.UploadFormat.argtypes=[ctypes.c_char_p,ctypes.c_char_p,ctypes.c_void_p,ctypes.c_uint]
def tick():
 D.TickUpload()
 while D.UploadPhase() in (5,6):D.TickUpload()
buf=ctypes.create_string_buffer(2048)
assert D.UploadFormat('A coaster 🎢'.encode(),b'Monobeast Metro',buf,len(buf))
assert buf.value.decode()=='This coaster template requires Coaster Manager to work. Please subscribe to Coaster Manager and enable that mod first.\n\n[H1]A coaster 🎢 - Monobeast Metro[/H1]\n[b]Cost:[/b] = $56,260\n[b]Buzz[/b] = Great (73)\n[b]Thrill[/b] = Bad (7)\n[b]Ferocity[/b] = 1%\n[b]Nausea[/b] = 0%'
assert D.UploadFormat(b'[b]name[/b]',b'Type',buf,len(buf)) and b'(b)name(/b)' in buf.value
assert not D.UploadFormat(b'name',b'Type',buf,8)
D.UploadFormatTotal.argtypes=[ctypes.c_char_p,ctypes.c_int,ctypes.c_uint,ctypes.c_uint,ctypes.c_void_p,ctypes.c_uint]
assert D.UploadFormatTotal(b"$51,670",45000,16,1,buf,len(buf))
assert b"[b]Cost:[/b] = $105,170" in buf.value and b"$51,670" not in buf.value
assert b"[b]Buzz[/b] = Great (71)" in buf.value
assert D.UploadFormatTotal(b"$51,670",45000,16,0,buf,len(buf)) and b"$104,670" in buf.value
assert not D.UploadFormatTotal(b"$51,670",45000,16,17,buf,len(buf))
assert not D.UploadFormatTotal(b"?",45000,16,1,buf,len(buf))
item='9876543210123456';owner='76561198012345678'
with tempfile.TemporaryDirectory() as tmp:
 os.environ['USERPROFILE']=tmp
 root=Path(tmp)/'AppData/LocalLow/Two Point Studios/TPM Coaster Library';folder=root/('a'*32);folder.mkdir(parents=True);entry=folder/'entry.json'
 metadata={'schemaVersion':1,'id':'a'*32,'stationDefinitionId':'-123','name':'Original','screenshot':'preview.png','planSha256':'1'*64,'extra':{'nested':[1,True,None,'é']}}
 def reset(**changes):
  m=dict(metadata);m.update(changes);entry.write_text(json.dumps(m,ensure_ascii=False),encoding='utf8');D.SetupUpload();assert D.SelectUpload(-123)==1
 def read():return json.loads(entry.read_text(encoding='utf8'))
 reset(padding='x'*50000);assert D.BeginUpload()==1 and D.UploadPhase()==1 and D.UploadCreates()==1
 assert D.BeginUpload()==0 # duplicate click while pending cannot create another item
 tick();assert D.UploadPhase()==2 and D.UploadSubmits()==1
 assert read()['workshopItemId']==item and read()['workshopOwnerId']==owner and read()['extra']==metadata['extra']
 assert len(read()['padding'])==50000
 assert D.UploadVisibility()==0 and D.UploadTags()==2 and D.UploadTarget()==int(item)
 assert D.UploadDependencies()==2
 assert D.UploadTypeTag()==b'Gearbox Glider'
 assert D.UploadSentTitle()==b'Original - Gearbox Glider'
 tick();assert D.UploadPhase()==0
 # A reload and rename keep the ID, and the next export updates it without CreateItem.
 D.SetupUpload();assert D.SelectUpload(-123)==1;assert D.UploadRename('Updated 🎢'.encode())
 assert read()['workshopItemId']==item and read()['extra']==metadata['extra'];assert D.SelectUpload(-123)==1
 assert D.BeginUpload()==1 and D.UploadPhase()==4 and D.UploadCreates()==0
 tick();assert D.UploadPhase()==2 and D.UploadTarget()==int(item)
 assert D.UploadSentTitle()=='Updated 🎢 - Gearbox Glider'.encode()
 assert D.UploadTags()==2
 tick();assert D.UploadPhase()==0
 # Failed upload retains the durable link for an update retry.
 D.SetupUpload();assert D.SelectUpload(-123)==1;assert D.BeginUpload()==1;tick();D.UploadFailure(2,0,1,0,0);tick()
 assert D.UploadPhase()==0 and read()['workshopItemId']==item
 D.SetupUpload();assert D.SelectUpload(-123)==1;assert D.BeginUpload()==1 and D.UploadCreates()==0;tick();tick()
 # Another account, malformed ID or partial link cannot publish to that ID or create a replacement.
 reset(workshopItemId=item,workshopOwnerId='99');assert not D.BeginUpload() and D.UploadCreates()==0
 for bad in ['0','18446744073709551616','x','-1','',123]:
  reset(workshopItemId=bad,workshopOwnerId=owner);assert not D.BeginUpload() and D.UploadCreates()==0
 reset(workshopItemId=item);assert not D.BeginUpload()
 # Failed atomic persistence prevents submit; retry persists the same created ID.
 reset();(folder/'.entry-update.tmp').write_bytes(b'foreign operation');assert D.BeginUpload()==1;tick()
 assert D.UploadPhase()==3 and D.UploadSubmits()==0 and 'workshopItemId' not in read()
 (folder/'.entry-update.tmp').unlink();tick();assert D.UploadPhase()==0 and read()['workshopItemId']==item
 assert D.SelectUpload(-123)==1;assert D.BeginUpload()==1 and D.UploadCreates()==1;tick();tick()
 # Legal agreement result saves ID but does not attempt upload until the next export.
 reset();D.UploadFailure(1,1,1,0,0);assert D.BeginUpload()==1;tick()
 assert D.UploadPhase()==0 and read()['workshopItemId']==item and D.UploadSubmits()==0
 # An unfinished call stays pending without a second CreateItem.
 reset();D.UploadFailure(1,0,0,0,0);assert D.BeginUpload()==1
 for _ in range(5):tick();assert D.UploadPhase()==1 and not D.BeginUpload()
 assert D.UploadCreates()==1
 # Setter rejection and account changes prevent submit.
 reset(workshopItemId=item,workshopOwnerId=owner);D.UploadFailure(1,0,1,0,1);assert D.BeginUpload()==1;tick();assert D.UploadPhase()==0 and D.UploadSubmits()==0
 reset();assert D.BeginUpload()==1;D.UploadFailure(1,0,1,1,0);tick();assert D.UploadSubmits()==0
 # Confirmed deletion replaces the ID; access/network failures do not.
 reset(workshopItemId=item,workshopOwnerId=owner);D.UploadDetailsResult(9);assert D.BeginUpload()==1
 tick();assert D.UploadPhase()==1 and D.UploadCreates()==1 and 'workshopItemId' not in read()
 D.UploadCreatedId(9876543210123457);tick();assert D.UploadPhase()==2 and read()['workshopItemId']=='9876543210123457';tick()
 reset(workshopItemId=item,workshopOwnerId=owner);assert D.BeginUpload()==1;D.UploadIOFailure(1);tick();assert D.UploadCreates()==0 and read()['workshopItemId']==item
 for result in [2,8,15]:
  reset(workshopItemId=item,workshopOwnerId=owner);D.UploadDetailsResult(result);assert D.BeginUpload()==1;tick()
  assert D.UploadPhase()==0 and D.UploadCreates()==0 and read()['workshopItemId']==item
 # Steam returns DuplicateRequest for an already attached required item: update continues.
 reset(workshopItemId=item,workshopOwnerId=owner);D.UploadDependencyResult(29);assert D.BeginUpload()==1;tick()
 assert D.UploadPhase()==2 and D.UploadSubmits()==1 and D.UploadCreates()==0 and read()['workshopItemId']==item
 tick();assert D.UploadPhase()==0
 # Required-item failure cannot be reported as a completed upload or create another item.
 reset();D.UploadDependencyResult(15);assert D.BeginUpload()==1;tick()
 assert D.UploadPhase()==0 and D.UploadSubmits()==0 and D.UploadDependencies()==1 and read()['workshopItemId']==item
 # Delayed create unlocks the library but retains the pending call; late result resumes safely.
 reset();assert D.BeginUpload()==1;D.UploadFailure(1,0,0,0,0);D.UploadAge();tick()
 assert D.UploadPhase()==1 and D.UploadBusy()==0 and D.BeginUpload()==0
 D.UploadFailure(1,0,1,0,0);tick();assert D.UploadPhase()==2 and D.UploadBusy()==1 and D.UploadCreates()==1;tick()
 # A saved-ID request times out without clearing the ID or creating a replacement.
 reset(workshopItemId=item,workshopOwnerId=owner);assert D.BeginUpload()==1
 D.UploadFailure(1,0,0,0,0);D.UploadAge();tick()
 assert D.UploadPhase()==4 and D.UploadBusy()==0 and read()['workshopItemId']==item and D.UploadCreates()==0
 D.UploadFailure(1,0,1,0,0);tick();tick();assert D.UploadPhase()==0
 # Exactly four coaster types require DLC; Monobeast remains exempt.
 D.UploadDLCType.argtypes=[ctypes.c_int]
 D.UploadNeedsDLC.argtypes=[ctypes.c_int]
 D.UploadDLCResult.argtypes=[ctypes.c_int,ctypes.c_uint]
 for coaster_type in range(5):
  assert D.UploadNeedsDLC(coaster_type)==int(coaster_type<4)
  reset();assert D.BeginUpload()==1
  D.UploadDLCType(coaster_type);tick()
  assert D.UploadDLCs()==int(coaster_type<4) and D.UploadSubmits()==1
  tick();assert D.UploadPhase()==0
 # Normal updates never add DLC, even for Gearbox or another DLC coaster.
 reset(workshopItemId=item,workshopOwnerId=owner);assert D.BeginUpload()==1
 tick();assert D.UploadDLCs()==0 and D.UploadSubmits()==1;tick()
 # DLC errors, duplicates and mismatched identities are best-effort: content still submits.
 for result,bad in [(2,0),(1,1),(1,2),(29,0)]:
  reset();assert D.BeginUpload()==1
  D.UploadDLCResult(result,bad);tick()
  assert D.UploadDLCs()==1 and D.UploadSubmits()==1
  tick();assert D.UploadPhase()==0 and read()['workshopItemId']==item
 # Timed-out DLC work does not block content or release staged files early.
 reset();assert D.BeginUpload()==1;D.TickUpload();assert D.UploadPhase()==6
 D.UploadFailure(1,0,0,0,0);D.UploadAge();D.TickUpload();assert D.UploadPhase()==5
 D.UploadFailure(1,0,1,0,0);tick();tick();assert D.UploadPhase()==0 and D.UploadSubmits()==1
 # Optional export absence and failed DLC callback still allow upload.
 reset();assert D.BeginUpload()==1;D.UploadNoDLCExport();tick()
 assert D.UploadDLCs()==0 and D.UploadSubmits()==1;tick();assert D.UploadPhase()==0
 reset();assert D.BeginUpload()==1;D.TickUpload();assert D.UploadPhase()==6
 D.UploadIOFailure(1);D.TickUpload();assert D.UploadPhase()==5
 D.UploadIOFailure(0);tick();tick();assert D.UploadPhase()==0 and D.UploadSubmits()==1
 # A timed-out DLC attempt with legal agreement pending must fully unlock.
 reset();D.UploadFailure(1,1,1,0,0);assert D.BeginUpload()==1
 D.TickUpload();assert D.UploadPhase()==6
 D.UploadFailure(1,1,0,0,0);D.UploadAge();D.TickUpload();assert D.UploadPhase()==0
 # All pending API phases retain actual staged files and owned pipe after timeout.
 D.UploadRetainStage.argtypes=[ctypes.c_wchar_p,ctypes.c_wchar_p]
 for pending in (2,4,5):
  for failure in (False,True):
   reset(workshopItemId=item,workshopOwnerId=owner);assert D.BeginUpload()==1
   for _ in range(4):
    if D.UploadPhase()==pending:break
    D.TickUpload()
   assert D.UploadPhase()==pending
   package=Path(tmp)/'pending.coaster';jpegpath=Path(tmp)/'pending.jpg'
   package.write_bytes(b'package');jpegpath.write_bytes(b'jpeg')
   D.UploadRetainStage(str(package),str(jpegpath))
   D.UploadFailure(1,0,0,0,0);D.UploadAge();D.TickUpload()
   assert D.UploadPhase()==pending and not D.UploadBusy() and not D.BeginUpload()
   assert package.read_bytes()==b'package' and jpegpath.read_bytes()==b'jpeg' and D.UploadConnectionRetained()
   D.TickUpload();assert D.UploadConnectionRetained() and package.exists() and jpegpath.exists()
   D.UploadFailure(1,0,1,0,0);D.UploadIOFailure(int(failure));D.TickUpload()
   if not failure:
    for _ in range(3):D.TickUpload()
   assert D.UploadPhase()==0 and D.UploadConnectionReleasedOnce()
   assert not package.exists() and not jpegpath.exists() and read()['workshopItemId']==item
   D.TickUpload();assert D.UploadConnectionReleasedOnce()
 # Bounded persistence retries unlock UI; retry retains and saves the original new ID.
 reset();(folder/'.entry-update.tmp').write_bytes(b'foreign');assert D.BeginUpload()==1;tick()
 for _ in range(6):tick()
 assert D.UploadPhase()==3 and D.UploadBusy()==0 and D.UploadSubmits()==0
 (folder/'.entry-update.tmp').unlink();D.RetryUploadEntry()
 assert read()['workshopItemId']==item and D.UploadCreates()==1 and D.UploadPhase()==0
print('PASS: deleted item confirmed and replaced with a new persisted ID; connection/access/unknown failures preserve the old ID. Mocked Steam create/update callbacks, Public visibility and two ordered required items, two exact template/type tags, atomic item/account persistence, reload+rename update, retry without duplicates, legal agreement, pending/failure/account/invalid-link guards and exact BBCode description. No Steam calls or uploads performed.')

D.UploadJpegValid.argtypes=[ctypes.c_char_p,ctypes.c_uint]
jpeg=Path(__file__).with_name('fixtures').joinpath('workshop-preview.jpg').read_bytes()
assert D.UploadJpegValid(jpeg,len(jpeg))
for bad in [jpeg[:-2],jpeg[:15],b'\x89PNG'+jpeg[4:],jpeg[:-2]+b'\0'*(1000000-len(jpeg))+jpeg[-2:]]:assert not D.UploadJpegValid(bad,len(bad))
print('PASS: actual 960x540 JPEG accepted; incomplete/wrong-format previews and the exact 1,000,000-byte boundary rejected.')

assert D.TestOwnedQueue()==1
print('PASS: isolated completion queue consumes only the owned pipe and call, preserves the primary pipe, rejects oversized callbacks, and releases only its own connection.')

D.UploadFixedType.restype=ctypes.c_char_p
assert [D.UploadFixedType(i) for i in range(5)]==[b'Steeper Chase',b'Gearbox Glider',b'Revolver Coaster',b'Cross-Country Cart',b'Monobeast Metro']
assert D.UploadFixedType(-1) is None and D.UploadFixedType(5) is None
print('PASS: delayed create unlock/late result, delayed details/dependency/submit ownership, late success/failure cleanup, bounded persistence/manual retry, fixed English type tags.')
