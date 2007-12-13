
#ifndef HVOLUMEVIS_H
#define HVOLUMEVIS_H 1

#include <Packages/rtrt/Core/BrickArray3.h>
#include <Packages/rtrt/Core/BBox.h>
#include <Packages/rtrt/Core/HitInfo.h>
#include <Packages/rtrt/Core/Ray.h>
#include <Packages/rtrt/Core/Color.h>
#include <Packages/rtrt/Core/VolumeVis.h>
#include <Packages/rtrt/Core/VolumeVisDpy.h>

#include <Core/Geometry/Point.h>
#include <Core/Thread/Thread.h>
#include <Core/Thread/Time.h>
#include <Core/Thread/Parallel.h>
#include <Core/Thread/Mutex.h>
#include <Core/Thread/WorkQueue.h>

#include <sgi_stl_warnings_off.h>
#include <fstream>
#include <iostream>
#include <sgi_stl_warnings_on.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>

namespace rtrt {

using SCIRun::Mutex;
using SCIRun::WorkQueue;

template<class DataT>
class VMCell {
public:
  // Need to make sure we have a 64 bit thing
  unsigned long long course_hash;
  // The max of unsigned long long is ULLONG_MAX
  VMCell(): course_hash(0) {}

  void turn_on_bits(DataT min, DataT max, DataT data_min, DataT data_max) {
    // We know that we have 64 bits, so figure out where min and max map
    // into [0..63].
    int min_index = (int)((min-data_min)/(data_max-data_min)*63);
/*      T max_index_prep = ((max-data_min)/(data_max-data_min)*63); */
/*      int max_index = max_index_prep-(int)max_index_prep>0? */
/*        (int)max_index_prep+1:(int)max_index_prep; */
    int max_index = (int)ceil(double((max-data_min)/(data_max-data_min)*63));
    // Do some checks
#if 0
    if (min_index > 63 || max_index < 0)
      // We don't want to turn any bits on
      return;
#else
    // This will handle clamping, I hope.
    if (min_index > 63)
      min_index = 63;
    if (max_index < 0)
      max_index = 0;
#endif
    if (min_index < 0)
      min_index = 0;
    if (max_index > 63)
      max_index = 63;
#if 1
    for (int i = min_index; i <= max_index; i++)
      course_hash |= 1ULL << i;
#else
    // The idea here is to create two bit fields that we can and together.
    //    max    min
    // 00011111111111111  This is the first expression computed from max_index
    // 11111111111110000  This is the second expression computed from min_index
    // 00011111111110000  What we want, which is the and'ed value
    //
    // We then or this with what we already have to turn on bits that
    // haven't already been turned on.  We don't want to turn off bits that
    // have already been turned on.
#  if 0
    unsigned long long high = ((1ULL << max_index) - 1) | (1ULL << max_index);
    unsigned long long low = ~((1ULL << min_index) - 1);
    course_hash |= high & low;      
#  else
    // Here min and max go the other way ( min ---> max ).
    course_hash |= ((1 << (65-min_index)) - 1) & ~((1 << (63-max_index)) - 1);
    //course_hash |= ((1 << (max_index+1)) - 1)/* | (1 << max_index))*/ & ~((1 << min_index) - 1);
#  endif
#endif
  }
  inline VMCell<DataT>& operator |= (const VMCell<DataT>& v) {
    course_hash |= v.course_hash;
    return *this;
  }
  inline bool operator & (const VMCell<DataT>& v) {
    return (course_hash & v.course_hash) != 0;
  }
  void print(bool print_endl = true) {
    for( int i = 0; i < 64; i++) {
      unsigned long long bit= course_hash & (1ULL << i);
      if (bit)
	cout << "1";
      else
	cout << "0";
    }
    if (print_endl) cout << endl;
  }
};

template<class DataT, class MetaCT>
class HVolumeVis: public VolumeVis<DataT> {
protected:
  //  template<class MetaCT>
  class HVIsectContext {
  public:
    // These parameters could be modified and hold accumulating state
    Color total;
    float alpha;
    // These parameters should not change
    int dix_dx, diy_dy, diz_dz;
    MetaCT transfunct;
    double t_inc;
    double t_min;
    double t_max;
    double t_inc_inv;
    Ray ray;
    Context *cx;
  };
  
public:
  Vector datadiag;
  Vector sdiag;
  Vector hierdiag;
  Vector ihierdiag;
  int depth;
  int* xsize;
  int* ysize;
  int* zsize;
  double* ixsize;
  double* iysize;
  double* izsize;
  BrickArray3<MetaCT>* macrocells;
  WorkQueue* work;

  void parallel_calc_mcell(int);
  void calc_mcell(int depth, int ix, int iy, int iz, MetaCT& mcell);
  void isect(int depth, double t_sample,
	     double dtdx, double dtdy, double dtdz,
	     double next_x, double next_y, double next_z,
	     int ix, int iy, int iz,
	     int startx, int starty, int startz,
	     const Vector& cellcorner, const Vector& celldir,
	     HVIsectContext &isctx);
  HVolumeVis(BrickArray3<DataT>& data, DataT data_min, DataT data_max,
	     int depth, Point min, Point max, VolumeVisDpy *dpy,
	     double spec_coeff, double ambient,
	     double diffuse, double specular, int np);
  virtual ~HVolumeVis();
  
  ///////////////////////////////////////
  // From Object
  virtual void print(ostream& out);

  ///////////////////////////////////////
  // From Material
  virtual void shade(Color& result, const Ray& ray,
		     const HitInfo& hit, int depth,
		     double atten, const Color& accumcolor,
		     Context* cx);

};
  

  /////////////////////////////////////////////////
  /////////////////////////////////////////////////
  // C code
  /////////////////////////////////////////////////
  /////////////////////////////////////////////////
  

extern Mutex io_lock_;
  
template<class DataT, class MetaCT>
HVolumeVis<DataT,MetaCT>::HVolumeVis(BrickArray3<DataT>& data,
				     DataT data_min, DataT data_max,
				     int depth, Point min, Point max,
				     VolumeVisDpy *dpy,
				     double spec_coeff, double ambient,
				     double diffuse, double specular, int np)
  : VolumeVis<DataT>(data, data_min, data_max,
		     data.dim1(), data.dim2(), data.dim3(),
		     min, max, spec_coeff, ambient, diffuse, specular, dpy),
    depth(depth)
{
  if(depth<=0)
    depth=1;
  
  datadiag=max-min;
  sdiag=datadiag/Vector(this->nx-1,this->ny-1,this->nz-1);
  
  // Compute all the grid stuff
  xsize=new int[depth];
  ysize=new int[depth];
  zsize=new int[depth];
  int tx=this->nx-1;
  int ty=this->ny-1;
  int tz=this->nz-1;
  for(int i=depth-1;i>=0;i--){
    int nx=(int)(pow(tx, 1./(i+1))+.9);
    tx=(tx+nx-1)/nx;
    xsize[depth-i-1]=nx;
    int ny=(int)(pow(ty, 1./(i+1))+.9);
    ty=(ty+ny-1)/ny;
    ysize[depth-i-1]=ny;
    int nz=(int)(pow(tz, 1./(i+1))+.9);
    tz=(tz+nz-1)/nz;
    zsize[depth-i-1]=nz;
  }
  ixsize=new double[depth];
  iysize=new double[depth];
  izsize=new double[depth];
  cerr << "Calculating depths...\n";
  for(int i=0;i<depth;i++){
    cerr << "xsize=" << xsize[i] << ", ysize=" << ysize[i] << ", zsize=" << zsize[i] << '\n';
    ixsize[i]=1./xsize[i];
    iysize[i]=1./ysize[i];
    izsize[i]=1./zsize[i];
  }
  cerr << "X: ";
  tx=1;
  for(int i=depth-1;i>=0;i--){
    cerr << xsize[i] << ' ';
    tx*=xsize[i];
  }
  cerr << "(" << tx << ")\n";
  if(tx<this->nx-1){
    cerr << "TX TOO SMALL!\n";
    exit(1);
  }
  cerr << "Y: ";
  ty=1;
  for(int i=depth-1;i>=0;i--){
    cerr << ysize[i] << ' ';
    ty*=ysize[i];
  }
  cerr << "(" << ty << ")\n";
  if(ty<this->ny-1){
    cerr << "TY TOO SMALL!\n";
    exit(1);
  }
  cerr << "Z: ";
  tz=1;
  for(int i=depth-1;i>=0;i--){
    cerr << zsize[i] << ' ';
    tz*=zsize[i];
  }
  cerr << "(" << tz << ")\n";
  if(tz<this->nz-1){
    cerr << "TZ TOO SMALL!\n";
    exit(1);
  }
  hierdiag=datadiag*Vector(tx,ty,tz)/Vector(this->nx-1,this->ny-1,this->nz-1);
  ihierdiag=Vector(1.,1.,1.)/hierdiag;
  
  if(depth==1){
    macrocells=0;
  } else {
    macrocells=new BrickArray3<MetaCT>[depth+1];
    int xs=1;
    int ys=1;
    int zs=1;
    for(int d=depth-1;d>=1;d--){
      xs*=xsize[d];
      ys*=ysize[d];
      zs*=zsize[d];
      macrocells[d].resize(xs, ys, zs);
      cerr << "Depth " << d << ": " << xs << "x" << ys << "x" << zs << '\n';
    }
    cerr << "Building hierarchy\n";
#if 0
    MetaCT top;
    calc_mcell(depth-1, 0, 0, 0, top);
    cerr << "Min: " << top.min << ", Max: " << top.max << '\n';
#else
    int nx=xsize[depth-1];
    int ny=ysize[depth-1];
    int nz=zsize[depth-1];
    int totaltop=nx*ny*nz;
    work=new WorkQueue("Building hierarchy");
    work->refill(totaltop, np, 5);
    SCIRun::Parallel<HVolumeVis<DataT,MetaCT> > phelper(this, &HVolumeVis<DataT,MetaCT>::parallel_calc_mcell);
    SCIRun::Thread::parallel(phelper, np, true);
    delete work;
#endif
    cerr << "done\n";
  }
  cerr << "**************************************************\n";
  print(cerr);
  cerr << "**************************************************\n";
}

template<class DataT, class MetaCT>
HVolumeVis<DataT,MetaCT>::~HVolumeVis()
{
}

template<class DataT, class MetaCT>
void HVolumeVis<DataT,MetaCT>::calc_mcell(int depth, int startx, int starty,
					  int startz, MetaCT& mcell)
{
  int endx=startx+xsize[depth];
  int endy=starty+ysize[depth];
  int endz=startz+zsize[depth];
  int nx = this->nx;
  int ny = this->ny;
  int nz = this->nz;
  if(endx>nx-1)
    endx=nx-1;
  if(endy>ny-1)
    endy=ny-1;
  if(endz>nz-1)
    endz=nz-1;
  if(startx>=endx || starty>=endy || startz>=endz){
    /* This cell won't get used... */
    return;
  }
  if(depth==0){
    // We are at the data level.  Loop over each voxel and compute the
    // mcell for this group of voxels.
    BrickArray3<DataT>& data = this->data;
    DataT data_min = this->data_min;
    DataT data_max = this->data_max;
    for(int ix=startx;ix<endx;ix++){
      for(int iy=starty;iy<endy;iy++){
	for(int iz=startz;iz<endz;iz++){
	  DataT rhos[8];
	  rhos[0]=data(ix, iy, iz);
	  rhos[1]=data(ix, iy, iz+1);
	  rhos[2]=data(ix, iy+1, iz);
	  rhos[3]=data(ix, iy+1, iz+1);
	  rhos[4]=data(ix+1, iy, iz);
	  rhos[5]=data(ix+1, iy, iz+1);
	  rhos[6]=data(ix+1, iy+1, iz);
	  rhos[7]=data(ix+1, iy+1, iz+1);
	  DataT minr=rhos[0];
	  DataT maxr=rhos[0];
	  for(int i=1;i<8;i++){
	    if(rhos[i]<minr)
	      minr=rhos[i];
	    if(rhos[i]>maxr)
	      maxr=rhos[i];
	  }
	  // Figure out what bits to turn on running from min to max.
	  mcell.turn_on_bits(minr, maxr, data_min, data_max);
	}
      }
    }
  } else {
    int nxl=xsize[depth-1];
    int nyl=ysize[depth-1];
    int nzl=zsize[depth-1];
    BrickArray3<MetaCT>& mcells=macrocells[depth];
    for(int x=startx;x<endx;x++){
      for(int y=starty;y<endy;y++){
	for(int z=startz;z<endz;z++){
	  // Compute the mcell for this block and store it in tmp
	  MetaCT tmp;
	  calc_mcell(depth-1, x*nxl, y*nyl, z*nzl, tmp);
	  // Stash it away
	  mcells(x,y,z)=tmp;
	  // Now aggregate all the mcells created for this depth by
	  // doing a bitwise or.
  	  mcell |= tmp;
	}
      }
    }
  }
}

// This function should not be called if depth is less than 2.
template<class DataT, class MetaCT>
void HVolumeVis<DataT,MetaCT>::parallel_calc_mcell(int)
{
  int ny=ysize[depth-1];
  int nz=zsize[depth-1];
  int nnx=xsize[depth-2];
  int nny=ysize[depth-2];
  int nnz=zsize[depth-2];
  BrickArray3<MetaCT>& mcells=macrocells[depth-1];
  int s, e;
  while(work->nextAssignment(s, e)){
    for(int block=s;block<e;block++){
      int z=block%nz;
      int y=(block%(nz*ny))/nz;
      int x=(block/(ny*nz));
      MetaCT tmp;
      calc_mcell(depth-2, x*nnx, y*nny, z*nnz, tmp);
      mcells(x,y,z)=tmp;
    }
  }
}

//#define BIGLER_DEBUG

template<class DataT, class MetaCT>
void HVolumeVis<DataT,MetaCT>::isect(int depth, double t_sample,
				     double dtdx, double dtdy, double dtdz,
				  double next_x, double next_y, double next_z,
				     int ix, int iy, int iz,
				     int startx, int starty, int startz,
				     const Vector& cellcorner,
				     const Vector& celldir,
				     HVIsectContext &isctx)
{
#ifdef BIGLER_DEBUG
  cerr << "**************************  start depth: " << depth << "\n";
  cerr << "startx = " << startx << "\tix = " << ix << endl;
  cerr << "starty = " << starty << "\tiy = " << iy << endl;
  cerr << "startz = " << startz << "\tiz = " << iz << endl<<endl<<endl;
  cerr << "celldir = "<<celldir<<", cellcorner = "<<cellcorner<<endl;
  flush(cerr);
#endif
  int cx=xsize[depth];
  int cy=ysize[depth];
  int cz=zsize[depth];
#ifdef BIGLER_DEBUG
  cerr << "cxyz = ("<<cx<<", "<<cy<<", "<<cz<<")\n";
#endif
  BrickArray3<DataT>& data = this->data;
  
  if(depth==0){
    int nx = this->nx;
    int ny = this->ny;
    int nz = this->nz;
    for(;;){
      int gx=startx+ix;
      int gy=starty+iy;
      int gz=startz+iz;
#ifdef BIGLER_DEBUG
      cerr << "starting for loop, gx,gy,gz = ("<<gx<<", "<<gy<<", "<<gz<<")\n";
#endif
      // t is our t_sample
#ifdef BIGLER_DEBUG
      cerr << "t_sample = "<<t_sample<<endl;
      cerr <<"next_x/y/z = ("<<next_x<<", "<<next_y<<", "<<next_z<<")\n";
      Point p0(this->min+sdiag*Vector(gx,gy,gz));
      Point p1(p0+sdiag);
      cerr << "p0 = "<<p0<<", p1 = "<<p1<<endl;
#endif
      // If we have valid samples
      if(gx<nx-1 && gy<ny-1 && gz<nz-1){
#ifdef BIGLER_DEBUG
	cerr << "Doing cell: " << gx << ", " << gy << ", " << gz
	     << " (" << startx << "+" << ix << ", " << starty << "+" << iy << ", " << startz << "+" << iz << ")\n";
#endif
	DataT rhos[8];
	rhos[0]=data(gx, gy, gz);
	rhos[1]=data(gx, gy, gz+1);
	rhos[2]=data(gx, gy+1, gz);
	rhos[3]=data(gx, gy+1, gz+1);
	rhos[4]=data(gx+1, gy, gz);
	rhos[5]=data(gx+1, gy, gz+1);
	rhos[6]=data(gx+1, gy+1, gz);
	rhos[7]=data(gx+1, gy+1, gz+1);

	////////////////////////////////////////////////////////////
	// get the weights
	
	Vector weights = cellcorner+celldir*t_sample;
	double x_weight_high = weights.x()-ix;
	double y_weight_high = weights.y()-iy;
	double z_weight_high = weights.z()-iz;
#ifdef BIGLER_DEBUG
	cerr << "weights = ("<<x_weight_high<<", "<<y_weight_high<<", "<<z_weight_high<<")\n";
#endif
	
	double lz1, lz2, lz3, lz4, ly1, ly2, value;
	lz1 = rhos[0] * (1 - z_weight_high) + rhos[1] * z_weight_high;
	lz2 = rhos[2] * (1 - z_weight_high) + rhos[3] * z_weight_high;
	lz3 = rhos[4] * (1 - z_weight_high) + rhos[5] * z_weight_high;
	lz4 = rhos[6] * (1 - z_weight_high) + rhos[7] * z_weight_high;
	
	ly1 = lz1 * (1 - y_weight_high) + lz2 * y_weight_high;
	ly2 = lz3 * (1 - y_weight_high) + lz4 * y_weight_high;
	
	value = ly1 * (1 - x_weight_high) + ly2 * x_weight_high;
	
	float alpha_factor = this->dpy->lookup_alpha(value) * (1-isctx.alpha);
	if (alpha_factor > 0.001) {
	  // the point is contributing, so compute the color
	  
	  Light* light=isctx.cx->scene->light(0);
          if (0 && light->isOn()) {
          
            // compute the gradient
            Vector gradient;
            double dx = ly2 - ly1;
            
            double dy, dy1, dy2;
            dy1 = lz2 - lz1;
            dy2 = lz4 - lz3;
            dy = dy1 * (1 - x_weight_high) + dy2 * x_weight_high;
            
            double dz, dz1, dz2, dz3, dz4, dzly1, dzly2;
            dz1 = rhos[1] - rhos[0];
            dz2 = rhos[3] - rhos[2];
            dz3 = rhos[5] - rhos[4];
            dz4 = rhos[7] - rhos[6];
            dzly1 = dz1 * (1 - y_weight_high) + dz2 * y_weight_high;
            dzly2 = dz3 * (1 - y_weight_high) + dz4 * y_weight_high;
            dz = dzly1 * (1 - x_weight_high) + dzly2 * x_weight_high;
            double length2 = dx*dx+dy*dy+dz*dz;
            if (length2){
              // this lets the compiler use a special 1/sqrt() operation
              double ilength2 = 1/sqrt(length2);
              gradient = Vector(dx*ilength2, dy*ilength2,dz*ilength2);
            } else {
              gradient = Vector(0,0,0);
            }
            
            Vector light_dir;
            Point current_p = isctx.ray.origin() + isctx.ray.direction()*t_sample - this->min.vector();
            light_dir = light->get_pos()-current_p;
            
            Color temp = color(gradient, isctx.ray.direction(),
                               light_dir.normal(), 
                               *(this->dpy->lookup_color(value)),
                               light->get_color());
            isctx.total += temp * alpha_factor;
          } else {
            isctx.total += this->dpy->lookup_color(value)->operator*(alpha_factor);
          }
	  isctx.alpha += alpha_factor;
	}
	
      }

      // Update the new position
      t_sample += isctx.t_inc;

      // If we have overstepped the limit of the ray
      if(t_sample >= isctx.t_max)
	break;

      // Check to see if we leave the level or not
      bool break_forloop = false;
      while (t_sample > next_x) {
	// Step in x...
	next_x+=dtdx;
	ix+=isctx.dix_dx;
	if(ix<0 || ix>=cx) {
	  break_forloop = true;
	  break;
	}
      }
      while (t_sample > next_y) {
	next_y+=dtdy;
	iy+=isctx.diy_dy;
	if(iy<0 || iy>=cy) {
	  break_forloop = true;
	  break;
	}
      }
      while (t_sample > next_z) {
	next_z+=dtdz;
	iz+=isctx.diz_dz;
	if(iz<0 || iz>=cz) {
	  break_forloop = true;
	  break;
	}
      }
#ifdef BIGLER_DEBUG
      cerr << "t_sample now "<<t_sample<<", ixyz = ("<<ix<<", "<<iy<<", "<<iz<<")\n";
#endif
      if (break_forloop)
	break;

      // This does early ray termination when we don't have anymore
      // color to collect.
      if (isctx.alpha >= RAY_TERMINATION_THRESHOLD)
	break;
    }
  } else {
    BrickArray3<MetaCT>& mcells=macrocells[depth];
    for(;;){
      int gx=startx+ix;
      int gy=starty+iy;
      int gz=startz+iz;
#ifdef BIGLER_DEBUG
      cerr << "startx = " << startx << "\tix = " << ix << endl;
      cerr << "starty = " << starty << "\tiy = " << iy << endl;
      cerr << "startz = " << startx << "\tiz = " << iz << endl;
      cerr << "doing macrocell: " << gx << ", " << gy << ", " << gz << ": "<<endl;
      flush(cerr);
#endif
      if(mcells(gx,gy,gz) & isctx.transfunct){
	// Do this cell...
	int new_cx=xsize[depth-1];
	int new_cy=ysize[depth-1];
	int new_cz=zsize[depth-1];
	int new_ix=(int)((cellcorner.x()+t_sample*celldir.x()-ix)*new_cx);
	int new_iy=(int)((cellcorner.y()+t_sample*celldir.y()-iy)*new_cy);
	int new_iz=(int)((cellcorner.z()+t_sample*celldir.z()-iz)*new_cz);
// 	cerr << "new: " << (cellcorner.x()+t*celldir.x()-ix)*new_cx
// 	<< " " << (cellcorner.y()+t*celldir.y()-iy)*new_cy
// 	<< " " << (cellcorner.z()+t*celldir.z()-iz)*new_cz
// 	<< '\n';
	if(new_ix<0)
	  new_ix=0;
	else if(new_ix>=new_cx)
	  new_ix=new_cx-1;
	if(new_iy<0)
	  new_iy=0;
	else if(new_iy>=new_cy)
	  new_iy=new_cy-1;
	if(new_iz<0)
	  new_iz=0;
	else if(new_iz>=new_cz)
	  new_iz=new_cz-1;
	
	double new_dtdx=dtdx*ixsize[depth-1];
	double new_dtdy=dtdy*iysize[depth-1];
	double new_dtdz=dtdz*izsize[depth-1];
	const Vector dir(isctx.ray.direction());
	double new_next_x;
	if(dir.x() > 0){
	  new_next_x=next_x-dtdx+new_dtdx*(new_ix+1);
	} else {
	  new_next_x=next_x-new_ix*new_dtdx;
	}
	double new_next_y;
	if(dir.y() > 0){
	  new_next_y=next_y-dtdy+new_dtdy*(new_iy+1);
	} else {
	  new_next_y=next_y-new_iy*new_dtdy;
	}
	double new_next_z;
	if(dir.z() > 0){
	  new_next_z=next_z-dtdz+new_dtdz*(new_iz+1);
	} else {
	  new_next_z=next_z-new_iz*new_dtdz;
	}
	int new_startx=gx*new_cx;
	int new_starty=gy*new_cy;
	int new_startz=gz*new_cz;
// 	cerr << "startz=" << startz << '\n';
// 	cerr << "iz=" << iz << '\n';
// 	cerr << "new_cz=" << new_cz << '\n';
	Vector cellsize(new_cx, new_cy, new_cz);
	isect(depth-1, t_sample,
	      new_dtdx, new_dtdy, new_dtdz,
	      new_next_x, new_next_y, new_next_z,
	      new_ix, new_iy, new_iz,
	      new_startx, new_starty, new_startz,
	      (cellcorner-Vector(ix, iy, iz))*cellsize, celldir*cellsize,
	      isctx);
      }

      // We need to determine where the next sample is.  We do this
      // using the closest crossing in x/y/z.  This will be the next
      // sample point.
      double closest;
      if(next_x < next_y && next_x < next_z){
	// next_x is the closest
	closest = next_x;
      } else if(next_y < next_z){
	closest = next_y;
      } else {
	closest = next_z;
      }
	
      double step = ceil((closest - t_sample)*isctx.t_inc_inv);
      t_sample += isctx.t_inc * step;

      if(t_sample >= isctx.t_max)
	break;

      // Now that we have the next sample point, we need to determine
      // which cell it ended up in.  There are cases (grazing corners
      // for example) which will make the next sample not be in the
      // next cell.  Because this case can happen, this code will try
      // to determine which cell the sample will live.
      //
      // Perhaps there is a way to use cellcorner and cellsize to get
      // ix/y/z.  The result would have to be cast to an int, and then
      // next_x/y/z would have to be updated as needed. (next_x +=
      // dtdx * (newix - ix).  The advantage of something like this
      // would be the lack of branches, but it does use casts.
      bool break_forloop = false;
      while (t_sample > next_x) {
	// Step in x...
	next_x+=dtdx;
	ix+=isctx.dix_dx;
	if(ix<0 || ix>=cx) {
	  break_forloop = true;
	  break;
	}
      }
      while (t_sample > next_y) {
	next_y+=dtdy;
	iy+=isctx.diy_dy;
	if(iy<0 || iy>=cy) {
	  break_forloop = true;
	  break;
	}
      }
      while (t_sample > next_z) {
	next_z+=dtdz;
	iz+=isctx.diz_dz;
	if(iz<0 || iz>=cz) {
	  break_forloop = true;
	  break;
	}
      }
      if (break_forloop)
	break;
#ifdef BIGLER_DEBUG
      cerr <<"t_sample = "<<t_sample<<endl;
      cerr <<"next_x/y/z = ("<<next_x<<", "<<next_y<<", "<<next_z<<")\n";
      cerr <<"ixyz = ("<<ix<<", "<<iy<<", "<<iz<<")\n";
#endif
      if (isctx.alpha >= RAY_TERMINATION_THRESHOLD)
	break;
    }
  }
#ifdef BIGLER_DEBUG
  cerr << "**************************    end depth: " << depth << "\n\n\n";
#endif
}

template<class DataT, class MetaCT>
void HVolumeVis<DataT,MetaCT>::shade(Color& result, const Ray& ray,
				     const HitInfo& hit, int ray_depth,
				     double atten, const Color& accumcolor,
				     Context* ctx)
{
  bool fast_render_mode = this->dpy->fast_render_mode;
  // alpha is the accumulating opacities
  // alphas are in levels of opacity: 1 - completly opaque
  //                                  0 - completly transparent
  float alpha = 0;
  Color total(0,0,0);

  float* t_maxp = (float*)hit.scratchpad;
  float t_min = t_maxp[29];
  float t_max = t_maxp[30];
  float child_hit = t_maxp[31];
  bool shade_child = false;
  if (child_hit < t_max) {
    t_max = child_hit;
    shade_child = true;
  }

#ifdef BIGLER_DEBUG
  cerr << "\t\tt_min = "<<t_min<<", t_max = "<<t_max<<endl;
  cerr << "ray.origin = "<<ray.origin()<<", dir = "<<ray.direction()<<endl;
  cerr << "sdiag = "<<sdiag<<endl;
#endif
  
  if (fast_render_mode) {
  const Vector dir(ray.direction());
  const Point orig(ray.origin());
  int dix_dx;
  int ddx;
  if(dir.x() > 0){
    dix_dx=1;
    ddx=1;
  } else {
    dix_dx=-1;
    ddx=0;
  }	
  int diy_dy;
  int ddy;
  if(dir.y() > 0){
    diy_dy=1;
    ddy=1;
  } else {
    diy_dy=-1;
    ddy=0;
  }
  int diz_dz;
  int ddz;
  if(dir.z() > 0){
    diz_dz=1;
    ddz=1;
  } else {
    diz_dz=-1;
    ddz=0;
  }

  Point start_p(orig+dir*t_min);
  Vector s((start_p-this->min)*ihierdiag);
  //cout << "s = " << s << "\tdepth = " << depth << endl;
  int cx=xsize[depth-1];
  int cy=ysize[depth-1];
  int cz=zsize[depth-1];
  int ix=(int)(s.x()*cx);
  int iy=(int)(s.y()*cy);
  int iz=(int)(s.z()*cz);
  //cerr << "ix = " << ix << endl;
  //cerr << "iy = " << iy << endl;
  //cerr << "iz = " << iz << endl;
  if(ix>=cx)
    ix--;
  if(iy>=cy)
    iy--;
  if(iz>=cz)
    iz--;
  if(ix<0)
    ix++;
  if(iy<0)
    iy++;
  if(iz<0)
    iz++;
  //cerr << "ix = " << ix << endl;
  //cerr << "iy = " << iy << endl;
  //cerr << "iz = " << iz << endl;
  
  double next_x, next_y, next_z;
  double dtdx, dtdy, dtdz;

  double icx=ixsize[depth-1];
  double x=this->min.x()+hierdiag.x()*double(ix+ddx)*icx;
  double xinv_dir=1./dir.x();
  next_x=(x-orig.x())*xinv_dir;
  dtdx=dix_dx*hierdiag.x()*icx*xinv_dir;

  double icy=iysize[depth-1];
  double y=this->min.y()+hierdiag.y()*double(iy+ddy)*icy;
  double yinv_dir=1./dir.y();
  next_y=(y-orig.y())*yinv_dir;
  dtdy=diy_dy*hierdiag.y()*icy*yinv_dir;

  double icz=izsize[depth-1];
  double z=this->min.z()+hierdiag.z()*double(iz+ddz)*icz;
  double zinv_dir=1./dir.z();
  next_z=(z-orig.z())*zinv_dir;
  dtdz=diz_dz*hierdiag.z()*icz*zinv_dir;
  
  Vector cellsize(cx,cy,cz);
  // cellcorner and celldir can be used to get the location in terms
  // of the metacell in index space.
  //
  // For example if you wanted to get the location at time t (world
  // space units) in terms of indexspace you would do the following
  // computation:
  //
  // Vector pos = cellcorner + celldir * t + Vector(startx, starty, startz);
  //
  // If you wanted to get how far you are inside a given cell you
  // could use the following code:
  //
  // Vector weights = cellcorner + celldir * t - Vector(ix, iy, iz);
  Vector cellcorner((orig-this->min)*ihierdiag*cellsize);
  Vector celldir(dir*ihierdiag*cellsize);

  HVIsectContext isctx;
  isctx.total = total;
  isctx.alpha = alpha;
  isctx.dix_dx = dix_dx;
  isctx.diy_dy = diy_dy;
  isctx.diz_dz = diz_dz;
  isctx.transfunct.course_hash = this->dpy->course_hash;
  //  isctx.transfunct.print();
  isctx.t_inc = this->dpy->t_inc;
  isctx.t_min = t_min;
  isctx.t_max = t_max;
  isctx.t_inc_inv = 1/isctx.t_inc;
  isctx.ray = ray;
  isctx.cx = ctx;
  
  isect(depth-1, t_min, dtdx, dtdy, dtdz, next_x, next_y, next_z,
	ix, iy, iz, 0, 0, 0,
	cellcorner, celldir,
	isctx);

  alpha = isctx.alpha;
  total = isctx.total;

  } else {
    float t_inc = this->dpy->t_inc;

    Vector inv_diag(this->inv_diag);
    BrickArray3<DataT>& data = this->data;
    int nx = this->nx;
    int ny = this->ny;
    int nz = this->nz;
    
    for(float t = t_min; t < t_max; t += t_inc) {
      // opaque values are 0, so terminate the ray at alpha values close to zero
      if (alpha < RAY_TERMINATION_THRESHOLD) {
	// get the point to interpolate
	Point current_p = ray.origin() + ray.direction() * t - this->min.vector();
	
	////////////////////////////////////////////////////////////
	// interpolate the point
	
	// get the indices and weights for the indicies
	double norm = current_p.x() * inv_diag.x();
	double step = norm * (nx - 1);
	int x_low = clamp(0, (int)step, data.dim1()-2);
	float x_weight_high = step - x_low;
	
	norm = current_p.y() * inv_diag.y();
	step = norm * (ny - 1);
	int y_low = clamp(0, (int)step, data.dim2()-2);
	float y_weight_high = step - y_low;
	
	norm = current_p.z() * inv_diag.z();
	step = norm * (nz - 1);
	int z_low = clamp(0, (int)step, data.dim3()-2);
	float z_weight_high = step - z_low;

	////////////////////////////////////////////////////////////
	// do the interpolation
	
	DataT a,b,c,d,e,f,g,h;
	a = data(x_low,   y_low,   z_low);
	b = data(x_low,   y_low,   z_low+1);
	c = data(x_low,   y_low+1, z_low);
	d = data(x_low,   y_low+1, z_low+1);
	e = data(x_low+1, y_low,   z_low);
	f = data(x_low+1, y_low,   z_low+1);
	g = data(x_low+1, y_low+1, z_low);
	h = data(x_low+1, y_low+1, z_low+1);
	
	float lz1, lz2, lz3, lz4, ly1, ly2, value;
	lz1 = a * (1 - z_weight_high) + b * z_weight_high;
	lz2 = c * (1 - z_weight_high) + d * z_weight_high;
	lz3 = e * (1 - z_weight_high) + f * z_weight_high;
	lz4 = g * (1 - z_weight_high) + h * z_weight_high;
	
	ly1 = lz1 * (1 - y_weight_high) + lz2 * y_weight_high;
	ly2 = lz3 * (1 - y_weight_high) + lz4 * y_weight_high;
	
	value = ly1 * (1 - x_weight_high) + ly2 * x_weight_high;
	
	//cout << "value = " << value << endl;

	float alpha_factor = this->dpy->lookup_alpha(value) * (1-alpha);
	if (alpha_factor > 0.001) {
	  //      if (true) {
	  // the point is contributing, so compute the color
	  Light* light=ctx->scene->light(0);
          if (0 && light->isOn()) {
	  
            // compute the gradient
            Vector gradient;
            float dx = ly2 - ly1;
            
            float dy, dy1, dy2;
            dy1 = lz2 - lz1;
            dy2 = lz4 - lz3;
            dy = dy1 * (1 - x_weight_high) + dy2 * x_weight_high;
            
            float dz, dz1, dz2, dz3, dz4, dzly1, dzly2;
            dz1 = b - a;
            dz2 = d - c;
            dz3 = f - e;
            dz4 = h - g;
            dzly1 = dz1 * (1 - y_weight_high) + dz2 * y_weight_high;
            dzly2 = dz3 * (1 - y_weight_high) + dz4 * y_weight_high;
            dz = dzly1 * (1 - x_weight_high) + dzly2 * x_weight_high;
            float length2 = dx*dx+dy*dy+dz*dz;
            if (length2){
              // this lets the compiler use a special 1/sqrt() operation
              float ilength2 = 1.0f/sqrtf(length2);
              gradient = Vector(dx*ilength2, dy*ilength2, dz*ilength2);
            } else {
              gradient = Vector(0,0,0);
            }
            
            Vector light_dir;
            light_dir = light->get_pos()-current_p;
	  
            Color temp = color(gradient, ray.direction(), light_dir.normal(), 
                               *(this->dpy->lookup_color(value)),
                               light->get_color());
            total += temp * alpha_factor;
          } else {
            total += this->dpy->lookup_color(value)->operator*(alpha_factor);
          }
	  alpha += alpha_factor;
	}
      } else {
	break;
      }
    }
  }
  
  if (alpha < RAY_TERMINATION_THRESHOLD) {
    Color bgcolor;
    if (this->child && shade_child) {
      HitInfo hit_child = hit;
      hit_child.hit_obj = this->child;
      hit_child.min_t = child_hit;
      this->child->get_matl()->shade(bgcolor, ray, hit_child, depth+1,
                                     atten, accumcolor, ctx);
    } else {
      // Grab the background color
      Ray r(ray.origin() + ray.direction() * t_max, ray.direction());
#if 1
      // We should do something to prevent infinate recursion by
      // checking the depth.
      if (depth < ctx->scene->maxdepth)
        Worker::traceRay(bgcolor, r, ray_depth+1, atten, accumcolor, ctx);
      else
        ctx->scene->get_bgcolor( ray.direction(), bgcolor );
#else
      Worker::traceRay(bgcolor, r, ray_depth+1, atten, accumcolor, ctx);
#endif
      
    }
    total += bgcolor * (1-alpha);
  }
  result = total;
}

template<class DataT, class MetaCT>
void HVolumeVis<DataT,MetaCT>::print(ostream& out) {
  //  out << "name_ = "<<get_name()<<endl;
  out << "min = "<<this->min<<", max = "<<this->max<<endl;
  out << "datadiag = "<<this->datadiag<<", hierdiag = "<<hierdiag<<", ihierdiag = "<<ihierdiag<<", sdiag = "<<sdiag<<endl;
  out << "dim = ("<<this->nx<<", "<<this->ny<<", "<<this->nz<<")\n";
  out << "data.get_datasize() = "<<this->data.get_datasize()<<endl;
  out << "data_min = "<<this->data_min<<", data_max = "<<this->data_max<<endl;
  out << "depth = "<<depth<<endl;
}

} // end namespace rtrt

#endif // HVOLUMEVIS_H
