/*asynCommonSyncIO.c*/
/***********************************************************************
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory, and the Regents of the University of
* California, as Operator of Los Alamos National Laboratory, and
* Berliner Elektronenspeicherring-Gesellschaft m.b.H. (BESSY).
* asynDriver is distributed subject to a Software License Agreement
* found in file LICENSE that is included with this distribution.
***********************************************************************/
/*
 * This package provide a simple, synchronous interface to asynCommon
 * Author:  Marty Kraimer
 * Created: 12OCT2004
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cantProceed.h>

#define epicsExportSharedSymbols
#include <shareLib.h>
#include "asynDriver.h"
#include "asynDrvUser.h"
#include "asynCommonSyncIO.h"

typedef struct ioPvt{
   asynCommon   *pasynCommon;
   void         *pcommonPvt;
   asynDrvUser  *pasynDrvUser;
   void         *drvUserPvt;
}ioPvt;

/*asynCommonSyncIO methods*/
static asynStatus connect(const char *port, int addr,
                          asynUser **ppasynUser, const char *drvInfo);
static asynStatus disconnect(asynUser *pasynUser);
static asynStatus connectDevice(asynUser *pasynUser);
static asynStatus disconnectDevice(asynUser *pasynUser);
static asynStatus report(asynUser *pasynUser, FILE *fp, int details);
static asynCommonSyncIO interface = {
    connect,
    disconnect,
    connectDevice,
    disconnectDevice,
    report
};
epicsShareDef asynCommonSyncIO *pasynCommonSyncIO = &interface;

static asynStatus connect(const char *port, int addr,
   asynUser **ppasynUser, const char *drvInfo)
{
    ioPvt         *pioPvt;
    asynUser      *pasynUser;
    asynStatus    status;
    asynInterface *pasynInterface;

    pioPvt = (ioPvt *)callocMustSucceed(1, sizeof(ioPvt),"asynCommonSyncIO");
    pasynUser = pasynManager->createAsynUser(0,0);
    pasynUser->userPvt = pioPvt;
    *ppasynUser = pasynUser;
    status = pasynManager->connectDevice(pasynUser, port, addr);    
    if (status != asynSuccess) {
      printf("Can't connect to port %s address %d %s\n",
          port, addr,pasynUser->errorMessage);
      pasynManager->freeAsynUser(pasynUser);
      free(pioPvt);
      return status ;
    }
    pasynInterface = pasynManager->findInterface(pasynUser, asynCommonType, 1);
    if (!pasynInterface) {
       printf("%s interface not supported\n", asynCommonType);
       goto cleanup;
    }
    pioPvt->pasynCommon = (asynCommon *)pasynInterface->pinterface;
    pioPvt->pcommonPvt = pasynInterface->drvPvt;
    if(drvInfo) {
        /* Check for asynDrvUser interface */
        pasynInterface = pasynManager->findInterface(pasynUser,asynDrvUserType,1);
        if(pasynInterface) {
            asynDrvUser *pasynDrvUser;
            void       *drvPvt;
            pasynDrvUser = (asynDrvUser *)pasynInterface->pinterface;
            drvPvt = pasynInterface->drvPvt;
            status = pasynDrvUser->create(drvPvt,pasynUser,drvInfo,0,0);
            if(status==asynSuccess) {
                pioPvt->pasynDrvUser = pasynDrvUser;
                pioPvt->drvUserPvt = drvPvt;
            } else {
                printf("asynCommonSyncIO::connect drvUserCreate drvInfo=%s %s\n",
                         drvInfo, pasynUser->errorMessage);
            }
        }
    }
    return asynSuccess ;
cleanup:
    disconnect(pasynUser);
    return asynError;
}

static asynStatus disconnect(asynUser *pasynUser)
{
    ioPvt      *pioPvt = (ioPvt *)pasynUser->userPvt;
    asynStatus status;

    if(pioPvt->pasynDrvUser) {
        status = pioPvt->pasynDrvUser->destroy(pioPvt->drvUserPvt,pasynUser);
        if(status!=asynSuccess) {
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                "asynCommonSyncIO pasynDrvUser->destroy failed %s\n",
                pasynUser->errorMessage);
            return status;
        }
    }
    status = pasynManager->disconnect(pasynUser);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
            "asynCommonSyncIO disconnect failed %s\n",pasynUser->errorMessage);
        return status;
    }
    status = pasynManager->freeAsynUser(pasynUser);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
            "asynCommonSyncIO freeAsynUser failed %s\n",
            pasynUser->errorMessage);
        return status;
    }
    free(pioPvt);
    return asynSuccess;
}
 

static asynStatus connectDevice(asynUser *pasynUser)
{
    asynStatus status;
    ioPvt      *pioPvt = (ioPvt *)pasynUser->userPvt;

    status = pasynManager->lockPort(pasynUser);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
            "asynCommonSyncIO lockPort failed %s\n",pasynUser->errorMessage);
        return status;
    }
    status = pioPvt->pasynCommon->connect(pioPvt->pcommonPvt, pasynUser);
    if (status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                 "asynCommonSyncIO connect failed %s\n",pasynUser->errorMessage);
    }
    if((pasynManager->unlockPort(pasynUser)) ) {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,
            "unlockPort error %s\n", pasynUser->errorMessage);
    }
    return(status);
}

static asynStatus disconnectDevice(asynUser *pasynUser)
{
    asynStatus status;
    ioPvt      *pioPvt = (ioPvt *)pasynUser->userPvt;

    status = pasynManager->lockPort(pasynUser);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
            "asynCommonSyncIO lockPort failed %s\n",pasynUser->errorMessage);
        return status;
    }
    status = pioPvt->pasynCommon->disconnect(pioPvt->pcommonPvt, pasynUser);
    if (status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,   
                 "asynCommonSyncIO disconnect failed %s\n",pasynUser->errorMessage);
    }
    if((pasynManager->unlockPort(pasynUser)) ) {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,
            "unlockPort error %s\n", pasynUser->errorMessage);
    }
    return(status);
}

static asynStatus report(asynUser *pasynUser, FILE *fp, int details)
{
    ioPvt      *pioPvt = (ioPvt *)pasynUser->userPvt;

    pioPvt->pasynCommon->report(pioPvt->pcommonPvt, fp, details);
    return(asynSuccess);
}
