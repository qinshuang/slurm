#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <string.h>

#include <src/common/log.h>
#include <src/common/slurm_protocol_api.h>
#include <src/slurmd/shmem_struct.h>

/* function prototypes */
void * add_task ( slurmd_shmem_t * shmem , task_t * new_task );
void copy_task ( task_t * dest , task_t * const src );
void copy_job_step ( job_step_t * dest , job_step_t * src );
void clear_task ( task_t * task );
void clear_job_step( job_step_t * job_step );


/* gets a pointer to the slurmd shared memory segment
 * if it doesn't exist, one is created 
 * returns - a void * pointer to the shared memory segment
 */
void * get_shmem ( )
{
	int shmem_id ;
	void * shmem_addr ;
	shmem_id = shmget ( 0 , sizeof ( slurmd_shmem_t ) , IPC_CREAT ); 
	assert ( shmem_id != SLURM_ERROR ) ;
	shmem_addr = shmat ( shmem_id , NULL , 0 ) ;
	assert ( shmem_addr != (void * ) SLURM_ERROR ) ;
	return shmem_addr ;
}

/* initializes the shared memory segment, this should only be called once by the master slurmd 
 * after the initial get_shmem call
 * shmem - pointer to the shared memory segment returned by get_shmem ( )
 */
void init_shmem ( slurmd_shmem_t * shmem )
{
	int i ;
	/* set everthing to zero */
	memset ( shmem , 0 , sizeof ( slurmd_shmem_t ) );
	/* sanity check */
	/* set all task objects to unused */

	for ( i=0 ; i < MAX_TASKS ; i ++ )
	{
		clear_task ( & shmem->tasks[i] ) ;
	}
	
	/* set all job_step objects to unused */
	for ( i=0 ; i < MAX_JOB_STEPS ; i ++ )
	{
		clear_job_step ( & shmem->job_steps[i] ) ;
	}
}

/* runs through the job_step array looking for a unused job_step.
 * upon finding one the passed src job_step is copied into the shared mem job_step array
 * shmem - pointer to the shared memory segment returned by get_shmem ( )
 * job_step_t - src job_step to be added to the shared memory list
 * returns - the address of the assigned job_step in the shared mem job_step array or
 * the function dies on a fatal log call if the array is full
 */
void * add_job_step ( slurmd_shmem_t * shmem , job_step_t * new_job_step ) 
{
	int i ;
	for ( i=0 ; i < MAX_JOB_STEPS ; i ++ )
        {
		if (shmem -> job_steps[i].used == false )
		{
			shmem -> job_steps[i].used = true ;
			copy_job_step ( & shmem -> job_steps[i] , new_job_step );
			return & shmem -> job_steps[i] ;
		} 
        }
		fatal ( "No available job_step slots in shmem segment");
	return (void * ) SLURM_ERROR ;
}

/* runs through the task array looking for a unused task.
 * upon finding one the passed src task is copied into the shared mem task array
 * shmem - pointer to the shared memory segment returned by get_shmem ( )
 * new_task - src task to be added to the shared memory list
 * returns - the address of the assigned task in the shared mem task array
 * the function dies on a fatal log call if the array is full
 */
void * add_task ( slurmd_shmem_t * shmem , task_t * new_task ) 
{
	int i ;
	for ( i=0 ; i < MAX_TASKS ; i ++ )
        {
		if (shmem -> tasks[i].used == false )
		{
			shmem -> tasks[i].used = true ;
			copy_task ( & shmem -> tasks[i] , new_task ) ;
			return & shmem -> tasks[i] ;
		} 
        }
		fatal ( "No available task slots in shmem segment");
	return (void * ) SLURM_ERROR ;
}

/* copies the contents of one task to another, has no memory allocate or dynamic length members */
void copy_task ( task_t * dest , task_t * const src ) 
{
	dest -> threadid 	= src -> threadid;
	dest -> pid		= src -> pid;
	dest -> task_id		= src -> task_id;
	dest -> uid		= src -> uid;
	dest -> gid		= src -> gid;
}

/* copies the contents of one job_step to another, has no memory allocate or dynamic length members */
void copy_job_step ( job_step_t * dest , job_step_t * src )
{
	dest -> job_id		= src -> job_id ;
	dest -> job_step_id	= src -> job_step_id ;
}

/* prepends a new task onto the front of a list of tasks assocuated with a job_step.
 * it calls add_task which copies the passed task into a task array in shared memoery
 * sets pointers from the task to the corresponding job_step array 
 * note if the task array is full,  the add_task function will assert and exiti
 * shmem - pointer to the shared memory segment returned by get_shmem ( )
 * job_step - job_step to receive the new task
 * task - task to be prepended
 */
int prepend_task ( slurmd_shmem_t * shmem , job_step_t * job_step , task_t * task )
{
	task_t * new_task ;
	if ( ( new_task = add_task ( shmem , task ) ) == ( void * ) SLURM_ERROR )
	{
		fatal ( "No available task slots in shmem segment during prepend_task call ");
		return SLURM_ERROR ;
	}
	/* prepend operation*/
	/* newtask next pointer gets head of the jobstep task list */
	new_task -> next = job_step -> head_task ;
	/* newtask pointer becomes the new head of the jobstep task list */
	job_step -> head_task = new_task ;
	/* set back pointer from task to job_step */
	new_task -> job_step = job_step ;
	return SLURM_SUCCESS ;
}

/* clears a job_step and associated task list for future use */
int deallocate_job_step ( job_step_t * jobstep )
{
	task_t * task_ptr = jobstep -> head_task ;
	task_t * task_temp_ptr ; 
	while ( task_ptr != NULL )
	{
		task_temp_ptr = task_ptr -> next ;
		clear_task ( task_ptr ) ;
		task_ptr = task_temp_ptr ;
	}
	clear_job_step ( jobstep ) ;
	return SLURM_SUCCESS ;
}

/* clears a job_step array memeber for future use */
void clear_task ( task_t * task )
{
	task -> used = false ;
	task -> job_step = NULL ;
	task -> next = NULL ;
}

/* clears a job_step array memeber for future use */
void clear_job_step( job_step_t * job_step )
{
	job_step -> used = false ;
	job_step -> head_task = NULL ;
}
