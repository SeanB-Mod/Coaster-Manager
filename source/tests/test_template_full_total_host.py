"""Host-executed production pricing arithmetic and real blueprint billing fixture."""
from pathlib import Path
import json, re, subprocess, tempfile
root = Path(__file__).resolve().parents[1]
src = (root / 'blueprint_display_stats.inc').read_text()
def extract(name):
    m = re.search(r'static (?:void|int)\s+' + re.escape(name) + r'\s*\(', src)
    assert m, name
    brace = src.index('{',m.start()); level=0
    for i in range(brace,len(src)):
        if src[i]=='{':level += 1
        elif src[i]=='}':
            level-=1
            if not level:return src[m.start():i+1]
    raise AssertionError(name)
production = '\n'.join(extract(x) for x in ['blueprint_cost_text','blueprint_track_cost_number','blueprint_component_display_cost','blueprint_template_total_cost'])
fixture=json.loads((Path(__file__).with_name('fixtures') / 'blender-cost-counts.json').read_text())
entry=json.loads((Path(__file__).with_name('fixtures') / 'blender-cost-entry.json').read_text())
supports=fixture['TrackSupportData'];pieces=fixture['TrackPieceData']
assert len(supports)==fixture['supportCount']==16
assert len(pieces)==fixture['pieceCount']==16
decorations=sum(1 for x in pieces if int(x['TrackPieceDecorationDefinition']['_id'])!=0)
assert decorations==1
assert entry['displayStats']['cost'] in ('$51,670','£51,670')
assert entry['stationDefinitionId']==fixture['sourceStationDefinitionId']
code='''
#include <string.h>
#include <stdio.h>
#include <assert.h>
#define WRITE_MAX_SUPPORTS 512U
static unsigned length(const char *s){return (unsigned)strlen(s);}
'''+production+'''
static void verify(const char *track,int station,unsigned supports,unsigned decorations,const char *want) {
 int extra=-1;
 if(!blueprint_component_display_cost(supports,decorations,&extra)){puts("FAIL count");__builtin_trap();}
 char s[48]={0};
 if(!blueprint_template_total_cost(track,station,extra,s,sizeof(s)) || strcmp(s,want)) {
  printf("FAIL: extra=%d got=%s wanted=%s\\n",extra,s,want); __builtin_trap();
 }
}
static void reject(const char *track,int station,int extra) {
 char s[48]={0};
 if(blueprint_template_total_cost(track,station,extra,s,sizeof(s))){puts("FAIL reject");__builtin_trap();}
}
int main(void){
 verify("$51,670",45000,16,1,"$105,170");
 verify("£51,670",45000,16,1,"£105,170");
 verify("$82,270",45000,11,0,"$132,770");
 verify("$0",35000,1,0,"$35,500");
 verify("$2,147,482,647",0,1,1,"$2,147,483,647");
 reject("$2,147,483,647",1,500);
 reject("?",45000,8500);
 reject("$51,670",-1,8500);
 reject("$51,670",45000,-1);
 reject("$12,34",45000,8500);
 int extra;
 assert(!blueprint_component_display_cost(0,1,&extra));
 assert(!blueprint_component_display_cost(1,2,&extra));
 assert(!blueprint_component_display_cost(513,0,&extra));
 puts("PASS: actual Blender 16 supports, 1 decorated piece, $51,670 saved track + $45,000 station = $105,170; overflow & invalid input rejected");
 return 0;
}
'''
with tempfile.TemporaryDirectory() as td:
    source=Path(td)/'host.c';exe=Path(td)/'host';source.write_text(code)
    subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(source),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
# Assert production integration reads actual field, remains UI-only.
lib=(root/'blueprint_library.inc').read_text()
assert 'library_total_price(&library_entries[index], library_station_build_cost,' in lib
assert 'library_component_price(entry, &components)' in lib
assert 'library_entry_read(e, L"blueprint.json", library_bytes' in lib
assert 'TrackPieceDecorationDefinition' in lib
assert 'sourceStationDefinitionId' in lib
print('PASS: production template UI integration reads existing blueprint.json without editing payment or build paths')
