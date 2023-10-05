/*
 * Driver code for airballoon problem
 */
#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

#define N_LORD_FLOWERKILLER 8
#define NROPES 16	
#define NTHREADS N_LORD_FLOWERKILLER + 3 //Includes Dandelion, Marigold, FlowerKiller and Ballon. Main thread not included.
static volatile int ropes_left = NROPES;
static volatile int threads_remaining = NTHREADS; 

/* Data structures for rope mappings */
struct hook{
	int hook_rope;
};

struct rope{
	struct lock *rope_lock;
	volatile bool is_severed;
};

struct stake{
	struct lock *stake_lock;
	volatile int stake_rope;
};

static struct rope *ropes;
static struct stake *stakes;
static struct hook *hooks;

/* Synchronization primitives */
static struct lock *threads_remaining_lock;
static struct lock *ropes_left_lock;

/*
 * Describe your design and any invariants or locking protocols
 * that must be maintained. Explain the exit conditions. How
 * do all threads know when they are done?
 * 
 * Ropes, stakes and hooks are represented by a pointer to an array of each of their respective structs containing NROPES elements.
 * The `hook` structure contains only the index of the rope it is attached, the `rope` structure contains a lock (exclusive to each rope)
 * and the status if the rope is severed, and the `stake` structure contains the index of the rope it is attached to  and a lock (exclusive
 * to each stake).
 *
 * We have two global variables `ropes_left` and `thread_remaining` to keep track of this informations and one lock for each
 * variable so they can be safely and correctly modified by the different threads. `ropes_left` starts as the number of ropes we have 
 * in the problem and `thread_remaining` will be the number of threads the main thread(airballon) will fork, including Dandalion, 
 * Marigold, FlowerKiller and Ballon.
 *
 * `airballon` thread will first initialize the locks for our global variables, alloc the space for our structs array, and fill the
 * array with NROPES elements each and all the variables of each element. At the start, structs, ropes and stakes will have a 
 * 1:1:1 relation regarding its index, therefore hook and stake at index N of their respective array will be connected to rope at index
 * N of the ropes array. After forking all the other threads `airballon` thread will thread_yeald until it achieve its exit condition of
 * having threads_remaing equal to zero, then it will clean up the memory, print its done statement and exit.
 *
 * `ballon` thread will just thread_yeald until it achieves its exit condition of ropes_left being 0, meaning that all ropes were cut and  
 * Dandelion is free. Before exiting, it print the free statement, acquires `threads_remaining` lock, do its done print, reduce 
 * `threads_remaining` by one, release `threads_remaining` lock and exit.
 *
 * The locking protocols are as follows:
 *   - When trying to modify `threads_remaining` or `ropes_left`, thread must acquire the respective lock, do its operation and release 
 *   lock.
 *   - In order to cut the rope, thread must acquire the specific lock for this rope, change its status, and release the lock.
 *   - In order to access the rope via the stake or change stake's rope, thread must acquire the lock for the stake than the lock for its 
 *   respective rope, do its operations and release both locks.
 *
 * `dandelion` will print its starting statement and get inside a loop. In each loop iteration, it will randomly get its next_hook and 
 *  the rope attached to the hook, than it will check the status of the rope. If not severed, it acquire the rope's lock, otherwise it restart the
 *  loop. When the rope's lock is acquired, it checks if other thread severed the rope when it was trying to acquire the lock. If the rope is severed
 *  it release the rope's lock, and restart the while loop. Otherwise it sets the rope status to severed, print its severed statement, reduce 
 *  `ropes_left` by one, making sure to acquire and release the variable's lock, release the rope's lock and thread_yeld(). It will only exit 
 *  the while loop when it achieve its exiting condition of `ropes_left` being zero. Before exiting, it acquires `threads_remaining` lock, do 
 *  its done print, reduce `threads_remaining` by one, release `threads_remaining` lock and exit.
 * 
 *  `marigold` will have a similar behaviour to `dandelion`, but accessig ropes via stakes, and, before getting the rope, it must acquire the
 *   stake's lock(So flowekiller can't modify the rope attached to the stake), and remember to release the stake's lock right after releasing
 *   the rope's lock or when the first check on rope status fail.

 *  `flowerkill` will print its starting statement and get inside a loop. In each loop iteration, it will randomly select its first stake and
 *  lock it, than it will lock the rope attached to first stake,  it will randomly select the second stake and check if stakes are different and rope
 *  1 is not severed. If not successfull, it releases all the previously acquired locks and restart the loop, otherwise it acquire the rope 
 *  attached to the second stake and check if it is severed, if severed it has to release all the locks acquired and restart the loop. If succesfull,
 *  it switches the ropes attached to each stake, print both switch states, release all 4 locks it is current holding and thread_yield. 
 *  The thread will loop until it achieve its exiting condition of having 1 or less ropes_left. This thread can leave when 1 rope is left since at 
 *  that point it will not have enough ropes and stakes to do the switch. Before exiting, it acquires `threads_remaining` lock, do its done print,
 *	reduce `threads_remaining by one, release `threads_remaining` lock and exit.
 * 
 */

static
void
dandelion(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;

	kprintf("Dandelion thread starting\n");

	//Looping until no ropes are left
	while(ropes_left > 0){
		int next_hook = random() % NROPES;
		int next_rope = hooks[next_hook].hook_rope;

		//Check to restart loop if connected rope is severed
		if(ropes[next_rope].is_severed == false){
			lock_acquire(ropes[next_rope].rope_lock);
			
			//Make sure state of rope have not been changed between first check and acquiring the lock
			if(ropes[next_rope].is_severed == true){
				lock_release(ropes[next_rope].rope_lock);
				continue;
			}
			ropes[next_rope].is_severed = true;
			kprintf("Dandelion severed rope %d\n", next_rope);

			lock_acquire(ropes_left_lock);
			ropes_left--;
			lock_release(ropes_left_lock);

			lock_release(ropes[next_rope].rope_lock);
			thread_yield();

		}
	}
	
	lock_acquire(threads_remaining_lock);
	kprintf("Dandelion thread done\n");
	threads_remaining--;
	lock_release(threads_remaining_lock);
}

static
void
marigold(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;

	kprintf("Marigold thread starting\n");

	while(ropes_left > 0){
		int next_stake = random() % NROPES;
		lock_acquire(stakes[next_stake].stake_lock); //Acquire stake so connected rope can't be modified

		int next_rope = stakes[next_stake].stake_rope;

		//Check to restart loop if connected rope is severed
		if(ropes[next_rope].is_severed == false){
			lock_acquire(ropes[next_rope].rope_lock);

			//Make sure state of rope have not been changed between first check and acquiring the lock
			if(ropes[next_rope].is_severed == true){
				lock_release(ropes[next_rope].rope_lock);
				lock_release(stakes[next_stake].stake_lock);		
				continue;
			}

			ropes[next_rope].is_severed = true;
			kprintf("Marigold severed rope %d from stake %d\n", next_rope, next_stake);

			lock_acquire(ropes_left_lock);
			ropes_left--;
			lock_release(ropes_left_lock);

			lock_release(ropes[next_rope].rope_lock);
			
			lock_release(stakes[next_stake].stake_lock);		

			thread_yield();

		} else{
			//make sure to release the stake's lock if attached rope is severed already
			lock_release(stakes[next_stake].stake_lock);		
		}
	}

	lock_acquire(threads_remaining_lock);
	kprintf("Marigold thread done\n");
	threads_remaining--;
	lock_release(threads_remaining_lock);
}

static
void
flowerkiller(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;

	kprintf("Lord FlowerKiller thread starting\n");

	while(ropes_left > 1){
		int stake_1 = random() % NROPES;
	
		lock_acquire(stakes[stake_1].stake_lock);

		int rope_1 = stakes[stake_1].stake_rope;

		lock_acquire(ropes[rope_1].rope_lock);

		int stake_2 = random() % NROPES;
		
		//Check rope 1 is not severed and that stakes are not the same
		if(ropes[rope_1].is_severed == true || stake_2 == stake_1){
			//Release all prevously acquired locks, restart the loop
			lock_release(ropes[rope_1].rope_lock);
			lock_release(stakes[stake_1].stake_lock);		
			continue;
		}

		lock_acquire(stakes[stake_2].stake_lock);
		int rope_2 = stakes[stake_2].stake_rope;

		lock_acquire(ropes[rope_2].rope_lock);
		//Check rope 2 is not severed
		if(ropes[rope_2].is_severed == true){
			//Release all prevously acquired locks, restart the loop
			lock_release(ropes[rope_1].rope_lock);
			lock_release(stakes[stake_1].stake_lock);
			lock_release(ropes[rope_2].rope_lock);
			lock_release(stakes[stake_2].stake_lock);
			continue;
		}

		//Switch ropes between the stakes
		stakes[stake_1].stake_rope = rope_2;
		stakes[stake_2].stake_rope = rope_1;

		//Both switched rope statements merged into one print so they are back-to-back.
		kprintf("Lord FlowerKiller switched rope %d from stake %d to stake %d\nLord FlowerKiller switched rope %d from stake %d to stake %d\n",
				rope_2, stake_2, stake_1,
				rope_1, stake_1, stake_2
		);		

		//Release all acquired locks
		lock_release(ropes[rope_1].rope_lock);
		lock_release(stakes[stake_1].stake_lock);
		lock_release(ropes[rope_2].rope_lock);
		lock_release(stakes[stake_2].stake_lock);

		thread_yield();

		
	}

	lock_acquire(threads_remaining_lock);
	kprintf("Lord FlowerKiller thread done\n");
	threads_remaining--;
	lock_release(threads_remaining_lock);
}

static
void
balloon(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;

	kprintf("Balloon thread starting\n");

	//Thread yield until all ropes are severed (Ballon is free)
	while(ropes_left > 0) {
		thread_yield();
	}

	kprintf("Balloon freed and Prince Dandelion escapes!\n");

	lock_acquire(threads_remaining_lock);
	kprintf("Balloon thread done\n");
	threads_remaining--;
	lock_release(threads_remaining_lock);
}


int
airballoon(int nargs, char **args)
{

	int err = 0, i;

	(void)nargs;
	(void)args;

	// Setup data structures representing hooks, stakes and ropes
	ropes = kmalloc(sizeof(struct rope) * NROPES);
	stakes = kmalloc(sizeof(struct stake) * NROPES);
	hooks = kmalloc(sizeof(struct hook) * NROPES);

	for(i = 0; i < NROPES; i++){
		ropes[i].is_severed = false;
		ropes[i].rope_lock = lock_create("");
		stakes[i].stake_lock = lock_create("");
		stakes[i].stake_rope = i;
		hooks[i].hook_rope = i;
	}

	//Set up locks for global variables
	threads_remaining_lock = lock_create("thread remaining");
	ropes_left_lock = lock_create("ropes left");

	//Fork other threads
	err = thread_fork("Marigold Thread",
			  NULL, marigold, NULL, 0);
	if(err)
		goto panic;

	err = thread_fork("Dandelion Thread",
			  NULL, dandelion, NULL, 0);
	if(err)
		goto panic;

	for (i = 0; i < N_LORD_FLOWERKILLER; i++) {
		err = thread_fork("Lord FlowerKiller Thread",
				  NULL, flowerkiller, NULL, 0);
		if(err)
			goto panic;
	}

	err = thread_fork("Air Balloon",
			  NULL, balloon, NULL, 0);
	if(err)
		goto panic;
	
	//Thread yield until all other threads are done
	while(threads_remaining > 0){
		thread_yield();
	}

	//Set up global variables again, so airballon works if called again
	ropes_left = NROPES;
	threads_remaining = NTHREADS;

	//Clean up locks and datastructures created
	lock_destroy(threads_remaining_lock);
	lock_destroy(ropes_left_lock);
	for (int i = 0; i < NROPES; i++){
		lock_destroy(ropes[i].rope_lock);
		lock_destroy(stakes[i].stake_lock);
	}
	kfree(hooks);
	kfree(stakes);
	kfree(ropes);


	goto done;
panic:
	panic("airballoon: thread_fork failed: %s)\n",
	      strerror(err));

done:
	kprintf("Main thread done\n");
	return 0;
}
