"""Compile production Workshop description formatter and full-price math as a Linux host test.
No Steam access, user save access or Windows game execution.
"""
from pathlib import Path
import json, re, subprocess, tempfile
root = Path(__file__).resolve().parents[1]
entry = json.loads((Path(__file__).with_name('fixtures') / 'blender-cost-entry.json').read_text())
bp = json.loads((Path(__file__).with_name('fixtures') / 'blender-cost-counts.json').read_text())
assert entry['name'] == 'The Blender'
supports = len(bp['TrackSupportData'])
decorations = sum(int(x['TrackPieceDecorationDefinition']['_id']) != 0 for x in bp['TrackPieceData'])
assert supports == 16 and decorations == 1

def fn(src, name):
    m=re.search(r'static (?:inline )?(?:void|int)\s+'+re.escape(name)+r'\s*\(',src)
    assert m,name
    brace=src.index('{',m.start()); depth=0
    for i in range(brace,len(src)):
        if src[i]=='{':depth+=1
        elif src[i]=='}':
            depth-=1
            if depth==0:return src[m.start():i+1]
    raise AssertionError(name)

stats=(root/'blueprint_display_stats.inc').read_text()
workshop=(root/'workshop_upload.inc').read_text()
prod='\n'.join(fn(stats,n) for n in ('blueprint_cost_text','blueprint_track_cost_number','blueprint_component_display_cost','blueprint_template_total_cost'))
prod+='\n'+ '\n'.join(fn(workshop,n) for n in ('workshop_append','workshop_format'))
source='''
#include <string.h>
#include <stdio.h>
#include <assert.h>
#define WRITE_MAX_SUPPORTS 512U
static unsigned length(const char *s) {return (unsigned)strlen(s);}
typedef struct {char value[5][48];} BlueprintDisplayStats;
typedef struct {char name[401]; BlueprintDisplayStats stats;} LibraryEntry;
static void blueprint_label_stats(BlueprintDisplayStats *s) {(void)s;}
'''+prod+'''
int main(void) {
 LibraryEntry e={0};
 strcpy(e.name,"The Blender");
 strcpy(e.stats.value[0],"$51,670");
 strcpy(e.stats.value[1],"Great (71)");
 strcpy(e.stats.value[2],"Perfect (122)");
 strcpy(e.stats.value[3],"39%");
 strcpy(e.stats.value[4],"0%");
 int extras=-1;
 assert(blueprint_component_display_cost(16,1,&extras)&&extras==8500);
 char all[48]={0};
 assert(blueprint_template_total_cost(e.stats.value[0],45000,extras,all,sizeof(all)));
 assert(!strcmp(all,"$105,170"));
 char description[2048]={0};
 assert(workshop_format(&e,"Revolver Coaster",all,description,sizeof(description)));
 assert(strstr(description,"[b]Cost:[/b] = $105,170\\n[b]Buzz[/b] = Great (71)"));
 assert(strstr(description,"[H1]The Blender - Revolver Coaster[/H1]"));
 assert(!strstr(description,"[b]Cost:[/b] = $51,670"));
 assert(!strcmp(e.stats.value[0],"$51,670")); /* track-only metadata preserved */
 assert(!workshop_format(&e,"Revolver Coaster",0,description,sizeof(description)));
 assert(!workshop_format(&e,"Revolver Coaster",all,description,8));
 puts("PASS: production Workshop description includes $105,170; saved track-only metadata retained; malformed cost rejected");
 return 0;
}
'''
with tempfile.TemporaryDirectory() as td:
    p=Path(td); (p/'src.c').write_text(source)
    subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'src.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
text=(root/'workshop_upload.inc').read_text()
assert 'library_total_price(&workshop_entry, library_station_build_cost,' in text
assert 'workshop_description(workshop_ugc, update, workshop_text)' in text
assert 'coaster_export_to(e, workshop_content_dir)' in text
assert 'workshop_format(&workshop_entry, workshop_type, uploadTotal' in text
print('PASS: Steam create/update paths use the same frozen full price; no billing/template serialization changes')
