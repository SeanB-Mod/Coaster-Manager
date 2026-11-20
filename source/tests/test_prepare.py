import copy
import importlib.util
import json
from pathlib import Path
import struct
import unittest

root = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('prepare_blueprint', root/'Prepare_Blueprint.py')
module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
source = json.loads((root/'source'/'tests'/'fixtures'/'source-blueprint.json').read_text())

class PrepareTests(unittest.TestCase):
    def test_cli_without_userprofile(self):
        import os, subprocess, sys
        env=dict(os.environ);env.pop('USERPROFILE', None)
        result=subprocess.run([sys.executable,str(root/'Prepare_Blueprint.py'),'--output','unused.tpmplan','--help'],env=env,capture_output=True)
        self.assertEqual(result.returncode,0,result.stderr)

    def test_native_bytes(self):
        data = module.prepare(source)
        self.assertEqual(len(data), 64+7*72)
        self.assertEqual(data[60:64], struct.pack('<I', int(source['TrackSupportData'][0]['PositionFloat32Bits'][0], 16)))
        fixture = root/'source'/'tests'/'import_plan_fixture.h'
        expected = bytes(int(v) for v in fixture.read_text().split('{')[1].split('}')[0].split(','))
        self.assertEqual(data[60:92], expected[60:92])
    def test_reversed_ecs_order(self):
        d = copy.deepcopy(source)
        for name in ('TrackSupportData', 'TrackPieceData'):
            d[name].reverse()
            for i, record in enumerate(d[name]): record['listIndex'] = i
        self.assertEqual(module.prepare(d), module.prepare(source))
    def test_other_normal_definition(self):
        other = json.loads((root/'source'/'tests'/'fixtures'/'nine-support-blueprint.json').read_text())
        data = module.prepare(other)
        self.assertEqual(struct.unpack_from('<q', data, 108)[0], -133003145)
        self.assertEqual(struct.unpack_from('<I', data, 8)[0], 9)
    def test_extended_records(self):
        d = json.loads((root/'source'/'tests'/'fixtures'/'extended-blueprint.json').read_text())
        data = module.prepare(d)
        self.assertEqual(data[:8], b'TPMPLAN2')
        by_index = {r['SectionIndex']:r for r in d['TrackSupportData']}
        for i in range(7):
            record = data[60+i*72:60+(i+1)*72]
            self.assertEqual(struct.unpack_from('<i',record,32)[0],by_index[i]['Height'])
            self.assertEqual(record[36:40],module.native(by_index[i],'TangentInModifier'))
        self.assertEqual(struct.unpack_from('<q',data,60+72+48)[0],-1978012938)
        self.assertEqual(struct.unpack_from('<q',data,60+4*72+56)[0],-2122730988)
    def test_variable_counts(self):
        for count in (1, 3, 128, 129, 512):
            d = copy.deepcopy(source)
            for name in ('TrackSupportData', 'TrackPieceData'):
                original = copy.deepcopy(d[name][0]); d[name] = []
                for index in range(count):
                    item = copy.deepcopy(original); item['listIndex'] = item['SectionIndex'] = index; d[name].append(item)
            self.assertEqual(len(module.prepare(d)), 64+count*72)
    def test_support_overflow(self):
        d = copy.deepcopy(source)
        for name in ('TrackSupportData', 'TrackPieceData'):
            original = copy.deepcopy(d[name][0]); d[name] = []
            for index in range(513):
                item = copy.deepcopy(original); item['listIndex'] = item['SectionIndex'] = index; d[name].append(item)
        with self.assertRaises(ValueError): module.prepare(d)
    def test_station_definition_plan(self):
        for value in (-123456789, 123456789):
            d = copy.deepcopy(source); d['sourceStationDefinitionId'] = str(value)
            data = module.prepare(d)
            self.assertEqual(data[:8], b'TPMPLAN3')
            self.assertEqual(struct.unpack_from('<q',data,24)[0], value)
            self.assertEqual(data[60:-4],module.prepare(source)[60:-4])
        for value in ('0',str(2**63),True):
            d = copy.deepcopy(source); d['sourceStationDefinitionId'] = value
            with self.assertRaises(ValueError): module.prepare(d)
    def test_rejections(self):
        def altered(path, value):
            d = copy.deepcopy(source); target = d
            for key in path[:-1]: target = target[key]
            target[path[-1]] = value
            with self.assertRaises((ValueError, KeyError, TypeError)): module.prepare(d)
        cases = [
            (('schemaVersion',), 1), (('supportOrientation','sourceSpaceConfirmed'), False),
            (('TrackSupportData',0,'SectionIndex'), 5), (('TrackSupportData',0,'Valid'), False),
            (('TrackSupportData',0,'Height'), 0), (('TrackSupportData',0,'Height'), True),
            (('TrackSupportData',0,'RotationFloat32Bits'), ['0x7fc00000']*4),
            (('TrackSupportData',0,'PositionFloat32Bits'), ['0x00000000']*3),
            (('TrackSupportData',0,'WorldPosition'), [0,0,0]),
            (('TrackSupportData',0,'TangentInModifier'), 1),
            (('stationFrameProbe','transformWorldPosition'), [0,1,0]),
            (('floorFrameProbe','worldRotation'), [0,1,0,0]),
            (('TrackSupportData',), []), (('sourceStationId',), '0'),
        ]
        for path, value in cases:
            with self.subTest(path=path): altered(path,value)
        with self.assertRaises(ValueError): json.loads('{"schema":1,"schema":2}', object_pairs_hook=module.no_duplicates)

if __name__ == '__main__': unittest.main()
