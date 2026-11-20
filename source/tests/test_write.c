#define PREFLIGHT_TEST_ENTRY PreflightTestMain
#include "test_preflight.c"
#undef PREFLIGHT_TEST_ENTRY
static int floorContainMode, floorContainCalls;
static MC snapC;
static MO snapO,oldO,emptyO,actualO,oldSL,oldPL,emptySL,emptyPL,actualSL,actualPL;
static MO supportCopies[WRITE_MAX_SUPPORTS],pieceCopies[WRITE_MAX_SUPPORTS];
static MM snapshotMethod,revertMethod,calculationMethod,splineRefreshMethod;
static unsigned calculationCalls,splineRefreshCalls;static int calculationThrows,splineRefreshThrows;
static MM pendingMethod;
static int pendingCount;
static long long resolvedNormal;
#include "import_plan_fixture.h"
#include "import_extended_fixture.h"
static MC allowedArrayC,decorArrayC,decorDefinitionC;static MO allowedArrayO,decorArrayO,allowedDefs[2],decorBoxes[1],decorDefinitionO;
static int compatibilityMode;
static void*compatibility_find(void*image,const char*ns,const char*name){if(equals(ns,"System")&&equals(name,"Array"))return &arrayC;return pf_find(image,ns,name);}
static void plan_checksum(unsigned char*b,unsigned n){unsigned hash=write_hash(b,n-4);memcpy(b+n-4,&hash,4);}
static MC queueC,delayedC;static MO incomingO,processingO,immediateO,ticketO;static int queueFail;
static int writeMode,snapshotCalls,revertCalls,restoreCalls;
static void*queueItems[4];static int queueUseItems;
static void*write_box(void*c,void*v){if(boxIndex>=4096)ExitProcess(92);MO*o=&boxes[boxIndex++];o->cls=c;memcpy(o->bytes,v,(unsigned)((MC*)c)->size);return o;}
static void*write_new(void*c){if(c==&supportList)return &supportListObject;if(c==&pieceList)return &pieceListObject;ExitProcess(93);return 0;}
static void write_set_value(void*o,void*f,void*v){
  if(o==&snapO&&f==g_snapshot_flag_field&&v&&*(unsigned char*)v==0)++restoreCalls;
  if(writeMode==5&&o==&snapO&&f==mock_field(&snapC,"SnapshotData")&&snapshotCalls==1){MO*first=0;memcpy(&first,v,8);if(first==&supportListObject)return;}
  MF*field=f;memcpy(((MO*)o)->bytes+field->offset,v,(unsigned)field->type->size);
}
static void write_set_object(void*o,void*f,void*v){MF*field=f;memcpy(((MO*)o)->bytes+field->offset,&v,8);}
static void write_lists(MO*data,MO*s,MO*p){data->cls=&dataClass;memcpy(data->bytes,&s,8);memcpy(data->bytes+8,&p,8);s->cls=&supportList;p->cls=&pieceList;}
static void*write_mock_invoke(void*m,void*o,void**args,void**ex){
  *ex=0;
  if(m==&splineRefreshMethod){if(o||!args||args[0]!=&stationObject||!args[1]||!args[2]||*(int*)args[1]!=0||*(unsigned char*)args[2]!=1)ExitProcess(219);++splineRefreshCalls;if(splineRefreshThrows)*ex=(void*)1;return 0;}
  if(m==&calculationMethod){if(o||!args||!args[0]||!args[1]||((unsigned*)args[0])[0]!=12||((unsigned*)args[0])[1]!=34||*(unsigned char*)args[1]!=1)ExitProcess(213);++calculationCalls;if(calculationThrows)*ex=(void*)1;return 0;}
  if(m==&configC.rotationMethod){if(compatibilityMode==1){*ex=(void*)1;return 0;}return &allowedArrayO;}
  if(m==&arrayC.countMethod){boxedCount.cls=&i32;memcpy(boxedCount.bytes,&((MO*)o)->count,4);return &boxedCount;}
  if(m==&arrayC.itemMethod){if(compatibilityMode==2){*ex=(void*)1;return 0;}int i=*(int*)args[0];if(o==&allowedArrayO)return i>=0&&i<2?&allowedDefs[i]:0;if(o==&decorArrayO)return i==0?&decorBoxes[0]:0;ExitProcess(135);}
  if(m==&dbC.itemMethod)return &decorDefinitionO;
  if(m==&configC.itemMethod)return &definitionO;
  if(m==&definitionC.countMethod||m==&decorDefinitionC.countMethod){boxedCount.cls=&i64;if(o==&definitionO)memcpy(boxedCount.bytes,&resolvedNormal,8);else memcpy(boxedCount.bytes,((MO*)o)->bytes,8);return &boxedCount;}
  if(m==&queueC.countMethod){boxedCount.cls=&i32;int n=((MO*)o)->count;memcpy(boxedCount.bytes,&n,4);return &boxedCount;}
  if(m==&queueC.itemMethod){if(queueFail){*ex=(void*)1;return 0;}return queueUseItems&&o==&incomingO?queueItems[*(int*)args[0]]:((MO*)o)->item;}
  if(m==&pendingMethod){boxedCount.cls=&i32;memcpy(boxedCount.bytes,&pendingCount,4);return &boxedCount;}
  if(m==&snapshotMethod){
    if(o!=&contextO)ExitProcess(94);++snapshotCalls;
    MO*data=snapshotCalls==1?&emptyO:&actualO;
    memcpy(snapO.bytes+8,data->bytes,16);snapO.bytes[0]=1;
    if((writeMode==2&&snapshotCalls==1)||(writeMode==7&&snapshotCalls==2))*ex=(void*)1;
    if(writeMode==3&&snapshotCalls==1){unsigned ver=35;memcpy(stationObject.bytes+4,&ver,4);}
    return 0;
  }
  if(m==&revertMethod){if(o!=&busO||!args||!args[0]||((unsigned*)args[0])[0]!=12||((unsigned*)args[0])[1]!=34)ExitProcess(95);++revertCalls;if(writeMode==6)*ex=(void*)1;return 0;}
  if(m==&supportList.positionMethod||m==&pieceList.positionMethod){((MO*)o)->count=0;return 0;}
  if(m==&supportList.rotationMethod||m==&pieceList.rotationMethod){
    MO*list=o;MO*copies=m==&supportList.rotationMethod?supportCopies:pieceCopies;
    if(list->count>=WRITE_MAX_SUPPORTS||!args||!args[0])ExitProcess(96);
    MO*copy=&copies[list->count++];copy->cls=m==&supportList.rotationMethod?&support:&piece;
    memcpy(copy->bytes,args[0],(unsigned)copy->cls->size);return 0;
  }
  if(m==&supportList.countMethod||m==&pieceList.countMethod){boxedCount.cls=&i32;memcpy(boxedCount.bytes,&((MO*)o)->count,4);return &boxedCount;}
  if(m==&utilsC.countMethod){
    ++floorContainCalls;if(!args||args[0]!=&currentFloorO)ExitProcess(171);
    if(floorContainMode==2){*ex=(void*)1;return 0;}
    boxedCount.cls=floorContainMode==3?&i32:&boolean;boxedCount.bytes[0]=(unsigned char)(floorContainMode!=1);return &boxedCount;
  }
  if(m==&utilsC.itemMethod)return writeMode==4?&otherFloorO:&currentFloorO;
  /* Unexpected calls, including other game writers, are forbidden. */ExitProcess(97);return 0;
}
static unsigned char floor_instance(void*m){return m==&utilsC.countMethod?0:pf_instance(m);}
static void*write_param(void*m,unsigned n){
  if(m==&utilsC.countMethod)return n==0?&floorC:&vec;
  if(m==&configC.rotationMethod)return &boolean;
  if(m==&supportList.rotationMethod)return &support;if(m==&pieceList.rotationMethod)return &piece;
  return pf_param(m,n);
}
static void setup_write(void){
  pf_setup();boxIndex=0;dataClass.name="TPS.Game.TrackRideSnapshotData";dataClass.size=16;
  supportList.positionMethod=(MM){&supportList,&voidC,".ctor",0};pieceList.positionMethod=(MM){&pieceList,&voidC,".ctor",0};
  supportList.rotationMethod=(MM){&supportList,&voidC,"Add",1};pieceList.rotationMethod=(MM){&pieceList,&voidC,"Add",1};
  p_object_new=write_new;p_value_box=write_box;p_field_set_value=write_set_value;p_field_set_value_object=write_set_object;
  p_method_get_param=write_param;p_runtime_invoke=write_mock_invoke;
  add_field(&stationBase,"Entity",&entityC,0);g_entity_field=mock_field(&stationBase,"Entity");
  snapC.name="TPS.Game.ECTrackRideSnapshot";add_field(&snapC,"HasSnapshotData",&boolean,0);add_field(&snapC,"SnapshotData",&dataClass,8);
  g_snapshot_flag_field=mock_field(&snapC,"HasSnapshotData");snapO.cls=&snapC;
  write_lists(&oldO,&oldSL,&oldPL);write_lists(&emptyO,&emptySL,&emptyPL);write_lists(&actualO,&actualSL,&actualPL);
}
void TestMain(void){
  setup_write();float target[3]={-50,0,-21},q[4];memcpy(q,write_source_rotation,16);
  if(!write_target_frame(999,target,q)||write_target_frame(WRITE_SOURCE_ID,target,q)||!write_target_frame(999,write_source_position,q))ExitProcess(1);
  q[1]=0;q[3]=1;if(!write_target_frame(999,target,q))ExitProcess(2);memcpy(q,write_source_rotation,16);
  target[1]=0.5f;if(!write_target_frame(999,target,q))ExitProcess(3);float raised[7][3];if(!write_plan_points(target,raised))ExitProcess(201);for(unsigned i=0;i<7;++i)if(raised[i][1]!=0.5f)ExitProcess(202);target[1]=0;if(!write_target_frame(999,target,q))ExitProcess(131);
  float points[7][3];if(!write_plan_points(target,points))ExitProcess(4);
  PFRoots roots={0};void*plan=write_make_data(&dataClass,points,123456789ULL,&roots);if(!plan)ExitProcess(5);
  if(supportListObject.count!=7||pieceListObject.count!=7)ExitProcess(6);
  for(int i=0;i<7;++i){
    unsigned long long floor=0;int section=-1;float pos[3];unsigned rotation[4];long long id=0;
    mock_field_value(&supportCopies[i],mock_field(&support,"SectionIndex"),&section);if(section!=i)ExitProcess(7);
    mock_field_value(&supportCopies[i],mock_field(&support,"Position"),pos);for(unsigned k=0;k<3;++k)if(pos[k]!=points[i][k])ExitProcess(8);
    mock_field_value(&supportCopies[i],mock_field(&support,"Rotation"),rotation);for(unsigned k=0;k<4;++k)if(rotation[k]!=write_fixture[i].rotation[k])ExitProcess(9);
    mock_field_value(&supportCopies[i],mock_field(&support,"FloorAreaInstance"),&floor);if(floor!=123456789ULL)ExitProcess(10);
    mock_field_value(&pieceCopies[i],mock_field(&piece,"TrackPieceDefinitionID"),&id);if(id!=WRITE_NORMAL_ID)ExitProcess(11);
    mock_field_value(&pieceCopies[i],mock_field(&piece,"TrackPieceDecorationDefinition"),&floor);if(floor)ExitProcess(12);
  }
  void*dataField=mock_field(&snapC,"SnapshotData");
  for(writeMode=0;writeMode<=8;++writeMode){
    unsigned entity[2]={12,34};memcpy(stationObject.bytes,entity,8);memcpy(snapO.bytes+8,oldO.bytes,16);snapO.bytes[0]=0;
    emptySL.count=emptyPL.count=writeMode==1?1:0;actualSL.count=actualPL.count=writeMode==8?6:7;
    write_attempted=0;snapshotCalls=revertCalls=restoreCalls=0;
    unsigned mark=roots.n;
    int ok=write_apply_plan(&snapshotMethod,&revertMethod,&busO,&contextO,&stationObject,entity,&currentFloorO,points,&snapO,dataField,&oldO,0,plan,&dataClass,&roots);
    if(ok!=(writeMode==0||writeMode==7||writeMode==8))ExitProcess(20+(unsigned)writeMode);
    if(ok){int verified=write_verify_live(&snapshotMethod,&contextO,&snapO,dataField,&dataClass,&roots);if(verified!=(writeMode==0))ExitProcess(30+(unsigned)writeMode);}
    int shouldRevert=writeMode==0||writeMode>=6;if(revertCalls!=shouldRevert)ExitProcess(40+(unsigned)writeMode);
    if(!shouldRevert){MO*s=0,*p=0;memcpy(&s,snapO.bytes+8,8);memcpy(&p,snapO.bytes+16,8);if(s!=&oldSL||p!=&oldPL||snapO.bytes[0]||restoreCalls!=1)ExitProcess(60+(unsigned)writeMode);}
    else if(restoreCalls)ExitProcess(70+(unsigned)writeMode);
    int before=revertCalls;if(write_apply_plan(&snapshotMethod,&revertMethod,&busO,&contextO,&stationObject,entity,&currentFloorO,points,&snapO,dataField,&oldO,0,plan,&dataClass,&roots)||revertCalls!=before)ExitProcess(82);
    while(roots.n>mark)p_gchandle_free(roots.h[--roots.n]);
  }
  /* Verification must not replace the edit-session cancellation baseline.
     Restoring the baseline changes no live support list or reconstruction queue. */
  writeMode=0;actualSL.count=actualPL.count=7;
  for(unsigned char initialFlag=0;initialFlag<2;++initialFlag){snapshotCalls=1;int before=revertCalls;
    if(!write_verify_live(&snapshotMethod,&contextO,&snapO,dataField,&dataClass,&roots))ExitProcess(210);
    if(!write_restore_snapshot(&snapO,dataField,&oldO,initialFlag))ExitProcess(211);
    MO*baselineSupports=0,*baselinePieces=0;memcpy(&baselineSupports,snapO.bytes+8,8);memcpy(&baselinePieces,snapO.bytes+16,8);
    if(baselineSupports!=&oldSL||baselinePieces!=&oldPL||snapO.bytes[0]!=initialFlag||actualSL.count!=7||actualPL.count!=7||revertCalls!=before)ExitProcess(212);
  }
  pf_release(&roots);if(liveHandles)ExitProcess(83);
  support.fields[0].flags=0x10;PFRoots rejected={0};if(write_make_data(&dataClass,points,123,&rejected))ExitProcess(84);pf_release(&rejected);
  int count=-1;pendingCount=0;if(!write_queue_count(&pendingMethod,&busO,&count)||count)ExitProcess(85);
  pendingCount=3;if(!write_queue_count(&pendingMethod,&busO,&count)||count!=3)ExitProcess(86);
  pendingCount=-1;if(write_queue_count(&pendingMethod,&busO,&count))ExitProcess(87);
  pendingCount=100001;if(write_queue_count(&pendingMethod,&busO,&count))ExitProcess(88);
  if(write_raise_method(&busC,&eventC,&rejected)||rejected.n)ExitProcess(89);
  write_pending_roots.n=0;if(!pf_root(&write_pending_roots,&busO))ExitProcess(98);
  write_pending=1;write_queued_tick=GetTickCount64()-10001;service_pending_write();write_release_retry();if(write_pending||liveHandles)ExitProcess(99);
  if(!pf_root(&write_pending_roots,&busO))ExitProcess(100);
  write_pending_bus=&busO;write_pending_station=&stationObject;
  write_pending_entity[0]=12;write_pending_entity[1]=999;write_pending=1;write_queued_tick=GetTickCount64();g_hook_count=2;write_queued_callback=0;
  write_pending_ticket=0;service_pending_write();write_release_retry();if(write_pending||liveHandles)ExitProcess(102);
  queueC.name="System.Collections.Generic.List<TPS.Core.GameEvents.IDelayedGameEvent>";
  delayedC.name="TPS.Core.GameEvents.IDelayedGameEvent";
  queueC.countMethod=(MM){&queueC,&i32,"get_Count",0};queueC.itemMethod=(MM){&queueC,&delayedC,"get_Item",1};
  incomingO.cls=processingO.cls=immediateO.cls=&queueC;ticketO.cls=&delayedC;
  add_field(&busC,"_raisedEvents",&queueC,8);add_field(&busC,"_raisingEvents",&queueC,16);add_field(&busC,"_raisedImmediateEvents",&queueC,24);
  add_field(&busC,"_processingEvents",&boolean,32);add_field(&busC,"_processingEvent",&boolean,33);
  MO*ptr=&incomingO;memcpy(busO.bytes+8,&ptr,8);ptr=&processingO;memcpy(busO.bytes+16,&ptr,8);ptr=&immediateO;memcpy(busO.bytes+24,&ptr,8);
  /* A stat notification must not force closing construction. Unknown or
   * duplicate track operations must still block, including after submission. */
  MC statC={0},trackC={0};MO statO={0},trackO={0},secondTrackO={0};
  statC.name="TPS.Core.GameEvents.DelayedGameEvent<TPS.Game.GameEvent.MuseumEvent.StatChanged>";
  trackC.name="TPS.Core.GameEvents.DelayedGameEvent<TPS.Game.GameEvent.TrackRide.TrackRevert>";
  statO.cls=&statC;trackO.cls=secondTrackO.cls=&trackC;
  incomingO.count=processingO.count=immediateO.count=0;void*submitted=0;
  if(!write_import_queue_ready(&busO,0)||write_import_queue_ready(&busO,&submitted))ExitProcess(210);
  incomingO.count=1;incomingO.item=&statO;
  if(!write_import_queue_ready(&busO,0))ExitProcess(211);
  incomingO.item=&ticketO;if(write_import_queue_ready(&busO,0))ExitProcess(212);
  queueUseItems=1;incomingO.count=2;queueItems[0]=&statO;queueItems[1]=&trackO;
  if(write_import_queue_ready(&busO,0)||!write_import_queue_ready(&busO,&submitted)||submitted!=&trackO)ExitProcess(213);
  queueItems[0]=&secondTrackO;if(write_import_queue_ready(&busO,&submitted))ExitProcess(214);
  queueItems[0]=&ticketO;if(write_import_queue_ready(&busO,&submitted))ExitProcess(215);
  queueItems[0]=&statO;queueFail=1;if(write_import_queue_ready(&busO,&submitted))ExitProcess(216);queueFail=0;
  busO.bytes[32]=1;if(write_import_queue_ready(&busO,&submitted))ExitProcess(217);busO.bytes[32]=0;
  queueUseItems=0;incomingO.count=0;processingO.count=1;processingO.item=&trackO;
  if(write_import_queue_ready(&busO,0)||write_import_queue_ready(&busO,&submitted))ExitProcess(218);
  processingO.item=&statO;immediateO.count=1;immediateO.item=&statO;
  if(!write_import_queue_ready(&busO,0))ExitProcess(219);
  processingO.count=immediateO.count=0;
  int total=0,active=0;incomingO.count=3;incomingO.item=&ticketO;
  if(!write_queue_state(&busO,&ticketO,&total,&active,0)||total!=3||!active)ExitProcess(103);
  incomingO.item=&delegateO;
  if(!write_queue_state(&busO,&ticketO,&total,&active,0)||total!=3||active)ExitProcess(104);
  processingO.count=1;processingO.item=&ticketO;
  if(!write_queue_state(&busO,&ticketO,&total,&active,0)||!active)ExitProcess(105);
  processingO.count=0;immediateO.count=1;immediateO.item=&ticketO;
  if(!write_queue_state(&busO,&ticketO,&total,&active,0)||!active)ExitProcess(106);
  busO.bytes[32]=1;if(write_queue_state(&busO,&ticketO,&total,&active,0))ExitProcess(107);busO.bytes[32]=0;
  busO.bytes[33]=1;if(write_queue_state(&busO,&ticketO,&total,&active,0))ExitProcess(108);busO.bytes[33]=0;
  queueFail=1;if(write_queue_state(&busO,&ticketO,&total,&active,0))ExitProcess(109);queueFail=0;
  incomingO.count=257;if(write_queue_state(&busO,&ticketO,&total,&active,0))ExitProcess(110);
  unsigned n=sizeof(import_plan_fixture);memcpy(write_plan_bytes,import_plan_fixture,n);
  if(!write_decode_selection(write_plan_bytes,n)||write_count!=7||WRITE_SOURCE_ID!=28044)ExitProcess(111);
  if(write_decode_selection(write_plan_bytes,n-1))ExitProcess(112);
  write_plan_bytes[0]^=1;if(write_decode_selection(write_plan_bytes,n))ExitProcess(113);write_plan_bytes[0]^=1;
  write_plan_bytes[n-1]^=1;if(write_decode_selection(write_plan_bytes,n))ExitProcess(114);write_plan_bytes[n-1]^=1;
  unsigned zeroCount=0;memcpy(write_plan_bytes+8,&zeroCount,4);plan_checksum(write_plan_bytes,n);if(write_decode_selection(write_plan_bytes,n))ExitProcess(115);
  memcpy(write_plan_bytes,import_plan_fixture,n);unsigned nan=0x7fc00000;memcpy(write_plan_bytes+60,&nan,4);plan_checksum(write_plan_bytes,n);if(write_decode_selection(write_plan_bytes,n))ExitProcess(116);
  memcpy(write_plan_bytes,import_plan_fixture,n);long long wrongId=123;memcpy(write_plan_bytes+24,&wrongId,8);plan_checksum(write_plan_bytes,n);if(!write_decode_selection(write_plan_bytes,n)||WRITE_NORMAL_ID!=123)ExitProcess(117);
  memcpy(write_plan_bytes,import_plan_fixture,n);unsigned reserved=1;memcpy(write_plan_bytes+12,&reserved,4);plan_checksum(write_plan_bytes,n);if(write_decode_selection(write_plan_bytes,n))ExitProcess(118);
  memcpy(write_plan_bytes,import_plan_fixture,n);unsigned maxCount=512;memcpy(write_plan_bytes+8,&maxCount,4);
  for(unsigned i=1;i<512;++i)memcpy(write_plan_bytes+60+i*32,write_plan_bytes+60,32);
  n=64+512*32;plan_checksum(write_plan_bytes,n);if(!write_decode_selection(write_plan_bytes,n)||write_count!=512)ExitProcess(119);
  static float maximumPoints[WRITE_MAX_SUPPORTS][3];if(!write_plan_points(target,maximumPoints))ExitProcess(120);
  PFRoots maximumRoots={0};support.fields[0].flags=0;boxIndex=0;void*maximumPlan=write_make_data(&dataClass,maximumPoints,777,&maximumRoots);
  if(!maximumPlan||supportListObject.count!=512||pieceListObject.count!=512)ExitProcess(121);pf_release(&maximumRoots);
  maxCount=513;memcpy(write_plan_bytes+8,&maxCount,4);plan_checksum(write_plan_bytes,n);if(write_decode_selection(write_plan_bytes,n))ExitProcess(122);
  memcpy(write_plan_bytes,import_plan_fixture,sizeof(import_plan_fixture));unsigned one=1;memcpy(write_plan_bytes+8,&one,4);n=96;plan_checksum(write_plan_bytes,n);
  if(!write_decode_selection(write_plan_bytes,n)||write_count!=1)ExitProcess(123);
  boxIndex=0;if(!write_make_data(&dataClass,maximumPoints,777,&maximumRoots)||supportListObject.count!=1||pieceListObject.count!=1)ExitProcess(124);pf_release(&maximumRoots);
  if(liveHandles)ExitProcess(125);
  static WCHAR selectedPath[512];if(!coaster_storage_path(selectedPath,512,L"Diagnostics",1))ExitProcess(126);unsigned selectedPos=0;while(selectedPath[selectedPos])++selectedPos;if(!append(selectedPath,512,&selectedPos,L"\\SelectedBlueprint.tpmplan"))ExitProcess(126);
  HANDLE planFile=CreateFileW(selectedPath,0x40000000,0,0,2,FILE_ATTRIBUTE_NORMAL,0);DWORD written=0;
  if(planFile==INVALID_HANDLE_VALUE||!WriteFile(planFile,import_plan_fixture,sizeof(import_plan_fixture),&written,0)||written!=sizeof(import_plan_fixture))ExitProcess(126);CloseHandle(planFile);
  if(!write_load_selection()||write_count!=7)ExitProcess(127);
  planFile=CreateFileW(selectedPath,0x40000000,0,0,2,FILE_ATTRIBUTE_NORMAL,0);
  if(planFile==INVALID_HANDLE_VALUE||!WriteFile(planFile,import_plan_fixture,1,&written,0))ExitProcess(128);CloseHandle(planFile);
  if(write_load_selection()||write_count!=7)ExitProcess(129);
  DeleteFileW(selectedPath);if(write_load_selection())ExitProcess(130);
  definitionC.name="TPS.Game.TrackPieceDefinition";configC.itemMethod=(MM){&configC,&definitionC,"GetNormalTrackPiece",0};definitionC.countMethod=(MM){&definitionC,&i64,"get_ID",0};
  PFRoots compatibilityRoots={0};WRITE_NORMAL_ID=-133003145;resolvedNormal=-133003145;
  if(!write_normal_matches(&configO,&compatibilityRoots))ExitProcess(132);pf_release(&compatibilityRoots);
  resolvedNormal=-115856756;if(write_normal_matches(&configO,&compatibilityRoots))ExitProcess(133);pf_release(&compatibilityRoots);
  if(liveHandles)ExitProcess(134);
  p_class_from_name=compatibility_find;arrayC.name="System.Array";
  arrayC.countMethod=(MM){&arrayC,&i32,"get_Length",0};arrayC.itemMethod=(MM){&arrayC,&objectC,"GetValue",1};
  allowedArrayC.name="TPS.Game.TrackPieceDefinition[]";allowedArrayC.parent=&arrayC;decorArrayC.name="TPS.Game.GameItemDefinitionID[]";decorArrayC.parent=&arrayC;
  allowedArrayO.cls=&allowedArrayC;allowedArrayO.count=2;decorArrayO.cls=&decorArrayC;decorArrayO.count=1;
  configC.rotationMethod=(MM){&configC,&allowedArrayC,"GetTrackPieces",1};
  decorDefinitionC.name="TPS.Game.GameItemDefinition";decorDefinitionO.cls=&decorDefinitionC;dbC.itemMethod=(MM){&dbC,&decorDefinitionC,"Get",1};
  add_field(&configC,"MaxHeight",&i32,8);add_field(&configC,"MaxBankingAngle",&i32,12);add_field(&configC,"MaxTangentModifier",&single,16);add_field(&configC,"TrackDecorations",&decorArrayC,24);
  int heightLimit=8,bankLimit=45;float tangentLimit=.5f;memcpy(configO.bytes+8,&heightLimit,4);memcpy(configO.bytes+12,&bankLimit,4);memcpy(configO.bytes+16,&tangentLimit,4);MO*decorPointer=&decorArrayO;memcpy(configO.bytes+24,&decorPointer,8);
  add_field(&configC,"ToggleTrackPiece",&i64,32);add_field(&configC,"ChainTrackPiece",&i64,40);add_field(&configC,"BoosterTrackPiece",&i64,48);
  decorDefinitionC.countMethod=(MM){&decorDefinitionC,&i64,"get_ID",0};
  long long normalId=-133003145,specialId=-1978012938,decorIdValue=-2122730988;
  allowedDefs[0].cls=allowedDefs[1].cls=&definitionC;memcpy(allowedDefs[0].bytes,&normalId,8);memcpy(allowedDefs[1].bytes,&specialId,8);
  decorBoxes[0].cls=&decorId;memcpy(decorBoxes[0].bytes,&decorIdValue,8);memcpy(decorDefinitionO.bytes,&decorIdValue,8);
  if(!write_decode_selection(import_extended_fixture,sizeof(import_extended_fixture))||WRITE_NORMAL_ID!=0||write_fixture[2].height!=3||write_fixture[3].height!=4||write_fixture[1].piece!=specialId||write_fixture[4].decoration!=decorIdValue)ExitProcess(136);
  PFRoots extendedRoots={0};if(!write_configuration_matches(&configO,&extendedRoots))ExitProcess(137);pf_release(&extendedRoots);
  allowedArrayO.count=1;memcpy(configO.bytes+40,&specialId,8);
  if(!write_configuration_matches(&configO,&extendedRoots))ExitProcess(151);pf_release(&extendedRoots);
  long long absent=0;memcpy(configO.bytes+40,&absent,8);if(write_configuration_matches(&configO,&extendedRoots))ExitProcess(152);pf_release(&extendedRoots);allowedArrayO.count=2;
  memcpy(decorDefinitionO.bytes,&normalId,8);if(write_configuration_matches(&configO,&extendedRoots))ExitProcess(153);pf_release(&extendedRoots);memcpy(decorDefinitionO.bytes,&decorIdValue,8);
  write_fixture[1].piece=123;if(write_configuration_matches(&configO,&extendedRoots))ExitProcess(138);pf_release(&extendedRoots);write_fixture[1].piece=specialId;
  write_fixture[4].decoration=123;if(write_configuration_matches(&configO,&extendedRoots))ExitProcess(139);pf_release(&extendedRoots);write_fixture[4].decoration=decorIdValue;
  heightLimit=2;memcpy(configO.bytes+8,&heightLimit,4);if(write_configuration_matches(&configO,&extendedRoots))ExitProcess(140);pf_release(&extendedRoots);heightLimit=8;memcpy(configO.bytes+8,&heightLimit,4);
  tangentLimit=.1f;memcpy(configO.bytes+16,&tangentLimit,4);if(write_configuration_matches(&configO,&extendedRoots))ExitProcess(141);pf_release(&extendedRoots);tangentLimit=.5f;memcpy(configO.bytes+16,&tangentLimit,4);
  for(compatibilityMode=1;compatibilityMode<=2;++compatibilityMode){if(write_configuration_matches(&configO,&extendedRoots))ExitProcess(142);pf_release(&extendedRoots);}compatibilityMode=0;
  float banking=50;memcpy(&write_fixture[0].modifiers[2],&banking,4);if(write_configuration_matches(&configO,&extendedRoots))ExitProcess(143);pf_release(&extendedRoots);banking=0;memcpy(&write_fixture[0].modifiers[2],&banking,4);
  memcpy(q,write_source_rotation,16);if(!write_target_frame(999,target,q)||!write_plan_points(target,points))ExitProcess(144);
  boxIndex=0;if(!write_make_data(&dataClass,points,777,&extendedRoots))ExitProcess(145);
  for(int i=0;i<7;++i){int h=0;unsigned mods[3];long long pid=0,did=0;unsigned dp=0;
    mock_field_value(&supportCopies[i],mock_field(&support,"Height"),&h);if(h!=write_fixture[i].height)ExitProcess(146);
    for(unsigned k=0;k<3;++k){mock_field_value(&supportCopies[i],mock_field(&support,supportSchema[8+k].name),&mods[k]);if(mods[k]!=write_fixture[i].modifiers[k])ExitProcess(147);}
    mock_field_value(&pieceCopies[i],mock_field(&piece,"TrackPieceDefinitionID"),&pid);mock_field_value(&pieceCopies[i],mock_field(&piece,"TrackPieceDecorationDefinition"),&did);mock_field_value(&pieceCopies[i],mock_field(&piece,"DecorationPosition"),&dp);
    if(pid!=write_fixture[i].piece||did!=write_fixture[i].decoration||dp!=write_fixture[i].decorationPosition)ExitProcess(148);
  }
  pf_release(&extendedRoots);if(liveHandles)ExitProcess(149);
  memcpy(write_plan_bytes,import_extended_fixture,sizeof(import_extended_fixture));int invalidHeight=0;memcpy(write_plan_bytes+92,&invalidHeight,4);plan_checksum(write_plan_bytes,sizeof(import_extended_fixture));if(write_decode_selection(write_plan_bytes,sizeof(import_extended_fixture)))ExitProcess(150);
  memcpy(write_plan_bytes,import_extended_fixture,sizeof(import_extended_fixture));unsigned reservedV2=1;memcpy(write_plan_bytes+128,&reservedV2,4);plan_checksum(write_plan_bytes,sizeof(import_extended_fixture));if(write_decode_selection(write_plan_bytes,sizeof(import_extended_fixture)))ExitProcess(154);
  memcpy(write_plan_bytes,import_extended_fixture,sizeof(import_extended_fixture));long long zeroPiece=0;memcpy(write_plan_bytes+108,&zeroPiece,8);plan_checksum(write_plan_bytes,sizeof(import_extended_fixture));if(write_decode_selection(write_plan_bytes,sizeof(import_extended_fixture)))ExitProcess(155);
  /* Stats refresh binds only the verified station and does not alter the
     cancellation snapshot, copy saved stats, or submit another TrackRevert. */
  unsigned refreshEntity[2]={12,34},wrongEntity[2]={12,35};memcpy(stationObject.bytes,refreshEntity,8);int priorReverts=revertCalls;
  memcpy(snapO.bytes+8,oldO.bytes,16);snapO.bytes[0]=0;
  if(write_recalculation_dispatch(&stationObject,wrongEntity,&calculationMethod,&splineRefreshMethod)||calculationCalls)ExitProcess(214);
  if(!write_recalculation_dispatch(&stationObject,refreshEntity,&calculationMethod,&splineRefreshMethod)||calculationCalls!=1||splineRefreshCalls!=1||revertCalls!=priorReverts)ExitProcess(215);
  for(unsigned i=0;i<16;++i)if(snapO.bytes[8+i]!=oldO.bytes[i])ExitProcess(216);if(snapO.bytes[0])ExitProcess(217);
  calculationThrows=1;if(write_recalculation_dispatch(&stationObject,refreshEntity,&calculationMethod,&splineRefreshMethod)||calculationCalls!=2||splineRefreshCalls!=1)ExitProcess(218);calculationThrows=0;
  splineRefreshThrows=1;if(write_recalculation_dispatch(&stationObject,refreshEntity,&calculationMethod,&splineRefreshMethod)||calculationCalls!=3||splineRefreshCalls!=2)ExitProcess(220);splineRefreshThrows=0;
  /* Multiple successful operations re-arm only after roots are released. */
  for(int operation=0;operation<3;++operation){
    write_attempted=1;write_pending=1;write_pending_bus=&busO;write_pending_context=&contextO;
    if(!pf_root(&write_pending_roots,&busO))ExitProcess(156);
    write_finish_pending(1);
    if(write_attempted||write_pending||write_pending_roots.n||liveHandles||write_pending_bus||write_pending_context||write_pending_ticket)ExitProcess(157);
  }
  incomingO.count=processingO.count=immediateO.count=0;
  write_attempted=1;placement_attempted=1;placement_import_pending=0;
  write_retain_retry(&busO);if(liveHandles!=1||!write_retry_idle())ExitProcess(167);
  incomingO.count=1;incomingO.item=&ticketO;if(write_retry_idle())ExitProcess(168);incomingO.count=0;
  busO.bytes[32]=1;if(write_retry_idle())ExitProcess(169);busO.bytes[32]=0;
  incomingO.count=1;queueFail=1;if(write_retry_idle())ExitProcess(170);queueFail=0;incomingO.count=0;
  if(placement_reset_failed(0,1,1)||placement_reset_failed(1,0,1)||!placement_attempted||!write_attempted||liveHandles!=1)ExitProcess(171);
  if(!placement_reset_failed(1,1,0)||!placement_attempted||!write_attempted||liveHandles!=1)ExitProcess(172);
  write_pending=1;if(placement_reset_failed(1,1,1))ExitProcess(173);write_pending=0;
  placement_import_pending=1;if(placement_reset_failed(1,1,1))ExitProcess(174);placement_import_pending=0;
  if(!pf_root(&placement_wait_roots,&stationObject))ExitProcess(175);placement_wait_item=&stationObject;write_selection_frozen=1;
  if(!placement_reset_failed(1,1,1)||placement_attempted||write_attempted||placement_wait_item||write_selection_frozen||liveHandles||write_retry_bus)ExitProcess(176);
  write_attempted=1;write_pending=1;write_pending_bus=&busO;if(!pf_root(&write_pending_roots,&busO))ExitProcess(158);write_finish_pending(0);
  if(!write_attempted||write_pending||liveHandles!=1||write_retry_bus!=&busO)ExitProcess(159);
  if(!write_retry_idle())ExitProcess(165);
  write_release_retry();if(liveHandles)ExitProcess(166);
  unsigned requests=write_count;run_first_write(&stationObject);if(write_count!=(int)requests||!write_attempted)ExitProcess(160);
  write_attempted=0;write_pending=1;run_first_write(&stationObject);if(write_count!=(int)requests||write_attempted||write_pending!=1)ExitProcess(161);write_pending=0;
  memcpy(write_plan_bytes,import_extended_fixture,sizeof(import_extended_fixture));write_plan_bytes[7]='3';
  long long stationDefinition=-123456789;memcpy(write_plan_bytes+24,&stationDefinition,8);plan_checksum(write_plan_bytes,sizeof(import_extended_fixture));
  if(!write_decode_selection(write_plan_bytes,sizeof(import_extended_fixture))||write_station_definition_id!=stationDefinition||write_normal_id)ExitProcess(162);
  stationDefinition=0;memcpy(write_plan_bytes+24,&stationDefinition,8);plan_checksum(write_plan_bytes,sizeof(import_extended_fixture));
  if(write_decode_selection(write_plan_bytes,sizeof(import_extended_fixture)))ExitProcess(163);
  if(!write_decode_selection(import_extended_fixture,sizeof(import_extended_fixture))||write_station_definition_id)ExitProcess(164);
  /* Topmost overlay is not a floor-boundary failure when the target contains it. */
  p_method_is_instance=floor_instance;
  utilsC.countMethod=(MM){&utilsC,&boolean,"ContainsWorldPosition",2};
  static float floorPoints[2][3]={{0,0,0},{1,30,1}};write_count=2;writeMode=4;
  floorContainMode=0;floorContainCalls=0;
  if(!write_same_floor_points(&currentFloorO,floorPoints)||floorContainCalls!=2)ExitProcess(172);
  for(floorContainMode=1;floorContainMode<=3;++floorContainMode){floorContainCalls=0;
    if(write_same_floor_points(&currentFloorO,floorPoints)||floorContainCalls!=1)ExitProcess(173);
  }
  utilsC.countMethod.name=0;
  if(write_same_floor_points(&currentFloorO,floorPoints))ExitProcess(174);
  writeMode=0;floorContainCalls=0;
  if(!write_same_floor_points(&currentFloorO,floorPoints)||floorContainCalls)ExitProcess(175);
  ExitProcess(0);
}




