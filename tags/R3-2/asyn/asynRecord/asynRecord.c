/* asynRecord.c - Record Support Routines for asyn record */
/*
 * Based on earlier asynRecord.c and serialRecord.c.  
 * See documentation for differences between those records and this one.
 *
 *      Author:    Mark Rivers
 *      Date:      3/8/2004
 *
 */


#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <epicsMutex.h>
#include <callback.h>
#include <cantProceed.h>
#include <dbScan.h>
#include <alarm.h>
#include <dbDefs.h>
#include <dbEvent.h>
#include <dbAccess.h>
#include <dbFldTypes.h>
#include <devSup.h>
#include <drvSup.h>
#include <errMdef.h>
#include <recSup.h>
#include <recGbl.h>
#include <special.h>
#include <epicsString.h>
#include <epicsStdio.h>
#include <epicsExport.h>
#include <asynGpibDriver.h>
#include <asynDriver.h>
#include <drvAsynIPPort.h>
#define GEN_SIZE_OFFSET
#include "asynRecord.h"
#undef GEN_SIZE_OFFSET

/* These should be in a header file*/
#define NUM_BAUD_CHOICES 12
static char *baud_choices[NUM_BAUD_CHOICES]={"Unknown",
                                             "300","600","1200","2400","4800",
                                             "9600","19200","38400","57600",
                                             "115200","230400"};
#define NUM_PARITY_CHOICES 4
static char *parity_choices[NUM_PARITY_CHOICES]={"Unknown","none","even","odd"};
#define NUM_DBIT_CHOICES 5
static char *data_bit_choices[NUM_DBIT_CHOICES]={"Unknown","5","6","7","8"};
#define NUM_SBIT_CHOICES 3
static char *stop_bit_choices[NUM_SBIT_CHOICES]={"Unknown","1","2"};
#define NUM_FLOW_CHOICES 3
static char *flow_control_choices[NUM_FLOW_CHOICES]={"Unknown","Y","N"};

#define OPT_SIZE 80  /* Size of buffer for setting and getting port options */
#define EOS_SIZE 10  /* Size of buffer for EOS */
#define ERR_SIZE 100 /* Size of buffer for error message */
#define QUEUE_TIMEOUT 5.0 /* Timeout for queueRequest */


/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(asynRecord *pasynRec, int pass);
static long process(asynRecord *pasynRec);
static long special(struct dbAddr *paddr, int after);
#define get_value NULL
static long cvt_dbaddr(struct dbAddr *paddr);
static long get_array_info(struct dbAddr *paddr, long *no_elements, 
                           long *offset);
static long put_array_info(struct dbAddr *paddr, long nNew);
#define get_units NULL
static long get_precision(struct dbAddr *paddr, long *precision);
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double NULL

static void monitor(asynRecord *pasynRec);
static void monitorStatus(asynRecord *pasynRec);
static asynStatus connectDevice(asynRecord *pasynRec);
static void GPIB_command(asynUser *pasynUser);

static void asynCallback(asynUser *pasynUser);
static void queueTimeoutCallback(asynUser *pasynUser);
static void exceptCallback(asynUser *pasynUser, asynException exception);
static void performIO(asynUser *pasynUser);
static void setOption(asynUser *pasynUser);
static void getOptions(asynUser *pasynUser);
static void reportError(asynRecord *pasynRec,asynStatus astatus,
    int rstatus, int severity, const char *pformat, ...);
static void resetError(asynRecord *pasynRec);

struct rset asynRSET={
   RSETNUMBER,
   report,
   initialize,
   init_record,
   process,
   special,
   get_value,
   cvt_dbaddr,
   get_array_info,
   put_array_info,
   get_units,
   get_precision,
   get_enum_str,
   get_enum_strs,
   put_enum_str,
   get_graphic_double,
   get_control_double,
   get_alarm_double };
epicsExportAddress(rset, asynRSET);


typedef struct oldValues {  /* Used in monitor() and monitorStatus() */
    epicsInt32      addr;   /*asyn address*/
    epicsInt32      nowt;   /*Number of bytes to write*/
    epicsInt32      nawt;   /*Number of bytes actually written*/
    epicsInt32      nrrd;   /*Number of bytes to read*/
    epicsInt32      nord;   /*Number of bytes read*/
    epicsEnum16     baud;   /*Baud rate*/
    epicsEnum16     prty;   /*Parity*/
    epicsEnum16     dbit;   /*Data bits*/
    epicsEnum16     sbit;   /*Stop bits*/
    epicsEnum16     fctl;   /*Flow control*/
    epicsEnum16     ucmd;   /*Universal command*/
    epicsEnum16     acmd;   /*Addressed command*/
    unsigned char   spr;    /*Serial poll response*/
    epicsInt32      tmsk;   /*Trace mask*/
    epicsEnum16     tb0;    /*Trace error*/
    epicsEnum16     tb1;    /*Trace IO device*/
    epicsEnum16     tb2;    /*Trace IO filter*/
    epicsEnum16     tb3;    /*Trace IO driver*/
    epicsEnum16     tb4;    /*Trace flow*/
    epicsInt32      tiom;   /*Trace I/O mask*/
    epicsEnum16     tib0;   /*Trace IO ASCII*/
    epicsEnum16     tib1;   /*Trace IO escape*/
    epicsEnum16     tib2;   /*Trace IO hex*/
    epicsInt32      tsiz;   /*Trace IO truncate size*/
    char            tfil[40]; /*Trace IO file*/
    FILE*           traceFd; /*Trace file descriptor*/
    epicsEnum16     auct;   /*Autoconnect*/
    epicsEnum16     cnct;   /*Connect/Disconnect*/
    epicsEnum16     enbl;   /*Enable/Disable*/
    char            errs[ERR_SIZE+1]; /*Error string*/
} oldValues;

#define REMEMBER_STATE(FIELD) pasynRecPvt->old.FIELD = pasynRec->FIELD
#define POST_IF_NEW(FIELD) \
    if (pasynRec->FIELD != pasynRecPvt->old.FIELD) { \
        if(interruptAccept) \
           db_post_events(pasynRec, &pasynRec->FIELD, monitor_mask); \
        pasynRecPvt->old.FIELD = pasynRec->FIELD; }

typedef enum {
    stateIdle, stateInitialIO, stateCancelIO, stateConnect, stateIO,
    stateGetOption,stateSetOption
} callbackState;

typedef struct asynRecPvt {
    CALLBACK        callback;
    asynRecord      *prec;  /* Pointer to record */
    callbackState   state; 
    int             fieldIndex; /* For special */
    asynUser        *pasynUser;
    asynOctet       *pasynOctet;
    void            *asynOctetPvt;
    asynCommon      *pasynCommon;
    void            *asynCommonPvt;
    asynGpib        *pasynGpib;
    void *          asynGpibPvt;
    char            *outbuff;
    oldValues       old;
} asynRecPvt;

static long init_record(asynRecord *pasynRec, int pass)
{
    asynRecPvt *pasynRecPvt;
    asynUser   *pasynUser;
    asynStatus status;

    if (pass != 0) return(0);

    /* Allocate and initialize private structure used by this record */
    pasynRecPvt = (asynRecPvt*) callocMustSucceed(
        1, sizeof(asynRecPvt),"asynRecord");
    pasynRec->dpvt = pasynRecPvt;

    pasynRecPvt->prec = pasynRec;

    /* Allocate the space for the binary/hybrid output and binary/hybrid 
     * input arrays */
    if (pasynRec->omax <= 0) pasynRec->omax=MAX_STRING_SIZE;
    if (pasynRec->imax <= 0) pasynRec->imax=MAX_STRING_SIZE;
    pasynRec->optr = (char *)callocMustSucceed(
        pasynRec->omax, sizeof(char),"asynRecord");
    pasynRec->iptr = (char *)callocMustSucceed(
        pasynRec->imax, sizeof(char),"asynRecord");
    pasynRecPvt->outbuff =  (char *)callocMustSucceed(
        pasynRec->omax, sizeof(char),"asynRecord");
    pasynRec->errs = (char *)callocMustSucceed(
        ERR_SIZE+1,sizeof(char),"asynRecord");
    /* Initialize asyn, connect to device */
    pasynUser = pasynManager->createAsynUser(asynCallback,queueTimeoutCallback);
    pasynRecPvt->pasynUser = pasynUser;
    pasynUser->userPvt = pasynRecPvt;
    if (strlen(pasynRec->port) != 0) {
        status = connectDevice(pasynRec);
        if(status==asynSuccess) {
           pasynRecPvt->state =
               (pasynRec->pini) ? stateInitialIO : stateGetOption;
           pasynRec->pini = 1;
        }
    }

    /* Get initial values of trace and connect bits */
    monitorStatus(pasynRec);
    strcpy(pasynRec->tfil, "Unknown");
    pasynRec->udf = 0;
    return(0);
}

static long process(asynRecord *pasynRec)
{
   asynRecPvt* pasynRecPvt = pasynRec->dpvt;

   /* This routine is called under the following possible states:
    * state                 pact
    * stateInitialIO        0     Queue request, callback will do getOptions
    *                             and performIO
    * stateIdle             0     Queue request, callback will performIO
    * stateSetOption        0     Queue request, callback will setOption
    * stateGetOption        0     Queue request, callback will getOption
    * stateConnect          0     Queue request with asynQueuePriorityConnect,
    *                             callback will connect or disconnect
    * stateCancelIO         1     Connect/disconnect request when there is
    *                             already a queueRequest.  Set pact=0 and
    *                             process record again.
    * Any                   1     Callback, finish record processing */

   if(!pasynRec->pact) {
      if(pasynRecPvt->state==stateIdle) pasynRecPvt->state = stateIO;
      /* Need to store state of fields that could have been changed from
       * outside since the last time the values were posted by monitor()
       * and which process() or routines it calls can also change */
      REMEMBER_STATE(ucmd);
      REMEMBER_STATE(acmd);
      REMEMBER_STATE(nowt);
      /* Make sure nrrd and nowt are valid */
      if (pasynRec->nrrd > pasynRec->imax) pasynRec->nrrd = pasynRec->imax;
      if (pasynRec->nowt > pasynRec->omax) pasynRec->nowt = pasynRec->omax;
      resetError(pasynRec);
      pasynManager->queueRequest(pasynRecPvt->pasynUser, 
         ((pasynRecPvt->state==stateConnect)
         ? asynQueuePriorityConnect : asynQueuePriorityLow), QUEUE_TIMEOUT);
      pasynRec->pact = TRUE;
   } else {
      /* PACT was true, finish record processing */
      switch (pasynRecPvt->state) {
         case stateCancelIO:
            /* special has called cancelRequest and 
             * callbackRequestProcessCallback
             * Just process the record again, leave state=stateConnect */
            pasynRecPvt->state = stateConnect;
            scanOnce(pasynRec);
            break;
         case stateIO:
         case stateInitialIO:
            /* We only do the following if we are completing record processing
             * for an I/O operation. 
             * - The record processing time records the time that the last 
             *   I/O operation completed
             * - Call monitor() which resets alarms and posts I/O related fields
             * - Process the records forward link.  This must only be done
             *   on I/O completion, because it is typically a record used to 
             *   parse the input received */
            recGblGetTimeStamp(pasynRec);
            monitor(pasynRec);
            recGblFwdLink(pasynRec);
            /* No break, do this for all states except stateConnect */
         default:
            pasynRecPvt->state = stateIdle;
      }
      pasynRec->pact=FALSE;
   }
   return(0);
}

/* special() is called when many fields are changed */

static long special(struct dbAddr *paddr, int after)
{
    asynRecord *pasynRec=(asynRecord *)paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);
    asynRecPvt *pasynRecPvt=pasynRec->dpvt;
    asynUser *pasynUser=pasynRecPvt->pasynUser;
    asynStatus status=asynSuccess;
    unsigned short monitor_mask;
    int traceMask;
    FILE *fd;

    if (!after) return(0);
    /* The first set of fields can be handled even if state != stateIdle */
    resetError(pasynRec);
    switch (fieldIndex) {
       case asynRecordTMSK: 
          pasynTrace->setTraceMask(pasynUser, pasynRec->tmsk);
          goto done;
       case asynRecordTB0: 
       case asynRecordTB1: 
       case asynRecordTB2: 
       case asynRecordTB3: 
       case asynRecordTB4:
          traceMask = (pasynRec->tb0 ? ASYN_TRACE_ERROR    : 0) |
                      (pasynRec->tb1 ? ASYN_TRACEIO_DEVICE : 0) |
                      (pasynRec->tb2 ? ASYN_TRACEIO_FILTER : 0) |
                      (pasynRec->tb3 ? ASYN_TRACEIO_DRIVER : 0) |
                      (pasynRec->tb4 ? ASYN_TRACE_FLOW     : 0);
          pasynTrace->setTraceMask(pasynUser, traceMask);
          goto done;
       case asynRecordTIOM: 
          pasynTrace->setTraceIOMask(pasynUser, pasynRec->tiom);
          goto done;
       case asynRecordTIB0: 
       case asynRecordTIB1: 
       case asynRecordTIB2: 
          traceMask = (pasynRec->tib0 ? ASYN_TRACEIO_ASCII  : 0) |
                      (pasynRec->tib1 ? ASYN_TRACEIO_ESCAPE : 0) |
                      (pasynRec->tib2 ? ASYN_TRACEIO_HEX    : 0);
          pasynTrace->setTraceIOMask(pasynUser, traceMask);
          goto done;
       case asynRecordTSIZ:
          pasynTrace->setTraceIOTruncateSize(pasynUser, pasynRec->tsiz);
          goto done;
       case asynRecordTFIL:
          if (strlen(pasynRec->tfil) == 0) {
             /* Zero length, use stdout */
             fd = stdout;
          } else {
             fd = fopen(pasynRec->tfil, "a+");
          }
          if (fd) {
             /* Set old value to this, so monitorStatus won't think another
              * thread changed it */
             /* Must do this before calling setTraceFile, because it 
              * generates an exeception, which calls monitorStatus() */
             pasynRecPvt->old.traceFd = fd;
             status = pasynTrace->setTraceFile(pasynUser, fd);
             if (status != asynSuccess) {
                reportError(pasynRec,status, NO_ALARM, NO_ALARM,
                            "Error setting trace file: %s",
                            pasynUser->errorMessage);
             }
          } else {    
             reportError(pasynRec,status, NO_ALARM, NO_ALARM,
                         "Error opening trace file: %s",
                         pasynRec->tfil);
          }
          goto done;
       case asynRecordAUCT:
          pasynManager->autoConnect(pasynUser, pasynRec->auct);
          goto done;
       case asynRecordENBL:
          pasynManager->enable(pasynUser, pasynRec->enbl);
          goto done;
    }
    if(fieldIndex==asynRecordCNCT) {
       if(pasynRec->pact) {
          int wasQueued;
          asynPrint(pasynUser,ASYN_TRACE_FLOW,
                    "%s:special connect/disconnect calling cancelRequest\n",
                    pasynRec->name);
          status = pasynManager->cancelRequest(pasynUser,&wasQueued);
          if(status!=asynSuccess || !wasQueued) {
             reportError(pasynRec,status, COMM_ALARM, MAJOR_ALARM,
                         "Cancel request failed: %s",
                         pasynUser->errorMessage);
             return 0;
          }
          pasynRecPvt->state = stateCancelIO;
          callbackRequestProcessCallback(&pasynRecPvt->callback,
                                         pasynRec->prio,pasynRec);
          goto done;
       }
       asynPrint(pasynUser,ASYN_TRACE_FLOW,
                 "%s:special connect/disconnect\n", pasynRec->name);
       pasynRecPvt->state = stateConnect;
       scanOnce(pasynRec);
       goto done;
    }
    /* These cases require idle state */
    if(pasynRecPvt->state!=stateIdle) {
       reportError(pasynRec,asynSuccess, NO_ALARM, NO_ALARM,
                   "state!=stateIdle, state=%d, try again later",
                   pasynRecPvt->state);
       goto done;
    } 
    switch (fieldIndex) {
       case asynRecordSOCK:
          strcpy(pasynRec->port, pasynRec->sock);
          pasynRec->addr = 0;
          status = drvAsynIPPortConfigure(pasynRec->port, pasynRec->sock, 
                                             0, 0, 0);
          /* We don't go to done on status!=asynSuccess here, because 
           * the error could be user just re-entering an existing port */
          monitor_mask = recGblResetAlarms(pasynRec) | DBE_VALUE | DBE_LOG;
          db_post_events(pasynRec, pasynRec->port, monitor_mask);
          db_post_events(pasynRec, &pasynRec->addr, monitor_mask);
          /* Now do same thing as when port or addr change */
       case asynRecordPORT: 
       case asynRecordADDR: 
          /* If the PORT or ADDR fields changed then reconnect to new device */
          status = connectDevice(pasynRec);
          asynPrint(pasynUser,ASYN_TRACE_FLOW,
             "%s: special() port=%s, addr=%d, connect status=%d\n",
             pasynRec->name, pasynRec->port, pasynRec->addr, status);
          if(status==asynSuccess) {
             pasynRecPvt->state = stateGetOption;
             scanOnce(pasynRec);
          }
          break;
       case asynRecordBAUD: 
       case asynRecordPRTY: 
       case asynRecordDBIT: 
       case asynRecordSBIT: 
       case asynRecordFCTL: 
          /* One of the serial port parameters */
          pasynRecPvt->fieldIndex = fieldIndex;
          pasynRecPvt->state = stateSetOption;
          asynPrint(pasynUser,ASYN_TRACE_FLOW,
              "%s: special() port=%s, addr=%d scanOnce request\n",
              pasynRec->name, pasynRec->port, pasynRec->addr);
          scanOnce(pasynRec);
          break;
    }
done:
    return(status);
}

static void asynCallback(asynUser *pasynUser)
{
   asynRecPvt *pasynRecPvt=pasynUser->userPvt;
   asynRecord *pasynRec = pasynRecPvt->prec;
   asynStatus status;

   asynPrint(pasynUser,ASYN_TRACE_FLOW,
        "%s: asynCallback, state=%d\n",
        pasynRec->name, pasynRecPvt->state);
   switch(pasynRecPvt->state) {
      case stateInitialIO:
         getOptions(pasynUser);
         /* no break */
      case stateIO:
         performIO(pasynUser);
         break;
      case stateSetOption:
         setOption(pasynUser);
         /* no break - every time an option is set call getOptions to verify */
      case stateGetOption:
         getOptions(pasynUser);
         break;
      case stateConnect: {
              int isConnected;

              status = pasynManager->isConnected(pasynUser,&isConnected);
              if(pasynRec->cnct) {
                  if(!isConnected) {
                      pasynRecPvt->pasynCommon->connect(
                          pasynRecPvt->asynCommonPvt,pasynUser);
                  } else {
                      monitorStatus(pasynRec);
                  }
              } else {
                  if(isConnected) {
                      pasynRecPvt->pasynCommon->disconnect(
                           pasynRecPvt->asynCommonPvt,pasynUser);
                  } else {
                      monitorStatus(pasynRec);
                  }
              }
          }
          break;
      default:
          reportError(pasynRec,asynError, NO_ALARM, NO_ALARM,
                      "asynCallback illegal state %d\n",
                      pasynRecPvt->state);
          return;
   }
   dbScanLock( (dbCommon*) pasynRec);
   process(pasynRec);
   dbScanUnlock( (dbCommon*) pasynRec);
}

static void exceptCallback(asynUser *pasynUser,asynException exception)
{
   asynRecPvt *pasynRecPvt=pasynUser->userPvt;
   asynRecord *pasynRec = pasynRecPvt->prec;
   int        callLock = interruptAccept;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,
        "%s: exception %d\n",
        pasynRec->name,(int)exception);
    if(callLock) dbScanLock( (dbCommon*) pasynRec);
    /* There has been a change in connect or enable status */
    monitorStatus(pasynRec);
    if(callLock) dbScanUnlock( (dbCommon*) pasynRec);
}

static void queueTimeoutCallback(asynUser *pasynUser)
{
   asynRecPvt *pasynRecPvt=pasynUser->userPvt;
   asynRecord *pasynRec = pasynRecPvt->prec;

   reportError(pasynRec,asynError, COMM_ALARM, MAJOR_ALARM, 
               "queueRequest timeout");
   dbScanLock( (dbCommon*) pasynRec);
   process(pasynRec);
   monitor(pasynRec); /* We call monitor so alarms get posted */
   dbScanUnlock( (dbCommon*) pasynRec);
}

static long cvt_dbaddr(struct dbAddr *paddr)
{
   asynRecord *pasynRec=(asynRecord *)paddr->precord;
   int fieldIndex = dbGetFieldIndex(paddr);

   if (fieldIndex == asynRecordBOUT) {
      paddr->pfield = (void *)(pasynRec->optr);
      paddr->no_elements = pasynRec->omax;
      paddr->field_type = DBF_CHAR;
      paddr->field_size = sizeof(char);
      paddr->dbr_field_type = DBF_CHAR;
   } else if (fieldIndex == asynRecordBINP) {
      paddr->pfield = (unsigned char *)(pasynRec->iptr);
      paddr->no_elements = pasynRec->imax;
      paddr->field_type = DBF_CHAR;
      paddr->field_size = sizeof(char);
      paddr->dbr_field_type = DBF_CHAR;
   } else if (fieldIndex == asynRecordERRS) {
      paddr->pfield = pasynRec->errs;
      paddr->no_elements = ERR_SIZE;
      paddr->field_type = DBF_CHAR;
      paddr->field_size = sizeof(char);
      paddr->dbr_field_type = DBR_CHAR;
      paddr->special = SPC_NOMOD;
   }
   return(0);
}

static long get_array_info(struct dbAddr *paddr, long *no_elements, 
                           long *offset)
{
   asynRecord *pasynRec=(asynRecord *)paddr->precord;
   int fieldIndex = dbGetFieldIndex(paddr);

   if (fieldIndex == asynRecordBOUT) {
      *no_elements =  pasynRec->nowt;
      *offset = 0;
   } else if (fieldIndex == asynRecordBINP) {
      *no_elements =  pasynRec->nord;
      *offset = 0;
   } else if (fieldIndex == asynRecordERRS) {
      *no_elements =  ERR_SIZE;
      *offset = 0;
   }
   return(0);
}

static long put_array_info(struct dbAddr *paddr, long nNew)
{
   asynRecord *pasynRec=(asynRecord *)paddr->precord;
   int fieldIndex = dbGetFieldIndex(paddr);

   if (fieldIndex == asynRecordBOUT) {
      pasynRec->nowt = nNew;
   } else if (fieldIndex == asynRecordBINP) {
      pasynRec->nord = nNew;
   }
   return(0);
}

static long get_precision(struct dbAddr *paddr, long *precision)
{
   int fieldIndex = dbGetFieldIndex(paddr);

   *precision = 0;
   if (fieldIndex == asynRecordTMOT) {
      *precision = 4;
      return(0);
   }
   recGblGetPrec(paddr, precision);
   return(0);
}


static void monitor(asynRecord *pasynRec)
{
    /* Called when record processes for I/O operation */
    unsigned short  monitor_mask;
    asynRecPvt* pasynRecPvt = pasynRec->dpvt;

    monitor_mask = recGblResetAlarms(pasynRec) | DBE_VALUE | DBE_LOG;
   
    if ((pasynRec->tmod == asynTMOD_Read) ||
        (pasynRec->tmod == asynTMOD_Write_Read)) {
       if (pasynRec->ifmt == asynFMT_ASCII)
          db_post_events(pasynRec,pasynRec->ainp, monitor_mask);
       else
          db_post_events(pasynRec, pasynRec->iptr, monitor_mask);
       db_post_events(pasynRec,pasynRec->tinp, monitor_mask);
    }
    POST_IF_NEW(nrrd);
    POST_IF_NEW(nord);
    POST_IF_NEW(nowt);
    POST_IF_NEW(nawt);
    POST_IF_NEW(spr);
    POST_IF_NEW(ucmd);
    POST_IF_NEW(acmd);
}


static void monitorStatus(asynRecord *pasynRec)
{
    /* Called to update trace and connect fields. */
    unsigned short  monitor_mask;
    asynRecPvt* pasynRecPvt = pasynRec->dpvt;
    asynUser *pasynUser = pasynRecPvt->pasynUser;
    int traceMask;
    asynStatus status;
    FILE *traceFd;
    int yesNo;

    /* For fields that could have been changed externally we need to 
     * remember their current value */
    REMEMBER_STATE(tmsk);
    REMEMBER_STATE(tb0);
    REMEMBER_STATE(tb1);
    REMEMBER_STATE(tb2);
    REMEMBER_STATE(tb3);
    REMEMBER_STATE(tb4);
    REMEMBER_STATE(tiom);
    REMEMBER_STATE(tib0);
    REMEMBER_STATE(tib1);
    REMEMBER_STATE(tib2);
    REMEMBER_STATE(tsiz);
    REMEMBER_STATE(auct);
    REMEMBER_STATE(cnct);
    REMEMBER_STATE(enbl);

    monitor_mask = DBE_VALUE | DBE_LOG;
    
    traceMask = pasynTrace->getTraceMask(pasynUser);
    pasynRec->tmsk = traceMask;
    pasynRec->tb0 = (traceMask & ASYN_TRACE_ERROR)    ? 1 : 0;
    pasynRec->tb1 = (traceMask & ASYN_TRACEIO_DEVICE) ? 1 : 0;
    pasynRec->tb2 = (traceMask & ASYN_TRACEIO_FILTER) ? 1 : 0;
    pasynRec->tb3 = (traceMask & ASYN_TRACEIO_DRIVER) ? 1 : 0;
    pasynRec->tb4 = (traceMask & ASYN_TRACE_FLOW)     ? 1 : 0;
    traceMask = pasynTrace->getTraceIOMask(pasynUser);
    pasynRec->tiom = traceMask;
    pasynRec->tib0 = (traceMask & ASYN_TRACEIO_ASCII)  ? 1 : 0;
    pasynRec->tib1 = (traceMask & ASYN_TRACEIO_ESCAPE) ? 1 : 0;
    pasynRec->tib2 = (traceMask & ASYN_TRACEIO_HEX)    ? 1 : 0;

    status = pasynManager->isAutoConnect(pasynUser,&yesNo);
    if (status == asynSuccess) pasynRec->auct = yesNo;
    else pasynRec->auct = 0;
    status = pasynManager->isConnected(pasynUser,&yesNo);
    if (status == asynSuccess) pasynRec->cnct = yesNo;
    else pasynRec->cnct = 0;
    status = pasynManager->isEnabled(pasynUser,&yesNo);
    if (status == asynSuccess) pasynRec->enbl = yesNo;
    else pasynRec->enbl = 0;
    pasynRec->tsiz = pasynTrace->getTraceIOTruncateSize(pasynUser);
    traceFd        = pasynTrace->getTraceFile(pasynUser);
    POST_IF_NEW(tmsk);
    POST_IF_NEW(tb0);
    POST_IF_NEW(tb1);
    POST_IF_NEW(tb2);
    POST_IF_NEW(tb3);
    POST_IF_NEW(tb4);
    POST_IF_NEW(tiom);
    POST_IF_NEW(tib0);
    POST_IF_NEW(tib1);
    POST_IF_NEW(tib2);
    POST_IF_NEW(tsiz);
    if (traceFd != pasynRecPvt->old.traceFd) {
       pasynRecPvt->old.traceFd = traceFd;
       /* Some other thread changed the trace file, we can't know file name */
       strcpy(pasynRec->tfil, "Unknown");
       db_post_events(pasynRec, pasynRec->tfil, monitor_mask);
    }
    POST_IF_NEW(auct);
    POST_IF_NEW(cnct);
    POST_IF_NEW(enbl);
}

static asynStatus connectDevice(asynRecord *pasynRec)
{
    asynInterface *pasynInterface;
    asynRecPvt *pasynRecPvt=pasynRec->dpvt;
    asynUser *pasynUser = pasynRecPvt->pasynUser;
    asynStatus status;

    resetError(pasynRec);
    /* Disconnect any connected device.  Ignore error if there
     * is no device currently connected.
     */
    pasynManager->exceptionCallbackRemove(pasynUser);
    pasynManager->disconnect(pasynUser);

    /* Connect to the new device */
    status = pasynManager->connectDevice(pasynUser,pasynRec->port,
                                         pasynRec->addr);
    if (status!=asynSuccess) {
        reportError(pasynRec,status, COMM_ALARM, MAJOR_ALARM, 
                    "Connect error, status=%d, %s",
                    status, pasynUser->errorMessage);
        return(status);
    }

    /* Get asynCommon interface */
    pasynInterface = pasynManager->findInterface(pasynUser,asynCommonType,1);
    if(!pasynInterface) {
        pasynRecPvt->pasynCommon = 0; pasynRecPvt->asynCommonPvt = 0;
        reportError(pasynRec,status, COMM_ALARM, MAJOR_ALARM, 
                    "findInterface common: %s",
                    pasynUser->errorMessage);
        return(asynError);
    }
    pasynRecPvt->pasynCommon = (asynCommon *)pasynInterface->pinterface;
    pasynRecPvt->asynCommonPvt = pasynInterface->drvPvt;

    /* Get asynOctet interface */
    pasynInterface = pasynManager->findInterface(pasynUser,asynOctetType,1);
    if(!pasynInterface) {
        pasynRecPvt->pasynOctet = 0; pasynRecPvt->asynOctetPvt = 0;
        reportError(pasynRec,status, COMM_ALARM, MAJOR_ALARM,
                    "findInterface octet: %s",
                    pasynUser->errorMessage);
        return(asynError);
    }
    pasynRecPvt->pasynOctet = (asynOctet *)pasynInterface->pinterface;
    pasynRecPvt->asynOctetPvt = pasynInterface->drvPvt;

    /* Get asynGpib interface if it exists*/
    pasynInterface = pasynManager->findInterface(pasynUser,asynGpibType,1);
    if(pasynInterface) {
        /* This device has an asynGpib interface, not serial or socket */
        pasynRecPvt->pasynGpib = (asynGpib *)pasynInterface->pinterface;
        pasynRecPvt->asynGpibPvt = pasynInterface->drvPvt;
    } else {
        pasynRecPvt->pasynGpib = 0; pasynRecPvt->asynGpibPvt = 0;
    }

    /* Add exception callback */
    pasynManager->exceptionCallbackAdd(pasynUser, exceptCallback);

    /* Get the trace and connect flags */
    monitorStatus(pasynRec);

    return(asynSuccess);
}

static void performIO(asynUser *pasynUser)
{
   asynRecPvt *pasynRecPvt=pasynUser->userPvt;
   asynRecord *pasynRec = pasynRecPvt->prec;
   asynStatus status = asynSuccess;
   int        nbytesTransfered;
   char    *inptr;
   char    *outptr;
   int     inlen;
   int     nread;
   int     nwrite;
   int     eoslen;
   char eos[EOS_SIZE];

   resetError(pasynRec);
   pasynUser->timeout = pasynRec->tmot;

   /* See if the record is processing because a GPIB Universal or Addressed 
    * Command is being done */
   if ((pasynRec->ucmd != gpibUCMD_None) || (pasynRec->acmd != gpibACMD_None)) {
      if (pasynRecPvt->pasynGpib == NULL) {
         reportError(pasynRec,status, COMM_ALARM, MAJOR_ALARM,
                     "GPIB op but no GPIB interface, port=%s",
                     pasynRec->port);
      } else {
         GPIB_command(pasynUser);
      }
      pasynRec->acmd = gpibACMD_None;  /* Reset to no Addressed Command */
      pasynRec->ucmd = gpibUCMD_None;  /* Reset to no Universal Command */
      return;
   }

   /* Nothing to do if NoI/O mode */
   if (pasynRec->tmod == asynTMOD_NoIO) return;

   if (pasynRec->ofmt == asynFMT_ASCII) {
      /* ASCII output mode */
      /* Translate escape sequences */
      nwrite = dbTranslateEscape(pasynRecPvt->outbuff, pasynRec->aout);
      outptr = pasynRecPvt->outbuff;
   } else if (pasynRec->ofmt == asynFMT_Hybrid) {
      /* Hybrid output mode */
      /* Translate escape sequences */
      nwrite = dbTranslateEscape(pasynRecPvt->outbuff, pasynRec->optr);
      outptr = pasynRecPvt->outbuff;
   } else {   
      /* Binary output mode */
      nwrite = pasynRec->nowt;
      outptr = pasynRec->optr;
   }
   /* If not binary mode, append the terminator */
   if (pasynRec->ofmt != asynFMT_Binary) {
      eoslen = dbTranslateEscape(eos, pasynRec->oeos);
      /* Make sure there is room for terminator */
      if ((nwrite + eoslen) < pasynRec->omax) {
         strncat(outptr, eos, eoslen);
         nwrite += eoslen;
      }
   }

   if (pasynRec->ifmt == asynFMT_ASCII) {
      /* ASCII input mode */
      inptr = pasynRec->ainp;
      inlen = sizeof(pasynRec->ainp);
   } else {
      /* Binary or Hybrid input mode */
      inptr = pasynRec->iptr;
      inlen = pasynRec->imax;
   }
   if (pasynRec->nrrd != 0)
      nread = pasynRec->nrrd;
   else
      nread = inlen;

   if ((pasynRec->tmod == asynTMOD_Flush) ||
       (pasynRec->tmod == asynTMOD_Write_Read)) {
      /* Flush the input buffer */
      pasynRecPvt->pasynOctet->flush(pasynRecPvt->asynOctetPvt, pasynUser);
      asynPrint(pasynUser,ASYN_TRACE_FLOW,"%s flush\n",pasynRec->name);
   }

   if ((pasynRec->tmod == asynTMOD_Write) ||
       (pasynRec->tmod == asynTMOD_Write_Read)) {
      /* Send the message */ 
      nbytesTransfered = 0;
      status = pasynRecPvt->pasynOctet->write(pasynRecPvt->asynOctetPvt, 
                   pasynUser, outptr, nwrite,&nbytesTransfered);
      pasynRec->nawt = nbytesTransfered;
      asynPrintIO(pasynUser, ASYN_TRACEIO_DEVICE, outptr, nbytesTransfered,
            "%s: nwrite=%d, status=%d, nawt=%d, data=", pasynRec->name, nwrite, 
            status, nbytesTransfered);
      if (status!=asynSuccess || nbytesTransfered != nwrite) {
         /* Something is wrong if we couldn't write everything */
         reportError(pasynRec,status, WRITE_ALARM, MAJOR_ALARM,
                     "Write error, nout=%d, %s", 
                     nbytesTransfered, pasynUser->errorMessage);
      }
   }
    
   if ((pasynRec->tmod == asynTMOD_Read) ||
       (pasynRec->tmod == asynTMOD_Write_Read)) {
      /* Set the input buffer to all zeros */
      memset(inptr, 0, inlen);
      /* Read the message  */
      if (pasynRec->ifmt == asynFMT_Binary) {
         eos[0] = '\0';
         eoslen = 0;
      } else {
         /* ASCII or Hybrid mode */
         eoslen = dbTranslateEscape(eos, pasynRec->ieos);
      }
      status = pasynRecPvt->pasynOctet->setEos(pasynRecPvt->asynOctetPvt, 
                                        pasynUser, eos, eoslen);
      if (status)  {
         reportError(pasynRec,status, WRITE_ALARM, MINOR_ALARM,
                     "Error setting EOS, %s", 
                     pasynUser->errorMessage);
      }
      nbytesTransfered = 0;
      status = pasynRecPvt->pasynOctet->read(pasynRecPvt->asynOctetPvt, 
                   pasynUser, inptr, nread,&nbytesTransfered);

      asynPrintIO(pasynUser, ASYN_TRACEIO_DEVICE, inptr, nbytesTransfered,
            "%s: inlen=%d, status=%d, ninp=%d, data=", pasynRec->name, inlen, 
            status, nbytesTransfered);
      inlen = nbytesTransfered;
      if (status!=asynSuccess) {
         reportError(pasynRec,status, READ_ALARM, MINOR_ALARM,
                        "Timeout nread %d %s", 
                        nbytesTransfered, pasynUser->errorMessage);
      }
      /* Check for input buffer overflow */
      if ((pasynRec->ifmt == asynFMT_ASCII) && 
          (nbytesTransfered >= (int)sizeof(pasynRec->ainp))) {
         reportError(pasynRec,status, READ_ALARM, MINOR_ALARM,
                     "Overflow nread %d %s", 
                     nbytesTransfered, pasynUser->errorMessage);
         /* terminate response with \0 */
         inptr[sizeof(pasynRec->ainp)-1] = '\0';
      }
      else if ((pasynRec->ifmt == asynFMT_Hybrid) && 
               (nbytesTransfered >= pasynRec->imax)) {
         reportError(pasynRec,status, READ_ALARM, MINOR_ALARM,
                     "Overflow nread %d %s", 
                     nbytesTransfered, pasynUser->errorMessage);
         /* terminate response with \0 */
         inptr[pasynRec->imax-1] = '\0';
      }
      else if ((pasynRec->ifmt == asynFMT_Binary) && 
               (nbytesTransfered > pasynRec->imax)) {
         /* This should not be able to happen */
         reportError(pasynRec,status, READ_ALARM, MAJOR_ALARM,
                     "Overflow nread %d %s", 
                     nbytesTransfered, pasynUser->errorMessage);
      }
      else if (pasynRec->ifmt != asynFMT_Binary) {
         /* Not binary and no input buffer overflow has occurred */
         /* Add null at end of input.  This is safe because of tests above */
         inptr[nbytesTransfered] = '\0';
         /* If the string is terminated by the requested terminator */
         /* remove it. */
         if ((eoslen > 0) && (nbytesTransfered >= eoslen) && 
             (strcmp(&inptr[nbytesTransfered-eoslen], eos) == 0)) {
            memset(&inptr[nbytesTransfered-eoslen], 0, eoslen);
            inlen -= eoslen;
         }
      } 
      pasynRec->nord = nbytesTransfered; /* Number of bytes read */
      /* Copy to tinp with dbTranslateEscape */
      epicsStrSnPrintEscaped(pasynRec->tinp, sizeof(pasynRec->tinp), 
                             inptr, inlen);
   }
}

static void GPIB_command(asynUser *pasynUser)
{
   /* This function handles GPIB-specific commands */
   asynRecPvt  *pasynRecPvt=pasynUser->userPvt;
   asynRecord  *pasynRec = pasynRecPvt->prec;
   asynGpib    *pasynGpib = pasynRecPvt->pasynGpib;
   void        *asynGpibPvt = pasynRecPvt->asynGpibPvt;
   asynStatus  status;
   int         nbytesTransfered;
   char        cmd_char=0;
   char        acmd[6];

   /* See asynRecord.dbd for definitions of constants gpibXXXX_Abcd */
   if (pasynRec->ucmd != gpibUCMD_None)
   {
      switch (pasynRec->ucmd)
      {
        case gpibUCMD_Device_Clear__DCL_:
            cmd_char = IBDCL;
            break;
        case gpibUCMD_Local_Lockout__LL0_:
            cmd_char = IBLLO;
            break;
        case gpibUCMD_Serial_Poll_Disable__SPD_:
            cmd_char = IBSPD;
            break;
        case gpibUCMD_Serial_Poll_Enable__SPE_:
            cmd_char = IBSPE;
            break;
        case gpibUCMD_Unlisten__UNL_:
            cmd_char = IBUNL;
            break;
        case gpibUCMD_Untalk__UNT_:
            cmd_char = IBUNT;
            break;
      }
      status = pasynGpib->universalCmd(asynGpibPvt, pasynUser, cmd_char);
      if (status) {
         /* Something is wrong if we couldn't write */
         reportError(pasynRec,status, WRITE_ALARM, MAJOR_ALARM,
                     "GPIB Universal command %s",
                     pasynUser->errorMessage);
      }
   }

   /* See if an Addressed Command is to be done */
   if (pasynRec->acmd != gpibACMD_None)
   {
      int lenCmd = 6;
      acmd[0] = IBUNT; /* Untalk */
      acmd[1] = IBUNL; /* Unlisten */
      acmd[2] = pasynRec->addr + LADBASE;  /* GPIB address + Listen Base */
      acmd[4] = IBUNT; /* Untalk */
      acmd[5] = IBUNL; /* Unlisten */
      switch (pasynRec->acmd)
      {
        case gpibACMD_Group_Execute_Trig___GET_:
            acmd[3] = IBGET[0];
            break;
        case gpibACMD_Go_To_Local__GTL_:
            acmd[3] = IBGTL[0];
            break;
        case gpibACMD_Selected_Dev__Clear__SDC_:
            acmd[3] = IBSDC[0];
            break;
        case gpibACMD_Take_Control__TCT_:
            /* This command requires Talker Base */
            acmd[2] = pasynRec->addr + TADBASE;
            acmd[3] = IBTCT[0];
            lenCmd = 4;
            break;
        case gpibACMD_Serial_Poll:
            /* Serial poll. Requires 3 operations */
            /* Serial Poll Enable */
            cmd_char = IBSPE;
            status = pasynGpib->universalCmd(asynGpibPvt, pasynUser, cmd_char);
            if (status!=asynSuccess) {
                /* Something is wrong if we couldn't write */
                reportError(pasynRec,status, WRITE_ALARM, MAJOR_ALARM,
                            "Error in GPIB Serial Poll write, %s",
                            pasynUser->errorMessage);
            }
            /* Read the response byte  */
            status = pasynRecPvt->pasynOctet->read(pasynRecPvt->asynGpibPvt,
                  pasynRecPvt->pasynUser, (char *)&pasynRec->spr,
                  1,&nbytesTransfered);
            if (status!=asynSuccess || nbytesTransfered!=1) {
                /* Something is wrong if we couldn't read */
                reportError(pasynRec,status, READ_ALARM, MAJOR_ALARM,
                            "Error in GPIB Serial Poll read, %s",
                            pasynUser->errorMessage);
            }
            /* Serial Poll Disable */
            cmd_char = IBSPD;
            status = pasynGpib->universalCmd(asynGpibPvt, pasynUser, cmd_char);
            if (status!=asynSuccess) {
                /* Something is wrong if we couldn't write */
                reportError(pasynRec,status, WRITE_ALARM, MAJOR_ALARM,
                            "Error in GPIB Serial Poll disable write, %s",
                            pasynUser->errorMessage);
            }
            return;
      }
      status = pasynRecPvt->pasynGpib->addressedCmd(pasynRecPvt->asynGpibPvt,
                      pasynRecPvt->pasynUser, acmd, lenCmd);
      if (status) {
         /* Something is wrong if we couldn't write */
         reportError(pasynRec,status, WRITE_ALARM, MAJOR_ALARM,
                     "Error in GPIB Addressed Command write, %s",
                     pasynUser->errorMessage);
      }
   }
}

static void setOption(asynUser *pasynUser)
{
   asynRecPvt *pasynRecPvt = (asynRecPvt *) pasynUser->userPvt;
   asynRecord *pasynRec = pasynRecPvt->prec;
   asynStatus status=asynSuccess;

   asynPrint(pasynUser,ASYN_TRACE_FLOW,
             "%s: setOptionCallback port=%s, addr=%d index=%d\n",
             pasynRec->name, pasynRec->port, pasynRec->addr, 
             pasynRecPvt->fieldIndex);

   switch (pasynRecPvt->fieldIndex) {
   
   case asynRecordBAUD:
      status = pasynRecPvt->pasynCommon->setOption(pasynRecPvt->asynCommonPvt, 
                                          pasynRecPvt->pasynUser, "baud", 
                                          baud_choices[pasynRec->baud]);
      break;
   case asynRecordPRTY:
      status = pasynRecPvt->pasynCommon->setOption(pasynRecPvt->asynCommonPvt, 
                                          pasynRecPvt->pasynUser, "parity", 
                                          parity_choices[pasynRec->prty]);
      break;
   case asynRecordSBIT:
      status = pasynRecPvt->pasynCommon->setOption(pasynRecPvt->asynCommonPvt, 
                                          pasynRecPvt->pasynUser, "stop", 
                                          stop_bit_choices[pasynRec->sbit]);
      break;
   case asynRecordDBIT:
      status = pasynRecPvt->pasynCommon->setOption(pasynRecPvt->asynCommonPvt, 
                                          pasynRecPvt->pasynUser, "bits", 
                                          data_bit_choices[pasynRec->dbit]);
      break;
   case asynRecordFCTL:
      status = pasynRecPvt->pasynCommon->setOption(pasynRecPvt->asynCommonPvt, 
                                          pasynRecPvt->pasynUser, "clocal", 
                                          flow_control_choices[pasynRec->fctl]);
      break;
   }
   if (status != asynSuccess) {
      reportError(pasynRec,status, NO_ALARM, NO_ALARM, 
         "Error setting option, %s", pasynUser->errorMessage);
   }
}

static void getOptions(asynUser *pasynUser)
{
    asynRecPvt *pasynRecPvt = (asynRecPvt *) pasynUser->userPvt;
    asynRecord *pasynRec = pasynRecPvt->prec;
    char optbuff[OPT_SIZE];
    int i;
    unsigned short monitor_mask;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,
           "%s: getOptionCallback() port=%s, addr=%d\n",
           pasynRec->name, pasynRec->port, pasynRec->addr);

    /* For fields that could have been changed externally we need to
     * remember their current value */
    REMEMBER_STATE(baud);
    REMEMBER_STATE(prty);
    REMEMBER_STATE(sbit);
    REMEMBER_STATE(dbit);
    REMEMBER_STATE(fctl);

    /* Get port options */
    pasynRecPvt->pasynCommon->getOption(pasynRecPvt->asynCommonPvt, pasynUser, 
                                     "baud", optbuff, OPT_SIZE);
    pasynRec->baud = 0;
    for (i=0; i<NUM_BAUD_CHOICES; i++)
       if (strcmp(optbuff, baud_choices[i]) == 0) pasynRec->baud = i;

    pasynRecPvt->pasynCommon->getOption(pasynRecPvt->asynCommonPvt, pasynUser, 
                                     "parity", optbuff, OPT_SIZE);
    pasynRec->prty = 0;
    for (i=0; i<NUM_PARITY_CHOICES; i++)
       if (strcmp(optbuff, parity_choices[i]) == 0) pasynRec->prty = i;

    pasynRecPvt->pasynCommon->getOption(pasynRecPvt->asynCommonPvt, pasynUser, 
                                     "stop", optbuff, OPT_SIZE);
    pasynRec->sbit = 0;
    for (i=0; i<NUM_SBIT_CHOICES; i++)
       if (strcmp(optbuff, stop_bit_choices[i]) == 0) pasynRec->sbit = i;

    pasynRecPvt->pasynCommon->getOption(pasynRecPvt->asynCommonPvt, pasynUser, 
                                     "bits", optbuff, OPT_SIZE);
    pasynRec->dbit = 0;
    for (i=0; i<NUM_DBIT_CHOICES; i++)
       if (strcmp(optbuff, data_bit_choices[i]) == 0) pasynRec->dbit = i;

    pasynRecPvt->pasynCommon->getOption(pasynRecPvt->asynCommonPvt, pasynUser, 
                                     "clocal", optbuff, OPT_SIZE);
    pasynRec->fctl = 0;
    for (i=0; i<NUM_FLOW_CHOICES; i++)
       if (strcmp(optbuff, flow_control_choices[i]) == 0) pasynRec->fctl = i;

    monitor_mask = DBE_VALUE | DBE_LOG;
    
    POST_IF_NEW(baud);
    POST_IF_NEW(prty);
    POST_IF_NEW(sbit);
    POST_IF_NEW(dbit);
    POST_IF_NEW(fctl);
}

static void reportError(asynRecord *pasynRec,asynStatus astatus,
    int rstatus, int severity, const char *pformat, ...)
{
   asynRecPvt* pasynRecPvt = pasynRec->dpvt;
   asynUser *pasynUser = pasynRecPvt->pasynUser;
   unsigned short monitor_mask;
   char buffer[ERR_SIZE+1];
   va_list  pvar;

   if (rstatus != NO_ALARM) recGblSetSevr(pasynRec, rstatus, severity);

   va_start(pvar,pformat);
   epicsVsnprintf(buffer, ERR_SIZE, pformat, pvar);
   va_end(pvar);
   if(astatus!=asynSuccess && astatus!=asynTimeout) {
      asynPrint(pasynUser, ASYN_TRACE_ERROR, "%s: %s\n", 
                      pasynRec->name, buffer);
   }
   strncpy(pasynRec->errs, buffer, ERR_SIZE);
   if (strncmp(pasynRec->errs, pasynRecPvt->old.errs, ERR_SIZE) != 0) {
       strncpy(pasynRecPvt->old.errs, pasynRec->errs, ERR_SIZE);
       monitor_mask = DBE_VALUE | DBE_LOG;
       db_post_events(pasynRec, pasynRec->errs, monitor_mask);
   }
}

static void resetError(asynRecord *pasynRec)
{
   asynRecPvt* pasynRecPvt = pasynRec->dpvt;
   unsigned short monitor_mask;

   pasynRec->errs[0] = 0;
   if (strncmp(pasynRec->errs, pasynRecPvt->old.errs,ERR_SIZE) != 0) {
       strncpy(pasynRecPvt->old.errs, pasynRec->errs,ERR_SIZE);
       monitor_mask = DBE_VALUE | DBE_LOG;
       db_post_events(pasynRec, pasynRec->errs, monitor_mask);
   }
}