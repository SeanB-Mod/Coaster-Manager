#define TestMain ExportTestMain
#include "test_export.c"
#undef TestMain
static unsigned originalCalls,contextCalls;
static void*expectedSelf,*expectedMethod;
static void entry_original(void*self,void*method){
  if(self!=expectedSelf||method!=expectedMethod)ExitProcess(201);++originalCalls;
}
static void*entry_context(void){++contextCalls;return 0;}
static MC stationDefinitionClass;
static MO stationDefinitionObject,stationDefinitionBox;
static int definitionError;
static void*definition_invoke(void*m,void*o,void**args,void**ex){
  (void)args;*ex=0;
  if(m==&stationBase.countMethod&&o==&stationObject){
    if(definitionError==1){*ex=(void*)1;return 0;}
    return definitionError==2?&boxedCount:&stationDefinitionObject;
  }
  if(m==&stationDefinitionClass.countMethod&&o==&stationDefinitionObject){
    if(definitionError==3){*ex=(void*)1;return 0;}
    return &stationDefinitionBox;
  }
  ExitProcess(214);return 0;
}
static MM placeMethod;
static unsigned dispatchCalls;
static int dispatchException;
static void*dispatch_invoke(void*m,void*o,void**args,void**ex){
  if(m!=&placeMethod||o!=&stationObject||args)ExitProcess(206);
  ++dispatchCalls;*ex=dispatchException?(void*)1:0;return 0;
}
static unsigned arrowCalls;
static int arrowException;
static void*arrow_invoke(void*m,void*o,void**args,void**ex){
  if(m!=&placeMethod||o||!args||!args[0]||!args[1]||
     ((unsigned*)args[0])[0]!=123||((unsigned*)args[0])[1]!=456||*(unsigned char*)args[1])ExitProcess(220);
  ++arrowCalls;*ex=arrowException?(void*)1:0;return 0;
}
static MC browserMock,voidClass;static MO browserObject,browserBool;
static unsigned browserInvokes;static int browserException;
static void*browser_param(void*m,unsigned index){(void)m;(void)index;return &boolean;}
static void*browser_invoke(void*m,void*o,void**args,void**ex){
  if(o!=&browserObject)ExitProcess(227);++browserInvokes;*ex=0;
  if(m==&browserMock.countMethod){if(args)ExitProcess(228);return &browserBool;}
  if(m==&browserMock.itemMethod){
    if(!args||!args[0]||*(unsigned char*)args[0])ExitProcess(229);
    if(browserException)*ex=(void*)1;else browserBool.bytes[0]=0;return 0;
  }
  ExitProcess(230);return 0;
}
static MC rowC,menuC,dataC,availableC,blueprintC,buttonC;
static MO rowO,menuO,dataO,availableO,blueprintO,buttonO,trueO;
static int rowError;
static void*row_domain(void){return (void*)1;}
static void**row_assemblies(void*d,SIZE_T*n){static void*a[1]={(void*)1};(void)d;*n=1;return a;}
static void*row_image(void*a){return a;}
static void*row_find(void*i,const char*ns,const char*name){(void)i;return equals(ns,"TPS.Game")&&equals(name,"GameItemDefinition")?&stationDefinitionClass:0;}
static void*row_invoke(void*m,void*o,void**args,void**ex){
  (void)args;*ex=0;
  if(m==&rowC.countMethod||m==&buttonC.countMethod||m==&buttonC.itemMethod){
    trueO.bytes[0]=(unsigned char)(rowError!=1);return &trueO;
  }
  if(m==&availableC.countMethod){if(rowError==2){*ex=(void*)1;return 0;}return &blueprintO;}
  if(m==&blueprintC.countMethod)return &stationDefinitionObject;
  if(m==&stationDefinitionClass.countMethod&&o==&stationDefinitionObject)return &stationDefinitionBox;
  ExitProcess(217);return 0;
}
static void row_tests(void){
  p_domain_get=row_domain;p_domain_get_assemblies=row_assemblies;p_assembly_get_image=row_image;p_class_from_name=row_find;p_runtime_invoke=row_invoke;
  rowC.name="TPS.Game.UI.RideBuildingMenu_Item";menuC.name="TPS.Game.UI.RideBuildingMenu";dataC.name="TPS.Game.UI.RideBuildingMenu.ItemData";
  availableC.name="TPS.Game.AvailableExhibitRideBlueprint";blueprintC.name="TPS.Game.ExhibitRideBlueprintDefinition";buttonC.name="TPS.Core.UI.UIButton";
  rowO.cls=&rowC;menuO.cls=&menuC;dataO.cls=&dataC;availableO.cls=&availableC;blueprintO.cls=&blueprintC;buttonO.cls=&buttonC;trueO.cls=&boolean;
  rowC.countMethod=(MM){&rowC,&boolean,"get_isActiveAndEnabled",0};buttonC.countMethod=(MM){&buttonC,&boolean,"get_isActiveAndEnabled",0};buttonC.itemMethod=(MM){&buttonC,&boolean,"get_interactable",0};
  availableC.countMethod=(MM){&availableC,&blueprintC,"get_Definition",0};blueprintC.countMethod=(MM){&blueprintC,&stationDefinitionClass,"get_ItemDefinition",0};
  add_field(&rowC,"_parentMenu",&menuC,0);add_field(&rowC,"_currentData",&dataC,8);add_field(&rowC,"_buttonBuyRide",&buttonC,16);
  MO*o=&menuO;set_value(&rowO,"_parentMenu",&o,8);o=&dataO;set_value(&rowO,"_currentData",&o,8);o=&buttonO;set_value(&rowO,"_buttonBuyRide",&o,8);
  add_field(&dataC,"AvailalableExhibitRideBlueprint",&availableC,0);add_field(&dataC,"CanAfford",&boolean,8);add_field(&dataC,"BuildCost",&i32,12);
  o=&availableO;set_value(&dataO,"AvailalableExhibitRideBlueprint",&o,8);unsigned char yes=1;set_value(&dataO,"CanAfford",&yes,1);int cost=35000;set_value(&dataO,"BuildCost",&cost,4);
  add_field(&blueprintC,"NumRequiredPieces",&i32,0);add_field(&availableC,"PiecesCollected",&i32,0);int pieces=37;set_value(&blueprintO,"NumRequiredPieces",&pieces,4);set_value(&availableO,"PiecesCollected",&pieces,4);
  PFRoots roots={0};long long id=-123456789;
  if(!funfair_row_matches(&rowO,&menuO,id,&roots)||!funfair_can_buy(&rowO))ExitProcess(218);pf_release(&roots);
  if(funfair_row_matches(&rowO,&menuO,id+1,&roots))ExitProcess(219);pf_release(&roots);
  if(funfair_row_matches(&rowO,&stationObject,id,&roots))ExitProcess(220);pf_release(&roots);
  rowError=1;if(funfair_row_matches(&rowO,&menuO,id,&roots)||funfair_can_buy(&rowO))ExitProcess(221);pf_release(&roots);
  rowError=2;if(funfair_row_matches(&rowO,&menuO,id,&roots))ExitProcess(222);pf_release(&roots);rowError=0;
  --pieces;set_value(&availableO,"PiecesCollected",&pieces,4);if(funfair_row_matches(&rowO,&menuO,id,&roots))ExitProcess(223);pf_release(&roots);
  yes=0;set_value(&dataO,"CanAfford",&yes,1);if(funfair_can_buy(&rowO))ExitProcess(224);
  if(liveHandles)ExitProcess(225);
}
static unsigned char lifecycleQuitting;
static void*lifecycleApp;
static void lifecycle_instance(void*f,void*out){if(f!=(void*)11)ExitProcess(230);*(void**)out=lifecycleApp;}
static void lifecycle_field(void*o,void*f,void*out){if(o!=&stationObject||f!=(void*)12)ExitProcess(231);*(unsigned char*)out=lifecycleQuitting;}
void TestMain(void){
  init();p_thread_current=entry_context;placement_original=entry_original;
  g_app_instance_field=(void*)11;g_app_quitting_field=(void*)12;
  lifecycleApp=&stationObject;p_field_static_get_value=lifecycle_instance;p_field_get_value=lifecycle_field;
  expectedSelf=&stationObject;expectedMethod=(void*)123;
  observed_placement_lateupdate(expectedSelf,expectedMethod);
  if(originalCalls!=1||contextCalls||placement_callbacks!=1)ExitProcess(202);
  placement_request=1;expectedSelf=0;
  observed_placement_lateupdate(expectedSelf,expectedMethod);
  if(originalCalls!=2||contextCalls||placement_request!=1)ExitProcess(203);
  expectedSelf=&stationObject;g_pending=2;
  observed_placement_lateupdate(expectedSelf,expectedMethod);
  if(originalCalls!=3||contextCalls!=1||placement_request||g_pending!=2||write_attempted||write_pending||liveHandles)ExitProcess(204);
  observed_placement_lateupdate(expectedSelf,expectedMethod);
  if(originalCalls!=4||contextCalls!=1||placement_callbacks!=4)ExitProcess(205);
  /* Native quit stops every mod service while still forwarding callbacks. */
  lifecycleQuitting=1;placement_request=1;
  observed_placement_lateupdate(expectedSelf,expectedMethod);
  if(originalCalls!=5||contextCalls!=1||placement_callbacks!=4||placement_request!=1||!g_app_stopping)ExitProcess(232);
  lifecycleQuitting=0;lifecycleApp=0;
  if(app_services_available())ExitProcess(233);
  g_app_stopping=0;if(app_services_available())ExitProcess(234);
  lifecycleApp=&stationObject;if(!app_services_available())ExitProcess(235);
  placement_request=0;p_field_get_value=mock_field_value;
  stationBase.name="TPS.Game.GameItemInstance";stationObject.cls=&stationBase;
  stationDefinitionClass.name="TPS.Game.GameItemDefinition";stationDefinitionObject.cls=&stationDefinitionClass;
  stationBase.countMethod=(MM){&stationBase,&stationDefinitionClass,"get_Definition",0};
  stationDefinitionClass.countMethod=(MM){&stationDefinitionClass,&i64,"get_ID",0};
  stationDefinitionBox.cls=&i64;long long definitionId=-123456789;memcpy(stationDefinitionBox.bytes,&definitionId,8);
  p_runtime_invoke=definition_invoke;
  if(export_station_definition_id(&stationObject)!=definitionId||liveHandles)ExitProcess(215);
  for(definitionError=1;definitionError<=3;++definitionError)if(export_station_definition_id(&stationObject)||liveHandles)ExitProcess(216);
  row_tests();p_runtime_invoke=dispatch_invoke;
  if(!placement_dispatch(&placeMethod,&stationObject)||dispatchCalls!=1||!placement_attempted)ExitProcess(209);
  if(placement_dispatch(&placeMethod,&stationObject)||dispatchCalls!=1)ExitProcess(210);
  placement_attempted=0;
  if(placement_dispatch(0,&stationObject)||placement_attempted||dispatchCalls!=1)ExitProcess(211);
  dispatchException=1;
  if(placement_dispatch(&placeMethod,&stationObject)||!placement_attempted||dispatchCalls!=2)ExitProcess(212);
  if(liveHandles)ExitProcess(213);
  /* A retained operation freezes its in-memory plan and owns its GC roots.
   * Loading must not touch a changed/missing file while placement is armed. */
  write_selection_frozen=1;int priorCount=write_count;
  if(!write_load_selection()||write_count!=priorCount)ExitProcess(217);
  if(!pf_root(&placement_wait_roots,&stationObject))ExitProcess(218);
  placement_wait_item=&stationObject;placement_wait_started=123;placement_cursor_ended=456;
  placement_next_check=789;placement_support_cleared=1;
  placement_wait_clear();
  if(liveHandles||placement_wait_item||placement_wait_started||placement_cursor_ended||
     placement_next_check||placement_support_cleared||write_selection_frozen)ExitProcess(219);
  p_runtime_invoke=arrow_invoke;unsigned arrowEntity[2]={123,456};
  if(!placement_hide_arrow(&placeMethod,arrowEntity)||arrowCalls!=1)ExitProcess(221);
  arrowException=1;
  if(placement_hide_arrow(&placeMethod,arrowEntity)||arrowCalls!=2)ExitProcess(222);
  arrowEntity[1]=0;
  if(placement_hide_arrow(&placeMethod,arrowEntity)||placement_hide_arrow(0,arrowEntity)||
     placement_hide_arrow(&placeMethod,0)||arrowCalls!=2)ExitProcess(223);
  placement_attempted=1;placement_import_pending=1;
  placement_import_finished(0,0);
  if(!placement_attempted||placement_import_pending)ExitProcess(224);
  for(int repeat=0;repeat<3;++repeat){
    placement_attempted=1;placement_import_pending=1;
    placement_import_finished(1,0);
    if(placement_attempted||placement_import_pending||liveHandles)ExitProcess(225);
  }
  placement_attempted=1;placement_import_finished(1,0);
  if(!placement_attempted)ExitProcess(226);
  browserMock.name="UnityEngine.UI.Toggle";voidClass.name="System.Void";
  browserObject.cls=&browserMock;browserBool.cls=&boolean;
  browserMock.countMethod=(MM){&browserMock,&boolean,"get_isOn",0};
  browserMock.itemMethod=(MM){&browserMock,&voidClass,"SetIsOnWithoutNotify",1};
  p_runtime_invoke=browser_invoke;p_method_get_param=browser_param;browser_failed=0;
  browserBool.bytes[0]=1;
  if(!browser_pressed(&browserObject)||browserBool.bytes[0]||browserInvokes!=2||browser_failed)ExitProcess(231);
  if(browser_pressed(&browserObject)||browserInvokes!=3)ExitProcess(232);
  browserBool.bytes[0]=1;browserException=1;
  browser_pressed(&browserObject);if(!browser_failed)ExitProcess(233);
  browser_failed=0;browserInvokes=0;
  if(browser_set(&browserObject,"MissingSetter","System.Boolean",browserBool.bytes)||!browser_failed||browserInvokes)ExitProcess(234);
  BrowserBinding before[2]={{(void*)1,100},{(void*)2,200}};
  BrowserBinding after[2]={{(void*)1,100},{(void*)2,200}};
  if(!browser_bindings_match(before,2,after,2))ExitProcess(235);
  after[1].definition=300;
  if(browser_bindings_match(before,2,after,2))ExitProcess(236);
  after[1].definition=200;after[1].row=(void*)3;
  if(browser_bindings_match(before,2,after,2)||browser_bindings_match(before,2,after,1))ExitProcess(237);
  if(!browser_bindings_match(before,0,after,0))ExitProcess(238);
  int allowedFamilies[6]={10,20,30,40,50,60};
  for(unsigned family=0;family<6;++family)if(!browser_type_allowed(allowedFamilies[family],allowedFamilies))ExitProcess(239);
  if(browser_type_allowed(70,allowedFamilies)||
     browser_type_allowed(80,allowedFamilies)||browser_type_allowed(-1,allowedFamilies))ExitProcess(240);
  ExitProcess(0);
}
