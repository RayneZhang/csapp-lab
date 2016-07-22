/*
 * proxy.c - CS:APP Web proxy
 * 
 * name:Zhang Lei
 * student number:5140219237
 * email:rayne_@sjtu.edu.cn
 *
 * This is a simple proxy that receive requests from client
 * to server, and then send data from server to client.
 * Multithreads are supported to work together.
 * Part I and II are implemented while part III hasn't been implemented yet.
 */ 

#include "csapp.h" 
#define NTHREADS 8
#define SBUFSIZE 512

typedef struct {
	int fd;
	struct sockaddr_in addr;
}Client;

typedef struct {
	Client **buf;
	int n;
	int front;
	int rear;
	sem_t mutex;
	sem_t slots;
	sem_t items;
}sbuf_t;

/*Create an empty, bounded, shared FIFO buffer with n slots*/
void sbuf_init(sbuf_t *sp, int n) {
	sp->buf = Calloc(n, sizeof(int));
	sp->n = n;
	sp->front = sp->rear = 0;
	Sem_init(&sp->mutex, 0, 1);
	Sem_init(&sp->slots, 0, n);
	Sem_init(&sp->items, 0, 0);
}

/*Clean up buffer sp*/
void sbuf_deinit(sbuf_t *sp) {
	Free(sp->buf);
}

/*Insert item onto the rear of shared buffer sp*/
void sbuf_insert(sbuf_t *sp, Client* item) {
	P(&sp->slots);
	P(&sp->mutex);
	sp->buf[(++sp->rear) % (sp->n)] = item;
	V(&sp->mutex);
	V(&sp->items);
}

/*Remove and return the first item from buffer sp*/
Client* sbuf_remove(sbuf_t *sp) {
	Client* item;
	P(&sp->items);
	P(&sp->mutex);
	item = sp->buf[(++sp->front) % (sp->n)];
	V(&sp->mutex);
	V(&sp->slots);
	return item;
}

/*
* Function prototypes
* Detailed descriptions are above their definition
*/
int parse_uri(char *uri, char *target_addr, char *path, int  *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);
void *thread(void* vargp);
void doit(int fd, struct sockaddr_in addr);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
int Open_clientfd_ts(char* servername, int port);
ssize_t Rio_readn_w(rio_t *rp, void *usrbuf, size_t n);
ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen);
ssize_t Rio_writen_w(int fd, void *usrbuf, size_t n);


sem_t log_mutex;
sem_t openclientfd_mutex;
sbuf_t sbuf;

/*
* main - Main routine for the proxy program
*/
int main(int argc, char **argv)
{
	int i, listenfd, connfd, port;
	socklen_t clientlen = sizeof(struct sockaddr_in);
	struct sockaddr_in clientaddr;
	pthread_t tid;

	Signal(SIGPIPE, SIG_IGN);
	/* Check arguments */
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
		exit(0);
	}

	port = atoi(argv[1]);
	sbuf_init(&sbuf, SBUFSIZE);
	listenfd = Open_listenfd(port);

	Sem_init(&log_mutex, 0, 1);
	Sem_init(&openclientfd_mutex, 0, 1);
	for (i = 0; i<NTHREADS; i++){
		Pthread_create(&tid, NULL, thread, NULL);
	}

	while (1) {
		Client *newclient = (Client *)Malloc(sizeof(Client));
		connfd = Accept(listenfd, (SA *)&newclient->addr, &clientlen);
		newclient->fd = connfd;
		newclient->addr = clientaddr;
		sbuf_insert(&sbuf, newclient);
	}
	exit(0);
}

void *thread(void *vargp) {
	Pthread_detach(Pthread_self());
	while (1) {
		Client* newclient = sbuf_remove(&sbuf);
		//struct sockaddr_in newaddr = newclient->addr;
		doit(newclient->fd, newclient->addr);
	}
	return NULL;
}

void doit(int fd, struct sockaddr_in addr){
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
	rio_t clientRio, serverRio;
	int serverfd, port;
	char client2server[MAXLINE], server2client[MAXLINE];
	char hostname[MAXLINE], pathname[MAXLINE];	

	Rio_readinitb(&clientRio, fd);
	Rio_readlineb_w(&clientRio, buf, MAXLINE);
	sscanf(buf, "%s %s %s", method, uri, version);
	if (strcasecmp(method, "GET")) {
		clienterror(fd, method, "501", "Not Implemented",
			"Tiny does not implement this method");
		return;
	}
	parse_uri(uri, hostname, pathname, &port);

	if ((serverfd = Open_clientfd_ts(hostname, port)) < 0) {
		strcpy(server2client, "Failed to connect server");
		Rio_writen_w(fd, server2client, strlen(server2client));
		return;
	}
	sprintf(client2server, "%s %s %s\r\n", method, pathname, version);
	
	while (strcmp(buf, "\r\n")) {
		Rio_readlineb_w(&clientRio, buf, MAXLINE);
		strcat(client2server, buf);
	}
	Rio_writen_w(serverfd, client2server, sizeof(client2server));
	Rio_readinitb(&serverRio, serverfd);
	//connected...
	int n, size = 0;
	while ((n = Rio_readn_w(&serverRio, buf, MAXLINE)) > 0) {
		if (Rio_writen_w(fd, buf, n) != n)
			break;
		size += n;
	}
	format_log_entry(buf, &addr, uri, size);
	//write log...
	P(&log_mutex);
	FILE* log = fopen("proxy.log", "a");
	fprintf(log, "%s", buf);
	fclose(log);
	V(&log_mutex);
	//disconnect from server
	Close(serverfd);
	Close(fd);
	return;
}

//on book P641, to send an error message to client
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
	char buf[MAXLINE], body[MAXBUF];

	sprintf(body, "<html><title>Tiny Error</title>");
	sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
	sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
	sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
	sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

	sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
	Rio_writen_w(fd, buf, strlen(buf));
	sprintf(buf, "Content-type: text/html\r\n");
	Rio_writen_w(fd, buf, strlen(buf));
	sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
	Rio_writen_w(fd, buf, strlen(buf));
	Rio_writen_w(fd, body, strlen(body));
}

//thread-safe version of open_clientfd
int Open_clientfd_ts(char* servername, int port) {
	P(&openclientfd_mutex);
	int clientfd;
	while ((clientfd = open_clientfd(servername, port)) < 0)
		printf("Open_clientfd error...\n");
	V(&openclientfd_mutex);
	return clientfd;
}

//Theses are wrappers for rio_functions..
ssize_t Rio_readn_w(rio_t *rp, void *usrbuf, size_t n) {
	ssize_t rc;
	if ((rc = rio_readnb(rp, usrbuf, n)) < 0) {
		printf("Rio_readnb error...\n");
		return -1;
	}
	return rc;
}

ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen) {
	ssize_t rc;
	if ((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0) {
		printf("Rio_readlineb error...\n");
		return -1;
	}
	return rc;
}

ssize_t Rio_writen_w(int fd, void *usrbuf, size_t n) {
	ssize_t rc;
	if ((rc = rio_writen(fd, usrbuf, n)) != n) {
		printf("Rio_writen error...\n");
		return -1;
	}
	return rc;
}

/*
* parse_uri - URI parser
*
* Given a URI from an HTTP proxy GET request (i.e., a URL), extract
* the host name, path name, and port.  The memory for hostname and
* pathname must already be allocated and should be at least MAXLINE
* bytes. Return -1 if there are any problems.
*/
int parse_uri(char *uri, char *hostname, char *pathname, int *port)
{
	char *hostbegin;
	char *hostend;
	char *pathbegin;
	int len;

	if (strncasecmp(uri, "http://", 7) != 0) {
		hostname[0] = '\0';
		return -1;
	}

	*port = 80; /* default */

				/* Extract the host name */
	hostbegin = uri + 7;
	hostend = strpbrk(hostbegin, " :/\r\n");
	len = hostend - hostbegin;
	if (strlen(hostbegin) < len) {
		// Failed to find the characters
		strcpy(hostname, hostbegin);
		strcpy(pathname, "/");
		return 0;
	}
	strncpy(hostname, hostbegin, len);
	hostname[len] = '\0';

	/* Extract the port number */
	if (*hostend == ':')
		*port = atoi(hostend + 1);

	/* Extract the path */
	pathbegin = strchr(hostbegin, '/');
	if (pathbegin == NULL)
		strcpy(pathbegin, "/");
	else
		strcpy(pathname, pathbegin);

	return 0;
}

/*
* format_log_entry - Create a formatted log entry in logstring.
*
* The inputs are the socket address of the requesting client
* (sockaddr), the URI from the request (uri), and the size in bytes
* of the response from the server (size).
*/
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr,
	char *uri, int size)
{
	time_t now;
	char time_str[MAXLINE];
	unsigned long host;
	unsigned char a, b, c, d;

	/* Get a formatted time string */
	now = time(NULL);
	strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

	/*
	* Convert the IP address in network byte order to dotted decimal
	* form. Note that we could have used inet_ntoa, but chose not to
	* because inet_ntoa is a Class 3 thread unsafe function that
	* returns a pointer to a static variable (Ch 13, CS:APP).
	*/
	host = ntohl(sockaddr->sin_addr.s_addr);
	a = host >> 24;
	b = (host >> 16) & 0xff;
	c = (host >> 8) & 0xff;
	d = host & 0xff;


	/* Return the formatted log entry string */
	sprintf(logstring, "%s: %d.%d.%d.%d %s %d\n", time_str, a, b, c, d, uri, size);
}