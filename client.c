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
int ThreadNum;
int ret;
sem_t *STC_sem_full;
sem_t *STC_sem_empty;
sem_t *CTS_sem_full; 
sem_t *CTS_sem_empty; 

int fd;
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


void* sender(void* SM)
{

	CTS_sem_full = sem_open(SEMCTSFULL,0);
	CTS_sem_empty = sem_open(SEMCTSEMPTY,0);
	sem_wait(CTS_sem_full);
	sem_post(CTS_sem_empty);
	struct client_message m;
	struct SharedMem* s;

	
	pthread_mutex_lock(&writelock1);

		s = ((struct SharedMem*)SM);
		pthread_t i = pthread_self();
		m.client_id = i;
		m.question = rand()%1000;
		s->CTS_Q[s->CTSFront] = m;
		(++s->CTSFront);
		s->CTSFront = (s->CTSFront)%(s-> NumOfQueueElem);

	pthread_mutex_unlock(&writelock1);
}
void* reciever(void* SM)
{	
	STC_sem_empty = sem_open(SEMSTCEMPTY,0);
	STC_sem_full = sem_open(SEMSTCFULL,0);

	sem_wait(STC_sem_empty);
	sem_post(STC_sem_full);
	struct SharedMem* s;
	
	pthread_mutex_lock(&writelock2);
		pthread_t i = pthread_self();
		s = ((struct SharedMem*)SM);
		printf("From server process %d to client thread %d - Result: %d\n",(int)s->STC_Q[s->STCRear].server_id,(int)i,(int)s->STC_Q[s->STCRear].answer);
		s->STC_Q[s->STCRear].client_id =0;
		s->STC_Q[s->STCRear].server_id =0;
		s->STC_Q[s->STCRear].answer =0;
		s->STCRear++;
		s->STCRear = s->STCRear % s->NumOfQueueElem;

	pthread_mutex_unlock(&writelock2);
}
struct SharedMem* SM;
void *takesize;
pthread_t* tid_sender;
pthread_t* tid_receiver;
void* b;
int i= 0;
int main(int argc, char const *argv[])
{
	char buffer[30];
	time_t raw;
	struct tm * timeinfo;
	time(&raw);
	timeinfo = localtime(&raw);
	strftime(buffer,30,"%a %b %d %X %Y",timeinfo );
	printf("Client program started at %s \n", buffer);
	if(argc > 2)
	{
		ThreadNum = atoi(argv[2]);
	}
	else
		ThreadNum = 10;

	fd = shm_open("/shared",O_RDWR,0600);
// Shared Memory is read
	takesize = mmap((void *)0x20000000,sizeof(int),PROT_READ|PROT_WRITE,MAP_FIXED|MAP_SHARED,fd,0); 
// Reciever and sender threads are created.
	int Qsize=*(int*)takesize;
	int RegionSize = 3*sizeof(int) + Qsize*sizeof(struct server_message) + 2*sizeof(int) +Qsize*sizeof(struct client_message);

	fd = shm_open("/shared",O_RDWR,0600);
	b = (void*)mmap((void *)0x20000000,RegionSize,PROT_READ|PROT_WRITE,MAP_FIXED|MAP_SHARED,fd,0);

	SM = ((struct SharedMem*)b );

	
	tid_sender= (pthread_t*)malloc(ThreadNum*sizeof(pthread_t));
	tid_receiver= (pthread_t*)malloc(ThreadNum*sizeof(pthread_t));
	for ( i = 0;i<ThreadNum;i++)
	{
		pthread_create(&(tid_sender[i]),NULL,&sender, (void*) SM);
		pthread_create(&(tid_receiver[i]),NULL,&reciever,(void*)SM);
	}

// Release the allocated memories
	sleep(ThreadNum);
	munmap(SM,RegionSize);

	close(fd);
	time(&raw);
	timeinfo = localtime(&raw);
	strftime(buffer,30,"%a %b %d %X %Y",timeinfo );
	printf("Client program closed at %s \n", buffer);
	return 0;
}
