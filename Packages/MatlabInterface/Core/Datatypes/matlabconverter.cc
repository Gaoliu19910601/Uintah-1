
/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2004 Scientific Computing and Imaging Institute,
   University of Utah.

   License for the specific language governing rights and limitations under
   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/

/*
 * FILE: matlabconverter.cc
 * AUTH: Jeroen G Stinstra
 * DATE: 18 MAR 2004
 */

#include "matlabconverter.h"


namespace MatlabIO {

// Currently the property converter only manages strings
// all other data is ignored both on matlabside as well
// as on the property manager side.

/* DESIGN NOTES : */

/* 
 * The BIG Problem in SCIRun is that it is hard to know what
 * kind of object a SCIRun property is and how it should be 
 * translated. For example there are countless array classes and
 * no attempt has been made to standardize the data formats
 * which leads to a huge conversion problem. Hence only the useful
 * objects are translated the rest is just discarded. Until there
 * is a better data management structure within SCIRun, the conversion
 * process is basically a big switch statemet scanning for each possible
 * object structure
 */

/* 
 * With the hope in mind that a better data management system will be
 * in place in the future, all conversion algorithms are grouped in
 * this one object. All the other matlab classes function independent
 * of the main SCIRun structure, it has its own memory and data management
 * making it easier to adapt to future changes. The separation of matlab
 * and SCIRun data management has the advantage that the matlab side can be
 * further enhanced to deal with different future matlab fileformats, without
 * having to comb through the conversion modules. Though there is a little 
 * memory overhead. Especially with the V7 compressed files, more memory
 * is needed to maintain the integrity of the matlab reader. Some changes 
 * in this converter may be needed to enhance performance. Currently the
 * a compressed file will be decompressed and scanned for suitable objects.
 * Upon loading the matrix, the matrix will be decompressed again as after
 * scanning the file, nothing will remain in memory
 */


// Manage converter options

// Set defaults in the constructor
matlabconverter::matlabconverter()
: numericarray_(false), indexbase_(1), datatype_(matlabarray::miSAMEASDATA), disable_transpose_(false)
{
}

void matlabconverter::setdatatype(matlabarray::mitype dataformat)
{
	datatype_ = dataformat;
}

void matlabconverter::setindexbase(long indexbase)
{
	indexbase_ = indexbase;
}

void matlabconverter::converttonumericmatrix()
{
	numericarray_ = true;
}

void matlabconverter::converttostructmatrix()
{
	numericarray_ = false;
}

void matlabconverter::setdisabletranspose(bool dt)
{
	disable_transpose_ = dt;
}



void matlabconverter::mlPropertyTOsciProperty(matlabarray &ma,SCIRun::PropertyManager *handle)
{
	long numfields;
	matlabarray::mlclass mclass;
	matlabarray subarray;
	std::string propname;
	std::string propval;
	matlabarray proparray;

	// properties are stored in field property
	long propindex = ma.getfieldnameindexCI("property");
	
	if (propindex > -1)
	{ // field property exists
	
		proparray = ma.getfield(0,propindex);
		if (proparray.isempty()) return;
		
		numfields = proparray.getnumfields();
	
		for (long p=0; p<numfields; p++)
		{
			subarray = proparray.getfield(0,p);
			mclass = subarray.getclass();
			if (mclass == matlabarray::mlSTRING)
			{   // only string arrays are converted
				propname = proparray.getfieldname(p);
				propval = subarray.getstring();
				handle->set_property(propname,propval,false);
			}
		}
	}
}

void matlabconverter::sciPropertyTOmlProperty(SCIRun::PropertyManager *handle,matlabarray &ma)
{
	long numfields;
	matlabarray proparray;
	std::string propname;
	std::string propvalue;
	matlabarray subarray;
	
	proparray.createstructarray();
	numfields = handle->nproperties();
	
	for (long p=0;p<numfields;p++)
	{
		propname = handle->get_property_name(p);
		if (handle->get_property(propname,propvalue))
		{
			subarray.createstringarray(propvalue);
			proparray.setfield(0,propname,subarray);
		}
	}
	
	ma.setfield(0,"property",proparray);
}

// The next function checks whether
// the program knows how to convert 
// the matlabarray into a scirun matrix

long matlabconverter::sciMatrixCompatible(matlabarray &ma, std::string &infotext, SCIRun::Module *module)
{
	infotext = "";

	matlabarray::mlclass mclass;
	mclass = ma.getclass();
	
	switch (mclass)
	{
		case matlabarray::mlDENSE:
		case matlabarray::mlSPARSE:
		{
			// check whether the data is of a proper format
	
			std::vector<long> dims;	
			dims = ma.getdims();
			if (dims.size() > 2)
			{   
				postmsg(module,std::string("Matrix '" + ma.getname() + "' cannot be translated into a SCIRun Matrix (dimensions > 2)"));
				return(0); // no multidimensional arrays supported yet in the SCIRun Matrix classes
			}
	
			matlabarray::mitype type;
			type = ma.gettype();
	
			infotext = ma.getinfotext(); 
			
			// We expect a double array.		
			// This classification is pure for convenience
			// Any integer array can be dealt with as well
	
			// doubles are most likely to be the data wanted by the users
			if (type == matlabarray::miDOUBLE) return(3);
			// though singles should work as well
			if (type == matlabarray::miSINGLE) return(2);
			// all other numeric formats should be integer types, which
			// can be converted using a simple cast
			return(1);			
		}		
		break;
		case matlabarray::mlSTRUCT:
		{
			long index;
			/* A lot of different names can be used for the data:
			   This has mainly historical reasons: a lot of different
			   names have been used at CVRTI to store data, to be 
			   compatible with all, we allow all of them. Though we
			   suggest the use of : "data" 
			*/
			index = ma.getfieldnameindexCI("data");
			if (index == -1) index = ma.getfieldnameindexCI("potvals");	// in case it is a saved TSDF file
			if (index == -1) index = ma.getfieldnameindexCI("field");
			if (index == -1) index = ma.getfieldnameindexCI("scalarfield");
			if (index == -1) index = ma.getfieldnameindexCI("vectorfield");
			if (index == -1) index = ma.getfieldnameindexCI("tensorfield");
			if (index == -1) 
			{
				postmsg(module,std::string("Matrix '" + ma.getname() + "' cannot be translated into a SCIRun Matrix (cannot find a field with data: create a .data field)"));
				return(0); // incompatible
			}
		
			long numel;
			numel = ma.getnumelements();
			if (numel > 1) 
			{
				postmsg(module,std::string("Matrix '" + ma.getname() + "' cannot be translated into a SCIRun Matrix (the struct matrix is not 1x1: do not define more than one matrix)"));
				return(0); // incompatible	
			}
			matlabarray subarray;
			subarray = ma.getfield(0,index);
			
			// check whether the data is of a proper format
	
			if (subarray.isempty()) 
			{
				postmsg(module,std::string("Matrix '" + ma.getname() + "' cannot be translated into a SCIRun Matrix (no data: matrix is empty)"));
				return(0); // not compatible
			}
			std::vector<long> dims;	
			dims = subarray.getdims();
			if (dims.size() > 2)
			{   
				postmsg(module,std::string("Matrix '" + ma.getname() + "' cannot be translated into a SCIRun Matrix (dimensions > 2)"));
				return(0); // no multidimensional arrays supported yet in the SCIRun Matrix classes
			}
	
			matlabarray::mitype type;
			type = subarray.gettype();
	
			infotext = subarray.getinfotext(ma.getname());
			// We expect a double array.		
			// This classification is pure for convenience
			// Any integer array can be dealt with as well
	
			// doubles are most likely to be the data wanted by the users
			if (type == matlabarray::miDOUBLE) return(3);
			// though singles should work as well
			if (type == matlabarray::miSINGLE) return(2);
			// all other numeric formats should be integer types, which
			// can be converted using a simple cast
			return(1);			
		} 
		break;
	}
	postmsg(module,std::string("Matrix '" + ma.getname() + "' cannot be translated into a SCIRun Matrix (matrix is not struct, dense or sparse array)"));
	return (0);
}
		



void matlabconverter::mlArrayTOsciMatrix(matlabarray &ma,SCIRun::MatrixHandle &handle, SCIRun::Module *module)
{
	matlabarray::mlclass mclass = ma.getclass();
	
	switch(mclass)
	{
		case matlabarray::mlDENSE:
			{   // new environment so I can create new variables
			
				if (disable_transpose_)
				{
					SCIRun::DenseMatrix* dmptr;							// pointer to a new dense matrix
						
					int m = static_cast<int>(ma.getm());
					int n = static_cast<int>(ma.getn());
					
					dmptr = new SCIRun::DenseMatrix(n,m);   // create dense matrix
						// copy and cast elements:
						// getnumericarray is a templated function that casts the data to the supplied pointer
						// type. It needs the dimensions of the memory block (in elements) to make sure
						// everything is still OK. 
					ma.getnumericarray(dmptr->getData(),(dmptr->nrows())*(dmptr->ncols()));  
					
					handle = static_cast<SCIRun::Matrix *>(dmptr); // cast it to a general matrix pointer
				}
				else
				{
				
					SCIRun::DenseMatrix* dmptr;							// pointer to a new dense matrix
					
					int m = static_cast<int>(ma.getm());
					int n = static_cast<int>(ma.getn());
					
					SCIRun::DenseMatrix  dm(n,m);   // create dense matrix
						// copy and cast elements:
						// getnumericarray is a templated function that casts the data to the supplied pointer
						// type. It needs the dimensions of the memory block (in elements) to make sure
						// everything is still OK. 
					ma.getnumericarray(dm.getData(),(dm.nrows())*(dm.ncols()));  
					
					// There is no transpose function to operate on the same memory block
					// Hence, it is a little memory inefficient.
					
					dmptr = dm.transpose();	// SCIRun has a C++-style matrix and matlab a FORTRAN-style matrix
					handle = static_cast<SCIRun::Matrix *>(dmptr); // cast it to a general matrix pointer
				}
			}
			break;
			
		case matlabarray::mlSPARSE:
			{
				if (disable_transpose_)
				{
					SCIRun::SparseRowMatrix* smptr;
					
					// Since the SparseRowMatrix does not allocate memory but on the 
					// otherhand frees it in the destructor. The memory needs to be
					// allocated outside of the object and then linked to the object
					// to have it freed lateron by the object.
					
					// in the matlabio classes they are defined as long, hence
					// the casting operators
					int nnz = static_cast<int>(ma.getnnz());
					int m = static_cast<int>(ma.getm());
					int n = static_cast<int>(ma.getn()); 
					
					double *values = scinew double[nnz];
					int *rows   = scinew int[nnz];
					int *cols   = scinew int[n+1];
					
					ma.getnumericarray(values,nnz);
					ma.getrowsarray(rows,nnz); // automatically casts longs to ints
					ma.getcolsarray(cols,(n+1));
					
					smptr = new SCIRun::SparseRowMatrix(n,m,cols,rows,nnz,values);
					
					handle = static_cast<SCIRun::Matrix *>(smptr); // cast it to a general matrix pointer
				}
				else
				{
					SCIRun::SparseRowMatrix* smptr;
					
					// Since the SparseRowMatrix does not allocate memory but on the 
					// otherhand frees it in the destructor. The memory needs to be
					// allocated outside of the object and then linked to the object
					// to have it freed lateron by the object.
					
					// in the matlabio classes they are defined as long, hence
					// the casting operators
					int nnz = static_cast<int>(ma.getnnz());
					int m = static_cast<int>(ma.getm());
					int n = static_cast<int>(ma.getn()); 
					
					double *values = scinew double[nnz];
					int *rows   = scinew int[nnz];
					int *cols   = scinew int[n+1];
					
					ma.getnumericarray(values,nnz);
					ma.getrowsarray(rows,nnz); // automatically casts longs to ints
					ma.getcolsarray(cols,(n+1));
					
					SCIRun::SparseRowMatrix  sm(n,m,cols,rows,nnz,values);
					
					smptr = sm.transpose(); // SCIRun uses Row sparse matrices and matlab Column sparse matrices
					handle = static_cast<SCIRun::Matrix *>(smptr); // cast it to a general matrix pointer
				}
			}
			break;
			
		case matlabarray::mlSTRUCT:
			{
				// A compatible struct has the following fields
				// - data:     submatrix with the actual matrix in it
				// - property: optional extra struct array with key
				//             /value pairs for the property manager
				
				long dataindex, propertyindex;
				dataindex = ma.getfieldnameindexCI("data");
				if (dataindex == -1) dataindex = ma.getfieldnameindex("potvals");
				if (dataindex == -1) dataindex = ma.getfieldnameindexCI("field");
				if (dataindex == -1) dataindex = ma.getfieldnameindexCI("scalarfield");
				if (dataindex == -1) dataindex = ma.getfieldnameindexCI("vectorfield");
				if (dataindex == -1) dataindex = ma.getfieldnameindexCI("tensorfield");

				propertyindex = ma.getfieldnameindexCI("property");
				
				if (dataindex == -1)
				{
					throw matlabconverter_error();
				}
				
				matlabarray subarray;
				subarray = ma.getfield(0,dataindex);
				mlArrayTOsciMatrix(subarray,handle,module);
				
				if (propertyindex != -1)
				{
					mlPropertyTOsciProperty(ma,static_cast<SCIRun::PropertyManager *>(handle.get_rep()));
				}
				
			}
			break;
		default:
			{   // The program should not get here
				throw matlabconverter_error();
			}
	}
}


void matlabconverter::sciMatrixTOmlMatrix(SCIRun::MatrixHandle &scimat,matlabarray &mlmat)
{
	// Get the format for exporting data
	matlabarray::mitype dataformat = datatype_;

	// SCIRun matrices are always (up till now) doubles
	if (dataformat == matlabarray::miSAMEASDATA) dataformat = matlabarray::miDOUBLE;
	
	if (scimat->is_dense())
	{
		SCIRun::DenseMatrix* dmatrix;
		SCIRun::DenseMatrix* tmatrix;
		dmatrix = scimat->as_dense();
		tmatrix = dmatrix->transpose();
		
		std::vector<long> dims(2);
		dims[1] = tmatrix->nrows();
		dims[0] = tmatrix->ncols();
		mlmat.createdensearray(dims,dataformat);
		mlmat.setnumericarray(tmatrix->getData(),mlmat.getnumelements());
	}
	if (scimat->is_column())
	{
		SCIRun::ColumnMatrix* cmatrix;
		std::vector<long> dims(2);
		cmatrix = scimat->as_column();
		dims[0] = cmatrix->nrows();
		dims[1] = cmatrix->ncols();
		mlmat.createdensearray(dims,dataformat);
		mlmat.setnumericarray(cmatrix->get_data(),mlmat.getnumelements());
	}
	if (scimat->is_sparse())
	{
		SCIRun::SparseRowMatrix* smatrix;
		SCIRun::SparseRowMatrix* tmatrix;
		smatrix = scimat->as_sparse();
		tmatrix = smatrix->transpose();
		
		std::vector<long> dims(2);
		dims[1] = tmatrix->nrows();
		dims[0] = tmatrix->ncols();
		mlmat.createsparsearray(dims,dataformat);
		
		mlmat.setnumericarray(tmatrix->get_val(),tmatrix->get_nnz());
		mlmat.setrowsarray(tmatrix->get_col(),tmatrix->get_nnz());
		mlmat.setcolsarray(tmatrix->get_row(),tmatrix->nrows()+1);
	}
}


void matlabconverter::sciMatrixTOmlArray(SCIRun::MatrixHandle &scimat,matlabarray &mlmat, SCIRun::Module *module)
{
	if (numericarray_ == true)
	{
		sciMatrixTOmlMatrix(scimat,mlmat);
	}
	else
	{
		matlabarray dataarray;
		mlmat.createstructarray();
		sciMatrixTOmlMatrix(scimat,dataarray);
		mlmat.setfield(0,"data",dataarray);
		sciPropertyTOmlProperty(static_cast<SCIRun::PropertyManager *>(scimat.get_rep()),mlmat);
	}
}

// Support function to check whether the names supplied are within the 
// rules that matlab allows. Otherwise we could save the file, but matlab 
// would complain it could not read the file.

bool matlabconverter::isvalidmatrixname(std::string name)
{
	const std::string validchar("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_");
	const std::string validstartchar("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");


	bool valid = true;
	bool foundchar = false;

	for (long p=0; p < name.size(); p++)
	{
		if (p == 0)
		{   
			// A variable name is not allowed to start with a number
			foundchar = false;
			for (long q = 0; q < validstartchar.size(); q++) 
			{
				if (name[p] == validstartchar[q]) { foundchar = true; break; }
			}
		}
		else
		{
			foundchar = false;
			for (long q = 0; q < validchar.size(); q++) 
			{
				if (name[p] == validchar[q]) { foundchar = true; break; }
			}
		}
		if (foundchar == false) { valid = false; break; }
	}
	return(valid);
}


#ifdef HAVE_TEEM_PACKAGE

// Test the compatibility of the matlabarray witha nrrd structure
// in case it is compatible return a positive value and write
// out an infostring with a summary of the contents of the matrix

long matlabconverter::sciNrrdDataCompatible(matlabarray &mlarray, std::string &infostring, SCIRun::Module *module)
{
	matlabarray::mlclass mclass;
	mclass = mlarray.getclass();
	
	// parse matrices are dealt with in a separate 
	// module as the the data needs to be divided over
	// three separate Nrrds

	infostring = "";
	
	if ((mclass == matlabarray::mlSTRUCT)||(mclass == matlabarray::mlOBJECT))
	{
		long fieldnameindex;
		matlabarray subarray;
		
		fieldnameindex = mlarray.getfieldnameindexCI("data");
		if (fieldnameindex == -1) fieldnameindex = mlarray.getfieldnameindexCI("potvals");
		if (fieldnameindex == -1) fieldnameindex = mlarray.getfieldnameindexCI("field");
		if (fieldnameindex == -1) fieldnameindex = mlarray.getfieldnameindexCI("scalarfield");
		if (fieldnameindex == -1) fieldnameindex = mlarray.getfieldnameindexCI("vectorfield");
		if (fieldnameindex == -1) fieldnameindex = mlarray.getfieldnameindexCI("tensorfield");

		if (fieldnameindex == -1) 
		{
			postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Nrrd Object (cannot find field with data: create a .data field)"));
			return(0);
		}
		
		subarray = mlarray.getfield(0,fieldnameindex);
	
		if (subarray.isempty()) 
		{
			postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Nrrd Object (field with data is empty)"));
			return(0);
		}
				
		infostring = subarray.getinfotext(mlarray.getname());
		matlabarray::mitype type;
		type = subarray.gettype();
	
		matlabarray::mlclass mclass;
		mclass = subarray.getclass();
		
		if ((mclass != matlabarray::mlDENSE)&&(mclass != matlabarray::mlSPARSE)) 
		{
			postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Nrrd Object (matrix is not dense or structured array)"));
			return(0);
		}
		
		// We expect a double array.
		// This classification is pure for convenience
		// Any integer array can be dealt with as well
	
		// doubles are most likely to be the data wanted by the users
		if (type == matlabarray::miDOUBLE) return(3);
		// though singles should work as well
		if (type == matlabarray::miSINGLE) return(2);
		// all other numeric formats should be integer types, which
		// can be converted using a simple cast
		return(1);	
	}
	
	if ((mclass != matlabarray::mlDENSE))
	{
		postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Nrrd Object (the data is not dense matrix or a structured array)"));
		return(0); // incompatible for the moment, no converter written for this type yet
	}

	// Need to enhance this code to squeeze out dimensions of size one

	// Nrrds can be multi dimensional hence no limit on the dimensions is
	// needed
	
	if (mlarray.isempty()) 
	{
		postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Nrrd Object (matrix is empty)"));
		return(0);
	}
	
	matlabarray::mitype type;
	type = mlarray.gettype();
	
	infostring = mlarray.getinfotext();
	
	// We expect a double array.
	// This classification is pure for convenience
	// Any integer array can be dealt with as well
	
	// doubles are most likely to be the data wanted by the users
	if (type == matlabarray::miDOUBLE) return(3);
	// though singles should work as well
	if (type == matlabarray::miSINGLE) return(2);
	// all other numeric formats should be integer types, which
	// can be converted using a simple cast
	return(1);	
}


unsigned int matlabconverter::convertmitype(matlabarray::mitype type)
{
	switch (type)
	{
		case matlabarray::miINT8:   return(nrrdTypeChar);
		case matlabarray::miUINT8:  return(nrrdTypeUChar);
		case matlabarray::miINT16:  return(nrrdTypeShort);
		case matlabarray::miUINT16: return(nrrdTypeUShort);
		case matlabarray::miINT32:  return(nrrdTypeInt);
		case matlabarray::miUINT32: return(nrrdTypeUInt);
		case matlabarray::miINT64:  return(nrrdTypeLLong);
		case matlabarray::miUINT64: return(nrrdTypeULLong);
		case matlabarray::miSINGLE: return(nrrdTypeFloat);
		case matlabarray::miDOUBLE: return(nrrdTypeDouble);
		default: return(nrrdTypeUnknown);
	}
}


// This function converts nrrds into matlab matrices
// only the datais being transformed into a matlab array

matlabarray::mitype matlabconverter::convertnrrdtype(int type)
{
	switch (type)
	{
		case nrrdTypeChar : return(matlabarray::miINT8);
		case nrrdTypeUChar : return(matlabarray::miUINT8);
		case nrrdTypeShort : return(matlabarray::miINT16);
		case nrrdTypeUShort : return(matlabarray::miUINT16);		
		case nrrdTypeInt : return(matlabarray::miINT32);
		case nrrdTypeUInt : return(matlabarray::miUINT32);
		case nrrdTypeLLong : return(matlabarray::miINT64);
		case nrrdTypeULLong : return(matlabarray::miUINT64);
		case nrrdTypeFloat : return(matlabarray::miSINGLE);
		case nrrdTypeDouble : return(matlabarray::miDOUBLE);
		default: return(matlabarray::miUNKNOWN);
	}
}

void matlabconverter::mlArrayTOsciNrrdData(matlabarray &mlarray,SCITeem::NrrdDataHandle &scinrrd, SCIRun::Module *module)
{
	// Depending on the matlabclass there are several converters
	// for converting the data from matlab into a SCIRun Nrrd object
	
	matlabarray::mlclass mclass;
	mclass = mlarray.getclass();
	
	// In case no converter is found return 0 
	// Hence initialise scinrrd as a NULL ptr
	
	scinrrd = 0; 
	
	// Pointer to a new SCIRun Nrrd Data object
	SCITeem::NrrdData* nrrddataptr = 0;
					
	switch(mclass)
	{
		case matlabarray::mlDENSE:
			{   // new environment so I can create new variables

				try
				{
					// new nrrd data handle
					nrrddataptr = new SCITeem::NrrdData(true); // nrrd is owned by the object
					nrrddataptr->nrrd = nrrdNew();
				
					// obtain the type of the new nrrd
					// we want to keep the nrrd type the same
					// as the original matlab type
					
					unsigned int nrrdtype = convertmitype(mlarray.gettype());
				
					// obtain the dimensions of the new nrrd
					int nrrddims[NRRD_DIM_MAX];
					std::vector<long> dims = mlarray.getdims();
					long nrrddim = dims.size();
					for (long p=0;p<nrrddim;p++) nrrddims[p] = dims[p];
				
					nrrdAlloc_nva(nrrddataptr->nrrd,nrrdtype,nrrddim,nrrddims);
					
					switch (nrrdtype)
					{
						case nrrdTypeChar:
							mlarray.getnumericarray(static_cast<signed char *>(nrrddataptr->nrrd->data),nrrdElementNumber(nrrddataptr->nrrd));
							break;
						case nrrdTypeUChar:
							mlarray.getnumericarray(static_cast<unsigned char *>(nrrddataptr->nrrd->data),nrrdElementNumber(nrrddataptr->nrrd));
							break;
						case nrrdTypeShort:
							mlarray.getnumericarray(static_cast<signed short *>(nrrddataptr->nrrd->data),nrrdElementNumber(nrrddataptr->nrrd));
							break;
						case nrrdTypeUShort:
							mlarray.getnumericarray(static_cast<unsigned short *>(nrrddataptr->nrrd->data),nrrdElementNumber(nrrddataptr->nrrd));
							break;
						case nrrdTypeInt:
							mlarray.getnumericarray(static_cast<signed long *>(nrrddataptr->nrrd->data),nrrdElementNumber(nrrddataptr->nrrd));
							break;
						case nrrdTypeUInt:
							mlarray.getnumericarray(static_cast<unsigned long *>(nrrddataptr->nrrd->data),nrrdElementNumber(nrrddataptr->nrrd));
							break;
#ifdef JGS_MATLABIO_USE_64INTS
						case nrrdTypeLLong:
							mlarray.getnumericarray(static_cast<int64 *>(nrrddataptr->nrrd->data),nrrdElementNumber(nrrddataptr->nrrd));
							break;
						case nrrdTypeULLong:
							mlarray.getnumericarray(static_cast<uint64 *>(nrrddataptr->nrrd->data),nrrdElementNumber(nrrddataptr->nrrd));
							break;
#endif
						case nrrdTypeFloat:
							mlarray.getnumericarray(static_cast<float *>(nrrddataptr->nrrd->data),nrrdElementNumber(nrrddataptr->nrrd));
							break;
						case nrrdTypeDouble:
							mlarray.getnumericarray(static_cast<double *>(nrrddataptr->nrrd->data),nrrdElementNumber(nrrddataptr->nrrd));
							break;
						default:
							throw matlabconverter_error();
							break;
					}
	
					// set some info on the axis as not all SCIRun modules check whether there is any
					// data and may crash if there is no label
					
					// Nrrd lib is C and needs a list with pointers to C-style strings
					// The following C++ code does this without the need of the need for
					// explicit dynamic memory allocation.
					
					std::vector<std::string> labels;
					labels.resize(nrrddim);
					const char *labelptr[NRRD_DIM_MAX];
					
					for (long p=0;p<nrrddim;p++)
					{
						std::ostringstream oss; 
						oss << "dimension " << (p+1);
						labels[p] = oss.str();
						labelptr[p] = labels[p].c_str();
						// labelptr contains ptrs into labels and
						// the memory it points to will be destroyed with
						// the labels object. The const cast is needed as
						// nrrd
					}
									
					nrrdAxisInfoSet_nva(nrrddataptr->nrrd,nrrdAxisInfoLabel,labelptr);
																													
					scinrrd = nrrddataptr;
				}
				catch (...)
				{
					// in case something went wrong
					// release the datablock attached to
					// the nrrdhandle
					
					delete nrrddataptr;
					scinrrd = 0;
					
					throw;
				}
			}
			
			if (scinrrd != 0)
			{
				std::string str = mlarray.getname();
				scinrrd->set_filename(str);
			}
			
			break;
			// END CONVERSION OF MATLAB MATRIX
			
		case matlabarray::mlSTRUCT:
		case matlabarray::mlOBJECT:
			{
				long dataindex;
				dataindex = mlarray.getfieldnameindexCI("data");
				if (dataindex == -1) dataindex = mlarray.getfieldnameindexCI("potvals");
				if (dataindex == -1) dataindex = mlarray.getfieldnameindexCI("field");
				if (dataindex == -1) dataindex = mlarray.getfieldnameindexCI("scalarfield");
				if (dataindex == -1) dataindex = mlarray.getfieldnameindexCI("vectorfield");
				if (dataindex == -1) dataindex = mlarray.getfieldnameindexCI("tensorfield");

			
				// We need data to create an object
				// if no data field is found return
				// an error
				
				if (dataindex == -1)
				{
					throw matlabconverter_error();
				}
			
				matlabarray subarray;
				subarray = mlarray.getfieldCI(0,"data");
				
				matlabarray::mlclass subclass;
				subclass = subarray.getclass();
				
				if (subclass != matlabarray::mlDENSE)
				{
					throw matlabconverter_error();
					return;
				}
				
				mlArrayTOsciNrrdData(subarray,scinrrd,module);
				
				if (scinrrd == 0)
				{
					throw matlabconverter_error();
					return;
				}
				
				// Add axes properties if they are specified
				
				long axisindex;
				axisindex = mlarray.getfieldnameindexCI("axis");
				
				if (axisindex != -1)
				{
					matlabarray::mlclass axisarrayclass;
					matlabarray axisarray;
					long		numaxis;
					long		fnindex;
					matlabarray farray;
					
					axisarray = mlarray.getfieldCI(0,"axis");
					
					if (!axisarray.isempty())
					{
						numaxis = axisarray.getm();
						axisarrayclass =axisarray.getclass();
					
						if ((axisarrayclass != matlabarray::mlSTRUCT)&&(axisarrayclass != matlabarray::mlOBJECT))
						{
							throw matlabconverter_error();
							return;
						}
				
				
						// insert labels into nnrd
						// labels can be defined in axis(n).label
				
						fnindex = axisarray.getfieldnameindexCI("label");
						
						if (fnindex != -1)
						{
							std::vector<std::string> labels(NRRD_DIM_MAX);
							const char *clabels[NRRD_DIM_MAX];
					
							// Set some default values in case the
							// label is not defined by the matlabarray
						
							for (long p=0;p<NRRD_DIM_MAX;p++)
							{
								std::ostringstream oss; 
								oss << "dimension " << (p+1);
								labels[p] = oss.str();
								clabels[p] = labels[p].c_str();
							}
						
							// In case the label is set in the matlabarray
							// add this label to the clabels array
						
							for (long p=0;p<numaxis;p++)
							{
								farray = axisarray.getfield(p,fnindex);
								if (farray.getclass() == matlabarray::mlSTRING)
								{
									labels[p] = farray.getstring();
									clabels[p] = labels[p].c_str();
								} 
							}
						
							nrrdAxisInfoSet_nva(scinrrd->nrrd,nrrdAxisInfoLabel,clabels);
						}

						
						// insert unit names
						// into nrrd object
						
						fnindex = axisarray.getfieldnameindexCI("unit");
						if (fnindex != -1)
						{
							std::vector<std::string> units(NRRD_DIM_MAX);
							const char *cunits[NRRD_DIM_MAX];
					
							// Set some default values in case the
							// unit is not defined by the matlabarray
						
							for (long p=0;p<NRRD_DIM_MAX;p++)
							{
								units[p] = "unknown";
								cunits[p] = units[p].c_str();
							}
						
							// In case the unit is set in the matlabarray
							// add this unit to the clabels array
						
							for (long p=0;p<numaxis;p++)
							{
								farray = axisarray.getfield(p,fnindex);
								if (farray.getclass() == matlabarray::mlSTRING)
								{
									units[p] = farray.getstring();
									cunits[p] = units[p].c_str();
								}
							}
						
							nrrdAxisInfoSet_nva(scinrrd->nrrd,nrrdAxisInfoUnit,cunits);
						}
					
						// insert spacing information
				
						fnindex = axisarray.getfieldnameindexCI("spacing");
						if (fnindex != -1)
						{
							double spacing[NRRD_DIM_MAX];
							std::vector<double> data(1);
						
							// Set some default values in case the
							// spacing is not defined by the matlabarray
						
							for (long p=0;p<NRRD_DIM_MAX;p++)
							{
								spacing[p] = AIR_NAN;
							}
						
							for (long p=0;p<numaxis;p++)
							{
								farray = axisarray.getfield(p,fnindex);
								if ((farray.getclass() == matlabarray::mlDENSE)&&(farray.getnumelements() > 0))
								{
									farray.getnumericarray(data);
									spacing[p] = data[0];
								}
							}
						
							nrrdAxisInfoSet_nva(scinrrd->nrrd,nrrdAxisInfoSpacing,spacing);
						}

						// insert minimum information
					
						fnindex = axisarray.getfieldnameindexCI("min");
						if (fnindex != -1)
						{
							double mindata[NRRD_DIM_MAX];
							std::vector<double> data;
						
							// Set some default values in case the
							// minimum is not defined by the matlabarray
						
							for (long p=0;p<NRRD_DIM_MAX;p++)
							{
								mindata[p] = AIR_NAN;
							}
						
							for (long p=0;p<numaxis;p++)
							{
								farray = axisarray.getfield(p,fnindex);
								if ((farray.getclass() == matlabarray::mlDENSE)&&(farray.getnumelements() > 0))
								{
									farray.getnumericarray(data);
									mindata[p] = data[0];
								}
							}
						
							nrrdAxisInfoSet_nva(scinrrd->nrrd,nrrdAxisInfoMin,mindata);
						}
					
					
						// insert maximum information
					
						fnindex = axisarray.getfieldnameindexCI("max");
						if (fnindex != -1)
						{
							double maxdata[NRRD_DIM_MAX];
							std::vector<double> data;
						
							// Set some default values in case the
							// maximum is not defined by the matlabarray
						
							for (long p=0;p<NRRD_DIM_MAX;p++)
							{
								maxdata[p] = AIR_NAN;
							}
						
							for (long p=0;p<numaxis;p++)
							{
								farray = axisarray.getfield(p,fnindex);
								if ((farray.getclass() == matlabarray::mlDENSE)&&(farray.getnumelements() > 0))
								{
									farray.getnumericarray(data);
									maxdata[p] = data[0];
								}
							}
						
							nrrdAxisInfoSet_nva(scinrrd->nrrd,nrrdAxisInfoMax,maxdata);
						}
				
						// insert centering information
					
						fnindex = axisarray.getfieldnameindexCI("center");
						if (fnindex != -1)
						{
							long centerdata[NRRD_DIM_MAX];
							std::vector<long> data;
						
							// Set some default values in case the
							// maximum is not defined by the matlabarray
						
							for (long p=0;p<NRRD_DIM_MAX;p++)
							{
								centerdata[p] = 0;
							}
						
							for (long p=0;p<numaxis;p++)
							{
								farray = axisarray.getfield(p,fnindex);
								if ((farray.getclass() == matlabarray::mlDENSE)&&(farray.getnumelements() > 0))
								{
									farray.getnumericarray(data);
									centerdata[p] = data[0];
								}
							}
						
							nrrdAxisInfoSet_nva(scinrrd->nrrd,nrrdAxisInfoCenter,centerdata);
						}
					}
				}
	
				
				long propertyindex;
				propertyindex = mlarray.getfieldnameindexCI("property");
				
				if (propertyindex != -1)
				{
					mlPropertyTOsciProperty(mlarray,static_cast<SCIRun::PropertyManager *>(scinrrd.get_rep()));
				}

				if (mlarray.isfieldCI("name"))
				{
					if (scinrrd != 0)
					{	
						matlabarray matname;
						matname = mlarray.getfieldCI(0,"name");
						std::string str = matname.getstring();
						if (matname.isstring())	scinrrd->set_filename(str);
					}
		
				}
				else
				{
					if (scinrrd != 0)
					{
						std::string str = mlarray.getname();
						scinrrd->set_filename(str);
					}
				}
			
			}	
			
			break;
			
		default:
			{   // The program should not get here
				throw matlabconverter_error(); 
			}
	}
}


void matlabconverter::sciNrrdDataTOmlMatrix(SCITeem::NrrdDataHandle &scinrrd, matlabarray &mlarray)
{

	Nrrd	    *nrrdptr;
	matlabarray::mitype dataformat = datatype_;

	mlarray.clear();

	// first determine the size of a nrrd
	std::vector<long> dims;
	
	nrrdptr = scinrrd->nrrd;

	// check if there is any data 
	if (nrrdptr->dim == 0) return;
	
	dims.resize(nrrdptr->dim);
	
	long totsize = 1; // this one is used as an internal check and is handy to have
	for (long p=0; p<(nrrdptr->dim);p++)
	{
		dims[p] = nrrdptr->axis[p].size;
		totsize *= dims[p];
	}
	
	// if there is no data leave the object empty
	if (totsize == 0) return;
	
	// we now have to determine the type of the matlab array
	// It can be either the same as in the nrrd array or casted
	// to a more appropriate type
	// type will store the new matlab array type
	
	if(dataformat == matlabarray::miSAMEASDATA) dataformat = convertnrrdtype(nrrdptr->type);
	
	// create the matlab array structure
    mlarray.createdensearray(dims,dataformat);
	
	// having the correct pointer type will automatically invoke
	// the proper template function for casting and storing the data
	
	switch (nrrdptr->type)
	{
		case nrrdTypeDouble  : mlarray.setnumericarray(static_cast<double *>(nrrdptr->data),totsize,dataformat); break;
		case nrrdTypeFloat   : mlarray.setnumericarray(static_cast<float *>(nrrdptr->data),totsize,dataformat); break;
		case nrrdTypeChar    : mlarray.setnumericarray(static_cast<signed char *>(nrrdptr->data),totsize,dataformat); break;
		case nrrdTypeUChar   : mlarray.setnumericarray(static_cast<unsigned char *>(nrrdptr->data),totsize,dataformat); break;
		case nrrdTypeShort   : mlarray.setnumericarray(static_cast<signed short *>(nrrdptr->data),totsize,dataformat); break;
		case nrrdTypeUShort  : mlarray.setnumericarray(static_cast<unsigned short *>(nrrdptr->data),totsize,dataformat); break;
		case nrrdTypeInt	 : mlarray.setnumericarray(static_cast<signed long *>(nrrdptr->data),totsize,dataformat); break;
		case nrrdTypeUInt    : mlarray.setnumericarray(static_cast<unsigned long *>(nrrdptr->data),totsize,dataformat); break;
		case nrrdTypeLLong   : mlarray.setnumericarray(static_cast<int64 *>(nrrdptr->data),totsize,dataformat); break;
		case nrrdTypeULLong  : mlarray.setnumericarray(static_cast<uint64 *>(nrrdptr->data),totsize,dataformat); break;	
	}
}


void matlabconverter::sciNrrdDataTOmlArray(SCITeem::NrrdDataHandle &scinrrd, matlabarray &mlarray, SCIRun::Module *module)
{

	if (numericarray_ == true)
	{
		sciNrrdDataTOmlMatrix(scinrrd,mlarray);
		return;
	}

	matlabarray matrix;
	sciNrrdDataTOmlMatrix(scinrrd,matrix);
		
	mlarray.createstructarray();
	mlarray.setfield(0,"data",matrix);
		
	// Set the properies of the axis
	std::vector<std::string> axisfieldnames(7);
	axisfieldnames[0] = "size";
	axisfieldnames[1] = "spacing";
	axisfieldnames[2] = "min";
	axisfieldnames[3] = "max";
	axisfieldnames[4] = "center";
	axisfieldnames[5] = "label";
	axisfieldnames[6] = "unit";
	
	Nrrd	*nrrdptr;
	nrrdptr = scinrrd->nrrd;
				
	matlabarray axisma;
	std::vector<long> dims(2);
	dims[0] = nrrdptr->dim;
	dims[1] = 1;
	axisma.createstructarray(dims,axisfieldnames);
		
		
	for (long p=0; p<nrrdptr->dim; p++ )
	{
		matlabarray sizema;
		matlabarray spacingma;
		matlabarray minma;
		matlabarray maxma;
		matlabarray centerma;
		matlabarray labelma;
		matlabarray unitma;
			
		sizema.createdoublescalar(static_cast<double>(nrrdptr->axis[p].size));
		axisma.setfield(p,0,sizema);
		spacingma.createdoublescalar(nrrdptr->axis[p].spacing);
		axisma.setfield(p,1,spacingma);
		minma.createdoublescalar(nrrdptr->axis[p].min);
		axisma.setfield(p,2,minma);
		maxma.createdoublescalar(nrrdptr->axis[p].max);
		axisma.setfield(p,3,maxma);
		centerma.createdoublescalar(static_cast<double>(nrrdptr->axis[p].center));
		axisma.setfield(p,4,centerma);
		if (nrrdptr->axis[p].label == 0)
		{
			labelma.createstringarray();
		}
		else
		{
			labelma.createstringarray(nrrdptr->axis[p].label);
		}
		axisma.setfield(p,5,labelma);
		if (nrrdptr->axis[p].unit == 0)
		{
			unitma.createstringarray();
		}
		else
		{
			unitma.createstringarray(nrrdptr->axis[p].unit);
		}
		axisma.setfield(p,6,unitma);
	}
	
	mlarray.setfield(0,"axis",axisma);
	sciPropertyTOmlProperty(static_cast<SCIRun::PropertyManager *>(scinrrd.get_rep()),mlarray);
}

#endif



// Routine for discovering which kind of mesh is being supplied
// It reads the matlabarray and looks for certain fields to see whether
// it is convertible into a SCIRun mesh.
// Currently there are dozens of mesh and field types being used in
// SCIRun, each having a similar but different interface. This function
// only supports the most used ones:
//   PointCloudMesh
//   CurveMesh
//   TriSurfMesh
//   QuadSurfMesh
//   TetVolMesh
//   HexVolMesh
//   PrismVolMesh
//   StructCurveMesh
//   StructQuadSurfMesh
//   StructHexVolMesh
//   Scanline
//   Image
//   LatVol
//   any suggestions for other types that need support ??

long matlabconverter::sciFieldCompatible(matlabarray mlarray,std::string &infostring, SCIRun::Module *module)
{

	// If it is regular matrix translate it to a image or a latvol
	// The following section of code rewrites the matlab matrix into a
	// structure and then the normal routine picks up the field and translates it
	// properly.

	if (mlarray.isdense())
	{
		long numdims = mlarray.getnumdims();
		if ((numdims >0)&&(numdims < 4))
		{
			matlabarray ml;
			matlabarray dimsarray;
			std::vector<long> d = mlarray.getdims();
			if ((d[0]==1)||(d[1]==1))
			{
				if (d[0]==1) d[0] = d[1];
				long temp = d[0];
				d.resize(1);
				d[0] = temp;
			}			
			dimsarray.createlongvector(d);
			ml.createstructarray();
			ml.setfield(0,"dims",dimsarray);
			ml.setfield(0,"field",mlarray);
			ml.setname(mlarray.getname());			
			mlarray = ml;
		}
	}

	if (!mlarray.isstruct())
	{ 
		postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (the matrix is not structured nor dense (2D or 3D))"));
		return(0); // not compatible if it is not structured data
	}
	fieldstruct fs = analyzefieldstruct(mlarray); // read the main structure of the object
	
	// Check what kind of field has been supplied
	
	// The fieldtype string, contains the type of field, so it can be listed in the
	// infostring. Just to supply the user with a little bit more data.
	std::string fieldtype;
	fieldtype = "NO FIELD DATA";
	
	// The next step will incorporate a new way of dealing with fields
	// Basically we alter the way fields are processed:
	// instead of specifying vectorfield as a field, it is now allowed and recommended to use
	// two fields: .field describing the data and .fieldtype for the type of data
	// The next piece of code translates the new notation back to the old one.
	
	if (!(fs.fieldtype.isempty()))
	{
		if ((fs.fieldtype.compareCI("vector"))&&(fs.vectorfield.isempty())&&(fs.scalarfield.isdense()))
		{   
			fs.vectorfield = fs.scalarfield;
			fs.scalarfield.clear();
		}

		if ((fs.fieldtype.compareCI("tensor"))&&(fs.tensorfield.isempty())&&(fs.scalarfield.isdense()))
		{   
			fs.tensorfield = fs.scalarfield;
			fs.scalarfield.clear();
		}
	}
	
	if (fs.scalarfield.isdense()) fieldtype = "SCALAR FIELD";
	if (fs.vectorfield.isdense()) fieldtype = "VECTOR FIELD";
	if (fs.tensorfield.isdense()) fieldtype = "TENSOR FIELD";
	
	// Field data has been analysed, now analyse the connectivity data
	// Connectivity data needs to be or edge data, or face data, or cell data,
	// or no data in which case it is a point cloud.
	
	
	// Tests for images/latvols/scanlines
	// vvvvvvvvvvvvvvvvvvvvvvvvvv
	
	// Test whether transform is a 4 by 4 matrix
	if (fs.transform.isdense())
	{
		if (fs.transform.getnumdims() != 2) 
		{
			postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (transformation matrix is not 2D)"));
			return(0);
		}
		if ((fs.transform.getn() != 4)&&(fs.transform.getm() != 4))
		{
			postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (transformation matrix is not 4x4)"));
			return(0);
		}
	}
	
	// In case one of the components above is given and dims is not given,
	// derive this one from the size of the data 
	if (((fs.transform.isdense())||(fs.meshclass.compareCI("scanline"))||(fs.meshclass.compareCI("image"))||(fs.meshclass.compareCI("latvol")))&&(fs.dims.isempty()))
	{
		if (fs.scalarfield.isdense()) 
			{  std::vector<long> dims = fs.scalarfield.getdims();
			   fs.dims.createlongvector(dims);
			}
		if (fs.vectorfield.isdense()) 
			{  std::vector<long> dims = fs.vectorfield.getdims();
				// remove the vector dimension from the array
			   fs.dims.createlongvector((dims.size()-1),&(dims[1]));
			}
		if (fs.tensorfield.isdense()) 
			{  std::vector<long> dims = fs.tensorfield.getdims();
				// remove the tensor dimension from the array
			   fs.dims.createlongvector((dims.size()-1),&(dims[1]));
			}
		if ((fs.data_at == SCIRun::Field::CELL)||(fs.data_at == SCIRun::Field::FACE)||(fs.data_at == SCIRun::Field::EDGE))
		{
			std::vector<long> dims = fs.scalarfield.getdims();
			// dimensions need to be one bigger
			for (long p = 0; p<dims.size(); p++) dims[p] = dims[p]+1;
			fs.dims.createlongvector(dims);
		}
	}
	
	// if dims is not present it is not a regular mesh
	// Data at edges, faces, or cells is only possible if the data
	// is of that dimension otherwise skip it and declare the data not usable
	
	if (fs.dims.isdense())
	{
		if ((fs.dims.getnumelements()==1)&&(fs.data_at!=SCIRun::Field::NONE)&&(fs.data_at!=SCIRun::Field::NODE)&&(fs.data_at!=SCIRun::Field::EDGE)) 
		{
			postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (specified data location is not supported for scanline)"));
			return(0);
		}
		if ((fs.dims.getnumelements()==2)&&(fs.data_at!=SCIRun::Field::NONE)&&(fs.data_at!=SCIRun::Field::NODE)&&(fs.data_at!=SCIRun::Field::FACE))
		{
			postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (specified data location is not supported for image)"));
			return(0);
		}
		if ((fs.dims.getnumelements()==3)&&(fs.data_at!=SCIRun::Field::NONE)&&(fs.data_at!=SCIRun::Field::NODE)&&(fs.data_at!=SCIRun::Field::CELL))
		{
			postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (specified data location is not supported for latvol)"));
			return(0);
		}
\
	}
	
	// If it survived until here it should be translatable or not a regular mesh at all
	
	if (fs.dims.isdense())
	{
		long size = fs.dims.getnumelements();
		
		if ((size > 0)&&(size < 4))
		{
			std::ostringstream oss;
			std::string name = mlarray.getname();
			oss << name << " ";
			if (name.length() < 30) oss << std::string(30-(name.length()),' '); // add some form of spacing		
		
			if (fs.meshclass.isstring())
			{   // explicitly stated type: (check whether type confirms the guessed type, otherwise someone supplied us with improper data)
				if ((fs.meshclass.compareCI("scanline"))&&(size!=1))
				{
					postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (scanline needs only one dimension)"));
					return(0);
				}
				if ((fs.meshclass.compareCI("image"))&&(size!=2)) 
				{
					postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (an image needs two dimensions)"));
					return(0);
				}

				if ((fs.meshclass.compareCI("latvolmesh"))&&(size!=3))
				{
					postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (a latvolmesh needs three dimensions)"));
					return(0);
				}

			}	

			switch (size)
			{
				case 1:
					oss << "[SCANLINE - " << fieldtype << "]";
					break;
				case 2:
					oss << "[IMAGE - " << fieldtype << "]";
					break;
				case 3:	
					oss << "[LATVOLMESH - " << fieldtype << "]";
					break;
			}
			infostring = oss.str();
			return(1);					
		}
		else
		{
			postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (number of dimensions (.dims field) needs to 1, 2, or 3)"));
			return(0);
		}
	
	}

	
	// Test for structured meshes
	// vvvvvvvvvvvvvvvvvvvvvvv
	
	if ((fs.x.isdense())&&(fs.y.isdense())&(fs.z.isdense()))
	{
	
		// TEST: The dimensions of the x, y, and z ,atrix should be equal
		long numdims = fs.x.getnumdims();
		if (fs.y.getnumdims() != numdims) 
		{
			postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (the dimensions of the x and y matrix do not match)"));
			return(0);
		}
		if (fs.z.getnumdims() != numdims) 
		{
			postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (the dimensions of the x and z matrix do not match)"));
			return(0);
		}
		
		std::vector<long> dimsx = fs.x.getdims();
		std::vector<long> dimsy = fs.y.getdims();
		std::vector<long> dimsz = fs.z.getdims();
		
		// Check dimension by dimension for any problems
		for (long p=0 ; p < numdims ; p++)
		{
			if(dimsx[p] != dimsy[p]) 
			{
				postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (the dimensions of the x and y matrix do not match)"));
				return(0);
			}
			if(dimsx[p] != dimsz[p]) 
			{
				postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (the dimensions of the x and z matrix do not match)"));
				return(0);
			}
		}

		
		std::ostringstream oss;
		std::string name = mlarray.getname();
		oss << name << " ";
		if (name.length() < 30) oss << std::string(30-(name.length()),' '); // add some form of spacing		
		
		// Minimum number of dimensions is in matlab is 2 and hence detect any empty dimension
		if (numdims == 2)
		{
			// This case will filter out the scanline objects
			// Currently SCIRun will fail/crash with an image where one of the
			// dimensions is one, hence prevent some troubles
			if ((dimsx[0] == 1)||(dimsx[1] == 1)) numdims = 1;
		}

		// Disregard data at odd locations. The translation function for those is not straight forward
		// Hence disregard those data locations.

		if ((numdims==1)&&(fs.data_at!=SCIRun::Field::NONE)&&(fs.data_at!=SCIRun::Field::NODE)&&(fs.data_at!=SCIRun::Field::EDGE))
		{
			postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (improper data location for a structured curvemesh)"));
			return(0);
		}
		if ((numdims==2)&&(fs.data_at!=SCIRun::Field::NONE)&&(fs.data_at!=SCIRun::Field::NODE)&&(fs.data_at!=SCIRun::Field::FACE)) 
		{
			postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (improper data location for a structured quadsurfmesh)"));
			return(0);
		}
		if ((numdims==3)&&(fs.data_at!=SCIRun::Field::NONE)&&(fs.data_at!=SCIRun::Field::NODE)&&(fs.data_at!=SCIRun::Field::CELL))
		{
			postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (improper data location for a structured hexvolmesh)"));
			return(0);
		}
	
		if (fs.meshclass.isstring())
		{   // explicitly stated type (check whether type confirms the guessed type, otherwise someone supplied us with improper data)
			if ((fs.meshclass.compareCI("structcurve"))&&(numdims!=1))
			{
				postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (invalid number of dimensions for x, y, and z matrix)"));
				return(0);
			}
			if ((fs.meshclass.compareCI("structquadsurf"))&&(numdims!=2)) 
			{
				postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (invalid number of dimensions for x, y, and z matrix)"));
				return(0);
			}
			if ((fs.meshclass.compareCI("structhexvol"))&&(numdims!=3)) 
			{
				postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (invalid number of dimensions for x, y, and z matrix)"));
				return(0);
			}
		}		
			
		switch (numdims)
		{
			case 1:
				oss << "[STRUCTURED CURVEMESH (" << dimsx[0] << " nodes) - " << fieldtype << "]";
				break;
			case 2:
				oss << "[STRUCTURED QUADSURFMESH (" << dimsx[0] << "x" << dimsx[1] << " nodes) - " << fieldtype << "]";
				break;
			case 3:
				oss << "[STRUCTURED HEXVOLMESH (" << dimsx[0] << "x" << dimsx[1] << "x" << dimsx[2] << " nodes) - " << fieldtype << "]";
				break;
			default:
					postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (invalid number of dimensions)"));
				return(0);  // matrix is not compatible
		}
		infostring = oss.str();
		return(1);		
	}
	
	// check for unstructured meshes

	if (fs.node.isempty()) 
	{
		postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (no node matrix for unstructured mesh, create a .node field)"));
		return(0); // a node matrix is always required
	}
	
	if (fs.node.getnumdims() > 2) 
	{
		postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (invalid number of dimensions for node matrix)"));
		return(0); // Currently N dimensional arrays are not supported here
	}
	// Check the dimensions of the NODE array supplied only [3xM] or [Mx3] are supported
	long m,n;
	m = fs.node.getm();
	n = fs.node.getn();
	
	long numpoints;
	long numel;
	
	if ((n==0)||(m==0)) 
	{
		postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (node matrix is empty)"));
		return(0); //empty matrix, no nodes => no mesh => no field......
	}
	if ((n != 3)&&(m != 3)) 
	{
		postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (one of the dimensions of node matrix needs to be 3)"));
		return(0); // SCIRun is ONLY 3D data, no 2D, or 1D
	}
	numpoints = n;
	if ((m!=3)&&(n==3)) numpoints = m;
	
	if ((fs.edge.isempty())&&(fs.face.isempty())&&(fs.cell.isempty()))
	{
		// This has no connectivity data => it must be a pointcloud ;)
		// Supported mesh/field types here:
		// PointCloudField
		
		if (fs.meshclass.isstring())
		{   // explicitly stated type (check whether type confirms the guessed type, otherwise someone supplied us with improper data)
			if (!(fs.meshclass.compareCI("pointcloud"))) 
			{
				postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (data has to be of the pointcloud class)"));
				return(0);
			}
		}
		
		// Data at edges, faces, and cells is nonsense for point clouds 
		if ((fs.data_at!=SCIRun::Field::NONE)&&(fs.data_at!=SCIRun::Field::NODE))
		{
			postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (data can only be located at the nodes of a pointcloud)"));
			return(0);
		}
		
		// Create an information string for the GUI
		std::ostringstream oss;
		std::string name = mlarray.getname();	
		oss << name << "  ";
		if (name.length() < 30) oss << std::string(30-(name.length()),' '); // add some form of spacing
		oss << "[POINTCLOUD (" << numpoints << " nodes) - " << fieldtype << "]";
		infostring = oss.str();
		return(1);
	}
	
	if (fs.edge.isdense())
	{
		// Edge data is provide hence it must be some line element!
		// Supported mesh/field types here:
		//  CurveField
		if (fs.meshclass.isstring())
		{   // explicitly stated type 
			if (!(fs.meshclass.compareCI("curve"))) return(0);
		}

		// Data at faces, and cells is nonsense for  curves
		if ((fs.data_at!=SCIRun::Field::NONE)&&(fs.data_at!=SCIRun::Field::NODE)&&(fs.data_at!=SCIRun::Field::EDGE))
		{
			postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (data cannot be located at faces or cells for a curvemesh)"));
			return(0);
		}	

		// Test whether someone made it into a line/surface/volume element type.
		// Since multiple connectivity matrices do not make sense (at least at this point)
		// we do not allow them to be used, hence they are not compatible
		if ((!fs.face.isempty())||(!fs.cell.isempty()))
		{   // a matrix with multiple connectivities is not yet allowed
			postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (multiple connectivity matrices defined)"));
			return(0);
		}
		  
		
		// Connectivity should be 2D
		if (fs.edge.getnumdims() > 2)
		{
			postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (edge connectivity matrix should be 2D)"));
			return(0);		
		} 
		 		
		// Check whether the connectivity data makes any sense, if not one of the dimensions is 2, the data is some not
		// yet supported higher order element
		m = fs.edge.getm();
		n = fs.edge.getn();
		
		if ((n!=2)&&(m!=2))
		{
			postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (one of the dimensions of edge needs to be of size 2)"));
			return(0);		
		} 

	
		numel = n;
		if ((m!=2)&&(n==2)) numel = m;

		// Create an information string for the GUI				
		std::ostringstream oss;	
		std::string name = mlarray.getname();
		oss << name << "  ";
		if (name.length() < 30) oss << std::string(30-(name.length()),' '); // add some form of spacing
		oss << "[CURVEFIELD (" << numpoints << " nodes, " << numel << " edges) - " << fieldtype << "]";
		infostring = oss.str();
		return(1);
	}

	if (fs.face.isdense())
	{
		// Supported mesh/field types here:
		// TriSurfField

		// The connectivity data should be 2D
		if (fs.face.getnumdims() > 2)
		{
			postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (the face connection matrix needs to be 2D)"));
			return(0);		
		} 
		
		m = fs.face.getm();
		n = fs.face.getn();
	
		// if the cell matrix is not empty, the mesh is both surface and volume, which
		// we do not support at the moment.
		if((!fs.cell.isempty()))
		{
			postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (both a face and a cell matrix are defined)"));
			return(0);		
		} 

		// Data at scells is nonsense for surface elements
		if ((fs.data_at!=SCIRun::Field::NONE)&&(fs.data_at!=SCIRun::Field::NODE)&&(fs.data_at!=SCIRun::Field::EDGE)&&(fs.data_at!=SCIRun::Field::FACE))
		{
			postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (the field is at the cell location for a surface mesh)"));
			return(0);		
		}

		if ((m==3)||((n==3)&&(n!=4)))
		{
			// check whether the element type is explicitly given
			if (fs.meshclass.isstring())
			{   // explicitly stated type 
				if (!(fs.meshclass.compareCI("trisurf")))
				{
					postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (meshclass should be trisurf)"));
					return(0);
				}
			}
	
			numel = n;
			if ((m!=3)&&(n==3)) numel = m;

							
			// Generate an output string describing the field we just recognized
			std::ostringstream oss;	
			std::string name = mlarray.getname();
			oss << name << "  ";
			if (name.length() < 30) oss << std::string(30-(name.length()),' '); // add some form of spacing
			oss << "[TRISURFFIELD (" << numpoints << " nodes, " << numel << " faces) - " << fieldtype << "]";
			infostring = oss.str();		
			return(1);
		}
		
		if ((m==4)||((n==4)&&(n!=3)))
		{
			// check whether the element type is explicitly given
			if (fs.meshclass.isstring())
			{   // explicitly stated type 
				if (!(fs.meshclass.compareCI("quadsurf"))) 
				{
					postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (meshclass should be quadsurf)"));
					return(0);
				}
			}
			
			numel = n;
			if ((m!=4)&&(n==4)) numel = m;
			
			
			// Generate an output string describing the field we just recognized
			std::ostringstream oss;	
			std::string name = mlarray.getname();
			oss << name << "  ";
			if (name.length() < 30) oss << std::string(30-(name.length()),' '); // add some form of spacing
			oss << "[QUADSURFFIELD (" << numpoints << "nodes , " << numel << " faces) - " << fieldtype << "]";
			infostring = oss.str();		
			return(1);
		}
		
		postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (matrix is of an unknown surface mesh class: the .face field does not have a dimension of size 3 or 4)"));
		return(0);
	}

	if (fs.cell.isdense())
	{
		// Supported mesh/field types here:
		// TetVolField
		// HexVolField
		
		if (fs.cell.getnumdims() > 2)
		{
			postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (the cell connection matrix should be 2D)"));
			return(0);
		}
		m = fs.cell.getm();
		n = fs.cell.getn();
		
		// m is the preferred direction for the element node indices of one element, n is the number of elements
		// However we except a transposed matrix as long as m != 8 , which would indicate hexahedral element
		if ((m==4)||((n==4)&&(m!=8)&&(m!=6)))
		{
			if (fs.meshclass.isstring())
			{   // explicitly stated type 
				if (!(fs.meshclass.compareCI("tetvol")))
				{
					postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (the mesh class should be tetvol)"));
					return(0);
				}
			}	
			
			numel = n;
			if ((m!=4)&&(n==4)) numel = m;

			
			std::ostringstream oss;	
			std::string name = mlarray.getname();			
			oss << name << "  ";
			if (name.length() < 30) oss << std::string(30-(name.length()),' '); // add some form of spacing
			oss << "[TETVOLFIELD (" << numpoints << " nodes, " << numel << " cells) - " << fieldtype << "]";
			infostring = oss.str();		
			return(1);
		}
		// In case it is a hexahedral mesh
		else if((m==8)||((n==8)&&(m!=4)&&(m!=6)))
		{
			if (fs.meshclass.isstring())
			{   // explicitly stated type 
				if (!(fs.meshclass.compareCI("hexvol"))) 
				{
					postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (the mesh class should be hexvol)"));
					return(0);			
				}
			}	
			
			numel = n;
			if ((m!=8)&&(n==8)) numel = m;
			
			std::ostringstream oss;	
			std::string name = mlarray.getname();		
			oss << name << "  ";
			if (name.length() < 30) oss << std::string(30-(name.length()),' '); // add some form of spacing
			oss << "[HEXVOLFIELD (" << numpoints << " nodes, " << numel << " cells) - " << fieldtype << "]";
			infostring = oss.str();		
			return(1);		
		}
		
		else if((m==6)||((n==6)&&(m!=4)&&(m!=8)))
		{
			if (fs.meshclass.isstring())
			{   // explicitly stated type 
				if (!(fs.meshclass.compareCI("prismvol"))) 
				{
					postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (the mesh class should be prismvol)"));
					return(0);			
				}
			}	
			
			numel = n;
			if ((m!=6)&&(n==6)) numel = m;

			
			std::ostringstream oss;	
			std::string name = mlarray.getname();		
			oss << name << "  ";
			if (name.length() < 30) oss << std::string(30-(name.length()),' '); // add some form of spacing
			oss << "[PRISMVOLFIELD (" << numpoints << " nodes, " << numel << " cells) - " << fieldtype << "]";
			infostring = oss.str();		
			return(1);		
		}
		
		postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (cell connection matrix dimensions do not match any of the supported mesh classes: the .cell field does not have a dimension of size 4, 6, or 8)"));		
		return(0);
	}
	
	postmsg(module,std::string("Matrix '" + mlarray.getname() + "' cannot be translated into a SCIRun Field (cannot match the matlab structure with any of the supported mesh classes)"));
	return(0);
}



void matlabconverter::mlArrayTOsciField(matlabarray mlarray,SCIRun::FieldHandle &scifield,SCIRun::Module *module)
{
	// The first part of the converter is NOT dynamic and is concerned with figuring out what the data
	// represents. We construct information string for the final dynamic phase 


	// If it is regular matrix translate it to a image or a latvol
	// The following section of code rewrites the matlab matrix into a
	// structure and then the normal routine picks up the field and translates it
	// properly.
	
	// This is a quick and not so pretty approach bu saves a lot of coding
	if (mlarray.isdense())
	{
		long numdims = mlarray.getnumdims();
		// We only add support for 1D, 2D and 3D matrices
		// We do not have 4D and higher visualization options
		if ((numdims >0)&&(numdims < 4))
		{
			matlabarray ml;
			matlabarray dimsarray;
			std::vector<long> d = mlarray.getdims();
			if ((d[0]==1)||(d[1]==1))
			{
				// Squeeze out a dimension if it has a size of one
				// This to avoid problems with the image class, 
				// which does crash in certain circumstances if one of the
				// dimensions is one, hence we convert it into a 
				// scanline object
				if (d[0]==1) d[0] = d[1];
				long temp = d[0];
				d.resize(1);
				d[0] = temp;
			}
			
			// convert the dense matrix into a structured matrix.
			// mlarary will now point to a new array that has been
			// created on top of the old one and points to the old one
			// as one of its fields.
			dimsarray.createlongvector(d);
			ml.createstructarray();
			ml.setfield(0,"dims",dimsarray);
			ml.setfield(0,"field",mlarray);
			ml.setname(mlarray.getname());
			mlarray = ml;
		}
	}

	if (!mlarray.isstruct()) throw matlabconverter_error(); // not compatible if it is not structured data
	fieldstruct fs = analyzefieldstruct(mlarray); // read the main structure of the object
	// The function above deals with all the different conventions around for writing geometry files in matlab
	// We have been using to many standards and this one should be compatible with most of them
	
	// Check what kind of field has been supplied
	
	// The fieldtype string, contains the type of field
	// This piece of information is used in the dynamic compilation
	std::string valuetype = "double"; // default type even if there is no data
	std::string fieldtype  = "";
	
	if (fs.fieldtype.compareCI("vector")) { fs.field = fs.scalarfield; valuetype = "Vector";}
	if (fs.fieldtype.compareCI("tensor")) { fs.field = fs.scalarfield; valuetype = "Tensor";} 
	
	if (fs.vectorfield.isdense()) { fs.field = fs.vectorfield; valuetype = "Vector"; }
	if (fs.tensorfield.isdense()) { fs.field = fs.tensorfield; valuetype = "Tensor"; }
	
	if ((fs.fieldtype.compareCI("scalar"))||(fs.fieldtype.isempty())&&(fs.scalarfield.isdense()))
	{
		matlabarray::mitype type = fs.scalarfield.gettype();
		switch (type)
		{
			case matlabarray::miUINT8:  valuetype = "unsigned char"; break;
			case matlabarray::miINT8:	valuetype = "signed char"; break;
			case matlabarray::miINT16:	valuetype = "short"; break;
			case matlabarray::miUINT16:	valuetype = "unsigned short"; break;
			case matlabarray::miINT32:	valuetype = "long"; break;
			case matlabarray::miUINT32:	valuetype = "unsigned long"; break;
			case matlabarray::miSINGLE:	valuetype = "float"; break;
			case matlabarray::miDOUBLE:	valuetype = "double"; break;
			default: valuetype = "double";
		}
		fs.field = fs.scalarfield;
	}

	// A latvol/image can be created without specifying the dimensions. In the latter case the meshclass must be given or a transform
	// matrix. The next piece of code deals with this situation
	
	if (((fs.transform.isdense())||(fs.meshclass.compareCI("scanline"))||(fs.meshclass.compareCI("image"))||(fs.meshclass.compareCI("latvol")))&&(fs.dims.isempty()))
	{
		std::vector<long> dims = fs.field.getdims();
		if ((valuetype == "Vector")||(valuetype == "Tensor"))
		{	
			fs.dims.createlongvector((dims.size()-1),&(dims[1])); 
		}
		else
		{   
			fs.dims.createlongvector(dims);	
		}
		
		if ((fs.data_at == SCIRun::Field::CELL)||(fs.data_at == SCIRun::Field::FACE)||(fs.data_at == SCIRun::Field::EDGE))
		{
			std::vector<long> dims;
			fs.dims.getnumericarray(dims);
			for (long p = 0; p<dims.size(); p++) dims[p] = dims[p]+1;
			fs.dims.setnumericarray(dims);
		}
	}
	
	// Complete a structured mesh
	
	if ((fs.x.isempty())&&(fs.y.isdense())) { std::vector<long> dims = fs.y.getdims(); fs.x.createdensearray(dims,matlabarray::miDOUBLE); }
	if ((fs.x.isempty())&&(fs.z.isdense())) { std::vector<long> dims = fs.z.getdims(); fs.x.createdensearray(dims,matlabarray::miDOUBLE); }
	if ((fs.y.isempty())&&(fs.x.isdense())) { std::vector<long> dims = fs.x.getdims(); fs.y.createdensearray(dims,matlabarray::miDOUBLE); }
	if ((fs.y.isempty())&&(fs.z.isdense())) { std::vector<long> dims = fs.z.getdims(); fs.y.createdensearray(dims,matlabarray::miDOUBLE); }
	if ((fs.z.isempty())&&(fs.x.isdense())) { std::vector<long> dims = fs.x.getdims(); fs.z.createdensearray(dims,matlabarray::miDOUBLE); }
	if ((fs.z.isempty())&&(fs.y.isdense())) { std::vector<long> dims = fs.y.getdims(); fs.z.createdensearray(dims,matlabarray::miDOUBLE); }
	
	// One of thee next ones is always required to make a regular, structured or an unstructured mesh
	if ((fs.node.isempty())&&(fs.x.isempty())&&(fs.dims.isempty())) throw matlabconverter_error(); 
	
	// The next piece of code reorders the matrices if needed
	// If the user made a mistake and transposed a matrix this piece of code will try to correct for any mistakes
	// All of these checks involve the unstructured mesh, where each dimension has a different meaning.
	//For structured meshes this thing does not play a role.

	long n,m;
	
	// Reorder the node matrix if it exists
	if (fs.node.isdense())
	{
		m = fs.node.getm();
		n = fs.node.getn();
	
		// This condition should have been checked by the compatibility algorithm
		if ((m != 3)&&(n != 3)) throw matlabconverter_error();
	
		// In case the matrix is transposed, reorder it in the proper order for this converter
		if (m != 3) fs.node.transpose();
	}
	
	// Check and reorder the edge matrix
	if (fs.edge.isdense())
	{
		m = fs.node.getm();
		n = fs.node.getn();
	
		if (fs.meshclass.isstring())
		{   // explicitly stated type 
			if (fs.meshclass.compareCI("curve"))
			{
				if ((n!=2)&&(m!=2)) throw matlabconverter_error();
				if (m != 2) fs.edge.transpose();
			}
		}
		else
		{
			if ((n!=2)&&(m!=2)) throw matlabconverter_error();
			if (m != 2) fs.edge.transpose();
		}
	}
	
	// Check and reorder the face martix
	if (fs.face.isdense())
	{
		m = fs.face.getm();
		n = fs.face.getn();
		
		if (fs.meshclass.isstring())
		{   // explicitly stated type 
			if (fs.meshclass.compareCI("trisurf"))
			{
				if ((n!=3)&&(m!=3)) throw matlabconverter_error();
				if (m!=3) fs.face.transpose();
			}
			
			if (fs.meshclass.compareCI("quadsurf"))
			{
				if ((n!=4)&&(m!=4)) throw matlabconverter_error();
				if (m!=4) fs.face.transpose();
			}
		}
		else
		{
			if ((n!=3)&&(m!=3)&&(n!=4)&&(m!=4)) throw matlabconverter_error();
			if ((m!=3)&&(m!=4)) fs.face.transpose();
		}
	}
	
	// Finally check the cell matrix
	if (fs.cell.isdense())
	{
		m = fs.cell.getm();
		n = fs.cell.getn();
		
		if (fs.meshclass.isstring())
		{   // explicitly stated type 
			if (fs.meshclass.compareCI("tetvol"))
			{
				if ((n!=4)&&(m!=4)) throw matlabconverter_error();
				if (m!=4) fs.cell.transpose();
			}
			
			if (fs.meshclass.compareCI("hexvol"))
			{
				if ((n!=8)&&(m!=8)) throw matlabconverter_error();
				if (m!=8) fs.cell.transpose();
			}
			
			if (fs.meshclass.compareCI("prismvol"))
			{
				if ((n!=6)&&(m!=6)) throw matlabconverter_error();
				if (m!=6) fs.cell.transpose();
			}
		}
		else
		{
			if ((n!=4)&&(m!=4)&&(n!=6)&&(m!=6)&&(n!=8)&&(m!=8)) throw matlabconverter_error();
			if ((m!=4)&&(m!=6)&&(m!=8)) fs.cell.transpose();
		}
	}
	
	// Check structured meshes
	
	if ((fs.x.isdense())&&(fs.y.isdense())&&(fs.z.isdense()))
	{
		long numdim = fs.x.getnumdims();
	
		if ((numdim == 2)&&((fs.x.getm() == 1)||(fs.x.getn() == 1)))
		{
			numdim = 1;
			if (fs.x.getm() == 1)
			{
				fs.x.transpose();
				fs.y.transpose();
				fs.z.transpose();
			}
		}
	}
	
	// Check the field information and preprocess the field information

	// detect whether there is data and where it is
	SCIRun::Field::data_location data_at = SCIRun::Field::NONE;

	// The default location for data is at the nodes
	// at least when there is a field defined
	
	if (fs.field.isdense()) data_at = SCIRun::Field::NODE;
	// Fieldlocation overrules previous settings
	if (!(fs.fieldlocation.isempty())) data_at = fs.data_at;
	
	// In case the location has been supplied but no actual data has been supplied, reset the location to NONE
	if (fs.field.isempty()) data_at = SCIRun::Field::NONE;

	// Currently the following assumption on the data is made:
	// first dimension is scalar/vector/tensor (in case of scalar this dimension does not need to exist)
	// second dimension is the number of nodes/edges/faces/cells (this one always exists)
	
	// This algorithm tries to transpose the field data if needed
	// This one only handles the unstructured mesh cases
	if ((data_at != SCIRun::Field::NONE)&&(fs.field.isdense())&&(fs.x.isempty())&&(fs.y.isempty())&&(fs.z.isempty())&&(fs.dims.isempty()))
	{
		if (valuetype == "Vector")
		{
			if (fs.field.getnumdims() == 2)
			{   
				m = fs.field.getm();
				n = fs.field.getn();
				if ((m != 3)&&(n != 3)) 
				{
					data_at = SCIRun::Field::NONE;
				}
				else
				{
					if (m != 3) fs.field.transpose();
				}
			}
			else
			{
				data_at = SCIRun::Field::NONE;
			}
		}
		else if (valuetype == "Tensor")
		{
			if (fs.field.getnumdims() == 2)
			{
				m = fs.field.getm();
				n = fs.field.getn();
				if ((m != 6)&&(n != 6)&&(m != 9)&&(n != 9))
				{
					data_at = SCIRun::Field::NONE;
				}
				else
				{
					if ((m != 6)&&(m != 9)) fs.field.transpose();
				}
			}
			else
			{
				data_at = SCIRun::Field::NONE;
			}
		}
		else
		{
			if (fs.field.getnumdims() == 2)
			{ 
				m = fs.field.getm();
				n = fs.field.getn();
				if ((m != 1)&&(n != 1)) 
				{   // ignore the data and only load the mesh
					data_at = SCIRun::Field::NONE;
				}
				else
				{
					if (m != 1) fs.field.transpose();
				}
			}
			else
			{   // ignore the data and only load the mesh 
				data_at = SCIRun::Field::NONE;
			}	
		}
	}
	
	fs.data_at = data_at;
	
	// Decide what container to use and what the geometry is
	
	// For unstructured geometries it is easy
	if ((fs.x.isempty())&&(fs.dims.isempty())&&(fs.node.isdense()))
	{
		if ((fs.edge.isempty())&&(fs.face.isempty())&&(fs.cell.isempty())) fieldtype = "PointCloudField";
		if (fs.edge.isdense())
		{
			if (fs.edge.getm() == 2) fieldtype = "CurveField";
		}
		if (fs.face.isdense())
		{
			if (fs.face.getm() == 3) fieldtype = "TriSurfField";
			if (fs.face.getm() == 4) fieldtype = "QuadSurfField";
		}
		if (fs.cell.isdense())
		{
			if (fs.cell.getm() == 4) fieldtype = "TetVolField";
			if (fs.cell.getm() == 6) fieldtype = "PrismVolField";
			if (fs.cell.getm() == 8) fieldtype = "HexVolField";
		}		
	}
	
	// For structured meshes
	if ((fs.x.isdense())&&(fs.node.isempty())&&(fs.dims.isempty()))
	{
		long numdims = fs.x.getnumdims();
		if ((numdims ==2)&&(fs.x.getn()==1)) numdims = 1;
		switch (numdims)
		{
			case 1: fieldtype = "StructCurveField"; break;
			case 2: fieldtype = "StructQuadSurfField"; break;
			case 3: fieldtype = "StructHexVolField"; break;
			default: throw matlabconverter_error();
		}
	}
	
	// And finally for regular meshes
	if ((fs.x.isempty())&&(fs.node.isempty())&&(fs.dims.isdense()))
	{
		long numdims = fs.dims.getnumelements();
		switch (numdims)
		{
			case 1: fieldtype = "ScanlineField"; break;
			case 2: fieldtype = "ImageField"; break;
			case 3: fieldtype = "LatVolField"; break;
			default: throw matlabconverter_error();
		}
	}
	
	// In case no field type was detected
	if (fieldtype == "") throw matlabconverter_error();

	// NOW IT IS TIME TO INVOKE SOME DYNAMIC MAGIC ... 
	
	
	// A lot of work just to make a TypeDescription
	// This code is in a way a tricked version, since we do not yet have
	// the object we want. This piece is a hack into the SCIRun system
	// for managing this. It's ugly.... 


	std::string fielddesc = "SCIRun::" + fieldtype +"<" + valuetype + "> ";
	std::string fieldname = SCIRun::DynamicAlgoBase::to_filename(fielddesc);
	//SCIRun::TypeDescription scifieldtype(fdesc,SCIRun::TypeDescription::cc_to_h(__FILE__),"SCIRun");
	
	static const std::string include_path(SCIRun::TypeDescription::cc_to_h(__FILE__));
	static const std::string template_class_name("MatlabFieldReaderAlgoT");
	static const std::string base_class_name("MatlabFieldReaderAlgo");
  
	// Everything we did is in the MatlabIO namespace so we need to add this
	// Otherwise the dynamically code generator will not generate a "using namespace ...."
	static const std::string name_space_name("MatlabIO");
	static const std::string name_space_name2("SCIRun");

	std::string filename = template_class_name + "." + fieldname + ".";
	// Supply the dynamic compiler with enough information to build a file in the
	// on-the-fly libs which will have the templated function in there
	SCIRun::CompileInfoHandle cinfo = 
		scinew SCIRun::CompileInfo(filename,base_class_name, template_class_name, fielddesc);
	cinfo->add_namespace(name_space_name2);
	cinfo->add_namespace(name_space_name);
	cinfo->add_include(include_path);

	SCIRun::Handle<MatlabFieldReaderAlgo> algo;
	
	// A placeholder for the dynamic code
	
	// Do the magic, internally algo will now refer to the proper dynamic class, which will be
	// loaded by this function as well
	if (!module->module_dynamic_compile(cinfo, algo)) 
	{
		// Dynamic compilation failed
		scifield = 0;
		module->error("matlabconverter::mlArrayTOsciField: Dynamic compilation failed\n");
		return;
	}
	
	// The function takes the matlabconverter pointer again, which we need to re-enter the object, which we will
	// leave in the dynamic compilation. The later was done to assure small filenames in the on-the-fly libs
	// Filenames over 31 chars will cause problems on certain systems
	// Since the matlabconverter holds all our converter settings, we don't want to lose it, hence it is added
	// here. The only disadvantage is that some of the functions in the matlabconverter class nedd to be public

	if (!(algo->execute(scifield,fs,*this)))
	{
		// The algorithm has an builtin sanity check. If a specific converter cannot be built
		// it will create an algorithm that returns a false. Hence instead of failing at the
		// compiler level a proper description will be issued to the user of the module
		scifield = 0;
		module->error("matlabconverter::mlArrayTOsciField: The dynamically compiled matlabconverter does not function properly; most probably some specific mesh or field converters are missing or have not yet been implemented\n");
		return;
	}

	if (fs.property.isstruct())
	{
		if (scifield != 0)
		{
			mlPropertyTOsciProperty(mlarray,static_cast<SCIRun::PropertyManager *>(scifield.get_rep()));
		}
	}

	if (fs.name.isstring())
	{
		if (scifield != 0)
		{
			if (fs.name.isstring())
			{
				scifield->set_property("name",fs.name.getstring(),false);
			}
		}
	}
	else
	{
		if (scifield != 0)
		{
			scifield->set_property("name",mlarray.getname(),false);
		}
	}
	return;
}
	

matlabconverter::fieldstruct matlabconverter::analyzefieldstruct(matlabarray &ma)
{
	// define possible fieldnames
	// This function searches through the matlab structure and identifies which fields
	// can be used in the construction of a field.
	// The last name in each list is the recommended name. When multiple fields are 
	// defined which suppose to have the same contents, the last one is chosen, which
	// is the recommended name listed in the documentation.
	
	fieldstruct		fs;
	long			index;
	
	if (!ma.isstruct()) return(fs);
	
	// NODE MATRIX
	index = ma.getfieldnameindexCI("pts");	if (index > -1) fs.node = ma.getfield(0,index);
	index = ma.getfieldnameindexCI("node"); if (index > -1) fs.node = ma.getfield(0,index);

	// STRUCTURE MATRICES IN SUBMATRICES
	index = ma.getfieldnameindexCI("x"); if (index > -1) fs.x = ma.getfield(0,index);
	index = ma.getfieldnameindexCI("y"); if (index > -1) fs.y = ma.getfield(0,index);
	index = ma.getfieldnameindexCI("z"); if (index > -1) fs.z = ma.getfield(0,index);

	// EDGE MATRIX
	index = ma.getfieldnameindexCI("line"); if (index > -1) fs.edge = ma.getfield(0,index);
	index = ma.getfieldnameindexCI("edge");	if (index > -1) fs.edge = ma.getfield(0,index);

	// FACE MATRIX
	index = ma.getfieldnameindexCI("fac");  if (index > -1) fs.face = ma.getfield(0,index);
	index = ma.getfieldnameindexCI("quad");	if (index > -1) fs.face = ma.getfield(0,index);
	index = ma.getfieldnameindexCI("face");	if (index > -1) fs.face = ma.getfield(0,index);
	
	// CELL MATRIX
	index = ma.getfieldnameindexCI("tet"); 	if (index > -1) fs.cell = ma.getfield(0,index);
	index = ma.getfieldnameindexCI("hex");	if (index > -1) fs.cell = ma.getfield(0,index);
	index = ma.getfieldnameindexCI("prism"); if (index > -1) fs.cell = ma.getfield(0,index);
	index = ma.getfieldnameindexCI("cell");	if (index > -1) fs.cell = ma.getfield(0,index);
	
	// FIELDNODE MATRIX
	index = ma.getfieldnameindexCI("data"); 	if (index > -1) fs.scalarfield = ma.getfield(0,index);
	index = ma.getfieldnameindexCI("potvals");	if (index > -1) fs.scalarfield = ma.getfield(0,index);
	index = ma.getfieldnameindexCI("scalarfield"); 	if (index > -1) fs.scalarfield = ma.getfield(0,index);
	index = ma.getfieldnameindexCI("scalardata");	if (index > -1) fs.scalarfield = ma.getfield(0,index);
	index = ma.getfieldnameindexCI("field");	if (index > -1) fs.scalarfield = ma.getfield(0,index);

	// This field is still being checked but will be replaced by a combination of fieldtype and field
	// VECTOR FIELD MATRIX
	index = ma.getfieldnameindexCI("vectordata");   if (index > -1) fs.vectorfield = ma.getfield(0,index);
	index = ma.getfieldnameindexCI("vectorfield");	if (index > -1) fs.vectorfield = ma.getfield(0,index);

	// This field is still being checked but will be replaced by a combination of fieldtype and field
	// TENSOR FIELD MATRIX
	index = ma.getfieldnameindexCI("tensordata");	if (index > -1) fs.tensorfield = ma.getfield(0,index);
	index = ma.getfieldnameindexCI("tensorfield");	if (index > -1) fs.tensorfield = ma.getfield(0,index);

	// old name was dataat but has been replaced with fieldat, since all other fields start with this one as well
	// FIELD LOCATION MATRIX
	index = ma.getfieldnameindexCI("dataat");			if (index > -1) fs.fieldlocation = ma.getfield(0,index);
	index = ma.getfieldnameindexCI("fieldlocation");	if (index > -1) fs.fieldlocation = ma.getfield(0,index);
	index = ma.getfieldnameindexCI("fieldat");			if (index > -1) fs.fieldlocation = ma.getfield(0,index);

	// fieldtype is the prefered name
	// FIELD TYPE MATRIX
	index = ma.getfieldnameindexCI("datatype");			if (index > -1) fs.fieldtype = ma.getfield(0,index);
	index = ma.getfieldnameindexCI("fieldtype");		if (index > -1) fs.fieldtype = ma.getfield(0,index);

	// meshclass is now the prefered name
	// meshclass MATRIX
	index = ma.getfieldnameindexCI("elemtype");		if (index > -1) fs.meshclass = ma.getfield(0,index);
	index = ma.getfieldnameindexCI("meshclass");	if (index > -1) fs.meshclass = ma.getfield(0,index);

	// NAME OF THE MESH/FIELD
	index = ma.getfieldnameindexCI("name");		if (index > -1) fs.name = ma.getfield(0,index);

	// PROPERTY FIELD
	index = ma.getfieldnameindexCI("property"); if (index > -1) fs.property = ma.getfield(0,index);

	// we need some field in case there is no data assigned
	// REGULAR MATRICES
	index = ma.getfieldnameindexCI("dim"); 	if (index > -1) fs.dims = ma.getfield(0,index);
	index = ma.getfieldnameindexCI("dims");	if (index > -1) fs.dims = ma.getfield(0,index);
	
	// Revised the design now only full affine transformation matrices are allowed
	index = ma.getfieldnameindexCI("transform"); if (index > -1) fs.transform = ma.getfield(0,index);
	
	fs.data_at = SCIRun::Field::NONE;
	
	if (!(fs.fieldlocation.isempty()))
	{   // converter table for the string in the field "fieldlocation" array
		// These are case insensitive comparisons.
		if (!(fs.fieldlocation.isstring())) throw matlabconverter_error();
		if (fs.fieldlocation.compareCI("node")||fs.fieldlocation.compareCI("pts")) fs.data_at = SCIRun::Field::NODE;
		if (fs.fieldlocation.compareCI("egde")||fs.fieldlocation.compareCI("line")) fs.data_at = SCIRun::Field::EDGE;
		if (fs.fieldlocation.compareCI("face")||fs.fieldlocation.compareCI("fac")) fs.data_at = SCIRun::Field::FACE;
		if (fs.fieldlocation.compareCI("cell")||fs.fieldlocation.compareCI("tet")
			||fs.fieldlocation.compareCI("hex")||fs.fieldlocation.compareCI("prism")) fs.data_at = SCIRun::Field::CELL;
	}
	
	return(fs);
}


bool matlabconverter::addfield(std::vector<SCIRun::Vector> &fdata,matlabarray mlarray)
{
	std::vector<double> fielddata;
	mlarray.getnumericarray(fielddata); // cast and copy the real part of the data
	
	unsigned int numdata = fielddata.size();
	if (numdata > (3*fdata.size())) numdata = (3*fdata.size()); // make sure we do not copy more data than there are elements
	
	unsigned int p,q;
	for (p=0,q=0; p < numdata; q++) 
	{ 
		fdata[q][0] = fielddata[p++];
		fdata[q][1] = fielddata[p++];
		fdata[q][2] = fielddata[p++];
	}
	return(true);
}

bool matlabconverter::addfield(SCIRun::FData2d<SCIRun::Vector> &fdata,matlabarray mlarray)
{
	std::vector<double> fielddata;
	mlarray.getnumericarray(fielddata); // cast and copy the real part of the data

	unsigned int numdata = fielddata.size();
	if (numdata > (3*fdata.size())) numdata = (3*fdata.size()); // make sure we do not copy more data than there are elements
	
	SCIRun::Vector **data = fdata.get_dataptr();
	unsigned int dim1 = fdata.dim1();
	unsigned int dim2 = fdata.dim2();
	
	unsigned int q,r,p;
	for (p=0,q=0;(q<dim1)&&(p < numdata);q++)
		for (r=0;(r<dim2)&&(p < numdata);r++)
		{
			data[q][r][0] = fielddata[p++];
			data[q][r][1] = fielddata[p++];
			data[q][r][2] = fielddata[p++];
		}
	return(true);
}


bool matlabconverter::addfield(SCIRun::FData3d<SCIRun::Vector> &fdata,matlabarray mlarray)
{
	std::vector<double> fielddata;
	mlarray.getnumericarray(fielddata); // cast and copy the real part of the data
	
	unsigned int numdata = fielddata.size();
	if (numdata > (3*fdata.size())) numdata = (3*fdata.size()); // make sure we do not copy more data than there are elements
	
	SCIRun::Vector ***data = fdata.get_dataptr();
	unsigned int dim1 = fdata.dim1();
	unsigned int dim2 = fdata.dim2();
	unsigned int dim3 = fdata.dim3();
	
	unsigned int q,r,s,p;
	for (p=0,q=0;(q<dim1)&&(p < numdata);q++)
		for (r=0;(r<dim2)&&(p < numdata);r++)
			for (s=0;(s<dim3)&&(p <numdata);s++)
			{
				data[q][r][s][0] = fielddata[p++];
				data[q][r][s][1] = fielddata[p++];
				data[q][r][s][2] = fielddata[p++];
			}
	return(true);
}


bool matlabconverter::addfield(std::vector<SCIRun::Tensor> &fdata,matlabarray mlarray)
{
	std::vector<double> fielddata;
	mlarray.getnumericarray(fielddata); // cast and copy the real part of the data

	unsigned int numdata = fielddata.size();
	SCIRun::Tensor tensor;

	if (mlarray.getm() == 6)
	{ // Compressed tensor data : xx,yy,zz,xy,xz,yz
		if (numdata > (6*fdata.size())) numdata = (6*fdata.size()); // make sure we do not copy more data than there are elements
		unsigned int p,q;
		for (p = 0, q = 0; p < numdata; p +=6, q++) { compressedtensor(fielddata,tensor,p); fdata[q] =  tensor; }
	}
	else
	{  // UnCompressed tensor data : xx,xy,xz,yx,yy,yz,zx,zy,zz 
		if (numdata > (9*fdata.size())) numdata = (9*fdata.size()); // make sure we do not copy more data than there are elements
		unsigned int p,q;
		for (p = 0, q = 0; p < numdata; p +=9, q++) { uncompressedtensor(fielddata,tensor,p); fdata[q] =  tensor; }
	}
	return(true);
}



bool matlabconverter::addfield(SCIRun::FData2d<SCIRun::Tensor> &fdata,matlabarray mlarray)
{
	std::vector<double> fielddata;
	mlarray.getnumericarray(fielddata); // cast and copy the real part of the data
	
	unsigned int numdata = fielddata.size();

	SCIRun::Tensor tens;
	SCIRun::Tensor **data = fdata.get_dataptr();
	unsigned int dim1 = fdata.dim1();
	unsigned int dim2 = fdata.dim2();

	if (mlarray.getm() == 6)
	{ // Compressed tensor data : xx,yy,zz,xy,xz,yz
		if (numdata > (6*fdata.size())) numdata = (6*fdata.size()); // make sure we do not copy more data than there are elements
		unsigned int q,r,p;
		for (p=0,q=0;(q<dim1)&&(p < numdata);q++)
			for (r=0;(r<dim2)&&(p < numdata);r++, p+=6)
			{   compressedtensor(fielddata,tens,p); data[q][r] = tens; }
	}
	else
	{  // UnCompressed tensor data : xx,xy,xz,yx,yy,yz,zx,zy,zz 
		if (numdata > (9*fdata.size())) numdata = (9*fdata.size()); // make sure we do not copy more data than there are elements
		unsigned int q,r,p;
		for (p=0,q=0;(q<dim1)&&(p < numdata);q++)
			for (r=0;(r<dim2)&&(p < numdata);r++, p+=9)
			{   uncompressedtensor(fielddata,tens,p); data[q][r] = tens; }
	}
	return(true);
}


bool matlabconverter::addfield(SCIRun::FData3d<SCIRun::Tensor> &fdata,matlabarray mlarray)
{
	std::vector<double> fielddata;
	mlarray.getnumericarray(fielddata); // cast and copy the real part of the data

	SCIRun::Tensor tens;
	SCIRun::Tensor ***data = fdata.get_dataptr();
	unsigned int dim1 = fdata.dim1();
	unsigned int dim2 = fdata.dim2();
	unsigned int dim3 = fdata.dim3();
	
	unsigned int numdata = fielddata.size();
	if (mlarray.getm() == 6)
	{ // Compressed tensor data : xx,yy,zz,xy,xz,yz
		if (numdata > (6*fdata.size())) numdata = (6*fdata.size()); // make sure we do not copy more data than there are elements
		unsigned int q,r,s,p;
		for (p=0,q=0;(q<dim1)&&(p < numdata);q++)
			for (r=0;(r<dim2)&&(p < numdata);r++)
				for (s=0;(s<dim3)&&(p < numdata);s++,p +=6)
				{   compressedtensor(fielddata,tens,p); data[q][r][s] = tens; }
	}
	else
	{  // UnCompressed tensor data : xx,xy,xz,yx,yy,yz,zx,zy,zz 
		if (numdata > (9*fdata.size())) numdata = (9*fdata.size()); // make sure we do not copy more data than there are elements
		unsigned int q,r,s,p;
		for (p=0,q=0;(q<dim1)&&(p < numdata);q++)
			for (r=0;(r<dim2)&&(p < numdata);r++)
				for (s=0; (s<dim3)&&(p <numdata); s++, p+= 9)
				{   uncompressedtensor(fielddata,tens,p); data[q][r][s] = tens; }
	}
	return(true);
}


bool matlabconverter::createmesh(SCIRun::LockingHandle<SCIRun::PointCloudMesh> &meshH,fieldstruct &fs)
{
	meshH = new SCIRun::PointCloudMesh;
	addnodes(meshH,fs.node);
	return(true);
}

bool matlabconverter::createmesh(SCIRun::LockingHandle<SCIRun::CurveMesh> &meshH,fieldstruct &fs)
{
	meshH = new SCIRun::CurveMesh;
	addnodes(meshH,fs.node);
	addedges(meshH,fs.edge);
	return(true);
}

bool matlabconverter::createmesh(SCIRun::LockingHandle<SCIRun::TriSurfMesh> &meshH,fieldstruct &fs)
{
	meshH = new SCIRun::TriSurfMesh;
	addnodes(meshH,fs.node);
	addfaces(meshH,fs.face);
	return(true);
}

bool matlabconverter::createmesh(SCIRun::LockingHandle<SCIRun::QuadSurfMesh> &meshH,fieldstruct &fs)
{
	meshH = new SCIRun::QuadSurfMesh;
	addnodes(meshH,fs.node);
	addfaces(meshH,fs.face);
	return(true);
}

bool matlabconverter::createmesh(SCIRun::LockingHandle<SCIRun::TetVolMesh> &meshH,fieldstruct &fs)
{
	meshH = new SCIRun::TetVolMesh;
	addnodes(meshH,fs.node);
	addcells(meshH,fs.cell);
	return(true);
}

bool matlabconverter::createmesh(SCIRun::LockingHandle<SCIRun::HexVolMesh> &meshH,fieldstruct &fs)
{
	meshH = new SCIRun::HexVolMesh;
	addnodes(meshH,fs.node);
	addcells(meshH,fs.cell);
	return(true);
}

bool matlabconverter::createmesh(SCIRun::LockingHandle<SCIRun::PrismVolMesh> &meshH,fieldstruct &fs)
{
	meshH = new SCIRun::PrismVolMesh;
	addnodes(meshH,fs.node);
	addcells(meshH,fs.cell);
	return(true);
}


bool matlabconverter::createmesh(SCIRun::LockingHandle<SCIRun::ScanlineMesh> &meshH,fieldstruct &fs)
{
	std::vector<long> dims; 
	fs.dims.getnumericarray(dims);

	SCIRun::Point PointO(0.0,0.0,0.0);
	SCIRun::Point PointP(static_cast<double>(dims[0]),0.0,0.0);
	meshH = new SCIRun::ScanlineMesh(static_cast<unsigned int>(dims[0]),PointO,PointP);
	if (fs.transform.isdense())
	{
		SCIRun::Transform T;
		double trans[16];
		fs.transform.getnumericarray(trans,16);
		T.set_trans(trans);
		meshH->transform(T);
	}
	return(true);
}

bool matlabconverter::createmesh(SCIRun::LockingHandle<SCIRun::ImageMesh> &meshH,fieldstruct &fs)
{
	std::vector<long> dims; 
	fs.dims.getnumericarray(dims);

	SCIRun::Point PointO(0.0,0.0,0.0);
	SCIRun::Point PointP(static_cast<double>(dims[0]),static_cast<double>(dims[1]),0.0);
	meshH = new SCIRun::ImageMesh(static_cast<unsigned int>(dims[0]),static_cast<unsigned int>(dims[1]),PointO,PointP);
	if (fs.transform.isdense())
	{
		SCIRun::Transform T;
		double trans[16];
		fs.transform.getnumericarray(trans,16);
		T.set_trans(trans);
		meshH->transform(T);
	}
	return(true);
}

bool matlabconverter::createmesh(SCIRun::LockingHandle<SCIRun::LatVolMesh> &meshH,fieldstruct &fs)
{
	std::vector<long> dims; 
	fs.dims.getnumericarray(dims);

	SCIRun::Point PointO(0.0,0.0,0.0);
	SCIRun::Point PointP(static_cast<double>(dims[0]),static_cast<double>(dims[1]),static_cast<double>(dims[2]));
	meshH = new SCIRun::LatVolMesh(static_cast<unsigned int>(dims[0]),static_cast<unsigned int>(dims[1]),static_cast<unsigned int>(dims[2]),PointO,PointP);
	if (fs.transform.isdense())
	{
		SCIRun::Transform T;
		double trans[16];
		fs.transform.getnumericarray(trans,16);
		T.set_trans(trans);
		meshH->transform(T);
	}
	return(true);
}

bool matlabconverter::createmesh(SCIRun::LockingHandle<SCIRun::StructCurveMesh> &meshH,fieldstruct &fs)
{
	std::vector<long> dims;
	std::vector<unsigned int> mdims;
	long numdim = fs.x.getnumdims();
	dims = fs.x.getdims();
	
	mdims.resize(numdim);	
	for (long p=0; p < numdim; p++)  mdims[p] = static_cast<unsigned int>(dims[p]); 
	
	if ((numdim == 2)&&(fs.x.getn() == 1))
	{
		numdim = 1;
		mdims.resize(1);
		mdims[0] = fs.x.getm();
	}

	meshH = new SCIRun::StructCurveMesh;
	long numnodes =fs.x.getnumelements();
	
	std::vector<double> X;
	std::vector<double> Y;
	std::vector<double> Z;
	fs.x.getnumericarray(X);
	fs.y.getnumericarray(Y);
	fs.z.getnumericarray(Z);
	
	meshH->set_dim(mdims);
	long p;
	for (p = 0; p < numnodes; p++)
	{
		meshH->set_point(SCIRun::Point(X[p],Y[p],Z[p]),static_cast<SCIRun::StructCurveMesh::Node::index_type>(p));
	}
					
	return(true);
}

bool matlabconverter::createmesh(SCIRun::LockingHandle<SCIRun::StructQuadSurfMesh> &meshH,fieldstruct &fs)
{
	std::vector<long> dims;
	std::vector<unsigned int> mdims;
	long numdim = fs.x.getnumdims();
	dims = fs.x.getdims();
	
	mdims.resize(numdim);	
	for (long p=0; p < numdim; p++)  mdims[p] = static_cast<unsigned int>(dims[p]); 

	meshH = new SCIRun::StructQuadSurfMesh;
	long numnodes =fs.x.getnumelements();
	
	std::vector<double> X;
	std::vector<double> Y;
	std::vector<double> Z;
	fs.x.getnumericarray(X);
	fs.y.getnumericarray(Y);
	fs.z.getnumericarray(Z);
	
	meshH->set_dim(mdims);

	unsigned p,r,q;
	q = 0;
	for (r = 0; r < mdims[1]; r++)
	for (p = 0; p < mdims[0]; p++)
	{
		meshH->set_point(SCIRun::Point(X[q],Y[q],Z[q]),SCIRun::StructQuadSurfMesh::Node::index_type(static_cast<SCIRun::ImageMesh *>(meshH.get_rep()),p,r));
		q++;
	}
					
	return(true);
}

bool matlabconverter::createmesh(SCIRun::LockingHandle<SCIRun::StructHexVolMesh> &meshH,fieldstruct &fs)
{
	std::vector<long> dims;
	std::vector<unsigned int> mdims;
	long numdim = fs.x.getnumdims();
	dims = fs.x.getdims();
	
	mdims.resize(numdim);	
	for (long p=0; p < numdim; p++)  mdims[p] = static_cast<unsigned int>(dims[p]); 

	meshH = new SCIRun::StructHexVolMesh;
	long numnodes =fs.x.getnumelements();
	
	std::vector<double> X;
	std::vector<double> Y;
	std::vector<double> Z;
	fs.x.getnumericarray(X);
	fs.y.getnumericarray(Y);
	fs.z.getnumericarray(Z);
	
	meshH->set_dim(mdims);

	unsigned p,r,s,q;
	q= 0;
	for (s = 0; s < mdims[2]; s++)
	for (r = 0; r < mdims[1]; r++)
	for (p = 0; p < mdims[0]; p++)
	{
		meshH->set_point(SCIRun::Point(X[q],Y[q],Z[q]),SCIRun::StructHexVolMesh::Node::index_type(static_cast<SCIRun::LatVolMesh *>(meshH.get_rep()),p,r,s));
		q++;
	}
	
	return(true);
}


// This function generates a structure, so SCIRun can generate a temporaly file
// containing a call to the dynamic call, which can subsequently be compiled
// See the on-the-fly-libs directory for the files it generated
SCIRun::CompileInfoHandle
MatlabFieldReaderAlgo::get_compile_info(const SCIRun::TypeDescription *fieldTD)
{
  // use cc_to_h if this is in the .cc file, otherwise just __FILE__
  static const std::string include_path(SCIRun::TypeDescription::cc_to_h(__FILE__));
  static const std::string template_class_name("MatlabFieldReaderAlgoT");
  static const std::string base_class_name("MatlabFieldReaderAlgo");
  
  // Everything we did is in the MatlabIO namespace so we need to add this
  // Otherwise the dynamically code generator will not generate a "using namespace ...."
  static const std::string name_space_name("MatlabIO");

  // Supply the dynamic compiler with enough information to build a file in the
  // on-the-fly libs which will have the templated function in there
  SCIRun::CompileInfoHandle cinfo = 
    scinew SCIRun::CompileInfo(template_class_name + "." +
		       fieldTD->get_filename() + ".",
                       base_class_name, 
                       template_class_name, 
                       fieldTD->get_name());

  // Add in the include path to compile this obj
  // rval->add_include(include_path); // Added this through other means
  // Add the MatlabIO namespace
  cinfo->add_namespace(name_space_name);
  // Fill out any other default values
  // fieldTD->fill_compile_info(cinfo); This function does not serve any purpose in this case
  return(cinfo);
}











////////////// HERE CODE FOR DYNAMIC FIELDWRITER STARTS ///////////////


//// Field Writer templated and dynamic compiling
// These functions definesome predefined converter algorithms

// None of the mesh classes could easily be captured by any templated function
// Hence they all exist as overloaded functions on the templated class. 
// If a meshclass is used that is not listed here the templated class will be used,
// which will result in an error being returned by the function.
// All of the overloaded functions return a true, meaning they actually correspond to
// a mesh.
// Since mostmesh classes have a lot of overlap, components of the mesh translated are
// captured in templated functions which are being called by the mladdmesh() functions.
// These templated functions are compiled when SCIRun is compiled, hence a full library
// of mesh generation  functions should exist when the dynamic compiler is called. The
// latter will only generate code that combines calls to these mesh classes with the
// proper translation of the field. Hence there is a certain compromise between precompiled
// functions, which occupy disk space and having to deal with fully adding all the code to
// the on-the-fly-libs. The current compromise should minimise disk space usage and have
// fast dynamic compiles at the cost at a longer compilation time when SCIRun is first built.

bool matlabconverter::mladdmesh(SCIRun::LockingHandle<SCIRun::TetVolMesh> meshH,matlabarray mlarray)
{
	mladdnodesfield(meshH,mlarray);
	mladdcellsfield(meshH,mlarray,4);
	mladdmeshclass("tetvol",mlarray);
	return(true);
}

bool matlabconverter::mladdmesh(SCIRun::LockingHandle<SCIRun::HexVolMesh> meshH,matlabarray mlarray)
{
	mladdnodesfield(meshH,mlarray);
	mladdcellsfield(meshH,mlarray,8);
	mladdmeshclass("hexvol",mlarray);
	return(true);
}

bool matlabconverter::mladdmesh(SCIRun::LockingHandle<SCIRun::PrismVolMesh> meshH,matlabarray mlarray)
{
	mladdnodesfield(meshH,mlarray);
	mladdcellsfield(meshH,mlarray,6);
	mladdmeshclass("prismvol",mlarray);
	return(true);
}

bool matlabconverter::mladdmesh(SCIRun::LockingHandle<SCIRun::QuadSurfMesh> meshH,matlabarray mlarray)
{
	mladdnodesfield(meshH,mlarray);
	mladdfacesfield(meshH,mlarray,4);
	mladdmeshclass("quadsurf",mlarray);
	return(true);
}

bool matlabconverter::mladdmesh(SCIRun::LockingHandle<SCIRun::TriSurfMesh> meshH,matlabarray mlarray)
{
	mladdnodesfield(meshH,mlarray);
	mladdfacesfield(meshH,mlarray,3);
	mladdmeshclass("trisurf",mlarray);
	return(true);
}

bool matlabconverter::mladdmesh(SCIRun::LockingHandle<SCIRun::CurveMesh> meshH,matlabarray mlarray)
{
	mladdnodesfield(meshH,mlarray);
	mladdedgesfield(meshH,mlarray,2);
	mladdmeshclass("curve",mlarray);
	return(true);
}

bool matlabconverter::mladdmesh(SCIRun::LockingHandle<SCIRun::PointCloudMesh> meshH,matlabarray mlarray)
{
	mladdnodesfield(meshH,mlarray);
	mladdmeshclass("pointcloud",mlarray);
	return(true);
}

bool matlabconverter::mladdmesh(SCIRun::LockingHandle<SCIRun::StructHexVolMesh> meshH,matlabarray mlarray)
{
	mladdxyznodes(meshH,mlarray);
	mladdmeshclass("structhexvol",mlarray);
	return(true);
}

bool matlabconverter::mladdmesh(SCIRun::LockingHandle<SCIRun::StructQuadSurfMesh> meshH,matlabarray mlarray)
{
	mladdxyznodes(meshH,mlarray);
	mladdmeshclass("structquadsurf",mlarray);
	return(true);
}

bool matlabconverter::mladdmesh(SCIRun::LockingHandle<SCIRun::StructCurveMesh> meshH,matlabarray mlarray)
{
	mladdxyznodes(meshH,mlarray);
	mladdmeshclass("structcurve",mlarray);
	return(true);
}

bool matlabconverter::mladdmesh(SCIRun::LockingHandle<SCIRun::ScanlineMesh> meshH,matlabarray mlarray)
{
	mladdmeshclass("scanline",mlarray);
	return(true);
}

bool matlabconverter::mladdmesh(SCIRun::LockingHandle<SCIRun::ImageMesh> meshH,matlabarray mlarray)
{
	mladdmeshclass("image",mlarray);
	return(true);
}

bool matlabconverter::mladdmesh(SCIRun::LockingHandle<SCIRun::LatVolMesh> meshH,matlabarray mlarray)
{
	mladdmeshclass("latvol",mlarray);
	return(true);
}

// ADD SUPPORT FOR DIFFERENT MESH CLASSES HERE
// (see examples above)
// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

// For convienence every mesh tyoe is listed as a string array in the final matlabarray.
// Hence the user can check whether the expected mesh class was used
// The following function just attaches this information onto the mlarray
void matlabconverter::mladdmeshclass(std::string meshclass,matlabarray mlarray)
{
	matlabarray meshcls;
	meshcls.createstringarray(meshclass);
	mlarray.setfield(0,"meshclass",meshcls);
}

// A couple of specialised functions for structured grids
// They use different iterators etc. hence they need a separate treatment

void matlabconverter::mladdxyznodes(SCIRun::LockingHandle<SCIRun::StructCurveMesh> meshH,matlabarray mlarray)
{
	matlabarray x,y,z;
	
	meshH->synchronize(SCIRun::Mesh::NODES_E);
	SCIRun::StructCurveMesh::Node::size_type size;
	meshH->size(size);
	unsigned int numnodes = static_cast<unsigned int>(size);
	x.createdensearray(numnodes,1,matlabarray::miDOUBLE);
	y.createdensearray(numnodes,1,matlabarray::miDOUBLE);
	z.createdensearray(numnodes,1,matlabarray::miDOUBLE);
	
	std::vector<double> xbuffer(numnodes);
	std::vector<double> ybuffer(numnodes);
	std::vector<double> zbuffer(numnodes);
	
	SCIRun::Point P;
	for (unsigned int p = 0; p < numnodes ; p++)
	{
		meshH->get_point(P,SCIRun::StructCurveMesh::Node::index_type(p));
		xbuffer[p] = P.x();
		ybuffer[p] = P.y();
		zbuffer[p] = P.z();
	}
	
	x.setnumericarray(xbuffer);
	y.setnumericarray(ybuffer);
	z.setnumericarray(zbuffer);
	
	mlarray.setfield(0,"x",x);
	mlarray.setfield(0,"y",y);
	mlarray.setfield(0,"z",z);
}

void matlabconverter::mladdxyznodes(SCIRun::LockingHandle<SCIRun::StructQuadSurfMesh> meshH,matlabarray mlarray)
{
	matlabarray x,y,z;
	
	meshH->synchronize(SCIRun::Mesh::NODES_E);
	// Get the dimensions of the mesh
	unsigned int dim1 = static_cast<unsigned int>(meshH->get_ni());
	unsigned int dim2 = static_cast<unsigned int>(meshH->get_nj());
	unsigned int numnodes = dim1*dim2;
	
	// Note: the dimensions are in reverse order as SCIRun uses C++
	// ordering
	x.createdensearray(dim2,dim1,matlabarray::miDOUBLE);
	y.createdensearray(dim2,dim1,matlabarray::miDOUBLE);
	z.createdensearray(dim2,dim1,matlabarray::miDOUBLE);
	
	// We use temp buffers to store all the values before committing them to the matlab
	// classes, this takes up more memory, but should decrease the number of actual function
	// calls, which should be boost performance 
	std::vector<double> xbuffer(numnodes);
	std::vector<double> ybuffer(numnodes);
	std::vector<double> zbuffer(numnodes);
	
	SCIRun::Point P;
	unsigned int r = 0;
	for (unsigned int p = 0; p < dim1 ; p++)
		for (unsigned int q = 0; q < dim2 ; q++)
		{   
			meshH->get_point(P,SCIRun::StructQuadSurfMesh::Node::index_type(static_cast<SCIRun::ImageMesh *>(meshH.get_rep()),p,q));
			xbuffer[r] = P.x();
			ybuffer[r] = P.y();
			zbuffer[r] = P.z();
			r++;
		}

	// upload all the buffers to matlab
	x.setnumericarray(xbuffer);
	y.setnumericarray(ybuffer);
	z.setnumericarray(zbuffer);
		
	mlarray.setfield(0,"x",x);
	mlarray.setfield(0,"y",y);
	mlarray.setfield(0,"z",z);
}

// StructhexVolMesh is to specialized to fit in any template

void matlabconverter::mladdxyznodes(SCIRun::LockingHandle<SCIRun::StructHexVolMesh> meshH,matlabarray mlarray)
{
	matlabarray x,y,z;
	
	meshH->synchronize(SCIRun::Mesh::NODES_E);
	// get the dimensions of the mesh
	unsigned int dim1 = static_cast<unsigned int>(meshH->get_ni());
	unsigned int dim2 = static_cast<unsigned int>(meshH->get_nj());
	unsigned int dim3 = static_cast<unsigned int>(meshH->get_nk());

	unsigned int numnodes = dim1*dim2*dim3;
	
	// Note: the dimensions are in reverse order as SCIRun uses C++
	// ordering
	std::vector<long> dims(3); dims[0] = dim3; dims[1] = dim2; dims[2] = dim1;
	x.createdensearray(dims,matlabarray::miDOUBLE);
	y.createdensearray(dims,matlabarray::miDOUBLE);
	z.createdensearray(dims,matlabarray::miDOUBLE);
	
	// We use temp buffers to store all the values before committing them to the matlab
	// classes, this takes up more memory, but should decrease the number of actual function
	// calls, which should be boost performance 
	std::vector<double> xbuffer(numnodes);
	std::vector<double> ybuffer(numnodes);
	std::vector<double> zbuffer(numnodes);
	
	SCIRun::Point P;
	unsigned int r = 0;
	for (unsigned int p = 0; p < dim1 ; p++)
		for (unsigned int q = 0; q < dim2 ; q++)
			for (unsigned int s = 0; s < dim3 ; s++)
			{   
				meshH->get_point(P,SCIRun::StructHexVolMesh::Node::index_type(static_cast<SCIRun::LatVolMesh *>(meshH.get_rep()),p,q,s));
				xbuffer[r] = P.x();
				ybuffer[r] = P.y();
				zbuffer[r] = P.z();
				r++;
			}

	// upload all the buffers to matlab
	x.setnumericarray(xbuffer);
	y.setnumericarray(ybuffer);
	z.setnumericarray(zbuffer);
		
	mlarray.setfield(0,"x",x);
	mlarray.setfield(0,"y",y);
	mlarray.setfield(0,"z",z);	
}

// Vectors and Tensors cannot be translated by the templated functions in the matlabarray class.
// This class was developed outside of SCIRun and does not recognize any of the 
// SCIRun special classes. Hence a overloaded version of the mladdfield function is
// given for both these objects

bool matlabconverter::mladdfield(std::vector<SCIRun::Vector> &fdata,matlabarray mlarray)
{
	matlabarray field;
	matlabarray fieldtype;

	fieldtype.createstringarray("vector");
	field.createdensearray(3,fdata.size(),matlabarray::miDOUBLE);
		
	unsigned int size = fdata.size();
	unsigned int p,q;
	std::vector<double> data(size*3);
	for (p = 0, q = 0; p < size; p++) 
	{ 
		data[q++] = fdata[p][0];
		data[q++] = fdata[p][1];
		data[q++] = fdata[p][2];
	}		
	field.setnumericarray(data);		
	mlarray.setfield(0,"field",field);
	mlarray.setfield(0,"fieldtype",fieldtype);
	return(true);
}

bool matlabconverter::mladdfield(std::vector<SCIRun::Tensor> &fdata,matlabarray mlarray)
{
	matlabarray field;
	matlabarray fieldtype;

	fieldtype.createstringarray("tensor");
	field.createdensearray(9,fdata.size(),matlabarray::miDOUBLE);
	
	unsigned int size = fdata.size();
	unsigned int p,q;
	std::vector<double> data(size*9);
	for (p = 0, q = 0; p < size; p++) 
	{   // write an uncompressed tensor by default
		data[q++] = fdata[p].mat_[0][0];
		data[q++] = fdata[p].mat_[0][1];
		data[q++] = fdata[p].mat_[0][2];
		data[q++] = fdata[p].mat_[1][0];
		data[q++] = fdata[p].mat_[1][1];
		data[q++] = fdata[p].mat_[1][2];
		data[q++] = fdata[p].mat_[2][0];
		data[q++] = fdata[p].mat_[2][1];
		data[q++] = fdata[p].mat_[2][2];
	}		
	field.setnumericarray(data);		
	mlarray.setfield(0,"field",field);
	mlarray.setfield(0,"fieldtype",fieldtype);
	return(true);	
}


// Vectors cannot be translated by the templated functions in the matlabarray class.
// This class was developed outside of SCIRun and does not recognize any of the 
// SCIRun special classes. Hence a overloaded version of the mladdfield function is
// given for both FData2d and FData3d. The Vectors SCIRun uses are not STL objects
// and are not recognized by any of the matlabclasses

bool mladdfield(SCIRun::FData2d<SCIRun::Vector> &fdata,matlabarray mlarray)
{
	matlabarray field;
	matlabarray fieldtype;

	fieldtype.createstringarray("vector");
	
	std::vector<long> dims(2);
	// Note: the dimensions are in reverse order as SCIRun uses C++
	// ordering
	dims[0] = fdata.dim2(); dims[1] = fdata.dim1();
	field.createdensearray(dims,matlabarray::miDOUBLE);
		
	unsigned int size = fdata.size();
	unsigned int p,q,r;
	unsigned int dim1 = fdata.dim1();
	unsigned int dim2 = fdata.dim2();
	SCIRun::FData2d<SCIRun::Vector>::value_type **dataptr = fdata.get_dataptr();
	
	std::vector<double> data(dim1*dim2*3);
	for (p=0,q=0;q<dim1;q++)
		for (r=0;r<dim2;r++)
		{
			data[p++] = dataptr[q][r][0];
			data[p++] = dataptr[q][r][1];
			data[p++] = dataptr[q][r][2];
		}		
	
	field.setnumericarray(data);		
	mlarray.setfield(0,"field",field);
	mlarray.setfield(0,"fieldtype",fieldtype);
	return(true);
}	

bool mladdfield(SCIRun::FData3d<SCIRun::Vector> &fdata,matlabarray mlarray)
{
	matlabarray field;
	matlabarray fieldtype;

	fieldtype.createstringarray("vector");
	
	std::vector<long> dims(3);
	// Note: the dimensions are in reverse order as SCIRun uses C++
	// ordering
	dims[0] = fdata.dim3(); dims[1] = fdata.dim2(); dims[2] = fdata.dim1();
	field.createdensearray(dims,matlabarray::miDOUBLE);
		
	unsigned int size = fdata.size();
	unsigned int p,q,r,s;

	unsigned int dim1 = fdata.dim1();
	unsigned int dim2 = fdata.dim2();
	unsigned int dim3 = fdata.dim3();

	SCIRun::FData3d<SCIRun::Vector>::value_type ***dataptr = fdata.get_dataptr();

	std::vector<double> data(dim1*dim2*dim3*3);
	for (p=0,q=0;q<dim1;q++)
		for (r=0;r<dim2;r++)
			for (s=0;s<dim3;s++)
			{
				data[p++] = dataptr[q][r][s][0];
				data[p++] = dataptr[q][r][s][1];
				data[p++] = dataptr[q][r][s][2];
			}		

	field.setnumericarray(data);		
	mlarray.setfield(0,"field",field);
	mlarray.setfield(0,"fieldtype",fieldtype);
	return(true);
}	


// Tensors cannot be translated by the templated functions in the matlabarray class.
// This class was developed outside of SCIRun and does not recognize any of the 
// SCIRun special classes. Hence a overloaded version of the mladdfield function is
// given for both FData2d and FData3d

bool mladdfield(SCIRun::FData2d<SCIRun::Tensor> &fdata,matlabarray mlarray)
{
	matlabarray field;
	matlabarray fieldtype;

	fieldtype.createstringarray("tensor");
	
	std::vector<long> dims(2);
	// Note: the dimensions are in reverse order as SCIRun uses C++
	// ordering
	dims[0] = fdata.dim2(); dims[1] = fdata.dim1();
	field.createdensearray(dims,matlabarray::miDOUBLE);
		
	unsigned int size = fdata.size();
	unsigned int p,q,r;
	unsigned int dim1 = fdata.dim1();
	unsigned int dim2 = fdata.dim2();

	SCIRun::FData2d<SCIRun::Tensor>::value_type **dataptr = fdata.get_dataptr();

	std::vector<double> data(dim1*dim2*9);
	for (p=0,q=0;q<dim1;q++)
		for (r=0;r<dim2;r++)
		{
			data[p++] = dataptr[q][r].mat_[0][0];
			data[p++] = dataptr[q][r].mat_[0][1];
			data[p++] = dataptr[q][r].mat_[0][2];
			data[p++] = dataptr[q][r].mat_[1][0];
			data[p++] = dataptr[q][r].mat_[1][1];
			data[p++] = dataptr[q][r].mat_[1][2];
			data[p++] = dataptr[q][r].mat_[2][0];
			data[p++] = dataptr[q][r].mat_[2][1];
			data[p++] = dataptr[q][r].mat_[2][2];
		}		
	
	field.setnumericarray(data);		
	mlarray.setfield(0,"field",field);
	mlarray.setfield(0,"fieldtype",fieldtype);
	return(true);
}

bool mladdfield(SCIRun::FData3d<SCIRun::Tensor> &fdata,matlabarray mlarray)
{
	matlabarray field;
	matlabarray fieldtype;

	fieldtype.createstringarray("tensor");
	
	std::vector<long> dims(3);
	// Note: the dimensions are in reverse order as SCIRun uses C++
	// ordering	
	dims[0] = fdata.dim3(); dims[1] = fdata.dim2(); dims[2] = fdata.dim1();
	field.createdensearray(dims,matlabarray::miDOUBLE);
		
	unsigned int size = fdata.size();
	unsigned int p,q,r,s;
	unsigned int dim1 = fdata.dim1();
	unsigned int dim2 = fdata.dim2();
	unsigned int dim3 = fdata.dim3();

	SCIRun::FData3d<SCIRun::Tensor>::value_type ***dataptr = fdata.get_dataptr();
	
	std::vector<double> data(dim1*dim2*dim3*9);
	for (p=0,q=0;q<dim1;q++)
		for (r=0;r<dim2;r++)
			for (s=0;s<dim3;s++)
			{
				data[p++] = dataptr[q][r][s].mat_[0][0];
				data[p++] = dataptr[q][r][s].mat_[0][1];
				data[p++] = dataptr[q][r][s].mat_[0][2];
				data[p++] = dataptr[q][r][s].mat_[1][0];
				data[p++] = dataptr[q][r][s].mat_[1][1];
				data[p++] = dataptr[q][r][s].mat_[1][2];
				data[p++] = dataptr[q][r][s].mat_[2][0];
				data[p++] = dataptr[q][r][s].mat_[2][1];
				data[p++] = dataptr[q][r][s].mat_[2][2];
			}		

	field.setnumericarray(data);		
	mlarray.setfield(0,"field",field);
	mlarray.setfield(0,"fieldtype",fieldtype);
	return(true);
}


// This function generates a structure, so SCIRun can generate a temporaly file
// containing a call to the dynamic call, which can subsequently be compiled
// See the on-the-fly-libs directory for the files it generated
SCIRun::CompileInfoHandle
MatlabFieldWriterAlgo::get_compile_info(const SCIRun::TypeDescription *fieldTD)
{
  // use cc_to_h if this is in the .cc file, otherwise just __FILE__
  static const std::string include_path(SCIRun::TypeDescription::cc_to_h(__FILE__));
  static const std::string template_class_name("MatlabFieldWriterAlgoT");
  static const std::string base_class_name("MatlabFieldWriterAlgo");
  
  // Everything we did is in the MatlabIO namespace so we need to add this
  // Otherwise the dynamically code generator will not generate a "using namespace ...."
  static const std::string name_space_name("MatlabIO");

  // Supply the dynamic compiler with enough information to build a file in the
  // on-the-fly libs which will have the templated function in there
  std::string filename = template_class_name + "." + fieldTD->get_filename() + ".";
  SCIRun::CompileInfo *rval = 
    scinew SCIRun::CompileInfo(filename,
                       base_class_name, 
                       template_class_name, 
                       fieldTD->get_name());

  // Add in the include path to compile this obj
   rval->add_include(include_path); 
  // Add the MatlabIO namespace
  rval->add_namespace(name_space_name);
  // Fill out any other default values
  fieldTD->fill_compile_info(rval);
  return(rval);
}

// This function invokes the sciField to matlab array converter, the top function
// does deal with the dynamic compilation and assures that the module using this
// functionality gets called to report compilation progress and as well reports
// errors directly to the user

void matlabconverter::sciFieldTOmlArray(SCIRun::FieldHandle &scifield,matlabarray &mlarray,SCIRun::Module *module)
{
	// Get the type information of the field for which we have to compile a converter
	const SCIRun::TypeDescription *fieldTD = scifield->get_type_description();
	// Get all the filenames etc.
	SCIRun::CompileInfoHandle cinfo = MatlabFieldWriterAlgo::get_compile_info(fieldTD);
	// A placeholder for the dynamic code
	SCIRun::Handle<MatlabFieldWriterAlgo> algo;
	// Do the magic, internally algo will now refer to the proper dynamic class, which will be
	// loaded by this function as well
	if (!module->module_dynamic_compile(cinfo, algo)) 
	{
		// Dynamic compilation failed
		module->error("matlabconverter::sciFieldTOmlArray: Dynamic compilation failed\n");
		return;
	}
	// The function takes the matlabconverter pointer again, which we need to re-enter the object, which we will
	// leave in the dynamic compilation. The later was done to assure small filenames in the on-the-fly libs
	// Filenames over 31 chars will cause problems on certain systems
	// Since the matlabconverter holds all our converter settings, we don't want to lose it, hence it is added
	// here. The only disadvantage is that some of the functions in the matlabconverter class nedd to be public

	// create a new structured matlab array
	mlarray.createstructarray();
	
	if(!(algo->execute(scifield,mlarray,*this)))
	{
		// The algorithm has an builtin sanity check. If a specific converter cannot be built
		// it will create an algorithm that returns a false. Hence instead of failing at the
		// compiler level a proper description will be issued to the user of the module
		mlarray.clear();
		module->error("matlabconverter::sciFieldTOmlArray: The dynamically compiled matlabconverter does not function properly; most probably some specific mesh or field converters are missing or have not yet been implemented\n");
		return;
	}
	if (mlarray.isempty())
	{
		// Apparently my sanity check did not work, we did not get a matlab object
		module->error("matlabconverter::sciFieldTOmlArray: Converter did not result in a useful translation, something went wrong, giving up\n");
		return;
	}
	
	// This code is not the most efficient one: we first create the complete structured one and then
	// strip out one field. But it seems to be the easiest one.
	
	if (numericarray_ == true)
	{
		if (mlarray.isfield("field"))
		{
			// strip the whole data structure
			mlarray = mlarray.getfield(0,"field");
			return; // leave since we are no longer dealing with a struct
		}
	}
	
	// add the properties
	if (scifield != 0)
	{
		sciPropertyTOmlProperty(static_cast<SCIRun::PropertyManager *>(scifield.get_rep()),mlarray);
	}
	
	// Parse the namefield separately
	if (scifield->is_property("name"))
	{
		std::string name;
		matlabarray namearray;
		scifield->get_property("name",name);
		namearray.createstringarray(name);
		mlarray.setfield(0,"name",namearray);
	}
	
}

// Bundle code
#ifdef HAVE_BUNDLE

long matlabconverter::sciBundleCompatible(matlabarray &mlarray, std::string &infostring, SCIRun::Module *module)
{
	if (!mlarray.isstruct()) return(0);
	if (mlarray.getnumelements()==0) return(0);
	long numfields = mlarray.getnumfields();
	
}

void matlabconverter::mlArrayTOsciBundle(matlabarray &mlmat,CardioWave::BundleHandle &scibundle, SCIRun::Module *module)
{

}

void matlabconverter::sciBundleTOmlArray(CardioWave::BundleHandle &scibundle, matlabarray &mlmat,SCIRun::Module *module)
{

}

#endif


// FUNCTIONS FOR RETURNING MESSAGES TO THE USER FOR DEBUGGING PURPOSES

void	matlabconverter::postmsg(SCIRun::Module *ptr,std::string msg)
{
	if (ptr)
	{
		if (postmsg_) ptr->remark(msg);
	}
}

void	matlabconverter::setpostmsg(bool val)
{
	postmsg_ = val;
}

} // end namespace


