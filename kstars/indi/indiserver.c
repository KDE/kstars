#if 0
    INDI
    Copyright (C) 2003 Elwood C. Downey

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#endif

/* INDI Server.
 * argv lists names of driver processes to run.
 * Clients can come and go and will see each device reported by each driver.
 * all newXXX() received from one Client are sent to all other Clients.
 *   atomicity is achieved by XML parsing and printing, a bit crude.
 *   TODO: since we are parsing, could send drivers only cmds for their devices.
 * each driver process's stderr is connected to our stderr.
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

#define INDIPORT        7624            /* TCP/IP port on which to listen */
#define	BUFSZ		1024		/* max buffering here */
#define	MAXRS		2		/* default times to restart a driver */

/* info for each connected client */
typedef struct {
    int active;				/* 1 when this record is in use */
    int s;				/* private socket */
    FILE *wfp;				/* FILE to write to s */
    LilXML *lp;				/* XML parsing context */
} ClInfo;
static ClInfo *clinfo;			/* malloced array of clients */
static int nclinfo;			/* n total (not n active) */

/* info for each connected driver */
typedef struct {
    char *name;				/* process name */
    int pid;				/* process id */
    int rfd;				/* read pipe fd */
    FILE *wfp;				/* write pipe fp */
    int restarts;			/* times process has been restarted */
    LilXML *lp;				/* XML parsing context */
} DvrInfo;
static DvrInfo *dvrinfo;		/* malloced array of drivers */
static int ndvrinfo;			/* n total */

static void usage (void);
static void noZombies (void);
static void noSigPipe (void);
static void indiListen (void);
static void indiRun (void);
static void newClient (void);
static int newClSocket (void);
static void closeClient (int cl);
static void clientMsg (int cl);
static void startDvr (char *name);
static void restartDvr (int i);
static void send2AllDrivers (XMLEle *root);
static void send2AllClients (ClInfo *notcp, XMLEle *root);
static void driverMsg (int dn);

static char *me;			/* our name */
static int port = INDIPORT;		/* public INDI port */
static int verbose;			/* more chatty */
static int maxrs = MAXRS;		/* max times to restart dieing driver */
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
		    maxrs = atoi(*++av);
		    ac--;
		    break;
		case 'v':
		    verbose++;
		    break;
		default:
		    usage();
		}
	}

	/* seed arrays so we can always use realloc */
	clinfo = (ClInfo *) malloc (sizeof(ClInfo));
	nclinfo = 0;
	dvrinfo = (DvrInfo *) malloc (sizeof(DvrInfo));
	ndvrinfo = 0;

	/* start each driver */
	if (ac == 0)
	    usage();
	noZombies();
	noSigPipe();
	while (ac-- > 0)
	    startDvr (*av++);

	/* announce we are online */
	indiListen();

	/* accept and service clients until fatal error */
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
	fprintf (stderr, "Usage: %s [options] [driver ...]\n", me);
	fprintf (stderr, "Purpose: INDI Server\n");
	fprintf (stderr, "Options:\n");
	fprintf (stderr, " -p p  : alternate IP port, default %d\n", INDIPORT);
	fprintf (stderr, " -r n  : max restart attempts, default %d\n", MAXRS);
	fprintf (stderr, " -vv   : more verbose to stderr\n");
	fprintf (stderr, "Remaining args are names of INDI drivers to run.\n");

	exit (1);
}

/* arrange for no zombies if things go badly */
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
noSigPipe()
{
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	(void)sigaction(SIGPIPE, &sa, NULL);
}

/* start the named INDI driver process.
 * exit if trouble.
 * N.B. name memory assumed to persist for duration of server process.
 */
static void
startDvr (char *name)
{
	DvrInfo *dp;
	int rp[2], wp[2];
	int pid;
	int i;

	/* new pipes */
	if (pipe (rp) < 0) {
	    fprintf (stderr, "%s: read pipe: %s\n", me, strerror(errno));
	    exit(1);
	}
	if (pipe (wp) < 0) {
	    fprintf (stderr, "%s: write pipe: %s\n", me, strerror(errno));
	    exit(1);
	}

	/* new process */
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
	    execlp (name, name, NULL);
	    fprintf (stderr, "Driver %s: %s\n", name, strerror(errno));
	    exit (1);	/* parent will notice EOF shortly */
	}

	/* add new or reuse if already in list */
	for (i = 0; i < ndvrinfo; i++)
	    if (!strcmp (dvrinfo[i].name, name))
		break;
	if (i == ndvrinfo) {
	    /* first time */
	    dvrinfo = (DvrInfo *) realloc(dvrinfo,(ndvrinfo+1)*sizeof(DvrInfo));
	    if (!dvrinfo) {
		fprintf (stderr, "%s: no memory for driver %s\n", me, name);
		exit(1);
	    }
	    dp = &dvrinfo[ndvrinfo++];
	    memset (dp, 0, sizeof(*dp));
	    if (verbose > 0)
		fprintf (stderr, "Driver %s: starting\n", name);
	} else {
	    /* restarting, zero out but preserve restarts */
	    int restarts;
	    dp = &dvrinfo[i];
	    restarts = dp->restarts;
	    memset (dp, 0, sizeof(*dp));
	    dp->restarts = restarts;
	    if (verbose > 0)
		fprintf (stderr, "Driver %s: restart #%d\n", name, restarts);
	}

	/* record pid, name, io channel, init lp */
	dp->pid = pid;
	dp->name = name;
	dp->rfd = rp[0];
	close (rp[1]);
	fcntl (wp[1], F_SETFL, O_NONBLOCK);
	dp->wfp = fdopen (wp[1], "a");
	setbuf (dp->wfp, NULL);
	close (wp[0]);
	dp->lp = newLilXML();
	if (verbose > 0)
	    fprintf (stderr, "Driver %s: rfd %d wfd %d\n", name, dp->rfd,wp[1]);
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
	serv_socket.sin_addr.s_addr = htonl (INADDR_ANY);
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

	/* collect all client and driver read fd's */
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
	    fprintf (stderr, "%s: select: %s\n", me, strerror(errno));
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
		clientMsg(i);
		s -= 1;
	    }
	}

	/* message from driver? */
	for (i = 0; s > 0 && i < ndvrinfo; i++) {
	    if (FD_ISSET(dvrinfo[i].rfd, &rs)) {
		driverMsg(i);
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
	ClInfo *cp;
	int s, cli;

	/* assign new socket */
	s = newClSocket ();

	/* try to reuse a clinfo slot, else add one */
	for (cli = 0; cli < nclinfo; cli++)
	    if (!(cp = &clinfo[cli])->active)
		break;
	if (cli == nclinfo) {
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
	cp->wfp = fdopen (cp->s, "a");
	setbuf (cp->wfp, NULL);
	cp->lp = newLilXML();

	if (verbose > 0)
	    fprintf (stderr, "Client %d: new arrival - welcome!\n", cp->s);
}

/* read more from client clinfo[cli], send to each driver when see xml closure.
 * also send all newXXX() to all other clients.
 * restart driver if not accepting commands.
 * shut down client if gives us any trouble.
 */
static void
clientMsg (int cli)
{
	ClInfo *cp = &clinfo[cli];
	char buf[BUFSZ];
	int i, nr;

	/* read client */
	nr = read (cp->s, buf, sizeof(buf));
	if (nr < 0) {
	    fprintf (stderr, "Client %d: %s\n", cp->s, strerror(errno));
	    closeClient (cli);
	    return;
	}
	if (nr == 0) {
	    if (verbose)
		fprintf (stderr, "Client %d: EOF\n", cp->s);
	    closeClient (cli);
	    return;
	} 
	if (verbose > 1)
	    fprintf (stderr, "Client %d: rcv from:\n%.*s", cp->s, nr, buf);

	/* process XML, sending when find closure */
	for (i = 0; i < nr; i++) {
	    char err[1024];
	    XMLEle *root = readXMLEle (cp->lp, buf[i], err);
	    if (root) {
		if (strncmp (tagXMLEle(root), "new", 3) == 0)
		    send2AllClients (cp, root);
		send2AllDrivers (root);
		delXMLEle (root);
	    } else if (err[0])
		fprintf (stderr, "Client %d: %s\n", cp->s, err);
	}
}

/* read more from driver dvrinfo[i], send to each client when see xml closure.
 * if driver dies, try to restart.
 * if any client can not keep up, drop its connection.
 */
static void
driverMsg (int i)
{
	DvrInfo *dp = &dvrinfo[i];
	char buf[BUFSZ];
	int nr;

	/* read driver */
	nr = read (dp->rfd, buf, sizeof(buf));
	if (nr < 0) {
	    fprintf (stderr, "Driver %s: %s\n", dp->name, strerror(errno));
	    restartDvr (i);
	    return;
	}
	if (nr == 0) {
	    fprintf (stderr, "Driver %s: died, or failed to start\n", dp->name);
	    restartDvr (i);
	    return;
	}
	if (verbose > 1)
	    fprintf (stderr, "Driver %s: rcv from:\n%.*s", dp->name, nr, buf);

	/* process XML, sending when find closure */
	for (i = 0; i < nr; i++) {
	    char err[1024];
	    XMLEle *root = readXMLEle (dp->lp, buf[i], err);
	    if (root) {
		send2AllClients (NULL, root);
		delXMLEle (root);
	    } else if (err[0])
		fprintf (stderr, "Driver %s: %s\n", dp->name, err);
	}
}

/* close down clinof[i] */
static void
closeClient (int i)
{
	ClInfo *cp = &clinfo[i];

	fclose (cp->wfp);		/* also closes cp->s */
	cp->active = 0;
	delLilXML (cp->lp);

	if (verbose > 0)
	    fprintf (stderr, "Client %d: closed\n", cp->s);
}

/* close down driver process dvrinfo[i] and restart if not too many already */
static void
restartDvr (int i)
{
	DvrInfo *dp = &dvrinfo[i];

	/* make sure it's dead, reclaim resources */
	kill (dp->pid, SIGKILL);
	fclose (dp->wfp);
	close (dp->rfd);
	delLilXML (dp->lp);

	/* restart unless too many already */
	if (++dp->restarts > maxrs) {
	    fprintf (stderr, "Driver %s: died after %d restarts\n", dp->name,
								    maxrs);
	    exit(1);
	}
	fprintf (stderr, "Driver %s: restart #%d\n", dp->name, dp->restarts);
	startDvr (dp->name);
}

/* send the xml command to each driver */
static void
send2AllDrivers (XMLEle *root)
{
	int i;

	for (i = 0; i < ndvrinfo; i++) {
	    DvrInfo *dp = &dvrinfo[i];
	    prXMLEle (dp->wfp, root, 0);
	    if (ferror(dp->wfp)) {
		fprintf (stderr, "Driver %s: %s\n", dp->name, strerror(errno));
		restartDvr (i);
	    } else if (verbose > 2) {
		fprintf (stderr, "Driver %s: send to:\n", dp->name);
		prXMLEle (stderr, root, 0);
	    } else if (verbose > 1)
		fprintf (stderr, "Driver %s: message sent\n", dp->name);
	}
}

/* send the xml command to all clients, except notcp */
static void
send2AllClients (ClInfo *notcp, XMLEle *root)
{
	int i;

	for (i = 0; i < nclinfo; i++) {
	    ClInfo *cp = &clinfo[i];
	    if (cp == notcp || !cp->active)
		continue;
	    prXMLEle (cp->wfp, root, 0);
	    if (ferror(cp->wfp)) {
		fprintf (stderr, "Client %d: %s\n", cp->s, strerror(errno));
		closeClient (i);
	    } else if (verbose > 2) {
		fprintf (stderr, "Client %d: send to:\n", cp->s);
		prXMLEle (stderr, root, 0);
	    } else if (verbose > 1)
		fprintf (stderr, "Client %d: message sent\n", cp->s);
	}
}

/* new client has arrived on lsocket.
 * accept and return private nonblocking socket or exit.
 */
static int
newClSocket ()
{
	struct sockaddr_in cli_socket;
	int cli_len, cli_fd;

	/* get a private connection to new client */
	cli_len = sizeof(cli_socket);
	cli_fd = accept (lsocket, (struct sockaddr *)&cli_socket, &cli_len);
	if(cli_fd < 0) {
	    fprintf (stderr, "%s: accept: %s", me, strerror(errno));
	    exit (1);
	}

	/* ok */
	fcntl (cli_fd, F_SETFL, O_NONBLOCK);
	return (cli_fd);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile$ $Date$ $Revision$ $Name:  $"};
