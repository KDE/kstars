/* INDI Server.
 * Copyright (C) 2005 Elwood C. Downey ecdowney@clearskyinstitute.com
 * licensed under GNU Lesser Public License version 2.1 or later.
 *
 * argv lists names of Driver programs to run or sockets to connect for Devices.
 * Drivers are restarted if they exit up to 4 times, sockets are not reopened.
 * Each Driver's stdin/out are assumed to provide INDI traffic and are connected
 *   here via pipes. Drivers' stderr are connected to our stderr.
 * We only support Drivers that advertise support for one Device. The problem
 *   with multiple Devices in one Driver is without a way to know what they
 *   _all_ are there is no way to avoid sending all messages to all Drivers.
 * Clients can come and go as they please and will see messages only for Devices
 *   for which they have queried via getProperties.
 * Messages to Devices on sockets always include Device so chained indiserver
 *   will only pass back info from that Device.
 * all newXXX() received from one Client are echoed to all other Clients who
 *   have shown an interest in the same Device.
 *
 * Implementation notes:
 *
 * Each Client is written to by its own thread to allow for wildly different
 * consumption rates. The main thread cracks the xml. When it sees a complete
 * message it puts it in a new Msg which is the XMLEle and a usage count. The
 * Msg is put on the q for each eligible Client and the Msg usage count is the
 * number of Clients on whose q it resides. The Client write thread waits for
 * a Msg to be on its q, performs the write, decrements the usage count and
 * frees the Msg (and its XMLEle) if the count reaches 0. This mechanism is
 * less valuable for Drivers since there is little need to send the same message
 * to more than one Driver. However, a Driver slow to consume it message can
 * block us so it still might be worth while for that reason someday. 
 * 
 * All manipulation of the Client info table, clinfo[], is guarded by client_m
 * and client_c. All heap access is guarded by malloc_m.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "lilxml.h"
#include "indiapi.h"
#include "fq.h"

#define INDIPORT        7624            /* TCP/IP port on which to listen */
#define	BUFSZ		2048		/* max buffering here */
#define	MAXDRS		4		/* default times to restart a driver */
#define	NOPID		(-1234)		/* invalid PID to flag remote drivers */

/* mutex and condition variables to guard client queue and heap access */
static pthread_mutex_t client_m;	/* client mutex */
static pthread_cond_t client_c;		/* client condition waiting for Msgs */
static pthread_mutex_t malloc_m;	/* heap mutex */

/* name of a device a client is interested in */
typedef char IDev[MAXINDIDEVICE];	/* handy array of char */

/* BLOB handling, NEVER is the default */
typedef enum {B_NEVER=0, B_ALSO, B_ONLY} BLOBEnable;

/* associate a usage count with an XMLEle message */
typedef struct {
    XMLEle *ep;				/* a message */
    int count;				/* number of consumers left */
} Msg;

/* info for each connected client */
typedef struct {
    int active;				/* 1 when this record is in use */
    int shutdown;			/* set to close writer thread */
    int s;				/* socket for this client */
    FILE *wfp;				/* FILE to write to s */
    BLOBEnable blob;			/* when to send setBLOBs */
    pthread_t wtid;			/* writer thread id */
    LilXML *lp;				/* XML parsing context */
    FQ *msgq;				/* Msg queue */
    IDev *devs;				/* malloced array of devices we want */
    int ndevs;				/* n entries in devs[] */
    int sawGetProperties;		/* mark when see getProperties */
} ClInfo;
static ClInfo *clinfo;			/* malloced array of clients */
static int nclinfo;			/* n total (not n active) */

/* info for each connected driver */
typedef struct {
    char *name;				/* malloced process path name */
    IDev dev;				/* device served by this driver */
    int pid;				/* process id or NOPID if remote */
    int rfd;				/* read pipe fd */
    FILE *wfp;				/* write pipe fp */
    int restarts;			/* times process has been restarted */
    LilXML *lp;				/* XML parsing context */
} DvrInfo;
static DvrInfo *dvrinfo;		/* malloced array of drivers */
static int ndvrinfo;			/* n total */

static void usage (void);
static void *mymalloc (size_t s);
static void *myrealloc (void *p, size_t s);
static void myfree (void *p);
static void noZombies (void);
static void noSIGPIPE (void);
static void indiRun (void);
static void indiListen (void);
static void newClient (void);
static int newClSocket (void);
static void shutdownClient (ClInfo *cp);
static void clientMsg (ClInfo *cp);
static void startDvr (DvrInfo *dp);
static void startLocalDvr (DvrInfo *dp);
static void startRemoteDvr (DvrInfo *dp);
static int openINDIServer (char host[], int port);
static void restartDvr (DvrInfo *dp);
static void send2Drivers (XMLEle *root, char *dev);
static void send2Clients (ClInfo *notme, XMLEle *root, char *dev);
static void addClDevice (ClInfo *cp, char *dev);
static int findClDevice (ClInfo *cp, char *dev);
static void driverMsg (DvrInfo *dp);
static void *clientWThread(void *carg);
static void freeMsg (Msg *mp);
static BLOBEnable crackBLOB (char enableBLOB[]);
static void xmlLog (XMLEle *root);

static char *me;			/* our name */
static int port = INDIPORT;		/* public INDI port */
static int verbose;			/* more chatty */
static int maxdrs = MAXDRS;		/* max times to restart dieing driver */
static int lsocket;			/* listen socket */

int
main (int ac, char *av[])
{
	/* save our name */
	me = av[0];

	/* crack args */
	while ((--ac > 0) && ((*++av)[0] == '-')) {
	    char *s;
	    for (s = av[0]+1; *s != '\0'; s++)
		switch (*s) {
		case 'p':
		    if (ac < 2)
			usage();
		    port = atoi(*++av);
		    ac--;
		    break;
		case 'r':
		    if (ac < 2)
			usage();
		    maxdrs = atoi(*++av);
		    ac--;
		    break;
		case 'v':
		    verbose++;
		    break;
		default:
		    usage();
		}
	}

	/* at this point there are ac args in av[] to name our drivers */
	if (ac == 0)
	    usage();

	/* take care of some unixisms */
	noZombies();
	noSIGPIPE();

	/* init mutexes and condition variables */
	pthread_mutex_init(&client_m, NULL);
	pthread_cond_init (&client_c, NULL);
	pthread_mutex_init(&malloc_m, NULL);

	/* install our locked heap functions */
	xmlMalloc (mymalloc, myrealloc, myfree);
	setMemFuncsFQ (mymalloc, myrealloc, myfree);

	/* seed client info array so we can always use realloc */
	clinfo = (ClInfo *) mymalloc (1);
	nclinfo = 0;

	/* create driver info array all at once so size never has to change */
	ndvrinfo = ac;
	dvrinfo = (DvrInfo *) mymalloc (ndvrinfo * sizeof(DvrInfo));
	memset (dvrinfo, 0, ndvrinfo * sizeof(DvrInfo));

	/* start each driver, malloc name once and keep it */
	while (ac-- > 0) {
	    dvrinfo[ac].name = strcpy (mymalloc(strlen(*av)+1), *av);
	    startDvr (&dvrinfo[ac]);
	    av++;
	}

	/* announce we are online */
	indiListen();

	/* handle new clients and all reading */
	while (1)
	    indiRun();

	/* whoa! */
	fprintf (stderr, "%s: unexpected return from main\n", me);
	return (1);
}

/* print usage message and exit (1) */
static void
usage(void)
{
	fprintf (stderr, "Usage: %s [options] driver [driver ...]\n", me);
	fprintf (stderr, "%s\n", "$Revision$");
	fprintf (stderr, "Purpose: INDI Server\n");
	fprintf (stderr, "Options:\n");
	fprintf (stderr, " -p p  : alternate IP port, default %d\n", INDIPORT);
	fprintf (stderr, " -r n  : max driver restarts, default %d\n", MAXDRS);
	fprintf (stderr, " -v    : show connects/disconnects, no traffic\n");
	fprintf (stderr, " -vv   : show -v + key message content\n");
	fprintf (stderr, " -vvv  : show -vv + complete xml\n");
	fprintf (stderr, "driver : program or device@host[:port]\n");

	exit (1);
}

/* like malloc(3) but honors malloc_m mutex lock */
static void *
mymalloc (size_t s)
{
	void *mem;

	pthread_mutex_lock (&malloc_m);
	mem = malloc (s);
	pthread_mutex_unlock (&malloc_m);
	return (mem);
}

/* like realloc(3) but honors malloc_m mutex lock */
static void *
myrealloc (void *p, size_t s)
{
	void *mem;

	pthread_mutex_lock (&malloc_m);
	mem = realloc (p, s);
	pthread_mutex_unlock (&malloc_m);
	return (mem);
}

/* like free(3) but honors malloc_m mutex lock */
static void
myfree (void *p)
{
	pthread_mutex_lock (&malloc_m);
	free (p);
	pthread_mutex_unlock (&malloc_m);
}

/* arrange for no zombies if drivers die */
static void
noZombies()
{
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
#ifdef SA_NOCLDWAIT
	sa.sa_flags = SA_NOCLDWAIT;
#else
	sa.sa_flags = 0;
#endif
	(void)sigaction(SIGCHLD, &sa, NULL);
}

/* turn off SIGPIPE on bad write so we can handle it inline */
static void
noSIGPIPE()
{
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	(void)sigaction(SIGPIPE, &sa, NULL);
}

/* start the INDI driver process or connection usingthe given DvrInfo slot.
 * exit if trouble.
 */
static void
startDvr (DvrInfo *dp)
{
	if (strchr (dp->name, '@'))
	    startRemoteDvr (dp);
	else
	    startLocalDvr (dp);
}

/* start the INDI driver process using the given DvrInfo slot.
 * exit if trouble.
 */
static void
startLocalDvr (DvrInfo *dp)
{
	int rp[2], wp[2];
	int pid;

	/* build two pipes for r and w */
	if (pipe (rp) < 0) {
	    fprintf (stderr, "%s: read pipe: %s\n", me, strerror(errno));
	    exit(1);
	}
	if (pipe (wp) < 0) {
	    fprintf (stderr, "%s: write pipe: %s\n", me, strerror(errno));
	    exit(1);
	}

	/* fork&exec new process */
	pid = fork();
	if (pid < 0) {
	    fprintf (stderr, "%s: fork: %s\n", me, strerror(errno));
	    exit(1);
	}
	if (pid == 0) {
	    /* child: exec name */
	    int fd;

	    /* rig up pipes as stdin/out; stderr stays, everything else goes */
	    dup2 (wp[0], 0);
	    dup2 (rp[1], 1);
	    for (fd = 3; fd < 100; fd++)
		(void) close (fd);

	    /* go -- should never return */
	    execlp (dp->name, dp->name, NULL);
	    fprintf (stderr, "Driver %s: %s\n", dp->name, strerror(errno));
	    _exit (1);	/* parent will notice EOF shortly */
	}

	/* don't need child's side of pipes */
	close (rp[1]);
	close (wp[0]);

	/* record pid, io channel, init lp */
	dp->pid = pid;
	dp->rfd = rp[0];
	dp->lp = newLilXML();

	/* N.B. beware implied use of malloc */
	pthread_mutex_lock (&malloc_m);
	dp->wfp = fdopen (wp[1], "a");
	setbuf (dp->wfp, NULL);
	pthread_mutex_unlock (&malloc_m);

	if (verbose > 0)
	    fprintf (stderr, "Driver %s: rfd=%d wfd=%d\n", dp->name, dp->rfd,
	    								wp[1]);
}

/* start the remote INDI driver connection using the given DvrInfo slot.
 * exit if trouble.
 */
static void
startRemoteDvr (DvrInfo *dp)
{
	char dev[1024];
	char host[1024];
	int indiport, sockfd;

	/* extract host and indiport */
	indiport = INDIPORT;
	if (sscanf (dp->name, "%[^@]@%[^:]:%d", dev, host, &indiport) < 2) {
	    fprintf (stderr, "Bad remote device syntax: %s\n", dp->name);
	    exit(1);
	}

	/* connect */
	sockfd = openINDIServer (host, indiport);

	/* record fake pid, io channel, init lp, dev name.
	 * N.B. storing name now is key to limiting remote traffic to this dev
	 */
	dp->pid = NOPID;
	dp->rfd = sockfd;
	dp->lp = newLilXML();
	strncpy (dp->dev, dev, sizeof(IDev)-1);
	dp->dev[sizeof(IDev)-1] = '\0';

	/* N.B. beware implied use of malloc */
	pthread_mutex_lock (&malloc_m);
	dp->wfp = fdopen (sockfd, "a");
	setbuf (dp->wfp, NULL);
	pthread_mutex_unlock (&malloc_m);

	if (verbose > 0)
	    fprintf (stderr, "Driver %s: socket=%d\n", dp->name, sockfd);
}

/* open a connection to the given host and port or die.
 * return socket fd.
 */
static int
openINDIServer (char host[], int indiport)
{
	struct sockaddr_in serv_addr;
	struct hostent *hp;
	int sockfd;

	/* lookup host address */
	hp = gethostbyname (host);
	if (!hp) {
	    fprintf (stderr, "gethostbyname(%s): %s\n", host, strerror(errno));
	    exit (1);
	}

	/* create a socket to the INDI server */
	(void) memset ((char *)&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr =
			    ((struct in_addr *)(hp->h_addr_list[0]))->s_addr;
	serv_addr.sin_port = htons(indiport);
	if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
	    fprintf (stderr, "socket(%s,%d): %s\n", host, indiport,strerror(errno));
	    exit(1);
	}

	/* connect */
	if (connect (sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr))<0){
	    fprintf (stderr, "connect(%s,%d): %s\n", host,port,strerror(errno));
	    exit(1);
	}

	/* ok */
	return (sockfd);
}

/* create the public INDI Driver endpoint lsocket on port.
 * return server socket else exit.
 */
static void
indiListen ()
{
	struct sockaddr_in serv_socket;
	int sfd;
	int reuse = 1;

	/* make socket endpoint */
	if ((sfd = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
	    fprintf (stderr, "%s: socket: %s", me, strerror(errno));
	    exit(1);
	}
	
	/* bind to given port for any IP address locally */
	memset (&serv_socket, 0, sizeof(serv_socket));
	serv_socket.sin_family = AF_INET;
	serv_socket.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
	serv_socket.sin_port = htons ((unsigned short)port);
	if (setsockopt(sfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse)) < 0){
	    fprintf (stderr, "%s: setsockopt: %s", me, strerror(errno));
	    exit(1);
	}
	if (bind(sfd,(struct sockaddr*)&serv_socket,sizeof(serv_socket)) < 0){
	    fprintf (stderr, "%s: bind: %s", me, strerror(errno));
	    exit(1);
	}

	/* willing to accept connections with a backlog of 5 pending */
	if (listen (sfd, 5) < 0) {
	    fprintf (stderr, "%s: listen: %s", me, strerror(errno));
	    exit(1);
	}

	/* ok */
	lsocket = sfd;
	if (verbose > 0)
	    fprintf (stderr, "%s: listening to port %d on fd %d\n",me,port,sfd);
}

/* service traffic from clients and drivers */
static void
indiRun(void)
{
	fd_set rs;
	int maxfd;
	int i, s;

	/* start with public contact point */
	FD_ZERO(&rs);
	FD_SET(lsocket, &rs);
	maxfd = lsocket;

	/* add all client and driver read fd's */
	for (i = 0; i < nclinfo; i++) {
	    ClInfo *cp = &clinfo[i];
	    if (cp->active) {
		FD_SET(cp->s, &rs);
		if (cp->s > maxfd)
		    maxfd = cp->s;
	    }
	}
	for (i = 0; i < ndvrinfo; i++) {
	    DvrInfo *dp = &dvrinfo[i];
	    FD_SET(dp->rfd, &rs);
	    if (dp->rfd > maxfd)
		maxfd = dp->rfd;
	}

	/* wait for action */
	s = select (maxfd+1, &rs, NULL, NULL, NULL);
	if (s < 0) {
	    fprintf (stderr, "%s: select(%d): %s\n",me,maxfd+1,strerror(errno));
	    exit(1);
	}

	/* new client? */
	if (s > 0 && FD_ISSET(lsocket, &rs)) {
	    newClient();
	    s -= 1;
	}

	/* message from client? */
	for (i = 0; s > 0 && i < nclinfo; i++) {
	    if (clinfo[i].active && FD_ISSET(clinfo[i].s, &rs)) {
		clientMsg(&clinfo[i]);
		s -= 1;
	    }
	}

	/* message from driver? */
	for (i = 0; s > 0 && i < ndvrinfo; i++) {
	    if (FD_ISSET(dvrinfo[i].rfd, &rs)) {
		driverMsg(&dvrinfo[i]);
		s -= 1;
	    }
	}
}

/* prepare for new client arriving on lsocket.
 * exit if trouble.
 */
static void
newClient()
{
	ClInfo *cp = NULL;
	int s, cli;

	/* assign new socket */
	s = newClSocket ();

	/* try to reuse a clinfo slot, else add one */
	for (cli = 0; cli < nclinfo; cli++)
	    if (!(cp = &clinfo[cli])->active)
		break;
	if (cli == nclinfo) {
	    /* grow clinfo, lock while moving */
	    pthread_mutex_lock (&client_m);
	    clinfo = (ClInfo *) myrealloc (clinfo, (nclinfo+1)*sizeof(ClInfo));
	    if (!clinfo) {
		fprintf (stderr, "%s: no memory for new client\n", me);
		exit(1);
	    }
	    cp = &clinfo[nclinfo++];
	    pthread_mutex_unlock (&client_m);
	}

	/* rig up new clinfo entry */
	memset (cp, 0, sizeof(*cp));
	cp->active = 1;
	cp->s = s;
	cp->lp = newLilXML();
	cp->msgq = newFQ(1);
	cp->devs = mymalloc (1);

	/* N.B. beware implied use of malloc */
	pthread_mutex_lock (&malloc_m);
	cp->wfp = fdopen (cp->s, "a");
	setbuf (cp->wfp, NULL);
	pthread_mutex_unlock (&malloc_m);

	if (verbose > 0)
	    fprintf (stderr, "Client %d: new arrival - welcome!\n", cp->s);

	/* start the writer thread */
	s = pthread_create (&cp->wtid, NULL, clientWThread, (void*)(cp-clinfo));
	if (s) {
	    fprintf (stderr, "Thread create error: %s\n", strerror(s));
	    exit (1);
	}
}

/* read more from the given client, send to each appropriate driver when see
 * xml closure. also send all newXXX() to all other interested clients.
 * shut down client if any trouble.
 */
static void
clientMsg (ClInfo *cp)
{
	char buf[BUFSZ];
	int i, nr;

	/* read client */
	nr = read (cp->s, buf, sizeof(buf));
	if (nr < 0) {
	    fprintf (stderr, "Client %d: %s\n", cp->s, strerror(errno));
	    shutdownClient (cp);
	    return;
	}
	if (nr == 0) {
	    if (verbose)
		fprintf (stderr, "Client %d: read EOF\n", cp->s);
	    shutdownClient (cp);
	    return;
	} 
	if (verbose > 2)
	    fprintf (stderr, "Client %d: read %d:\n%.*s", cp->s, nr, nr, buf);

	/* process XML, sending when find closure */
	for (i = 0; i < nr; i++) {
	    char err[1024];
	    XMLEle *root = readXMLEle (cp->lp, buf[i], err);
	    if (root) {
		char *roottag = tagXMLEle(root);
		char *dev = findXMLAttValu (root, "device");

		if (verbose > 1)
		    fprintf (stderr, "Client %d: read ", cp->s), xmlLog(root);

		/* snag enableBLOB */
		if (!strcmp (roottag, "enableBLOB"))
		    cp->blob = crackBLOB (pcdataXMLEle(root));

		/* snag interested devices */
		if (!strcmp (roottag, "getProperties"))
		    addClDevice (cp, dev);

		/* send message to driver(s) responsible for dev */
		send2Drivers (root, dev);

		/* echo new* commands back to other clients, else done */
		if (!strncmp (roottag, "new", 3))
		    send2Clients (cp, root, dev); /* does delXMLEle */
		else
		    delXMLEle (root);
	    } else if (err[0])
		fprintf (stderr, "Client %d: %s\n", cp->s, err);
	}
}

/* read more from the given driver, send to each interested client when see
 * xml closure. if driver dies, try to restarting up to MAXDRS times.
 */
static void
driverMsg (DvrInfo *dp)
{
	char buf[BUFSZ];
	int i, nr;

	/* read driver */
	nr = read (dp->rfd, buf, sizeof(buf));
	if (nr < 0) {
	    fprintf (stderr, "Driver %s: %s\n", dp->name, strerror(errno));
	    restartDvr (dp);
	    return;
	}
	if (nr == 0) {
	    fprintf (stderr, "Driver %s: died, or failed to start\n", dp->name);
	    restartDvr (dp);
	    return;
	}
	if (verbose > 2)
	    fprintf (stderr,"Driver %s: read %d:\n%.*s", dp->name, nr, nr, buf);

	/* process XML, sending when find closure */
	for (i = 0; i < nr; i++) {
	    char err[1024];
	    XMLEle *root = readXMLEle (dp->lp, buf[i], err);
	    if (root) {
		char *dev = findXMLAttValu (root, "device");

		if (verbose > 1)
		   fprintf(stderr,"Driver %s: read ", dp->name), xmlLog(root);

		/* snag device name if not known yet */
		if (!dp->dev[0] && dev[0]) {
		    strncpy (dp->dev, dev, sizeof(IDev)-1);
		    dp->dev[sizeof(IDev)-1] = '\0';
		}

		/* send to interested clients */
		send2Clients (NULL, root, dev);
	    } else if (err[0])
		fprintf (stderr, "Driver %s: %s\n", dp->name, err);
	}
}

/* close down the given client */
static void
shutdownClient (ClInfo *cp)
{
	/* inform writer thread to exit then wait for it */
	cp->shutdown = 1;
	pthread_cond_broadcast (&client_c);
	pthread_join (cp->wtid, NULL);

	/* recycle this clinfo cell */
	cp->active = 0;

	if (verbose > 0)
	    fprintf (stderr, "Client %d: closed\n", cp->s);
}

/* close down the given driver and restart if not too many already */
static void
restartDvr (DvrInfo *dp)
{
	/* make sure it's dead, reclaim resources */
	if (dp->pid != NOPID)
	    kill (dp->pid, SIGKILL);
	fclose (dp->wfp);
	close (dp->rfd);
	delLilXML (dp->lp);

	/* restart unless too many already */
	if (++dp->restarts > maxdrs) {
	    fprintf (stderr, "Driver %s: still dead after %d restarts\n",
							    dp->name, maxdrs);
	    exit(1);
	}
	fprintf (stderr, "Driver %s: restart #%d\n", dp->name, dp->restarts);
	startDvr (dp);
}

/* send the xml command to each driver supporting device dev, or all if unknown.
 * restart if write fails.
 */
static void
send2Drivers (XMLEle *root, char *dev)
{
	int i;

	for (i = 0; i < ndvrinfo; i++) {
	    DvrInfo *dp = &dvrinfo[i];
	    int adddev = 0;

	    if (dev[0]) {
		/* skip unless we know driver supports this device */
		if (dp->dev[0] && strcmp (dev, dp->dev))
		    continue;
	    } else {
		/* add dev to message if known from driver.
		 * N.B. this is key to limiting remote traffic to this dev
		 */
		if (dp->dev[0]) {
		    addXMLAtt (root, "device", dp->dev);
		    adddev = 1;
		}
	    }

	    prXMLEle (dp->wfp, root, 0);

	    if (ferror(dp->wfp)) {
		fprintf (stderr, "Driver %s: %s\n", dp->name, strerror(errno));
		restartDvr (dp);
	    } else if (verbose > 2) {
		fprintf (stderr, "Driver %s: send:\n", dp->name);
		prXMLEle (stderr, root, 0);
	    } else if (verbose > 1)
		fprintf(stderr,"Driver %s: send ", dp->name), xmlLog(root);

	    if (adddev)
		rmXMLAtt (root, "device");
	}
}

/* queue the xml command in root from the given device to each
 * interested client, except notme
 */
static void
send2Clients (ClInfo *notme, XMLEle *root, char *dev)
{
	ClInfo *cp;
	Msg *mp;

	/* build a new message */
	mp = (Msg *) mymalloc (sizeof(Msg));
	mp->ep = root;
	mp->count = 0;

	/* lock access to client info */
	pthread_mutex_lock (&client_m);

	/* queue message to each interested client */
	for (cp = clinfo; cp < &clinfo[nclinfo]; cp++) {
	    int isblob;

	    /* cp ok? notme? valid dev? blob? */
	    if (!cp->active || cp == notme)
		continue;
	    if (findClDevice (cp, dev) < 0)
		continue;
	    isblob = !strcmp (tagXMLEle(root), "setBLOBVector");
	    if ((isblob && cp->blob==B_NEVER) || (!isblob && cp->blob==B_ONLY))
		continue;

	    /* ok: queue message to given client */
	    mp->count++;
	    pushFQ (cp->msgq, mp);
	}

	/* wake up client write threads, the last of which will free the Msg */
	if (mp->count > 0)
	    pthread_cond_broadcast(&client_c);
	else {
	    if (verbose > 2)
		fprintf (stderr, "no clients want "), xmlLog(root);
	    freeMsg (mp);	/* no interested clients, free Msg now */
	}

	/* finished with client info */
	pthread_mutex_unlock (&client_m);
}

/* free Msg mp and everything it contains */
static void
freeMsg (Msg *mp)
{
	delXMLEle (mp->ep);
	myfree (mp);
}

/* this function is the thread to perform all writes to client carg.
 * return with client closed when we have problems or when shutdown flag is set.
 * N.B. coordinate all access to clinfo via client_m/c.
 * N.B. clinfo can move (be realloced) when unlocked so beware pointers thereto.
 */
static void *
clientWThread(void *carg)
{
	int c = (int)carg;
	ClInfo *cp;
	Msg *mp;

	/* start off wanting exclusive access to client info */
	pthread_mutex_lock (&client_m);

	/* loop until told to shut down or get write error */
	while (1) {

	    /* check for message or shutdown, unlock while waiting */
	    while (nFQ(clinfo[c].msgq) == 0 && !clinfo[c].shutdown) {
		if (verbose > 2)
		    fprintf (stderr,"Client %d: thread sleeping\n",clinfo[c].s);
		pthread_cond_wait (&client_c, &client_m);
		if (verbose > 2)
		    fprintf (stderr, "Client %d: thread awake\n", clinfo[c].s);
	    }
	    if (clinfo[c].shutdown)
		break;

	    /* get next message for this client */
	    mp = popFQ (clinfo[c].msgq);

	    /* unlock client info while writing */
	    pthread_mutex_unlock (&client_m);
	    prXMLEle (clinfo[c].wfp, mp->ep, 0);
	    pthread_mutex_lock (&client_m);

	    /* trace */
	    cp = &clinfo[c];		/* ok to use pointer while locked */
	    if (verbose > 2) {
		fprintf (stderr, "Client %d: send:\n", cp->s);
		prXMLEle (stderr, mp->ep, 0);
	    } else if (verbose > 1)
		fprintf (stderr, "Client %d: send ", cp->s), xmlLog(mp->ep);

	    /* update message usage count, free if goes to 0 */
	    if (--mp->count == 0)
		freeMsg (mp);

	    /* exit this thread if encountered write errors */
	    if (ferror(cp->wfp)) {
		fprintf (stderr, "Client %d: %s\n", cp->s, strerror(errno));
		break;
	    }
	}

	/* close down this client */
	cp = &clinfo[c];		/* ok to use pointer while locked */
	fclose (cp->wfp);		/* also closes cp->s */
	delLilXML (cp->lp);
	myfree (cp->devs);

	/* decrement and possibly free any unsent messages for this client */
	while ((mp = (Msg*) popFQ(cp->msgq)) != NULL)
	    if (--mp->count == 0)
		freeMsg (mp);
	delFQ (cp->msgq);

	/* this thread is now finished with client info */
	pthread_mutex_unlock (&client_m);

	/* exit thread */
	return (0);
}

/* return 0 if we have seen getProperties from this client and dev is in its
 * devs[] list or the list is empty or no dev specified, else return -1
 */
static int
findClDevice (ClInfo *cp, char *dev)
{
	int i;

	if (!cp->sawGetProperties)
	    return (-1);
	if (cp->ndevs == 0 || !dev[0])
	    return (0);
	for (i = 0; i < cp->ndevs; i++)
	    if (!strncmp (dev, cp->devs[i], sizeof(IDev)-1))
		return (0);
	return (-1);
}

/* add the given device to the devs[] list of client cp unless empty.
 * regardless, record having seen getProperties from this client.
 */
static void
addClDevice (ClInfo *cp, char *dev)
{

	if (dev[0]) {
	    char *ip;
	    cp->devs = (IDev *) myrealloc (cp->devs,(cp->ndevs+1)*sizeof(IDev));
	    ip = (char*)&cp->devs[cp->ndevs++];
	    strncpy (ip, dev, sizeof(IDev)-1);
	    ip[sizeof(IDev)-1] = '\0';
	}

	cp->sawGetProperties = 1;
}


/* block to accept a new client arriving on lsocket.
 * return private nonblocking socket or exit.
 */
static int
newClSocket ()
{
	struct sockaddr_in cli_socket;
	socklen_t cli_len;
	int cli_fd;

	/* get a private connection to new client */
	cli_len = sizeof(cli_socket);
	cli_fd = accept (lsocket, (struct sockaddr *)&cli_socket, &cli_len);
	if(cli_fd < 0) {
	    fprintf (stderr, "%s: accept: %s", me, strerror(errno));
	    exit (1);
	}

	/* ok */
	return (cli_fd);
}

/* convert the string value of enableBLOB to our state value */
static BLOBEnable
crackBLOB (char enableBLOB[])
{
	if (!strcmp (enableBLOB, "Also"))
	    return (B_ALSO);
	if (!strcmp (enableBLOB, "Only"))
	    return (B_ONLY);
	return (B_NEVER);
}

/* print tag, device, name attributes and
 * values of the given xml
 */
static void
xmlLog (XMLEle *root)
{
            const char *prtags[] = {
	    "defNumber", "oneNumber",
	    "defText",   "oneText",
	    "defSwitch", "oneSwitch",
	    "defLight",  "oneLight",
	};
	XMLEle *e;
	unsigned int i;

	/* print tag header */
	fprintf (stderr, "%s %s %s", tagXMLEle(root),
						findXMLAttValu(root,"device"),
						findXMLAttValu(root,"name"));

	/* print each array value */
	for (e = nextXMLEle(root,1); e; e = nextXMLEle(root,0))
	    for (i = 0; i < sizeof(prtags)/sizeof(prtags[0]); i++)
		if (strcmp (prtags[i], tagXMLEle(e)) == 0)
		    fprintf (stderr, " %s=%s", findXMLAttValu(e,"name"),
							    pcdataXMLEle (e));

	fprintf (stderr, "\n");
}

