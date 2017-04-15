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
	sem_wait(CTS_sem_full);
	struct client_message m;
	struct SharedMem* s;

	
	pthread_mutex_lock(&writelock1);

		s = ((struct SharedMem*)SM);
		printf("%d\n",s-> NumOfQueueElem);
		pthread_t i = pthread_self();
		m.client_id = i;
		m.question = rand()%1000;
		printf("client_id:%d question: %d\n", (int)m.client_id,(int)m.question);
		s->CTS_Q[s->CTSFront] = m;
		(++s->CTSFront);
		s->CTSFront = (s->CTSFront)%(s-> NumOfQueueElem);
		printf("Sender: %d \n",s->CTSFront);

	pthread_mutex_unlock(&writelock1);
}
void* reciever(void* SM)
{	
	STC_sem_empty = sem_open(SEMSTCEMPTY,0);
	sem_wait(STC_sem_empty);
	struct server_message m;
	struct SharedMem* s;
	
	pthread_mutex_lock(&writelock2);
		pthread_t i = pthread_self();

		s = ((struct SharedMem*)SM);

		s->STC_Q[s->STCFront] = m;
		s->STCFront--;
		printf("Sender: %d \n",(int)i);

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
	printf("Qsize : %d\n",*(int*)takesize );
	int Qsize=*(int*)takesize;
	int RegionSize = 3*sizeof(int) + Qsize*sizeof(struct server_message) + 2*sizeof(int) +Qsize*sizeof(struct client_message);

	printf("%d\n",RegionSize );


	fd = shm_open("/shared",O_RDWR,0600);
	b = (void*)mmap((void *)0x20000000,RegionSize,PROT_READ|PROT_WRITE,MAP_FIXED|MAP_SHARED,fd,0);

	SM = ((struct SharedMem*)b );
	printf("%d\n",(SM)-> NumOfQueueElem);
	printf("%d\n",SM->CTS_Q[99].client_id);
	
	tid_sender= (pthread_t*)malloc(ThreadNum*sizeof(pthread_t));
	tid_receiver= (pthread_t*)malloc(ThreadNum*sizeof(pthread_t));
	for ( i = 0;i<ThreadNum;i++)
	{
		pthread_create(&(tid_sender[i]),NULL,&sender, (void*) SM);
		pthread_create(&(tid_receiver[i]),NULL,&reciever,(void*)SM);
	}

// Release the allocated memories
	sleep(10);
	munmap(SM,RegionSize);

	close(fd);
	return 0;
}
