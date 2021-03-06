#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
//////////////////////////Include the pthread library//////////////////////////
#include <pthread.h>
///////////////////////////////////////////////////////////////////////////////

////////////////////////////////////Define's///////////////////////////////////
#define BUFSIZE 8096
#define ERROR 42
#define SORRY 43
#define LOG   44
///////////////////////////////////////////////////////////////////////////////
void weblog(int, char *, char *, int);
void *web();
void initialize_pthreads();
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////Global//////////////////////////////////////
pthread_t *servert;
int numthreads = 0, gfd, ghit;
//////////////////////////////////////////////////////////////////////////////
struct handler_s{
	int ntfree;
	int rqts;
	pthread_mutex_t mutexr;
	pthread_cond_t condfree;
	pthread_cond_t condfull;
	} handler_t = {0,0,PTHREAD_MUTEX_INITIALIZER,PTHREAD_COND_INITIALIZER,PTHREAD_COND_INITIALIZER};

struct {//Extensions supported
	char *ext;
	char *filetype;
	} extensions [] = {
	{"gif", "image/gif" },
	{"jpg", "image/jpeg"},
	{"jpeg","image/jpeg"},
	{"png", "image/png" },
	{"zip", "image/zip" },
	{"gz", "image/gz" },
	{"tar", "image/tar" },
	{"htm", "text/html" },
	{"html","text/html" },
	{"ico", "image/ico"},
	{"txt", "text/html"},
	{0,0} };
////////////////////////////////////////////////////////////////////////////////


///////////////////////////////Functions///////////////////////////////////////
///////////////////////////////Log messages////////////////////////////////////
void initialize_pthreads(){
	int i;
	servert = (pthread_t *)malloc(sizeof(pthread_t)*numthreads);
	handler_t.ntfree = numthreads;
	for(i=0; i<numthreads; i++){
		pthread_create(&servert[i], NULL, &web, NULL);
	}
}

void log(int type, char *s1, char *s2, int num){
	int fd ;
	char logbuffer[BUFSIZE*2];

	switch (type) {//Action to perform in base of the error

		case ERROR://Error type ERROR 
			(void)sprintf(logbuffer,"ERROR: %s:%s Errno=%d exiting pid=%d",s1, s2, errno,pthread_self()); 
			break;
		
		case SORRY://Error type SORRY 
			(void)sprintf(logbuffer, "<HTML><BODY><H1>nweb Web Server Sorry: %s %s</H1></BODY></HTML>\r\n", s1, s2);
			(void)write(num,logbuffer,strlen(logbuffer));
			(void)sprintf(logbuffer,"SORRY: %s:%s",s1, s2); 
			break;
		
		case LOG:////Error type LOG 
			(void)sprintf(logbuffer," INFO: %s:%s:%d",s1, s2,num); 
			break;
	}
	
	/* no checks here, nothing can be done a failure anyway */
	if((fd = open("nweb.log", O_CREAT| O_WRONLY | O_APPEND,0644)) >= 0) {
		(void)write(fd,logbuffer,strlen(logbuffer)); 
		(void)write(fd,"\n",1);      
		(void)close(fd);
	}
	/*if(type == ERROR || type == SORRY){//Error or sorry stop the process 
		exit(3);// exit the child and status is returned to the parent
	}*///No needed as no using child process so i don't want to exit
}
///////////////////////////////////////////////////////////////////////////////

/////////////////////////////////web function//////////////////////////////////
/* this is a child web server process, so we can exit on errors */
void *web(){//need to be modified

	for(;;){
		int intfd;
		int inthit;
		int j, file_fd, buflen, len;
		long i, ret;
		char * fstr;
		static char buffer[BUFSIZE+1]; /* static so zero filled */
		memset(buffer, 0, BUFSIZE+1);

		pthread_mutex_lock(&(handler_t.mutexr));
		while((handler_t.rqts == 0) || (handler_t.ntfree >= numthreads)){
			pthread_cond_wait(&(handler_t.condfull), &(handler_t.mutexr));
		}
		intfd = gfd;
		inthit = ghit;
		handler_t.rqts = 0;

		pthread_mutex_unlock(&(handler_t.mutexr));
		pthread_cond_signal(&(handler_t.condfree));

		ret =read(intfd,buffer,BUFSIZE); 	/* read Web request in one go */
	
		if(ret == 0 || ret == -1) {	/* read failure stop child now */
			log(SORRY,"failed to read browser request","",intfd);
			goto stop;
		}

		if(ret > 0 && ret < BUFSIZE){	/* return code is valid chars */
			buffer[ret]=0;		/* terminate the buffer */
		}
	
		else { 
			buffer[0]=0;
		}

		for(i=0;i<ret;i++){	/* remove CF and LF characters */
			if(buffer[i] == '\r' || buffer[i] == '\n'){
				buffer[i]='*';
			}
		}

		log(LOG,"request",buffer,inthit);//log request, not a error so child doesn't die

		if( strncmp(buffer,"GET ",4) && strncmp(buffer,"get ",4) ){
			log(SORRY,"Only simple GET operation supported",buffer,intfd);
			goto stop;
		}

		for(i=4;i<BUFSIZE;i++) { /* null terminate after the second space to ignore extra stuff */
			if(buffer[i] == ' ') { /* string is "GET URL " +lots of other stuff */
				buffer[i] = 0;
				break;
			}
		}

		for(j=0;j<i-1;j++){ 	/* check for illegal parent directory use .. */
			if(buffer[j] == '.' && buffer[j+1] == '.'){
				log(SORRY,"Parent directory (..) path names not supported",buffer,intfd);
				goto stop;
			}
		}

		if( !strncmp(&buffer[0],"GET /\0",6) || !strncmp(&buffer[0],"get /\0",6) ){ /* convert no filename to index file */
			(void)strcpy(buffer,"GET /index.html");
		}

		/* work out the file type and check we support it */
		buflen=strlen(buffer);
		fstr = (char *)0;
		for(i=0;extensions[i].ext != 0;i++) {
			len = strlen(extensions[i].ext);
			if( !strncmp(&buffer[buflen-len], extensions[i].ext, len)) {
				fstr =extensions[i].filetype;
				break;
			}
		}

		if(fstr == 0){ //Sorry and stop the child
			log(SORRY,"file extension type not supported",buffer,intfd);
			goto stop;
		}
	
		if(( file_fd = open(&buffer[5],O_RDONLY)) == -1){ /* open the file for reading */
			log(SORRY, "failed to open file",&buffer[5],intfd);//child stopped
			goto stop;
		}

		log(LOG,"SEND",&buffer[5],inthit);//log the send process

		(void)sprintf(buffer,"HTTP/1.0 200 OK\r\nContent-Type: %s\r\n\r\n", fstr);
		(void)write(intfd,buffer,strlen(buffer));

		/* send file in 8KB block - last block may be smaller */
		while (	(ret = read(file_fd, buffer, BUFSIZE)) > 0 ) {
			(void)write(intfd,buffer,ret);
		}

#ifdef LINUX
		sleep(1);	/* to allow socket to drain */
#endif
		//exit(1);No childs//exit the child and return status 1 to parent
stop:
		close(intfd);

		pthread_mutex_lock(&(handler_t.mutexr));

		handler_t.ntfree += 1;

		pthread_mutex_unlock(&(handler_t.mutexr));
		pthread_cond_signal(&(handler_t.condfree));
	}
}

////////////////////////////////////main///////////////////////////////////////

int main(int argc, char **argv){//main
	int i, port, pid, listenfd, hit;
	size_t length;
	char *flag; //used to check for the numbers of threads parameter to be a number
	//static struct sockaddr_in cli_addr; /* static = initialised to zeros */
	static struct sockaddr_in serv_addr; /* static = initialised to zeros */

	if( argc < 4 || argc > 4 || !strcmp(argv[1], "-?") ) {
		(void)printf("hint: nweb NUM-Threads Port-Number Top-Directory\n\n"
		"\tnweb is a small and very safe mini web server\n"
		"\tnweb only servers out file/web pages with extensions named below\n"
		"\t and only from the named directory or its sub-directories.\n"
		"\tThere is no fancy features = safe and secure.\n\n"
		"\tExample: nweb 8181 /home/nwebdir &\n\n"
		"\tOnly Supports:");
		for(i=0;extensions[i].ext != 0;i++)
			(void)printf(" %s",extensions[i].ext);

			(void)printf("\n\tNot Supported: URLs including \"..\", Java, Javascript, CGI\n"
			"\tNot Supported: directories / /etc /bin /lib /tmp /usr /dev /sbin \n"
			"\tNo warranty given or implied\n\tNigel Griffiths nag@uk.ibm.com\n");
			exit(0);
		}
	if( !strncmp(argv[3],"/" ,2 ) || !strncmp(argv[3],"/etc", 5 ) ||
		!strncmp(argv[3],"/bin",5 ) || !strncmp(argv[3],"/lib", 5 ) ||
		!strncmp(argv[3],"/tmp",5 ) || !strncmp(argv[3],"/usr", 5 ) ||
		!strncmp(argv[3],"/dev",5 ) || !strncmp(argv[3],"/sbin",6) ){
		(void)printf("ERROR: Bad top directory %s, see nweb -?\n",argv[3]);
		exit(3);
	}
	if(chdir(argv[3]) == -1){
		(void)printf("ERROR: Can't Change to directory %s\n",argv[3]);
		exit(4);
	}

	/*check for the number of threads to be a number*/
	numthreads= strtoul(argv[1],&flag,10);
	if(*flag != NULL){
		(void)printf("ERROR: Bad num of threads, must be a number, see nweb -?\n",argv[1]);
		exit(4);//stop the process parent, because bad arg
	}
	/* Become deamon + unstopable and no zombies children (= no wait()) */
	if(fork() != 0){//KILL the father and leave the child like a daemon
		return 0; /* parent returns OK to shell */
	}

	(void)signal(SIGCLD, SIG_IGN); /* ignore child death */ /*when a child die on a exit i 
								/*ignore the returns of the exits*/

	(void)signal(SIGHUP, SIG_IGN); /* ignore terminal hangups */
	
	for(i=0;i<32;i++){
		(void)close(i);		/* close open files */
	}
	
	(void)setpgrp();		/* break away from process group */

	log(LOG,"nweb starting",argv[2],getpid());//log message starting the web
	
	/* setup the network socket */
	if((listenfd = socket(AF_INET, SOCK_STREAM,0)) <0){//creates a new socket of a certain socket type
		log(ERROR, "system call","socket",0);//error creating the socket
	}

	port = atoi(argv[2]);//assign port number
	if(port < 0 || port >60000){//check if the port is a valid number
		log(ERROR,"Invalid port number (try 1->60000)",argv[2],0);//wrong port number
		exit(-1);
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);

	if(bind(listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) <0){//associates a socket with a socket address structure
		log(ERROR,"system call","bind",0);//error on associates a socket with a socket address structure
		exit(-1);
	}

	if( listen(listenfd,numthreads) <0){//bound TCP socket to enter listening state
		log(ERROR,"system call","listen",0);
	}

	/*initialize the threads*/
	initialize_pthreads();
	sleep(1);

	for(hit=1; ;hit++) {//INFINITE LOOP
		int socketfd = -1;
		if((socketfd = accept(listenfd, NULL, NULL)) < 0){ //(struct sockaddr *)&cli_addr, &length)) < 0){/*It accepts a received incoming attempt 
											     /*to create a new TCP connection from the 
											     remote client, and creates a new socket 
											     associated with the socket address pair 
										 	     of this connection*/
			log(ERROR,"system call","accept",0);//error on accept()
			continue;
		}

//		if((pid = fork()) < 0) {//creates a child
//			log(ERROR,"system call","fork",0);//error on creating the child
//		}
//
//		else {
//			if(pid == 0) { 	/* child *///if it's the child
//				(void)close(listenfd);//causes the system to release resources allocated to a socket
//				web(socketfd,hit); /*never returns*///execute nweb but nweb kill the process
//
//			} else { /* parent */ //if it's the parent
//				(void)close(socketfd);//causes the system to release resources allocated to a socket
//			}
//		}
		
		pthread_mutex_lock(&(handler_t.mutexr));

		while(handler_t.ntfree == 0 ){
			pthread_cond_wait(&(handler_t.condfree), &(handler_t.mutexr));
		}
		// Global data interchange
		gfd = socketfd;
		ghit = hit;

		// Threads syncronization status
		handler_t.rqts = 1;
		handler_t.ntfree -= 1;

		pthread_mutex_unlock(&(handler_t.mutexr));
		pthread_cond_signal(&(handler_t.condfull));
		
	}
}//END
