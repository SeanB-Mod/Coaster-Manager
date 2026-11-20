import json,os
from pathlib import Path
d=json.loads(Path(__file__).with_name('test-records.json').read_text())
s=d['TrackSupportData'][0];p=d['TrackPieceData'][0]
assert s['StationRelativeWorldPosition']==[1,2,3]
assert s['FloorAreaInstance']['Value']=='18446744073709551615'
assert p['TrackPieceDefinitionID']=='9223372036854775807'
assert p['TrackPieceDecorationDefinition']['ID']=='-9223372036854775807'
assert p['DecorationPosition']==.25
assert p['DecorationPositionFloat32Bits']=='0x3e800000'
assert s['BankingModifierFloat32Bits']=='0x3e800000'
assert s['Rotation']==[0,0,0,1]
assert s['StationRelativeRotation']==[0,0,0,1]
assert s['StationRelativeRotationStatus']=='confirmed-world-source'
print('PASS: mock record JSON and relative coordinates')
folder=Path(os.environ['USERPROFILE'])/'AppData/LocalLow/Two Point Studios/TPM Coaster Library/Diagnostics'
files=list(folder.glob('*.json'))
assert len(files)>=2
assert not list(folder.glob('*.tmp'))
assert sum(json.loads(p.read_text())==d for p in files)>=2
print('PASS: unique complete JSON publication through actual Win32 file APIs')
full=json.loads(Path(__file__).with_name('test-floor-document.json').read_text())
assert full['floorFrameProbe']['status']=='world-frame-read'
assert full['floorFrameProbe']['worldPosition']==[100,0,50]
assert full['floorFrameProbe']['supportRotationSpaceConfirmed'] is True
assert full['sourceStationId']=='77'
assert full['supportCount']==full['pieceCount']==1
assert len(full['TrackSupportData'])==1
print('PASS: full export document with floor, station and support probes')

assert full['schemaVersion']==2
assert full['supportOrientation']['sourceSpaceConfirmed'] is True
assert full['supportOrientation']['sourceSpace']=='world'
assert full['placementReady'] is False
