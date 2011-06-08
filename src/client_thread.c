#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/time.h>     //Added to use function gettimeofday()

#define PORT 8181     /* port number as an integer */
#define IP_ADDRESS "10.0.2.15" /* IP address as a string */

#define BUFSIZE 8196


pthread_t *clientthreads;//Added to use threads
int threadscreated;

int nthreads = 1;	//N Number of Threads, initially 1
int mrequests = 1;	//M Number of Request, initially 1


pthread_mutex_t count_mutex;
pthread_cond_t count_threshold_cv;

pexit(char * msg)
{
	perror(msg);
	exit(1);
}

void make_nrequest(int id)
{
    //wait for threads
    pthread_mutex_lock(&count_mutex);
    while (threadscreated < nthreads)
        pthread_cond_wait(&count_threshold_cv, &count_mutex);
    pthread_mutex_unlock(&count_mutex);
    int j;
    //requests to the server
    for(j=0;j<mrequests;j++) {
        int i,sockfd;
        char buffer[BUFSIZE];
        static struct sockaddr_in serv_addr;
        printf("client %d trying to connect to %s and port %d for request %d\n",id,IP_ADDRESS,PORT, j);
        if((sockfd = socket(AF_INET, SOCK_STREAM,0)) <0)
        pexit("socket() failed");

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = inet_addr(IP_ADDRESS);
        serv_addr.sin_port = htons(PORT);

        if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) <0)
        pexit("connect() failed");

        /* now the sockfd can be used to communicate to the server */
        write(sockfd, "GET /index.html \r\n", 18);
        /* note second space is a delimiter and important */

        /* this displays the raw HTML file as received by the browser */
        while( (i=read(sockfd,buffer,BUFSIZE)) > 0);
        write(1,buffer,i);
        //printf("client %d finished getting response from %s and port %d for request %d\n",id,ipaddress,port,k);
	}
    pthread_exit(NULL);
}

int main(int argc, char **argv)
{
	//struct timeval first;
	//Vars to use to calculate the time
	struct timeval init_time, final_time;
	double time_delay;
	//get the intial time
	gettimeofday(&init_time,NULL);
    int selection;
	//parsing
        while ((selection = getopt (argc, argv, "n:m:?")) != -1)
          switch (selection)
            {
            case 'n':
              nthreads = atoi(optarg);
              break;
        case 'm':
             mrequests = atoi(optarg);
             break;
        case '?':
			 printf("Client receive 2 arguments:\n-t number_of_threads\n -r number_of_requests_per_thread\n");
             return 1;
         break;
            }

	//initialization of thread, mutex and condition
	int rc;
	clientthreads = (pthread_t *)malloc(sizeof(pthread_t)*nthreads);
	int i;
	pthread_mutex_init(&count_mutex, NULL);
   	pthread_cond_init (&count_threshold_cv, NULL);
	pthread_mutex_lock(&count_mutex);

	threadscreated = 0;
	//create the threads
	for(i=0; i<nthreads; i++){
	rc = pthread_create(&clientthreads[i], NULL, (void *(*)(void *))&make_nrequest, (void*)i);
	if (rc) {
          printf("ERROR; return code from pthread_create() is %d\n", rc);
          exit(-1);
    }
    else {
	threadscreated++;
	}
	}

//all threads have been created
	pthread_cond_broadcast(&count_threshold_cv);
	pthread_mutex_unlock(&count_mutex);

//wait for all threads
	for(i=0; i<nthreads; i++){
	rc = pthread_join(clientthreads[i],NULL);
	if (rc) {
          printf("ERROR; return code from pthread_join() is %d\n", rc);
          exit(-1);
  	}
	}

	//get the final time
	gettimeofday(&final_time,NULL);
	//total time calculation on miliseconds
	time_delay = (final_time.tv_sec - init_time.tv_sec)*1000 + (final_time.tv_usec - init_time.tv_usec)/1000.0;
    printf("Connection time: %g miliseconds\n", time_delay);
}
