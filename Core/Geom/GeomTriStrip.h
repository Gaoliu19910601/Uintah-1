
/*
 *  TriStrip.h: Triangle Strip object
 *
 *  Written by:
 *   Steven G. Parker & David Weinstein
 *   Department of Computer Science
 *   University of Utah
 *   April 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#ifndef SCI_Geom_TriStrip_h
#define SCI_Geom_TriStrip_h 1

#include <Core/Geom/GeomVertexPrim.h>

namespace SCIRun {

class SCICORESHARE GeomTriStrip : public GeomVertexPrim {
public:
    GeomTriStrip();
    GeomTriStrip(const GeomTriStrip&);
    virtual ~GeomTriStrip();

    virtual GeomObj* clone();

#ifdef SCI_OPENGL
    virtual void draw(DrawInfoOpenGL*, Material*, double time);
#endif

    int size(void);
    virtual void io(Piostream&);
    static PersistentTypeID type_id;
    virtual bool saveobj(std::ostream&, const clString& format, GeomSave*);
};

class SCICORESHARE GeomTriStripList : public GeomObj {
    int n_strips;
    Array1<float> pts;
    Array1<float> nrmls;
    Array1<int>   strips;
public:
    GeomTriStripList();
    virtual ~GeomTriStripList();

    virtual GeomObj* clone();

    void add(const Point&);
    void add(const Point&, const Vector&);
    
    void end_strip(void); // ends a tri-strip

    Point get_pm1(void);
    Point get_pm2(void);

    void permute(int,int,int);
#ifdef SCI_OPENGL
    virtual void draw(DrawInfoOpenGL*, Material*, double time);
#endif

    virtual void get_bounds(BBox&);

   int size(void);
   int num_since(void);
    virtual void io(Piostream&);
    static PersistentTypeID type_id;
    
    virtual bool saveobj(std::ostream&, const clString& format, GeomSave*);
};

} // End namespace SCIRun


#endif /* SCI_Geom_TriStrip_h */
