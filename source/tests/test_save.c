#include "../coaster_selection_probe.c"
static int save_test_unchanged=1;
__declspec(dllexport) int StageSave(const unsigned char*b,unsigned n,const char*name,unsigned long long source,long long definition){
  if(save_stage_owned||n>=JSON_CAPACITY||!library_copy(save_name,sizeof(save_name),name)||!save_normalize_name(save_name))return 0;
  library_copy(save_coaster_name,sizeof(save_coaster_name),"Test Coaster");save_source=source;save_definition=definition;
  unsigned hash=write_hash((const unsigned char*)write_fixture,sizeof(write_fixture));int before_count=write_count;unsigned long long before_source=write_source_id;long long before_type=write_station_definition_id;
  memcpy(jbuf,b,n);jpos=n;blueprint_save_export_request=1;save_phase=1;blueprint_save_exported();blueprint_save_export_request=0;
  save_test_unchanged=write_hash((const unsigned char*)write_fixture,sizeof(write_fixture))==hash&&write_count==before_count&&write_source_id==before_source&&write_station_definition_id==before_type;
  return save_phase==2;
}
__declspec(dllexport) const WCHAR*StagePath(void){return save_stage;}
__declspec(dllexport) const WCHAR*PhotoPath(void){return save_photo;}
__declspec(dllexport) const WCHAR*FinalPath(void){return save_final;}
__declspec(dllexport) int FinishSave(void){return save_finish_stage();}
__declspec(dllexport) void DiscardSave(void){save_discard_stage();save_phase=0;}
__declspec(dllexport) int CheckPng(const unsigned char*b,unsigned n){return save_complete_png(b,n);}
__declspec(dllexport) int PlanUnchanged(void){return save_test_unchanged;}

/* Exercise production restoration with retained, destroyed and throwing canvases. */
static unsigned capture_restored,capture_freed;
static void*capture_target(GCHandle h){return h==(GCHandle)3?0:(void*)(UINT_PTR)h;}
static void capture_free(GCHandle h){(void)h;++capture_freed;}
static void*capture_invoke(void*m,void*o,void**a,void**ex){
  if(m!=(void*)99||!a||*(unsigned char*)a[0]!=1){*ex=(void*)1;return 0;}
  ++capture_restored;if(o==(void*)2)*ex=(void*)1;return 0;
}
__declspec(dllexport) int TestRestoreUi(void){
  fn_gchandle_get_target oldTarget=p_gchandle_get_target;fn_gchandle_free oldFree=p_gchandle_free;fn_runtime_invoke oldInvoke=p_runtime_invoke;
  p_gchandle_get_target=capture_target;p_gchandle_free=capture_free;p_runtime_invoke=capture_invoke;
  capture_restored=capture_freed=0;save_canvas_setter=(void*)99;save_ui_hidden=1;
  save_hidden_canvases[0].n=3;save_hidden_canvases[0].h[0]=(GCHandle)1;save_hidden_canvases[0].h[1]=(GCHandle)2;save_hidden_canvases[0].h[2]=(GCHandle)3;
  save_hidden_canvases[1].n=1;save_hidden_canvases[1].h[0]=(GCHandle)4;
  save_restore_ui();int ok=capture_restored==3&&capture_freed==4&&!save_ui_hidden&&!save_canvas_setter;
  save_restore_ui();ok=ok&&capture_restored==3&&capture_freed==4;
  p_gchandle_get_target=oldTarget;p_gchandle_free=oldFree;p_runtime_invoke=oldInvoke;return ok;
}


__declspec(dllexport) void SetStats(const unsigned char*b,unsigned n){unsigned root=lj_parse(b,n);blueprint_stats_read(root,&save_display_stats);}

__declspec(dllexport) void SeedPreviewCurve(int enabled){save_curve_count=enabled?CURVE_PREVIEW_MAX:0;for(unsigned i=0;i<save_curve_count;++i){save_curve[i][0]=(float)i/10;save_curve[i][1]=(float)(i%20);save_curve[i][2]=-(float)i/10;}}
__declspec(dllexport) unsigned ReadPreviewCurve(const unsigned char*b,unsigned n){unsigned root=lj_parse(b,n);library_read_preview_curve(root);return footprint_curve_count;}

__declspec(dllexport) int OwnerExpired(unsigned long long now,unsigned long long seen,int alive){return save_ui_owner_expired(now,seen,alive);}

__declspec(dllexport) void SeedColour(int custom) {
 colour_saved=(CoasterColour){1,(unsigned char)custom,{0.125f,0.25f,0.5f,1}};
}
