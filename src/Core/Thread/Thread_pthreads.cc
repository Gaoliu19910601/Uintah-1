
#include "Barrier.h"
#include "ConditionVariable.h"
#include "Mutex.h"
#include "PoolMutex.h"
#include "RealtimeThread.h"
#include "Semaphore.h"
#include "ThreadGroup.h"
#include "ThreadListener.h"
#include "ThreadEvent.h"
#include "Thread.h"
#include <iostream.h>
#include <stdio.h>
#include <stdlib.h>

extern "C" {
#include <semaphore.h>
#include <pthread.h>
};
#include <errno.h>

#define NF cerr << "Not finished: " << __FILE__ << ": " << __LINE__ << '\n';


#define MAXBSTACK 10
#define MAXTHREADS 4000
#define N_POOLMUTEX 301

// Thread states
#define STATE_STARTUP 1
#define STATE_RUNNING 2
#define STATE_IDLE 3
#define STATE_SHUTDOWN 4
#define STATE_BLOCK_SEMAPHORE 5
#define STATE_PROGRAM_EXIT 6
#define STATE_JOINING 7
#define STATE_BLOCK_MUTEX 8
#define STATE_BLOCK_ANY 9
#define STATE_DIED 10
#define STATE_BLOCK_POOLMUTEX 11

struct Thread_private {
    Thread* thread;
    pthread_t threadid;
    int state;
    int bstacksize;
    const char* blockstack[MAXBSTACK];
    sem_t done;
    sem_t delete_ready;
    bool detached;
};

static char* poolmutex_names[N_POOLMUTEX];
static char poolmutex_namearray[N_POOLMUTEX*20]; // Enough to hold "PoolMutex #%d"

static Thread_private* active[MAXTHREADS];
static int nactive;
static bool initialized;
static pthread_mutex_t sched_lock;
static pthread_mutex_t pool_mutex[N_POOLMUTEX];
static pthread_key_t thread_key;
static struct timeval start_time;
static sem_t main_sema;

static void lock_scheduler() {
    if(pthread_mutex_lock(&sched_lock)){
	perror("pthread_mutex_lock");
	Thread::niceAbort();
    }
}

static void unlock_scheduler() {
    if(pthread_mutex_unlock(&sched_lock)){
	perror("pthread_mutex_unlock");
	Thread::niceAbort();
    }
}

static int push_bstack(Thread_private* p, int state, const char* name) {
    int oldstate=p->state;
    p->state=state;
    p->blockstack[p->bstacksize]=name;
    p->bstacksize++;
    if(p->bstacksize>MAXBSTACK){
	fprintf(stderr, "Blockstack Overflow!\n");
	Thread::niceAbort();
    }
    return oldstate;
}

static void pop_bstack(Thread_private* p, int oldstate) {
    p->bstacksize--;
    p->state=oldstate;
}

// This is only used for the single processor implementation
struct Barrier_private {
    Mutex mutex;
    ConditionVariable cond0;
    ConditionVariable cond1;
    int cc;
    int nwait;
    Barrier_private();
};

Barrier_private::Barrier_private()
: cond0("Barrier condition 0"), cond1("Barrier condition 1"),
  mutex("Barrier lock"), nwait(0), cc(0)
{
}   

Barrier::Barrier(const char* name, int nthreads)
 : name(name), nthreads(nthreads), threadGroup(0)
{
    if(!initialized){
	Thread::initialize();
    }
    priv=new Barrier_private;
}

Barrier::Barrier(const char* name, ThreadGroup* threadGroup)
 : name(name), nthreads(0), threadGroup(threadGroup)
{
    if(!initialized){
	Thread::initialize();
    }
    priv=new Barrier_private;
}

Barrier::~Barrier()
{
    delete priv;
}

void Barrier::wait()
{
    int n=threadGroup?threadGroup->nactive(true):nthreads;
    Thread_private* p=Thread::currentThread()->priv;
    int oldstate=push_bstack(p, STATE_BLOCK_SEMAPHORE, name);
    priv->mutex.lock();
    ConditionVariable& cond=priv->cc?priv->cond0:priv->cond1;
    int me=priv->nwait++;
    if(priv->nwait == n){
	// Wake everybody up...
	priv->nwait=0;
	priv->cc=1-priv->cc;
	cond.cond_broadcast();
    } else {
	cond.wait(priv->mutex);
    }
    priv->mutex.unlock();
    pop_bstack(p, oldstate);
}

struct Mutex_private {
    pthread_mutex_t mutex;
};

Mutex::Mutex(const char*) {
    priv=new Mutex_private;
    if(pthread_mutex_init(&priv->mutex, NULL) != 0){
	perror("pthread_mutex_init");
	Thread::niceAbort();
    }
}

Mutex::~Mutex() {
    if(pthread_mutex_destroy(&priv->mutex) != 0){
	perror("pthread_mutex_destroy");
	Thread::niceAbort();
    }
    delete priv;
}

void Mutex::unlock() {
    if(pthread_mutex_unlock(&priv->mutex) != 0){
	perror("pthread_mutex_unlock");
	Thread::niceAbort();
    }
}

void Mutex::lock() {
    if(pthread_mutex_lock(&priv->mutex) != 0){
	perror("pthread_mutex_lock");
	Thread::niceAbort();
    }
}

bool Mutex::try_lock() {
    if(pthread_mutex_trylock(&priv->mutex) != 0){
	if(errno == EAGAIN)
	    return false;
	perror("pthread_mutex_trylock");
	Thread::niceAbort();
    }
    return true;
}

PoolMutex::PoolMutex() { NF }
PoolMutex::~PoolMutex() { NF }
void PoolMutex::lock() { NF }
bool PoolMutex::try_lock() { NF;return 0; }
void PoolMutex::unlock() { NF }

struct Semaphore_private {
    sem_t sem;
};

Semaphore::Semaphore(const char* name, int value) : name(name) {
    priv=new Semaphore_private;
    if(sem_init(&priv->sem, 0, value) != 0){
	perror("sem_init");
	Thread::niceAbort();
    }
}
    
Semaphore::~Semaphore() {
    if(sem_destroy(&priv->sem) != 0){
	perror("sem_destroy");
	Thread::niceAbort();
    }
    delete priv;
}

void Semaphore::up() {
    if(sem_post(&priv->sem) != 0){
	perror("sem_post");
	Thread::niceAbort();
    }
}

void Semaphore::down() {
    if(sem_wait(&priv->sem) != 0){
	perror("sem_wait");
	Thread::niceAbort();
    }
}

bool Semaphore::tryDown() {
    if(sem_trywait(&priv->sem) != 0){
	if(errno == EAGAIN)
	    return false;
	perror("sem_trywait");
	Thread::niceAbort();
    }
    return true;
}

void ThreadGroup::gangSchedule() { NF }

void Thread::alert(int) { NF }

void Thread::check_exit() {
    lock_scheduler();
    int done=true;
    for(int i=0;i<nactive;i++){
	Thread_private* p=active[i];
	if(!p->thread->isDaemon()){
	    done=false;
	    break;
	}
    }
    unlock_scheduler();

    if(done)
	Thread::exitAll(0);
}

Thread* Thread::currentThread() { 
    void* p=pthread_getspecific(thread_key);
    if(!p){
	perror("pthread_getspecific");
	Thread::niceAbort();
    }
    return (Thread*)p;
}

double Thread::secondsPerTick() { NF;return 0; }

void Thread::join() {
    Thread* us=Thread::currentThread();
    int os=push_bstack(us->priv, STATE_JOINING, threadname);
    if(sem_wait(&priv->done) != 0){
	perror("sem_wait");
	Thread::niceAbort();
    }
    pop_bstack(us->priv, os);
    detach();
}

void Thread::profile(FILE*, FILE*) { NF }
int Thread::numProcessors() {
    return 1;
}

void Thread_shutdown(Thread* thread) {
    Thread_private* priv=thread->priv;

    if(sem_post(&priv->done) != 0){
	perror("sem_post");
	Thread::niceAbort();
    }

    delete thread;

    // Wait to be deleted...
    if(sem_wait(&priv->delete_ready) == -1) {
	perror("sem_wait");
	Thread::niceAbort();
    }

    // Allow this thread to run anywhere...
    if(thread->cpu != -1)
	thread->migrate(-1);

    priv->thread=0;
    lock_scheduler();
    /* Remove it from the active queue */
    int i;
    for(i=0;i<nactive;i++){
	if(active[i]==priv)
	    break;
    }
    for(i++;i<nactive;i++){
	active[i-1]=active[i];
    }
    nactive--;
    unlock_scheduler();
    Thread::check_exit();
    if(priv->threadid == 0){
	priv->state=STATE_PROGRAM_EXIT;
	if(sem_wait(&main_sema) == -1){
	    perror("sem_wait");
	    Thread::niceAbort();
	}
    }
}

void Thread_run(Thread* t) {
    t->run_body();
}

static void* run_threads(void* priv_v) {
    Thread_private* priv=(Thread_private*)priv_v;
    if(pthread_setspecific(thread_key, priv->thread) != 0){
	perror("pthread_setspecific");
	Thread::niceAbort();
    }
    priv->state=STATE_RUNNING;
    Thread_run(priv->thread);
    priv->state=STATE_SHUTDOWN;
    Thread_shutdown(priv->thread);
    priv->state=STATE_IDLE;
    return 0;
}

void Thread::os_start(bool) {
    if(!initialized){
	Thread::initialize();
    }

    priv=new Thread_private;

    if(sem_init(&priv->done, 0, 0) != 0){
	perror("sem_init");
	Thread::niceAbort();
    }
    if(sem_init(&priv->delete_ready, 0, 0) != 0){
	perror("sem_init");
	Thread::niceAbort();
    }
    priv->state=STATE_STARTUP;
    priv->bstacksize=0;
    priv->detached=0;

    priv->thread=this;
    priv->threadid=0;
    lock_scheduler();
    if(pthread_create(&priv->threadid, NULL, run_threads, priv) != 0){
	perror("pthread_create");
	Thread::niceAbort();
    }
    active[nactive]=priv;
    nactive++;
    unlock_scheduler();
}

double Thread::currentSeconds() {
    struct timeval now_time;
    if(gettimeofday(&now_time, 0) != 0){
	perror("gettimeofday");
	Thread::niceAbort();
    }
    return (now_time.tv_sec-start_time.tv_sec)+(now_time.tv_usec-start_time.tv_usec)*1.e-6;
}
void Thread::stop() { NF }
void Thread::resume() { NF }

SysClock Thread::currentTicks() { 
    struct timeval now_time;
    if(gettimeofday(&now_time, 0) != 0){
	perror("gettimeofday");
	Thread::niceAbort();
    }
    return (now_time.tv_sec-start_time.tv_sec)*100000+(now_time.tv_usec-start_time.tv_usec);
}

int Thread::couldBlock(const char* why) {
    Thread_private* p=Thread::currentThread()->priv;
    return push_bstack(p, STATE_BLOCK_ANY, why);
}

void Thread::couldBlock(int restore) {
    Thread_private* p=Thread::currentThread()->priv;
    pop_bstack(p, restore);
}

void Thread::detach() {
    if(sem_post(&priv->delete_ready) != 0){
	perror("sem_post");
	Thread::niceAbort();
    }
    priv->detached=1;
    if(pthread_detach(priv->threadid) != 0){
	perror("pthread_detach");
	Thread::niceAbort();
    }
}

void Thread::setPriority(int) { NF }
double Thread::ticksPerSecond() { return 1000000; }
void Thread::exitAll(int) { NF }

void Thread::initialize() {
    if(gettimeofday(&start_time, 0) != 0){
	perror("getimeofday");
	Thread::niceAbort();
    }
    if(pthread_mutex_init(&sched_lock, NULL) != 0){
	perror("pthread_mutex_init");
	Thread::niceAbort();
    }

    // Allocate PoolMutex pool
    unsigned int c=0;
    for(int i=0;i<N_POOLMUTEX;i++){
	if(pthread_mutex_init(&pool_mutex[i], NULL) != 0){
	    perror("pthread_mutex_init");
	    Thread::niceAbort();
	}
	poolmutex_names[i]=&poolmutex_namearray[c];
	sprintf(poolmutex_names[i], "PoolMutex #%d", i);
	c+=strlen(poolmutex_names[i])+1;
	if(c > sizeof(poolmutex_namearray)){
	    fprintf(stderr, "PoolMutex name array overflow!\n");
	    exit(1);
	}
    }

    if(pthread_key_create(&thread_key, NULL) != 0){
	perror("pthread_key_create");
	Thread::niceAbort();
    }

    ThreadGroup::default_group=new ThreadGroup("default group", 0);
    Thread* mainthread=new Thread(ThreadGroup::default_group, "main");
    mainthread->priv=new Thread_private;
    mainthread->priv->thread=mainthread;
    mainthread->priv->state=STATE_RUNNING;
    mainthread->priv->bstacksize=0;
    if(pthread_setspecific(thread_key, mainthread) != 0){
	perror("pthread_setspecific");
	Thread::niceAbort();
    }
    if(sem_init(&mainthread->priv->done, 0, 0) != 0){
	perror("sem_init");
	Thread::niceAbort();
    }
    if(sem_init(&mainthread->priv->delete_ready, 0, 0) != 0){
	perror("sem_init");
	Thread::niceAbort();
    }
    if(sem_init(&main_sema, 0, 0) != 0){
	perror("sem_init");
	Thread::niceAbort();
    }
    lock_scheduler();
    active[nactive]=mainthread->priv;
    nactive++;
    unlock_scheduler();

    //install_signal_handlers();

    initialized=1;
}

void Thread::waitUntil(double seconds) {
    waitFor(seconds-currentSeconds());
}

void Thread::waitFor(double seconds) {
    if(seconds<=0)
	return;
    struct timespec ts;
    ts.tv_sec=(int)seconds;
    ts.tv_nsec=(int)(1.e9*(seconds-ts.tv_sec));
    while (nanosleep(&ts, &ts) == 0) /* Nothing */ ;
}

void Thread::waitUntil(SysClock time) {
    waitFor(time-currentTicks());
}

void Thread::waitFor(SysClock time) {
    if(time<=0)
	return;
    struct timespec ts;
    ts.tv_sec=(int)(time*1.e-6);
    ts.tv_nsec=(int)(1.e9*(time*1.e-6-ts.tv_sec));
    while (nanosleep(&ts, &ts) == 0) /* Nothing */ ;
}

void RealtimeThread::frameYield() { NF }
void RealtimeThread::frameReady() { NF }
void RealtimeThread::frameSchedule(int, RealtimeThread*) { NF }

void Thread::yield() {
    sched_yield();
}

void Thread::migrate(int proc)
{
    // Nothing for now...
}
