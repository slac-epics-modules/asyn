/* E2050Reboot.c */

/***********************************************************************
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory, and the Regents of the University of
* California, as Operator of Los Alamos National Laboratory, and
* Berliner Elektronenspeicherring-Gesellschaft m.b.H. (BESSY).
* asynDriver is distributed subject to a Software License Agreement
* found in file LICENSE that is included with this distribution.
***********************************************************************/

/* REBOOT code for HP E2050 LAN to GPIB server
 * Author: Marty Kraimer.
 * Extracted from Code by Benjamin Franksen and Stephanie Allison
 *****************************************************************************/

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>

#include <osiSock.h>
#include <epicsThread.h>

int E2050Reboot(char * inetAddr)
{
    struct sockaddr_in serverAddr;
    int fd;
    int status;
    int nbytes;

    errno = 0;
    fd = epicsSocketCreate(PF_INET, SOCK_STREAM, 0);
    if(fd == -1) {
        printf("can't create socket %s\n",strerror(errno));
        return(-1);
    }
    memset((char*)&serverAddr, 0, sizeof(struct sockaddr_in));
    serverAddr.sin_family = PF_INET;
    /* 23 is telnet port */
    status = aToIPAddr(inetAddr,23,&serverAddr);
    if(status) {
        printf("aToIPAddr failed\n");
        return(-1); 
    }
    errno = 0;
    status = connect(fd,(struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if(status) {
        printf("can't connect %s\n",strerror (errno));
        close(fd);
        return(-1);
    }
    nbytes = send(fd,"reboot\ny\n",9,0);
    if(nbytes!=9) printf("nbytes %d expected 9\n",nbytes);
    close(fd);
    epicsThreadSleep(20.0);
    return(0);
}