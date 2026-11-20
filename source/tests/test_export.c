#define TPM_TEST_FIXTURE 1
#include "../coaster_selection_probe.c"
#include "writer_fixture.h"
DLLIMPORT void WINAPI ExitProcess(unsigned);
typedef struct MockClass MC;
typedef struct {const char*name;MC*type;MC*parent;unsigned offset;int flags;} MF;
typedef struct {MC*parent;MC*ret;const char*name;unsigned params;} MM;
struct MockClass {const char*name;int size;MF fields[16];unsigned fieldsCount;MM countMethod,itemMethod,positionMethod,rotationMethod,worldPositionMethod,worldRotationMethod,transformMethod,floorMethod;MC*parent;};
typedef struct MO {MC*cls;unsigned char bytes[96];int count;struct MO*item;} MO;
static MC i32,i64,u64,boolean,single,vec,quat,managedId,decorId,support,piece,supportList,pieceList,dataClass;
static MO supportObject,pieceObject,supportListObject,pieceListObject,dataObject,boxedCount,boxes[4096];
static MC stationBase,stationDerived;
static MO stationObject,stationPositionBox,stationRotationBox;
static MC transformClass;
static MO transformObject,transformPositionBox,transformRotationBox;
static MO floorObject,floorPositionBox,floorRotationBox;
static unsigned boxIndex,liveHandles;
static int mode,countCalls;
static void* mock_class_type(void*c){return c;}
static char* mock_type_name(void*t){return (char*)((MC*)t)->name;}
static unsigned char mock_value(void*c){return (unsigned char)(((MC*)c)->size>0);}
static int mock_size(void*c,unsigned*a){*a=4;return ((MC*)c)->size;}
static void* mock_field(void*c,const char*name){MC*k=(MC*)c;for(unsigned i=0;i<k->fieldsCount;++i)if(equals(k->fields[i].name,name))return &k->fields[i];return 0;}
static void* mock_fields(void*c,void**iter){MC*k=(MC*)c;unsigned n=(unsigned)(UINT_PTR)*iter;if(n>=k->fieldsCount)return 0;*iter=(void*)(UINT_PTR)(n+1);return &k->fields[n];}
static void* mock_field_type(void*f){return ((MF*)f)->type;}
static void* mock_field_parent(void*f){return ((MF*)f)->parent;}
static int mock_field_flags(void*f){return ((MF*)f)->flags;}
static const char* mock_field_name(void*f){return ((MF*)f)->name;}
static void mock_field_value(void*o,void*f,void*out){MF*field=(MF*)f;unsigned size=(unsigned)(field->type->size>0?field->type->size:8);memcpy(out,((MO*)o)->bytes+field->offset,size);}
static void* mock_field_box(void*f,void*o){if(boxIndex>=4096)return 0;MO*b=&boxes[boxIndex++];b->cls=((MF*)f)->type;mock_field_value(o,f,b->bytes);return b;}
static void* mock_class(void*o){return ((MO*)o)->cls;}
static GCHandle mock_pin(void*o,unsigned char pin){(void)pin;++liveHandles;return o;}
static void* mock_target(GCHandle h){return h;}
static void mock_release(GCHandle h){if(!h||!liveHandles)ExitProcess(99);--liveHandles;}
static void mock_free(void*p){(void)p;}
static void* mock_method(void*c,const char*n,int args){MC*k=(MC*)c;(void)args;MM*methods[]={&k->countMethod,&k->itemMethod,&k->positionMethod,&k->rotationMethod,&k->worldPositionMethod,&k->worldRotationMethod,&k->transformMethod,&k->floorMethod};for(unsigned i=0;i<8;++i)if(equals(methods[i]->name,n))return methods[i];return k->parent?mock_method(k->parent,n,args):0;}
static void* mock_methods(void*c,void**it){MC*k=(MC*)c;MM*methods[]={&k->countMethod,&k->itemMethod,&k->positionMethod,&k->rotationMethod,&k->worldPositionMethod,&k->worldRotationMethod,&k->transformMethod,&k->floorMethod};unsigned n=(unsigned)(UINT_PTR)*it;while(n<8){MM*m=methods[n++];*it=(void*)(UINT_PTR)n;if(m->name)return m;}return 0;}
static void* mock_parent(void*c){return ((MC*)c)->parent;}
static const char* mock_method_name(void*m){return ((MM*)m)->name;}
static unsigned mock_method_flags(void*m,unsigned*impl){(void)m;*impl=0;return 0;}
static unsigned char mock_assignable(void*base,void*actual){MC*c=(MC*)actual;while(c){if(c==base)return 1;c=c->parent;}return 0;}
static void* mock_method_class(void*m){return ((MM*)m)->parent;}
static unsigned char mock_instance(void*m){(void)m;return 1;}
static unsigned char mock_generic(void*m){(void)m;return 0;}
static unsigned mock_params(void*m){return ((MM*)m)->params;}
static void* mock_ret(void*m){return ((MM*)m)->ret;}
static void* mock_param(void*m,unsigned n){(void)m;(void)n;return &i32;}
static void* mock_unbox(void*o){return ((MO*)o)->bytes;}
static void* mock_invoke(void*m,void*obj,void**args,void**ex){
  MM*method=(MM*)m;MO*o=(MO*)obj;*ex=0;
  if(equals(method->name,"GetFloorAreaTransform")){if(mode==31)return 0;if(mode==32){*ex=(void*)1;return 0;}if(mode==33)return &boxedCount;return &floorObject;}
  if(equals(method->name,"get_GameObjectTransform")){if(mode==21)return 0;if(mode==22){*ex=(void*)1;return 0;}if(mode==23)return &boxedCount;return &transformObject;}
  if(equals(method->name,"get_position")){if(mode==24){*ex=(void*)1;return 0;}return obj==&floorObject?&floorPositionBox:&transformPositionBox;}
  if(equals(method->name,"get_rotation"))return obj==&floorObject?&floorRotationBox:&transformRotationBox;
  if(equals(method->name,"get_Position")||equals(method->name,"get_WorldPosition")){if(mode==20){*ex=(void*)1;return 0;}return &stationPositionBox;}
  if(equals(method->name,"get_Rotation")||equals(method->name,"get_WorldRotation"))return &stationRotationBox;
  if(equals(method->name,"get_Count")){int count=o->count;if(mode==8&&countCalls++)++count;boxedCount.cls=&i32;memcpy(boxedCount.bytes,&count,4);return &boxedCount;}
  if(mode==4){*ex=(void*)1;return 0;}if(mode==5)return &boxedCount;
  if(!args||*(int*)args[0]!=0)return 0;return o->item;
}
static void add_field(MC*c,const char*n,MC*t,unsigned offset){MF*f=&c->fields[c->fieldsCount++];f->name=n;f->type=t;f->parent=c;f->offset=offset;f->flags=0;}
static MC* type_for(const char*n){MC*types[]={&i32,&i64,&boolean,&single,&vec,&quat,&managedId,&decorId};for(unsigned i=0;i<8;++i)if(equals(types[i]->name,n))return types[i];return 0;}
static void init_schema(MC*c,const RecordField*schema,unsigned n){unsigned offset=0;for(unsigned i=0;i<n;++i){add_field(c,schema[i].name,type_for(schema[i].type),offset);offset+=(unsigned)schema[i].size;}}
static void set_value(MO*o,const char*n,const void*v,unsigned bytes){MF*f=(MF*)mock_field(o->cls,n);memcpy(o->bytes+f->offset,v,bytes);}
static void init(void){
  i32.name="System.Int32";i32.size=4;i64.name="System.Int64";i64.size=8;u64.name="System.UInt64";u64.size=8;
  boolean.name="System.Boolean";boolean.size=1;single.name="System.Single";single.size=4;
  vec.name="UnityEngine.Vector3";vec.size=12;quat.name="UnityEngine.Quaternion";quat.size=16;
  managedId.name="TPS.Core.Utils.ManagedEntityID";managedId.size=8;decorId.name="TPS.Game.GameItemDefinitionID";decorId.size=8;
  add_field(&managedId,"Value",&u64,0);add_field(&decorId,"ID",&i64,0);
  const char*names[]={"x","y","z","w"};for(unsigned i=0;i<4;++i){add_field(&quat,names[i],&single,i*4);if(i<3)add_field(&vec,names[i],&single,i*4);}
  support.name="TPS.Game.TrackSupportSnapshotData";support.size=80;piece.name="TPS.Game.TrackPieceSnapshotData";piece.size=32;
  init_schema(&support,supportSchema,11);init_schema(&piece,pieceSchema,4);
  supportList.name="System.Collections.Generic.List<TPS.Game.TrackSupportSnapshotData>";pieceList.name="System.Collections.Generic.List<TPS.Game.TrackPieceSnapshotData>";
  supportList.countMethod=(MM){&supportList,&i32,"get_Count",0};supportList.itemMethod=(MM){&supportList,&support,"get_Item",1};
  pieceList.countMethod=(MM){&pieceList,&i32,"get_Count",0};pieceList.itemMethod=(MM){&pieceList,&piece,"get_Item",1};
  add_field(&dataClass,"TrackSupportData",&supportList,0);add_field(&dataClass,"TrackPieceData",&pieceList,8);
  supportObject.cls=&support;pieceObject.cls=&piece;supportListObject.cls=&supportList;pieceListObject.cls=&pieceList;dataObject.cls=&dataClass;
  supportListObject.count=pieceListObject.count=1;supportListObject.item=&supportObject;pieceListObject.item=&pieceObject;
  MO*list=&supportListObject;memcpy(dataObject.bytes,&list,8);list=&pieceListObject;memcpy(dataObject.bytes+8,&list,8);
  int section=2,height=4;unsigned char valid=1;float pos[3]={11,4,8},rotation[4]={0,0,0,1},quarter=.25f;unsigned long long floor=18446744073709551615ULL;long long id=9223372036854775807LL,decor=-9223372036854775807LL;
  set_value(&supportObject,"SectionIndex",&section,4);set_value(&supportObject,"Position",pos,12);set_value(&supportObject,"WorldPosition",pos,12);set_value(&supportObject,"Rotation",rotation,16);
  set_value(&supportObject,"Valid",&valid,1);set_value(&supportObject,"FloorAreaInstance",&floor,8);set_value(&supportObject,"Height",&height,4);set_value(&supportObject,"BankingModifier",&quarter,4);
  set_value(&pieceObject,"SectionIndex",&section,4);set_value(&pieceObject,"TrackPieceDefinitionID",&id,8);set_value(&pieceObject,"TrackPieceDecorationDefinition",&decor,8);set_value(&pieceObject,"DecorationPosition",&quarter,4);
  p_class_get_type=mock_class_type;p_class_from_type=mock_class_type;p_type_get_name=mock_type_name;p_free=mock_free;p_class_is_valuetype=mock_value;p_class_value_size=mock_size;
  p_class_get_field_from_name=mock_field;p_class_get_fields=mock_fields;p_field_get_type=mock_field_type;p_field_get_parent=mock_field_parent;p_field_get_flags=mock_field_flags;p_field_get_name=mock_field_name;p_field_get_value=mock_field_value;p_field_get_value_object=mock_field_box;
  p_object_get_class=mock_class;p_gchandle_new=mock_pin;p_gchandle_free=mock_release;p_gchandle_get_target=mock_target;
  p_class_get_method_from_name=mock_method;p_class_get_methods=mock_methods;p_method_get_class=mock_method_class;p_method_is_instance=mock_instance;p_method_is_generic=mock_generic;p_method_get_param_count=mock_params;p_method_get_return_type=mock_ret;p_method_get_param=mock_param;p_runtime_invoke=mock_invoke;p_object_unbox=mock_unbox;
  p_class_get_parent=mock_parent;p_method_get_name=mock_method_name;p_method_get_flags=mock_method_flags;p_class_is_assignable_from=mock_assignable;
}
static int run_support(void){jpos=0;jok=1;boxIndex=0;countCalls=0;int count=0;return export_list(&dataObject,&dataClass,"TrackSupportData",supportList.name,support.name,80,supportSchema,11,&count);}
void TestMain(void){
  init();g_frame=1;g_origin[0]=10;g_origin[1]=2;g_origin[2]=5;g_rotation[3]=1;
  unsigned char call[7]={0x48,0xff,0x25,0xe0,0xff,0xff,0xff};UINT_PTR slot=0;
  if(!orientation_slot(call,7,0,0x1000,1,&slot)||slot!=0xfe7)ExitProcess(63);
  if(orientation_slot(call,6,0,0x1000,1,&slot))ExitProcess(64);
  call[0]=0;if(orientation_slot(call,7,0,0x1000,1,&slot))ExitProcess(65);
  unsigned char direct[6]={0xff,0x15,0x20,0,0,0};
  if(!orientation_slot(direct,6,0,0x1000,0,&slot)||slot!=0x1026)ExitProcess(66);
  if(!orientation_layout(writer_fixture,sizeof(writer_fixture)))ExitProcess(70);
  if(!orientation_slot(writer_fixture,sizeof(writer_fixture),0x365,0,0,&slot)||slot!=0x3859b38)ExitProcess(71);
  writer_fixture[0x3f2]=0;
  if(orientation_layout(writer_fixture,sizeof(writer_fixture)))ExitProcess(72);
  writer_fixture[0x3f2]=0x40;
  g_support_world_verified=1;
  jpos=0;jok=1;int count=0;js("{");
  if(!export_list(&dataObject,&dataClass,"TrackSupportData",supportList.name,support.name,80,supportSchema,11,&count)||count!=1)ExitProcess(1);
  js(",");if(!export_list(&dataObject,&dataClass,"TrackPieceData",pieceList.name,piece.name,32,pieceSchema,4,&count)||count!=1)ExitProcess(2);js("}");
  HANDLE file=CreateFileW(L"test-records.json",0x40000000,0,0,2,FILE_ATTRIBUTE_NORMAL,0);DWORD written=0;
  if(file==INVALID_HANDLE_VALUE||!WriteFile(file,jbuf,jpos,&written,0)||written!=jpos)ExitProcess(3);CloseHandle(file);
  publish_blueprint();Sleep(20);publish_blueprint();
  g_support_world_verified=0;
  if(!run_support())ExitProcess(67);
  jbuf[jpos]=0;
  if(orientation_contains(jbuf,"StationRelativeRotation\":"))ExitProcess(68);
  if(!orientation_contains(jbuf,"source-space-unverified"))ExitProcess(69);
  g_support_world_verified=1;
  supportListObject.count=-1;if(run_support())ExitProcess(4);supportListObject.count=4097;if(run_support())ExitProcess(5);supportListObject.count=1;
  supportList.itemMethod.params=0;if(run_support())ExitProcess(6);supportList.itemMethod.params=1;
  mode=4;if(run_support())ExitProcess(7);mode=5;if(run_support())ExitProcess(8);mode=0;
  support.fields[0].flags=0x10;if(run_support())ExitProcess(9);support.fields[0].flags=0;
  ++support.fieldsCount;if(run_support())ExitProcess(10);--support.fieldsCount;
  unsigned char invalid=2;set_value(&supportObject,"Valid",&invalid,1);if(run_support())ExitProcess(11);invalid=1;set_value(&supportObject,"Valid",&invalid,1);
  unsigned nan=0x7fc00000;set_value(&supportObject,"BankingModifier",&nan,4);if(run_support())ExitProcess(12);nan=0;set_value(&supportObject,"BankingModifier",&nan,4);
  mode=8;if(run_support())ExitProcess(13);mode=0;
  supportListObject.count=0;if(!run_support())ExitProcess(14);
  supportListObject.count=1;float zeroRotation[4]={0,0,0,0};set_value(&supportObject,"Rotation",zeroRotation,16);
  if(!run_support())ExitProcess(30);
  zeroRotation[3]=1;set_value(&supportObject,"Rotation",zeroRotation,16);g_frame=0;
  if(!run_support())ExitProcess(31);g_frame=1;
  stationBase.name="Mock.StationBase";stationDerived.name="Mock.StationDerived";stationDerived.parent=&stationBase;
  stationObject.cls=&stationDerived;stationPositionBox.cls=&vec;stationRotationBox.cls=&quat;
  float origin[3]={10,2,5},rotation[4]={0,0,0,1};memcpy(stationPositionBox.bytes,origin,12);memcpy(stationRotationBox.bytes,rotation,16);
  stationBase.positionMethod=(MM){&stationBase,&vec,"get_Position",0};
  station_frame(&stationObject);if(g_frame||!g_station_position_verified||g_origin[0]!=10)ExitProcess(16);
  stationBase.rotationMethod=(MM){&stationBase,&quat,"get_Rotation",0};
  station_frame(&stationObject);if(!g_frame||!g_station_position_verified||g_rotation[3]!=1)ExitProcess(17);
  rotation[3]=2;memcpy(stationRotationBox.bytes,rotation,16);station_frame(&stationObject);if(g_frame)ExitProcess(18);
  rotation[3]=1;memcpy(stationRotationBox.bytes,rotation,16);stationBase.rotationMethod.ret=&single;
  station_frame(&stationObject);if(g_frame||!g_station_position_verified)ExitProcess(19);
  stationBase.worldPositionMethod=(MM){&stationBase,&vec,"get_WorldPosition",0};stationBase.worldRotationMethod=(MM){&stationBase,&quat,"get_WorldRotation",0};
  station_frame(&stationObject);if(!g_frame||!equals(g_station_rotation_source,"get_WorldRotation"))ExitProcess(20);
  mode=20;station_frame(&stationObject);if(g_frame||g_station_position_verified)ExitProcess(21);mode=0;
  unsigned originNan=0x7fc00000;memcpy(stationPositionBox.bytes,&originNan,4);station_frame(&stationObject);if(g_frame||g_station_position_verified)ExitProcess(22);
  memcpy(stationPositionBox.bytes,origin,12);stationBase.worldPositionMethod.name=0;stationBase.worldRotationMethod.name=0;
  transformClass.name="UnityEngine.Transform";transformObject.cls=&transformClass;transformPositionBox.cls=&vec;transformRotationBox.cls=&quat;
  memcpy(transformPositionBox.bytes,origin,12);memcpy(transformRotationBox.bytes,rotation,16);
  stationBase.transformMethod=(MM){&stationBase,&transformClass,"get_GameObjectTransform",0};
  transformClass.positionMethod=(MM){&transformClass,&vec,"get_position",0};transformClass.rotationMethod=(MM){&transformClass,&quat,"get_rotation",0};
  station_frame(&stationObject);if(!g_frame||!g_transform_probe_read||!g_transform_position_agrees||!equals(g_station_rotation_source,"get_GameObjectTransform().get_rotation"))ExitProcess(23);
  float mismatch[3]={11,2,5};memcpy(transformPositionBox.bytes,mismatch,12);station_frame(&stationObject);if(g_frame||!g_transform_probe_read||g_transform_position_agrees)ExitProcess(24);memcpy(transformPositionBox.bytes,origin,12);
  rotation[3]=2;memcpy(transformRotationBox.bytes,rotation,16);station_frame(&stationObject);if(g_frame)ExitProcess(25);rotation[3]=1;memcpy(transformRotationBox.bytes,rotation,16);
  for(int m=21;m<=24;++m){mode=m;station_frame(&stationObject);if(g_frame)ExitProcess(26);}mode=0;
  transformClass.rotationMethod.params=1;station_frame(&stationObject);if(g_frame)ExitProcess(27);transformClass.rotationMethod.params=0;
  transformClass.rotationMethod.ret=&single;station_frame(&stationObject);if(g_frame)ExitProcess(28);transformClass.rotationMethod.ret=&quat;
  station_frame(&stationObject);if(!g_frame)ExitProcess(29);
  floor_frame(&stationObject);if(g_floor_frame_read||!equals(g_floor_frame_status,"getter-unavailable"))ExitProcess(40);
  stationBase.floorMethod=(MM){&stationBase,&transformClass,"GetFloorAreaTransform",0};
  floorObject.cls=&transformClass;floorPositionBox.cls=&vec;floorRotationBox.cls=&quat;
  float floorPos[3]={100,0,50},floorRot[4]={0,0.707106769f,0,0.707106769f};
  memcpy(floorPositionBox.bytes,floorPos,12);memcpy(floorRotationBox.bytes,floorRot,16);
  floor_frame(&stationObject);if(!g_floor_frame_read||!equals(g_floor_frame_status,"world-frame-read")||g_floor_position[0]!=100)ExitProcess(41);
  for(int m=31;m<=33;++m){mode=m;floor_frame(&stationObject);if(g_floor_frame_read)ExitProcess(42);}mode=0;
  stationBase.floorMethod.params=1;floor_frame(&stationObject);if(g_floor_frame_read)ExitProcess(43);stationBase.floorMethod.params=0;
  transformClass.rotationMethod.ret=&single;floor_frame(&stationObject);if(g_floor_frame_read)ExitProcess(44);transformClass.rotationMethod.ret=&quat;
  floorRot[3]=2;memcpy(floorRotationBox.bytes,floorRot,16);floor_frame(&stationObject);if(g_floor_frame_read)ExitProcess(45);
  unsigned invalidBits=0x7fc00000;memcpy(floorPositionBox.bytes,&invalidBits,4);floor_frame(&stationObject);if(g_floor_frame_read)ExitProcess(46);
  memcpy(floorPositionBox.bytes,floorPos,12);floorRot[3]=0.707106769f;memcpy(floorRotationBox.bytes,floorRot,16);
  mode=24;floor_frame(&stationObject);if(g_floor_frame_read)ExitProcess(47);mode=0;
  floor_frame(&stationObject);if(!g_floor_frame_read)ExitProcess(48);
  add_field(&stationBase,"ID",&i64,0);g_id_field=&stationBase.fields[stationBase.fieldsCount-1];
  add_field(&stationBase,"Entity",&i64,8);g_entity_field=&stationBase.fields[stationBase.fieldsCount-1];
  long long stationId=77;unsigned entityValue[2]={100,2};memcpy(stationObject.bytes,&stationId,8);memcpy(stationObject.bytes+8,entityValue,8);
  /* Invalid module handle makes publication stop before file creation here;
   * the full-document bytes are saved explicitly for the JSON assertions. */
  g_module=(HMODULE)1;export_blueprint(&dataObject,&dataClass,&stationObject);
  if(!jok||!g_floor_frame_read||!g_frame)ExitProcess(49);
  file=CreateFileW(L"test-floor-document.json",0x40000000,0,0,2,FILE_ATTRIBUTE_NORMAL,0);written=0;
  if(file==INVALID_HANDLE_VALUE||!WriteFile(file,jbuf,jpos,&written,0)||written!=jpos)ExitProcess(50);CloseHandle(file);
  if(liveHandles)ExitProcess(15);ExitProcess(0);
}

