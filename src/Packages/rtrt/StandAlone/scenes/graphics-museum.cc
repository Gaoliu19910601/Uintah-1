/* look from above:

rtrt -np 8 -eye -18.9261 -22.7011 52.5255 -lookat -7.20746 -8.61347 -16.643 -up 0.490986 -0.866164 -0.0932288 -fov 40 -scene scenes/graphics-museum 

look from hallway:
./rtrt -np 15 -bv 4 -hgridcellsize 8 8 8 -eye -5.85 -6.2 2 -lookat -8.16796 -16.517 2 -up 0 0 1 -fov 60 -scene scenes/graphics-museum 

looking at David:
./rtrt -np 40 -eye -11.6982 -16.4997 1.42867 -lookat -12.18 -21.0565 1.42867 -up 0 0 1 -fov 66.9403 -scene scenes/graphics-museum

and

./rtrt -np 20 -bv 4 -hgridcellsize 8 8 8 -eye -19.0241 -27.0214 1.97122 -lookat -15.2381 -17.2251 1.97122 -up 0 0 1 -fov 60 -scene scenes/graphics-museum


BEHIND:
./rtrt -np 20 -bv 4 -hgridcellsize 8 8 8 -eye -8.66331 -14.4693 1.97982 -lookat -10.5978 -17.9173 1.97982 -up 0 0 1 -fov 66.9403 -scene scenes/graphics-museum


rtrt -np 8 -eye -18.5048 -25.9155 1.63435 -lookat -14.7188 -16.1192 0.164304 -up 0 0 1 -fov 60  -scene scenes/graphics-museum 

from the other dirction (at David):
rtrt -np 8 -eye -10.9222 -16.5818 1.630637 -lookat -11.404 -21.1386 0.630637 -up 0 0 1 -fov 66.9403 -scene scenes/graphics-museum

rtrt -np 14 -eye -10.2111 -16.2099 1.630637 -lookat -11.7826 -20.5142 0.630637 -up 0 0 1 -fov 66.9403 scene scenes/graphics-museum


*/

#include <Packages/rtrt/Core/Camera.h>
#include <Packages/rtrt/Core/Grid.h>
#include <Packages/rtrt/Core/HierarchicalGrid.h>
#include <Packages/rtrt/Core/Disc.h>
#include <Packages/rtrt/Core/Ring.h>
#include <Packages/rtrt/Core/Group.h>
#include <Packages/rtrt/Core/Phong.h>
#include <Packages/rtrt/Core/PhongMaterial.h>
#include <Packages/rtrt/Core/LambertianMaterial.h>
#include <Packages/rtrt/Core/ASEReader.h>
#include <Packages/rtrt/Core/Scene.h>
#include <iostream>
#include <math.h>
#include <string.h>
#include <Packages/rtrt/Core/Point4D.h>
#include <Packages/rtrt/Core/CrowMarble.h>
#include <Core/Geometry/Point.h>
#include <Core/Geometry/Vector.h>
#include <Packages/rtrt/Core/Mesh.h>
#include <Packages/rtrt/Core/ASEReader.h>
#include <Packages/rtrt/Core/ObjReader.h>
#include <Packages/rtrt/Core/Bezier.h>
#include <Packages/rtrt/Core/BV1.h>
#include <Packages/rtrt/Core/Checker.h>
#include <Packages/rtrt/Core/Speckle.h>
#include <Packages/rtrt/Core/Box.h>
#include <Packages/rtrt/Core/CoupledMaterial.h>
#include <Packages/rtrt/Core/DielectricMaterial.h>
#include <Packages/rtrt/Core/MetalMaterial.h>
#include <Packages/rtrt/Core/Rect.h>
#include <Packages/rtrt/Core/Sphere.h>
#include <Core/Math/MinMax.h>
#include <Packages/rtrt/Core/Tri.h>
#include <Core/Geometry/Transform.h>
#include <Packages/rtrt/Core/ImageMaterial.h>
#include <Packages/rtrt/Core/Parallelogram.h>
#include <Packages/rtrt/Core/UVSphere.h>
#include <Packages/rtrt/Core/UVCylinder.h>
#include <Packages/rtrt/Core/UVCylinderArc.h>
#include <Packages/rtrt/Core/DiscArc.h>
#include <Packages/rtrt/Core/Cylinder.h>
#include <Packages/rtrt/Core/ply.h>
#include <Packages/rtrt/Core/BBox.h>
#include <Packages/rtrt/Core/PerlinBumpMaterial.h>
#include <Packages/rtrt/Core/PLYReader.h>
#include <Packages/rtrt/Core/TriMesh.h>
#include <Packages/rtrt/Core/VideoMap.h>
#include <Packages/rtrt/Core/Array1.h>
#include <Packages/rtrt/Core/TrisReader.h>
#include <Packages/rtrt/Core/GridTris.h>

using namespace rtrt;
using namespace SCIRun;

#define MAXBUFSIZE 256
#define IMG_EPS 0.01
#define SCALE 500

// all the following should be 1 for demos  (0 to speedup for debugging)
#define IMGSONWALL 0
#define INSERTDAVID 0
#define INSERTCEILING 1
#define INSERTVIDEO 0
#define INSERTMODERNMODELS 1
#define INSERTHISTORYMODELS 0

#define INSERTHUGEMODELS 0

#if INSERTMODERNMODELS

//  #define INSERT_CRANK
//  #define INSERT_DRAGON
//  #define INSERT_TORSO
#define INSERT_BUDDHA
//  #define INSERT_WELL
//  #define INSERT_VENUS
// #define INSERT_BUNNY
// #define INSERT_SPIRAL
// #define INSERT_STADIUM
// #define INSERT_HEAD

#endif

void add_image_on_wall (char *image_name, const Point &top_left, 
			 const Vector &right, const Vector &down,
			 Group* wall_group) {
  Material* image_mat = 
    new ImageMaterial(image_name,ImageMaterial::Clamp, ImageMaterial::Clamp,
		      1, Color(0,0,0), 0); 
  Object* image_obj = 
    new Parallelogram(image_mat, top_left, right, down);

  //  image_mat->my_lights.add(l);
  image_mat->local_ambient_mode=Arc_Ambient;
  wall_group->add(image_obj);
}

void add_video_on_wall (Material* video, const Point &top_left, 
			 const Vector &right, const Vector &down,
			 Group* wall_group) {
  Object* image_obj = 
    new Parallelogram(video, top_left, right, down);
  video->local_ambient_mode=Arc_Ambient;
  wall_group->add(image_obj);

  Material* glass= new DielectricMaterial(1.5, 1.0, 0.05, 400.0, 
					  Color(.80, .93 , .87), 
					  Color(1,1,1), false);
  Vector in = Cross (right,down);
  Vector out = Cross (down, right);
  in.normalize();
  out.normalize();
  in *= 0.01;
  out *= 0.02;

  BBox glass_bbox;
  glass_bbox.extend (top_left + in);
  glass_bbox.extend (top_left + out + right + down);
  wall_group->add(new Box (glass, glass_bbox.min(), glass_bbox.max()));
}

void add_poster_on_wall (char *image_name, const Point &top_left, 
			 const Vector &right, const Vector &down,
			 Group* wall_group) {

  /* add glass frame */
  Material* glass= new DielectricMaterial(1.5, 1.0, 0.05, 400.0, 
					  Color(.80, .93 , .87), 
					  Color(1,1,1), false);
  Material* grey = new PhongMaterial(Color(.5,.5,.5),1,0.3,100,true);
  Vector in = Cross (right,down);
  Vector out = Cross (down, right);
  in.normalize();
  out.normalize();

#if IMGSONWALL
  add_image_on_wall(image_name, top_left, right, down, wall_group);
#endif

  in *= 0.01;
  out *= 0.05;

  BBox glass_bbox;
  glass_bbox.extend (top_left + in);
  glass_bbox.extend (top_left + out + right + down);
  
  wall_group->add(new Box (glass, glass_bbox.min(), glass_bbox.max()));
  //  wall_group->add(new Box (clear, glass_bbox.min(), glass_bbox.max()));

  /* add cylinders */
  wall_group->add(new Cylinder (grey, top_left+in+right*0.05+down*0.05,
				top_left+out*1.1+right*0.05+down*0.05, 0.01));
  wall_group->add(new Disc (grey, top_left+out*1.1+right*0.05+down*0.05, 
			    out, 0.01));

  wall_group->add(new Cylinder (grey, top_left+in+right*0.95+down*0.05,
				top_left+out*1.1+right*0.95+down*0.05, 0.01));
  wall_group->add(new Disc (grey, top_left+out*1.1+right*0.95+down*0.05, 
			    out, 0.01));

  wall_group->add(new Cylinder (grey, top_left+in+right*0.05+down*0.95,
				top_left+out*1.1+right*0.05+down*0.95, 0.01));
  wall_group->add(new Disc (grey, top_left+out*1.1+right*0.05+down*0.95, 
			    out, 0.01));

  wall_group->add(new Cylinder (grey, top_left+in+right*0.95+down*0.95,
				top_left+out*1.1+right*0.95+down*0.95, 0.01));
  wall_group->add(new Disc (grey, top_left+out*1.1+right*0.95+down*0.95, 
			    out, 0.01));

}

void add_cheap_poster_on_wall (char *image_name, const Point &top_left, 
			 const Vector &right, const Vector &down,
			 Group* wall_group, Scene *scene) {

  /* add glass frame */
  Material* glass = new PhongMaterial (Color(0.45,0.5,0.45),0.1,0.15,50,false);
  Material* grey = new PhongMaterial(Color(.5,.5,.5),1,0.3,100,true);
  Vector in = Cross (right,down);
  Vector out = Cross (down, right);
  in.normalize();
  out.normalize();

  Light *l1 = new Light (top_left+out*2+Vector(0,0,2),Color(1.,1.,1.),0,0.7);
  scene->add_per_matl_light(l1);
  glass->my_lights.add(l1);
  glass->local_ambient_mode=Arc_Ambient;

#if IMGSONWALL
  add_image_on_wall(image_name, top_left, right, down, wall_group);
#endif

  in *= 0.01;
  out *= 0.05;

  BBox glass_bbox;
  glass_bbox.extend (top_left + in);
  glass_bbox.extend (top_left + out + right + down);

  
  wall_group->add(new Box (glass, glass_bbox.min(), glass_bbox.max()));
  //  wall_group->add(new Box (clear, glass_bbox.min(), glass_bbox.max()));

  /* add cylinders */
  wall_group->add(new Cylinder (grey, top_left+in+right*0.05+down*0.05,
				top_left+out*1.1+right*0.05+down*0.05, 0.01));
  wall_group->add(new Disc (grey, top_left+out*1.1+right*0.05+down*0.05, 
			    out, 0.01));

  wall_group->add(new Cylinder (grey, top_left+in+right*0.95+down*0.05,
				top_left+out*1.1+right*0.95+down*0.05, 0.01));
  wall_group->add(new Disc (grey, top_left+out*1.1+right*0.95+down*0.05, 
			    out, 0.01));

  wall_group->add(new Cylinder (grey, top_left+in+right*0.05+down*0.95,
				top_left+out*1.1+right*0.05+down*0.95, 0.01));
  wall_group->add(new Disc (grey, top_left+out*1.1+right*0.05+down*0.95, 
			    out, 0.01));

  wall_group->add(new Cylinder (grey, top_left+in+right*0.95+down*0.95,
				top_left+out*1.1+right*0.95+down*0.95, 0.01));
  wall_group->add(new Disc (grey, top_left+out*1.1+right*0.95+down*0.95, 
			    out, 0.01));

}

void add_baseboards (Group* obj_group, Light *h0, Light *h1, Light *h2,
		     Light *dav1, Light *dav2, Light *mod1){
  Material* ebase = new Phong (Color(0.05,0.05,0.05),Color(.5,.5,.5),30);
  Material* wbase = new Phong (Color(0.05,0.05,0.05),Color(.5,.5,.5),30);
  Material* nbase = new Phong (Color(0.05,0.05,0.05),Color(.5,.5,.5),30);
  Material* sbase = new Phong (Color(0.05,0.05,0.05),Color(.5,.5,.5),30);
  Material* nebase = new Phong (Color(0.05,0.05,0.05),Color(.5,.5,.5),30);

  /* four directions 
  Vector w (-1,0,0);
  Vector e (1,0,0);
  Vector n (0,1,0);
  Vector s (0,-1,0);
  Vector up (0,0,0.12);  */

  // east wall
  obj_group->add(new Box(ebase, Point(-4.03,-5,0), Point(-4,-4,0.12)));
  obj_group->add(new Box(ebase, Point(-4.03,-28,0), Point(-4,-7,0.12)));

  // west wall
  obj_group->add(new Box(wbase, Point(-20,-28,0), Point(-19.97,-4,0.12)));

  // south wall
  obj_group->add(new Box(sbase, Point(-20,-28,0), Point(-4,-27.97,0.12)));

  // north wall
  obj_group->add(new Box(nbase, Point(-20,-4.03,0), Point(-11,-4,0.12)));
  obj_group->add(new Box(nebase, Point(-9,-4.03,0), Point(-4,-4,0.12)));

  ebase->my_lights.add(h0);
  ebase->my_lights.add(h1);
  sbase->my_lights.add(h1);
  sbase->my_lights.add(h2);
  sbase->my_lights.add(dav1);
  wbase->my_lights.add(dav1);
  wbase->my_lights.add(dav2);
  nbase->my_lights.add(mod1);
  nebase->my_lights.add(mod1);
  nebase->my_lights.add(h0);
}

void add_glass_box (Group* obj_group, const Point UpperCorner,Vector FarDir) {
  
  Material* clear = new PhongMaterial (Color(0.8,0.85,0.8),0.1,0.15,50,false);

  Vector u (FarDir.x()/2., 0, 0);
  Vector v (0,FarDir.y()/2.,0);
  Vector w (0,0,FarDir.z()/2.);
  Point OppUppCorner = UpperCorner+u+u+v+v;

  // top  
  obj_group->add(new Rect(clear, UpperCorner+u+v, -u, -v));
  // sides
  obj_group->add(new Rect(clear, UpperCorner+u+w, -u, w));
  obj_group->add(new Rect(clear, UpperCorner+v+w, -v, w));
  obj_group->add(new Rect(clear, OppUppCorner-u+w, u, w));  
  obj_group->add(new Rect(clear, OppUppCorner-v+w, v, w));  
}

void add_lit_glass_box (Group* obj_group, const Point UpperCorner,Vector FarDir,
			Light *l1, Light *l2) {
  
  Material* clear = new PhongMaterial (Color(0.45,0.5,0.45),0.1,0.15,50,false);
  clear->my_lights.add(l1);
  clear->my_lights.add(l2);

  Vector u (FarDir.x()/2., 0, 0);
  Vector v (0,FarDir.y()/2.,0);
  Vector w (0,0,FarDir.z()/2.);
  Point OppUppCorner = UpperCorner+u+u+v+v;

  // top  
  obj_group->add(new Rect(clear, UpperCorner+u+v, -u, -v));
  // sides
  obj_group->add(new Rect(clear, UpperCorner+u+w, -u, w));
  obj_group->add(new Rect(clear, UpperCorner+v+w, -v, w));
  obj_group->add(new Rect(clear, OppUppCorner-u+w, u, w));  
  obj_group->add(new Rect(clear, OppUppCorner-v+w, v, w));  
}

/* year is shifted to right, glass box included */
void add_pedestal_and_year (Group* obj_group, Group *glass_group,
			    char* sign_name, const Point UpperCorner, 
			    const Vector FarDir, const Point GlassCorner, 
			    const Vector GlassDir, float sign_ratio, 
			    const Vector u, const Vector v, const Vector w,
			    Light* l1, Light* l2) {
  Group* solid_group = new Group();
  Material* ped_white = new ImageMaterial("/usr/sci/data/Geometry/textures/museum/general/tex-pill.ppm",
					  ImageMaterial::Tile,
					  ImageMaterial::Tile, 1,
					  Color(0,0,0), 0);

  Point OppUppCorner = UpperCorner+u+u+v+v;

  ped_white->my_lights.add(l1);
  ped_white->my_lights.add(l2);

  // top  
  solid_group->add(new Rect(ped_white, UpperCorner+u+v, -u, -v));
  // sides
  solid_group->add(new Rect(ped_white, UpperCorner+u+w, -u, w));
  solid_group->add(new Rect(ped_white, UpperCorner+v+w, -v, w));
  solid_group->add(new Rect(ped_white, OppUppCorner-u+w, u, w));  
  solid_group->add(new Rect(ped_white, OppUppCorner-v+w, v, w));

#if IMGSONWALL
  Material* sign = new ImageMaterial(sign_name, ImageMaterial::Tile,
				     ImageMaterial::Tile, 1, Color(0,0,0), 0);
  sign->my_lights.add(l1);
  sign->my_lights.add(l2);

  // signs on all sides
  const float part = 0.8;
  Vector small_u=u*0.1;

  Vector small_v=v*0.1;
  Vector small_w=w*0.1;
  Vector part_u=u*part;
  Vector part_v=v*part;

  Vector sign_height (0,0,(FarDir.x()<0?FarDir.x():-FarDir.x())*0.5*part*sign_ratio);

  solid_group->add(new Parallelogram(sign, UpperCorner-Vector(0,FarDir.y()*0.01,0)+small_u+small_w, 
				   part_u, sign_height));  
  solid_group->add(new Parallelogram(sign, UpperCorner+v+v-Vector(FarDir.x()*0.01,0,0)-small_v+small_w, 
				   -part_v, sign_height));  
  solid_group->add(new Parallelogram(sign, OppUppCorner+Vector(0,FarDir.y()*0.01,0)-small_u+small_w, 
				   -part_u, sign_height));  
  solid_group->add(new Parallelogram(sign, OppUppCorner-v-v+Vector(FarDir.x()*0.01,0,0)+small_v+small_w, 
				   part_v, sign_height));  
#endif
  obj_group->add(solid_group);

  add_lit_glass_box (glass_group, GlassCorner, GlassDir, l1, l2);
}

/* label is centered */
void add_pedestal_and_label (Group* obj_group, char* sign_name, const Point UpperCorner, 
			     const Vector FarDir, float sign_ratio, Scene *scene) {
  Material* ped_white = new ImageMaterial("/usr/sci/data/Geometry/textures/museum/general/tex-pill.ppm",
					  ImageMaterial::Tile,
					  ImageMaterial::Tile, 1,
					  Color(0,0,0), 0);
  Vector u (FarDir.x()/2., 0, 0);
  Vector v (0,FarDir.y()/2.,0);
  Vector w (0,0,FarDir.z()/2.);
  
  Point OppUppCorner = UpperCorner+u+u+v+v;
  // top  
  obj_group->add(new Rect(ped_white, UpperCorner+u+v, -u, -v));
  // sides
  obj_group->add(new Rect(ped_white, UpperCorner+u+w, -u, w));
  obj_group->add(new Rect(ped_white, UpperCorner+v+w, -v, w));
  obj_group->add(new Rect(ped_white, OppUppCorner-u+w, u, w));  
  obj_group->add(new Rect(ped_white, OppUppCorner-v+w, v, w));

  Light *l1=new Light(UpperCorner-u*3-v*3-w*2,Color(1.,1.,1.),0,0.3);
  Light *l2=new Light(UpperCorner-u*3+v*5-w*2,Color(1.,1.,1.),0,0.3);
  Light *l3=new Light(UpperCorner+u*5-v*3-w*2,Color(1.,1.,1.),0,0.3);
  Light *l4=new Light(UpperCorner+u*5+v*5-w*2,Color(1.,1.,1.),0,0.3);
  scene->add_per_matl_light(l1);
  scene->add_per_matl_light(l2);
  scene->add_per_matl_light(l3);
  scene->add_per_matl_light(l4);
  ped_white->my_lights.add(l1);
  ped_white->my_lights.add(l2);
  ped_white->my_lights.add(l3);
  ped_white->my_lights.add(l4);

#if IMGSONWALL
  Material* sign = new ImageMaterial(sign_name, ImageMaterial::Tile,
				     ImageMaterial::Tile, 1, Color(0,0,0), 0);
  // signs on all sides
  const float part = 0.8;
  Vector part_u=u*part*2;
  Vector part_v=v*part*2;
  Vector half_u=u*(1-part);
  Vector half_v=v*(1-part);
  Vector small_w=w*0.1;

  Vector sign_height (0,0,(FarDir.x()<0?FarDir.x():-FarDir.x())*0.5*part*sign_ratio);

  obj_group->add(new Parallelogram(sign, UpperCorner-Vector(0,FarDir.y()*0.01,0)+half_u+small_w, 
				   part_u, sign_height));  
  obj_group->add(new Parallelogram(sign, UpperCorner+v+v-Vector(FarDir.x()*0.01,0,0)-half_v+small_w, 
				   -part_v, sign_height)); 
  obj_group->add(new Parallelogram(sign, OppUppCorner+Vector(0,FarDir.y()*0.01,0)-half_u+small_w, 
				   -part_u, sign_height));  
  obj_group->add(new Parallelogram(sign, OppUppCorner-v-v+Vector(FarDir.x()*0.01,0,0)+half_v+small_w, 
				   part_v, sign_height));  

  sign->my_lights.add(l1);
  sign->my_lights.add(l2);
  sign->my_lights.add(l3);
  sign->my_lights.add(l4);
#endif

}

void add_pedestal (Group* obj_group, const Point UpperCorner, 
		   const Vector FarDir, Scene *scene) {
  Material* ped_white = new ImageMaterial("/usr/sci/data/Geometry/textures/museum/general/tex-pill.ppm",
					  ImageMaterial::Tile,
					  ImageMaterial::Tile, 1,
					  Color(0,0,0), 0);
  Vector u (FarDir.x()/2., 0, 0);
  Vector v (0,FarDir.y()/2.,0);
  Vector w (0,0,FarDir.z()/2.);
  Point OppUppCorner = UpperCorner+u+u+v+v;
  // top  
  obj_group->add(new Rect(ped_white, UpperCorner+u+v, -u, -v));
  // sides
  obj_group->add(new Rect(ped_white, UpperCorner+u+w, -u, w));
  obj_group->add(new Rect(ped_white, UpperCorner+v+w, -v, w));
  obj_group->add(new Rect(ped_white, OppUppCorner-u+w, u, w));  
  obj_group->add(new Rect(ped_white, OppUppCorner-v+w, v, w));  

  Light *l1=new Light(UpperCorner-u*3-v*3-w*2,Color(1.,1.,1.),0,0.3);
  Light *l2=new Light(UpperCorner-u*3+v*5-w*2,Color(1.,1.,1.),0,0.3);
  Light *l3=new Light(UpperCorner+u*5-v*3-w*2,Color(1.,1.,1.),0,0.3);
  Light *l4=new Light(UpperCorner+u*5+v*5-w*2,Color(1.,1.,1.),0,0.3);
  scene->add_per_matl_light(l1);
  scene->add_per_matl_light(l2);
  scene->add_per_matl_light(l3);
  scene->add_per_matl_light(l4);
  ped_white->my_lights.add(l1);
  ped_white->my_lights.add(l2);
  ped_white->my_lights.add(l3);
  ped_white->my_lights.add(l4);
}

void add_east_pedestal (Group* obj_group, Group *glass_group,
			char* sign_name, const Point UpperCorner, 
			const Vector FarDir, const Point GlassCorner, 
			const Vector GlassDir, float sign_ratio, Scene *scene) {

  Vector u (FarDir.x()/2., 0, 0);
  Vector v (0,FarDir.y()/2.,0);
  Vector w (0,0,FarDir.z()/2.); 

  Light *l1=new Light(UpperCorner-u*3-v*3-w*3,Color(1.,1.,1.),0,0.6);
  Light *l2=new Light(UpperCorner-u*3+v*5-w*3,Color(1.,1.,1.),0,0.6);
  //  l1->name_ = "His Ped1";
  //  l2->name_ = "His Ped2";
  scene->add_per_matl_light(l1);
  scene->add_per_matl_light(l2);
  
  add_pedestal_and_year (obj_group, glass_group, sign_name, UpperCorner, 
			 FarDir, GlassCorner, GlassDir, sign_ratio, u,v,w,l1,l2);
}

void add_west_pedestal (Group* obj_group, Group *glass_group,
			char* sign_name, const Point UpperCorner, 
			const Vector FarDir, const Point GlassCorner, 
			const Vector GlassDir, float sign_ratio, Scene *scene) {

  Vector u (FarDir.x()/2., 0, 0);
  Vector v (0,FarDir.y()/2.,0);
  Vector w (0,0,FarDir.z()/2.); 

  Light *l1=new Light(UpperCorner+u*5-v*3-w*3,Color(1.,1.,1.),0,0.6);
  Light *l2=new Light(UpperCorner+u*5+v*5-w*3,Color(1.,1.,1.),0,0.6);
  //  l1->name_ = "His Ped1";
  //  l2->name_ = "His Ped2";
  scene->add_per_matl_light(l1);
  scene->add_per_matl_light(l2);
  
  add_pedestal_and_year (obj_group, glass_group, sign_name, UpperCorner, 
			 FarDir, GlassCorner, GlassDir, sign_ratio, u,v,w,l1,l2);
}

void add_north_pedestal (Group* obj_group, Group *glass_group,
			char* sign_name, const Point UpperCorner, 
			const Vector FarDir, const Point GlassCorner, 
			const Vector GlassDir, float sign_ratio, Scene *scene) {

  Vector u (FarDir.x()/2., 0, 0);
  Vector v (0,FarDir.y()/2.,0);
  Vector w (0,0,FarDir.z()/2.); 

  Light *l1=new Light(UpperCorner-u*3-v*3-w*3,Color(1.,1.,1.),0,0.6);
  Light *l2=new Light(UpperCorner+u*5-v*3-w*3,Color(1.,1.,1.),0,0.6);
  //  l1->name_ = "His Ped1";
  //  l2->name_ = "His Ped2";
  scene->add_per_matl_light(l1);
  scene->add_per_matl_light(l2);
  
  add_pedestal_and_year (obj_group, glass_group, sign_name, UpperCorner, 
			 FarDir, GlassCorner, GlassDir, sign_ratio, u,v,w,l1,l2);
}

void build_cornell_box (Group* main_group, const Point CBoxPoint, float ped_size, 
			Scene* scene) {
  Material* flat_white = new LambertianMaterial(Color(.8,.8,.8));
  // read in and place the Cornell box.
  Parallelogram *cboxfloor, *cboxceiling, *back_wall, *left_wall, *right_wall,
    *cboxlight,
    *short_block_top, *short_block_left, *short_block_right,
    *short_block_front, *short_block_back,
    *tall_block_top, *tall_block_left, *tall_block_right,
    *tall_block_front, *tall_block_back;
  Light *l = new Light(CBoxPoint+Vector(-1,1,4),Color(1.,1.,1.),0,0.7);
  l->name_ = "Cornell Box";
  scene->add_per_matl_light(l);
  flat_white->my_lights.add(l);
  
  Point cbmin(0,0,0);
  Point cbmax(556,548.8,559.2);
  Vector cbdiag = cbmax-cbmin;

  Transform cboxT;
  cboxT.pre_translate(-Vector((cbmax.x()+cbmin.x())/2.,cbmin.y(),(cbmax.z()+cbmin.z())/2.)); // center cbox over 0
  cboxT.pre_rotate(M_PI_2,Vector(1,0,0));  // make z up
  cboxT.pre_rotate(M_PI_4,Vector(0,0,1));
  double cb_scale = ped_size/(sqrt(cbdiag.x()*cbdiag.x()+cbdiag.z()*cbdiag.z()));
  cboxT.pre_scale(Vector(cb_scale,cb_scale,cb_scale)); // scale to pedestal
  cboxT.pre_translate(CBoxPoint.asVector()+Vector(0,0,1E-2));

  cboxfloor = new Parallelogram(flat_white,
			    cboxT.project(Point(0,0,0)),
			    cboxT.project(Vector(0,0,559.2)),
			    cboxT.project(Vector(556.0,0,0)));
  cboxlight = new Parallelogram(flat_white,
			    cboxT.project(Point(213,548.79,227)),
			    cboxT.project(Vector(130.,0,0)),
			    cboxT.project(Vector(0,0,105.)));
  back_wall = new Parallelogram(flat_white,
				cboxT.project(Point(0,0,559.2)),
				cboxT.project(Vector(0,548.8,0)),
				cboxT.project(Vector(556.0,0,0)));
  cboxceiling = new Parallelogram(flat_white,
			      cboxT.project(Point(0,548.8,0)),
			      cboxT.project(Vector(556,0,0)),
			      cboxT.project(Vector(0,0,559.2)));
  left_wall = new Parallelogram(flat_white,
				cboxT.project(Point(556.0,0,0)),
				cboxT.project(Vector(0,0,559.2)),
				cboxT.project(Vector(0,548.8,0)));
  right_wall = new Parallelogram(flat_white,
				 cboxT.project(Point(0,0,0)),
				 cboxT.project(Vector(0,548.8,0)),
				 cboxT.project(Vector(0,0,559.2)));
  
  
  short_block_top = new Parallelogram(flat_white,
				      cboxT.project(Point(130.0, 165.0, 65.0)),
				      cboxT.project(Vector(-48,0,160)),
				      cboxT.project(Vector(158,0,47)));
  
  short_block_left = new Parallelogram(flat_white,
				       cboxT.project(Point(288.0,   0.0, 112.0)),
				       cboxT.project(Vector(0,165,0)),
				       cboxT.project(Vector(-48,0,160)));
  
  short_block_right = new Parallelogram(flat_white,
					cboxT.project(Point(82.0,0.0, 225.0)),
					cboxT.project(Vector(0,165,0)),
					cboxT.project(Vector(48,0,-160)));
  
  short_block_front = new Parallelogram(flat_white,
					cboxT.project(Point(130.0,   0.0,  65.0)),
					cboxT.project(Vector(0,165,0)),
					cboxT.project(Vector(158,0,47)));
  short_block_back = new Parallelogram(flat_white,
				       cboxT.project(Point(240.0,   0.0, 272.0)),
				       cboxT.project(Vector(0,165,0)),
				       cboxT.project(Vector(-158,0,-47)));
  
  
  
  
  tall_block_top = new Parallelogram(flat_white,
				     cboxT.project(Point(423.0, 330.0, 247.0)),
				     cboxT.project(Vector(-158,0,49)),
				     cboxT.project(Vector(49,0,160)));
  tall_block_left = new Parallelogram(flat_white,
				      cboxT.project(Point(423.0,   0.0, 247.0)),
				      cboxT.project(Vector(0,330,0)),
				      cboxT.project(Vector(49,0,159)));
  tall_block_right = new Parallelogram(flat_white,
				       cboxT.project(Point(314.0,   0.0, 456.0)),
				       cboxT.project(Vector(0,330,0)),
				       cboxT.project(Vector(-49,0,-160)));
  tall_block_front = new Parallelogram(flat_white,
				       cboxT.project(Point(265.0,   0.0, 296.0)),
				       cboxT.project(Vector(0,330,0)),
				       cboxT.project(Vector(158,0,-49)));
  tall_block_back = new Parallelogram(flat_white,
				      cboxT.project(Point(472.0,   0.0, 406.0)),
				      cboxT.project(Vector(0,330,0)),
				      cboxT.project(Vector(-158,0,50)));
  
  Group *cornellg = new Group();
  
  cornellg->add(cboxfloor);
  cornellg->add(cboxlight);
  cornellg->add(back_wall);
  cornellg->add(cboxceiling);
  cornellg->add(left_wall);
  cornellg->add(right_wall);
  cornellg->add(short_block_top);
  cornellg->add(short_block_left);
  cornellg->add(short_block_right);
  cornellg->add(short_block_front);
  cornellg->add(short_block_back);
  cornellg->add(tall_block_top);
  cornellg->add(tall_block_left);
  cornellg->add(tall_block_right);
  cornellg->add(tall_block_front);
  cornellg->add(tall_block_back);

  char tens_buf[256];
  
  for (int i=0; i<cornellg->numObjects(); i++)
    {
      sprintf(tens_buf,"/usr/sci/data/Geometry/textures/museum/history/cbox/TENSOR.%d.rad.tex",i);
      Material *matl = new ImageMaterial(tens_buf,ImageMaterial::Clamp,
					 ImageMaterial::Clamp,1,
					 Color(0,0,0), 0);
      matl->my_lights.add(l);
      cornellg->objs[i]->set_matl(matl);
    }

  main_group->add(cornellg);
  
  // end Cornell box
}

/*********************** ROOMS START HERE *******************************/

void build_history_hall (Group* main_group, Group* no_shadow_group, Scene *scene,
			 Light* light0, Light* light1, Light* light2) {
  FILE *fp;
  char buf[MAXBUFSIZE];
  char *name;
  double x,y,z;
  int subdivlevel = 3;
  
  Group *solidg = new Group();   /* group with shadows */
  Group *clearg = new Group();   /* group with no shadow */

  /* **************** history hall **************** */

  // history hall pedestals
  Group *historyg = new Group();
  const float img_size = 1.0;     
  float img_div = 0.3;
  const float img_ht = 2.7;
  const float ped_size = 0.75;
  float ped_div = (img_size - ped_size)/2.;
  const float ped_ht = 1.0;
  float gbox_size = ped_size*0.95;
  float gbox_ht = 0.5;
  float tall_gbox_ht = 0.6;
  const float sign_ratio = 0.583;
  float diff = (ped_size-gbox_size)/2.;

  /* **************** image on North wall in history hall **************** */

  Vector NorthRight (img_size,0,0);
  Vector NorthDown (0,0,-img_size);
  Point NorthPoint (-8-img_div-img_size, -24.15-IMG_EPS, img_ht);
  Point PedPoint (0, -25.25, 0);

  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/years-blur/museum-plaque.ppm",
		      Point(-7.5, -4-IMG_EPS, img_ht+1), Vector(3,0,0),
		      Vector(0,0,-3*0.16667),historyg);


  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/gourardC-fill.ppm",
		      Point(-6.75, -4-IMG_EPS, img_ht), Vector(1.5,0,0),
		      Vector(0,0,-1.5),historyg);

  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/jelloC-fill.ppm",
		      NorthPoint, NorthRight, NorthDown,historyg);

  NorthPoint += Vector(-2*img_div-img_size, 0,0);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/herb1280C-fill.ppm",
		      NorthPoint, NorthRight, NorthDown, historyg);

  NorthPoint += Vector(-2*img_div-img_size, 0,0);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/copter-fill.ppm",
		      NorthPoint, NorthRight, NorthDown,historyg);
  PedPoint.x(NorthPoint.x()+ped_div);
  add_north_pedestal (solidg,clearg,
		      "/usr/sci/data/Geometry/textures/museum/history/years-blur/1989.ppm",
		      PedPoint+Vector(0,0,ped_ht), Vector(ped_size,ped_size,-ped_ht),
		      PedPoint+Vector(diff,diff,ped_ht+gbox_ht), Vector(gbox_size,gbox_size,-gbox_ht),sign_ratio,scene);
  Point CopterPt (PedPoint+Vector(0,ped_size,ped_ht));

  NorthPoint += Vector(-2*img_div-img_size, 0,0);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/museumC-fill.ppm",
      
		      NorthPoint, NorthRight, NorthDown,historyg);

  NorthPoint += Vector(-2*img_div-img_size, 0,0);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/tinC-fill.ppm",
		      NorthPoint, NorthRight, NorthDown,historyg);

  //  cerr << "North Wall: " << NorthPoint << endl;
  
  /* **************** image on East wall in history hall **************** */
  img_div = 0.25;
  Vector EastRight (0,-img_size,0);
  Vector EastDown (0,0,-img_size);
  PedPoint = Point (-4.625, 0, ped_ht);
  
  Point EastPoint (-4-IMG_EPS, -7-img_div, img_ht);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/phongC-fill.ppm",
		      EastPoint, EastRight, EastDown,historyg);
  PedPoint.y(EastPoint.y()-ped_div);
  add_east_pedestal (solidg,clearg,
		     "/usr/sci/data/Geometry/textures/museum/history/years-blur/1973.ppm",
		     PedPoint-Vector(ped_size,ped_size,0), Vector(ped_size,ped_size,-ped_ht),
		     PedPoint-Vector(ped_size-diff,ped_size-diff,-gbox_ht), Vector(gbox_size,gbox_size,-gbox_ht), sign_ratio, scene);
  Point PhongPt (PedPoint-Vector(ped_size/2.,ped_size/2.,0));  

  EastPoint -= Vector(0, 2*img_div+img_size, 0); 
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/eggC-fill.ppm",
		      EastPoint, EastRight, EastDown,historyg);

  EastPoint -= Vector(0, 2*img_div+img_size, 0); 
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/blinnC-fill.ppm",
		      EastPoint, EastRight, EastDown, historyg);

  PedPoint.y(EastPoint.y()-ped_div);
  add_east_pedestal (solidg,clearg,
		     "/usr/sci/data/Geometry/textures/museum/history/years-blur/1978.ppm",
		     PedPoint-Vector(ped_size,ped_size,0),
		     Vector(ped_size,ped_size,-ped_ht),
		     PedPoint-Vector(ped_size-diff,ped_size-diff,-tall_gbox_ht),Vector(gbox_size,gbox_size,-tall_gbox_ht), sign_ratio, scene);
  Point BumpMapPoint (PedPoint-Vector(ped_size/2.,ped_size/2.,0));

  EastPoint -= Vector(0, 2*img_div+img_size, 0); 
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/rthetaC-fill.ppm",
		      EastPoint, EastRight, EastDown, historyg);

  EastPoint -= Vector(0, 2*img_div+img_size, 0); 
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/vasesC-fill.ppm",
		      EastPoint, EastRight, EastDown,historyg);

  EastPoint -= Vector(0, 2*img_div+img_size, 0); 
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/ringsC-fill.ppm",
		      EastPoint, EastRight, EastDown,historyg);
  PedPoint.y(EastPoint.y()-ped_div);
  add_east_pedestal (solidg,clearg,
		     "/usr/sci/data/Geometry/textures/museum/history/years-blur/1982.ppm",
		     PedPoint-Vector(ped_size,ped_size,0),Vector(ped_size,ped_size,-ped_ht),
		     PedPoint-Vector(ped_size-diff,ped_size-diff,-gbox_ht),Vector(gbox_size,gbox_size,-gbox_ht), sign_ratio, scene);
  Point RingsPoint (PedPoint-Vector(ped_size/2.,ped_size/2.,0)); 	
		
  EastPoint -= Vector(0, 2*img_div+img_size, 0); 
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/reyesC-fill.ppm",
		      EastPoint, EastRight, EastDown, historyg);

  EastPoint -= Vector(0, 2*img_div+img_size, 0); 
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/boxmontageC-fill.ppm",
		      EastPoint, EastRight, EastDown,historyg);
  PedPoint.y(EastPoint.y()-ped_div);
  Point CBoxPoint (PedPoint-Vector(ped_size/2.,ped_size/2.,0)); 
  add_east_pedestal (solidg,clearg,
		     "/usr/sci/data/Geometry/textures/museum/history/years-blur/1984.ppm",
		     PedPoint-Vector(ped_size-diff,ped_size-diff,0),Vector(ped_size,ped_size,-ped_ht),
		     PedPoint-Vector(ped_size,ped_size,-tall_gbox_ht),Vector(gbox_size,gbox_size,-tall_gbox_ht), sign_ratio, scene);
#if INSERTHISTORYMODELS
  build_cornell_box (historyg, CBoxPoint, gbox_size, scene);
#endif
  
  EastPoint -= Vector(0, 2*img_div+img_size, 0); 
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/museum-4.ppm",
		      EastPoint, EastRight, EastDown,historyg);

  EastPoint -= Vector(0, 2*img_div+img_size, 0); 
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/perlin.ppm",
		      EastPoint, EastRight, EastDown,historyg);
  PedPoint.y(EastPoint.y()-ped_div);
  add_east_pedestal (solidg,clearg,
		     "/usr/sci/data/Geometry/textures/museum/history/years-blur/1985.ppm",
		     PedPoint-Vector(ped_size,ped_size,0),Vector(ped_size,ped_size,-ped_ht),
		     PedPoint-Vector(ped_size-diff,ped_size-diff,-tall_gbox_ht),Vector(gbox_size,gbox_size,-tall_gbox_ht), sign_ratio, scene);
  Point PerlinPt (PedPoint-Vector(ped_size/2.,ped_size/2.,0)); 

  EastPoint -= Vector(0, 2*img_div+img_size, 0); 
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/mapleC-fill.ppm",
		      EastPoint, EastRight, EastDown, historyg);

  EastPoint -= Vector(0, 2*img_div+img_size, 0); 
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/luxoC-fill.ppm",
		      EastPoint, EastRight, EastDown, historyg);

  EastPoint -= Vector(0, 2*img_div+img_size, 0); 
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/chessC-fill.ppm",
		      EastPoint, EastRight, EastDown, historyg);
  PedPoint.y(EastPoint.y()-ped_div);
  add_east_pedestal (solidg,clearg,
		     "/usr/sci/data/Geometry/textures/museum/history/years-blur/1986.ppm",
		     PedPoint-Vector(ped_size,ped_size,0),Vector(ped_size,ped_size,-ped_ht),
		     PedPoint-Vector(ped_size-diff,ped_size-diff,-gbox_ht),Vector(gbox_size,gbox_size,-gbox_ht), sign_ratio, scene);
  Point ChessPt (PedPoint-Vector(ped_size/2.,ped_size/2.,0)); 

  EastPoint -= Vector(0, 2*img_div+img_size, 0); 
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/dancersC-fill.ppm",
		      EastPoint, EastRight, EastDown,historyg);

  //  cerr << "East Wall:  " << EastPoint-Vector(0,img_size,0) << endl;

  /* **************** image on West wall in history hall **************** */
  
  img_div = 0.21;
  Vector WestRight (0,img_size,0);
  Vector WestDown (0,0,-img_size);
  Point WestPoint (-7.85+IMG_EPS, -4-img_div-img_size, img_ht);

  PedPoint = Point (-7.375, 0, 0);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/VWC-fill.ppm",
		      WestPoint, WestRight, WestDown,historyg);
  PedPoint.y(WestPoint.y()+ped_div);
  add_west_pedestal (solidg,clearg,
		     "/usr/sci/data/Geometry/textures/museum/history/years-blur/1973.ppm",
		     PedPoint+Vector(0,0,ped_ht),Vector(ped_size,ped_size,-ped_ht),
		     PedPoint+Vector(0,0,ped_ht+gbox_ht),Vector(ped_size,ped_size,-gbox_ht),sign_ratio,scene);
  Vector VWVector (PedPoint.vector()+Vector(ped_size/2.,ped_size/2.,ped_ht));

  WestPoint -= Vector (0, 2*img_div+img_size, 0);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/catmullC-fill.ppm",
		      WestPoint, WestRight, WestDown, historyg);
  
  WestPoint -= Vector (0, 2*img_div+img_size, 0);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/newellC-fill.ppm",
		      WestPoint, WestRight, WestDown,historyg);
  PedPoint.y(WestPoint.y()+ped_div);
  add_west_pedestal (solidg,clearg,
		     "/usr/sci/data/Geometry/textures/museum/history/years-blur/1974.ppm",
		     PedPoint+Vector(0,0,ped_ht),Vector(ped_size,ped_size,-ped_ht),
		     PedPoint+Vector(0,0,ped_ht+gbox_ht),Vector(ped_size,ped_size,-gbox_ht),sign_ratio,scene);
  Point NewellPt (PedPoint+Vector(ped_size/2., ped_size/2., ped_ht));
  
  WestPoint -= Vector (0, 2*img_div+img_size, 0);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/tea-potC-fill.ppm",
		      WestPoint, WestRight, WestDown,historyg);
  PedPoint.y(WestPoint.y()+ped_div);
  add_west_pedestal (solidg,clearg,
		     "/usr/sci/data/Geometry/textures/museum/history/years-blur/1975.ppm",
		     PedPoint+Vector(0,0,ped_ht),Vector(ped_size,ped_size,-ped_ht),
		     PedPoint+Vector(diff,diff,ped_ht+tall_gbox_ht),Vector(gbox_size,gbox_size,-tall_gbox_ht), sign_ratio, scene);
  Vector TeapotVector (PedPoint.vector()+Vector(ped_size/2.,ped_size/2.,ped_ht));

  WestPoint -= Vector (0, 2*img_div+img_size, 0);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/maxC-fill.ppm",
		      WestPoint, WestRight, WestDown,historyg);

  WestPoint -= Vector (0, 2*img_div+img_size, 0);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/blurC-fill.ppm",
		      WestPoint, WestRight, WestDown,historyg);

  WestPoint -= Vector (0, 2*img_div+img_size, 0);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/recursive-rt-fill.ppm",
		      WestPoint, WestRight, WestDown, historyg);
  PedPoint.y(WestPoint.y()+ped_div);
  add_west_pedestal (solidg,clearg,
			 "/usr/sci/data/Geometry/textures/museum/history/years-blur/1980.ppm",
			 PedPoint+Vector(0,0,ped_ht),Vector(ped_size,ped_size,-ped_ht),
			 PedPoint+Vector(diff,diff,ped_ht+gbox_ht),Vector(gbox_size,gbox_size,-gbox_ht), sign_ratio, scene);
  Point RTPoint (PedPoint+Vector(ped_size/2., ped_size/2., ped_ht));

  WestPoint -= Vector (0, 2*img_div+img_size, 0);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/tron.ppm",
		      WestPoint, WestRight, WestDown, 
		      historyg);
		      
  WestPoint -= Vector (0, 2*img_div+img_size, 0);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/morphineC-fill.ppm",
		      WestPoint, WestRight, WestDown, 
		      historyg);
  PedPoint.y(WestPoint.y()+ped_div);
  add_west_pedestal (solidg,clearg,
		     "/usr/sci/data/Geometry/textures/museum/history/years-blur/1983.ppm",
		     PedPoint+Vector(0,0,ped_ht),Vector(ped_size,ped_size,-ped_ht),
		     PedPoint+Vector(diff,diff,ped_ht+tall_gbox_ht),Vector(gbox_size,gbox_size,-tall_gbox_ht), sign_ratio, scene);
  Point MorphinePt(PedPoint.vector()+Vector(ped_size/2.,ped_size/2.,ped_ht));
  
  WestPoint -= Vector (0, 2*img_div+img_size, 0);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/beeC-fill.ppm",
		      WestPoint, WestRight, WestDown, 
		      historyg);

  WestPoint -= Vector (0, 2*img_div+img_size, 0);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/ballsC-fill.ppm",
		      WestPoint, WestRight, WestDown, 
 		      historyg);
  PedPoint.y(WestPoint.y()+ped_div);
  add_west_pedestal (solidg,clearg,
		     "/usr/sci/data/Geometry/textures/museum/history/years-blur/1984.ppm",
		     PedPoint+Vector(0,0,ped_ht),Vector(ped_size,ped_size,-ped_ht),
		     PedPoint+Vector(diff,diff,ped_ht+gbox_ht),Vector(gbox_size,gbox_size,-gbox_ht), sign_ratio, scene);
  Point BallsPoint (PedPoint+Vector(ped_size/2., ped_size/2., ped_ht));
  
  WestPoint -= Vector (0, 2*img_div+img_size, 0);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/girlC-fill.ppm",
		      WestPoint, WestRight, WestDown, 
		      historyg);

  WestPoint -= Vector (0, 2*img_div+img_size, 0);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/kitchenC-fill.ppm",
		      WestPoint, WestRight, WestDown, 
		      historyg);

  WestPoint -= Vector (0, 2*img_div+img_size, 0);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/BdanceC-fill.ppm",
		      WestPoint, WestRight, WestDown, 
		      historyg);

  WestPoint = Point (-20+IMG_EPS+.1, -27, img_ht+0.4);

#if INSERTVIDEO
  Material* video = new VideoMap ("/usr/sci/data/Geometry/textures/museum/history/morph/huge/sci-morph%d.ppm",500,5,Color(0.7,.7,.7),20,0,20);
//    Material* video = new VideoMap ("/usr/sci/data/Geometry/textures/museum/history/morph/huge/sci-morph%d.ppm",1918,10,Color(0.7,.7,.7),20,0,10);
//    Material* video = new VideoMap ("/usr/sci/data/Geometry/textures/museum/history/morph/huge/sci-morph%d.ppm",100,10,Color(0.7,.7,.7),20,0);
  add_video_on_wall (video,WestPoint+Vector(0.125,0,1.3), Vector(0,2.5,0), 
		     Vector(0,0,-2.5),historyg); 
#else
  add_image_on_wall ("/usr/sci/data/Geometry/textures/museum/history/morph/sci-morph1275.ppm",
		      WestPoint+Vector(0.125,0,1.3), Vector(0,2.5,0), Vector(0,0,-2.5),
		      historyg); 
#endif

  Group *tvg = new Group();
  Transform t;
  /*  x is actually y
      y is actually x
      z is z
   */
  t.pre_scale (Vector(0.0165,0.01,0.022));
  t.pre_rotate (-M_PI_2,Vector(0,0,1));
  t.pre_translate (WestPoint.vector()+Vector(0,1.25,-1.6));
  if (!readObjFile("/usr/sci/data/Geometry/models/museum/museum-obj/television.obj",
		   "/usr/sci/data/Geometry/models/museum/museum-obj/television.mtl",
		   t, tvg)) {
    exit(0);
  }
  historyg->add(new Grid(tvg,10));

  //  cerr << "West Wall:  " << WestPoint << endl;

  /* **************** image on South wall in history hall **************** */
  img_div = .22;
  Vector SouthRight (-img_size, 0, 0);
  Vector SouthDown (0,0,-img_size);
  Point SouthPoint (-4-img_div, -28+IMG_EPS, img_ht);
  
  PedPoint = Point (-0,-26.5,ped_ht);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/vermeerC-fill.ppm",
		      SouthPoint, SouthRight, SouthDown, 
		      historyg);		      

  SouthPoint -= Vector(2*img_div+img_size,0,0);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/dreamC-fill.ppm",
		      SouthPoint, SouthRight, SouthDown, 
		      historyg);
  
  SouthPoint -= Vector(2*img_div+img_size,0,0);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/factoryC-fill.ppm",
		      SouthPoint, SouthRight, SouthDown,
		      historyg);
  
  SouthPoint -= Vector(2*img_div+img_size,0,0);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/dragon.ppm",
		      SouthPoint, SouthRight, SouthDown, 
		      historyg);

  SouthPoint -= Vector(2*img_div+img_size,0,0);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/knickC-fill.ppm",
		      SouthPoint, SouthRight, SouthDown, 
		      historyg);
  
  SouthPoint -= Vector(2*img_div+img_size,0,0);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/painterfig2P-fill.ppm",
		      SouthPoint, SouthRight, SouthDown, 
		      historyg);
  
  SouthPoint -= Vector(2*img_div+img_size,0,0);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/towerC-fill.ppm",
		      SouthPoint, SouthRight, SouthDown, 
		      historyg);

  SouthPoint -= Vector(2*img_div+img_size,0,0);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/SpatchIllustrationC-fill.ppm",
		      SouthPoint, SouthRight, SouthDown, 
		      historyg);
  
  SouthPoint -= Vector(2*img_div+img_size,0,0);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/accC-fill.ppm",
		      SouthPoint, SouthRight, SouthDown, 
		      historyg);
  
  SouthPoint -= Vector(2*img_div+img_size,0,0);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/openglC-fill.ppm",
		      SouthPoint, SouthRight, SouthDown, 
		      historyg);

  SouthPoint -= Vector(2*img_div+img_size,0,0);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/history/space_cookiesC-fill.ppm",
		      SouthPoint, SouthRight, SouthDown, 
		      historyg);

  /* south wall couch */
  Transform sofa_trans;
  Vector sofa_center (-15,-27.1,0);

  // first, get it centered at the origin (in x and y), and scale it
  sofa_trans.pre_translate(Vector(0,-445,0));
  sofa_trans.pre_scale(Vector(0.001, 0.001, 0.002));

  // now rotate/translate it to the right angle/position

  for (int i=0; i<2; i++) {
    Array1<Material*> matl;
    Transform t = sofa_trans;
    t.pre_translate(sofa_center+Vector(6*i,0,0));
    Group *couchg = new Group();
    if (!readObjFile("/usr/sci/data/Geometry/models/museum/bench.obj",
		     "/usr/sci/data/Geometry/models/museum/bench.mtl",
		     t, matl, couchg)) {
      exit(0);
    }
    for (int k = 0; k<matl.size(); k++) {
      matl[k]->my_lights.add(light1);
      matl[k]->my_lights.add(light2);
    }
    historyg->add(new Grid(couchg,10));
  }

  //  cerr << "South Wall: " << SouthPoint-Vector(img_size, 0,0) << endl;

#if INSERTHISTORYMODELS
  /* **************** teapot **************** */
  Material* teapot_silver = new MetalMaterial( Color(0.8, 0.8, 0.8),20);
  ImageMaterial* wood = new ImageMaterial("/usr/sci/data/Geometry/models/livingroom/livingroom-obj2_fullpaths/maps/bubing_2.ppm",
		      ImageMaterial::Tile, ImageMaterial::Tile,
		      1, Color(0,0,0), 0); 
  Light *l;
  l = (new Light(TeapotVector.point()+Vector(1,1,4),Color(1.,1.,1.),0,0.7));
  l->name_ = "teapot";
  scene->add_per_matl_light (l);
  teapot_silver->my_lights.add(l);
  wood->my_lights.add(l);

  historyg->add (new Parallelogram(wood,
				   TeapotVector.point()-Vector(ped_size/2.-diff,ped_size/2.-diff,-0.001),
				   Vector(0,gbox_size,0),Vector(gbox_size,0,0)));

  Transform teapotT;

  teapotT.pre_rotate(M_PI_4,Vector(0,0,1));
  teapotT.pre_scale(Vector(0.003, 0.003, 0.003));
  teapotT.pre_translate(TeapotVector);
  
  fp = fopen("/usr/sci/data/Geometry/models/teapot.dat","r");
  
   Group* teapot_g = new Group();

  while (fscanf(fp,"%s",buf) != EOF) {
    if (!strcasecmp(buf,"bezier")) {
      int numumesh, numvmesh, numcoords=3;
      Mesh *m;
      Bezier *b;
      Point p;
      
      fscanf(fp,"%s",buf);
      name = new char[strlen(buf)+1];
      strcpy(name,buf);
      
      fscanf(fp,"%d %d %d\n",&numumesh,&numvmesh,&numcoords);
          m = new Mesh(numumesh,numvmesh);
          for (int j=0; j<numumesh; j++) {
            for (int k=0; k<numvmesh; k++) {
              fscanf(fp,"%lf %lf %lf",&x,&y,&z);
              p = teapotT.project(Point(x,y,z));
              m->mesh[j][k] = p;
            }
          }
          b = new Bezier(teapot_silver,m);
          b->SubDivide(subdivlevel,.5);
          teapot_g->add(b->MakeBVH());
    }
  }
  fclose(fp);

  historyg->add(new Grid(teapot_g,10));

  /* **************** car **************** */
  Material* lightblue = new LambertianMaterial(Color(.4,.67,.90));
  Material* vwmat=new Phong (Color(.6,.6,0),Color(.5,.5,.5),30);
  l = (new Light(VWVector.point()+Vector(1,-1,4),Color(1.,1.,1.),0,0.7));  
  l->name_ = "vw bug";
  scene->add_per_matl_light (l);
  lightblue->my_lights.add(l);
  vwmat->my_lights.add(l);

  historyg->add (new Parallelogram(lightblue,
				   VWVector.point()-Vector(ped_size/2.-diff,ped_size/2.-diff,-0.001),
				   Vector(0,gbox_size,0),Vector(gbox_size,0,0)));
  fp = fopen("/usr/sci/data/Geometry/models/vw.geom","r");
  if (!fp) {
    fprintf(stderr,"No such file!\n");
    exit(-1);
  }
  
  int vertex_count,polygon_count,edge_count;
  int numverts;
  Transform vwT;
  int pi0, pi1, pi2;

  vwT.pre_scale(Vector(0.004,0.004,0.004));
  vwT.pre_rotate(M_PI_2,Vector(0,1,0));
  vwT.pre_rotate(M_PI_2,Vector(1,0,0));
  vwT.pre_rotate(-3.*M_PI_4/2.,Vector(0,0,1));
  //  vwT.pre_translate(Vector(-7,-10.83,ped_ht));
  //  vwT.pre_translate(Vector(-5,-8.83,ped_ht));
  vwT.pre_translate(VWVector);

  fscanf(fp,"%d %d %d\n",&vertex_count,&polygon_count,&edge_count);
  
  double (*vert)[3] = new double[vertex_count][3];

  for (int i=0; i<vertex_count; i++) 
      fscanf(fp,"%lf %lf %lf",&vert[i][0],&vert[i][1],&vert[i][2]);
  Group* vw=new Group();
  while(fscanf(fp,"%d %d %d %d",&numverts, &pi0, &pi1, &pi2) != EOF) 
  {
      
      vw->add(new Tri(vwmat,
                     vwT.project(Point(vert[pi0-1][0],vert[pi0-1][1],vert[pi0-1][2])),
                     vwT.project(Point(vert[pi1-1][0],vert[pi1-1][1],vert[pi1-1][2])),
                     vwT.project(Point(vert[pi2-1][0],vert[pi2-1][1],vert[pi2-1][2]))));
      
      for (int i=0; i<numverts-3; i++) 
      {
          pi1 = pi2;
          fscanf(fp,"%d",&pi2);
	  Tri* t;
	  if(pi0 != pi2 && pi1 != pi2 && pi0 != pi1){
	      vw->add((t=new Tri(vwmat,
				 vwT.project(Point(vert[pi0-1][0],vert[pi0-1][1],vert[pi0-1][2])),
				 vwT.project(Point(vert[pi1-1][0],vert[pi1-1][1],vert[pi1-1][2])),
				 vwT.project(Point(vert[pi2-1][0],vert[pi2-1][1],vert[pi2-1][2])))));
	      
	      if(t->isbad()){
		  cerr << "BAD: " << pi0 << ", " << pi1 << ", " << pi2 << '\n';
	      }
	  }
      }
  }
  historyg->add(new Grid(vw,15));
  /* **************** bump-mapped sphere **************** */
  Material* blue = new LambertianMaterial(Color(.08,.08,.62));
  historyg->add (new Parallelogram(blue,
				   BumpMapPoint+Vector(ped_size/2.-diff,ped_size/2.-diff,0.001),
				   Vector(0,-gbox_size,0),Vector(-gbox_size,0,0)));
  Material* orange = 
    new ImageMaterial("/usr/sci/data/Geometry/textures/museum/history/orange3.ppm",
		      ImageMaterial::Clamp, ImageMaterial::Clamp,1,Color(0,0,0),0); 

  l = (new Light(BumpMapPoint+Vector(-1,1,4),Color(1.,1.,1.),0,0.7));
  l->name_ = "Bump Map";
  scene->add_per_matl_light (l);
  blue->my_lights.add(l);
  orange->my_lights.add(l);
  
  /*  
  Material* orange = new Phong(Color(.7,.4,.0),Color(.2,.2,.2),40);

  historyg->add (new Sphere ( new PerlinBumpMaterial(new Phong(Color(.72,.27,.0),Color(.2,.2,.2),50)), BumpMapPoint+Vector(0,0,0.3),0.2));  
  historyg->add (new UVSphere ( new Phong(Color(.41,.39,.16),Color(.2,.2,.2),50),  */
  historyg->add (new UVSphere (orange,
			       BumpMapPoint+Vector(0,0,0.3),0.2)); 

  /* **************** ray-traced scene **************** */
  Material* outside_glass= new DielectricMaterial(1.5, 1.0, 0.04, 400.0, 
					  Color(.80, .93 , .87), 
					  Color(1,1,1), false, 0.001);
  Material* inv_glass= new DielectricMaterial(1.0, 1.5, 0.04, 400.0, 
					  Color(.80, .93 , .87), 
					  Color(1,1,1), true, 0.001);
  Material* silver = new MetalMaterial( Color(0.8, 0.8, 0.8),20);

  Material* rtchessbd = 
    new ImageMaterial("/usr/sci/data/Geometry/textures/museum/misc/recursive.ppm",
		      ImageMaterial::Clamp, ImageMaterial::Clamp,
		      1, Color(0,0,0), 0); 

  l = (new Light(RTPoint+Vector(1,1,4),Color(1.,1.,1.),0,0.7));
  l->name_ = "Whitted";
  scene->add_per_matl_light (l);
  outside_glass->my_lights.add(l);
  inv_glass->my_lights.add(l);
  silver->my_lights.add(l);
  rtchessbd->my_lights.add(l);

  historyg->add (new Sphere(outside_glass, RTPoint+Vector(0.25,-0.1,0.3),0.1));
  historyg->add (new Sphere(inv_glass, RTPoint+Vector(0.25,-0.1,0.3),0.099));
  //  historyg->add (new Sphere(clearg, RTPoint+Vector(0.25,-0.1,0.3),0.1));  
  historyg->add (new Sphere(silver, RTPoint+Vector(0,0.1,0.2),0.08));
  /* -eye -5.43536 -13.0406 2 -lookat -15.9956 -12.5085 2 -up 0 0 1 -fov 60*/
  historyg->add (new Parallelogram(rtchessbd,
				   RTPoint-Vector(ped_size/2.-diff,ped_size/2.-diff,-0.001),
				   Vector(0,gbox_size,0),Vector(gbox_size,0,0)));
  
  /* **************** Saturn scene **************** */
  Material* flat_white = new LambertianMaterial(Color(.8,.8,.8));
  Material* flat_grey = new LambertianMaterial(Color(.4,.4,.4));
  Material* saturn_black = new LambertianMaterial(Color(0.08,.08,.1));
  Material* Saturn_color = new ImageMaterial("/usr/sci/data/Geometry/textures/museum/history/saturn.ppm",
					   ImageMaterial::Clamp,
					   ImageMaterial::Clamp, 1,
					   Color(0,0,0), 0);

  l = (new Light(RingsPoint+Vector(-1,1,4),Color(1.,1.,1.),0,0.7));
  l->name_ = "Saturn";
  scene->add_per_matl_light (l);
  flat_white->my_lights.add(l);
  flat_grey->my_lights.add(l);
  saturn_black->my_lights.add(l);
  Saturn_color->my_lights.add(l);

  historyg->add (new Parallelogram(saturn_black,
				   RingsPoint+Vector(ped_size/2.-diff,ped_size/2.-diff,0.001),
				   Vector(0,-gbox_size,0),Vector(-gbox_size,0,0)));

  historyg->add (new UVSphere(Saturn_color, RingsPoint+Vector(0,0,0.25),0.15,
		   Vector(-.2,-.25,1))); 
  historyg->add (new Ring(flat_grey, RingsPoint+Vector(0,0,0.25),
			  Vector(-.2,-.25,1),0.18,0.03));  
  historyg->add (new Ring(flat_white, RingsPoint+Vector(0,0,0.25),
			  Vector(-.2,-.25,1),0.2105,0.07));  
  historyg->add (new Ring(flat_grey, RingsPoint+Vector(0,0,0.25),
			  Vector(-.2,-.25,1),0.281,0.01));  
  historyg->add (new Ring(flat_white, RingsPoint+Vector(0,0,0.25),
			  Vector(-.2,-.25,1),0.2915,0.04));  

  /* **************** Billiard Balls **************** */
  Material* balls_black = new LambertianMaterial(Color(0.08,.08,.1));
  historyg->add (new Parallelogram(balls_black,
				   BallsPoint-Vector(ped_size/2.-diff,ped_size/2.-diff,-0.001),
				   Vector(0,gbox_size,0),Vector(gbox_size,0,0))); 
  Material* ball1 = new ImageMaterial("/usr/sci/data/Geometry/textures/museum/history/1ball_s1.ppm",
				      ImageMaterial::Clamp,
				      ImageMaterial::Clamp, 1,
				      Color(1.0,1.0,1.0), 50, 0.05, false);
  Material* ball4 = new ImageMaterial("/usr/sci/data/Geometry/textures/museum/history/4ball_s.ppm",
				      ImageMaterial::Clamp,
				      ImageMaterial::Clamp, 1,
				      Color(1.0,1.0,1.0), 50, 0.05, false);
  Material* ball8 = new ImageMaterial("/usr/sci/data/Geometry/textures/museum/history/8ball_s.ppm",
				      ImageMaterial::Clamp,
				      ImageMaterial::Clamp, 1,
				      Color(1.0,1.0,1.0), 50, 0.05, false);
  Material* ball9 = new ImageMaterial("/usr/sci/data/Geometry/textures/museum/history/9ball_s1.ppm",
				      ImageMaterial::Clamp,
				      ImageMaterial::Clamp, 1,
				      Color(1.0,1.0,1.0), 50, 0.05, false);
  Material* white = new PhongMaterial(Color(.9,.9,.7),1,0.05,50,false);

  l = (new Light(BallsPoint+Vector(1,1,4),Color(1.,1.,1.),0,0.7));
  l->name_ = "Billiards";
  scene->add_per_matl_light (l);
  balls_black->my_lights.add(l);
  ball1->my_lights.add(l);
  ball4->my_lights.add(l);
  ball8->my_lights.add(l);
  ball9->my_lights.add(l);
  white->my_lights.add(l);

  historyg->add (new UVSphere(ball1, BallsPoint+Vector(-0.1,-0.2,0.07),0.07,
			      Vector(-0.2,0.2,1),Vector(0,1,0)));
  historyg->add (new UVSphere(ball9, BallsPoint+Vector(0,-0.03,0.07),0.07));
  historyg->add (new Sphere(white, BallsPoint+Vector(0.23,-0.05,0.07),0.07));
  historyg->add (new UVSphere(ball8, BallsPoint+Vector(-0.1,0.18,0.07),0.07,
			      Vector(0,0,1),Vector(0,1,0)));
  historyg->add (new UVSphere(ball4, BallsPoint+Vector(-0.15,0.29,0.07),0.07,
			      Vector(0,-.1,.9),Vector(-.15,.85,0)));

  /* **************** Newell's Chess Scene **************** */
  Material* newchessbd = 
    new ImageMaterial("/usr/sci/data/Geometry/textures/museum/misc/newell.ppm",
		      ImageMaterial::Clamp, ImageMaterial::Clamp,
		      1, Color(0,0,0), 0); 

  historyg->add (new Parallelogram(newchessbd,
				   NewellPt-Vector(ped_size/2.-diff,ped_size/2.-diff,-0.001),
				   Vector(0,gbox_size,0),Vector(gbox_size,0,0)));

  l = new Light(NewellPt+Vector(1,1,4),Color(1.,1.,1.),0,0.5);
  l->name_ = "Newell";
  scene->add_per_matl_light (l);
  newchessbd->my_lights.add(l);

  Group *pawn_g = new Group();

  for (int i = 0; i<4; i++) 
    for (int j = 0; j<6; j++) { 
      t.load_identity();
    t.pre_translate(NewellPt.vector()+Vector(i*ped_size/8.-ped_size/16.,j*ped_size/8.-5*ped_size/16.,0.));
    Array1<Material*> matl;
    if (!readObjFile("/usr/sci/data/Geometry/models/museum/pawn.obj",
		     "/usr/sci/data/Geometry/models/museum/pawn2.mtl",
		     t, matl, pawn_g)) {
      exit(0);
    }  
    for (int k = 0; k<matl.size(); k++)
      matl[k]->my_lights.add(l);
  }
  
  historyg->add(new HierarchicalGrid(pawn_g,5,5,5,4,16,4));

  /* **************** Kajiya's Chess Scene **************** */
  Material* kaj_white = new Phong(Color(.95,.95,.85),Color(.2,.2,.2),40);
  Material* pink = new LambertianMaterial(Color(.78,.59,.50));
  Material* kaj_glass= new DielectricMaterial(1.5, 1.0, 0.05, 400.0, 
					  Color(.80, .93 , .87), 
					      Color(.40,.93,.47), true, 3);

  l = (new Light(ChessPt+Vector(-1,1,4),Color(1.,1.,1.),0,0.7));
  l->name_ = "Kajiya";
  scene->add_per_matl_light (l);
  kaj_white->my_lights.add(l);
  pink->my_lights.add(l);
  kaj_glass->my_lights.add(l);

  Group* kajiya_g = new Group();
  kajiya_g->add (new Box(kaj_white,ChessPt+Vector(-0.26,0.08,0),ChessPt+Vector(-0.10,0.24,0.03)));
  kajiya_g->add (new Box(kaj_white,ChessPt+Vector(-0.22,0.12,0.03),ChessPt+Vector(-0.14,0.20,0.20)));
  kajiya_g->add (new Sphere(kaj_glass, ChessPt+Vector(-0.18,0.16,0.23),0.03));

  kajiya_g->add (new Box(kaj_white,ChessPt+Vector(-0.32,-0.30,0),ChessPt+Vector(-0.16,-0.14,0.03)));
  kajiya_g->add (new Box(kaj_white,ChessPt+Vector(-0.28,-0.26,0.03),ChessPt+Vector(-0.20,-0.18,0.20)));
  kajiya_g->add (new Sphere(kaj_glass, ChessPt+Vector(-0.24,-0.22,0.23),0.03));

  kajiya_g->add (new Box(kaj_white,ChessPt+Vector(0.07,0.08,0),ChessPt+Vector(0.23,0.24,0.03)));
  kajiya_g->add (new Box(kaj_white,ChessPt+Vector(0.11,0.12,0.03),ChessPt+Vector(0.19,0.20,0.20)));
  kajiya_g->add (new Sphere(kaj_glass, ChessPt+Vector(0.15,0.16,0.23),0.03));

  kajiya_g->add (new Sphere(kaj_glass, ChessPt+Vector(-0.04,0.01,0.03),0.03));
  kajiya_g->add (new Sphere(kaj_glass, ChessPt+Vector(-0.07,-0.02,0.03),0.03));
  kajiya_g->add (new Sphere(kaj_glass, ChessPt+Vector(-0.1,-0.05,0.03),0.03));
  kajiya_g->add (new Sphere(kaj_glass, ChessPt+Vector(-0.07,-0.08,0.03),0.03));
  kajiya_g->add (new Sphere(kaj_glass, ChessPt+Vector(-0.04,-0.11,0.03),0.03));
  kajiya_g->add (new Sphere(kaj_glass, ChessPt+Vector(-0.04,-0.05,0.03),0.03));

  kajiya_g->add (new Sphere(kaj_glass, ChessPt+Vector(-0.055,-0.02,0.08),0.03));
  kajiya_g->add (new Sphere(kaj_glass, ChessPt+Vector(-0.085,-0.05,0.08),0.03));
  kajiya_g->add (new Sphere(kaj_glass, ChessPt+Vector(-0.055,-0.08,0.08),0.03));

  kajiya_g->add (new Sphere(kaj_glass, ChessPt+Vector(-0.07,-0.05,0.13),0.03));

  kajiya_g->add (new Parallelogram(pink,
				   ChessPt+Vector(ped_size/2.-diff,ped_size/2.-diff,0.001),
				   Vector(0,-gbox_size,0),Vector(-gbox_size,0,0)));
  historyg->add(new Grid(kajiya_g,7));

  /* **************** Phong Glass Scene **************** */

  Transform phong_glass;
  Material* clear = new PhongMaterial (Color(0.1,0.3,0.4),0.1,0.5,100,true);  
  Material* phchessbd = 
    new ImageMaterial("/usr/sci/data/Geometry/textures/museum/misc/phong-bk.ppm",
		      ImageMaterial::Clamp, ImageMaterial::Clamp,
		      1, Color(0,0,0), 0); 

  l = (new Light(PhongPt+Vector(-1,1,4),Color(1.,1.,1.),0,0.7));
  l->name_ = "Phong";
  scene->add_per_matl_light (l);
  clear->my_lights.add(l);
  phchessbd->my_lights.add(l);

  //phong_glass.pre_rotate(M_PI_2,Vector(1,0,0));
  //  phong_glass.pre_translate(Vector(0,-0.0633,0));
  phong_glass.pre_scale(Vector(3,3,3));
  historyg->add (new Parallelogram(phchessbd,
				   PhongPt+Vector(ped_size/2.-diff,ped_size/2.-diff,0.001),
				   Vector(0,-gbox_size,0),Vector(-gbox_size,0,0)));
  t=phong_glass;
  t.pre_translate(PhongPt.vector());

  Group *phong_g = new Group();
  if (!readObjFile("/usr/sci/data/Geometry/models/museum/phong-glass.obj",
		   "/usr/sci/data/Geometry/models/museum/phong-clear.mtl",
		   t, phong_g, 0, clear)) {
    exit(0);
  }  

  historyg->add(new Grid(phong_g,15));

  /* **************** Perlin vase **************** */
  Material* vase_black = new LambertianMaterial(Color(0.08,.08,.1));
  historyg->add (new Parallelogram(vase_black,
				   PerlinPt+Vector(ped_size/2.-diff,ped_size/2.-diff,0.001),
				   Vector(0,-gbox_size,0),Vector(-gbox_size,0,0)));

  Material* perlin_marble
    = new CrowMarble(8, Vector(-.3, -.3, 1), Color(.98,.82,.78),
		     //		     Color(.28,.25,.02), Color(.16,.16,.16),0,50);
		     Color(.78,.35,.02), Color(.16,.16,.16),0.1,80);

  l = (new Light(PerlinPt+Vector(-1,1,4),Color(1.,1.,1.),0,0.7));
  l->name_ = "Perlin";
  scene->add_per_matl_light (l);
  vase_black->my_lights.add(l);
  perlin_marble->my_lights.add(l);

  Transform perlint;
  //  perlint.pre_rotate(M_PI_2,Vector(1,0,0));
  perlint.pre_scale(Vector(5,5,5));
  t=perlint;
  t.pre_translate(PerlinPt.vector());

  Group *perlin_g = new Group();
  if (!readObjFile("/usr/sci/data/Geometry/models/museum/vase.obj",
		   "/usr/sci/data/Geometry/models/museum/vase.mtl",
		   t, perlin_g, 0, perlin_marble)) {
    exit(0);
  }  

  historyg->add(new HierarchicalGrid(perlin_g,10,10,10,8,32,4));

  /* **************** morphine  **************** */
  Material* yellow = new LambertianMaterial(Color(.95,.8,.0));
  Material* Ccolor = new MetalMaterial(Color(0.35,0.63,0.63),20);
  Material* Hcolor = new MetalMaterial(Color(0.3,0.15,0.15),20);

  l = (new Light(MorphinePt+Vector(1,1,4),Color(1.,1.,1.),0,0.7));
  l->name_ = "Morphine";
  scene->add_per_matl_light (l);
  yellow->my_lights.add(l);
  Ccolor->my_lights.add(l);
  Hcolor->my_lights.add(l);

  historyg->add (new Parallelogram(yellow,
				   MorphinePt-Vector(ped_size/2.-diff,ped_size/2.-diff,-0.001),
				   Vector(0,gbox_size,0),Vector(gbox_size,0,0)));
  const float Crad = 0.06;
  const float Hrad = 0.03;
  const float s3 = sqrt(3.)*0.5;
  float mol_ht = Hrad;

  historyg->add (new Sphere(Hcolor, MorphinePt+Vector(Crad+0.5*(Crad+Hrad),
							  0,mol_ht),Hrad));
  historyg->add (new Sphere(Hcolor, MorphinePt-Vector(Crad+0.5*(Crad+Hrad),
							  0,-mol_ht),Hrad));
  mol_ht += s3*(Hrad+Crad);
  historyg->add (new Sphere(Ccolor, 
			    MorphinePt+Vector(Crad,0,mol_ht),Crad));
  historyg->add (new Sphere(Ccolor, 
			    MorphinePt-Vector(Crad,0,-mol_ht),Crad));
  mol_ht += s3*2*Crad;
  historyg->add (new Sphere(Ccolor, 
			    MorphinePt+Vector(Crad*2,0,mol_ht),Crad));
  historyg->add (new Sphere(Ccolor, 
			    MorphinePt-Vector(Crad*2,0,-mol_ht),Crad));
  historyg->add (new Sphere(Hcolor, 
			    MorphinePt+Vector((Crad+Hrad)*2,0,mol_ht),Hrad));
  historyg->add (new Sphere(Hcolor, 
			    MorphinePt-Vector((Crad+Hrad)*2,0,-mol_ht),Hrad));
  mol_ht += s3*2*Crad;
  historyg->add (new Sphere(Ccolor, 
			    MorphinePt+Vector(Crad,0,mol_ht),Crad));
  historyg->add (new Sphere(Ccolor, 
			    MorphinePt-Vector(Crad,0,-mol_ht),Crad));
  mol_ht += s3*(Hrad+Crad);
  historyg->add (new Sphere(Hcolor, MorphinePt+Vector(Crad+0.5*(Crad+Hrad),
							  0,mol_ht),Hrad));
  historyg->add (new Sphere(Hcolor, MorphinePt-Vector(Crad+0.5*(Crad+Hrad),
							  0,-mol_ht),Hrad));

  /* **************** alpha1 helicopter **************** */
  Material* turquoise = new LambertianMaterial(Color(.21,.55,.65));
//    Material* copter_matl = new MetalMaterial(Color(0.7,0.73,0.8));
  Material* copter_matl = new Phong(Color(210/255.,180/255.,140/255.),
				    Color(.2,.2,.2),40);

  l = (new Light(CopterPt+Vector(1,-1,4),Color(1.,1.,1.),0,0.7));
  l->name_ = "Copter";
  scene->add_per_matl_light (l);
  turquoise->my_lights.add(l);
  copter_matl->my_lights.add(l);

  historyg->add (new Parallelogram(turquoise,
				   CopterPt+Vector(-diff,-diff,0.001),
				   Vector(gbox_size,0,0),Vector(0,-gbox_size,0)));

  Group* copterg;
  // read in the copter geometry
  copterg = readtris("/usr/sci/data/Geometry/models/museum/copter.tris",copter_matl);

  BBox copter_bbox;

  copterg->compute_bounds(copter_bbox,0);

  Point copter_min = copter_bbox.min();
  Point copter_max = copter_bbox.max();
  Vector copter_diag = copter_bbox.diagonal();

  Point CopterPedPt = CopterPt + Vector(ped_size/2., -ped_size/2., 0);

  Transform copterT;

  copterT.pre_translate(-Vector((copter_max.x()+copter_min.x())/2.,
				(copter_min.y()),
				(copter_max.z()+copter_min.z())/2.)); // center copter over 0
  copterT.pre_rotate(M_PI_2,Vector(1,0,0));  // make z up
  copterT.pre_rotate(5*M_PI_4,Vector(0,0,1));  // orient heli
  double copter_scale = .4*2./(sqrt(copter_diag.x()*copter_diag.x()+copter_diag.z()*copter_diag.z()));
  copterT.pre_scale(Vector(copter_scale,
			  copter_scale,
			  copter_scale));
  copterT.pre_translate(CopterPedPt.asVector());

  copterg->transform(copterT);


  main_group->add(new HierarchicalGrid(copterg,16,16,64,16,1024,4));

#endif

  main_group->add(historyg);
  main_group->add(solidg);
  
  no_shadow_group->add(clearg);

}

/* **************** david room **************** */

void build_david_room (Group* main_group, Scene *scene, Light *light1, Light *light2) {
  //  Material* david_white = new LambertianMaterial(Color(.8,.75,.7)); 
  //  Material* david_white = new Phong(Color(.8,.75,.7),
  //				    Color(.2,.2,.2),40); 
  Material* flat_white = new LambertianMaterial(Color(.8,.8,.8));
  Material* light_marble1 
    = new CrowMarble(4.5, Vector(.3, .3, 0), Color(.9,.9,.9), 
		     Color(.8, .8,.8), Color(.7, .7, .7)); 

  /* **************** david pedestal **************** */

  Group* davidp = new Group();
  Point dav_ped_top(-14,-20,1);
  
  davidp->add(new UVCylinder(light_marble1,
			  Point(-14,-20,0),
			  dav_ped_top,2.7/2.));
  davidp->add(new Disc(flat_white,dav_ped_top,Vector(0,0,1),2.7/2.));
  light_marble1->my_lights.add(light1);
  light_marble1->my_lights.add(light2);

  /* **************** David **************** */

  Light *l1, *l2, *l3, *l4;
  l1 = (new Light(Point(-13,-22.25,6.5),Color (.4,.401,.4), 0));
  l1->name_ = "per David A";
  scene->add_per_matl_light (l1);
  l2 = (new Light(Point(-17,-22.25,6),Color (.4,.402,.4), 0));
  l2->name_ = "per David B";
  scene->add_per_matl_light (l2);
  l3 = (new Light(Point(-14.75,-20.75,1),Color (.4,.403,.4), 0));
  l3->name_ = "per David C";
  scene->add_per_matl_light (l3);
  l4 = (new Light(Point(-17,-17.75,7.9),Color (.4,.404,.4), 0));
  l4->name_ = "per David D";
  scene->add_per_matl_light (l4);
  /*
  l = (new Light(Point(-14,-16.15,1), Color (.4,.405,.4), 0));
  l->name_ = "per David E";
  scene->add_per_matl_light (l);
  david_white->my_lights.add (l);
  l = new Light(Point(-14,-23.85,7.9),Color (.4,.406,.4), 0);
  l->name_ = "per David F";
  scene->add_per_matl_light (l);
  david_white->my_lights.add (l);
  l = (new Light(Point(-11,-17.75,7.9),	Color (.4,.407,.4), 0));
  l->name_ = "per David G";
  scene->add_per_matl_light (l);
  david_white->my_lights.add (l);
  l = (new Light(Point(-11.25,-20.75,3.5),Color (.4,.408,.4), 0)); 
  l->name_ = "per David H";
  scene->add_per_matl_light (l);
  david_white->my_lights.add (l);
  l = (new Light(Point(-15.2,-21,5),Color (.4,.409,.4), 0)); 
  l->name_ = "per David I";
  scene->add_per_matl_light (l);
  david_white->my_lights.add (l);
  l = (new Light(Point(-13.8,-17.8,6),Color (.4,.41,.4), 0)); 
  l->name_ = "per David J";
  scene->add_per_matl_light (l);
  david_white->my_lights.add (l);
  */

  /*  1 for David, 0 for Bender */
#if INSERTDAVID
  int cells=5;
  int depth=3;

  printf ("\n\n*********************************\n\n");
  Transform davidT (Point(0,0,0),
		    Vector(0.001000,0.,0.),
		    Vector(0.,0.,0.001000),
		    Vector(0.,-0.001000,0.));
  davidT.pre_translate (Vector(-14.079300,-33.336876,3.586000));
  davidT.print();

  Color bone(0.9608, 0.8706, 0.7020);
  Material* david_white=new Phong(bone*.6, bone*.6, 100, 0);
  GridTris* davidg = new GridTris(david_white, cells, depth);

#if INSERTHUGEMODELS
  read_ply("/usr/sci/data/Geometry/Stanford_Sculptures/david_1mm.ply",davidg,&davidT);
#else
  read_ply("/usr/sci/data/Geometry/Stanford_Sculptures/david_2mm.ply",davidg,&davidT);  
#endif

  david_white->my_lights.add (l1);
  david_white->my_lights.add (l2);
  david_white->my_lights.add (l3);
  david_white->my_lights.add (l4);

  /*  
  BBox david_bbox;

  davidg->compute_bounds(david_bbox,0);

  Point min = david_bbox.min();
  Point max = david_bbox.max();
  Vector diag = david_bbox.diagonal();

  printf ("\n\n*********************************\n\n");
  printf("BBox: min: %lf %lf %lf \n\tmax: %lf %lf %lf\n\tDimensions: %lf %lf %lf\n",
	 min.x(),min.y(), min.z(),
	 max.x(),max.y(), max.z(),
	 diag.x(),diag.y(),diag.z());

  davidT.load_identity();
  davidT.pre_translate(-Vector((max.x()+min.x())/2.,min.y(),(max.z()+min.z())/2.)); // center david over 0
  davidT.pre_rotate(M_PI_2,Vector(1,0,0));  // make z up
  davidT.pre_scale(Vector(.001,.001,.001)); // make units meters
  davidT.pre_translate(dav_ped_top.asVector());
  printf ("\n\n*********************************\n\n");
  davidT.print();
  
  davidg->transform(davidT);

  david_bbox.reset();
  davidg->compute_bounds(david_bbox,0);

  min = david_bbox.min();
  max = david_bbox.max();
  diag = david_bbox.diagonal();

  printf ("\n\n*********************************\n\n");
  printf("BBox: min: %lf %lf %lf \n\tmax: %lf %lf %lf\n\tDimensions: %lf %lf %lf\n",
	 min.x(),min.y(), min.z(),
	 max.x(),max.y(), max.z(),
	 diag.x(),diag.y(),diag.z());
  */
  main_group->add(davidg);

#else
 Transform bender_trans;
  Point bender_center (-12.5,-20,0);

  // first, get it centered at the origin (in x and y), and scale it
  bender_trans.pre_translate(Vector(0,0,32));
  bender_trans.pre_scale(Vector(0.035, 0.035, 0.035));

  Transform bt(bender_trans);
  bt.pre_translate(Vector(-14,-20,1));
  Group* bender_g = new Group();
  Array1<Material*> bender_matl;
  if (!readObjFile("/usr/sci/data/Geometry/models/museum/bender-2.obj",
		   "/usr/sci/data/Geometry/models/museum/bender-2.mtl",
		   bt, bender_matl, bender_g)) {
    exit(0);
  }
  for (int k=0; k<bender_matl.size();k++) {
    bender_matl[k]->my_lights.add(l1);
    bender_matl[k]->my_lights.add(l2);
    bender_matl[k]->my_lights.add(l3);
    bender_matl[k]->my_lights.add(l4);
  }
  main_group->add(new Grid(bender_g,25));
#endif

  /* **************** couches in David room **************** */
  Transform sofa_trans;
  Point sofa_center (-19.5,-20,0);

  // first, get it centered at the origin (in x and y), and scale it
  sofa_trans.pre_translate(Vector(0,-445,0));
  sofa_trans.pre_scale(Vector(0.001, 0.001, 0.002));

  // now rotate/translate it to the right angle/position
  for (int i=0; i<2; i++) {
    Array1<Material*> matl;
    Transform t(sofa_trans);
    double rad=(M_PI/2.);
    t.pre_rotate(rad, Vector(0,0,1));
    t.pre_translate(sofa_center.vector()+Vector(10*i,0,0));
    
    Group* couchg = new Group();
    if (!readObjFile("/usr/sci/data/Geometry/models/museum/bench.obj",
		     "/usr/sci/data/Geometry/models/museum/bench.mtl",
		     t, matl, couchg)) {
      exit(0);
    }
    for (int k = 0; k<matl.size(); k++) {
      matl[k]->my_lights.add(light1);
      matl[k]->my_lights.add(light2);
    }
    main_group->add(new Grid(couchg,10));
  }

  /* **************** image on West wall in David room **************** */

  Group *david_signs = new Group();
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/david/digital_michelangelo.ppm",
		      Point (-20+IMG_EPS,-20,3.1), Vector(0,2,0), Vector(0,0,-2),
		      david_signs);


  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/david/digital_michelangelo.ppm",
		      Point (-8.15-IMG_EPS,-20,3.1), Vector (0,-2,0), Vector(0,0,-2),
		      david_signs);

  /* **************** rope barrier in David room **************** */
  Group *ropeg1, *ropeg2, *ropeg3, *ropeg4, *ropeg5, *ropeg6;

  ropeg1 = new Group();
  ropeg2 = new Group();
  ropeg3 = new Group();
  ropeg4 = new Group();
  ropeg5 = new Group();
  ropeg6 = new Group();

  Vector rope_center = dav_ped_top.vector()-Vector(0,0,1);

  /*
  Transform t;
  t.pre_translate(rope_center);
  if (!readObjFile("/usr/sci/data/Geometry/models/museum/barriers.obj",
		   "/usr/sci/data/Geometry/models/museum/barriers.mtl",
		   t, main_group)) {
    exit(0);
  }
  */

  // first, get it centered at the origin (in x and y), and scale it

  // now rotate/translate it to the right angle/position
  
  Transform t;
  Array1<Material*> matl;
  t.pre_translate(rope_center);
  if (!readObjFile("/usr/sci/data/Geometry/models/museum/barrier-01.obj",
		   "/usr/sci/data/Geometry/models/museum/barrier-2.mtl",
		   t, matl, ropeg1)) {
    exit(0);
  }
    for (int k = 0; k<matl.size(); k++) {
      matl[k]->my_lights.add(light1);
      matl[k]->my_lights.add(light2);
    }

  if (!readObjFile("/usr/sci/data/Geometry/models/museum/barrier-02.obj",
		   "/usr/sci/data/Geometry/models/museum/barrier-2.mtl",
		   t, matl, ropeg2)) {
    exit(0);
  }
    for (int k = 0; k<matl.size(); k++) {
      matl[k]->my_lights.add(light1);
      matl[k]->my_lights.add(light2);
    }
  if (!readObjFile("/usr/sci/data/Geometry/models/museum/barrier-03.obj",
		   "/usr/sci/data/Geometry/models/museum/barrier-2.mtl",
		   t, matl, ropeg3)) {
    exit(0);
  }
    for (int k = 0; k<matl.size(); k++) {
      matl[k]->my_lights.add(light1);
      matl[k]->my_lights.add(light2);
    }
  if (!readObjFile("/usr/sci/data/Geometry/models/museum/barrier-04.obj",
		   "/usr/sci/data/Geometry/models/museum/barrier-2.mtl",
		   t, matl, ropeg4)) {
    exit(0);
  }
    for (int k = 0; k<matl.size(); k++) {
      matl[k]->my_lights.add(light1);
      matl[k]->my_lights.add(light2);
    }

  if (!readObjFile("/usr/sci/data/Geometry/models/museum/barrier-05.obj",
		   "/usr/sci/data/Geometry/models/museum/barrier-2.mtl",
		   t, matl, ropeg5)) {
    exit(0);
  }
    for (int k = 0; k<matl.size(); k++) {
      matl[k]->my_lights.add(light1);
      matl[k]->my_lights.add(light2);
    }
  if (!readObjFile("/usr/sci/data/Geometry/models/museum/barrier-06.obj",
		   "/usr/sci/data/Geometry/models/museum/barrier-2.mtl",
		   t, matl, ropeg6)) {
    exit(0);
  }
    for (int k = 0; k<matl.size(); k++) {
      matl[k]->my_lights.add(light1);
      matl[k]->my_lights.add(light2);
    }
  main_group->add(new HierarchicalGrid(ropeg1,6,16,64,8,1024,4));
  main_group->add(new HierarchicalGrid(ropeg2,6,16,64,8,1024,4));
  main_group->add(new HierarchicalGrid(ropeg3,6,16,64,8,1024,4));
  main_group->add(new HierarchicalGrid(ropeg4,6,16,64,8,1024,4));
//    main_group->add(new HierarchicalGrid(ropeg4,8,16,64,8,1024,4));
  main_group->add(new HierarchicalGrid(ropeg5,6,16,64,8,1024,4));
  main_group->add(new HierarchicalGrid(ropeg6,6,16,64,8,1024,4));

  /* **************** images on North partition in David room **************** */
  Group* david_nwall=new Group();
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/david/david-b1-fill.ppm",  
		      Point(-18.5, -16.15-IMG_EPS, 3.0), 
		      Vector(-1.0,0,0), Vector(0,0,-1.0), david_nwall);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/david/david-b2-fill.ppm",  
		      Point(-18.5+1.5, -16.15-IMG_EPS, 3.0), 
		      Vector(-1.0,0,0), Vector(0,0,-1.0), david_nwall);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/david/david-b3-fill.ppm",  
		      Point(-18.5+(1.5*2), -16.15-IMG_EPS, 3.0), 
		      Vector(-1.0,0,0), Vector(0,0,-1.0), david_nwall);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/david/david-b4-fill.ppm",  
		      Point(-18.5+(1.5*3), -16.15-IMG_EPS, 3.0), 
		      Vector(-1.0,0,0), Vector(0,0,-1.0), david_nwall);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/david/david-b5-fill.ppm",  
		      Point(-18.5+(1.5*4), -16.15-IMG_EPS, 3.0), 
		      Vector(-1.0,0,0), Vector(0,0,-1.0), david_nwall);
		      
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/david/text-blur/second-para-01.ppm",
		      Point(-19.5, -16.15-IMG_EPS, 1.85),
		      Vector(1.0,0,0), Vector(0,0,-0.5624), david_nwall);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/david/text-blur/second-para-02.ppm",
		      Point(-19.5+1.5, -16.15-IMG_EPS, 1.85),
		      Vector(1.0,0,0), Vector(0,0,-0.5624), david_nwall);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/david/text-blur/second-para-03.ppm",
		      Point(-19.5+(1.5*2), -16.15-IMG_EPS, 1.85),
		      Vector(1.0,0,0), Vector(0,0,-0.5624), david_nwall);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/david/text-blur/second-para-04.ppm",
		      Point(-19.5+(1.5*3), -16.15-IMG_EPS, 1.85),
		      Vector(1.0,0,0), Vector(0,0,-0.5624), david_nwall);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/david/text-blur/second-para-05.ppm",
		      Point(-19.5+(1.5*4), -16.15-IMG_EPS, 1.85),
		      Vector(1.0,0,0), Vector(0,0,-0.5624), david_nwall);

  /* **************** images on South partition in David room **************** */
  Group* david_swall=new Group();
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/david/david-a2-fill.ppm",  
		      Point(-8.5, -23.85+IMG_EPS, 4.5), 
		      Vector(-2.0,0,0), Vector(0,0,-2.0), david_swall);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/david/david-c1-fill.ppm",  
		      Point(-8.5-2.5, -23.85+IMG_EPS, 4.5), 
		      Vector(-2.0,0,0), Vector(0,0,-2.0), david_swall);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/david/david-c2-fill.ppm",  
		      Point(-8.5-(2.5*2), -23.85+IMG_EPS, 4.5), 
		      Vector(-2.0,0,0), Vector(0,0,-2.0), david_swall);

  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/david/text-blur/museum-paragraph-03.ppm",
		      Point(-8.5, -23.85+IMG_EPS, 2.35), 
		      Vector(-2.0,0,0), Vector(0,0,-0.5625*2), david_swall);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/david/text-blur/museum-names-01.ppm",
		      Point(-8.5-2.5, -23.85+IMG_EPS, 2.35), 
		      Vector(-0.95,0,0), Vector(0,0,-1.69), david_swall);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/david/text-blur/museum-names-02.ppm",
		      Point(-8.5-3.55, -23.85+IMG_EPS, 2.35), 
		      Vector(-0.95,0,0), Vector(0,0,-1.69), david_swall);
  add_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/david/text-blur/museum-paragraph-01.ppm",
		      Point(-8.5-(2.5*2), -23.85+IMG_EPS, 2.35), 
		      Vector(-2.0,0,0), Vector(0,0,-0.5625*2), david_swall);

  main_group->add(david_signs);
  main_group->add(david_nwall);
  main_group->add(david_swall);
  main_group->add(davidp);

}

  /* **************** modern graphics room **************** */

void build_modern_room (Group *main_group, Scene *scene) {
  const float ped_ht = 1.0;
  float short_ped_ht = 0.6;
  const float half_ped_size = 0.375;
  const float sign_ratio = 0.63;
  Light *l1, *l2;
  
  //  pedestals
  Group* moderng = new Group();

  // along east wall

  /*  Alpha_1 Crank  */
  Point crank_ped_top(-10,-8,ped_ht);
  add_pedestal_and_label (moderng, "/usr/sci/data/Geometry/textures/museum/modern/pillar-text/crank-shaft.ppm",
		crank_ped_top-Vector(half_ped_size,half_ped_size,0),
		Vector(2.*half_ped_size,2.*half_ped_size,-ped_ht),sign_ratio,scene);

#ifdef INSERT_CRANK
  Material* crank_matl = new MetalMaterial(Color(0.7,0.73,0.8));
  l1 = new Light(crank_ped_top+Vector(-1,1,1),Color(1.,1.,1.),0,0.7);
  l2 = new Light(crank_ped_top+Vector(-1,-1,1),Color(1.,1.,1.),0,0.7);
  l1->name_ = "Crank 1";
  l2->name_ = "Crank 2";
  scene->add_per_matl_light (l1);
  scene->add_per_matl_light (l2);
  crank_matl->my_lights.add(l1);
  crank_matl->my_lights.add(l2);
  //  crank_matl->local_ambient_mode=Arc_Ambient;

  Group* crankg;
  // read in the part geometry
  crankg = readtris("/usr/sci/data/Geometry/models/museum/Crank.tris",crank_matl);

  BBox crank_bbox;

  crankg->compute_bounds(crank_bbox,0);

  Point crank_min = crank_bbox.min();
  Point crank_max = crank_bbox.max();
  Vector crank_diag = crank_bbox.diagonal();

  Transform crankT;

  crankT.pre_translate(-Vector((crank_max.x()+crank_min.x())/2.,crank_min.y(),(crank_max.z()+crank_min.z())/2.)); // center crank over 0
  crankT.pre_rotate(M_PI_2,Vector(1,0,0));  // make z up
  crankT.pre_rotate(-M_PI_2/2.,Vector(0,0,1));  // make z up
  double crank_scale = .6*2./(sqrt(crank_diag.x()*crank_diag.x()+crank_diag.z()*crank_diag.z()));
  crankT.pre_scale(Vector(crank_scale,
			 crank_scale,
			 crank_scale));
  crankT.pre_translate(crank_ped_top.asVector());

  crankg->transform(crankT);

  main_group->add(new HierarchicalGrid(crankg,16,16,64,16,1024,4));
#endif

  /*  David's head  */
  Point head_ped_top(-10,-11,ped_ht);
  add_pedestal_and_label (moderng, "/usr/sci/data/Geometry/textures/museum/modern/pillar-text/david.ppm",
			  head_ped_top-Vector(half_ped_size,half_ped_size,0),
			  Vector(2.*half_ped_size,2.*half_ped_size,-ped_ht),
			  sign_ratio,scene);

#ifdef INSERT_HEAD
  int dhead_cells=5;
  int dhead_depth=3;

  printf ("\n\n********* David Head ************************\n\n");
  Transform dheadT (Point(0,0,0),
		    Vector(0.000707,0.000707,0.),
		    Vector(-0.000707,0.000707,0),
		    Vector(0.,0.,0.001000));
  dheadT.pre_translate (Vector(-11.110928,-11.531475,1.015668));
  //  dheadT.print();

  Material *dhead_white = new LambertianMaterial(Color(1,1,1));
  GridTris* dheadg = new GridTris(dhead_white, dhead_cells, dhead_depth);
  
  l1 = new Light(head_ped_top+Vector(-1,1,1),Color(1.,1.,1.),0,0.7);
  l2 = new Light(head_ped_top+Vector(-1,-1,1),Color(1.,1.,1.),0,0.7);
  l1->name_ = "Head 1";
  l2->name_ = "Head 2";
  scene->add_per_matl_light (l1);
  scene->add_per_matl_light (l2);
  dheadg->my_lights.add(l1);
  dheadg->my_lights.add(l2);
  dheadg->local_ambient_mode=Arc_Ambient;

  read_ply("/usr/sci/data/Geometry/Stanford_Sculptures/david_head_1mm_color.ply",dheadg,&dheadT);

  /*
  BBox dhead_bbox;

  dheadg->compute_bounds(dhead_bbox,0);

  Point dhead_min = dhead_bbox.min();
  Point dhead_max = dhead_bbox.max();
  Transform dheadT;

  dheadT.pre_translate(-Vector((dhead_max.x()+dhead_min.x())/2.,(dhead_max.y()+dhead_min.y())/2,dhead_min.z())); // center david over 0
  dheadT.pre_rotate(M_PI/4,Vector(0,0,1));  // make z up
  dheadT.pre_scale(Vector(.001,.001,.001)); // make units meters
  dheadT.pre_translate(head_ped_top.asVector());

  printf ("\n\n*********** DAVID HEAD **********************\n\n");
  dheadT.print();
  printf ("\n\n*********************************\n\n");

  dhead_tm->transform(dheadT);
  main_group->add(new HierarchicalGrid(dheadg,23,64,64,32,1024,4));
  */
  main_group->add(dheadg);
#endif

  // south wall 
  /* dragon */
  Point dragon_ped_top(-13,-14,ped_ht);
  add_pedestal_and_label (moderng, "/usr/sci/data/Geometry/textures/museum/modern/pillar-text/dragon.ppm",
			  dragon_ped_top-Vector(half_ped_size,half_ped_size,0),
		Vector(2.*half_ped_size,2.*half_ped_size,-ped_ht), sign_ratio,scene);

#ifdef INSERT_DRAGON
  int dragon_cells=5;
  int dragon_depth=2;

  printf ("\n\n*********************************\n\n");
  Transform dragonT (Point(0,0,0),
		     Vector(0.,-3.375918,0.),
		     Vector(0.,0.,3.375918),
		     Vector(-3.375918,0.,0.));
  dragonT.pre_translate (Vector(-13.015208,-14.020845,0.821603));
  dragonT.print();

  Color dragon_green(.15,.7,.15);
  Material* shiny_green = new Phong(dragon_green,
				    Color(.2,.2,.2),
				    60);
  GridTris* dragong = new GridTris(shiny_green,dragon_cells,dragon_depth);

  l1 = new Light(dragon_ped_top+Vector(1,0,1),Color(1.,1.,1.),0,0.7);
  l2 = new Light(dragon_ped_top+Vector(-1,-1,1),Color(1.,1.,1.),0,0.7);
  l1->name_ = "Dragon 1";
  l2->name_ = "Dragon 2";
  scene->add_per_matl_light (l1);
  scene->add_per_matl_light (l2);
  shiny_green->my_lights.add(l1);
  shiny_green->my_lights.add(l2);
  shiny_green->local_ambient_mode=Arc_Ambient;

#if INSERTHUGEMODELS
  read_ply("/usr/sci/data/Geometry/Stanford_Sculptures/dragon_vrip.ply",dragong,&dragonT);
#else
  read_ply("/usr/sci/data/Geometry/Stanford_Sculptures/dragon_vrip_res2.ply",dragong,&dragonT);
#endif
  
  /*
  BBox dragon_bbox;

  dragong->compute_bounds(dragon_bbox,0);

  Point dmin = dragon_bbox.min();
  Point dmax = dragon_bbox.max();
  Vector ddiag = dragon_bbox.diagonal();
  Transform dragonT;

  dragonT.pre_translate(-Vector((dmax.x()+dmin.x())/2.,dmin.y(),(dmax.z()+dmin.z())/2.)); // center dragon over 0
  dragonT.pre_rotate(M_PI_2,Vector(1,0,0));  // make z up
  dragonT.pre_rotate(-M_PI_2,Vector(0,0,1));  // make it face front
  double dragon_scale = .375*2./(sqrt(ddiag.x()*ddiag.x()+ddiag.z()*ddiag.z()));
  dragonT.pre_scale(Vector(dragon_scale,
			   dragon_scale,
			   dragon_scale));
  dragonT.pre_translate(dragon_ped_top.asVector());

  printf ("\n\n*********** DRAGON **********************\n\n");
  dragonT.print();
  printf ("\n\n*********************************\n\n");

  dragon_tm->transform(dragonT);

  dragon_bbox.reset();
  dragong->compute_bounds(dragon_bbox,0);

  dmin = dragon_bbox.min();
  dmax = dragon_bbox.max();
  ddiag = dragon_bbox.diagonal();

  printf("BBox: min: %lf %lf %lf max: %lf %lf %lf\nDimensions: %lf %lf %lf\n",
	 dmin.x(),dmin.y(), dmin.z(),
	 dmax.x(),dmax.y(), dmax.z(),
	 ddiag.x(),ddiag.y(),ddiag.z());
  main_group->add(new Grid(dragong,64));
  */
  main_group->add(dragong);

#endif

  /* SCI torso */
  Vector torso_ped_top (-15,-14,ped_ht);
  add_pedestal_and_label (moderng, "/usr/sci/data/Geometry/textures/museum/modern/pillar-text/utah.ppm",
			  torso_ped_top.point()-Vector(half_ped_size,half_ped_size,0),
		Vector(2.*half_ped_size,2.*half_ped_size,-ped_ht), sign_ratio,scene);

  Transform torso_trans;

  // first, get it centered at the origin (in x and y), and scale it
  torso_trans.pre_translate(Vector(0,0,-280));
  torso_trans.pre_scale(Vector(0.001, 0.001, 0.001));

  // now rotate/translate it to the right angle/position
  Transform t = torso_trans;

#ifdef INSERT_TORSO
  double rot=(M_PI);
  t.pre_rotate(rot, Vector(0,0,1));
  t.pre_translate(torso_ped_top+Vector(0,0,0.3));

  l1 = new Light(torso_ped_top.point()+Vector(1,2,1.5),Color(1.,1.,1.),0,0.7);
  l2 = new Light(torso_ped_top.point()+Vector(-1,1,1),Color(1.,1.,1.),0,0.7);
  l1->name_ = "Torso 1";
  l2->name_ = "Torso 2";
  scene->add_per_matl_light (l1);
  scene->add_per_matl_light (l2);

  Group *torsog = new Group();
  Array1<Material*> torso_matl;
  if (!readObjFile("/usr/sci/data/Geometry/models/museum/utahtorso/utahtorso-isosurface.obj",
		   "/usr/sci/data/Geometry/models/museum/utahtorso/utahtorso-isosurface.mtl",
		   t, torso_matl, torsog)) {
      exit(0);
  }
  for (int k = 0; k<torso_matl.size(); k++) {
    torso_matl[k]->my_lights.add(l1);
    torso_matl[k]->my_lights.add(l2);
    torso_matl[k]->local_ambient_mode=Arc_Ambient;
  }
  if (!readObjFile("/usr/sci/data/Geometry/models/museum/utahtorso/utahtorso-heart.obj",
		   "/usr/sci/data/Geometry/models/museum/utahtorso/utahtorso-heart.mtl",
		   t, torso_matl, torsog)) {
      exit(0);
  }
  for (int k = 0; k<torso_matl.size(); k++) {
    torso_matl[k]->my_lights.add(l1);
    torso_matl[k]->my_lights.add(l2);
    torso_matl[k]->local_ambient_mode=Arc_Ambient;
  }
  if (!readObjFile("/usr/sci/data/Geometry/models/museum/utahtorso/utahtorso-lung.obj",
		   "/usr/sci/data/Geometry/models/museum/utahtorso/utahtorso-lung.mtl",
		   t, torso_matl, torsog)) {
      exit(0);
  }
  for (int k = 0; k<torso_matl.size(); k++) {
    torso_matl[k]->my_lights.add(l1);
    torso_matl[k]->my_lights.add(l2);
    torso_matl[k]->local_ambient_mode=Arc_Ambient;
  }
  if (!readObjFile("/usr/sci/data/Geometry/models/museum/utahtorso/utahtorso-skin.obj",
		   "/usr/sci/data/Geometry/models/museum/utahtorso/utahtorso-skin.mtl",
		   t, torso_matl, torsog)) {
      exit(0);
  }
  for (int k = 0; k<torso_matl.size(); k++) {
    torso_matl[k]->my_lights.add(l1);
    torso_matl[k]->my_lights.add(l2);
    torso_matl[k]->local_ambient_mode=Arc_Ambient;
  }

  main_group->add(new Grid(torsog,10));
#endif

  // along west wall 
  /* buddha */
  Point buddha_ped_top(-18,-13,0.3*ped_ht);
  add_pedestal_and_label (moderng, "/usr/sci/data/Geometry/textures/museum/modern/pillar-text/buddha.ppm",
			  buddha_ped_top-Vector(half_ped_size,half_ped_size,0),
			  Vector(2.*half_ped_size,2.*half_ped_size,-0.3*ped_ht), sign_ratio,scene);

#ifdef INSERT_BUDDHA
  int buddha_cells=5;
  int buddha_depth=2;

  printf ("\n\n*********************************\n\n");
  Transform buddhaT (Point(0,0,0),
		     Vector(0.,6.518254,0.),
		     Vector(0.,0.,6.518254),
		     Vector(6.518254,0.,0.));
  buddhaT.pre_translate (Vector(-17.956413,-12.964477,-0.024539));
  buddhaT.print();

  // read in the buddha geometry
  Color buddha_diff(113./255.,  53./255.,  17./255.);
  Color buddha_spec(180./255.,  180./255.,  180./255.);
  Material* buddha_mat = new Phong(buddha_diff,
				    buddha_spec,
				    40);
  GridTris* buddhag = new GridTris(buddha_mat,buddha_cells,buddha_depth);

  l1 = new Light(buddha_ped_top+Vector(1,-1,4),Color(1.,1.,1.),0,0.7);
  l2 = new Light(buddha_ped_top+Vector(1,1,2),Color(1.,1.,1.),0,0.7);
  l1->name_ = "Buddha 1";
  l2->name_ = "Buddha 2";
  scene->add_per_matl_light (l1);
  scene->add_per_matl_light (l2);
  buddha_mat->my_lights.add(l1);
  buddha_mat->my_lights.add(l2);
  buddha_mat->local_ambient_mode=Arc_Ambient;
  
#if INSERTHUGEMODELS
  read_ply("/usr/sci/data/Geometry/Stanford_Sculptures/happy_vrip.ply",buddhag,&buddhaT);
#else
  read_ply("/usr/sci/data/Geometry/Stanford_Sculptures/happy_vrip_res2.ply",buddhag,&buddhaT);
#endif

  /*
  BBox buddha_bbox;

  buddhag->compute_bounds(buddha_bbox,0);

  Point bmin = buddha_bbox.min();
  Point bmax = buddha_bbox.max();
  Vector bdiag = buddha_bbox.diagonal();

  printf("BBox: min: %lf %lf %lf max: %lf %lf %lf\nDimensions: %lf %lf %lf\n",
	 bmin.x(),bmin.y(), bmin.z(),
	 bmax.x(),bmax.y(), bmax.z(),
	 bdiag.x(),bdiag.y(),bdiag.z());

  Transform buddhaT;
  buddhaT.pre_translate(-Vector((bmax.x()+bmin.x())/2.,bmin.y(),(bmax.z()+bmin.z())/2.)); // center buddha over 0
  buddhaT.pre_rotate(M_PI_2,Vector(1,0,0));  // make z up
  buddhaT.pre_rotate(M_PI_2,Vector(0,0,1));  // make z up
  double buddha_scale = .375*2./(sqrt(bdiag.x()*bdiag.x()+bdiag.z()*bdiag.z()));
  buddhaT.pre_scale(Vector(buddha_scale,
			   buddha_scale,
			   buddha_scale));
  buddhaT.pre_translate(buddha_ped_top.asVector());

  printf ("\n\n*********** BUDDHA **********************\n\n");
  buddhaT.print();
  printf ("\n\n*********************************\n\n");

  buddha_tm->transform(buddhaT);

  buddha_bbox.reset();
  buddhag->compute_bounds(buddha_bbox,0);

  bmin = buddha_bbox.min();
  bmax = buddha_bbox.max();
  bdiag = buddha_bbox.diagonal();
  printf("BBox: min: %lf %lf %lf max: %lf %lf %lf\nDimensions: %lf %lf %lf\n",
	 bmin.x(),bmin.y(), bmin.z(),
	 bmax.x(),bmax.y(), bmax.z(),
	 bdiag.x(),bdiag.y(),bdiag.z());
  main_group->add(new HierarchicalGrid(buddhag,16,16,64,16,1024,4));
  */
  main_group->add(buddhag);
#endif

  /*  UNC well */
  Point unc_ped_top (-18,-10,short_ped_ht);
  add_pedestal_and_label (moderng,"/usr/sci/data/Geometry/textures/museum/modern/pillar-text/unc.ppm",
			  unc_ped_top-Vector(half_ped_size,half_ped_size,0),
		Vector(2.*half_ped_size,2.*half_ped_size,-short_ped_ht), sign_ratio,scene);

#ifdef INSERT_WELL
  Group* well_g = new Group();
  Array1<Material*> well_matl;
  l1 = new Light(unc_ped_top+Vector(1,-1,0.7),Color(1.,1.,1.),0,0.7);
  l2 = new Light(unc_ped_top+Vector(-1,-1,1),Color(1.,1.,1.),0,0.7);
  l1->name_ = "Well 1";
  l2->name_ = "Well 2";
  scene->add_per_matl_light (l1);
  scene->add_per_matl_light (l2);

  t.load_identity();
  t.pre_translate(unc_ped_top.vector()); 
  if (!readObjFile("/usr/sci/data/Geometry/models/museum/old-well.obj",
		   "/usr/sci/data/Geometry/models/museum/old-well.mtl",
		   t, well_matl, well_g)) {
      exit(0);
  }
  for (int k = 0; k<well_matl.size(); k++) {
    well_matl[k]->my_lights.add(l1);
    well_matl[k]->my_lights.add(l2);
    well_matl[k]->local_ambient_mode=Arc_Ambient;
  }
  main_group->add( new HierarchicalGrid(well_g,10,10,10,8,8,4));
#endif

  /*  Venus  */
  Point venus_ped_top(-18,-7,0.25*ped_ht);
  add_pedestal_and_label (moderng,"/usr/sci/data/Geometry/textures/museum/modern/pillar-text/venus.ppm",
			  venus_ped_top-Vector(half_ped_size,half_ped_size,0),
			  Vector(2.*half_ped_size,2.*half_ped_size,-0.25*ped_ht),sign_ratio,scene);

#ifdef INSERT_VENUS
  int venus_cells=5;
  int venus_depth=2;

  printf ("\n\n*********************************\n\n");
  Transform venusT (Point(0,0,0),
		    Vector(5.537701,0.,0.),
		    Vector(0.,0.,5.537701),
		    Vector(0.,-5.537701,0.));
  venusT.pre_translate (Vector(-17.990726,-6.970150,0.335281));
  venusT.print();

  Material* flat_white = new LambertianMaterial(Color(.8,.8,.8));
  GridTris* venusg = new GridTris(flat_white,venus_cells,venus_depth);

  l1 = new Light(venus_ped_top+Vector(1,-0.5,1.9),Color(1.,1.,1.),0,0.6);
  l2 = new Light(venus_ped_top+Vector(0.5,-1,3),Color(1.,1.,1.),0,0.7);
  l1->name_ = "Venus 1";
  l2->name_ = "Venus 2";
  scene->add_per_matl_light (l1);
  scene->add_per_matl_light (l2);
  flat_white->my_lights.add(l1);
  flat_white->my_lights.add(l2);
  //  flat_white->local_ambient_mode=Arc_Ambient;

  read_ply("/usr/sci/data/Geometry/Stanford_Sculptures/venus.ply",venusg,&venusT);

  /*
  BBox venus_bbox;

  venusg->compute_bounds(venus_bbox,0);

  Point vmin = venus_bbox.min();
  Point vmax = venus_bbox.max();
  Vector vdiag = venus_bbox.diagonal();

  printf("BBox: min: %lf %lf %lf max: %lf %lf %lf\nDimensions: %lf %lf %lf\n",
	 vmin.x(),vmin.y(), vmin.z(),
	 vmax.x(),vmax.y(), vmax.z(),
	 vdiag.x(),vdiag.y(),vdiag.z());

  Transform venusT;

  venusT.pre_translate(-Vector((vmax.x()+vmin.x())/2.,vmin.y(),(vmax.z()+vmin.z())/2.)); 
  venusT.pre_rotate(M_PI_2,Vector(1,0,0));  // make z up
  double ven_scale = .375*2./(sqrt(vdiag.x()*vdiag.x()+vdiag.z()*vdiag.z()));
  venusT.pre_scale(Vector(ven_scale,ven_scale,ven_scale)); // make units meters
  venusT.pre_translate(venus_ped_top.asVector());

  printf ("\n\n*********** VENUS **********************\n\n");
  venusT.print();
  printf ("\n\n*********************************\n\n");

  venus_tm->transform(venusT);

  venus_bbox.reset();
  venusg->compute_bounds(venus_bbox,0)
;
  vmin = venus_bbox.min();
  vmax = venus_bbox.max();
  vdiag = venus_bbox.diagonal();

  printf("BBox: min: %lf %lf %lf max: %lf %lf %lf\nDimensions: %lf %lf %lf\n",
	 vmin.x(),vmin.y(), vmin.z(),
	 vmax.x(),vmax.y(), vmax.z(),
	 vdiag.x(),vdiag.y(),vdiag.z());
  main_group->add(new HierarchicalGrid(venusg,16,32,64,8,1024,4));
  */
  main_group->add(venusg);
#endif

  // along north wall
  /* Stanford bunny */
  Point bun_ped_top (-15,-6,ped_ht);
  add_pedestal_and_label (moderng,"/usr/sci/data/Geometry/textures/museum/modern/pillar-text/bunny.ppm",
			  bun_ped_top-Vector(half_ped_size,half_ped_size,0),
			  Vector(2.*half_ped_size,2.*half_ped_size,-ped_ht),sign_ratio,scene);

#ifdef INSERT_BUNNY
  FILE *fp;

  Transform bunnyT;

  fp = fopen("/usr/sci/data/Geometry/models/bun.ply","r");
  if (!fp) {
    fprintf(stderr,"No such file!\n");
    exit(-1);
  }
  int num_verts, num_tris;
  
  fscanf(fp,"%d %d",&num_verts,&num_tris);
  
  double (*vert)[3] = new double[num_verts][3];
  double conf,intensity;
  int i;

  Material *bunnymat = new Phong(Color(.63,.51,.5),Color(.3,.3,.3),400);
  l1 = new Light(bun_ped_top+Vector(1,-1,1),Color(1.,1.,1.),0,0.7);
  l2 = new Light(bun_ped_top+Vector(-1,-1,1),Color(1.,1.,1.),0,0.7);
  l1->name_ = "Bunny 1";
  l2->name_ = "Bunny 2";
  scene->add_per_matl_light (l1);
  scene->add_per_matl_light (l2);
  bunnymat->my_lights.add(l1);
  bunnymat->my_lights.add(l2);
  bunnymat->local_ambient_mode=Arc_Ambient;
  
  BBox bunny_bbox;

  for (i=0; i<num_verts; i++) {
    fscanf(fp,"%lf %lf %lf %lf %lf",&vert[i][0],&vert[i][2],&vert[i][1],
	   &conf,&intensity);
    bunny_bbox.extend(Point(vert[i][0],vert[i][1],vert[i][2]));
  }
  
  Point bunny_min = bunny_bbox.min();
  Point bunny_max = bunny_bbox.max();
  Vector bunny_diagonal = bunny_bbox.diagonal();

  bunnyT.pre_translate(Vector(-.5*(bunny_max.x()+bunny_min.x()),
			      -.5*(bunny_max.y()+bunny_min.y()),
			      -bunny_min.z()));
  bunnyT.pre_rotate(M_PI,Vector(0,0,1));
  double bunny_rad = 2*.375 / (sqrt(bunny_diagonal.x()*bunny_diagonal.x() + 
				  bunny_diagonal.y()*bunny_diagonal.y()));
  bunnyT.pre_scale(Vector(bunny_rad,bunny_rad,bunny_rad));
  bunnyT.pre_translate(bun_ped_top.asVector());

  int num_pts, pi0, pi1, pi2;
  
  Group* bunny=new Group();
  for (i=0; i<num_tris; i++) {
    fscanf(fp,"%d %d %d %d\n",&num_pts,&pi0,&pi1,&pi2);
    bunny->add(new Tri(bunnymat,
		       bunnyT.project(Point(vert[pi0][0],vert[pi0][1],vert[pi0][2])),
		       bunnyT.project(Point(vert[pi1][0],vert[pi1][1],vert[pi1][2])),
		       bunnyT.project(Point(vert[pi2][0],vert[pi2][1],vert[pi2][2]))));
  }
  delete vert;
  fclose(fp);

  main_group->add (new HierarchicalGrid(bunny,8,16,16,16,1024,4));
#endif

  /*  Gooch NPR models */
  Point npr_ped_top(-12,-6,ped_ht);
  add_pedestal_and_label (moderng, "/usr/sci/data/Geometry/textures/museum/modern/pillar-text/npr.ppm",
			  npr_ped_top-Vector(half_ped_size,half_ped_size,0),
			  Vector(2.*half_ped_size,2.*half_ped_size,-ped_ht), 
			  sign_ratio,scene);

#ifdef INSERT_SPIRAL
  Material* silver = new MetalMaterial(Color(0.7,0.73,0.8));
  l1 = new Light(npr_ped_top+Vector(1,-1,3),Color(1.,1.,1.),0,0.7);
  l2 = new Light(npr_ped_top+Vector(-1,-1,3),Color(1.,1.,1.),0,0.7);
  l1->name_ = "NPR 1";
  l2->name_ = "NPR 2";
  scene->add_per_matl_light (l1);
  scene->add_per_matl_light (l2);
  silver->my_lights.add(l1);
  silver->my_lights.add(l2);
  silver->local_ambient_mode=Arc_Ambient;

  Group* partg;
  // read in the part geometry
  partg = readtris("/usr/sci/data/Geometry/models/museum/part.tris",silver);

  BBox part_bbox;

  partg->compute_bounds(part_bbox,0);

  Point part_min = part_bbox.min();
  Point part_max = part_bbox.max();
  Vector part_diag = part_bbox.diagonal();

  Transform partT;

  partT.pre_translate(-Vector((part_max.x()+part_min.x())/2.,part_min.y(),(part_max.z()+part_min.z())/2.)); // center part over 0
  partT.pre_rotate(M_PI_2,Vector(1,0,0));  // make z up
  double part_scale = .25*2./(sqrt(part_diag.x()*part_diag.x()+part_diag.z()*part_diag.z()));
  partT.pre_scale(Vector(part_scale,
			 part_scale,
			 part_scale));
  partT.pre_translate(npr_ped_top.asVector());

  partg->transform(partT);

  main_group->add(new HierarchicalGrid(partg,16,16,64,16,1024,4));
#endif

//    // center of lucy
//    double lucy_ht = .3;
//    Point lucy_centerpt(-14,-10,lucy_ht);
//    double lucy_radius = 1;
//    add_pedestal (moderng, lucy_centerpt-Vector(lucy_radius,lucy_radius,0),
//  		Vector(2*lucy_radius,2*lucy_radius,-lucy_ht),scene);
//    // Add lucy here.
//    Material *lucy_white = new LambertianMaterial(Color(1,1,1));
//    Group* lucyg = new Group();
//    TriMesh* lucy_tm = new TriMesh();
//    read_ply("/usr/sci/data/Geometry/Stanford_Sculptures/lucy.ply",lucy_white, lucy_tm, lucyg);

//    BBox lucy_bbox;

//    lucyg->compute_bounds(lucy_bbox,0);

//    Point lucy_min = lucy_bbox.min();
//    Point lucy_max = lucy_bbox.max();
//    Vector lucy_diag = lucy_bbox.diagonal();
//    Transform lucyT;

//    lucyT.pre_translate(-Vector((lucy_max.x()+lucy_min.x())/2.,(lucy_max.y()+lucy_min.y())/2.,lucy_min.z())); // center david over 0
//  //    lucyT.pre_rotate(M_PI_2,Vector(1,0,0));  // make z up
//    lucyT.pre_scale(2*Vector(.001,.001,.001)); // make units meters
//    lucyT.pre_translate(lucy_centerpt.asVector());

//    lucy_tm->transform(lucyT);

//  //    main_group->add(new HierarchicalGrid(lucyg,32,64,64,16,1024,4));
//    main_group->add(new HierarchicalGrid(lucyg,24,32,64,16,1024,4));


  // center of stadium
  double stadium_ht = 1.2;
  Point stadium_centerpt(-14,-10,stadium_ht);
  double stadium_radius = 1;
  add_pedestal_and_label (moderng, "/usr/sci/data/Geometry/textures/museum/modern/pillar-text/stadium.ppm",
			  stadium_centerpt-Vector(stadium_radius,stadium_radius,0),
			  Vector(2*stadium_radius,2*stadium_radius,-stadium_ht),sign_ratio,scene);

#ifdef INSERT_STADIUM
  Array1<Material*> ase_matls;
  string env_map;

  Transform stadiumt;
  stadiumt.load_identity();
  Group *stadiumg = new Group();
  if (!readASEFile("/usr/sci/data/Geometry/models/stadium/fordfield3.ase", stadiumt, stadiumg, 
		   ase_matls, env_map)) return;
  l1 = new Light(stadium_centerpt+Vector(0,0,2),Color(1.,1.,1.),0,0.7);
  l2 = new Light(stadium_centerpt+Vector(stadium_radius+.2,-stadium_radius-.2,.2),
			Color(1.,1.,1.),0,0.7);
  l1->name_ = "Stadium 1";
  l2->name_ = "Stadium 2";
  scene->add_per_matl_light (l1);
  scene->add_per_matl_light (l2);
  for (int i=0;i<ase_matls.size();i++) {
    ase_matls[i]->my_lights.add(l1);
    ase_matls[i]->my_lights.add(l2);
  }			
  BBox stadium_bbox;

  stadiumg->compute_bounds(stadium_bbox,0);
  
  Point stadium_min = stadium_bbox.min();
  Point stadium_max = stadium_bbox.max();
  Vector stadium_diag = stadium_bbox.diagonal();

  Transform stadiumT;

  stadiumT.pre_translate(-Vector((stadium_max.x()+stadium_min.x())/2.,(stadium_min.y()+stadium_max.y()),stadium_min.z())); // center buddha over 0
  double stadium_scale = stadium_radius*2./(sqrt(stadium_diag.x()*stadium_diag.x()+stadium_diag.y()*stadium_diag.y()));
  stadiumT.pre_scale(Vector(stadium_scale,
			    stadium_scale,
			    stadium_scale));
  stadiumT.pre_translate(stadium_centerpt.asVector());

  stadiumg->transform(stadiumT);

  main_group->add(new HierarchicalGrid(stadiumg,10,10,10,100,20,4));
#endif

  /* **************** image on North wall in modern room **************** */
  const float img_size = 1.1;     
  float img_div = 0.25;
  const float img_ht = 2.7;

  Vector NorthRight (img_size,0,0);
  Vector NorthDown (0,0,-img_size);
  Point NorthPoint (-11.1-img_div-img_size, -4-IMG_EPS, img_ht);

  add_cheap_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/modern/Figure12C-fill.ppm",
		      NorthPoint, NorthRight, NorthDown, moderng, scene);

  NorthPoint += Vector(-2*img_div-img_size, 0,0);  
  add_cheap_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/modern/aging-venusC-fill.ppm",
		      NorthPoint, NorthRight, NorthDown, moderng, scene);

  NorthPoint += Vector(-2*img_div-img_size, 0,0);  
  add_cheap_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/modern/bookscroppedC-fill.ppm",
		      NorthPoint, NorthRight, NorthDown, moderng, scene);

  NorthPoint += Vector(-2*img_div-img_size, 0,0);  
  add_cheap_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/modern/buddhasC-fill.ppm",
		      NorthPoint, NorthRight, NorthDown, moderng, scene);

  NorthPoint += Vector(-2*img_div-img_size, 0,0);  
  add_cheap_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/modern/bugsC-fill.ppm",
		      NorthPoint, NorthRight, NorthDown, moderng, scene);

  //  cerr << "North Wall: " << NorthPoint << endl;

  Vector WestRight (0,img_size,0);
  Vector WestDown (0,0,-img_size);
  Point WestPoint (-20+IMG_EPS, -4-img_div-img_size, img_ht);


  add_cheap_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/modern/bunnyC-fill.ppm",
		      WestPoint, WestRight, WestDown, moderng, scene);

  WestPoint -= Vector (0, 2*img_div+img_size, 0);
  add_cheap_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/modern/chickenposter2C-fill.ppm",
		      WestPoint, WestRight, WestDown, moderng, scene);

  WestPoint -= Vector (0, 2*img_div+img_size, 0);
  add_cheap_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/modern/collage_summaryC-fill.ppm",
		      WestPoint, WestRight, WestDown, moderng, scene);

  WestPoint -= Vector (0, 2*img_div+img_size, 0);
  add_cheap_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/modern/discontinuityC-fill.ppm",
		      WestPoint, WestRight, WestDown, moderng, scene);

  WestPoint -= Vector (0, 2*img_div+img_size, 0);
  add_cheap_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/modern/dressC-fill.ppm",
		      WestPoint, WestRight, WestDown, moderng, scene);

  WestPoint -= Vector (0, 2*img_div+img_size, 0);
  add_cheap_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/modern/flower_combC-fill.ppm",
		      WestPoint, WestRight, WestDown, moderng, scene);

  WestPoint -= Vector (0, 2*img_div+img_size, 0);
  add_cheap_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/modern/geriC-fill.ppm",
		      WestPoint, WestRight, WestDown, moderng, scene);

  //  cerr << "West Wall:  " << WestPoint << endl;

  img_div = 0.35;
  Vector SouthRight (-img_size, 0, 0);
  Vector SouthDown (0,0,-img_size);
  Point SouthPoint (-20+img_div+img_size, -15.85+IMG_EPS, img_ht);

  add_cheap_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/modern/ir-imageC-fill.ppm",
		      SouthPoint, SouthRight, SouthDown, moderng, scene);

  SouthPoint += Vector(2*img_div+img_size,0,0);  
  add_cheap_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/modern/large-lakeC-fill.ppm",
		      SouthPoint, SouthRight, SouthDown, moderng, scene);

  SouthPoint += Vector(2*img_div+img_size,0,0);  
  add_cheap_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/modern/louvreC-fill.ppm",

		      SouthPoint, SouthRight, SouthDown, moderng, scene);

  SouthPoint += Vector(2*img_div+img_size,0,0);  
  add_cheap_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/modern/lumigraph2C-fill.ppm",
		      SouthPoint, SouthRight, SouthDown, moderng, scene);

  //  cerr << "South Wall: " << SouthPoint-Vector(img_size, 0,0) << endl;

  Vector EastRight (0,-img_size,0);
  Vector EastDown (0,0,-img_size);
  Point EastPoint (-8.15-IMG_EPS, -4-img_div, img_ht);

  add_cheap_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/modern/the_endC-fill.ppm",
		      EastPoint, EastRight, EastDown,moderng, scene);

  EastPoint -= Vector(0, 2*img_div+img_size, 0);   
  add_cheap_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/modern/subd-venusC-fill.ppm",
		      EastPoint, EastRight, EastDown,moderng, scene);

  EastPoint -= Vector(0, 2*img_div+img_size, 0); 
  add_cheap_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/modern/storyC-fill.ppm",
		      EastPoint, EastRight, EastDown,moderng, scene);

  EastPoint -= Vector(0, 2*img_div+img_size, 0); 
  add_cheap_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/modern/poolC-fill.ppm",
		      EastPoint, EastRight, EastDown,moderng, scene);

  EastPoint -= Vector(0, 2*img_div+img_size, 0); 
  add_cheap_poster_on_wall ("/usr/sci/data/Geometry/textures/museum/modern/mayaC-fill.ppm",
		      EastPoint, EastRight, EastDown,moderng, scene);

  //  cerr << "East Wall:  " << EastPoint-Vector(0,img_size,0) << endl;


  main_group->add(moderng);

  //  scene->add_light(new Light(Point(-6, -16, 5), Color(.401,.4,.4), 0));
  //  scene->add_light(new Light(Point(-12, -26, 5), Color(.402,.4,.4), 0));

}

extern "C"
Scene* make_scene(int argc, char* argv[], int /*nworkers*/)
{
  for(int i=1;i<argc;i++) {
    cerr << "Unknown option: " << argv[i] << '\n';
    cerr << "Valid options for scene: " << argv[0] << '\n';
    return 0;
  }

  Point Eye(-5.85, -6.2, 2.0);
  Point Lookat(-13.5, -13.5, 2.0);
  Vector Up(0,0,1);
  double fov=60;
  Group *g = new Group();
  Group *shadow = new Group();
 
  Camera cam(Eye,Lookat,Up,fov);

  /*
  Material* flat_white = new LambertianMaterial(Color(.8,.8,.8));
  Material* shinyred = new MetalMaterial( Color(0.8, 0.0, 0.08) );
  Material* marble1=new CrowMarble(5.0,
				   Vector(2,1,0),
				   Color(0.5,0.6,0.6),
				   Color(0.4,0.55,0.52),
				   Color(0.35,0.45,0.42));
  Material* marble2=new CrowMarble(7.5,
				   Vector(-1,3,0),
				   Color(0.4,0.3,0.2),
				   Color(0.35,0.34,0.32),
				   Color(0.20,0.24,0.24));
  Material* marble3=new CrowMarble(5.0, 
				   Vector(2,1,0),
				   Color(0.2,0.2,0.2),
				   Color(0,0,0),
				   Color(0.35,0.4,0.4)
				   );
  Material* marble4=new CrowMarble(7.5, 
				   Vector(-1,3,0),
				   Color(0,0,0),
				   Color(0.35,0.34,0.32),
				   Color(0.20,0.24,0.24)
				   );
				   
  Material* floor_mat = new LambertianMaterial(Color(.7,.7,.5));
  Material* dark_marble1 
    = new CrowMarble(4.5, Vector(-.3, -.3, 0), Color(.05,.05, .05),
		     Color(.075, .075, .075), Color(.1, .1, .1),0,80);

  Material* marble=new Checker(dark_marble1,
			       dark_marble1,
			       Vector(3,0,0), Vector(0,3,0));
  */

  Material* floor_mat = new ImageMaterial("/usr/sci/data/Geometry/textures/museum/general/floor1024.ppm",
					  ImageMaterial::Tile,
					  ImageMaterial::Tile, 1,
					  Color(0,0,0), 0);
  floor_mat->SetScale (10,10);

  Object* check_floor=new Rect(floor_mat, Point(-12, -16, 0),
			       Vector(8, 0, 0), Vector(0, 12, 0));

  Group* south_wall=new Group();
  Group* west_wall=new Group();
  Group* north_wall=new Group();
  Group* east_wall=new Group();
  Group* ceiling_floor=new Group();
  Group* partitions=new Group();
  Transform t;

  ceiling_floor->add(check_floor);

  Material* wall_white = new ImageMaterial("/usr/sci/data/Geometry/textures/museum/general/tex-wall.ppm",
					   ImageMaterial::Tile,
					   ImageMaterial::Tile, 1,
					   Color(0,0,0), 0);
  Material* his1_wall_white = new ImageMaterial("/usr/sci/data/Geometry/textures/museum/general/tex-wall.ppm",
					   ImageMaterial::Tile,
					   ImageMaterial::Tile, 1,
					   Color(0,0,0), 0);
  Material* his2_wall_white = new ImageMaterial("/usr/sci/data/Geometry/textures/museum/general/tex-wall.ppm",
					   ImageMaterial::Tile,
					   ImageMaterial::Tile, 1,
					   Color(0,0,0), 0);
  Material* dav_wall_white = new ImageMaterial("/usr/sci/data/Geometry/textures/museum/general/tex-wall.ppm",
					   ImageMaterial::Tile,
					   ImageMaterial::Tile, 1,
					   Color(0,0,0), 0);
  Material* mod_wall_white = new ImageMaterial("/usr/sci/data/Geometry/textures/museum/general/tex-wall.ppm",
					   ImageMaterial::Tile,
					   ImageMaterial::Tile, 1,
					   Color(0,0,0), 0);
  Material* west_wall_white = new ImageMaterial("/usr/sci/data/Geometry/textures/museum/general/tex-wall.ppm",
					   ImageMaterial::Tile,
					   ImageMaterial::Tile, 1,
					   Color(0,0,0), 0);
  Material* north_wall_white = new ImageMaterial("/usr/sci/data/Geometry/textures/museum/general/tex-wall.ppm",
					   ImageMaterial::Tile,
					   ImageMaterial::Tile, 1,
					   Color(0,0,0), 0);
  Material* his_wall_white = new ImageMaterial("/usr/sci/data/Geometry/textures/museum/general/tex-wall.ppm",
					   ImageMaterial::Tile,
					   ImageMaterial::Tile, 1,
					   Color(0,0,0), 0);
  Material* his2_dav_wall_white = new ImageMaterial("/usr/sci/data/Geometry/textures/museum/general/tex-wall.ppm",
					   ImageMaterial::Tile,
					   ImageMaterial::Tile, 1,
					   Color(0,0,0), 0);
  Material* all_wall_white = new ImageMaterial("/usr/sci/data/Geometry/textures/museum/general/tex-wall.ppm",
					   ImageMaterial::Tile,
					   ImageMaterial::Tile, 1,
					   Color(0,0,0), 0);
  Material* some_wall_white = new ImageMaterial("/usr/sci/data/Geometry/textures/museum/general/tex-wall.ppm",
					   ImageMaterial::Tile,
					   ImageMaterial::Tile, 1,
					   Color(0,0,0), 0);
  const float wall_width = .16;

  south_wall->add(new Rect(his2_wall_white, Point(-12, -28, 4), 
		       Vector(8, 0, 0), Vector(0, 0, 4)));

  south_wall->add(new Rect(wall_white, Point(-12, -28-wall_width, 4), 
		       Vector(8+wall_width, 0, 0), Vector(0, 0, 4)));

  /*
  Material* shiny_red = new MetalMaterial( Color(0.8, 0.0, 0.08) );
  t.load_identity();
  t.pre_scale(Vector(0.025,0.025,0.025));
  t.pre_translate(Vector(-0.15,-0.15,0));
  if (!readObjFile("/usr/sci/data/Geometry/models/museum/museum-obj/room2_moulding.obj",
		   "/usr/sci/data/Geometry/models/museum/museum-obj/room2_moulding.mtl",
		   t, west_wall, 0, shiny_red)) {
      exit(0);
  }

  if (!readObjFile("/usr/sci/data/Geometry/models/museum/museum-obj/room1_moulding.obj",
		   "/usr/sci/data/Geometry/models/museum/museum-obj/room1_moulding.mtl",
		   t, west_wall, 0, shiny_red)) {
      exit(0);
  }
  */

  west_wall->add(new Rect(west_wall_white, Point(-20, -16, 4), 
		       Vector(0, 12, 0), Vector(0, 0, 4)));

  west_wall->add(new Rect(wall_white, Point(-20-wall_width, -16, 4), 
		       Vector(0, 12+wall_width, 0), Vector(0, 0, 4)));

  //  north_wall->add(new Rect(wall_white, Point(-12, -4, 4), 
  //		       Vector(8, 0, 0), Vector(0, 0, 4)));
  // doorway cut out of North wall for W. tube: attaches to Hologram scene
  // door is from (-9,-4,0) to (-11,-4,0)

  north_wall->add(new Rect(north_wall_white, Point(-15.5, -4, 4), 
		       Vector(4.5, 0, 0), Vector(0, 0, 4)));
  north_wall->add(new Rect(north_wall_white, Point(-7.5, -4, 5), 
		       Vector(3.5, 0, 0), Vector(0, 0, 3)));
  north_wall->add(new Rect(north_wall_white, Point(-6.5, -4, 1), 
		       Vector(2.5, 0, 0), Vector(0, 0, 1)));

  north_wall->add(new Rect(wall_white, Point(-15.5-wall_width/2., -4+wall_width, 4), 
		       Vector(4.5+wall_width/2., 0, 0), Vector(0, 0, 4)));
  north_wall->add(new Rect(wall_white, Point(-7.5+wall_width/2., -4+wall_width, 5), 
		       Vector(3.5+wall_width/2., 0, 0), Vector(0, 0, 3)));
  north_wall->add(new Rect(wall_white, Point(-6.5+wall_width/2., -4+wall_width, 1), 
		       Vector(2.5+wall_width/2., 0, 0), Vector(0, 0, 1)));
  /* door frame */
  const float frame_width = 0.02;
  const float frame_height = 0.10;
  Material *mod_black = new Phong(Color(0.1,0.1,0.1), Color(0.3,0.3,0.3), 20, 0);
  BBox frame;
  frame.extend (Point(-9-frame_height,-4-frame_width,0));
  frame.extend (Point(-9+frame_height,-4+wall_width+frame_width,2));
  north_wall->add(new Box(mod_black,frame.min(),frame.max()));
  frame.reset();
  frame.extend (Point(-11-frame_height,-4-frame_width,2));
  frame.extend (Point(-9+frame_height,-4+wall_width+frame_width,2+frame_height));
  north_wall->add(new Box(mod_black,frame.min(),frame.max()));
  frame.reset();
  frame.extend (Point(-11-frame_height,-4-frame_width,0));
  frame.extend (Point(-11+frame_height,-4+wall_width+frame_width,2));
  north_wall->add(new Box(mod_black,frame.min(),frame.max()));

  //  east_wall->add(new Rect(wall_white, Point(-4, -16, 4), 
  //			  Vector(0, 12, 0), Vector(0, 0, 4)));

  // doorway cut out of East wall for S. tube: attaches to Sphere Room scene
  // door is from (-4,-5,0) to (-4,-7,2)

  east_wall->add(new Rect(his1_wall_white, Point(-4, -17.5, 4), 
		       Vector(0, 10.5, 0), Vector(0, 0, 4)));
  east_wall->add(new Rect(his1_wall_white, Point(-4, -6, 5), 
		       Vector(0, 1, 0), Vector(0, 0, 3)));
  east_wall->add(new Rect(his1_wall_white, Point(-4, -4.5, 4), 
		       Vector(0, 0.5, 0), Vector(0, 0, 4)));

  east_wall->add(new Rect(wall_white, Point(-4+wall_width, -17.5-wall_width/2., 4), 
		       Vector(0, 10.5+wall_width, 0), Vector(0, 0, 4)));
  east_wall->add(new Rect(wall_white, Point(-4+wall_width, -6+wall_width/2., 5), 
		       Vector(0, 1+wall_width/2., 0), Vector(0, 0, 3)));
  east_wall->add(new Rect(wall_white, Point(-4+wall_width, -4.5+wall_width/2., 4), 
		       Vector(0, 0.5+wall_width/2., 0), Vector(0, 0, 4)));

  Material *his_black = new Phong(Color(0.1,0.1,0.1), Color(0.3,0.3,0.3), 20, 0);
  frame.reset();
  frame.extend (Point(-4-frame_width,-5-frame_height,0));
  frame.extend (Point(-4+wall_width+frame_width,-5+frame_height,2));
  east_wall->add(new Box(his_black,frame.min(),frame.max()));
  frame.reset();
  frame.extend (Point(-4-frame_width,-7-frame_height,2));
  frame.extend (Point(-4+wall_width+frame_width,-5+frame_height,2+frame_height));
  east_wall->add(new Box(his_black,frame.min(),frame.max()));
  frame.reset();
  frame.extend (Point(-4-frame_width,-7-frame_height,0));
  frame.extend (Point(-4+wall_width+frame_width,-7+frame_height,2));
  east_wall->add(new Box(his_black,frame.min(),frame.max()));
		 
  /*
  if (!readObjFile("/usr/sci/data/Geometry/models/museum/museum-obj/hall1_moulding.obj",
		   "/usr/sci/data/Geometry/models/museum/museum-obj/hall1_moulding.mtl",
		   t, east_wall, 0, shiny_red)) {
      exit(0);
  }
  if (!readObjFile("/usr/sci/data/Geometry/models/museum/museum-obj/hall2_moulding.obj",
		   "/usr/sci/data/Geometry/models/museum/museum-obj/hall2_moulding.mtl",
		   t, south_wall,0,shiny_red)) {
      exit(0);
  }
*/

#if INSERTCEILING 
  ceiling_floor->add(new Rect(some_wall_white, Point(-12, -16, 8),
  			      Vector(8.16, 0, 0), Vector(0, 12.16, 0)));
#endif

  partitions->add(new Rect(west_wall_white, Point(-8-.15,-14,2.5),
			   Vector(0,10,0),Vector(0,0,2.5)));
  partitions->add(new Rect(his_wall_white, Point(-8+.15,-14,2.5),
			   Vector(0,-10,0),Vector(0,0,2.5)));
  partitions->add(new UVCylinder(all_wall_white, Point(-8,-4,5),
  Point(-8,-24-.15,5),0.15));  
  partitions->add(new Sphere(all_wall_white, Point(-8,-24,5),0.15));
  UVCylinderArc *uvc = new UVCylinderArc(his_wall_white, Point(-8,-24,0),
			       Point(-8,-24,5),0.15);
  uvc->set_arc(M_PI_2,M_PI);
  partitions->add(uvc);

  partitions->add(new Rect(his2_dav_wall_white, Point(-12,-24-.15,2.5),
			   Vector(4, 0, 0), Vector(0,0,2.5)));
  partitions->add(new Rect(his2_dav_wall_white, Point(-12,-24+.15,2.5),
			   Vector(-4, 0, 0), Vector(0,0,2.5)));
  partitions->add(new UVCylinder(his2_dav_wall_white, Point(-16,-24,0),
			       Point(-16,-24,5),0.15));
  partitions->add(new UVCylinder(his2_dav_wall_white, Point(-16,-24,5),
			       Point(-8+.15,-24,5),0.15));
  partitions->add(new Sphere(his2_dav_wall_white, Point(-16,-24,5),0.15));


  partitions->add(new Rect(west_wall_white, Point(-16,-16-.15,2.5),
			   Vector(4,0,0), Vector(0,0,2.5)));
  partitions->add(new Rect(west_wall_white, Point(-16,-16+.15,2.5),
			   Vector(-4,0,0), Vector(0,0,2.5)));
  partitions->add(new UVCylinder(west_wall_white, Point(-12,-16,0),
			       Point(-12,-16,5),0.15));
  partitions->add(new UVCylinder(west_wall_white, Point(-12,-16,5),
			       Point(-20,-16,5),0.15));
  partitions->add(new Sphere(west_wall_white, Point(-12,-16,5),0.15));

  Color cdown(1,1,1);
  Color cup(1,1,1);
  rtrt::Plane groundplane(Point(0,0,-5), Vector(0,0,1));
  Color bgcolor(0.1, 0.1, 0.6);

  //  Scene *scene = new Scene(g, cam, bgcolor, cdown, cup, groundplane, 0.5); 
  Scene *scene = new Scene(new Grid(g,16), cam, bgcolor, cdown, cup, groundplane, 0.7, 
			   Constant_Ambient);
  scene->setBaseAmbientColor(Color(0.5,0.5,0.5));
  EnvironmentMapBackground *emap = new EnvironmentMapBackground ("/usr/sci/data/Geometry/textures/holo-room/environmap2.ppm", Vector(0,0,1));
  scene->set_ambient_environment_map(emap);
  scene->select_shadow_mode( Hard_Shadows );
  scene->maxdepth = 8;

  /* history hall lights */
  Light *HistoryL0 = new Light(Point(-6, -10, 4), Color(1.,1.,1.), 0, 0.3);
  HistoryL0->name_ = "History Hall A";
  Light *HistoryL1 = new Light(Point(-6, -19, 4), Color(1.,1.,1.), 0, 0.3);
  HistoryL1->name_ = "History Hall B";
  Light *HistoryL2 = new Light(Point(-12, -26, 4), Color(1.,1.,1.), 0, 0.4);
  HistoryL2->name_ = "History Hall C";
  scene->add_per_matl_light(HistoryL0);
  scene->add_per_matl_light(HistoryL1);
  scene->add_per_matl_light(HistoryL2);
  scene->select_shadow_mode( Hard_Shadows );
  scene->maxdepth = 8;
  scene->shadowobj = new BV1(shadow);
  //  scene->shadowobj = new Grid(shadow,16);
  scene->animate=false;

  /* David room lights */
  Light *DavL1 = new Light(Point(-14, -10, 4), Color(1.,1.,1.), 0, 0.2);
  DavL1->name_ = "David A";
  scene->add_per_matl_light(DavL1);
  Light *DavL2 = new Light(Point(-11.3, -18.05, 4), Color(1.,1.,1.), 0, 0.2);
  DavL2->name_ = "David B";
  scene->add_per_matl_light(DavL2);
  /*  Light *DavL3 = new Light(Point(-17, -22, 1.4), Color(1.,1.,1.), 0, 0.2);
  DavL3->name_ = "David C";
  scene->add_per_matl_light(DavL3); */

  /* modern room lights */
  Light *ModL1 = new Light(Point(-17, -7, 4), Color(1.,1.,1.), 0, 0.3);
  ModL1->name_ = "Modern Room A";
  scene->add_per_matl_light(ModL1);

  Group *solidg = new Group();
  Group *clearg = new Group();
  
  //  build_david_room (solidg,scene, DavL1, DavL2);
  build_history_hall (solidg, clearg, scene, HistoryL0,HistoryL1, HistoryL2); 
  //  build_modern_room (solidg,scene);

 /*
  Transform outlet_trans;
  // first, get it centered at the origin (in x and y), and scale it
  outlet_trans.pre_translate(Vector(238,-9,-663));
  outlet_trans.pre_scale(Vector(0.00003, 0.00003, 0.00003));
  */

  solidg->add(ceiling_floor);
  solidg->add(south_wall);
  solidg->add(west_wall);
  solidg->add(north_wall);
  solidg->add(east_wall);
  solidg->add(partitions);

  g->add(solidg);
  g->add(clearg);
  shadow->add(solidg);

  add_baseboards (ceiling_floor, HistoryL0,HistoryL1,HistoryL2, DavL1, DavL2, ModL1);

  his_black->my_lights.add (HistoryL0);
  his1_wall_white->my_lights.add (HistoryL0);
  his1_wall_white->my_lights.add (HistoryL1);
  his2_wall_white->my_lights.add (HistoryL2);
  dav_wall_white->my_lights.add (DavL1);
  dav_wall_white->my_lights.add (DavL2);
  mod_wall_white->my_lights.add (ModL1);
  west_wall_white->my_lights.add (DavL1);
  west_wall_white->my_lights.add (DavL2);
  west_wall_white->my_lights.add (ModL1);
  north_wall_white->my_lights.add (ModL1);
  north_wall_white->my_lights.add (HistoryL0);
  his_wall_white->my_lights.add (HistoryL0);
  his_wall_white->my_lights.add (HistoryL1);
  his_wall_white->my_lights.add (HistoryL2);
  his2_dav_wall_white->my_lights.add (HistoryL2);
  his2_dav_wall_white->my_lights.add (DavL1);
  his2_dav_wall_white->my_lights.add (DavL2);
  all_wall_white->my_lights.add (HistoryL0);
  all_wall_white->my_lights.add (HistoryL1);
  all_wall_white->my_lights.add (HistoryL2);
  all_wall_white->my_lights.add (DavL1);
  all_wall_white->my_lights.add (DavL2);
  all_wall_white->my_lights.add (ModL1);
  some_wall_white->my_lights.add (HistoryL0);
  some_wall_white->my_lights.add (HistoryL2);
  some_wall_white->my_lights.add (DavL1);
  some_wall_white->my_lights.add (ModL1);
  floor_mat->my_lights.add(HistoryL2);
  floor_mat->my_lights.add(DavL1);
  floor_mat->my_lights.add(DavL2);
  
  return scene;
}

/* images to be moved to star:
   /home/sci/dmw/stadium/big/alltextures/


3939146/4000880
Allocating 14640415 grid indices (3.6593 per tri, 0.0599671 per cell)

3719120/4000880
Allocating 1755864 grid indices (2.01602 per tri, 0.899002 per cell)

789062/870954
Allocating 1755864 grid indices (2.01602 per tri, 0.899002 per cell)


Allocating 1128848 grid indices (2.37074 per tri, 0.57797 per cell)

helicopter
-eye -10.474 -25.4725 1.7875 -lookat -20.6444 -22.5593 1.7875 -up 0 0 1 -fov 60

*/
