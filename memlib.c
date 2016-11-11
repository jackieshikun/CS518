#include "memlib.h"

#define MEMORY_SIZE 1024*1024*8
#define PAGE_SIZE sysconf(_SC_PAGE_SIZE)

//TODO: Need to replace this declaration to memalign() 
static char memory_space[MEMORY_SIZE];	//total memory
void *memory_base = NULL;

//for memory allocation within a thread
typedef struct thread_block_meta
{
	size_t size;		// shows space can be allocated. exclude the size of meta block
	int isFree;			// 1 is for free; 0 for not free
	//TODO: Need to add more members?
}tb_meta;
#define THREAD_META_SIZE sizeof(struct thread_block_meta)

/*Page table part*/
typedef struct pt_node
{
	int owner_id;		//thread id
	int index;			//index of the page of current thread
	struct pt_node *next;		//indicate the next page
	void *map_to_addr;  //the physical memory this page mapped to
	void *current_addr;	//the physical memory this page currently swapped to
	int padding;		//only for 8 bytes alignment in x86, useless
}pt_node;

//should be less than 2048 in most case. whatever...
static pt_node page_table[2048];

//initialize page table
void init_page_table()
{
	for (int i = 0 ; i < 2048 ; i++)
	{
		page_table[i].owner_id = -1;	//unused node has id -1
		page_table[i].index = -1;
		page_table[i].next = NULL;
		page_table[i].map_to_addr = NULL;
		page_table[i].current_addr = NULL;
	}
}

/*Instrument part*/
//TODO: Finish this function
void defragmentation()
{

}

/*@return: the thread block meta if success; NULL if no space to allocate
 * */
//TODO: check correctness
void *find_free_space(size_t size, block_meta *block)
{
	tb_meta *t_block = (tb_meta *)(block + 1);
	tb_meta *temp = t_block;
	int mark = 0;
	size_t size_counter = 0;
	while (size_counter < PAGE_SIZE - META_SIZE)
	{
		if (t_block->isFree == 1 && t_block->size >= size)
		{
			if (t_block->size < size + THREAD_META_SIZE + 1)
			{	
				//return the whole space, because the remaining space is not enough for a meta block
				t_block->isFree = 0;
			}	
			else
			{
				//write the block meta information for the remaining memory space 
				temp = (tb_meta *)((char *)(temp + 1) + size);
				temp->isFree = 1;
				temp->size = t_block->size - size - THREAD_META_SIZE;

				/*We can add defragmentation operation function here later*/

				t_block->isFree = 0;
				t_block->size = size;
			}
			mark = 1;
			break;
		}

		//increment counter and move the t_block to the next
		size_counter += (THREAD_META_SIZE + t_block->size);
		t_block = (tb_meta*)((char*)(block + 1) + size_counter);		
	}
	if (mark == 1)
		return t_block;
	else
		return NULL;
}

/*Instrument part end*/

void *malloc_lib(size_t size)
{
	block_meta *block;
	tb_meta *t_block;

	if (queue_size() > 0)
	{
		int page_number = dequeue();
		block = (block_meta *)((char*)memory_base + page_number * PAGE_SIZE);
		block->page_num = page_number;
		block->size = PAGE_SIZE;
		block->owner_id = current_thread_id;

		//insert page table
		int i = 0;
		while (i < 2048 && page_table[i].owner_id != -1)
			i++;
		page_table[i].owner_id = current_thread_id;
		page_table[i].map_to_addr = block;
		page_table[i].current_addr = block;

		//initialize the thread block
		t_block= (tb_meta*)(block + 1);
		t_block->size = PAGE_SIZE - META_SIZE - THREAD_META_SIZE;
		t_block->isFree = 1;
	}
	else
		return NULL;
	return block;
}

void *malloc_thread(size_t size)
{
	//find the page of current thread
	block_meta *block;
	int i;
	for (i = 0 ; i < 2048 ; i++)
	{
		if (page_table[i].owner_id == current_thread_id)
		{
			//Phase A; later we need to consider about the swap in Phase B here
			block = page_table[i].map_to_addr;
			break;
		}
	}

	//check the allocated_size and find the free place to allocate memory
	tb_meta *t_block = find_free_space(size, block);
	return t_block;
}
 
void *myallocate(size_t size, char FILE[], int LINE, int type)
{
	if (size <= 0 || size >= 4096)
	{
		return NULL;
	}

	//first call, initialize queue and page table
	if (memory_base == NULL)
	{
		init_queue();
		init_page_table();
		memory_base = memory_space;
	}

    //choose which function to deal with the request
	if (type == THREADREQ)
	{
		tb_meta *t_block = (tb_meta*)malloc_thread(size);
		if (t_block == NULL)
			return NULL;
		else
			return t_block + 1;
	}
	else if (type == LIBRARYREQ)                                                                                                             	{
		block_meta *block = (block_meta *)malloc_lib(size);
		if (block == NULL)
			return NULL;
		else
			return block + 1;
	}
	else 
		printf("Wrong request type!\n");
	return NULL; 
}

void mydeallocate(void *ptr, char FILE[], int LINE, int type)
{
	if (!ptr)
		return;
	if (type == THREADREQ)
	{
		tb_meta *t_block = ((tb_meta *) ptr) - 1;
		t_block->isFree = 1;
	}
	else if (type == LIBRARYREQ)
	{
		block_meta *block = ((block_meta *) ptr) - 1;
		enqueue(block->page_num);
	}
	else
		printf("Wrong request type!\n");
}

int main()
{
	int thread_id_1 = 1;
	int thread_id_2 = 2;
    int thread_id_3 = 3;

	current_thread_id = 1;
	void *thread_1 = myallocate(4000, NULL, 0, LIBRARYREQ);
	current_thread_id = 2;
	void *thread_2 = myallocate(4000, NULL, 0, LIBRARYREQ);

	printf("thread 1 address: %x\n", thread_1);
	printf("thread 2 address: %x\n", thread_2);

	current_thread_id = 1;
	void *t1_proc1 = myallocate(4056, NULL, 0, THREADREQ);
	current_thread_id = 1;
	mydeallocate(t1_proc1, NULL, 0, THREADREQ);
	void *t1_proc2 = myallocate(1, NULL, 0, THREADREQ);

	current_thread_id = 2;
	mydeallocate(thread_2, NULL, 0, LIBRARYREQ);
	current_thread_id = 3;
	void *thread_3 = myallocate(4000, NULL, 0, LIBRARYREQ);

	printf("thread 1 proc 1 address: %x\n", t1_proc1);
	printf("thread 1 proc 2 address: %x\n", t1_proc2);

	return 0;
}