#include "../coaster_selection_probe.c"
__declspec(dllexport) int ImportTransform(const float*source,const float*target,const float*point,const float*rotation,float angle,float*out){
  write_source_id=1;write_count=1;write_source_position[0]=write_source_position[1]=write_source_position[2]=0;
  memcpy(write_source_rotation,source,16);memcpy(write_fixture[0].position,point,12);memcpy(write_fixture[0].rotation,rotation,16);memcpy(&write_fixture[0].angle,&angle,4);
  float origin[3]={0,0,0},p[1][3];if(!write_target_frame(2,origin,target)||!write_plan_points(origin,p))return 0;
  memcpy(out,p[0],12);memcpy(out+3,write_target_rotations[0],16);out[7]=write_target_angles[0];return 1;
}
