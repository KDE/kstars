/* INDI Server.
 * Copyright (C) 2006 Elwood C. Downey ecdowney@clearskyinstitute.com
 * licensed under GNU Lesser Public License version 2.1 or later.
 *
 * argv lists names of Driver programs to run or sockets to connect for Devices.
 * Drivers are restarted if they exit up to 4 times, sockets are not reopened.
 * Each Driver's stdin/out are assumed to provide INDI traffic and are connected
 *   here via pipes. Drivers' stderr are connected to our stderr.
 * We only support Drivers that advertise support for one Device. The problem
 *   with multiple Devices in one Driver is without a way to know what they
 *   _all_ are there is no way to avoid sending all messages to all Drivers.
 *   For efficiency, we want Client traffic to be restricted to only those
 *   Devices for which they have queried via getProperties.
 *   Similary, messages to Devices on sockets always include Device so
 *   chained indiserver will only pass back info from that Device.
 * all newXXX() received from one Client are echoed to all other Clients who
 *   have shown an interest in the same Device.
 *
 * Implementation notes:
 *
 * We fork each driver and open a server socket listening for INDI clients.
 * Then forever we listen for new clients and pass traffic between clients and
 * drivers, subject to optimizations based on sniffing getProperties messages.
 * Whereas it is often the case that a message from a driver will be sent to
 * more than one client, to avoid starving fast clients able to read quickly
 * while waiting for messages to drain to slow clients, each client message
 * is put on a queue for each interested client with a count of consumers.
 * Similarly, even though it is more rare for a message to be sent to more
 * than one driver, we still want to avoid blocking on a driver slow to
 * consume its message so driver messages are also queued in like fashion to
 * clients. Note that the same messages can be destined for both clients and
 * devices (for example, new* messages from clients are sent to drivers and
 * echoed to clients) it is allowed for one message to be on both driver and
 * client queues. Once queued, select(2) is used to write only to clients and
 * drivers that are known ready to read or accept more traffic. A message is
 * freed once it is no longer in use by clients or drivers.
 *
 * TODO:
 * Writing large BLOBs is sufficiently likely to dominate we should fork a new
 *   process each time we write one to a client.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "lilxml.h"
#include "indiapi.h"
#include "fq.h"

/* Observers */
#include "observer.h"

#define INDIPORT        7624            /* TCP/IP port on which to listen */
#define	BUFSZ		2048		/* max buffering here */
#define	MAXDRS		4		/* default times to restart a driver */
#define	NOPID		(-1234)		/* invalid PID to flag remote drivers */

/* name of a device a client is interested in */
typedef char IDev[MAXINDIDEVICE];	/* handy array of char */

/* BLOB handling, NEVER is the default */
typedef enum {B_NEVER=0, B_ALSO, B_ONLY} BLOBHandling;

/* associate a usage count with an XMLEle message */
typedef struct {
    XMLEle *ep;				/* a message */
    int count;				/* number of consumers left */
} Msg;

/* info for each connected client */
typedef struct {
    int active;				/* 1 when this record is in use */
    int s;				/* socket for this client */
    FILE *wfp;				/* FILE to write to s */
    BLOBHandling blob;			/* when to send setBLOBs */
    LilXML *lp;				/* XML parsing context */
    FQ *msgq;				/* Msg queue */
    IDev *devs;				/* malloced array of devices we want */
    int ndevs;				/* n entries in devs[] */
    int sawGetProperties;		/* mark when see getProperties */
} ClInfo;
static ClInfo *clinfo;			/*  malloced array of clients */
static int nclinfo;			/* n total (not active) */

/* info for each connected driver */
typedef struct {
    char *name;				/* malloced process path name */
    IDev dev;				/* device served by this driver */
    int pid;				/* process id or NOPID if remote */
    int rfd;				/* read pipe fd */
    int wfd;				/* write pipe fd */
    FILE *wfp;				/* write pipe fp */
    int restarts;			/* times process has been restarted */
    LilXML *lp;				/* XML parsing context */
    FQ *msgq;				/* Msg queue */
} DvrInfo;
static DvrInfo *dvrinfo;		/* malloced array of drivers */
static int ndvrinfo;			/* n total */

/*******************************************************
 JM: Observer Extension Start
 ******************************************************/
typedef struct 
{
    int in_use;
    char dev[MAXINDIDEVICE];
    char name[MAXINDINAME];
    IDType type;
    IPState last_state;
    DvrInfo *dp;
} ObserverInfo;

static  ObserverInfo* observerinfo;		/* malloced array of drivers */
static int nobserverinfo;					/* n total */
static int nobserverinfo_active;			/* n total, active */

static Msg *q2Observers (DvrInfo *dp, XMLEle *root, char *dev, Msg *mp);
static void manageObservers(DvrInfo *dp, XMLEle *root);
static void alertObservers(DvrInfo *dp);

/******************************************************/
/***************** Observer End ***********************/
/******************************************************/

static void usage (void);
static void noZombies (void);
static void noSIGPIPE (void);
static void indiRun (void);
static void indiListen (void);
static void newClient (void);
static int newClSocket (void);
static void shutdownClient (ClInfo *cp);
static void readFromClient (ClInfo *cp);
static void startDvr (DvrInfo *dp);
static void startLocalDvr (DvrInfo *dp);
static void startRemoteDvr (DvrInfo *dp);
static int openINDIServer (char host[], int indi_port);
static void restartDvr (DvrInfo *dp);
static Msg *q2Drivers (XMLEle *root, char *dev, Msg *mp);
static Msg * q2Clients (ClInfo *notme, XMLEle *root, char *dev, Msg *mp);
static void addClDevice (ClInfo *cp, char *dev);
static int findClDevice (ClInfo *cp, char *dev);
static void readFromDriver (DvrInfo *dp);
static void freeMsg (Msg *mp);
static void sendClientMsg (ClInfo *cp);
static void sendDriverMsg (DvrInfo *cp);
static BLOBHandling crackBLOB (char enableBLOB[]);
static void logMsg (XMLEle *root);

static char *me;			/* our name */
static int port = INDIPORT;		/* public INDI port */
static int maxdrs = MAXDRS;		/* max times to restart dieing driver */
static int verbose;			/* chattiness */
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

	/* seed for realloc */
	clinfo = (ClInfo *) malloc (1);
	nclinfo = 0;

	/* create driver info array all at once so size never has to change */
	ndvrinfo = ac;
	dvrinfo = (DvrInfo *) malloc (ndvrinfo * sizeof(DvrInfo));
	memset (dvrinfo, 0, ndvrinfo * sizeof(DvrInfo));

	/* start each driver, malloc name once and keep it */
	while (ac-- > 0) {
	    dvrinfo[ac].name = strcpy (malloc(strlen(*av)+1), *av);
	    startDvr (&dvrinfo[ac]);
	    av++;
	}

	/* announce we are online */
	indiListen();

	/* handle new clients and all io */
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
	fprintf (stderr, " -vv   : -v + key message content\n");
	fprintf (stderr, " -vvv  : -vv + complete xml\n");
	fprintf (stderr, "driver : program or device@host[:port]\n");

	exit (1);
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
	dp->wfd = wp[1];
	dp->wfp = fdopen (dp->wfd, "a");
	dp->lp = newLilXML();
	dp->msgq = newFQ(1);

	if (verbose > 0)
	    fprintf (stderr, "Driver %s: rfd=%d wfd=%d\n", dp->name, dp->rfd,
								    dp->wfd);
}

/* start the remote INDI driver connection using the given DvrInfo slot.
 * exit if trouble.
 */
static void
startRemoteDvr (DvrInfo *dp)
{
	char dev[1024];
	char host[1024];
	int indi_port, sockfd;

	/* extract host and port */
	indi_port = INDIPORT;
	if (sscanf (dp->name, "%[^@]@%[^:]:%d", dev, host, &indi_port) < 2) {
	    fprintf (stderr, "Bad remote device syntax: %s\n", dp->name);
	    exit(1);
	}

	/* connect */
	sockfd = openINDIServer (host, indi_port);

	/* record fake pid, io channel, init lp */
	dp->pid = NOPID;
	dp->rfd = sockfd;
	dp->wfd = sockfd;
	dp->wfp = fdopen (sockfd, "a");
	dp->lp = newLilXML();
	dp->msgq = newFQ(1);
	
	/* N.B. storing name now is key to limiting remote traffic to this dev*/
	strncpy (dp->dev, dev, sizeof(IDev)-1);
	dp->dev[sizeof(IDev)-1] = '\0';

	if (verbose > 0)
	    fprintf (stderr, "Driver %s: socket=%d\n", dp->name, sockfd);
}

/* open a connection to the given host and port or die.
 * return socket fd.
 */
static int
openINDIServer (char host[], int indi_port)
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
	serv_addr.sin_port = htons(indi_port);
	if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
	    fprintf (stderr, "socket(%s,%d): %s\n", host, indi_port,strerror(errno));
	    exit(1);
	}

	/* connect */
	if (connect (sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr))<0){
	    fprintf (stderr, "connect(%s,%d): %s\n", host,indi_port,strerror(errno));
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
	
	/* bind to given port for any IP address */
	memset (&serv_socket, 0, sizeof(serv_socket));
	serv_socket.sin_family = AF_INET;
	#ifdef SSH_TUNNEL
	serv_socket.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
	#else
	serv_socket.sin_addr.s_addr = htonl (INADDR_ANY);
	#endif
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
	fd_set rs, ws;
	int maxfd;
	int i, s;

	/* init with no writers or readers */
	FD_ZERO(&ws);
	FD_ZERO(&rs);

	/* always listen for new clients */
	FD_SET(lsocket, &rs);
	maxfd = lsocket;

	/* add all client readers and pending client writers */
	for (i = 0; i < nclinfo; i++) {
	    ClInfo *cp = &clinfo[i];
	    if (cp->active) {
		FD_SET(cp->s, &rs);
		if (nFQ(cp->msgq) > 0)
		    FD_SET(cp->s, &ws);
		if (cp->s > maxfd)
		    maxfd = cp->s;
	    }
	}

	/* add all driver readers and pending driver writers */
	for (i = 0; i < ndvrinfo; i++) {
	    DvrInfo *dp = &dvrinfo[i];
	    FD_SET(dp->rfd, &rs);
	    if (dp->rfd > maxfd)
		maxfd = dp->rfd;
	    if (nFQ(dp->msgq) > 0) {
		FD_SET(dp->wfd, &ws);
		if (dp->wfd > maxfd)
		    maxfd = dp->wfd;
	    }
	}

	/* wait for action */
	s = select (maxfd+1, &rs, &ws, NULL, NULL);
	if (s < 0) {
	    fprintf (stderr, "%s: select(%d): %s\n",me,maxfd+1,strerror(errno));
	    exit(1);
	}

	/* new client? */
	if (s > 0 && FD_ISSET(lsocket, &rs)) {
	    newClient();
	    s--;
	}

	/* message to/from client? */
	for (i = 0; s > 0 && i < nclinfo; i++) {
	    ClInfo *cp = &clinfo[i];
	    if (cp->active) {
		if (FD_ISSET(cp->s, &rs)) {
		    readFromClient(cp);
		    s--;
		}
	    }
	    if (cp->active) {	/* read might have shut it down */
		if (FD_ISSET(cp->s, &ws)) {
		    sendClientMsg(cp);
		    s--;
		}
	    }
	}

	/* message to/from driver? */
	for (i = 0; s > 0 && i < ndvrinfo; i++) {
	    DvrInfo *dp = &dvrinfo[i];
	    if (FD_ISSET(dp->rfd, &rs)) {
		readFromDriver(dp);
		s--;
	    }
	    if (FD_ISSET(dp->wfd, &ws) && nFQ(dp->msgq) > 0) {
		sendDriverMsg(dp);
		s--;
	    }
	}
}

/* prepare for new client arriving on lsocket.
 * exit if trouble.
 */
static void
newClient()
{
	ClInfo *cp;
	int s, cli;

	/* assign new socket */
	s = newClSocket ();

	/* try to reuse a clinfo slot, else add one */
	for (cli = 0; cli < nclinfo; cli++)
	    if (!(cp = &clinfo[cli])->active)
		break;
	if (cli == nclinfo) {
	    /* grow clinfo */
	    clinfo = (ClInfo *) realloc (clinfo, (nclinfo+1)*sizeof(ClInfo));
	    if (!clinfo) {
		fprintf (stderr, "%s: no memory for new client\n", me);
		exit(1);
	    }
	    cp = &clinfo[nclinfo++];
	}

	/* rig up new clinfo entry */
	memset (cp, 0, sizeof(*cp));
	cp->active = 1;
	cp->s = s;
	cp->lp = newLilXML();
	cp->msgq = newFQ(1);
	cp->devs = malloc (1);
	cp->wfp = fdopen (cp->s, "a");

	if (verbose > 0)
	    fprintf (stderr, "Client %d: new arrival - welcome!\n", cp->s);
}

/* read more from the given client, send to each appropriate driver when see
 * xml closure. also send all newXXX() to all other interested clients.
 * shut down client if any trouble.
 */
static void
readFromClient (ClInfo *cp)
{
	char buf[BUFSZ];
	int i, nr;

	/* read client */
	nr = read (cp->s, buf, sizeof(buf));
	if (nr <= 0) {
	    if (nr < 0)
		fprintf (stderr, "Client %d: read: %s\n",cp->s,strerror(errno));
	    else if (verbose > 0)
		fprintf (stderr, "Client %d: read EOF\n", cp->s);
	    shutdownClient (cp);
	    return;
	}

	/* process XML, sending when find closure */
	for (i = 0; i < nr; i++) {
	    char err[1024];
	    XMLEle *root = readXMLEle (cp->lp, buf[i], err);
	    if (root) {
		char *roottag = tagXMLEle(root);
		char *dev = findXMLAttValu (root, "device");
		Msg *mp;

		if (verbose > 1) {
		    fprintf (stderr, "Client %d: read ", cp->s);
		    logMsg (root);
		}

		/* snag enableBLOB */
		if (!strcmp (roottag, "enableBLOB"))
		    cp->blob = crackBLOB (pcdataXMLEle(root));

		/* snag interested devices */
		if (!strcmp (roottag, "getProperties")) {
		    addClDevice (cp, dev);
		    cp->sawGetProperties = 1;
		}

		/* send message to driver(s) responsible for dev */
		mp = q2Drivers (root, dev, NULL);

		/* echo new* commands back to other clients */
		if (!strncmp (roottag, "new", 3))
		    q2Clients (cp, root, dev, mp);

		/* N.B. delXMLele(root) called when root no longer in any q */

	    } else if (err[0])
		fprintf (stderr, "Client %d: %s\n", cp->s, err);
	}
}

/* read more from the given driver, send to each interested client when see
 * xml closure. if driver dies, try to restarting up to MAXDRS times.
 */
static void
readFromDriver (DvrInfo *dp)
{
	char buf[BUFSZ];
	int i, nr;
	Msg *mp=NULL;

	/* read driver */
	nr = read (dp->rfd, buf, sizeof(buf));
	if (nr <= 0) {
	    if (nr < 0) 
		fprintf (stderr, "Driver %s: %s\n", dp->name, strerror(errno));
	    else
		fprintf (stderr, "Driver %s: died, or failed to start\n",
								    dp->name);
	    restartDvr (dp);
	    return;
	}

	/* process XML, sending when find closure */
	for (i = 0; i < nr; i++) {
	    char err[1024];
	    XMLEle *root = readXMLEle (dp->lp, buf[i], err);
	    if (root) {
		char *dev = findXMLAttValu (root, "device");

		if (verbose > 1) {
		   fprintf(stderr, "Driver %s: read ", dp->name);
		   logMsg (root);
		}

		/* snag device name if not known yet */
		if (!dp->dev[0] && dev[0]) {
		    strncpy (dp->dev, dev, sizeof(IDev)-1);
		    dp->dev[sizeof(IDev)-1] = '\0';
		}

		/* send to interested observers */
		mp = q2Observers (dp, root, dev, NULL);

		/* send to interested clients */
		q2Clients (NULL, root, dev, mp);

		/* N.B. delXMLele(root) called when root no longer in any q */

	    } else if (err[0])
		fprintf (stderr, "Driver %s: %s\n", dp->name, err);
	}
}

/* close down the given client */
static void
shutdownClient (ClInfo *cp)
{
	Msg *mp;

	/* close connection */
	shutdown (cp->s, SHUT_RDWR);
	fclose (cp->wfp);		/* also closes cp->s */
        cp->wfp = 0;

	/* free memory */
	delLilXML (cp->lp);
	free (cp->devs);
        cp->devs = 0;

	/* decrement and possibly free any unsent messages for this client */
	while ((mp = (Msg*) popFQ(cp->msgq)) != NULL)
	    if (--mp->count == 0)
		freeMsg (mp);
	delFQ (cp->msgq);

	/* ok now to recycle */
	cp->active = 0;

	if (verbose > 0)
	    fprintf (stderr, "Client %d: shut down complete - bye!\n", cp->s);
}

/* close down the given driver and restart if not too many already */
static void
restartDvr (DvrInfo *dp)
{
	/* JM: Alert observers */
	alertObservers(dp);
	
	/* make sure it's dead, reclaim resources */
	if (dp->pid == NOPID) {
	    /* socket connection */
	    shutdown (dp->wfd, SHUT_RDWR);
	    fclose (dp->wfp);		/* also closes wfd */
	} else {
	    /* local pipe connection */
	    kill (dp->pid, SIGKILL);	/* we've insured there are no zombies */
	    fclose (dp->wfp);		/* also closes wfd */
	    close (dp->rfd);
	}
	delLilXML (dp->lp);
	delFQ (dp->msgq);

	/* restart unless too many already */
	if (++dp->restarts > maxdrs) {
	    fprintf (stderr, "Driver %s: still dead after %d restarts\n",
							    dp->name, maxdrs);
	    exit(1);
	}
	fprintf (stderr, "Driver %s: restart #%d\n", dp->name, dp->restarts);
	startDvr (dp);
}

/* queue the xml command in root to each driver supporting device dev, or all
 *   drivers if unknown.
 * add more count to mp if not NULL, else make a fresh Msg here.
 * return the Msg we create in case we want to add it to more queues. We may
 *   return NULL if there were no drivers interested in root msg.
 */
static Msg *
q2Drivers (XMLEle *root, char *dev, Msg *mp)
{
	DvrInfo *dp;

	if (!mp) {
	    /* build a new message */
	    mp = (Msg *) malloc (sizeof(Msg));
	    mp->ep = root;
	    mp->count = 0;
	}

	/* queue message to each interested driver */
	for (dp = dvrinfo; dp < &dvrinfo[ndvrinfo]; dp++) {
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

	    /* ok: queue message to given driver */
	    mp->count++;
	    pushFQ (dp->msgq, mp);
	    if (verbose > 2)
		fprintf (stderr,"Driver %s: message %d queued with count %d\n",
					    dp->name, nFQ(dp->msgq), mp->count);

	    if (adddev)
		rmXMLAtt (root, "device");
	}

	/* forget it if no drivers wanted the message */
	if (mp->count == 0) {
	    if (verbose > 2) {
		fprintf (stderr, "no drivers want ");
		logMsg (root);
	    }
	    freeMsg (mp);
	    mp = NULL;
	}

	/* return mp in case someone else wants to add to it */
	return (mp);
}

/* queue the xml command in root known to be from the given device to each
 *   interested client, except notme.
 * add more count to mp if not NULL, else make a fresh Msg here.
 * return the Msg we create in case we want to add it to more queues. We may
 *   return NULL if there were no clients interested in root msg.
 */
static Msg *
q2Clients (ClInfo *notme, XMLEle *root, char *dev, Msg *mp)
{
	ClInfo *cp;

	/* Ignore subscribtion requests, don't send them to clients */
	if (!strcmp(tagXMLEle(root), "propertyVectorSubscribtion"))
		return NULL;

	if (!mp) {
	    /* build a new message */
	    mp = (Msg *) malloc (sizeof(Msg));
	    mp->ep = root;
	    mp->count = 0;
	}
	
	/* queue message to each interested client */
	for (cp = clinfo; cp < &clinfo[nclinfo]; cp++) {
	    int isblob;

	    /* cp in use? notme? valid dev? blob? */
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
	    if (verbose > 2)
		fprintf (stderr,"Client %d: message %d queued with count %d\n",
					    cp->s, nFQ(cp->msgq), mp->count);
	}

	/* forget it if no clients wanted the message */
	if (mp->count == 0) {
	    if (verbose > 2) {
		fprintf (stderr, "no clients want ");
		logMsg (root);
	    }
	    freeMsg (mp);
	    mp = NULL;
	}

	/* return mp in case someone else wants to add to it */
	return (mp);
}

Msg *
q2Observers  (DvrInfo *sender, XMLEle *root, char *dev, Msg *mp)
{
	ObserverInfo *ob;
	XMLAtt* ap;
	int prop_state = 0;
	char prop_dev[MAXINDIDEVICE];
	char prop_name[MAXINDINAME];
	
	dev=dev;

	/* Subscribtion request */
	if (!strcmp(tagXMLEle(root), "propertyVectorSubscribtion"))
	{
		manageObservers(sender, root);
		return NULL;
	}
	/* We discard messages */
       else if (!strcmp(tagXMLEle(root), "message"))
		return NULL;

	/* We have no observers, return */
	if (nobserverinfo_active == 0)
		return NULL;

	ap = findXMLAtt(root, "device");
	if (!ap)
	{
		fprintf(stderr, "<%s> missing 'device' attribute.\n", tagXMLEle(root));
		return NULL;
	}
	else
		strncpy(prop_dev, valuXMLAtt(ap), MAXINDIDEVICE);
	
	/* Del prop might not have name, so don't panic */
	ap = findXMLAtt(root, "name");
	if (!ap && strcmp(tagXMLEle(root), "delProperty"))
	{
		fprintf(stderr, "<%s> missing 'name' attribute.\n", tagXMLEle(root));
		return NULL;
	}
	else if (ap)
		strncpy(prop_name, valuXMLAtt(ap), MAXINDINAME);
	
	/* Del prop does not have state, so don't panic */
	ap = findXMLAtt(root, "state");
	if (!ap && strcmp(tagXMLEle(root), "delProperty"))
	{
		fprintf(stderr, "<%s> missing 'name' attribute.\n", tagXMLEle(root));
		return NULL;
	}
	else if (ap)
	{
		prop_state = crackPropertyState(valuXMLAtt(ap));
		if (prop_state < 0)
		{
			fprintf(stderr, "<%s> invalid property state '%s'.\n", tagXMLEle(root), valuXMLAtt(ap));
			return NULL;
		}
	}

	if (!mp) {
	    /* build a new message */
	    mp = (Msg *) malloc (sizeof(Msg));
	    mp->ep = root;
	    mp->count = 0;
	}
			
	/* Now let's see if a registered observer is interested in this property */
	for (ob = observerinfo; ob < &observerinfo[nobserverinfo]; ob++)
	{
		if (!ob->in_use)
			continue;
		
		if (!strcmp(ob->dev, prop_dev) && ((!strcmp(tagXMLEle(root), "delProperty")) || (!strcmp(ob->name, prop_name))))
		{
			/* Check for state requirtments only if property is of setXXXVector type
			   defXXX and delXXX go through */
			if (strstr(tagXMLEle(root), "set") && ob->type == IDT_STATE)
			{
				/* If state didn't change, there is no need to update observer */
				if (ob->last_state == (unsigned) prop_state) {
					free(mp);
					return NULL;
				}
				
				/* Otherwise, record last state transition */
				ob->last_state = prop_state;
			}
			/* ok: queue message to given driver */
			mp->count++;
			pushFQ (ob->dp->msgq, mp);
			if (verbose > 2)
				fprintf (stderr,"Driver %s: message %d queued with count %d\n",
					 ob->dp->name, nFQ(ob->dp->msgq), mp->count);
			
			break;
		}
		
	}	

  /* Return mp if anyone else want to use it */
  return mp;

}

void 
manageObservers (DvrInfo *dp, XMLEle *root)
{

	char ob_dev[MAXINDIDEVICE];
    	char ob_name[MAXINDINAME];
	int ob_type = 0;
	XMLAtt* ap;
	ObserverInfo* ob;
	
	ap = findXMLAtt(root, "device");
	if (!ap)
	{
		fprintf(stderr, "<%s> missing 'device' attribute.\n", tagXMLEle(root));
		return;
	}
	
	strncpy(ob_dev, valuXMLAtt(ap), MAXINDIDEVICE);
	
	ap = findXMLAtt(root, "name");
	if (!ap)
	{
		fprintf(stderr, "<%s> missing 'name' attribute.\n", tagXMLEle(root));
		return;
	}
	
	strncpy(ob_name, valuXMLAtt(ap), MAXINDINAME);
	
	ap = findXMLAtt(root, "notification");
	if (ap)
	{
		ob_type = crackObserverState(valuXMLAtt(ap));
		if (ob_type < 0)
		{
			fprintf(stderr, "<%s> invalid notification state '%s'.\n", tagXMLEle(root), valuXMLAtt(ap));
			return;
		}
	}
		
	ap = findXMLAtt(root, "action");
	if (!ap)
	{
		fprintf(stderr, "<%s> missing 'action' attribute.\n", tagXMLEle(root));
		return;
	}
	
	/* Subscribe */
	if (!strcmp(valuXMLAtt(ap), "subscribe"))
	{
		/* First check if it's already in the list. If found, silenty ignore. */
		for (ob = observerinfo; ob < &observerinfo[nobserverinfo]; ob++)
		{
			if (ob->in_use && (ob->dp == dp) && !strcmp(ob->dev, ob_dev) && !strcmp(ob->name, ob_name)
						 && (ob->type == (unsigned) ob_type))
				return;
		}
		
		/* Next check for the first avaiable slot to use */
		for (ob = observerinfo; ob < &observerinfo[nobserverinfo]; ob++)
		{
			if (!ob->in_use)
				break;
		}	
		
		/* If list if full, allocate more memory, or seed */
		if (ob == &observerinfo[nobserverinfo])
		{
			observerinfo = observerinfo ? (ObserverInfo *) realloc (observerinfo, (nobserverinfo+1)*sizeof(ObserverInfo)) 
				: (ObserverInfo *) malloc (sizeof(ObserverInfo));
			
			ob = &observerinfo[nobserverinfo++];
		}

		/* init new entry */
		ob->in_use 	= 1;
		ob->dp 		= dp;
		ob->type 	= ob_type;
		ob->last_state 	= -1;
		strncpy(ob->dev, ob_dev, MAXINDIDEVICE);
		strncpy(ob->name, ob_name, MAXINDINAME);
		
		nobserverinfo_active++;

		if (verbose > 1)
			fprintf(stderr, "Added observer <%s> to listen to changes in %s.%s\n", dp->dev, ob->dev, ob->name);

	}
	else if (!strcmp(valuXMLAtt(ap), "unsubscribe"))
	{
		/* Search list, if found, set in_use to 0 */
		for (ob = observerinfo; ob < &observerinfo[nobserverinfo]; ob++)
		{
			if ((ob->dp == dp) && !strcmp(ob->dev, ob_dev) && !strcmp(ob->name, ob_name))
			{
				ob->in_use = 0;
				nobserverinfo_active--;
				if (verbose > 1)
					fprintf(stderr, "Removed observer <%s> which was listening to changes in %s.%s\n", dp->dev, ob->dev, ob->name);
				break;
			}
		}
	}
	else
	{
		fprintf(stderr, "<%s> invalid action value: %s\n", tagXMLEle(root), valuXMLAtt(ap));
	}


}

/* free Msg mp and everything it contains */
static void
freeMsg (Msg *mp)
{
	delXMLEle (mp->ep);
	free (mp);
}

static void alertObservers(DvrInfo *dp)
{
	ObserverInfo* ob;
	Msg *mp;
	char xmlAlertStr[BUFSZ];
	char errmsg[BUFSZ];
	XMLEle* xmlAlert = NULL;
	unsigned int i=0;
	
	snprintf(xmlAlertStr, BUFSZ, "<subscribtionAlert device='%s' alert='died' />", dp->dev);
	
	for (i=0; i < strlen(xmlAlertStr); i++)
	{
		xmlAlert = readXMLEle(dp->lp, xmlAlertStr[i], errmsg);
		if (xmlAlert)
			break;
	}
	
	/* Shouldn't happen! */
	if (!xmlAlert) return;
	
	/* build a new message */
	mp = (Msg *) malloc (sizeof(Msg));
	mp->ep = xmlAlert;
	mp->count = 0;
	
	/* Check if any observers are listening to this driver */
	for (ob = observerinfo; ob < &observerinfo[nobserverinfo]; ob++)
	{
		if (!strcmp(ob->dev,dp->dev))
		{
			mp->count++;
			pushFQ (ob->dp->msgq, mp);
			if (verbose > 2)
				fprintf (stderr,"Driver %s: message %d queued with count %d\n",
					 ob->dp->name, nFQ(ob->dp->msgq), mp->count);
		}
	}
	if (!mp->count) /* not added anywhere */
	    freeMsg(mp);
}

/* write the next message in the queue for the given client.
 * free the message if we are the last client to use it.
 * shut down this client if trouble.
 */
static void
sendClientMsg (ClInfo *cp)
{
	Msg *mp;

	if (!cp->active)
		return;
	
	/* get next message for this client */
	mp = popFQ (cp->msgq);

	/* send it */
	prXMLEle (cp->wfp, mp->ep, 0);
	fflush (cp->wfp);

	/* trace */
	if (verbose > 2) {
	    fprintf (stderr, "Client %d: send msg %d:\n", cp->s, nFQ(cp->msgq));
	    prXMLEle (stderr, mp->ep, 0);
	} else if (verbose > 1) {
	    fprintf (stderr, "Client %d: sent msg %d ", cp->s, nFQ(cp->msgq));
	    logMsg (mp->ep);
	}

	/* update message usage count, free if goes to 0 */
	if (--mp->count == 0) {
	    if (verbose > 2)
		fprintf (stderr, "Client %d: last client, freeing msg\n",cp->s);
	    freeMsg (mp);
	}

	/* shut down this client if encountered write errors */
	if (ferror(cp->wfp)) {
	    fprintf (stderr, "Client %d: write: %s\n", cp->s,
							strerror(errno));
	    shutdownClient (cp);
	}
}

/* write the next message in the queue for the given driver.
 * free the message if we are the last driver to use it.
 * restart this driver if trouble.
 */
static void
sendDriverMsg (DvrInfo *dp)
{
	Msg *mp;

	/* get next message for this driver */
	mp = popFQ (dp->msgq);

	/* send it */
	prXMLEle (dp->wfp, mp->ep, 0);
	fflush (dp->wfp);

	/* trace */
	if (verbose > 2) {
	    fprintf (stderr,"Driver %s: send msg %d:\n",dp->name,nFQ(dp->msgq));
	    prXMLEle (stderr, mp->ep, 0);
	} else if (verbose > 1) {
	    fprintf (stderr, "Driver %s: sent msg %d ", dp->name,nFQ(dp->msgq));
	    logMsg (mp->ep);
	}

	/* update message usage count, free if goes to 0 */
	if (--mp->count == 0) {
	    if (verbose > 2)
		fprintf (stderr, "Driver %s: last client, freeing msg\n",
								    dp->name);
	    freeMsg (mp);
	}

	/* restart this driver if encountered write errors */
	if (ferror(dp->wfp)) {
	    fprintf (stderr, "Driver %s: write: %s\n",dp->name,strerror(errno));
	    restartDvr (dp);
	}
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
 */
static void
addClDevice (ClInfo *cp, char *dev)
{

	if (dev[0]) {
	    char *ip;
	    cp->devs = (IDev *) realloc (cp->devs,(cp->ndevs+1)*sizeof(IDev));
	    ip = (char*)&cp->devs[cp->ndevs++];
	    strncpy (ip, dev, sizeof(IDev)-1);
	    ip[sizeof(IDev)-1] = '\0';
	}
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
static BLOBHandling
crackBLOB (char enableBLOB[])
{
	if (!strcmp (enableBLOB, "Also"))
	    return (B_ALSO);
	if (!strcmp (enableBLOB, "Only"))
	    return (B_ONLY);
	return (B_NEVER);
}

/* print key attributes and values of the given xml to stderr.
 */
static void
logMsg (XMLEle *root)
{
	static char *prtags[] = {
	    "defNumber", "oneNumber",
	    "defText",   "oneText",
	    "defSwitch", "oneSwitch",
	    "defLight",  "oneLight",
	};
	XMLEle *e;
	char *msg, *perm;
	int i;

	/* print tag header */
	fprintf (stderr, "%s %s %s %s", tagXMLEle(root),
						findXMLAttValu(root,"device"),
						findXMLAttValu(root,"name"),
						findXMLAttValu(root,"state"));
	perm = findXMLAttValu(root,"perm");
	if (perm[0])
	    fprintf (stderr, " %s", perm);
	msg = findXMLAttValu(root,"message");
	if (msg[0])
	    fprintf (stderr, " '%s'", msg);

	/* print each array value */
	for (e = nextXMLEle(root,1); e; e = nextXMLEle(root,0))
	    for (i = 0; i < sizeof(prtags)/sizeof(prtags[0]); i++)
		if (strcmp (prtags[i], tagXMLEle(e)) == 0)
		    fprintf (stderr, " %s='%s'", findXMLAttValu(e,"name"),
							    pcdataXMLEle (e));

	fprintf (stderr, "\n");
}

int crackObserverState(char *stateStr)
{
	if (!strcmp(stateStr, "Value"))
		return (IDT_VALUE);
	else if (!strcmp(stateStr, "State"))
		return (IDT_STATE);
	else if (!strcmp(stateStr, "All"))
		return (IDT_ALL);
	
	else return -1;
}

int crackPropertyState(char *pstateStr)
{
	if (!strcmp(pstateStr, "Idle"))
		return IPS_IDLE;
	else if (!strcmp(pstateStr, "Ok"))
		return IPS_OK;
	else if (!strcmp(pstateStr, "Busy"))
		return IPS_BUSY;
	else if (!strcmp(pstateStr, "Alert"))
		return IPS_ALERT;
	else return -1;
}

