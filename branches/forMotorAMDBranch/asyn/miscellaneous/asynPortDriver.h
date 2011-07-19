#ifndef asynPortDriver_H
#define asynPortDriver_H

#include <epicsTypes.h>
#include <epicsMutex.h>

#include <asynStandardInterfaces.h>
#include <asynParamTypes.h>

epicsShareFunc void* findAsynPortDriver(const char *portName);

#ifdef __cplusplus

/** Masks for each of the asyn standard interfaces */
#define asynCommonMask          0x00000001
#define asynDrvUserMask         0x00000002
#define asynOptionMask          0x00000004
#define asynInt32Mask           0x00000008
#define asynUInt32DigitalMask   0x00000010
#define asynFloat64Mask         0x00000020
#define asynOctetMask           0x00000040
#define asynInt8ArrayMask       0x00000080
#define asynInt16ArrayMask      0x00000100
#define asynInt32ArrayMask      0x00000200
#define asynFloat32ArrayMask    0x00000400
#define asynFloat64ArrayMask    0x00000800
#define asynGenericPointerMask  0x00001000

//Forward Declaration
class ParamVal;

/** Class to support parameter library (also called parameter list); 
 * set and get values indexed by parameter number (pasynUser->reason)
 * and do asyn callbacks when parameters change.
 * The parameter class supports 3 types of parameters: int, double
 * and dynamic-length strings. */
class paramList
{
public:
  paramList(int nVals, asynStandardInterfaces *pasynInterfaces);
  ~paramList();
  asynStatus createParam(const char *name, asynParamType type, int *index);
  void findParam(const char *name, int *index);
  asynStatus getName(int index, const char **name);

  asynStatus callCallbacks(int addr);
  asynStatus callCallbacks();
  void report(FILE *fp, int details);
  bool isIndexValid(int &index);
  ParamVal* getParam(int &index);
  asynStandardInterfaces* standardInterfaces();

  void setFlag(int index);
  asynStatus float64Callback(int command, int addr, epicsFloat64 value);
  asynStatus int32Callback(int command, int addr, epicsInt32 value);
  asynStatus octetCallback(int command, int addr, char *value);
  asynStatus uint32Callback(int command, int addr, epicsUInt32 value,
      epicsUInt32 interruptMask);

private:
  int nextParam;
  int nVals;
  int nFlags;
  asynStandardInterfaces *pasynInterfaces;
  int *flags;
  ParamVal **vals;
};


/** Base class for asyn port drivers; handles most of the bookkeeping for writing an asyn port driver
 * with standard asyn interfaces and a parameter library. */
class asynPortDriver
{
public:
    asynPortDriver(const char *portName, int maxAddr, int paramTableSize, int interfaceMask, int interruptMask, int asynFlags, int autoConnect, int priority, int stackSize);
    asynPortDriver(const char *portName, int maxAddr, int interfaceMask, int interruptMask, int asynFlags, int autoConnect, int priority, int stackSize);
    virtual ~asynPortDriver();
    virtual asynStatus lock();
    virtual asynStatus unlock();
    virtual asynStatus getAddress(asynUser *pasynUser, int *address);
    virtual asynStatus readInt32(asynUser *pasynUser, epicsInt32 *value);
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus readUInt32Digital(asynUser *pasynUser, epicsUInt32 *value, epicsUInt32 mask);
    virtual asynStatus writeUInt32Digital(asynUser *pasynUser, epicsUInt32 value, epicsUInt32 mask);
    virtual asynStatus setInterruptUInt32Digital(asynUser *pasynUser, epicsUInt32 mask, interruptReason reason);
    virtual asynStatus clearInterruptUInt32Digital(asynUser *pasynUser, epicsUInt32 mask);
    virtual asynStatus getInterruptUInt32Digital(asynUser *pasynUser, epicsUInt32 *mask, interruptReason reason);
    virtual asynStatus getBounds(asynUser *pasynUser, epicsInt32 *low, epicsInt32 *high);
    virtual asynStatus readFloat64(asynUser *pasynUser, epicsFloat64 *value);
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    virtual asynStatus readOctet(asynUser *pasynUser, char *value, size_t maxChars, size_t *nActual, int *eomReason);
    virtual asynStatus writeOctet(asynUser *pasynUser, const char *value, size_t maxChars, size_t *nActual);
    virtual asynStatus readInt8Array(asynUser *pasynUser, epicsInt8 *value, size_t nElements, size_t *nIn);
    virtual asynStatus writeInt8Array(asynUser *pasynUser, epicsInt8 *value, size_t nElements);
    virtual asynStatus doCallbacksInt8Array(epicsInt8 *value, size_t nElements, int reason, int addr);
    virtual asynStatus readInt16Array(asynUser *pasynUser, epicsInt16 *value, size_t nElements, size_t *nIn);
    virtual asynStatus writeInt16Array(asynUser *pasynUser, epicsInt16 *value, size_t nElements);
    virtual asynStatus doCallbacksInt16Array(epicsInt16 *value, size_t nElements, int reason, int addr);
    virtual asynStatus readInt32Array(asynUser *pasynUser, epicsInt32 *value, size_t nElements, size_t *nIn);
    virtual asynStatus writeInt32Array(asynUser *pasynUser, epicsInt32 *value, size_t nElements);
    virtual asynStatus doCallbacksInt32Array(epicsInt32 *value, size_t nElements, int reason, int addr);
    virtual asynStatus readFloat32Array(asynUser *pasynUser, epicsFloat32 *value, size_t nElements, size_t *nIn);
    virtual asynStatus writeFloat32Array(asynUser *pasynUser, epicsFloat32 *value, size_t nElements);
    virtual asynStatus doCallbacksFloat32Array(epicsFloat32 *value, size_t nElements, int reason, int addr);
    virtual asynStatus readFloat64Array(asynUser *pasynUser, epicsFloat64 *value, size_t nElements, size_t *nIn);
    virtual asynStatus writeFloat64Array(asynUser *pasynUser, epicsFloat64 *value, size_t nElements);
    virtual asynStatus doCallbacksFloat64Array(epicsFloat64 *value, size_t nElements, int reason, int addr);
    virtual asynStatus readGenericPointer(asynUser *pasynUser, void *pointer);
    virtual asynStatus writeGenericPointer(asynUser *pasynUser, void *pointer);
    virtual asynStatus doCallbacksGenericPointer(void *pointer, int reason, int addr);
    virtual asynStatus drvUserCreate(asynUser *pasynUser, const char *drvInfo, const char **pptypeName, size_t *psize);
    virtual asynStatus drvUserGetType(asynUser *pasynUser, const char **pptypeName, size_t *psize);
    virtual asynStatus drvUserDestroy(asynUser *pasynUser);
    virtual void report(FILE *fp, int details);
    virtual asynStatus connect(asynUser *pasynUser);
    virtual asynStatus disconnect(asynUser *pasynUser);
    virtual asynStatus createParam(const char *name, asynParamType type, int *index);
    virtual asynStatus createParam(int list, const char *name, asynParamType type, int *index);
    virtual asynStatus findParam(const char *name, int *index);
    virtual asynStatus findParam(int list, const char *name, int *index);
    virtual asynStatus getParamName(int index, const char **name);
    virtual asynStatus getParamName(int list, int index, const char **name);
    virtual void reportSetParamErrors(asynStatus status, int index, int list, const char *functionName);
    virtual void reportGetParamErrors(asynStatus status, int index, int list, const char *functionName);
    virtual asynStatus setIntegerParam(int index, int value);
    virtual asynStatus setIntegerParam(int list, int index, int value);
    virtual asynStatus setUIntDigitalParam(int index, epicsUInt32 value, epicsUInt32 mask);
    virtual asynStatus setUIntDigitalParam(int list, int index, epicsUInt32 value, epicsUInt32 mask);
    virtual asynStatus setUInt32DigitalInterrupt(int index, epicsUInt32 mask, interruptReason reason);
    virtual asynStatus setUInt32DigitalInterrupt(int list, int index, epicsUInt32 mask, interruptReason reason);
    virtual asynStatus clearUInt32DigitalInterrupt(int index, epicsUInt32 mask);
    virtual asynStatus clearUInt32DigitalInterrupt(int list, int index, epicsUInt32 mask);
    virtual asynStatus getUInt32DigitalInterrupt(int index, epicsUInt32 *mask, interruptReason reason);
    virtual asynStatus getUInt32DigitalInterrupt(int list, int index, epicsUInt32 *mask, interruptReason reason);
    virtual asynStatus setDoubleParam(int index, double value);
    virtual asynStatus setDoubleParam(int list, int index, double value);
    virtual asynStatus setStringParam(int index, const char *value);
    virtual asynStatus setStringParam(int list, int index, const char *value);
    virtual asynStatus getIntegerParam(int index, int *value);
    virtual asynStatus getIntegerParam(int list, int index, int *value);
    virtual asynStatus getUIntDigitalParam(int index, epicsUInt32 *value, epicsUInt32 mask);
    virtual asynStatus getUIntDigitalParam(int list, int index, epicsUInt32 *value, epicsUInt32 mask);
    virtual asynStatus getDoubleParam(int index, double *value);
    virtual asynStatus getDoubleParam(int list, int index, double *value);
    virtual asynStatus getStringParam(int index, int maxChars, char *value);
    virtual asynStatus getStringParam(int list, int index, int maxChars, char *value);
    virtual asynStatus callParamCallbacks();
    virtual asynStatus callParamCallbacks(int addr);
    virtual asynStatus callParamCallbacks(int list, int addr);
    virtual void reportParams(FILE *fp, int details);
    virtual int getNumParams();
    asynStatus initializePortDriver();
    char *portName;
    int maxAddr;
    void callbackTask();
    bool isAddrInvalid(int & addr);
protected:
    virtual asynStatus createDriverParams();
    virtual asynStatus preInitDriver();
    virtual asynStatus postInitDriver();
    asynUser *pasynUserSelf;
    asynStandardInterfaces asynStdInterfaces;
private:
  asynStatus allocateParamList(int paramTableSize);
  paramList **params;
  epicsMutexId mutexId;

};

#endif /* cplusplus */

#endif
