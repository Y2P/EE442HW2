#include <stdio.h>
#include <semaphore.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

#define MAXSIZE 	10
#define BASE        0x20000000
#define SEMSTCFULL	"/semstc_full"
#define SEMCTSFULL	"/semcts_full"
#define SEMSTCEMPTY	"/semstc_empty"
#define SEMCTSEMPTY	"/semcts_empty"

struct server_message 
{
	pthread_t client_id;
	pid_t server_id;
	int answer;
};
struct client_message 
{
	pthread_t client_id;
	int question;
};
struct SharedMem
{

	int NumOfQueueElem;
	int STCFront ;
	int STCRear ;
	struct server_message STC_Q[MAXSIZE];
	int CTSFront ;
	int CTSRear ;
	struct client_message CTS_Q[MAXSIZE];
};


int SharedMemSize;
int ret;
int fd;

sem_t *STC_sem_full;
sem_t *STC_sem_empty;
sem_t *CTS_sem_full; 
sem_t *CTS_sem_empty; 

void* s;
struct SharedMem *SM;
int main(int argc, char const *argv[])
{
	if(argc > 2)
	{
		SharedMemSize = atoi(argv[2]);
	}
	else
		SharedMemSize = 10;

	int RegionSize = sizeof(int)*3  + SharedMemSize*sizeof(struct server_message) + sizeof(int)*2 +SharedMemSize*sizeof(struct client_message);
	printf("%d\n",RegionSize );
// Initialize the queue semaphores
	STC_sem_full = sem_open(SEMSTCFULL,O_CREAT,0644,3);
	STC_sem_empty = sem_open(SEMSTCEMPTY,O_CREAT,0644,3);

	CTS_sem_full = sem_open(SEMCTSFULL,O_CREAT,0644,3);
	CTS_sem_empty = sem_open(SEMCTSEMPTY,O_CREAT,0644,3);

	sem_init(&(*CTS_sem_full),1,SharedMemSize);
	sem_init(&(*STC_sem_full),1,SharedMemSize);
	sem_init(&(*CTS_sem_empty),1,0);
	sem_init(&(*STC_sem_empty),1,1);

	fd = shm_open("/shared", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
	if(fd == -1)
		printf("The memory cannot created!!!\n");

	if(ftruncate(fd,sizeof(struct SharedMem )) == -1)
		printf("Truncation is failed!!!\n");

	SM = (struct SharedMem*)mmap((void *)0x20000000,RegionSize,PROT_READ|PROT_WRITE,
				MAP_FIXED|MAP_SHARED,fd,0);

	if (SM == MAP_FAILED)
		printf("Mapping is failed!!!\n");
	
	
	SM->NumOfQueueElem = SharedMemSize;
	
	SM->STCFront = 0;
	SM->STCRear = 0;
	SM->CTSFront = 0;
	SM->CTSRear = 0;
	SM->CTS_Q[9].client_id =5 ;
	SM->CTS_Q[19].client_id =5 ;

   printf("Please press enter to finish the server operation\n");
   getc(stdin);

 //  free(SM->STC_Q);
   //free(SM->CTS_Q);
   // Unmap the object
   munmap(SM,sizeof(struct SharedMem) );
   sem_unlink(SEMSTCFULL);
   sem_unlink(SEMCTSFULL);
   sem_unlink(SEMSTCEMPTY);
   sem_unlink(SEMCTSEMPTY);
   // Close the shared memory object handle
   close(fd);
   // Remove the shared memory object
   shm_unlink("/shared");

   return 0;
}