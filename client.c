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


#define BASE        0x20000000
#define MAXSIZE 	10
#define SEMSTCFULL	"/semstc_full"
#define SEMCTSFULL	"/semcts_full"
#define SEMSTCEMPTY	"/semstc_empty"
#define SEMCTSEMPTY	"/semcts_empty"

pthread_mutex_t writelock1;
pthread_mutex_t writelock2;


// Necessary integer values
int ThreadNum;
int fd;

// Semaphores are created
// The wait conditions are condisered for chosing their missions
sem_t *STC_sem_full; // Wait semaphore when Server to Client (STC) queue is full
sem_t *STC_sem_empty; // Wait semaphore when Server to Client (STC) queue is empty
sem_t *CTS_sem_full; // Wait semaphore when Client to Server (CTS) queue is full
sem_t *CTS_sem_empty;  // Wait semaphore when Client to Server (CTS) queue is empty


// Message Formats are defined here
struct client_message 
{
	pthread_t client_id;
	int question;
};
struct server_message 
{
	pthread_t client_id;
	pid_t server_id;
	int answer;
};
// Shared Memory Prototype
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

// Sender thread
void* sender(void* SM)
{

	// Read the necessary semaphores
	CTS_sem_full = sem_open(SEMCTSFULL,0);
	CTS_sem_empty = sem_open(SEMCTSEMPTY,0);

	// If CTS full, block the thread
	sem_wait(CTS_sem_full);

	// Initialize the temporary variables
	struct client_message m;
	struct SharedMem* s;

	// Lock the write to Shared Memory
	
	pthread_mutex_lock(&writelock1);

		s = ((struct SharedMem*)SM);
		pthread_t i = pthread_self();
		m.client_id = i;

		// Keep question in 3 digit
		m.question = rand()%1000;
		if(m.question < 100)
			m.question  = m.question +100;
		// Push new message
		printf("id : %d m: %d \n",i,m.question );
		s->CTS_Q[s->CTSFront] = m;
		// Circular queue
		(++s->CTSFront);
		s->CTSFront = (s->CTSFront)%(s-> NumOfQueueElem);
		sem_post(CTS_sem_empty);
	// Unlock the operation 
	pthread_mutex_unlock(&writelock1);
}

// Reciever method is defined here
void* reciever(void* SM)
{	
	
	// Initialize the random number generator
	srand(time(NULL) ^ pthread_self()<<16);
	// Read the necessary semaphores

	STC_sem_empty = sem_open(SEMSTCEMPTY,0);
	STC_sem_full = sem_open(SEMSTCFULL,0);

	// Wait if STC is empty
	sem_wait(STC_sem_empty);
	// Define temporary variables
	struct SharedMem* s;
	// Lock the pop operation on Shared Memory
	pthread_mutex_lock(&writelock2);

		s = ((struct SharedMem*)SM);
		// Print the result from server
		printf("From server process %d to client thread %d - Result: %d\n",(int)s->STC_Q[s->STCRear].server_id,(int)s->STC_Q[s->STCRear].client_id ,(int)s->STC_Q[s->STCRear].answer);
		// Pop the message in the queue
		s->STC_Q[s->STCRear].client_id =0;
		s->STC_Q[s->STCRear].server_id =0;
		s->STC_Q[s->STCRear].answer =0;
		// Circular queue indexes are adjusted
		s->STCRear++;
		s->STCRear = s->STCRear % s->NumOfQueueElem;
		sem_post(STC_sem_full);
	pthread_mutex_unlock(&writelock2);
}
// Necessary variables are defined here
struct SharedMem* SM;
void *takesize;
pthread_t* tid_sender;
pthread_t* tid_receiver;
void* b;
int i= 0;
int main(int argc, char const *argv[])
{

	// Client main process 
	char buffer[30];
	time_t raw;
	struct tm * timeinfo;
	time(&raw);
	timeinfo = localtime(&raw);
	strftime(buffer,30,"%a %b %d %X %Y",timeinfo );
	printf("Client program started at %s \n", buffer);

	// Read the number of threads
	if(argc > 2)
	{
		ThreadNum = atoi(argv[2]);
	}
	else
		ThreadNum = 10;
// Open the shared memory
	fd = shm_open("/shared",O_RDWR,0600);
// Shared Memory is read
	takesize = mmap((void *)0x20000000,sizeof(int),PROT_READ|PROT_WRITE,MAP_FIXED|MAP_SHARED,fd,0); 
// Reciever and sender threads are created.
	int Qsize=*(int*)takesize;
	int RegionSize = 3*sizeof(int) + Qsize*sizeof(struct server_message) + 2*sizeof(int) +Qsize*sizeof(struct client_message);

	// Open the shared file
	fd = shm_open("/shared",O_RDWR,0600);
	// Read the whole file
	b = (void*)mmap((void *)0x20000000,RegionSize,PROT_READ|PROT_WRITE,MAP_FIXED|MAP_SHARED,fd,0);
	// Type cast raw data to prototype
	SM = ((struct SharedMem*)b );

	// Allocate the sender and reciever threads
	tid_sender= (pthread_t*)malloc(ThreadNum*sizeof(pthread_t));
	tid_receiver= (pthread_t*)malloc(ThreadNum*sizeof(pthread_t));

	for ( i = 0;i<ThreadNum;i++)
	{
		// Create and spawn the threads
		pthread_create(&(tid_sender[i]),NULL,&sender, (void*) SM);
		pthread_create(&(tid_receiver[i]),NULL,&reciever,(void*)SM);
	}

// Release the allocated memories
	// Wait number of thread seconds by blocking the main process.
	sleep(ThreadNum);
	munmap(SM,RegionSize);

	close(fd);
	time(&raw);
	timeinfo = localtime(&raw);
	strftime(buffer,30,"%a %b %d %X %Y",timeinfo );
	printf("Client program closed at %s \n", buffer);
	return 0;
}
