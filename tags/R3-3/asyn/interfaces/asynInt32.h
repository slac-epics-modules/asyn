/*  asynInt32.h */
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

#ifndef asynInt32H
#define asynInt32H

#include <asynDriver.h>
#include <epicsTypes.h>

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define asynInt32Type "asynInt32"
typedef struct asynInt32 {
    asynStatus (*write)(void *drvPvt, asynUser *pasynUser, epicsInt32 value);
    asynStatus (*read)(void *drvPvt, asynUser *pasynUser, epicsInt32 *value);
    asynStatus (*getBounds)(void *drvPvt, asynUser *pasynUser, 
                            epicsInt32 *low, epicsInt32 *high);
} asynInt32;

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* asynInt32H */
