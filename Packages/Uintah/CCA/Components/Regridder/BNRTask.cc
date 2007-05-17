#include <Packages/Uintah/CCA/Components/Regridder/BNRRegridder.h>
#include <Packages/Uintah/CCA/Components/Regridder/BNRTask.h>

using namespace Uintah;
#include <vector>
#include <set>
#include <algorithm>
using namespace std;

int sign(int i)
{
    if(i>0)
            return 1;
    else if (i<0)
            return -1;
    else
            return 0;
}
BNRRegridder *BNRTask::controller_=0;

BNRTask::BNRTask(Region patch, FlagsList flags, const vector<int> &p_group, int p_rank, BNRTask *parent, unsigned int tag): status_(NEW), patch_(patch), flags_(flags), parent_(parent), sibling_(0), tag_(tag), remaining_requests_(0),p_group_(p_group), p_rank_(p_rank)
{
  //calculate hypercube dimensions
   unsigned int p=1;
  d_=0;
  while(p<p_group_.size())
  {
    p<<=1;
    d_++;
  }
}

MPI_Request* BNRTask::getRequest()
{
  remaining_requests_++;
  if(controller_->free_requests_.empty())
  {
    int index=controller_->requests_.size();
    
    //allocate a new request
    MPI_Request request;
    controller_->requests_.push_back(request);
    controller_->indicies_.push_back(0);
    controller_->statuses_.push_back(MPI_Status());
    
    //assign request
    controller_->request_to_task_.push_back(this);
    return &controller_->requests_[index]; 
  }
  else
  {
    //get a free request
    int index=controller_->free_requests_.front();
    
    //assign request
    controller_->free_requests_.pop();
    controller_->request_to_task_[index]=this;
    return &controller_->requests_[index]; 
  }
}

/*
 *  This function continues a task from where it left off
 *  Each task runs through the BR algorithm and performs
 *  communication where needed.  When the task is unable
 *  to make progress by waiting on communication it terminates 
 *  and is restarted later by the main controll loop in
 *  BNRRegridder::RunBR().  I use goto's in this and I know 
 *  they are bad form but in this case goto's make the algorithm 
 *  easier to understand.
 *
 *  Most local variables are stored as class variables so the 
 *  state will remain the same when a task is restarted.  
 */
void BNRTask::continueTask()
{
  int stride;
  int msg_size;
  unsigned int p;
  unsigned int partner;
  int start;
  
  switch (status_)
  {
    case NEW:                                                             //0
      goto TASK_START;
    case GATHERING_FLAG_COUNT:                                            //1
      goto GATHER_FLAG_COUNT;
    case BROADCASTING_FLAG_COUNT:                                         //2
      goto BROADCAST_FLAG_COUNT;
    case COMMUNICATING_SIGNATURES:                                        //3
      goto COMMUNICATE_SIGNATURES;
    case SUMMING_SIGNATURES:                                              //4
      goto SUM_SIGNATURES;
    case WAITING_FOR_TAGS:                                                //5
      goto WAIT_FOR_TAGS;                                            
    case BROADCASTING_CHILD_TASKS:                                        //6
      goto BROADCAST_CHILD_TASKS;
    case WAITING_FOR_CHILDREN:                                            //7
      goto WAIT_FOR_CHILDREN;                                  
    case WAITING_FOR_PATCH_COUNT:                                         //8
      goto WAIT_FOR_PATCH_COUNT;
    case WAITING_FOR_PATCHES:                                             //9
      goto WAIT_FOR_PATCHES;
    case TERMINATED:                                                      //10
      return;
    default:
      char error[100];
      sprintf(error,"Error invalid status (%d) in parallel task\n",status_);
      throw InternalError(error,__FILE__,__LINE__);
  }
                  
  TASK_START:
  
  offset_=-patch_.getLow();
  
  if(p_group_.size()>1)
  {
    //gather # of flags_ on root
    status_=GATHERING_FLAG_COUNT;
    //set mpi state
    stage_=0;
  
    //Allocate recieve buffer
    flagscount_.resize(1<<d_);    //make this big enough to recieve for entire hypercube
    flagscount_[0].count=flags_.size;
    flagscount_[0].rank=p_group_[p_rank_];

    //Gather the flags onto the root processor without blocking
    GATHER_FLAG_COUNT:

    if(stage_<d_)
    {
      stride=1<<(d_-1-stage_);
      msg_size=1<<stage_;

      stage_++;
      if(p_rank_<stride)  //recieving
      {
        partner=p_rank_+stride;
        if(partner<p_group_.size())
        {
          //Nonblocking recieve msg from partner
          MPI_Irecv(&flagscount_[msg_size],msg_size*sizeof(FlagsCount),MPI_BYTE,p_group_[partner],tag_,controller_->d_myworld->getComm(),getRequest());
          return;
        }
        else
        {
          for(int f=0;f<msg_size;f++)
          {
              flagscount_[msg_size+f].count=0;
              flagscount_[msg_size+f].rank=-1;
          }    
          goto GATHER_FLAG_COUNT;
        }
      }
      else if(p_rank_ < (stride<<1) )  //sending
      {
        int partner=p_rank_-stride;
      
        //non blocking send msg of size size to partner
        MPI_Isend(&flagscount_[0],msg_size*sizeof(FlagsCount),MPI_BYTE,p_group_[partner],tag_,controller_->d_myworld->getComm(),getRequest());
        return;
      }
    }
    
    status_=BROADCASTING_FLAG_COUNT;
    stage_=0;
    
    BROADCAST_FLAG_COUNT:
  
    if(Broadcast(&flagscount_[0],flagscount_.size()*sizeof(FlagsCount),MPI_BYTE))
      return;
    
    //sort ascending #flags_
    sort(flagscount_.begin(),flagscount_.end());
    
    //update p_rank_
    for(p=0;p<p_group_.size();p++)
    {
      if(flagscount_[p].rank==p_group_[p_rank_])
      {
          p_rank_=p;
          break;
      }
    }
    //update p_group_
    for(p=0;p<p_group_.size();p++)
    {
      if(flagscount_[p].count==0)
              break;
      p_group_[p]=flagscount_[p].rank;  
    }
    p_group_.resize(p);
    
    if(flags_.size==0)  //if i don't have any flags don't participate any longer
    {
      p_rank_=-1;
      goto TERMINATE;    
    }
    
    //calculate hypercube dimensions
    p=1;    
    d_=0;
    while(p<p_group_.size())
    {
      p<<=1;
      d_++;
    }
    //compute total # of flags on new root 
    if(p_rank_==0)
    {
      total_flags_=0;
      for(unsigned int p=0;p<p_group_.size();p++)
      {
        total_flags_+=flagscount_[p].count;
      }
    }
  
    //give buffer back to OS
    flagscount_.clear();  
  }
  else
  {
    total_flags_=flags_.size;
  }
  
  //compute local signatures
  ComputeLocalSignature();

  if(p_group_.size()>1)
  {
    sum_.resize(sig_size_);
    //sum signatures
    stage_=0;
    status_=COMMUNICATING_SIGNATURES;
    COMMUNICATE_SIGNATURES:
      
    //global reduce sum signatures
    if(stage_<d_)
    {
      stride=1<<(d_-1-stage_);
      stage_++;
      //determine if i'm a sender or a reciever
      if(p_rank_<stride)
      {
        partner=p_rank_+stride;
        if(partner<p_group_.size())
        {
          status_=SUMMING_SIGNATURES;
          
          //Nonblocking recieve msg from partner
          MPI_Irecv(&sum_[0],sig_size_,MPI_INT,p_group_[partner],tag_,controller_->d_myworld->getComm(),getRequest());
          return;

          SUM_SIGNATURES:
          
          for(unsigned int i=0;i<sig_size_;i++)
          {
            count_[i]+=sum_[i];
          }
            
          status_=COMMUNICATING_SIGNATURES;
          goto COMMUNICATE_SIGNATURES;
        }
        else
        {
            goto COMMUNICATE_SIGNATURES;
        }
      }
      else if(p_rank_< (stride<<1))
      {
        partner=p_rank_-stride;
          
        //Nonblocking recieve msg from partner
        MPI_Isend(&count_[0],sig_size_,MPI_INT,p_group_[partner],tag_,controller_->d_myworld->getComm(),getRequest());
        return;
      }
    }
    //deallocate sum_ array
    sum_.clear();  
  }  
  
  //controlling task determines if the current patch is good or not
  //if not find the location to split this patch
  if(p_rank_==0)
  {
    //bound signatures
    BoundSignatures();  
    
    //check tolerance a
    CheckTolA();

    if(acceptable_)
    {
      //set d=-1 to signal that the patch is acceptable
      ctasks_.split.d=-1;
    }
    else
    {
      //find split
      ctasks_.split=FindSplit();
      
      //split the current patch
      ctasks_.left=ctasks_.right=patch_;
      ctasks_.left.high()[ctasks_.split.d]=ctasks_.right.low()[ctasks_.split.d]=ctasks_.split.index;

      WAIT_FOR_TAGS:
      //check if tags are available
      if(!controller_->getTags(ctasks_.ltag,ctasks_.rtag) )
      {
        status_=WAITING_FOR_TAGS;
        controller_->tag_q_.push(this);
        return;
      }
    }
  }  
  
  //signature is no longer needed so free memory
  count_.clear();
  //broadcast child tasks
  if(p_group_.size()>1)
  {
    status_=BROADCASTING_CHILD_TASKS;
    stage_=0;
    BROADCAST_CHILD_TASKS:
    //broadcast children tasks
    if(Broadcast(&ctasks_,sizeof(ChildTasks),MPI_BYTE))
    {
      return;
    }
  }
  
  if(ctasks_.split.d==-1)
  {
      //current patch is acceptable
      my_patches_.push_back(patch_);
  }
  else
  {
    //create tasks
    CreateTasks();
  
    //Wait for childern to reactivate parent
    status_=WAITING_FOR_CHILDREN;  
    return;
    WAIT_FOR_CHILDREN:
    
    if(p_rank_==0)
    {  
      //begin # of patches recv

      //if i'm also the master of the children tasks copy the patches from the child task
      if(left_->p_group_[0]==p_group_[0])
      {
         left_size_=left_->my_patches_.size();
         my_patches_.assign(left_->my_patches_.begin(),left_->my_patches_.end());
      }
      else
      {
        MPI_Irecv(&left_size_,1,MPI_INT,left_->p_group_[0],left_->tag_,controller_->d_myworld->getComm(),getRequest());
      }
      if(right_->p_group_[0]==p_group_[0])
      {
         right_size_=right_->my_patches_.size();
         my_patches_.insert(my_patches_.end(),right_->my_patches_.begin(),right_->my_patches_.end());
      }
      else
      {
        MPI_Irecv(&right_size_,1,MPI_INT,right_->p_group_[0],right_->tag_,controller_->d_myworld->getComm(),getRequest());
      }
      //recv's might not be done yet so place back on delay_q
      status_=WAITING_FOR_PATCH_COUNT;  
      if(remaining_requests_>0)
        return;
      
      WAIT_FOR_PATCH_COUNT:
      status_=WAITING_FOR_PATCHES;
      
      start=my_patches_.size();               //start of receive buff
      //resize my_patches_ buffer to recieve
      my_patches_.resize(left_size_+right_size_);
     
      //recieve patch_sets from children on child tag only if it hasn't been copied already
      if(left_->p_group_[0]!=p_group_[0])
      {
        MPI_Irecv(&my_patches_[start],left_size_*sizeof(Region),MPI_BYTE,left_->p_group_[0],left_->tag_,controller_->d_myworld->getComm(),getRequest());    
        start+=left_size_;                        //move recieve buffer forward
      }
      if(right_->p_group_[0]!=p_group_[0])
      {
        MPI_Irecv(&my_patches_[start],right_size_*sizeof(Region),MPI_BYTE,right_->p_group_[0],right_->tag_,controller_->d_myworld->getComm(),getRequest());    
      }    
      if(remaining_requests_>0)
        return;
      WAIT_FOR_PATCHES:
    
      controller_->tags_.push(left_->tag_);    //reclaim tag
      controller_->tags_.push(right_->tag_);    //reclaim tag
      
      //check tolerance b and take better patchset
      CheckTolB();
      if(!acceptable_)
      {
        my_patches_.resize(0);
        my_patches_.push_back(patch_);
      }
    } //if(p_rank_==0)
  }
  
  //COMMUNICATE_PATCH_LIST:  
  if(p_rank_==0 && parent_!=0)
  {
    if(p_group_[0]!=parent_->p_group_[0]) //if I am not the same rank as the parent master process
    {
      //send up to the master using mpi
      my_size_=my_patches_.size();
  
      //send patch_ count to parent
      MPI_Isend(&my_size_,1,MPI_INT,parent_->p_group_[0],tag_,controller_->d_myworld->getComm(),getRequest());
     
      if(my_size_>0)
      {
        //send patch list to parent
        MPI_Isend(&my_patches_[0],my_size_*sizeof(Region),MPI_BYTE,parent_->p_group_[0],tag_,controller_->d_myworld->getComm(),getRequest());
      }
    }
  }
  
  TERMINATE:
  
  status_=TERMINATED;
  
  //if parent is waiting activiate parent 
  if(parent_!=0 && sibling_->status_==TERMINATED )
  {
    //place parent_ on immediate queue (parent is waiting for communication from children and both are done sending)
    controller_->immediate_q_.push(parent_);
  }
  
  return; 
}

/***************************************************
 * Same as continueTask but on 1 processor only
 * ************************************************/
void BNRTask::continueTaskSerial()
{
  switch (status_)
  {
          case NEW:                                                             //0
                  goto TASK_START;
          case WAITING_FOR_CHILDREN:                                            //8
                  goto WAIT_FOR_CHILDREN;
          default:
                  char error[100];
                  sprintf(error,"Error invalid status (%d) in serial task\n",status_);
                  throw InternalError(error,__FILE__,__LINE__);
  }
                  
  TASK_START:
          
  offset_=-patch_.getLow();
  
  //compute local signatures
  ComputeLocalSignature();

  BoundSignatures();  
  
  total_flags_=flags_.size;
  
  CheckTolA();
  if(acceptable_)
  {
    my_patches_.push_back(patch_);
      
    //signature is no longer needed so free memory
    count_.clear();
  }
  else
  {
    ctasks_.split=FindSplit();
      
    //split the current patch
    ctasks_.left=ctasks_.right=patch_;
    ctasks_.left.high()[ctasks_.split.d]=ctasks_.right.low()[ctasks_.split.d]=ctasks_.split.index;
    
    ctasks_.ltag=-1;
    ctasks_.rtag=-1;
     
    //signature is no longer needed so free memory
    count_.clear();
      
    CreateTasks();
    
    status_=WAITING_FOR_CHILDREN;  

    return;
    
    WAIT_FOR_CHILDREN:
    
    if(left_->tag_!=-1 || right_->tag_!=-1)
    {
      controller_->tags_.push(left_->tag_);    //reclaim tag
      controller_->tags_.push(right_->tag_);    //reclaim tag
    }
    //copy patches from left children
    my_patches_.assign(left_->my_patches_.begin(),left_->my_patches_.end());
    my_patches_.insert(my_patches_.end(),right_->my_patches_.begin(),right_->my_patches_.end());
    
    //check tolerance b and take better patchset
    CheckTolB();
    if(!acceptable_)
    {
      my_patches_.resize(0);
      my_patches_.push_back(patch_);
    }
  }
  
  //COMMUNICATE_PATCH_LIST:  
  if( parent_!=0 && parent_->p_group_[0]!=p_group_[0])  //if parent exists and I am not also the master on the parent
  {
    {
      //send up the chain or to the root processor
      my_size_=my_patches_.size();
  
      //send patch count to parent
      MPI_Isend(&my_size_,1,MPI_INT,parent_->p_group_[0],tag_,controller_->d_myworld->getComm(),getRequest());
     
      if(my_size_>0)
      {
        //send patch list to parent
        MPI_Isend(&my_patches_[0],my_size_*sizeof(Region),MPI_BYTE,parent_->p_group_[0],tag_,controller_->d_myworld->getComm(),getRequest());
      }
    }
  }
  
  status_=TERMINATED;
  
  //if parent is waiting activiate parent 
  if(parent_!=0 && sibling_->status_==TERMINATED )
  {
    //place parent on immediate queue 
    controller_->immediate_q_.push(parent_);
  }

  return; 
}

void BNRTask::ComputeLocalSignature()
{
  IntVector size=patch_.getHigh()-patch_.getLow();
  sig_offset_[0]=0;
  sig_offset_[1]=size[0];
  sig_offset_[2]=size[0]+size[1];
  sig_size_=size[0]+size[1]+size[2];
  
  //resize signature count_
  count_.resize(sig_size_);

  //initialize signature
  count_.assign(sig_size_,0);
  
  //count flags
  for(int f=0;f<flags_.size;f++)
  {
      IntVector loc=flags_.locs[f]+offset_;
      count_[loc[0]]++;
      count_[sig_offset_[1]+loc[1]]++;
      count_[sig_offset_[2]+loc[2]]++;
  }
}
void BNRTask::BoundSignatures()
{
    IntVector low;
    IntVector high;
    IntVector size=patch_.getHigh()-patch_.getLow();
    //for each dimension
    for(int d=0;d<3;d++)
    {
      int i;
      //search for first non zero
      for(i=0;i<size[d];i++)
      {
        if(count_[sig_offset_[d]+i]!=0)
          break;
      }
      low[d]=i+patch_.getLow()[d];
      //search for last non zero
      for(i=size[d]-1;i>=0;i--)
      {
        if(count_[sig_offset_[d]+i]!=0)
              break;  
      }
      high[d]=i+1+patch_.getLow()[d];
    }
    patch_=Region(low,high);
}

void BNRTask::CheckTolA()
{
  IntVector size=patch_.getHigh()-patch_.getLow();
  acceptable_= float(total_flags_)/patch_.getVolume()>=controller_->tola_;
}

void BNRTask::CheckTolB()
{
  //calculate patch_ volume of children
  int children_vol=0;
  for(unsigned int p=0;p<my_patches_.size();p++)
  {
      children_vol+=my_patches_[p].getVolume();
  }
  //compare to patch volume of parent
  if(float(children_vol)/patch_.getVolume()>=controller_->tolb_)
  {
    acceptable_=false;
  }
  else
  {
    acceptable_=true;
  }
}
Split BNRTask::FindSplit()
{
  Split split;
  split.d=-1;
  IntVector size=patch_.getHigh()-patch_.getLow();
  //search for zero split in each dimension
  for(int d=0;d<3;d++)
  {
    int index=patch_.getLow()[d]+offset_[d]+1;
    for(int i=1;i<size[d]-1;i++,index++)
    {
      if(count_[sig_offset_[d]+index]==0)
      {
          split.d=d;
          split.index=index-offset_[d];
          return split;
      }    
    }
  }
  //no zero split found  
  //search for second derivitive split
  IntVector mid=(patch_.getLow()+patch_.getHigh())/IntVector(2,2,2);
  int max_change=-1,max_dist=INT_MAX;
    
  for(int d=0;d<3;d++)
  {
    if(size[d]>2)
    {
      int d2, last_d2;
      int s;
      
      int index=patch_.getLow()[d]+offset_[d];
      last_d2=count_[sig_offset_[d]+index+1]-count_[sig_offset_[d]+index];
      int last_s=sign(last_d2);
      index++;
      for(int i=1;i<size[d]-1;i++,index++)
      {
        d2=count_[sig_offset_[d]+index-1]+count_[sig_offset_[d]+index+1]-2*count_[sig_offset_[d]+index];
        s=sign(d2);
        
        //if sign change
        if(last_s!=s)
        {
          int change=abs(last_d2-d2);
          int dist=abs(mid[d]-index);
          //compare to max found sign change and update max
          if(change>=max_change)
          {
            if(change>max_change)
            {
              max_change=change;
              split.d=d;
              split.index=index-offset_[d];
              max_dist=dist;
            }
            else
            {
              //tie breaker - take longest dimension
              if(size[d]>=size[split.d])
              {
                if(size[d]>size[split.d])
                {
                  max_change=change;
                  split.d=d;
                  split.index=index-offset_[d];
                  max_dist=dist;
                }
                else
                {
                  //tie breaker - closest to center
                  if(dist<max_dist)
                  {
                    max_change=change;
                    split.d=d;
                    split.index=index-offset_[d];
                    max_dist=dist;
                  }
                } 
              }  
            }
          }
        }
        
        last_d2=d2;
        last_s=s;
      }
      d2=count_[sig_offset_[d]+index-1]-count_[sig_offset_[d]+index];
      s=sign(d2);
              
      //if sign change
      if(last_s!=s)
      {
        int change=abs(last_d2-d2);
        int dist=abs(mid[d]-index);
        //compare to max found sign change and update max
        if(change>=max_change)
        {
          if(change>max_change)
          {
            max_change=change;
            split.d=d;
            split.index=index-offset_[d];
            max_dist=dist;
          }
          else
          {
            //tie breaker - take longest dimension
            if(size[d]>=size[split.d])
            {
              if(size[d]>size[split.d])
              {
                max_change=change;
                split.d=d;
                split.index=index-offset_[d];
                max_dist=dist;
              }
            }  
          }
        }
      }
    }
  }
  
  if(split.d>=0)
  {
    return split;
  }
  //no second derivitive split found 
  //take middle of longest dim
  int max_d=0;
  for(int d=1;d<3;d++)
  {
    if(size[d]>size[max_d])
            max_d=d;
  }
  split.d=max_d;
  split.index=mid[max_d];
  return split;
}
/************************************************************
 * Broadcast message of size count in a non blocking fashion
 * the return value indicates if there is more broadcasting to 
 * perform on this processor
 * *********************************************************/
bool BNRTask::Broadcast(void *message, int count_, MPI_Datatype datatype)
{
  unsigned int partner;
  //broadcast flagscount_ back to procs
  if(stage_<d_)
  {
    int stride=1<<stage_;
    stage_++;
    if(p_rank_<stride)
    {
      partner=p_rank_+stride;
      if(partner<p_group_.size())
      {
        //Nonblocking send msg to partner
        MPI_Isend(message,count_,datatype,p_group_[partner],tag_,controller_->d_myworld->getComm(),getRequest());
        return true;
      }
    }
    else if(p_rank_< (stride<<1))
    {
      partner=p_rank_-stride;
        
      //Nonblocking recieve msg from partner
      MPI_Irecv(message,count_,datatype,p_group_[partner],tag_,controller_->d_myworld->getComm(),getRequest());  
      return true;
    }
    else
    {
      controller_->immediate_q_.push(this);
      return true;
    }
  }
    
  return false;    
}

void BNRTask::CreateTasks()
{
  FlagsList leftflags_,rightflags_;
  //split the flags
  int front=0, back=flags_.size-1;  
  int d=ctasks_.split.d, v=ctasks_.split.index;
  while(front<back)
  {
    if(flags_.locs[front][d]<v) //place at front
    {
      front++;
    }
    else
    {
      swap(flags_.locs[front],flags_.locs[back]);
      back--;
    }
  }
  if(flags_.locs[front][d]<v)
    front++;
  
  leftflags_.locs=flags_.locs;
  leftflags_.size=front;

  rightflags_.locs=flags_.locs+front;
  rightflags_.size=flags_.size-front;
  
  //create new tasks
  controller_->tasks_.push_back(BNRTask(ctasks_.left,leftflags_,p_group_,p_rank_,this,ctasks_.ltag));
  left_=&controller_->tasks_.back();
  
  controller_->tasks_.push_back(BNRTask(ctasks_.right,rightflags_,p_group_,p_rank_,this,ctasks_.rtag));
  right_=&controller_->tasks_.back();
  
  left_->setSibling(right_);  
  right_->setSibling(left_);  

  controller_->immediate_q_.push(left_);
  controller_->immediate_q_.push(right_);
  controller_->task_count_+=2;
}

  
