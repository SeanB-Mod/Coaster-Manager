#include "../coaster_selection_probe.c"
__declspec(dllexport) unsigned PrepareLibrary(const unsigned char*b,unsigned n,unsigned char*out){
  if(n>4U*1024U*1024U)return 0;unsigned root=lj_parse(b,n);unsigned size=root?library_prepare(root,out):0;
  return size&&write_decode_selection(out,size)?size:0;
}
__declspec(dllexport) int ParseLibrary(const unsigned char*b,unsigned n){return lj_parse(b,n)!=0;}
__declspec(dllexport) void HashLibrary(const unsigned char*b,unsigned n,char*out){library_sha(b,n,out);}
__declspec(dllexport) int ImageLibrary(const unsigned char*b,unsigned n){unsigned w=0,h=0;return library_image_size(b,n,&w,&h);}
__declspec(dllexport) unsigned ScanLibrary(long long definition){library_scan(definition);return library_count;}
__declspec(dllexport) const char*NameLibrary(unsigned index){return index<library_count?library_entries[index].name:0;}
__declspec(dllexport) int ReadOnlyLibrary(unsigned index){return index<library_count?library_entries[index].readOnly:-1;}
__declspec(dllexport) int ManualLibrary(unsigned index){return index<library_count&&library_entries[index].package[0]!=0;}
__declspec(dllexport) int SourceLibrary(unsigned index){return index<library_count?library_entries[index].source:-1;}
__declspec(dllexport) unsigned ScanWorkshopLibrary(const WCHAR*root,long long definition){library_scan(definition);coaster_workshop_scan_at(root,definition);return library_count;}
__declspec(dllexport) int ResolveWorkshopLibrary(const WCHAR*exe,WCHAR*out){return coaster_workshop_resolve(exe,out);}
__declspec(dllexport) unsigned ReadEntryLibrary(unsigned index,int photo,unsigned char*out,unsigned capacity){return index<library_count?library_entry_read(&library_entries[index],photo?L"preview.png":L"blueprint.json",out,capacity):0;}

__declspec(dllexport) int LoadLibrary(unsigned index,long long definition){library_index=index;browser_definition=definition;return library_load_plan();}

__declspec(dllexport) int DirectLibrary(unsigned index,long long definition){
  library_index=index;browser_definition=definition;if(!library_load_plan())return 0;
  long long before=write_station_definition_id;unsigned long long source=write_source_id;int count=write_count;
  write_library_selection=1;int ok=write_load_selection()&&write_station_definition_id==before&&write_source_id==source&&write_count==count;
  write_library_selection=0;return ok;
}

__declspec(dllexport) unsigned ListFirst(unsigned count,float position){return library_visible_first(count,position);}

__declspec(dllexport) int StatsLabel(const unsigned char*b,unsigned n,char*out,unsigned capacity){unsigned root=lj_parse(b,n);BlueprintDisplayStats stats;blueprint_stats_read(root,&stats);blueprint_stats_label(&stats,out,capacity);return root!=0;}
__declspec(dllexport) void CostText(int value,char*out){blueprint_cost_text(value,out);}
__declspec(dllexport) int TemplateTotalCost(const char*track,int station,int components,char*out,unsigned capacity){return blueprint_template_total_cost(track,station,components,out,capacity);}
__declspec(dllexport) int ComponentsCost(unsigned supports,unsigned decorations,int*out){return blueprint_component_display_cost(supports,decorations,out);}
__declspec(dllexport) int ComponentPriceLibrary(unsigned index,int*out){return index<library_count?library_component_price(&library_entries[index],out):0;}

__declspec(dllexport) int RatingNumber(const char*s,float*out,char*number){return blueprint_rating_number(s,out,number);}

__declspec(dllexport) unsigned CurveLibrary(void){return footprint_curve_count;}

__declspec(dllexport) int ColourLibrary(const unsigned char *b,unsigned n,float *rgba) {
 unsigned root=lj_parse(b,n);
 if(!root||!library_read_colour(root))return -1;
 memcpy(rgba,colour_selected.rgba,16);
 return colour_selected.present ? (colour_selected.custom?2:1):0;
}

__declspec(dllexport) unsigned ScanCachedLibrary(long long definition) { library_open_scan(definition); return library_count; }
__declspec(dllexport) void InvalidateCatalogue(void) { library_catalogue_invalidate(); }
__declspec(dllexport) void ExpireCatalogue(void) { library_catalogue_until = GetTickCount64(); }

__declspec(dllexport) unsigned WorkshopFolders(void) { return coaster_workshop_scanned_folders; }
__declspec(dllexport) void DropDisplayedCatalogue(void) { library_count = 0; memset(library_entries, 0, sizeof(library_entries)); }

__declspec(dllexport) unsigned WorkshopPackages(void) { return coaster_workshop_scanned_packages; }
__declspec(dllexport) unsigned LocalEntriesSeen(void) { return library_local_count; }
__declspec(dllexport) unsigned ManualEntriesSeen(void) { return coaster_manual_seen; }

__declspec(dllexport) int ScanBytesFit(unsigned used, unsigned file) { return library_scan_bytes_fit(used,file); }
