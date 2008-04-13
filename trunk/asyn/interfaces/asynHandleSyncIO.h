/*  asynHandelSyncIO.h */
/***********************************************************************
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory, and the Regents of the University of
* California, as Operator of Los Alamos National Laboratory, and
* Berliner Elektronenspeicherring-Gesellschaft m.b.H. (BESSY).
* asynDriver is distributed subject to a Software License Agreement
* found in file LICENSE that is included with this distribution.
***********************************************************************/

/*  28-June-2004 Mark Rivers
*/

#ifndef asynHandleSyncIOH
#define asynHandleSyncIOH

#include <asynDriver.h>
#include <epicsTypes.h>
#include <shareLib.h>

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define asynHandleSyncIOType "asynHandleSyncIO"
typedef struct asynHandleSyncIO {
    asynStatus (*connect)(const char *port, int addr, 
                       asynUser **ppasynUser, const char *drvInfo);
    asynStatus (*disconnect)(asynUser *pasynUser);
    asynStatus (*write)(asynUser *pasynUser,void *pvalue,double timeout);
    asynStatus (*read)(asynUser *pasynUser,void *pvalue,double timeout);
    asynStatus (*writeOnce)(const char *port, int addr,
                       void *pvalue,double timeout,const char *drvInfo);
    asynStatus (*readOnce)(const char *port, int addr,
                       void *pvalue,double timeout,const char *drvInfo);
} asynHandleSyncIO;
epicsShareExtern asynHandleSyncIO *pasynHandleSyncIO;

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* asynHandleSyncIOH */
