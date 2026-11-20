import copy, importlib.util, json, unittest
from pathlib import Path
root=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('verify_import',root/'Verify_Import.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
source=json.loads((root/'source/tests/fixtures/extended-blueprint.json').read_text())
class VerifyTests(unittest.TestCase):
    def fixture(self):
        d=copy.deepcopy(source);d['stationPosition'][0]+=40
        for support in d['TrackSupportData']:
            support['Position'][0]+=40;support['WorldPosition'][0]+=40;support['FloorAreaInstance']={'_id':'999'}
        return d
    def test_zero_and_invalid_rotations(self):
        for q in ([0,0,0,0],[0,0,0], [float('nan'),0,0,1]):
            actual=self.fixture();actual['stationRotation']=q
            self.assertIn('Invalid source or target station rotation',m.verify(source,actual))
        bad=copy.deepcopy(source);bad['TrackSupportData'][0]['Rotation']=[0,0,0,0]
        self.assertIn('Invalid source support rotation',m.verify(bad,self.fixture()))
    def test_utf8_bom_cli(self):
        import tempfile,subprocess,sys
        with tempfile.TemporaryDirectory() as tmp:
            first=Path(tmp)/'source.json';second=Path(tmp)/'actual.json'
            a=copy.deepcopy(source);a['name']='Coaster é 🎢';b=self.fixture();b['name']=a['name']
            first.write_text(json.dumps(a,ensure_ascii=False),encoding='utf-8-sig');second.write_text(json.dumps(b,ensure_ascii=False),encoding='utf-8-sig')
            result=subprocess.run([sys.executable,str(root/'Verify_Import.py'),str(first),str(second)],capture_output=True)
            self.assertEqual(result.returncode,0,result.stderr)
    def test_expanded_success(self): self.assertEqual(m.verify(source,self.fixture()),[])
    def test_expanded_mismatches(self):
        for key,field,value in [('TrackSupportData','Height',9),('TrackSupportData','TangentInModifier',.9),('TrackSupportData','BankingModifier',.5),('TrackPieceData','TrackPieceDefinitionID','123')]:
            actual=self.fixture();actual[key][0][field]=value;self.assertTrue(m.verify(source,actual))
        for field,value in [('TrackPieceDecorationDefinition',{'_id':'123'}),('DecorationPosition',.25)]:
            actual=self.fixture();record=next(r for r in actual['TrackPieceData'] if r['SectionIndex']==4);record[field]=value
            self.assertTrue(m.verify(source,actual))
if __name__=='__main__': unittest.main()
