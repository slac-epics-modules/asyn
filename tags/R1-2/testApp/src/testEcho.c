/*testEcho.c */
/***********************************************************************
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory, and the Regents of the University of
* California, as Operator of Los Alamos National Laboratory, and
* Berliner Elektronenspeicherring-Gesellschaft m.b.H. (BESSY).
* asynDriver is distributed subject to a Software License Agreement
* found in file LICENSE that is included with this distribution.
***********************************************************************/

/* client to test testEcho
 *
 * Author: Marty Kraimer
 */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <cantProceed.h>
#include <epicsStdio.h>
#include <epicsStdio.h>
#include <epicsThread.h>
#include <epicsEvent.h>
#include <epicsExport.h>
#include <iocsh.h>

#include <asynDriver.h>

typedef struct testPvt {
    asynUser *pasynUser;
    char *prefix;
    int ntimes;
    int itimes;
    int isRead;
    double queueTimeout;
    char buffer[80];
    asynOctet *pasynOctet;
    void *asynOctetPvt;
    epicsEventId done;
}testPvt;

void queueCallback(asynUser *pasynUser)
{
    testPvt *ptestPvt = (testPvt *)pasynUser->userPvt;
    asynOctet *pasynOctet = ptestPvt->pasynOctet;
    void *asynOctetPvt = ptestPvt->asynOctetPvt;
    int nchars;
    asynStatus status;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,
        "%s queueCallback entered\n",ptestPvt->prefix);
    if(ptestPvt->isRead) {
        nchars = pasynOctet->read(asynOctetPvt,
             pasynUser,ptestPvt->buffer,80);
        asynPrintIO(pasynUser,ASYN_TRACEIO_DEVICE,ptestPvt->buffer,nchars,
            "%s received %d chars",ptestPvt->prefix,nchars);
        asynPrint(pasynUser,ASYN_TRACE_FLOW,"%s calling unlock\n",ptestPvt->prefix);
        status = pasynManager->unlock(pasynUser);
        if(status!=asynSuccess) {
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"%s\n",pasynUser->errorMessage);
        }
    } else {
        sprintf(ptestPvt->buffer,"%s_ntime_%d",
            ptestPvt->prefix,ptestPvt->itimes);
        asynPrintIO(pasynUser,ASYN_TRACEIO_DEVICE,ptestPvt->buffer,
           strlen(ptestPvt->buffer),"%s send %d chars",
           ptestPvt->prefix,strlen(ptestPvt->buffer));
        nchars = pasynOctet->write(asynOctetPvt,
            pasynUser,ptestPvt->buffer,strlen(ptestPvt->buffer));
    }
    epicsEventSignal(ptestPvt->done);
}

void timeoutCallback(asynUser *pasynUser)
{
    testPvt *ptestPvt = (testPvt *)pasynUser->userPvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW|ASYN_TRACE_ERROR,"%s timeoutCallback\n",
        ptestPvt->prefix);
    epicsEventSignal(ptestPvt->done);
}

static void writeReadThread(testPvt *ptestPvt)
{
    asynUser *pasynUser = ptestPvt->pasynUser;
    asynStatus status;

    ptestPvt->isRead = 1;
    while(1) {
        if(ptestPvt->isRead) {
            ptestPvt->itimes++;
            if(ptestPvt->ntimes && ptestPvt->ntimes<ptestPvt->itimes) break;
            ptestPvt->isRead = 0;
            asynPrint(pasynUser,ASYN_TRACE_FLOW,"%s calling lock\n",ptestPvt->prefix);
            status = pasynManager->lock(pasynUser);
            if(status!=asynSuccess) {
                asynPrint(pasynUser,ASYN_TRACE_ERROR,"%s\n",pasynUser->errorMessage);
            }
        } else {
            ptestPvt->isRead = 1;
        }
        asynPrint(pasynUser,ASYN_TRACE_FLOW,
            "%s calling queueRequest\n",ptestPvt->prefix);
        status = pasynManager->queueRequest(pasynUser,
            asynQueuePriorityLow,ptestPvt->queueTimeout);
        if(status!=asynSuccess) {
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"%s\n",pasynUser->errorMessage);
            pasynManager->freeAsynUser(pasynUser);
            break;
        }
        epicsEventMustWait(ptestPvt->done);
    }
    asynPrint(pasynUser,ASYN_TRACE_FLOW,"%s calling disconnectDevice\n",
        ptestPvt->prefix);
    status = pasynManager->disconnectDevice(pasynUser);
    if(status!=asynSuccess) asynPrint(pasynUser,ASYN_TRACE_ERROR,"%s disconnectDevice %s\n",
        ptestPvt->prefix,pasynUser->errorMessage);
    status = pasynManager->freeAsynUser(pasynUser);
    if(status!=asynSuccess) asynPrint(pasynUser,ASYN_TRACE_ERROR,"%s freeAsynUser %s\n",
        ptestPvt->prefix,pasynUser->errorMessage);
    epicsEventDestroy(ptestPvt->done);
    printf("%s exiting\n",ptestPvt->prefix);
    free(ptestPvt->prefix);
    free(ptestPvt);
}

static int testEcho(const char *portName,int addr,const char *pre,
    int ntimes,double queueTimeout)
{
    asynUser *pasynUser;
    testPvt *ptestPvt;
    char *prefix;
    asynInterface *pasynInterface;
    asynCommon *pasynCommon;
    void *asynCommonPvt;
    asynStatus status;

    prefix = calloc(strlen(pre)+1,sizeof(char));
    strcpy(prefix,pre);
    ptestPvt = calloc(1,sizeof(testPvt));
    ptestPvt->prefix = prefix;
    ptestPvt->queueTimeout = queueTimeout;
    ptestPvt->ntimes = ntimes;
    ptestPvt->done = epicsEventMustCreate(epicsEventEmpty);
    pasynUser = pasynManager->createAsynUser(
        queueCallback,timeoutCallback);
    pasynUser->userPvt = ptestPvt;
    ptestPvt->pasynUser = pasynUser;
    status = pasynManager->connectDevice(pasynUser,portName,addr);
    if(status!=asynSuccess) {
        printf("%s\n",pasynUser->errorMessage);
        pasynManager->freeAsynUser(pasynUser);
        return(-1);
    }
    asynPrint(pasynUser,ASYN_TRACE_FLOW,
        "%s connected and calling findInterface\n",
        ptestPvt->prefix);
    pasynInterface = pasynManager->findInterface(
        pasynUser,asynCommonType,1);
    if(!pasynInterface) {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,
            "%s %s\n",asynCommonType,pasynUser->errorMessage);
        pasynManager->freeAsynUser(pasynUser);
        return(-1);
    }
    pasynCommon = (asynCommon *)pasynInterface->pinterface;
    asynCommonPvt = pasynInterface->drvPvt;
    pasynInterface = pasynManager->findInterface(
        pasynUser,asynOctetType,1);
    if(!pasynInterface) {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"%s %s\n",
            asynOctetType,pasynUser->errorMessage);
        pasynManager->freeAsynUser(pasynUser);
        return(-1);
    }
    ptestPvt->pasynOctet = (asynOctet *)pasynInterface->pinterface;
    ptestPvt->asynOctetPvt = pasynInterface->drvPvt;
    epicsThreadCreate(prefix,epicsThreadPriorityLow,
        epicsThreadGetStackSize(epicsThreadStackSmall),
        (EPICSTHREADFUNC)writeReadThread,(void *)ptestPvt);
    return(0);
}

/* register testEcho*/
static const iocshArg testEchoArg0 = { "portName", iocshArgString };
static const iocshArg testEchoArg1 = { "addr", iocshArgInt };
static const iocshArg testEchoArg2 = { "prefix", iocshArgString };
static const iocshArg testEchoArg3 = { "ntimes", iocshArgInt };
static const iocshArg testEchoArg4 = { "queueTimeout", iocshArgDouble };
static const iocshArg *testEchoArgs[] = {
    &testEchoArg0,&testEchoArg1,&testEchoArg2,&testEchoArg3,&testEchoArg4};
static const iocshFuncDef testEchoFuncDef = {
    "testEcho", 5, testEchoArgs};
static void testEchoCallFunc(const iocshArgBuf *args)
{
    testEcho(args[0].sval,args[1].ival,args[2].sval,args[3].ival,args[4].dval);
}

static void testEchoRegister(void)
{
    static int firstTime = 1;
    if (firstTime) {
        firstTime = 0;
        iocshRegister(&testEchoFuncDef, testEchoCallFunc);
    }
}
epicsExportRegistrar(testEchoRegister);