
/*
 *  TriSurface.cc: Triangulated Surface Data type
 *
 *  Written by:
 *   David Weinstein
 *   Department of Computer Science
 *   University of Utah
 *   July 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#ifdef _WIN32
#pragma warning(disable:4291) // quiet the visual C++ compiler
#endif

#include <Core/Datatypes/TriSurface.h>

#include <Core/Util/Assert.h>
#include <Core/Util/NotFinished.h>
#include <Core/Containers/TrivialAllocator.h>
#include <Core/Containers/Queue.h>
#include <Core/Datatypes/SurfTree.h>
#include <Core/Geometry/BBox.h>
#include <Core/Geometry/Grid.h>
#include <Core/Math/MiscMath.h>
#include <Core/Malloc/Allocator.h>

#include <iostream>
using std::cerr;

#ifdef _WIN32
#include <stdlib.h>
#define drand48() rand()
#endif

namespace SCIRun {


static Persistent *
make_TriSurface()
{
  return scinew TriSurface;
}


PersistentTypeID TriSurface::type_id("TriSurface", "Surface", make_TriSurface);


TriSurface::TriSurface()
  : Surface(TriSurf, 0),
    empty_index(-1),
    normType(NrmlsNone)
{
}


TriSurface::TriSurface(const TriSurface& copy)
  : Surface(copy)
{
  points_ = copy.points_;
  faces_ = copy.faces_;
  normals = copy.normals;
}


TriSurface::~TriSurface()
{
}


bool
TriSurface::inside(const Point &)
{
  NOT_FINISHED("TriSurface::inside");
  return true;
}


void
TriSurface::buildNormals(NormalsType nt)
{
  normals.resize(0);
  normType=NrmlsNone;

  // build per vertex, per point or per element normals

  // point->point     (return)                1
  // point->element   x-product		
  // point->vertex    copy			3
  // point->none      (throw away)		2

  // element->point	average			
  // element->element (return)		1
  // element->vertex  copy			4
  // element->none    (throw away)		2

  // vertex->point    average			
  // vertex->element  x-product		
  // vertex->vertex   (return)		1
  // vertex->none	(throw away)		2

  // none->point      x-products		
  // none->element    x-products		
  // none->vertex     average of x-products	
  // none->none       (return)		1

  if (normType==nt) return;			// 1
  if (nt==NrmlsNone)
  {normals.resize(0); normType=nt; return;}	// 2
  if (normType==PointType && nt==VertexType)    // 3
  {			
    // We want normals at the vertices, we have them at each point.
    Array1<Vector> old(normals);
    normals.resize(faces_.size()*3);
    for (int i=0; i<faces_.size(); i++)
    {
      const TSElement &e = faces_[i];
      normals[i*3]=old[e.i1];
      normals[i*3+1]=old[e.i2];
      normals[i*3+2]=old[e.i3];
    }
    normType=VertexType;
    return;
  }
  if (normType==ElementType && nt==VertexType)  // 4
  {			
    // We want normals at the vertices, we have them at the elements.
    normals.resize(faces_.size()*3);
    for (int i=normals.size()/3; i>=0; i--)
    {
      normals[i]=normals[i/3];
    }
    normType=VertexType;
  }

  // Store/Compute the vertex normals in tmp, then we'll compute from those.
  Array1<Vector> tmp(faces_.size()*3);
  if (normType==VertexType && nt==PointType)
  {
    tmp=normals;
  }
  else if (normType==ElementType && nt==PointType)
  {
    for (int i=0; i<faces_.size()*3; i++)
      tmp[i*3]=tmp[i*3+1]=tmp[i*3+2]=normals[i];
  }
  else
  {
    // For the rest we'll build a temp array of x-products at the elements.
    for (int i=0; i<faces_.size(); i++)
    {
      const TSElement &e = faces_[i];
      Vector v(Cross((points_[e.i1]-points_[e.i2]),
		     (points_[e.i1]-points_[e.i3])));
      v.normalize();
      tmp[i*3]=tmp[i*3+1]=tmp[i*3+2]=v;
    }
  }

  // now, for those that want them at the vertices, we copy;
  // for those that want them at the elements, we grab one;
  // and for those that want them at the points, we average.
  if (nt==VertexType)
  {
    normals=tmp;
  }
  else if (nt==ElementType)
  {
    normals.resize(faces_.size());
    for (int i=0; i<faces_.size(); i++)
      normals[i]=tmp[i*3];
  }
  else
  {
    normals.resize(points_.size());
    normals.initialize(Vector(0,0,0));

    int i;
    for (i=0; i<faces_.size(); i++)
    {
      const TSElement &e = faces_[i];
      normals[e.i1]+=tmp[i*3];
      normals[e.i2]+=tmp[i*3+1];
      normals[e.i3]+=tmp[i*3+2];
    }
    for (i=0; i<points_.size(); i++)
    {
      if (normals[i].length2()) normals[i].normalize();
      else
      {
	cerr << "Error -- normalizing a zero vector to (1,0,0)\n";
	normals[i].x(1);
      }
    }
  }
  normType=nt;
}



int
TriSurface::get_closest_vertex_id(const Point &p1, const Point &p2,
				  const Point &p3)
{
  if (grid==0)
  {
    ASSERTFAIL("Can't run TriSurface::get_closest_vertex_id() w/o a grid\n");
  }
  int i[3], j[3], k[3];	// grid element indices containing these points
  int maxi, maxj, maxk, mini, minj, mink;
  grid->get_element(p1, &(i[0]), &(j[0]), &(k[0]));
  grid->get_element(p2, &(i[1]), &(j[1]), &(k[1]));
  grid->get_element(p3, &(i[2]), &(j[2]), &(k[2]));

  maxi=Max(i[0],i[1],i[2]); mini=Min(i[0],i[1],i[2]);
  maxj=Max(j[0],j[1],j[2]); minj=Min(j[0],j[1],j[2]);
  maxk=Max(k[0],k[1],k[2]); mink=Min(k[0],k[1],k[2]);

  int rad=Max(maxi-mini,maxj-minj,maxk-mink)/2;
  int ci=(maxi+mini)/2; int cj=(maxj+minj)/2; int ck=(maxk+mink)/2;

  //    BBox bb;
  //    bb.extend(p1); bb.extend(p2); bb.extend(p3);

  // We make a temporary surface which just contains this one triangle,
  // so we can use the existing Surface->distance code to find the closest
  // vertex to this triangle.

  TriSurface* surf=scinew TriSurface;
  surf->construct_grid(grid->dim1(), grid->dim2(), grid->dim3(),
		       grid->get_min(), grid->get_spacing());
  surf->points_.add(p1);
  surf->points_.add(p2);
  surf->points_.add(p3);
  surf->add_triangle(0,1,2);

  Array1<int> cu;
  for(;;)
  {
    double dist=0;
    int vid=-1;
    grid->get_cubes_at_distance(rad,ci,cj,ck, cu);
    for (int i=0; i<cu.size(); i+=3)
    {
      Array1<int> &el=*grid->get_members(cu[i], cu[i+1], cu[i+2]);
      if (&el)
      {
	for (int j=0; j<el.size(); j++)
	{
	  Array1<int> res;
	  double tdist=surf->distance(points_[faces_[el[j]].i1],
				      res);
	  if (vid==-1 || (Abs(tdist) < Abs(dist)))
	  {
	    vid = faces_[el[j]].i1;
	    dist = tdist;
	  }
	}
      }
    }
    if (vid != -1) return vid;
    rad++;
  }
}


int
TriSurface::find_or_add(const Point &p)
{
  if (pntHash==0)
  {
    points_.add(p);
    return(points_.size()-1);
  }

  MapIntInt::iterator oldId;
  const int val =
    (Round((p.z()-hash_min.z())/resolution) * hash_y +
     Round((p.y()-hash_min.y())/resolution)) * hash_x +
    Round((p.x()-hash_min.x())/resolution);

  oldId = pntHash->find(val);
  if (oldId != pntHash->end())
  {
    return (*oldId).second;
  }
  else
  {
    (*pntHash)[val] = points_.size();
    points_.add(p);	
    return points_.size()-1;
  }
}


int
TriSurface::cautious_add_triangle(const Point &p1, const Point &p2,
				  const Point &p3, int cw)
{
  if (grid==0)
  {
    ASSERTFAIL("Can't run TriSurface::cautious_add_triangle w/o a grid\n");
  }
  int i1=find_or_add(p1);
  int i2=find_or_add(p2);
  int i3=find_or_add(p3);
  return (add_triangle(i1,i2,i3,cw));
}


int
TriSurface::add_triangle(int i1, int i2, int i3, int cw)
{
  if (!cw)
  {
    const int swap = i1;
    i1 = i3;
    i3 = swap;
  }

  int temp;
  if (i1==i2 || i1==i3)	// don't even add degenerate triangles
    return -1;
  if (empty_index == -1)
  {
    faces_.add(TSElement(i1, i2, i3));
    temp = faces_.size()-1;
  }
  else
  {
    faces_[empty_index].i1 = i1;
    faces_[empty_index].i2 = i2;
    faces_[empty_index].i3 = i3;
    temp=empty_index;
    empty_index=-1;
  }

  //if we have a grid add this triangle to it
  if (grid) grid->add_triangle(temp, points_[i1], points_[i2], points_[i3]);

  return temp;
}


void
TriSurface::separate(int idx, TriSurface* conn, TriSurface* d_conn, int updateConnIndices, int updateDConnIndices)
{
  if (idx<0 || idx>points_.size())
  {
    cerr << "Trisurface:separate() failed -- index out of range";
    return;
  }

  // brute force -- every node has its neighbor nodes in this list... twice!
  Array1<Array1<int> > nbr(points_.size());
  Array1<int> newLocation(points_.size());
  Array1<int> d_newLocation(points_.size());
  Array1<int> visited(points_.size());

  int i;
  for (i=0; i<points_.size(); i++)
  {
    newLocation[i]=d_newLocation[i]=-1;
    visited[i]=0;
  }
  for (i=0; i<points_.size(); i++)
  {
    nbr[i].resize(0);
  }
  for (i=0; i<faces_.size(); i++)
  {
    nbr[faces_[i].i1].add(faces_[i].i2);	
    nbr[faces_[i].i1].add(faces_[i].i3);	
    nbr[faces_[i].i2].add(faces_[i].i1);	
    nbr[faces_[i].i2].add(faces_[i].i3);	
    nbr[faces_[i].i3].add(faces_[i].i1);	
    nbr[faces_[i].i3].add(faces_[i].i2);	
  }
  Queue<int> q;
  q.append(idx);
  visited[idx]=1;
  while(!q.is_empty())
  {
    // enqueue non-visited neighbors
    int c=q.pop();
    for (int j=0; j<nbr[c].size(); j++)
    {
      if (!visited[nbr[c][j]])
      {
	q.append(nbr[c][j]);
	visited[nbr[c][j]] = 1;
      }
    }
  }

  if (updateConnIndices)
  {
    conn->points_.resize(0);
  }
  else
  {
    conn->points_ = points_;
  }

  if (updateDConnIndices)
  {
    d_conn->points_.resize(0);
  }
  else
  {
    d_conn->points_=points_;
  }

  if (updateConnIndices || updateDConnIndices)
  {
    for (i=0; i<visited.size(); i++)
    {
      if (visited[i])
      {
	if (updateConnIndices)
	{
	  const int tmp = conn->points_.size();
	  conn->points_.add(points_[i]);	
	  newLocation[i] = tmp;
	}
      }
      else
      {
	if (updateDConnIndices)
	{
	  const int tmp = d_conn->points_.size();		
	  d_conn->points_.add(points_[i]);
	  d_newLocation[i] = tmp;
	}
      }
    }
  }

  for (i=0; i<faces_.size(); i++)
    if (visited[faces_[i].i1])
    {
      if (updateConnIndices)
	conn->faces_.add(TSElement(newLocation[faces_[i].i1],
				   newLocation[faces_[i].i2],
				   newLocation[faces_[i].i3]));
      else
	conn->faces_.add(TSElement(faces_[i].i1,
				   faces_[i].i2,
				   faces_[i].i3));
    }
    else
    {
      if (updateDConnIndices)
	d_conn->faces_.add(TSElement(d_newLocation[faces_[i].i1],
				     d_newLocation[faces_[i].i2],
				     d_newLocation[faces_[i].i3]));
      else
	d_conn->faces_.add(TSElement(faces_[i].i1,
				     faces_[i].i2,
				     faces_[i].i3));
    }
}		


void
TriSurface::remove_empty_index()
{
  if (empty_index!=-1)
  {
    faces_.remove(empty_index);
    empty_index=-1;
  }
}


void
TriSurface::construct_grid(int xdim, int ydim, int zdim,
			   const Point &min, double spacing)
{
  remove_empty_index();
  if (grid) delete grid;
  grid = scinew Grid(xdim, ydim, zdim, min, spacing);
  for (int i=0; i<faces_.size(); i++)
    grid->add_triangle(i, points_[faces_[i].i1],
		       points_[faces_[i].i2], points_[faces_[i].i3]);
}


void
TriSurface::construct_hash(int xdim, int ydim, const Point &p, double res)
{
  xdim=(int)(xdim/res);
  ydim=(int)(ydim/res);
  hash_x = xdim;
  hash_y = ydim;
  hash_min = p;
  resolution = res;
  if (pntHash)
  {
    delete pntHash;
  }
  pntHash = scinew MapIntInt;
  for (int i=0; i<points_.size(); i++)
  {
    int val=(Round((points_[i].z()-p.z())/res)*ydim+
	     Round((points_[i].y()-p.y())/res))*xdim+
      Round((points_[i].x()-p.x())/res);
    (*pntHash)[val] = i;
  }
}


// Method to find the distance from a point to the surface.  the algorithm
// goes like this:
// find which "cube" of the mesh the point is in; look for nearest neighbors;
// determine if it's closest to a vertex, edge, or face, and store
// the type in "type" and the info (i.e. edge #, vertex #, triangle #, etc)
// in "res" (result); finally, return the distance.
// the information in res will be stored thus...
// if the thing we're closest to is a:
// 	face   -- [0]=triangle index
//	edge   -- [0]=triangle[1] index
//	  	  [1]=triangle[1] edge #
//		  [2]=triangle[2] index
//		  [3]=triangle[2] edge #
//	vertex -- [0]=triangle[1] index
//		  [1]=triangle[1] vertex #
//		   ...

double
TriSurface::distance(const Point &p,Array1<int> &res, Point *pp)
{
  if (grid==0)
  {
    ASSERTFAIL("Can't run TriSurface::distance w/o a grid\n");
  }

  Array1<int>* elem;
  Array1<int> tri;
  int i, j, k, imax, jmax, kmax;

  double dmin;
  double sp=grid->get_spacing();
  grid->get_element(p, &i, &j, &k, &dmin);
  grid->size(&imax, &jmax, &kmax);
  imax--; jmax--; kmax--;
  int dist=0;
  bool done=false;
  double Dist=1000000;
  Array1<int> info;
  int type;
  int max_dist=Max(imax,jmax,kmax);

  Array1<int> candid;
  while (!done)
  {
    while (!tri.size())
    {
      grid->get_cubes_within_distance(dist, i, j, k, candid);
      for(int index=0; index<candid.size(); index+=3)
      {
	elem=grid->get_members(candid[index], candid[index+1],
			       candid[index+2]);
	if (elem)
	{
	  for (int a=0; a<elem->size(); a++)
	  {
	    int duplicate;
	    int b;
	    for (duplicate=0, b=0; b<tri.size(); b++)
	      if (tri[b]==(*elem)[a]) duplicate=1;
	    if (!duplicate) tri.add((*elem)[a]);
	  }
	}
      }
      dist++;
      ASSERT(dist<=max_dist+2);
    }
    // now tri holds the indices of the triangles we're closest to

    Point ptemp;
	
    for (int index=0; index<tri.size(); index++)
    {
      double d=distance(p, tri[index], &type, &ptemp);
      if (Abs(d-Dist)<.00001)
      {
	if (type==0)
	{
	  info.remove_all();
	  Dist=d;
	  if (pp!=0) (*pp)=ptemp;
	  info.add(tri[index]);
	}
	else
	{
	  if (res.size() != 1)
	  {
	    info.add(tri[index]);
	    info.add((type-1)%3);
	    if (pp!=0) (*pp)=ptemp;
	  }
	}
      }
      else if (Abs(d)<Abs(Dist))
      {
	info.remove_all();
	Dist=d;
	if (pp!=0) *pp=ptemp;
	info.add(tri[index]);
	if (type>0) info.add((type-1)%3);
      }
    }

    tri.remove_all();

    // If our closest point is INSIDE of the squares we looked at...
    if (Abs(Dist)<(dmin+(dist-1)*sp))
      done=true;		// ... we're done.
    else
      info.remove_all();
  }
	
  res=info;
  return Dist;
}


// This is basically the ray/triangle interesect code from the ray-tacing
// chapter in Graphics Gems I.  Much of the c-code comes from page 735 --
// thanks to Didier Badouel.  Other parts, and a discussion of the algorithm
// were presented on pages 390-393.

// We return the signed distance from the point to the triangle "el",
// and put the type of intersection (face, edge or vertex) into the
// variable type.  If we're closest to...
//	the face, then *type=0
//	an edge,  then *type=1+vertex# (that we're furthest from)
//      a vertex, then *type=4+vertex# (that we're closest to)

double
TriSurface::distance(const Point &p, int el, int *type, Point *pp)
{
  const Point &a = points_[faces_[el].i1];
  const Point &b = points_[faces_[el].i2];
  const Point &c = points_[faces_[el].i3];

  double V[3][3];	// our array of vertices
  V[0][0]=a.x(); V[0][1]=a.y(); V[0][2]=a.z();
  V[1][0]=b.x(); V[1][1]=b.y(); V[1][2]=b.z();
  V[2][0]=c.x(); V[2][1]=c.y(); V[2][2]=c.z();

  Vector e(a-b);
  Vector f(a-c);
  Vector N(Cross(e,f));
  N.normalize();
  double d=-(a.x()*N.x()+a.y()*N.y()+a.z()*N.z());
  double t=-(d+Dot(N, Vector(p-Point(0,0,0))));
  int sign=Sign(t);
  Point Pp(p+N*t);

  double P[3]; // our point on the plane
  P[0]=Pp.x(); P[1]=Pp.y(); P[2]=Pp.z();

  int i[3];	// order the normal components backwards by magnitude
  if (Abs(N.x()) > Abs(N.y()))
  {
    if (Abs(N.x()) > Abs(N.z()))
    {
      i[0]=0; i[1]=1; i[2]=2;
    }
    else
    {
      i[0]=2; i[1]=0; i[2]=1;
    }
  }
  else
  {
    if (Abs(N.y()) > Abs(N.z()))
    {
      i[0]=1; i[1]=0; i[2]=2;
    }
    else
    {
      i[0]=2; i[1]=0; i[2]=1;
    }
  }

  int I=2;	// because we only have a triangle
  double u0=P[i[1]]-V[0][i[1]];
  double v0=P[i[2]]-V[0][i[2]];
  double u1=V[I-1][i[1]]-V[0][i[1]];
  double v1=V[I-1][i[2]]-V[0][i[2]];
  double u2=V[I][i[1]]-V[0][i[1]];
  double v2=V[I][i[2]]-V[0][i[2]];

  double alpha, beta;
  int inter=0;

  if (Abs(u1)<.0001)
  {
    beta=u0/u2;
    if ((beta >= -0.0001) && (beta <= 1.0001))
    {
      alpha = (v0-beta*v2)/v1;
      if ((inter=((alpha>=-0.0001) && ((alpha+beta)<=1.0001))))
      {
	if (pp!=0)
	{
	  (*pp)=Pp;
	}
      }
    }
  }
  else
  {
    beta=(v0*u1-u0*v1)/(v2*u1-u2*v1);
    if ((beta >= -0.000001)&&(beta<=1.0001))
    {
      alpha=(u0-beta*u2)/u1;
      if ((inter=((alpha>=-0.0001) && ((alpha+beta)<=1.0001))))
      {
	if (pp!=0)
	{
	  (*pp)=Pp;
	}
      }
    }
  }
  *type = 0;
  if (inter) return t;

  // we know the point is outside of the triangle (i.e. the distance is
  // *not* simply the distance along the normal from the point to the
  // plane).  so, we find out which of the six areas it's in by checking
  // which lines it's "inside" of, and which ones it's "outside" of.

  // first find the slopes and the intercepts of the lines.
  // since i[1] and i[2] are the projected components, we'll just use those
  //	   as x and y.

  // now form our projected vectors for edges, such that a known inside
  //     point is in fact inside.

  // Note: this could be optimized to take advantage of the surface being
  //     directed.  Wouldn't need to check signs, etc.

  double A[3][3], B[3][3], C[3][3];
  //double mid[2];
  //mid[0]=(V[0][i[1]]+V[1][i[1]]+V[2][i[1]])/3;
  //mid[1]=(V[0][i[2]]+V[1][i[2]]+V[2][i[2]])/3;

  int X;
  for (X=0; X<3; X++)
  {
    // handle vertical lines
    if (V[(X+2)%3][i[1]]==V[(X+1)%3][i[1]])
    { 		// x=const.
      B[X][2]=0;
      // primary edge
      A[X][2]=1; C[X][2]=-V[(X+2)%3][i[1]];
      // perpindicular segments
      A[X][1]=A[X][0]=0; B[X][0]=1; B[X][1]=-1;
      C[X][0]=-V[(X+1)%3][i[2]]; C[X][1]=V[(X+2)%3][i[2]];
    }
    else
    {						// not vertical
      B[X][2]=1; A[X][2]=-((V[(X+2)%3][i[2]]-V[(X+1)%3][i[2]])/
			   (V[(X+2)%3][i[1]]-V[(X+1)%3][i[1]]));
      C[X][2]=-B[X][2]*V[(X+2)%3][i[2]]-A[X][2]*V[(X+2)%3][i[1]];
      if (A[X][2]==0)
      {
	A[X][0]=-1; A[X][1]=1;
	B[X][0]=B[X][1]=0;
      }
      else
      {		
	B[X][0]=-1; B[X][1]=1;
	A[X][0]=1/A[X][2];
	A[X][1]=-A[X][0];
      }
      C[X][0]=-B[X][0]*V[(X+1)%3][i[2]]-A[X][0]*V[(X+1)%3][i[1]];
      C[X][1]=-B[X][1]*V[(X+2)%3][i[2]]-A[X][1]*V[(X+2)%3][i[1]];
    }
  }
  // now make sure we have all of the signs right!
  for (X=0; X<3; X++)
  {
    for (int j=0; j<3; j++)
      if (A[X][j]*V[(X+2-j)%3][i[1]]+B[X][j]*V[(X+2-j)%3][i[2]]+C[X][j]
	  < 0)
      {
	A[X][j]*=-1; B[X][j]*=-1; C[X][j]*=-1;
      }
  }

  // we'll use the variable out to tell us if we're "outside" of that
  // edge.
  int out[3][3];
  for (X=0; X<3; X++)
  {
    for (int j=0; j<3; j++)
      out[X][j]=(A[X][j]*P[i[1]]+B[X][j]*P[i[2]]+C[X][j] < 0);
  }

  if (out[2][0] && out[1][1])
  {
    *type=1;
    if (pp) (*pp)=a;
    return (sign*(p-a).length());
  }
  if (out[0][0] && out[2][1])
  {
    *type=2;
    if (pp) (*pp)=b;
    return (sign*(p-b).length());
  }
  if (out[1][0] && out[0][1])
  {
    *type=3;
    if (pp) (*pp)=c;
    return (sign*(p-c).length());
  }

  ASSERT(out[0][2] || out[1][2] || out[2][2]);
  double theDist=-100;
  for (X=0; X<3; X++)
    if (out[X][2])
    {
      // take twice the area of the triangle formed between the two
      // end vertices and this point, then divide by the length
      // of the edge...this gives the distance.  The area is twice
      // the length of the cross-product of two edges.
      Vector v1(V[(X+1)%3][0]-p.x(),
		V[(X+1)%3][1]-p.y(),
		V[(X+1)%3][2]-p.z());
      Vector v2(V[(X+1)%3][0]-V[(X+2)%3][0],
		V[(X+1)%3][1]-V[(X+2)%3][1],
		V[(X+1)%3][2]-V[(X+2)%3][2]);
      double dtemp=Sqrt(Cross(v1, v2).length2()/v2.length2());
      if (dtemp>theDist)
      {
	theDist=dtemp;
	*type=X+4;
	if (pp)
	  (*pp)=AffineCombination(Point(V[(X+1)%3][0],
					V[(X+1)%3][1],
					V[(X+1)%3][2]), .5,
				  Point(V[(X+2)%3][0],
					V[(X+2)%3][1],
					V[(X+2)%3][2]), .5);
      }
    }
  return sign*theDist;
}


static void
orderNormal(int i[], const Vector& v)
{
  if (Abs(v.x())>Abs(v.y()))
  {
    if (Abs(v.y())>Abs(v.z()))        // x y z
    {
      i[0]=0; i[1]=1; i[2]=2;
    }
    else if (Abs(v.z())>Abs(v.x()))   // z x y
    {
      i[0]=2; i[1]=0; i[2]=1;
    }
    else
    {                                 // x z y
      i[0]=0; i[1]=2; i[2]=1;
    }
  }
  else
  {
    if (Abs(v.x())>Abs(v.z()))        // y x z
    {
      i[0]=1; i[1]=0; i[2]=2;
    }
    else if (Abs(v.z())>Abs(v.y()))   // z y x
    {
      i[0]=2; i[1]=1; i[2]=0;
    }
    else                              // y z x
    {
      i[0]=1; i[1]=2; i[2]=0;
    }
  }
}


int
TriSurface::intersect(const Point& origin, const Vector& dir,
		      double &d, int &v, int face)
{
  double P[3], t, alpha, beta;
  double u0,u1,u2,v0,v1,v2;
  int i[3];
  double V[3][3];
  int inter;

  const TSElement &e = faces_[face];
  const Point &p1 = points_[e.i1];
  const Point &p2 = points_[e.i2];
  const Point &p3 = points_[e.i3];

  Vector n(Cross(p2 - p1, p3 - p1));
  n.normalize();

  double dis=-Dot(n,p1);
  t=-(dis+Dot(n,origin))/Dot(n,dir);
  if (t<0) return 0;
  if (d!=-1 && t>d) return 0;

  V[0][0]=p1.x();
  V[0][1]=p1.y();
  V[0][2]=p1.z();

  V[1][0]=p2.x();
  V[1][1]=p2.y();
  V[1][2]=p2.z();

  V[2][0]=p3.x();
  V[2][1]=p3.y();
  V[2][2]=p3.z();

  orderNormal(i,n);

  P[0]= origin.x()+dir.x()*t;
  P[1]= origin.y()+dir.y()*t;
  P[2]= origin.z()+dir.z()*t;

  u0=P[i[1]]-V[0][i[1]];
  v0=P[i[2]]-V[0][i[2]];
  inter=0;
  u1=V[1][i[1]]-V[0][i[1]];
  v1=V[1][i[2]]-V[0][i[2]];
  u2=V[2][i[1]]-V[0][i[1]];
  v2=V[2][i[2]]-V[0][i[2]];
  if (u1==0)
  {
    beta=u0/u2;
    if ((beta >= 0.) && (beta <= 1.))
    {
      alpha = (v0-beta*v2)/v1;
      if ((alpha>=0.) && ((alpha+beta)<=1.)) inter=1;
    }
  }
  else
  {
    beta=(v0*u1-u0*v1)/(v2*u1-u2*v1);
    if ((beta >= 0.)&&(beta<=1.))
    {
      alpha=(u0-beta*u2)/u1;
      if ((alpha>=0.) && ((alpha+beta)<=1.)) inter=1;
    }
  }
  if (!inter) return 0;

  if (alpha<beta && alpha<(1-(alpha+beta))) v=e.i1;
  else if (beta<alpha && beta<(1-(alpha+beta))) v=e.i2;
  else v=e.i3;

  d=t;
  return (1);
}


void
TriSurface::remove_triangle(int i)
{
  // if there hasn't been a triangle added since the last one was deleted
  // then we need to start deleting.  Otherwise, we're probably merging
  // contours, so just setting the empty_index is fine.

  // if we have a grid remove this triangle from it
  if (grid)
  {
    grid->remove_triangle(i, points_[faces_[i].i1], points_[faces_[i].i2],
			  points_[faces_[i].i3]);
  }

  // if we don't have an empty index lying around...
  if (empty_index != -1)
  {
    faces_.remove(i);
    faces_.remove(empty_index);
    empty_index=-1;
    // else make this the empty index -- hopefully someone will fill it
  }
  else
  {
    empty_index=i;
  }
}


#define TRISURFACE_VERSION 6

void
TriSurface::io(Piostream& stream)
{
  remove_empty_index();
  int version=stream.begin_class("TriSurface", TRISURFACE_VERSION);
  Surface::io(stream);

  if (version == 5)
  {
    int tmp = 0;
    Pio(stream, tmp);
  }

  if (version >= 4)
  {
    int* flag=(int*)&normType;
    Pio(stream, *flag);
    if (normType != NrmlsNone)
      Pio(stream, normals);
  }
  else if (version >= 3)
  {
    int haveNormals;
    Pio(stream, haveNormals);
    if (haveNormals)
    {
      normType=VertexType;
      Pio(stream, normals);
    }
    else
    {
      normType=NrmlsNone;
    }
  }
  Pio(stream, points_);
  Pio(stream, faces_);
  stream.end_class();
}


SurfTree *
TriSurface::toSurfTree()
{
  SurfTree *st = new SurfTree;

  st->surfI_.resize(1);
  st->faces_ = faces_;
  st->points_ = points_;
  for (int i = 0; i<faces_.size(); i++)
  {
    st->surfI_[0].faces.add(i);
    st->surfI_[0].faceOrient.add(i);
  }
  st->surfI_[0].name = name;
  st->surfI_[0].inner.add(0);
  st->surfI_[0].outer = 0;
  st->surfI_[0].matl = 0;
  st->value_type_ = SurfTree::NodeValuesSome;
  return st;
}


Surface *
TriSurface::clone()
{
  return scinew TriSurface(*this);
}


void
TriSurface::construct_grid()
{
  BBox bbox;
  for(int i=0;i<points_.size();i++)
  {
    bbox.extend(points_[i]);
  }
  Vector d(bbox.diagonal());
  double volume=d.x()*d.y()*d.z();
  // Make it about 10000 elements.
  int ne=10000;
  double spacing=Cbrt(volume/ne);
  int nx=RoundUp(d.x()/spacing);
  int ny=RoundUp(d.y()/spacing);
  int nz=RoundUp(d.z()/spacing);
  construct_grid(nx, ny, nz, bbox.min(), spacing);
}


GeomObj *
TriSurface::get_geom(const ColorMapHandle&)
{
  NOT_FINISHED("TriSurface::get_obj");
  return 0;
}


Point
RandomPoint(Point& p1, Point& p2, Point& p3)
{
  const double alpha = sqrt(drand48());
  const double  beta = drand48();

  return AffineCombination(p1, 1.0 - alpha,
			   p2, alpha - alpha * beta,
			   p3, alpha * beta);
}


void
Pio(Piostream& stream, TSElement &data)
{
  stream.begin_cheap_delim();
  Pio(stream, data.i1);
  Pio(stream, data.i2);
  Pio(stream, data.i3);
  stream.end_cheap_delim();
}


} // End namespace SCIRun

