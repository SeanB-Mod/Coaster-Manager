#define TestMain ExportTestMain
#include "test_export.c"
#undef TestMain
static MC busC,dictC,listC,eventC,typeC,objectC,delegateC,multiC,typedDelegateC,contextC,entityC,reflectionC,voidC,eventRefC,arrayC;
static MO busO,dictO,listO,keyO,delegateO,contextO,reflectionO;
static MC configC,dbC,definitionC,floorC,utilsC;
static MO configO,definitionO,currentFloorO,otherFloorO;
static int queryMode,queryCalls;
static int pfMode,reads;
static void*pf_domain(void){return (void*)1;}
static void**pf_assemblies(void*d,SIZE_T*n){static void*a[1]={(void*)1};(void)d;*n=1;return a;}
static void*pf_image(void*a){return a;}
static SIZE_T pf_image_count(void*i){(void)i;return 1;}
static void*pf_image_class(void*i,SIZE_T n){(void)i;return n?0:&eventC;}
static const char*pf_class_name(void*c){return c==&eventC?"TrackRevert":"Other";}
static void*pf_find(void*image,const char*ns,const char*name){(void)image;
  if(equals(ns,"TPS.Game")){if(equals(name,"LevelDatabaseUtils"))return &dbC;if(equals(name,"FloorAreaUtils"))return &utilsC;}
  if(!equals(ns,"System"))return 0;if(equals(name,"Delegate"))return &delegateC;if(equals(name,"MulticastDelegate"))return &multiC;return 0;
}
static void*pf_key(void*t){(void)t;return &keyO;}
static void*pf_param(void*m,unsigned n){if(m==&dictC.itemMethod)return &typeC;if(m==&contextC.itemMethod)return &eventRefC;if(m==&dbC.itemMethod)return &decorId;if(m==&utilsC.itemMethod)return n==0?&vec:&boolean;return &i32;}
static unsigned char pf_instance(void*m){return m!=&dbC.itemMethod&&m!=&utilsC.itemMethod;}
static void*pf_reflection(void*o){return o==&reflectionO&&pfMode!=5?&contextC.itemMethod:0;}
static void*pf_mock_invoke(void*m,void*o,void**args,void**ex){
  *ex=0;
  if(m==&dictC.itemMethod){if(pfMode==1){*ex=(void*)1;return 0;}return &listO;}
  if(m==&listC.countMethod){int v=pfMode==2?4097:(pfMode==3?2:1);if(pfMode==7&&reads)v=2;memcpy(boxedCount.bytes,&v,4);boxedCount.cls=&i32;return &boxedCount;}
  if(m==&listC.itemMethod){++reads;if(pfMode==4){*ex=(void*)1;return 0;}if(!args)return 0;return &delegateO;}
  if(m==&delegateC.countMethod)return &reflectionO;
  if(m==&dbC.itemMethod){++queryCalls;if(queryMode==1){*ex=(void*)1;return 0;}return queryMode==2?&boxedCount:&definitionO;}
  if(m==&stationBase.floorMethod)return queryMode==3?0:&currentFloorO;
  if(m==&stationBase.worldPositionMethod){if(queryMode==4){*ex=(void*)1;return 0;}return &stationPositionBox;}
  if(m==&utilsC.itemMethod){++queryCalls;if(!args||*(unsigned char*)args[1]||*(unsigned char*)args[2])ExitProcess(91);if(queryMode==5){*ex=(void*)1;return 0;}return queryMode==6?&otherFloorO:&currentFloorO;}
  (void)o;/* An unexpected invocation could be a writer: fail the test. */ExitProcess(90);return 0;
}
static void pf_setup(void){
  init();
  busC.name="TPS.Core.GameEvents";dictC.name="System.Collections.Generic.Dictionary<System.Type,System.Collections.IList>";
  listC.name="System.Collections.Generic.List<TPS.Core.GameEvents.EventDelegate<TPS.Game.GameEvent.TrackRide.TrackRevert>>";
  eventC.name="TPS.Game.GameEvent.TrackRide.TrackRevert";typeC.name="System.Type";objectC.name="System.Object";
  delegateC.name="System.Delegate";multiC.name="System.MulticastDelegate";multiC.parent=&delegateC;
  typedDelegateC.name="TPS.Core.GameEvents.EventDelegate<TPS.Game.GameEvent.TrackRide.TrackRevert>";typedDelegateC.parent=&multiC;
  contextC.name="TPS.Game.ECPTrackRide.<>c__DisplayClass20_0";contextC.parent=&objectC;
  entityC.name="Unity.Entities.Entity";entityC.size=8;reflectionC.name="System.Reflection.MethodInfo";
  voidC.name="System.Void";eventRefC.name="TPS.Game.GameEvent.TrackRide.TrackRevert&";arrayC.name="System.Delegate[]";
  stationBase.name="TPS.Game.GameItemInstance";stationObject.cls=&stationBase;
  add_field(&busC,"_delegates",&dictC,0);add_field(&delegateC,"m_target",&objectC,0);add_field(&multiC,"delegates",&arrayC,8);
  add_field(&contextC,"trackRideEntity",&entityC,0);add_field(&contextC,"trackRideItem",&stationBase,8);
  dictC.itemMethod=(MM){&dictC,0,"get_Item",1};static MC ilist;ilist.name="System.Collections.IList";dictC.itemMethod.ret=&ilist;
  listC.countMethod=(MM){&listC,&i32,"get_Count",0};listC.itemMethod=(MM){&listC,&typedDelegateC,"get_Item",1};
  delegateC.countMethod=(MM){&delegateC,&reflectionC,"get_Method",0};contextC.itemMethod=(MM){&contextC,&voidC,"<SetComponents>g__OnTrackRevert|13",1};
  busO.cls=&busC;dictO.cls=&dictC;listO.cls=&listC;delegateO.cls=&typedDelegateC;contextO.cls=&contextC;reflectionO.cls=&reflectionC;keyO.cls=&typeC;
  MO*p=&dictO;memcpy(busO.bytes,&p,8);p=&contextO;memcpy(delegateO.bytes,&p,8);p=&stationObject;memcpy(contextO.bytes+8,&p,8);
  unsigned entity[2]={12,34};memcpy(contextO.bytes,entity,8);
  p_domain_get=pf_domain;p_domain_get_assemblies=pf_assemblies;p_assembly_get_image=pf_image;p_class_from_name=pf_find;
  p_image_get_class_count=pf_image_count;p_image_get_class=pf_image_class;p_class_get_name=pf_class_name;
  p_type_get_object=pf_key;p_method_get_param=pf_param;p_method_get_from_reflection=pf_reflection;p_runtime_invoke=pf_mock_invoke;p_method_is_instance=pf_instance;
  configC.name="TPS.Game.TrackRideConfig";dbC.name="TPS.Game.LevelDatabaseUtils";definitionC.name="TPS.Game.GameItemDefinition";
  configO.cls=&configC;definitionO.cls=&definitionC;add_field(&configC,"SupportDefinition",&decorId,0);
  long long definitionId=123;memcpy(configO.bytes,&definitionId,8);dbC.itemMethod=(MM){&dbC,&definitionC,"Get",1};
  floorC.name="TPS.Game.FloorAreaInstance";currentFloorO.cls=&floorC;otherFloorO.cls=&floorC;
  stationBase.floorMethod=(MM){&stationBase,&floorC,"get_FloorArea",0};stationBase.worldPositionMethod=(MM){&stationBase,&vec,"get_WorldPosition",0};
  stationPositionBox.cls=&vec;utilsC.name="TPS.Game.FloorAreaUtils";utilsC.itemMethod=(MM){&utilsC,&floorC,"GetTopMostFloorAreaAtWorldPosition",3};
}
static void pf_case(int modeNumber,const unsigned*entity,const char*expected,int pass){
  pfMode=modeNumber;reads=0;PFRoots roots={0};const char*status=0;
  void*c=pf_binding(&busO,&busC,&eventC,&stationObject,entity,&roots,&status);
  if(!!c!=pass||!equals(status,expected))ExitProcess((unsigned)(10+modeNumber));
  pf_release(&roots);if(liveHandles)ExitProcess(80);
}
#ifndef PREFLIGHT_TEST_ENTRY
#define PREFLIGHT_TEST_ENTRY TestMain
#endif
void PREFLIGHT_TEST_ENTRY(void){
  pf_setup();unsigned entity[2]={12,34};
  if(pf_event_class()!=&eventC)ExitProcess(9);
  pf_case(0,entity,"pass-exact-entity-item-handler",1);
  pf_case(1,entity,"track-revert-list-missing-or-getter-unavailable",0);
  pf_case(2,entity,"listener-count-unavailable-or-over-limit",0);
  pf_case(3,entity,"duplicate-entity-contexts",0);
  pf_case(4,entity,"listener-read-failed",0);
  pf_case(5,entity,"item-or-handler-mismatch",0);
  entity[1]=35;pf_case(6,entity,"no-matching-entity-context",0);entity[1]=34;
  pf_case(7,entity,"listener-list-changed",0);
  MO*wrong=&supportObject;memcpy(contextO.bytes+8,&wrong,8);pf_case(8,entity,"item-or-handler-mismatch",0);
  wrong=&stationObject;memcpy(contextO.bytes+8,&wrong,8);
  wrong=&delegateO;memcpy(delegateO.bytes+8,&wrong,8);pf_case(9,entity,"multicast-listener-not-supported",0);
  for(queryMode=0;queryMode<3;++queryMode){
    int ok=pf_definition(&configO,&configC,"SupportDefinition","TPS.Game.GameItemDefinitionID","TPS.Game.GameItemDefinition");
    if(ok!=(queryMode==0))ExitProcess(40+(unsigned)queryMode);
  }
  queryMode=0;queryCalls=0;memset(configO.bytes,0,8);
  if(pf_definition(&configO,&configC,"SupportDefinition","TPS.Game.GameItemDefinitionID","TPS.Game.GameItemDefinition")||queryCalls)ExitProcess(44);
  const char*floorStatuses[]={"pass-station-floor-matches-world-query","skip-station-floor-or-world-position-unavailable","skip-station-floor-or-world-position-unavailable","skip-query-null-exception-or-signature","skip-world-query-differs-from-station-floor"};
  const int modes[]={0,3,4,5,6};
  for(unsigned i=0;i<5;++i){queryMode=modes[i];PFRoots roots={0};const char*status=pf_floor(&stationObject,&roots);if(!equals(status,floorStatuses[i]))ExitProcess(50+i);pf_release(&roots);if(liveHandles)ExitProcess(81);}
  utilsC.itemMethod.params=2;queryMode=0;PFRoots roots={0};queryCalls=0;
  if(!equals(pf_floor(&stationObject,&roots),"skip-query-null-exception-or-signature")||queryCalls)ExitProcess(60);pf_release(&roots);
  /* Full listener binding on the reviewed 12.1 closure, including rejection paths. */
  memset(delegateO.bytes+8,0,8);
  contextC.name="TPS.Game.ECPTrackRide.<>c__DisplayClass15_0";
  contextC.itemMethod.name="<SetComponents>g__OnTrackRevert|14";
  pf_case(0,entity,"pass-exact-entity-item-handler",1);
  pf_case(3,entity,"duplicate-entity-contexts",0);
  pf_case(5,entity,"item-or-handler-mismatch",0);
  entity[1]=35;pf_case(6,entity,"no-matching-entity-context",0);entity[1]=34;
  ExitProcess(0);
}
