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
#include  <signal.h>


#define MAXSIZE 	10
#define BASE        0x20000000
#define SEMSTCFULL	"/semstc_full"
#define SEMCTSFULL	"/semcts_full"
#define SEMSTCEMPTY	"/semstc_empty"
#define SEMCTSEMPTY	"/semcts_empty"

// Memory write operation locks are initialized
pthread_mutex_t writelock;
pthread_mutex_t writelock2;

// Message Formats are initialized
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

// Shared Memory is prototyped here. It is used as a template further casting operations
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
// Signal ISR initialized
void INThandler(int);

// Necessary integer values
int SharedMemSize;
int ret;

// Semaphores are created
// The wait conditions are condisered for chosing their missions
sem_t *STC_sem_full; // Wait semaphore when Server to Client (STC) queue is full
sem_t *STC_sem_empty; // Wait semaphore when Server to Client (STC) queue is empty
sem_t *CTS_sem_full; // Wait semaphore when Client to Server (CTS) queue is full
sem_t *CTS_sem_empty;  // Wait semaphore when Client to Server (CTS) queue is empty



// Shared memory structure pointer is defined 
struct SharedMem *SM;
// Status for waitpid
int status=0;
int fd;
// Indicator for parent process.
int x = 0;

// ISR for catching CTRL-C
void  INThandler(int sig)
{
   // Unlink and unmap operations
   munmap(SM,sizeof(struct SharedMem) );
   sem_unlink(SEMSTCFULL);
   sem_unlink(SEMCTSFULL);
   sem_unlink(SEMSTCEMPTY);
   sem_unlink(SEMCTSEMPTY);
   // Close the shared memory object handle
   close(fd);
   // Remove the shared memory object
   shm_unlink("/shared");
  	if(x == 1)
	{
		x++;
		printf("Do you have to go .... \n");
		printf("See ya!!! \n");
		// Print the modification time in proper format
		char buffer[30];
		time_t raw;
		struct tm * timeinfo;
		time(&raw);
		timeinfo = localtime(&raw);
		strftime(buffer,30,"%a %b %d %X %Y",timeinfo );
		printf("Server program closed at %s \n", buffer);
	}

   exit(0);

}

int main(int argc, char const *argv[])
{
	srand(time(NULL) ^ (getpid()<<16));

	// Print the modification time in proper format
	char buffer[30];
	time_t raw;
	struct tm * timeinfo;
	time(&raw);
	timeinfo = localtime(&raw);
	strftime(buffer,30,"%a %b %d %X %Y",timeinfo );
	printf("Server program started at %s \n", buffer);

	// Get memory size
	if(argc > 2)
	{
		SharedMemSize = atoi(argv[2]);
	}
	else
		SharedMemSize = 10;

	// Calculate the necessary memory size (in byte)
	int RegionSize = sizeof(int)*3  + SharedMemSize*sizeof(struct server_message) + sizeof(int)*2 +SharedMemSize*sizeof(struct client_message);
	/*
	Size
	Front Index
	Rear Index
	Queue
	Front Index
	Rear Index
	Queue
	 */

	// Initialize the queue semaphores
	STC_sem_full = sem_open(SEMSTCFULL,O_CREAT,0644,3);
	STC_sem_empty = sem_open(SEMSTCEMPTY,O_CREAT,0644,3);

	CTS_sem_full = sem_open(SEMCTSFULL,O_CREAT,0644,3);
	CTS_sem_empty = sem_open(SEMCTSEMPTY,O_CREAT,0644,3);

	sem_init(&(*CTS_sem_full),1,SharedMemSize);
	sem_init(&(*STC_sem_full),1,SharedMemSize);
	sem_init(&(*CTS_sem_empty),1,0);
	sem_init(&(*STC_sem_empty),1,0);

	// Open a shared file
	fd = shm_open("/shared", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
	if(fd == -1)
		printf("The memory cannot created!!!\n");

	// Cut the file by the prototyped structure
	if(ftruncate(fd,sizeof(struct SharedMem )) == -1)
		printf("Truncation is failed!!!\n");

	// Map the shared file and output a pointer for this file
	// Type casting is also applied here
	SM = (struct SharedMem*)mmap((void *)0x20000000,RegionSize,PROT_READ|PROT_WRITE,
				MAP_FIXED|MAP_SHARED,fd,0);


	if (SM == MAP_FAILED)
		printf("Mapping is failed!!!\n");
	
	// Initialize the memory	
	SM->NumOfQueueElem = SharedMemSize;
	
	SM->STCFront = 0;
	SM->STCRear = 0;
	SM->CTSFront = 0;
	SM->CTSRear = 0;

	// Set an interrupt for CTRL-C
   	printf("CTRL-C to quit\n");
	signal(SIGINT, INThandler);
	
	// Server is executed here
	while(1)
	{
			// Parent indicator is increased
			x++;
			// Fork a child process
			if(fork() != 0)
				waitpid(-1,&status,0);
			else
			{
			// If it is child 
			// Check CTS is empty
			sem_wait(CTS_sem_empty);
			// Continues if it is not
			// For alteration on shared memory
			// Lock the process
			pthread_mutex_lock(&writelock);
				// Get the client id and question, and pop them
				int passclientid = SM->CTS_Q[SM->CTSRear].client_id;
				SM->CTS_Q[SM->CTSRear].client_id = 0;
				int passquestion = SM->CTS_Q[SM->CTSRear].question;
				SM->CTS_Q[SM->CTSRear].question = 0;
				// Increment on Rear index
				SM->CTSRear++;
				// Circular queue
				SM->CTSRear = SM->CTSRear % SM->NumOfQueueElem;

				// Decrement in CTS_full semaphore for indicating a place is free in queue
				sem_post(CTS_sem_full);
			pthread_mutex_unlock(&writelock);
			usleep(1000*(rand()%1000));
			
			sem_wait(STC_sem_full);
			pthread_mutex_lock(&writelock2);
				SM->STC_Q[SM->STCFront].client_id = passclientid;
				SM->STC_Q[SM->STCFront].server_id =getpid();
				SM->STC_Q[SM->STCFront].answer = 2*passquestion;
				SM->STCFront++;
				SM->STCFront = SM->STCFront % SM->NumOfQueueElem;
				sem_post(STC_sem_empty);
			pthread_mutex_unlock(&writelock2);
			
			}
		}

	
	

   return 0;
}