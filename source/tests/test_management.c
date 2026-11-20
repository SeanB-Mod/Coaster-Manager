#include "../coaster_selection_probe.c"
__declspec(dllexport) int ManageSelect(long long definition,unsigned index){library_scan(definition);if(index>=library_count)return 0;management_entry=library_entries[index];return 1;}
__declspec(dllexport) int ManageReadOnly(void){return management_entry.readOnly;}
__declspec(dllexport) int ManageRename(const char*name){return management_rename_entry(&management_entry,name);}
__declspec(dllexport) int ManageDelete(void){return management_delete_entry(&management_entry);}
__declspec(dllexport) int ManageStartPhoto(void){return management_start_photo(&management_entry);}
__declspec(dllexport) int ManageFinishPhoto(void){return management_finish_photo(&management_entry);}
__declspec(dllexport) const WCHAR*ManagePhotoPath(void){return management_photo;}
__declspec(dllexport) void ManageDiscardPhoto(void){management_discard_photo();}

__declspec(dllexport) int ManageExport(const WCHAR*directory){return coaster_export_to(&management_entry,directory);}
__declspec(dllexport) const WCHAR*ExportResult(void){return coaster_export_result;}
__declspec(dllexport) int ManageSelectWorkshop(const WCHAR*root,long long definition,unsigned index){library_scan(definition);coaster_workshop_scan_at(root,definition);if(index>=library_count)return 0;management_entry=library_entries[index];return management_entry.source==2;}

__declspec(dllexport) int RetakeIdentity(unsigned long long saved,long long definition,unsigned long long current,long long currentDefinition,unsigned index,unsigned generation){unsigned entity[2]={index,generation};return retake_identity_matches(saved,definition,current,currentDefinition,entity);}
