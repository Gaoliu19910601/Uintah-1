
/*
 *  TiffReader.cc: TiffReader Reader class
 *  Written by:
 *
 *    Scott Morris
 *    July 1997
 */

#include <Dataflow/Network/Module.h>
#include <Dataflow/Ports/ScalarFieldPort.h>
#include <Core/Datatypes/ScalarField.h>
#include <Core/Datatypes/ScalarFieldRG.h>
#include <Core/Malloc/Allocator.h>
#include <Core/TclInterface/TCLTask.h>
#include <Core/TclInterface/TCLvar.h>

#include <stdio.h>
#include <stdlib.h>
#include "tiffio.h"

namespace SCIRun {


class TiffReader : public Module {
    ScalarFieldOPort* outport;
    TCLstring filename;
    ScalarFieldHandle handle;
    clString old_filename;
    TIFF *tif;
    unsigned long imagelength;
    unsigned char *buf;
    unsigned short *buf16;
    long row;
    int x;
  
public:
    TiffReader(const clString& id);
    virtual ~TiffReader();
    virtual void execute();
};

extern "C" Module* make_TiffReader(const clString& id)
{
    return scinew TiffReader(id);
}

TiffReader::TiffReader(const clString& id)
: Module("TiffReader", id, Source), filename("filename", id, this)
{
    // Create the output data handle and port
    outport=scinew ScalarFieldOPort(this, "Output Data", ScalarFieldIPort::Atomic);
    add_oport(outport);
}

TiffReader::~TiffReader()
{
}

void TiffReader::execute()
{
    clString fn(filename.get());
    if(!handle.get_rep() || fn != old_filename){
	old_filename=fn;

	tif = TIFFOpen(fn(), "r");
	if (!tif) {
	  error("File not found or not a TIFF file..");
	  return;
	}

	int xdim,ydim,zdim=1;
	uint16 bps,spp;
	
	TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &imagelength);
	TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &xdim);
        TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &ydim);
	TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bps);
	TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &spp);
	
	cerr << "--TiffReader--\n";
	cerr << "Reading TIFF file : " << fn << "\n";
	cerr << "Dimensions: " << xdim << " " << ydim << "\n";
	cerr << "spp: " << spp << "  bps: " << bps << "\n";
	
	ScalarFieldRG* sf=new ScalarFieldRG;
	zdim = spp;
	sf->resize(ydim, xdim, zdim);
	sf->compute_bounds();
	Point pmin(0,0,0),pmax(ydim,xdim,zdim);
	sf->set_bounds(pmin,pmax);  // something more reasonable later...

	if ((bps!=8) && (bps!=16)) {
          error("16 or 8 bit files required..");
	  return;
	}
	
	if (bps==16) {
	  buf16 = new unsigned short[TIFFScanlineSize(tif) / 2];

	  for (row = 0; row < imagelength; row++){
	    TIFFReadScanline(tif, buf16, row, 0);
	    for (x=0; x<(xdim*spp); x+=spp)   // hack to force 8bit but still get something 
	      sf->grid((int)(imagelength-1-row),x / spp,0) = buf16[x];
	  }
	  
	} else
	{
	  buf = new unsigned char[TIFFScanlineSize(tif)];
	  cerr << "scanlinesize : " << TIFFScanlineSize(tif) << "\n";
	  if (spp==1) {
	    for (row = 0; row < imagelength; row++){
	      TIFFReadScanline(tif, buf, row, 0);
	      for (x=0; x<(xdim); x++)   
		  sf->grid((int)(imagelength-row-1),x,0) = buf[x];   // x / spp, row
	    }
	  }
	  if (spp==3) {  //Let's try RGB
	    for (row = 0; row < imagelength; row++){
	      TIFFReadScanline(tif, buf, row, 0);
	      for (x=0; x<(xdim*3); x+=3) {
		  sf->grid((int)(imagelength-row-1),x/3,0) = buf[x];
		  sf->grid((int)(imagelength-row-1),x/3,1) = buf[x+1];
		  sf->grid((int)(imagelength-row-1),x/3,2) = buf[x+2];
	      }
	    }
	  }
	}
	TIFFClose(tif);
	cerr << "--ENDTiffReader--\n";
	handle = sf;
    }
    outport->send(handle);
}

} // End namespace SCIRun

