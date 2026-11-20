"""Exercise production native JSON parser and price counting with real blueprint fixtures."""
from pathlib import Path
import json, copy, re, subprocess, tempfile
srcroot=Path(__file__).resolve().parents[1]
jsonsrc=(srcroot/'library_json.h').read_text()
js=jsonsrc[:jsonsrc.index('static int lj_floats(')]
component=(srcroot/'blueprint_display_stats.inc').read_text()
library=(srcroot/'blueprint_library.inc').read_text()
def func(src,name):
    m=re.search(r'static (?:void|int)\s+'+re.escape(name)+r'\s*\(',src)
    assert m,name
    a=src.index('{',m.start()); level=0
    for i in range(a,len(src)):
        if src[i]=='{':level+=1
        elif src[i]=='}':
            level-=1
            if not level:return src[m.start():i+1]
    raise ValueError(name)
pricefunc=func(component,'blueprint_component_display_cost')
readfunc=func(library,'library_component_price')
program='''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#define WRITE_MAX_SUPPORTS 512U
static int equals(const char*a,const char*b){return a&&b&&!strcmp(a,b);}
static unsigned length(const char*s){return (unsigned)strlen(s);}
'''+js+'''
static unsigned char library_bytes[4U*1024U*1024U+1];
typedef struct {
    long long definition;
    int componentsPrice;
    unsigned char componentsPriceChecked;
    unsigned char componentsPriceValid;
} LibraryEntry;
static unsigned stored_n;
static unsigned library_entry_read(const LibraryEntry*e, const wchar_t *file,unsigned char*out,unsigned capacity){
 (void)e;(void)file;
 if(stored_n>capacity)return 0;
 memcpy(out,library_bytes,stored_n);return stored_n;
}
'''+pricefunc+readfunc+'''
int main(int argc,char**argv){
 if(argc!=3)return 2;
 FILE*f=fopen(argv[1],"rb");if(!f)return 3;
 stored_n=(unsigned)fread(library_bytes,1,sizeof(library_bytes)-1,f);fclose(f);
 LibraryEntry e={0}; e.definition=-1872241465LL;
 int p=-1; int ok=library_component_price(&e,&p);
 if(ok)printf("%d\\n",p);else puts("invalid");
 int second=-1; int cached=library_component_price(&e,&second);
 if(ok!=cached||(ok&&second!=p))return 4;
 if(!strcmp(argv[2],"valid")!=ok)return 5;
 return 0;
}
'''
base=json.loads((Path(__file__).with_name('fixtures') / 'blender-cost-counts.json').read_text())
with tempfile.TemporaryDirectory() as td:
    td=Path(td);source=td/'test.c';exe=td/'test'
    source.write_text(program)
    subprocess.run(['gcc','-std=c11','-O1','-w','-fsanitize=undefined',str(source),'-o',str(exe)],check=True)
    def check(data,expected):
        path=td/'fixture.json';path.write_text(json.dumps(data,separators=(',',':')),encoding='utf8')
        p=subprocess.run([str(exe),str(path),'valid' if expected is not None else 'invalid'],check=True,text=True,capture_output=True)
        result=int(p.stdout.strip()) if expected is not None else None
        assert result==expected,(result,expected)
    check(base,8500)
    b=copy.deepcopy(base);b['TrackPieceData'][7]['TrackPieceDecorationDefinition']['_id']='0';check(b,8000)
    b=copy.deepcopy(base);b['TrackPieceData'][0]['TrackPieceDecorationDefinition']['_id']='7';check(b,9000)
    b=copy.deepcopy(base);b.pop('supportCount');b.pop('pieceCount');check(b,8500)
    b=copy.deepcopy(base);b['supportCount']=17;check(b,None)
    b=copy.deepcopy(base);b['TrackPieceData'][1]['TrackPieceDecorationDefinition']['_id']='unknown';check(b,None)
    b=copy.deepcopy(base);b['TrackPieceData'].pop();check(b,None)
    b=copy.deepcopy(base);b['sourceStationDefinitionId']='-55';check(b,None)
    b=copy.deepcopy(base);b['TrackSupportData']=[];b['TrackPieceData']=[];b['supportCount']=0;b['pieceCount']=0;check(b,None)
    print('PASS: production JSON parser + component-price reader on real Blender, missing optional counters, decorated variants and malformed blueprints')
