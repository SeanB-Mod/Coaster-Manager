#include "../coaster_selection_probe.c"
static unsigned mock_create_count,mock_submit_count,mock_settings,mock_visibility,mock_tags,mock_ready=1,mock_fail,mock_legal,mock_account_change,mock_setting_fail;
static int mock_result_code=1,mock_details_result=1;
static unsigned mock_details_count,mock_dependency_count;
static int mock_dependency_result=1;
static unsigned mock_dlc_count, mock_dlc_bad;
static int mock_dlc_result=1;
static WorkshopId mock_item=9876543210123456ULL,mock_updated;
static char mock_title[401],mock_description[2048];
static WorkshopId mock_owner(void*o){(void)o;return mock_account_change?99:76561198012345678ULL;}
static WorkshopBool mock_logged(void*o){(void)o;return 1;}
static WorkshopId mock_create(void*o,unsigned app,int type){(void)o;if(app!=2185060||type)return 0;++mock_create_count;return 10;}
static WorkshopId mock_update(void*o,unsigned app,WorkshopId id){(void)o;if(app!=2185060)return 0;mock_updated=id;return 20;}
static WorkshopBool mock_vis(void*o,WorkshopId h,int v){(void)o;(void)h;mock_visibility=(unsigned)v;return !mock_setting_fail;}
static WorkshopBool mock_tag(void*o,WorkshopId h,const WorkshopTags*t){(void)o;(void)h;mock_tags=(unsigned)t->count;return t->count==2&&t->strings&&equals(t->strings[0],"Templates")&&equals(t->strings[1],workshop_tag_type);}
static WorkshopBool mock_set_title(void*o,WorkshopId h,const char*s){(void)o;(void)h;library_copy(mock_title,sizeof(mock_title),s);++mock_settings;return 1;}
static WorkshopBool mock_set_description(void*o,WorkshopId h,const char*s){(void)o;(void)h;library_copy(mock_description,sizeof(mock_description),s);++mock_settings;return 1;}
static WorkshopBool mock_path(void*o,WorkshopId h,const char*s){(void)o;(void)h;++mock_settings;return s&&s[0];}
static WorkshopId mock_submit(void*o,WorkshopId h,const char*s){(void)o;(void)h;(void)s;++mock_submit_count;return 11;}
static WorkshopId mock_details(void*o,WorkshopId id,unsigned age){(void)o;(void)age;mock_updated=id;++mock_details_count;return 12;}
static WorkshopId mock_dependency(void*o,WorkshopId parent,WorkshopId child){(void)o;if(parent!=mock_item||child!=workshop_required_items[mock_dependency_count%WORKSHOP_REQUIRED_ITEM_COUNT])return 0;++mock_dependency_count;return 13+mock_dependency_count;}
static WorkshopId mock_app_dependency(void*o,WorkshopId item,unsigned app){(void)o;if(item!=mock_item||app!=WORKSHOP_RIDES_RELICS_APP)return 0;++mock_dlc_count;return 14;}
static WorkshopBool mock_completed(void*o,WorkshopId h,WorkshopBool*f){(void)o;(void)h;*f=(WorkshopBool)mock_fail;if(workshop_phase==4)workshop_completion_size=24;return (WorkshopBool)mock_ready;}
static WorkshopBool mock_result(void*o,WorkshopId h,void*out,int size,int callback,WorkshopBool*f){
 (void)o;(void)h;*f=0;
 if(callback==3414&&size==24){WorkshopAppDependency*r=out;r->result=mock_dlc_result;r->item=mock_item+(mock_dlc_bad==1);r->app=WORKSHOP_RIDES_RELICS_APP+(mock_dlc_bad==2);return 1;}
 if(callback==3412&&size==24){WorkshopDependency*r=out;r->result=mock_dependency_result;r->parent=mock_item;r->child=workshop_required_items[workshop_dependency_index];return 1;}
 if(callback==3402&&size==24){memset(out,0,24);WorkshopId id=mock_details_result==9?0:mock_item;unsigned app=2185060;memcpy(out,&id,8);memcpy((unsigned char*)out+8,&mock_details_result,4);memcpy((unsigned char*)out+20,&app,4);return 1;}
 if(callback==3403&&size==24){WorkshopCreated*r=out;r->result=mock_result_code;r->id=mock_item;r->legal=(WorkshopBool)mock_legal;return 1;}
 if(callback==3404&&size==16){WorkshopSubmitted*r=out;r->result=mock_result_code;r->id=mock_item;r->legal=(WorkshopBool)mock_legal;return 1;}return 0;
}
__declspec(dllexport) int SelectUpload(long long type){library_scan(type);if(!library_count)return 0;management_entry=library_entries[0];return 1;}
__declspec(dllexport) void SetupUpload(void){
 workshop_relaxed=0;workshop_persist_attempts=0;workshop_tag_type[0]=0;workshop_pipe=0;workshop_client=0;workshop_completion_ready=0;workshop_phase=0;workshop_due=0;workshop_ugc=workshop_utils=workshop_user=(void*)1;
 workshop_add_app_dependency=mock_app_dependency;workshop_add_dependency=mock_dependency;workshop_details=mock_details;workshop_create=mock_create;workshop_update=mock_update;workshop_visibility=mock_vis;workshop_tags=mock_tag;workshop_title=mock_set_title;workshop_description=mock_set_description;workshop_content=workshop_preview=mock_path;workshop_submit=mock_submit;workshop_completed=mock_completed;workshop_result=mock_result;workshop_owner=mock_owner;workshop_logged_on=mock_logged;
 mock_create_count=mock_submit_count=mock_settings=mock_visibility=mock_tags=mock_account_change=mock_setting_fail=mock_fail=mock_legal=0;mock_item=9876543210123456ULL;mock_result_code=1;mock_details_result=1;mock_details_count=mock_dependency_count=0;mock_dependency_result=1;mock_ready=1;mock_dlc_count=mock_dlc_bad=0;mock_dlc_result=1;
 library_copy(workshop_content_utf8,sizeof(workshop_content_utf8),"C:/isolated/content");library_copy(workshop_preview_utf8,sizeof(workshop_preview_utf8),"C:/isolated/preview.jpg");
}
__declspec(dllexport) int BeginUpload(void){
 if(workshop_phase)return 0;WorkshopId owner=0;if(!workshop_link(&management_entry,&workshop_item,&owner))return 0;workshop_account=mock_owner(0);
 if(workshop_item&&owner!=workshop_account)return 0;workshop_entry=management_entry;
 library_copy(workshop_tag_type,sizeof(workshop_tag_type),workshop_fixed_type(1));library_copy(workshop_type,sizeof(workshop_type),"Gearbox Glider");library_copy(workshop_text,sizeof(workshop_text),"test description");workshop_format_title(&workshop_entry,"Gearbox Glider",workshop_title_text,sizeof(workshop_title_text));return workshop_begin_prepared();
}
__declspec(dllexport) void TickUpload(void){workshop_due=0;workshop_tick();}
__declspec(dllexport) int UploadPhase(void){return workshop_phase;}
__declspec(dllexport) unsigned UploadCreates(void){return mock_create_count;}
__declspec(dllexport) unsigned UploadSubmits(void){return mock_submit_count;}
__declspec(dllexport) unsigned UploadVisibility(void){return mock_visibility;}
__declspec(dllexport) unsigned UploadTags(void){return mock_tags;}
__declspec(dllexport) WorkshopId UploadTarget(void){return mock_updated;}
__declspec(dllexport) void UploadFailure(int result,int legal,int ready,int account,int settings){mock_result_code=result;mock_legal=(unsigned)legal;mock_ready=(unsigned)ready;mock_account_change=(unsigned)account;mock_setting_fail=(unsigned)settings;}
__declspec(dllexport) int UploadRename(const char*s){return management_rename_entry(&management_entry,s);}
__declspec(dllexport) int UploadFormat(const char*name,const char*type,char*out,unsigned cap){
 LibraryEntry e={0};library_copy(e.name,sizeof(e.name),name);const char*stats[]={"$56,260","Great (73)","Bad (7)","1%","0%"};for(unsigned i=0;i<5;++i)library_copy(e.stats.value[i],48,stats[i]);return workshop_format(&e,type,"$56,260",out,cap);
}

/* Host test fixture: verify the production pricing function is the sole
 * source of the Steam description's displayed amount. */
__declspec(dllexport) int UploadFormatTotal(const char*track, int station,
                                           unsigned supports, unsigned decorated,
                                           char*out, unsigned cap) {
    int components = 0;
    char full[48] = {0};
    if (!blueprint_component_display_cost(supports, decorated, &components) ||
        !blueprint_template_total_cost(track, station, components, full, sizeof(full)))
        return 0;
    LibraryEntry e = {0};
    library_copy(e.name, sizeof(e.name), "The Blender");
    library_copy(e.stats.value[0], 48, track);
    library_copy(e.stats.value[1], 48, "Great (71)");
    library_copy(e.stats.value[2], 48, "Perfect (122)");
    library_copy(e.stats.value[3], 48, "39%");
    library_copy(e.stats.value[4], 48, "0%");
    return workshop_format(&e, "Revolver Coaster", full, out, cap);
}

__declspec(dllexport) int UploadJpegValid(const unsigned char*b,unsigned n){return workshop_jpeg_valid(b,n);}

__declspec(dllexport) void UploadDetailsResult(int result){mock_details_result=result;}
__declspec(dllexport) unsigned UploadChecks(void){return mock_details_count;}

static unsigned owned_frames,owned_freed,owned_reads,owned_bad_pipe,owned_at,owned_released,owned_user_released;
static WorkshopCallCompleted owned_notifications[2];
static int owned_primary(void){return 1;}
static void owned_frame(int pipe){if(pipe!=2)++owned_bad_pipe;++owned_frames;}
static WorkshopBool owned_next(int pipe,WorkshopCallbackMessage*m){if(pipe!=2)++owned_bad_pipe;if(owned_at>=2)return 0;m->callback=703;m->size=16;m->data=&owned_notifications[owned_at++];return 1;}
static void owned_free(int pipe){if(pipe!=2)++owned_bad_pipe;++owned_freed;}
static WorkshopBool owned_get(int pipe,WorkshopId call,void*out,int size,int cb,WorkshopBool*f){if(pipe!=2||call!=10||size!=24||cb!=3403)++owned_bad_pipe;++owned_reads;WorkshopCreated result={1,9876543210123456ULL,0};memcpy(out,&result,24);*f=0;return 1;}
static void owned_release_user(void*c,int pipe,int user){(void)c;if(user!=3||pipe!=2)++owned_bad_pipe;++owned_user_released;}
static WorkshopBool owned_release(void*c,int pipe){(void)c;if(pipe!=2)++owned_bad_pipe;++owned_released;return 1;}
__declspec(dllexport) int TestOwnedQueue(void){
 owned_frames=owned_freed=owned_reads=owned_bad_pipe=owned_at=owned_released=owned_user_released=0;
 workshop_primary_pipe=owned_primary;workshop_run_frame=owned_frame;workshop_next_callback=owned_next;workshop_free_callback=owned_free;workshop_get_call_result=owned_get;
 workshop_pipe=2;workshop_phase=1;workshop_completion_ready=0;
 owned_notifications[0]=(WorkshopCallCompleted){999,3403,24};owned_notifications[1]=(WorkshopCallCompleted){10,3403,24};
 WorkshopBool failed=0;if(!workshop_owned_completed(0,10,&failed)||failed||owned_frames!=1||owned_freed!=2||owned_reads!=1||owned_bad_pipe)return 0;
 WorkshopCreated result={0};if(!workshop_owned_result(0,10,&result,24,3403,&failed)||failed||result.result!=1||result.id!=9876543210123456ULL)return 0;
 /* Malformed completion cannot make us write beyond the owned result buffer. */
 owned_at=0;owned_notifications[1].size=65537;workshop_completion_ready=0;
 if(!workshop_owned_completed(0,10,&failed)||!failed||owned_reads!=1)return 0;
 workshop_pipe=1;workshop_completion_ready=0;unsigned before=owned_frames;
 if(workshop_owned_completed(0,10,&failed)||owned_frames!=before)return 0;
 workshop_pipe=2;workshop_pipe_user=3;workshop_client=(void*)1;workshop_release_user=owned_release_user;workshop_release_pipe=owned_release;
 workshop_release_connection();workshop_phase=0;
 return owned_released==1&&owned_user_released==1&&!owned_bad_pipe&&!workshop_pipe&&!workshop_client;
}

__declspec(dllexport) void UploadCreatedId(WorkshopId id){mock_item=id;}
__declspec(dllexport) void UploadIOFailure(int failed){mock_fail=(unsigned)failed;}

__declspec(dllexport) const char*UploadSentTitle(void){return mock_title;}

__declspec(dllexport) unsigned UploadDependencies(void){return mock_dependency_count;}
__declspec(dllexport) void UploadDependencyResult(int result){mock_dependency_result=result;}

__declspec(dllexport) const char* UploadTypeTag(void){return workshop_type;}
__declspec(dllexport) void UploadSetType(const char*s){library_copy(workshop_type,sizeof(workshop_type),s);}

__declspec(dllexport) void UploadAge(void){workshop_started=GetTickCount64()-600001;}
__declspec(dllexport) int UploadBusy(void){return workshop_busy();}
__declspec(dllexport) const char* UploadFixedType(int i){return workshop_fixed_type(i);}
__declspec(dllexport) void RetryUploadEntry(void){workshop_start(&management_entry);}

/* Real temporary files and owned connection counters verify timeout lifetime. */
__declspec(dllexport) void UploadRetainStage(const WCHAR *package, const WCHAR *jpeg) {
    unsigned i=0;
    for (; i<1099 && package[i]; ++i) workshop_package[i]=package[i];
    workshop_package[i]=0;
    for (i=0; i<1099 && jpeg[i]; ++i) workshop_jpeg[i]=jpeg[i];
    workshop_jpeg[i]=0;
    owned_released=owned_user_released=owned_bad_pipe=0;
    workshop_pipe=2; workshop_pipe_user=3; workshop_client=(void*)1;
    workshop_release_user=owned_release_user; workshop_release_pipe=owned_release;
}
__declspec(dllexport) int UploadConnectionRetained(void) {
    return workshop_pipe==2 && workshop_pipe_user==3 && workshop_client==(void*)1 &&
           !owned_released && !owned_user_released;
}
__declspec(dllexport) int UploadConnectionReleasedOnce(void) {
    return !workshop_pipe && !workshop_client && owned_released==1 &&
           owned_user_released==1 && !owned_bad_pipe;
}

__declspec(dllexport) unsigned UploadDLCs(void){return mock_dlc_count;}
__declspec(dllexport) void UploadDLCResult(int result,unsigned bad){mock_dlc_result=result;mock_dlc_bad=bad;}
__declspec(dllexport) void UploadDLCType(int index){library_copy(workshop_tag_type,sizeof(workshop_tag_type),workshop_fixed_type(index));}
__declspec(dllexport) int UploadNeedsDLC(int index){library_copy(workshop_tag_type,sizeof(workshop_tag_type),workshop_fixed_type(index));return workshop_requires_dlc();}

__declspec(dllexport) void UploadNoDLCExport(void){workshop_add_app_dependency=0;}
