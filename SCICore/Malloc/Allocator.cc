//static char *id="@(#) $Id$";

/*
 *  Allocator.cc: ?
 *
 *  Written by:
 *   Author: ?
 *   Department of Computer Science
 *   University of Utah
 *   Date: ?
 *
 *  Copyright (C) 199? SCI Group
 */

/* TODO:
5) Printout of leaks
6) Destroy allocators
*/

#include <SCICore/Malloc/Allocator.h>
#include <SCICore/Malloc/AllocPriv.h>
#include <SCICore/Malloc/AllocOS.h>

// irix64 KCC stuff
#include <strings.h>

#ifdef __sun
  #include <string.h>
  #define bcopy(src,dest,n) memcpy(dest,src,n)
#else
  #ifndef __linux
    #include <bstring.h>
  #endif
#endif

#include <stdio.h>
#include <sci_config.h>

#ifdef SCI_PTHREAD
#include <pthread.h>
#endif

extern "C" void audit() {
    using namespace SCICore::Malloc;
    AuditAllocator(DefaultAllocator());
    fprintf(stderr, "Memory audit OK\n");
}

namespace SCICore {
namespace Malloc {

Allocator* default_allocator;

// Granularity of small things - 8 bytes
// Anything smaller than this is considered "small"
#define SMALL_THRESHOLD (512-8)
#define SMALLEST_ALLOCSIZE (8*1024)

#define SMALL_BIN(size) (((size)+7)>>4)
#define NSMALL_BINS ((SMALL_THRESHOLD+8)>>4)
#define SMALL_BINSIZE(bin) (((bin)<<4)+8)

// Granularity of medium things - 2k bytes
#define MEDIUM_THRESHOLD (65536)

#define MEDIUM_BIN(size) (((size)-1)>>11)
#define NMEDIUM_BINS ((MEDIUM_THRESHOLD)>>11)
#define MEDIUM_BINSIZE(bin) (((bin)<<11)+2048)

#define OVERHEAD (sizeof(Tag)+sizeof(Sentinel)+sizeof(Sentinel))

#define OBJFREE 1
#define OBJINUSE 2
#define OBJFREEING 3
#define OBJMEMALIGNFREEING 4

#define SENT_VAL_FREE 0xdeadbeef
#define SENT_VAL_INUSE 0xbeefface

#define NORMAL_OS_ALLOC_SIZE (512*1024)

// Objects bigger than this can't be allocated
#define MAX_ALLOCSIZE (1024*1024*1024)

inline size_t Allocator::obj_maxsize(Tag* t)
{
    return (t->bin == &big_bin)?(t->hunk->len-OVERHEAD):t->bin->maxsize;
}

static void account_bin(Allocator* a, AllocBin* bin, FILE* out,
			size_t& bytes_overhead,
			size_t& bytes_free,
			size_t& bytes_fragmented,
			size_t& bytes_inuse)
{
    Tag* p;
    for(p=bin->free;p!=0;p=p->next){
	bytes_overhead+=OVERHEAD;
	bytes_free+=a->obj_maxsize(p);
    }
    for(p=bin->inuse;p!=0;p=p->next){
	bytes_overhead+=OVERHEAD;
	bytes_inuse+=p->reqsize;
	bytes_fragmented+=a->obj_maxsize(p)-p->reqsize;
	if(out){
	    fprintf(out, "%p: %d bytes (%s)\n", p+sizeof(Tag)+sizeof(Sentinel),
		    p->reqsize, p->tag);
	}
    }
}

static void shutdown()
{
    Allocator* a=DefaultAllocator();
    if(a->stats_out){
	// Just in case...
	a->lock();

	fprintf(a->stats_out, "Unfreed objects:\n");
	// Full accounting - go through each bin...
	size_t bytes_overhead=0, bytes_free=0, bytes_fragmented=0,
	    bytes_inuse=0, bytes_inhunks=0;
	int i;
	for(i=0;i<NSMALL_BINS;i++)
	    account_bin(a, &a->small_bins[i], a->stats_out, bytes_overhead,
			bytes_free, bytes_fragmented, bytes_inuse);
	for(i=0;i<NMEDIUM_BINS;i++)
	    account_bin(a, &a->medium_bins[i], a->stats_out, bytes_overhead,
			bytes_free, bytes_fragmented, bytes_inuse);
	account_bin(a, &a->big_bin, a->stats_out, bytes_overhead, bytes_free,
		    bytes_fragmented, bytes_inuse);

	// Count hunks...
	for(OSHunk* hunk=a->hunks;hunk!=0;hunk=hunk->next){
	    bytes_overhead+=sizeof(OSHunk);
	    bytes_inhunks+=hunk->spaceleft;
	}
	// And the ones in the bigbin...
	Tag* p;
	for(p=a->big_bin.free;p!=0;p=p->next)
	    bytes_overhead+=sizeof(OSHunk);
	for(p=a->big_bin.inuse;p!=0;p=p->next)
	    bytes_overhead+=sizeof(OSHunk);
	bytes_overhead+=a->mysize;
	if(bytes_inuse == 0)
	    fprintf(a->stats_out, "None\n");

        fprintf(a->stats_out, "statistics:\n");
	fprintf(a->stats_out, "alloc:\t\t\t%d calls\n", a->nalloc);
	fprintf(a->stats_out, "alloc:\t\t\t%d bytes\n", a->sizealloc);
	fprintf(a->stats_out, "free:\t\t\t%d calls\n", a->nfree);
	fprintf(a->stats_out, "free:\t\t\t%d bytes\n", a->sizefree);
	fprintf(a->stats_out, "fillbin:\t\t%d calls\n", a->nfillbin);
	fprintf(a->stats_out, "mmap:\t\t\t%d calls\n", a->nmmap);
	fprintf(a->stats_out, "mmap:\t\t\t%d bytes\n", a->sizemmap);
	fprintf(a->stats_out, "munmap:\t\t\t%d calls\n", a->nmunmap);
	fprintf(a->stats_out, "munmap:\t\t\t%d bytes\n", a->sizemunmap);
	fprintf(a->stats_out, "highwater alloc:\t%d bytes\n", a->highwater_alloc);
	fprintf(a->stats_out, "highwater mmap:\t\t%d bytes\n", a->highwater_mmap);
	fprintf(a->stats_out, "long locks:\t\t%d times\n", a->nlonglocks);
	fprintf(a->stats_out, "naps:\t\t\t%d times\n", a->nnaps);
	fprintf(a->stats_out, "\n");
	fprintf(a->stats_out, "breakdown of total bytes:\n");	
	fprintf(a->stats_out, "in use:\t\t\t%d bytes\n", bytes_inuse);
	fprintf(a->stats_out, "free:\t\t\t%d bytes\n", bytes_free);
	fprintf(a->stats_out, "fragmentation:\t\t%d bytes\n", bytes_fragmented);
	fprintf(a->stats_out, "left in mmap hunks:\t%d\n", bytes_inhunks);
	fprintf(a->stats_out, "per object overhead:\t%d bytes\n", bytes_overhead);
	fprintf(a->stats_out, "\n");
	fprintf(a->stats_out, "%d bytes missing (%d memory objects)\n",
		a->sizealloc-a->sizefree, a->nalloc-a->nfree);

	if(a->stats_out != stderr)
	    fclose(a->stats_out);
	a->unlock();
    }
}

inline AllocBin* Allocator::get_bin(size_t size)
{
    if(size <= SMALL_THRESHOLD){
	int bin=SMALL_BIN(size);
	return &small_bins[bin];
    } else if(size <= MEDIUM_THRESHOLD){
	int bin=MEDIUM_BIN(size);
	return &medium_bins[bin];
    } else {
	return &big_bin;
    }
}

#ifdef SCI_PTHREAD

pthread_mutex_t thelock = PTHREAD_MUTEX_INITIALIZER;

void Allocator::initlock()
{
}

inline void Allocator::lock()
{
  if(pthread_mutex_lock(&thelock) != 0){
    perror("pthread_mutex_lock");
    exit(-1);
  }
}

inline void Allocator::unlock()
{
  if(pthread_mutex_unlock(&thelock) != 0){
    perror("pthread_mutex_lock");
    exit(-1);
  }
}

#else

#ifdef __sgi

extern "C" int Allocator_try_lock(unsigned int*);

#include <unistd.h>

void Allocator::initlock()
{
    the_lock=0;
}


inline void Allocator::lock()
{
    // Simple try first...
    if(Allocator_try_lock(&the_lock) != 0)
       longlock();
}

void Allocator::longlock()
{
    int rt=1000000;
    nlonglocks++;
    while(rt){
	// Try the lock...
	for(int i=0;i<10;i++){
	    if(Allocator_try_lock(&the_lock) == 0)
		return;
	}
	// Sleep...
	sginap(0);
	nnaps++;

	rt--;
    }
    AllocError("Possible deadlock after 1000000 retries\n");
}

inline void Allocator::unlock()
{
    the_lock=0;
}

#endif
#ifdef __linux

extern inline char tas(char* m)
{
    char res;

    __asm__("xchgb %0,%1":"=q" (res),"=m" (*m):"0" (0x1));
    return res;
}

void Allocator::initlock()
{
    the_lock=0;
}


inline void Allocator::lock()
{
    // Simple try first...
    if(tas((char*)&the_lock) != 0)
       longlock();
}

static void (*sleeper)();

void Allocator::longlock()
{
    int rt=1000000;
    nlonglocks++;
    while(rt){
	// Try the lock...
	for(int i=0;i<10;i++){
	    if(tas((char*)&the_lock) == 0)
		return;
	}
	// Sleep...
	if(sleeper)
	    (*sleeper)();
	nnaps++;

	rt--;
    }
    AllocError("Possible deadlock after 1000000 retries\n");
}

inline void Allocator::unlock()
{
    the_lock=0;
}

void SetAllocSleeper(void (*s)())
{
    sleeper=s;
}

#endif
#endif

void MakeDefaultAllocator()
{
    if(!default_allocator){
	default_allocator=MakeAllocator();
    }
}

void AllocError(char* msg)
{
    fprintf(stderr, "Allocator error: %s\n", msg);
    abort();
}

Allocator* MakeAllocator()
{
    // Compute the size of the allocator structures
    size_t size=sizeof(Allocator);
    int nsmall=NSMALL_BINS;
    size+=nsmall*sizeof(AllocBin);
    int nmedium=NMEDIUM_BINS;
    size+=nmedium*sizeof(AllocBin);

    OSHunk* alloc_hunk=OSHunk::alloc(size);
    Allocator* a=(Allocator*)alloc_hunk->data;
    alloc_hunk->spaceleft=0;
    alloc_hunk->next=0;
    alloc_hunk->ninuse=1;

    a->hunks=alloc_hunk;
    a->small_bins=(AllocBin*)(a+1);
    a->mysize=size;
    // Fill in the small bin info...
    for(int j=0;j<NSMALL_BINS;j++){
	int minsize=j==0?0:SMALL_BINSIZE(j-1)+1;
	a->init_bin(&a->small_bins[j], SMALL_BINSIZE(j), minsize);
    }
    a->medium_bins=a->small_bins+nsmall;
    for(int i=0;i<NMEDIUM_BINS;i++){
	int minsize=i==0?SMALL_THRESHOLD+1:MEDIUM_BINSIZE(i-1)+1;
	a->init_bin(&a->medium_bins[i], MEDIUM_BINSIZE(i), minsize);
    }
    a->init_bin(&a->big_bin, MAX_ALLOCSIZE, MEDIUM_THRESHOLD+1);


    // See if we are in strict mode
    if(getenv("MALLOC_STRICT")){
	a->strict=1;
    } else {
	a->strict=0;
    }

    if(getenv("MALLOC_LAZY")){
	a->lazy=1;
    } else {
	a->lazy=0;
    }

    // Initialize stats...
    a->nmmap=1;
    a->sizemmap=size+sizeof(OSHunk);
    a->highwater_mmap=size;
    a->nalloc=a->nfree=a->sizealloc=a->sizefree=0;
    a->nlonglocks=a->nnaps=0;
    a->nfillbin=0;
    a->nmunmap=a->sizemunmap=0;
    a->highwater_alloc=0;

    // Setup the lock...
    a->initlock();
    if(getenv("MALLOC_STATS")){
	// Set the default allocator, since the fopen below may
	// call malloc.
	if(!default_allocator)
	    default_allocator=a;
	char* file=getenv("MALLOC_STATS");
	if(strlen(file) == 0){
	    a->stats_out=stderr;
	} else {
	    a->stats_out=fopen(file, "w");
	    setvbuf(a->stats_out, (char*)a->alloc(4096+BUFSIZ, "Malloc stats buffer"), _IOFBF, 4096+BUFSIZ);
	    if(!a->stats_out){
		perror("fopen");
		fprintf(stderr, "cannot open stats file: %s, will not print stats\n",
			file);
		a->stats_out=0;
	    }
	}
	if(a->stats_out){
	    atexit(shutdown);
	}
    } else {
	a->stats_out=0;
    }
    if(getenv("MALLOC_TRACE")){
	// Set the default allocator, since the fopen below may
	// call malloc.
	if(!default_allocator)
	    default_allocator=a;
	char* file=getenv("MALLOC_TRACE");
	if(strlen(file) == 0){
	    a->trace_out=stderr;
	} else {
	    a->trace_out=fopen(file, "w");
	    setvbuf(a->trace_out, (char*)a->alloc(4096+BUFSIZ, "Malloc trace buffer"), _IOFBF, 4096+BUFSIZ);
	    if(!a->trace_out){
		perror("fopen");
		fprintf(stderr, "cannot open trace file: %s, not tracing\n",
			file);
		a->trace_out=0;
	    }
	}
	if(a->trace_out){
	    if(!a->stats_out){
		a->stats_out=a->trace_out;
		atexit(shutdown);
	    }
	}
    } else {
	a->trace_out=0;
    }

    return a;
}

void* Allocator::alloc(size_t size, const char* tag)
{
    if(size > MEDIUM_THRESHOLD)
	return alloc_big(size, tag);
    if(size == 0)
	return 0;

    // Find a block that this will fit in...
    AllocBin* obj_bin=get_bin(size);
#ifndef DEBUG
    if(obj_bin->maxsize < size || size < obj_bin->minsize){
	fprintf(stderr, "maxsize: %d\n", obj_bin->maxsize);
	fprintf(stderr, "size: %d\n", size);
	AllocError("Bins messed up...");
    }
#endif
    lock();

    if(!obj_bin->free)
	fill_bin(obj_bin);
    Tag* obj=obj_bin->free;
    obj_bin->free=obj->next;
    if(obj_bin->free)
	obj_bin->free->prev=0;

    // Tell the hunk that we are using this one...
    obj->hunk->ninuse++;
    obj->tag=tag;
    obj->next=obj_bin->inuse;
    if(obj_bin->inuse)
	obj_bin->inuse->prev=obj;
    obj->prev=0;
    obj_bin->inuse=obj;
    obj->reqsize=size;
    obj_bin->ninuse++;

    nalloc++;
    sizealloc+=size;
    size_t bytes_inuse=sizealloc-sizefree;
    if(bytes_inuse > highwater_alloc)
	highwater_alloc=bytes_inuse;
    obj_bin->nalloc++;

    // Safe to unlock now
    unlock();

    // Make sure that it is still cleared out...
    if(!lazy)
	audit(obj, OBJFREE);

    // Setup the new sentinels...
    char* data=(char*)obj;
    data+=sizeof(Tag);
    Sentinel* sent1=(Sentinel*)data;
    data+=sizeof(Sentinel);
    char* d=data;
    data+=obj_maxsize(obj);
    Sentinel* sent2=(Sentinel*)data;

    sent1->first_word=sent1->second_word=
	sent2->first_word=sent2->second_word=SENT_VAL_INUSE;
    // Fill in the region between the end of the allocation and the
    // end of the chunk.
    if(strict){
	char i=(char)(long)d;
	for(char* p=d+obj->reqsize;p<(char*)sent2;p++)
	    *p++=i++;
    }

    if(trace_out)
	fprintf(trace_out, "A %08p %d (%s)\n", d, size, tag);
    return (void*)d;
}

void* Allocator::alloc_big(size_t size, const char* tag)
{
    lock();

    Tag* obj=big_bin.free;
    size_t osize=size+OVERHEAD;
    size_t maxsize=osize+(size>>4);
    for(;obj!=0;obj=obj->next){
	// See if this object is within .0625% of the right size...
	if(obj->hunk->len > osize && obj->hunk->len <= maxsize)
	    break;
    }
    if(!obj){
	// First, see if we need to clean out the list.
	int nfree=big_bin.ntotal-big_bin.ninuse;
	if(nfree >= 10){
	    // Skip the first 5...
	    obj=big_bin.free;
	    for(int i=0;i<4;i++)
		obj=obj->next;
	    Tag* last=obj;
	    obj=obj->next;
	    // Free these ones...
	    while(obj!=0){
		Tag* next=obj->next;
		OSHunk* hunk=obj->hunk;
		nmunmap++;
		sizemunmap+=hunk->len;
		OSHunk::free(hunk);
		obj=next;
		big_bin.ntotal--;
	    }
	    last->next=0;
	}

	// Make a new one...
	size_t tsize=sizeof(OSHunk)+OVERHEAD+size;
	// Round up to nearest page size (assuming 4k pages).
	size_t npages=(tsize+4095)/4096;
	tsize=npages*4096;
	tsize-=sizeof(OSHunk);
	OSHunk* hunk=OSHunk::alloc(tsize);
	nmmap++;
	sizemmap+=tsize+sizeof(OSHunk);
	size_t diffmmap=sizemmap-sizemunmap;
	if(diffmmap > highwater_mmap)
	    highwater_mmap=diffmmap;
	obj=(Tag*)hunk->data;
	obj->bin=&big_bin;
	obj->tag="never used (big object)";
	obj->next=big_bin.free;
	if(big_bin.free)
	    big_bin.free->prev=obj;
	obj->prev=0;
	big_bin.free=obj;
	obj->hunk=hunk;
	big_bin.ntotal++;

	// Fill in sentinel info...
	char* data=(char*)obj;
	data+=sizeof(Tag);
	Sentinel* sent1=(Sentinel*)data;
	data+=sizeof(Sentinel);
	char* d=(char*)data;
	data+=obj_maxsize(obj);
	Sentinel* sent2=(Sentinel*)data;
	    
	sent1->first_word=sent1->second_word=
	    sent2->first_word=sent2->second_word=SENT_VAL_FREE;
	if(strict){
	    // Fill in the data region with markers.
	    unsigned long* p=(unsigned long*)d;
	    unsigned long* last=(unsigned long*)sent2;
	    unsigned long i=(unsigned long)p;
	    for(;p<last;p++)
		*p++=i++;
	}
    }

    // Have obj now...
    if(obj->prev)
	obj->prev->next=obj->next;
    else
	big_bin.free=obj->next;
    if(obj->next)
	obj->next->prev=obj->prev;

    // Tell the hunk that we are using this one...
    obj->hunk->ninuse++;
    obj->tag=tag;
    obj->next=big_bin.inuse;
    obj->prev=0;
    if(big_bin.inuse)
       big_bin.inuse->prev=obj;
    big_bin.inuse=obj;
    obj->reqsize=size;
    big_bin.ninuse++;

    nalloc++;
    sizealloc+=size;
    size_t bytes_inuse=sizealloc-sizefree;
    if(bytes_inuse > highwater_alloc)
	highwater_alloc=bytes_inuse;
    big_bin.nalloc++;

    // Safe to unlock now
    unlock();

    // Make sure that it is still cleared out...
    if(!lazy)
	audit(obj, OBJFREE);

    // Setup the new sentinels...
    char* data=(char*)obj;
    data+=sizeof(Tag);
    Sentinel* sent1=(Sentinel*)data;
    data+=sizeof(Sentinel);
    char* d=data;
    data+=obj_maxsize(obj);
    Sentinel* sent2=(Sentinel*)data;

    sent1->first_word=sent1->second_word=
	sent2->first_word=sent2->second_word=SENT_VAL_INUSE;
    // Fill in the region between the end of the allocation and the
    // end of the chunk.
    if(strict){
	char i=(char)(long)d;
	for(char* p=d+obj->reqsize;p<(char*)sent2;p++)
	    *p++=i++;
    }

    if(trace_out)
	fprintf(trace_out, "A %08p %d (%s)\n", d, size, tag);
    return (void*)d;
}

void* Allocator::realloc(void* dobj, size_t newsize)
{
    if(!dobj)
	return alloc(newsize, "realloc");
    // NOTE:  Realloc after free is NOT supported, and
    // probably never will be - MP problems
    char* dd=(char*)dobj;
    dd-=sizeof(Sentinel);
    dd-=sizeof(Tag);
    Tag* oldobj=(Tag*)dd;

    // Make sure that it is still intact...
    if(!lazy)
	audit(oldobj, OBJFREEING);

    // Check the simple case first...
    AllocBin* oldbin=get_bin(oldobj->bin->maxsize);
    if(newsize <= obj_maxsize(oldobj) && newsize >= oldbin->minsize){
	size_t oldsize=oldobj->reqsize;
	oldobj->reqsize=newsize;
	// Setup the new sentinels...
	char* data=(char*)oldobj;
	data+=sizeof(Tag);
	data+=sizeof(Sentinel);
	char* d=data;
	data+=obj_maxsize(oldobj);
	Sentinel* sent2=(Sentinel*)data;

	// Fill in the region between the end of the allocation and the
	// end of the chunk.
	if(strict){
	    char i=(char)(long)d;
	    for(char* p=d+oldobj->reqsize;p<(char*)sent2;p++)
		*p++=i++;
	}
	if(trace_out)
	    fprintf(trace_out, "R %08p %d %08p %d (%s)\n", dobj, oldsize, dobj, newsize, oldobj->tag);
	return dobj;
    }

    void* nobj=alloc(newsize, "realloc");
    size_t minsize=newsize;
    size_t oldsize=oldobj->reqsize;
    if(newsize > oldsize)
	minsize=oldsize;
    bcopy(dobj, nobj, minsize);
    free(dobj);
    if(trace_out)
	fprintf(trace_out, "R %08p %d %08p %d (%s)\n", dobj, oldsize, nobj, newsize, oldobj->tag);
    return nobj;
}

void* Allocator::memalign(size_t alignment, size_t size, const char* ctag)
{
    if(alignment <= 8)
	return alloc(size, ctag);

    size_t asize=size+sizeof(Tag)+sizeof(Sentinel)+alignment-8;
    void* addr=(char*)alloc(asize, ctag);
    char* m=(char*)addr;
    size_t misalign=((size_t)m+sizeof(Tag)+sizeof(Sentinel))%alignment;
    misalign=misalign==0?0:alignment-misalign;
    m+=misalign;
    Tag* tag=(Tag*)m;
    m+=sizeof(Tag);
    tag->bin=0;
    tag->next=tag->prev=(Tag*)addr;
    tag->hunk=0;
    tag->reqsize=size;
    tag->tag=ctag;
    Sentinel* sent1=(Sentinel*)m;
    m+=sizeof(Sentinel);
    sent1->first_word=sent1->second_word=SENT_VAL_INUSE;
    return m;
}

void Allocator::free(void* dobj)
{
//    fprintf(stderr, "Freeing %x\n", dobj);
    if(!dobj)
	return;
    char* dd=(char*)dobj;
    dd-=sizeof(Sentinel);
    dd-=sizeof(Tag);
    Tag* obj=(Tag*)dd;

    if(!obj->bin){
	// This was allocated with memalign...
	if(!lazy)
	    audit(obj, OBJMEMALIGNFREEING);
	if(obj->next != obj->prev)
	    AllocError("Memalign tag inconsistency, or memory corrupt!\n");
	free((void*)obj->prev);
	return;
    }

    // Make sure that it is still intact...
    if(trace_out)
	fprintf(trace_out, "F %08p %d (%s)\n", dobj, obj->reqsize, obj->tag);
    if(!lazy)
	audit(obj, OBJFREEING);

    AllocBin* obj_bin=get_bin(obj->bin->maxsize);


    lock();
    nfree++;
    sizefree+=obj->reqsize;
    obj_bin->nfree++;

    // Remove it from the inuse list...
    if(obj->next)
	obj->next->prev=obj->prev;
    if(obj->prev){
	obj->prev->next=obj->next;
    } else {
	obj_bin->inuse=obj->next;
    }
    obj_bin->ninuse--;

    if(obj_bin == &big_bin && obj->reqsize > 10*1024*1024){
      // Go ahead and unmap this segment...
      OSHunk* hunk=obj->hunk;
      nmunmap++;
      sizemunmap+=hunk->len;
      OSHunk::free(hunk);
      big_bin.ntotal--;
    } else {
      // Put it in the free list...
      obj->next=obj_bin->free;
      if(obj_bin->free)
	obj_bin->free->prev=obj;
      obj->prev=0;
      obj_bin->free=obj;


      // Setup the new sentinels...
      char* data=(char*)obj;
      data+=sizeof(Tag);
      Sentinel* sent1=(Sentinel*)data;
      data+=sizeof(Sentinel);
      char* d=(char*)data;
      data+=obj_maxsize(obj);
      Sentinel* sent2=(Sentinel*)data;

      sent1->first_word=sent1->second_word=
	sent2->first_word=sent2->second_word=SENT_VAL_FREE;
      
      if(strict){
	// Fill in the data region with markers.
	unsigned long* p=(unsigned long*)d;
	unsigned long* last=(unsigned long*)sent2;
	unsigned long i=(unsigned long)p;
	for(;p<last;p++)
	  *p++=i++;
      }
    }
    unlock();
}

void Allocator::fill_bin(AllocBin* bin)
{
    nfillbin++;
    if (bin->maxsize <= MEDIUM_THRESHOLD){
	size_t tsize;
	tsize=bin->maxsize+OVERHEAD;
	unsigned int nalloc=SMALLEST_ALLOCSIZE/tsize;
	if(nalloc<1)nalloc=1;
	size_t reqsize=nalloc*tsize;

	// Get the hunk...
	OSHunk* hunk;
	void* p;
	get_hunk(reqsize, hunk, p);
	for(int i=0;i<nalloc;i++){
	    Tag* t=(Tag*)p;
	    t->bin=bin;
	    t->tag="never used";
	    t->next=bin->free;
	    if(bin->free)
		bin->free->prev=t;
	    t->prev=0;
	    bin->free=t;
	    t->hunk=hunk;
	    p=(void*)((char*)p+tsize);
	    char* data=(char*)t;
	    data+=sizeof(Tag);
	    Sentinel* sent1=(Sentinel*)data;
	    data+=sizeof(Sentinel);
	    char* d=(char*)data;
	    data+=t->bin->maxsize;
	    Sentinel* sent2=(Sentinel*)data;
	    
	    sent1->first_word=sent1->second_word=
		sent2->first_word=sent2->second_word=SENT_VAL_FREE;
	    if(strict){
		// Fill in the data region with markers.
		unsigned long* p=(unsigned long*)d;
		unsigned long* last=(unsigned long*)sent2;
		unsigned long i=(unsigned long)p;
		for(;p<last;p++)
		    *p++=i++;
	    }
	}
	bin->ntotal+=nalloc;
    } else {
	AllocError("fill_bin not finished...");
    }
}

void Allocator::init_bin(AllocBin* bin, size_t maxsize, size_t minsize)
{
    bin->maxsize=maxsize;
    bin->minsize=minsize;
    bin->free=0;
    bin->inuse=0;
    bin->ninuse=0;
    bin->ntotal=0;
}

void Allocator::audit(Tag* obj, int what)
{
    char* data=(char*)obj;
    data+=sizeof(Tag);
    Sentinel* sent1=(Sentinel*)data;
    data+=sizeof(Sentinel);
    char* d=data;
    if(what != OBJMEMALIGNFREEING)
	data+=obj_maxsize(obj);
    Sentinel* sent2=(Sentinel*)data;

//    fprintf(stderr, "sentinels: %x %x %x %x\n", sent1->first_word, sent1->second_word, sent2->first_word, sent2->second_word);


    // Check that the sentinels are OK...
    if(what == OBJFREE){
	if(sent1->first_word != SENT_VAL_FREE || sent1->second_word != SENT_VAL_FREE){
	    if(sent1->first_word == SENT_VAL_INUSE){
		fprintf(stderr, "Object was allocated with this tag:\n%s\n", obj->tag);
		AllocError("Object should be free, but is tagged as INUSE");
	    } else {
		fprintf(stderr, "Free object has been corrupted within\n");
		fprintf(stderr, "the 8 bytes before the allocated region\n");
		fprintf(stderr, "Object was allocated with this tag:\n%s\n", obj->tag);
		AllocError("Freed object corrupt");
	    }
	}
	if(sent2->first_word != SENT_VAL_FREE || sent2->second_word != SENT_VAL_FREE){
	    if(sent2->first_word == SENT_VAL_INUSE){
		AllocError("Object should be free, but is tagged as INUSE (on tail only)");
	    } else {
		fprintf(stderr, "Free object has been corrupted within\n");
		fprintf(stderr, "the 8 bytes following the allocated region\n");
		fprintf(stderr, "Object was allocated with this tag:\n%s\n", obj->tag);
		AllocError("Freed object corrupt");
	    }
	}
    } else if(what == OBJFREEING || what == OBJINUSE || what == OBJMEMALIGNFREEING){
	if(sent1->first_word != SENT_VAL_INUSE || sent1->second_word != SENT_VAL_INUSE){
	    if(sent1->first_word == SENT_VAL_FREE){
		if(what == OBJFREEING){
		    fprintf(stderr, "Pointer (%x) was freed twice!\n", d);
		    fprintf(stderr, "Object was allocated with this tag:\n%s\n", obj->tag);
		    AllocError("Freeing pointer twice");
		} else {
		    fprintf(stderr, "Object was allocated with this tag:\n%s\n", obj->tag);
		    AllocError("Object should be inuse, but is tagged as FREE");
		}
	    } else {
		fprintf(stderr, "Object has been corrupted within\n");
		fprintf(stderr, "the 8 bytes before the allocated region\n");
		fprintf(stderr, "Object was allocated with this tag:\n%s\n", obj->tag);
		AllocError("Memory Object corrupt");
	    }
	}
	if(what != OBJMEMALIGNFREEING){
	    if(sent2->first_word != SENT_VAL_INUSE || sent2->second_word != SENT_VAL_INUSE){
		if(sent2->first_word == SENT_VAL_FREE){
		    if(what == OBJFREEING){
			fprintf(stderr, "Pointer (%x) was freed twice! (tail only?)\n", d);
			fprintf(stderr, "Object was allocated with this tag:\n%s\n", obj->tag);
			AllocError("Freeing pointer twice");
		    } else {
			fprintf(stderr, "Object was allocated with this tag:\n%s\n", obj->tag);
			AllocError("Object should be inuse, but is tagged as FREE");
		    }
		} else {
		    fprintf(stderr, "Object has been corrupted within\n");
		    fprintf(stderr, "the 8 bytes after the allocated region\n");
		    fprintf(stderr, "Object was allocated with this tag:\n%s\n", obj->tag);
		    AllocError("Memory Object corrupt");
		}
	    }
	}
    }

    // Check the space between the end of the allocation and the sentinel...
    if(strict && (what == OBJFREEING || what == OBJINUSE)){
	char i=(char)(long)d;
	for(char* p=d+obj->reqsize;p<(char*)sent2;p++){
	    char p1=*p++;
	    char p2=i++;
	    if(p1 != p2){
		fprintf(stderr, "p1=%d, p2=%d\n", (int)p1, (int)p2);
		fprintf(stderr, "Object has been corrupted immediately ");
		fprintf(stderr, "after the allocated region\n");
		fprintf(stderr, "Object was allocated with this tag:\n%s\n", obj->tag);
		AllocError("Memory Object corrupt");
	    }
	}
    }
    if(strict && what == OBJFREE){
	// Check the markers in the data region...
	unsigned long* p=(unsigned long*)d;
	unsigned long* last=(unsigned long*)sent2;
	unsigned long i=(unsigned long)p;
	for(;p<last;p++){
	    unsigned long p1=*p++;
	    unsigned long p2=i++;
	    if(p1 != p2){
		fprintf(stderr, "Object has been written after free\n");
		fprintf(stderr, "Object was allocated with this tag:\n%s\n", obj->tag);
		AllocError("Write after free");
	    }
	}
    }
}


void Allocator::get_hunk(size_t reqsize, OSHunk*& ret_hunk, void*& ret_p)
{
    // See if we have room in any of the current hunks...
    OSHunk* hunk;
    for(hunk=hunks; hunk!=0; hunk=hunk->next){
	if(hunk->spaceleft >= reqsize)
	    break;
    }
    if(!hunk){
	// Always request big chunks
	size_t s=reqsize>NORMAL_OS_ALLOC_SIZE?reqsize:NORMAL_OS_ALLOC_SIZE;
	hunk=OSHunk::alloc(s);
	hunk->next=hunks;
	hunks=hunk;
	hunk->spaceleft=s;
	hunk->curr=hunk->data;
	nmmap++;
	sizemmap+=s+sizeof(OSHunk);
	size_t diffmmap=sizemmap-sizemunmap;
	if(diffmmap > highwater_mmap)
	    highwater_mmap=diffmmap;
    }
    hunk->spaceleft-=reqsize;
    ret_p=hunk->curr;
    hunk->curr=(void*)((char*)hunk->curr+reqsize);
    ret_hunk=hunk;
}

void PrintTag(void* dobj)
{
    char* dd=(char*)dobj;
    dd-=sizeof(Sentinel);
    Sentinel* sent1=(Sentinel*)dd;
    dd-=sizeof(Tag);
    Tag* obj=(Tag*)dd;

    fprintf(stderr, "tag %x: allocated by: %s\n", obj, obj->tag);
    fprintf(stderr, "requested object size: %d bytes\n", obj->reqsize);
    fprintf(stderr, "maximum bin size: %d bytes\n", obj->bin->maxsize);
    fprintf(stderr, "range of object: %x - %x\n", dobj, (char*)dobj+obj->reqsize);
    fprintf(stderr, "range of object with overhead and sentinels: %x - %x\n",
	    obj, (char*)obj+OVERHEAD);
    fprintf(stderr, "range of hunk: %x - %x\n", obj->hunk->data, (char*)obj->hunk->data+obj->hunk->len);
    fprintf(stderr, "pre-sentinels: %x %x\n",
	    sent1->first_word, sent1->second_word);
    if(sent1->first_word == SENT_VAL_FREE && sent1->second_word == SENT_VAL_FREE){
	fprintf(stderr, "object should be free\n");
    } else if(sent1->first_word == SENT_VAL_INUSE && sent1->second_word == SENT_VAL_INUSE){
	fprintf(stderr, "object should be inuse\n");
    } else {
	fprintf(stderr, "status of object is unknown - sentinels must be messed up\n");
    }
}

void GetGlobalStats(Allocator* a,
		    size_t& nalloc, size_t& sizealloc,
		    size_t& nfree, size_t& sizefree,
		    size_t& nfillbin,
		    size_t& nmmap, size_t& sizemmap,
		    size_t& nmunmap, size_t& sizemunmap,
		    size_t& highwater_alloc, size_t& highwater_mmap,
		    size_t& nlonglocks, size_t& nnaps,
		    size_t& bytes_overhead,
		    size_t& bytes_free,
		    size_t& bytes_fragmented,
		    size_t& bytes_inuse,
		    size_t& bytes_inhunks)
{
    if(!a){
	nalloc=sizealloc=nfree=sizefree=nfillbin=0;
	nmmap=sizemmap=nmunmap=sizemunmap=0;
	highwater_alloc=highwater_mmap=nlonglocks=nnaps=0;
	bytes_overhead=bytes_free=bytes_fragmented=bytes_inuse=0;
	bytes_inhunks=0;
	return;
    }
    a->lock();
    nalloc=a->nalloc;
    sizealloc=a->sizealloc;
    nfree=a->nfree;
    sizefree=a->sizefree;
    nfillbin=a->nfillbin;
    nmmap=a->nmmap;
    sizemmap=a->sizemmap;
    nmunmap=a->nmunmap;
    sizemunmap=a->sizemunmap;
    highwater_alloc=a->highwater_alloc;
    highwater_mmap=a->highwater_mmap;
    nlonglocks=a->nlonglocks;
    nnaps=a->nnaps;
    
    // Full accounting - go through each bin...
    bytes_overhead=bytes_free=bytes_fragmented=bytes_inuse=bytes_inhunks=0;
    int i;
    for(i=0;i<NSMALL_BINS;i++)
	account_bin(a, &a->small_bins[i], 0, bytes_overhead, bytes_free,
		    bytes_fragmented, bytes_inuse);
    for(i=0;i<NMEDIUM_BINS;i++)
	account_bin(a, &a->medium_bins[i], 0, bytes_overhead, bytes_free,
		    bytes_fragmented, bytes_inuse);
    account_bin(a, &a->big_bin, 0, bytes_overhead, bytes_free,
		    bytes_fragmented, bytes_inuse);

    // Count hunks...
    for(OSHunk* hunk=a->hunks;hunk!=0;hunk=hunk->next){
	bytes_overhead+=sizeof(OSHunk);
	bytes_inhunks+=hunk->spaceleft;
    }
    // And the ones in the bigbin...
    Tag* p;
    for(p=a->big_bin.free;p!=0;p=p->next)
	bytes_overhead+=sizeof(OSHunk);
    for(p=a->big_bin.inuse;p!=0;p=p->next)
	bytes_overhead+=sizeof(OSHunk);
    bytes_overhead+=a->mysize;

    // Count the allocators memory...
    a->unlock();
}

int GetNbins(Allocator*)
{
    return NSMALL_BINS+NMEDIUM_BINS+1;
}

void GetBinStats(Allocator* a, int binno, size_t& minsize, size_t& maxsize,
		 size_t& nalloc, size_t& nfree, size_t& ninlist)
{
    AllocBin* bin;
    if(binno < NSMALL_BINS)
	bin=&a->small_bins[binno];
    else if(binno < NSMALL_BINS+NMEDIUM_BINS)
	bin=&a->medium_bins[binno-NSMALL_BINS];
    else
	bin=&a->big_bin;

    a->lock();
    minsize=bin->minsize;
    maxsize=bin->maxsize;
    nalloc=bin->nalloc;
    nfree=bin->nfree;
    ninlist=bin->ntotal-bin->ninuse;
    a->unlock();
}

Allocator* DefaultAllocator()
{
    return default_allocator;
}

static void audit_bin(Allocator* a, AllocBin* bin)
{
    Tag* p;
    for(p=bin->free;p!=0;p=p->next){
	if(p->next && p->next->prev != p)
	    AllocError("Free list confused");
	a->audit(p, OBJFREE);
    }
    for(p=bin->inuse;p!=0;p=p->next){
	if(p->next && p->next->prev != p)
	    AllocError("Inuse list confused");
	a->audit(p, OBJINUSE);
    }
}

void AuditAllocator(Allocator* a)
{
    a->lock();
    int i;
    for(i=0;i<NSMALL_BINS;i++)
	audit_bin(a, &a->small_bins[i]);
    for(i=0;i<NMEDIUM_BINS;i++)
	audit_bin(a, &a->medium_bins[i]);
    audit_bin(a, &a->big_bin);
    a->unlock();
}

void AuditDefaultAllocator()
{
    AuditAllocator(default_allocator);
}

static void dump_bin(Allocator*, AllocBin* bin, FILE* fp)
{
    for(Tag* p=bin->inuse;p!=0;p=p->next){
	fprintf(fp, "%x %d %s\n", (char*)p+sizeof(Tag)+sizeof(Sentinel),
		p->reqsize, p->tag);
    }
}

void DumpAllocator(Allocator* a)
{
    FILE* fp=fopen("alloc.dump", "w");
    fprintf(fp, "\n");
    a->lock();
    int i;
    for(i=0;i<NSMALL_BINS;i++)
	dump_bin(a, &a->small_bins[i], fp);
    for(i=0;i<NMEDIUM_BINS;i++)
	dump_bin(a, &a->medium_bins[i], fp);
    dump_bin(a, &a->big_bin, fp);
    a->unlock();
    fclose(fp);
}

} // End namespace Malloc
} // End namespace SCICore

//
// $Log$
// Revision 1.9  1999/10/13 05:59:52  sparker
// Fixed pthreads configuration problem in Malloc for non x86 linux boxen
//
// Revision 1.8  1999/10/07 02:07:58  sparker
// use standard iostreams and complex type
//
// Revision 1.7  1999/09/30 00:33:46  sparker
// Added support for memalign (got orphaned in move from SCIRun)
//
// Revision 1.6  1999/09/25 08:30:54  sparker
// Added support for MALLOC_STATS environment variable.  If set, it will
//   send statistics about malloc and a list of all unfreed objects at
//   program shutdown.  It isn't perfect yet - I need to figure out how
//   to make it run after the C++ dtors.
//
// Revision 1.5  1999/09/17 05:04:43  sparker
// Enhanced malloc tracing facility.  You can now set the environment
// variable MALLOC_TRACE to a filename, where the allocator will dump
// all calls to alloc/free.  It will also dump any unfreed objects
// and some statistics at the end of the program.  Setting MALLOC_TRACE
// to an empty string sends this information to stderr.
//
// Revision 1.4  1999/08/23 06:30:37  sparker
// Linux port
// Added X11 configuration options
// Removed many warnings
//
// Revision 1.3  1999/08/19 05:30:56  sparker
// Configuration updates:
//  - renamed config.h to sci_config.h
//  - also uses sci_defs.h, since I couldn't get it to substitute vars in
//    sci_config.h
//  - Added flags for --enable-scirun, --enable-uintah, and
//    --enable-davew, to build the specific package set.  More than one
//    can be specified, and at least one must be present.
//  - Added a --enable-parallel, to build the new parallel version.
//    Doesn't do much yet.
//  - Made construction of config.h a little bit more general
//
// Revision 1.2  1999/08/17 06:39:30  sparker
// Merged in modifications from PSECore to make this the new "blessed"
// version of SCIRun/Uintah.
//
// Revision 1.1  1999/07/27 16:56:58  mcq
// Initial commit
//
// Revision 1.1.1.1  1999/04/24 23:12:24  dav
// Import sources
//
//
