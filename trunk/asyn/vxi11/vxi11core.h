/******************************************************************************
 *
 * $RCSfile: vxi11core.h,v $ 
 *
 ******************************************************************************/

/***********************************************************************
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory, and the Regents of the University of
* California, as Operator of Los Alamos National Laboratory, and
* Berliner Elektronenspeicherring-Gesellschaft m.b.H. (BESSY).
* asynDriver is distributed subject to a Software License Agreement
* found in file LICENSE that is included with this distribution.
***********************************************************************/

/*
 *Types and constants for RPC protocol (core channel)
 *
 * Author: Benjamin Franksen
 *
 ******************************************************************************
 *
 * Notes:
 *
 *	This file was generated by RPCGEN from vxi/RPCL/vxi11core.rpcl but 
 *	modified manually.
 *
 */
#ifndef vxi11coreH
#define vxi11coreH
#include "osiRpc.h"

typedef long Device_Link;
bool_t xdr_Device_Link(XDR *, void *,...);

enum Device_AddrFamily {
	DEVICE_TCP = 0,
	DEVICE_UDP = 1
};
typedef enum Device_AddrFamily Device_AddrFamily;
bool_t xdr_Device_AddrFamily(XDR *, void *,...);

typedef long Device_Flags;
bool_t xdr_Device_Flags(XDR *, void *,...);

typedef long Device_ErrorCode;
bool_t xdr_Device_ErrorCode(XDR *, void *,...);

struct Device_Error {
	Device_ErrorCode error;
};
typedef struct Device_Error Device_Error;
bool_t xdr_Device_Error(XDR *, void *,...);

struct Create_LinkParms {
	long clientId;
	bool_t lockDevice;
	u_long lock_timeout;
	char *device;
};
typedef struct Create_LinkParms Create_LinkParms;
bool_t xdr_Create_LinkParms(XDR *, void *,...);

struct Create_LinkResp {
	Device_ErrorCode error;
	Device_Link lid;
	u_short abortPort;
	u_long maxRecvSize;
};
typedef struct Create_LinkResp Create_LinkResp;
bool_t xdr_Create_LinkResp(XDR *, void *,...);

struct Device_WriteParms {
	Device_Link lid;
	u_long io_timeout;
	u_long lock_timeout;
	Device_Flags flags;
	struct {
		u_int data_len;
		char *data_val;
	} data;
};
typedef struct Device_WriteParms Device_WriteParms;
bool_t xdr_Device_WriteParms(XDR *, void *,...);

struct Device_WriteResp {
	Device_ErrorCode error;
	u_long size;
};
typedef struct Device_WriteResp Device_WriteResp;
bool_t xdr_Device_WriteResp(XDR *, void *,...);

struct Device_ReadParms {
	Device_Link lid;
	u_long requestSize;
	u_long io_timeout;
	u_long lock_timeout;
	Device_Flags flags;
	char termChar;
};
typedef struct Device_ReadParms Device_ReadParms;
bool_t xdr_Device_ReadParms(XDR *, void *,...);

struct Device_ReadResp {
	Device_ErrorCode error;
	long reason;
	struct {
		u_int data_len;
		char *data_val;
	} data;
};
typedef struct Device_ReadResp Device_ReadResp;
bool_t xdr_Device_ReadResp(XDR *, void *,...);

struct Device_ReadStbResp {
	Device_ErrorCode error;
	u_char stb;
};
typedef struct Device_ReadStbResp Device_ReadStbResp;
bool_t xdr_Device_ReadStbResp(XDR *, void *,...);

struct Device_GenericParms {
	Device_Link lid;
	Device_Flags flags;
	u_long lock_timeout;
	u_long io_timeout;
};
typedef struct Device_GenericParms Device_GenericParms;
bool_t xdr_Device_GenericParms(XDR *, void *,...);

struct Device_RemoteFunc {
	u_long hostAddr;
	u_long hostPort;
	u_long progNum;
	u_long progVers;
	Device_AddrFamily progFamily;
};
typedef struct Device_RemoteFunc Device_RemoteFunc;
bool_t xdr_Device_RemoteFunc(XDR *, void *,...);

struct Device_EnableSrqParms {
	Device_Link lid;
	bool_t enable;
	struct {
		u_int handle_len;
		char *handle_val;
	} handle;
};
typedef struct Device_EnableSrqParms Device_EnableSrqParms;
bool_t xdr_Device_EnableSrqParms(XDR *, void *,...);

struct Device_LockParms {
	Device_Link lid;
	Device_Flags flags;
	u_long lock_timeout;
};
typedef struct Device_LockParms Device_LockParms;
bool_t xdr_Device_LockParms(XDR *, void *,...);

struct Device_DocmdParms {
	Device_Link lid;
	Device_Flags flags;
	u_long io_timeout;
	u_long lock_timeout;
	long cmd;
	bool_t network_order;
	long datasize;
	struct {
		u_int data_in_len;
		char *data_in_val;
	} data_in;
};
typedef struct Device_DocmdParms Device_DocmdParms;
bool_t xdr_Device_DocmdParms(XDR *, void *,...);

struct Device_DocmdResp {
	Device_ErrorCode error;
	struct {
		u_int data_out_len;
		char *data_out_val;
	} data_out;
};
typedef struct Device_DocmdResp Device_DocmdResp;
bool_t xdr_Device_DocmdResp(XDR *, void *,...);

#define DEVICE_ASYNC ((u_long)0x0607B0)
#define DEVICE_ASYNC_VERSION ((u_long)1)
#define device_abort ((u_long)1)
extern Device_Error *device_abort_1();

#define DEVICE_CORE ((u_long)0x0607AF)
#define DEVICE_CORE_VERSION ((u_long)1)
#define create_link ((u_long)10)
#define device_write ((u_long)11)
#define device_read ((u_long)12)
#define device_readstb ((u_long)13)
#define device_trigger ((u_long)14)
#define device_clear ((u_long)15)
#define device_remote ((u_long)16)
#define device_local ((u_long)17)
#define device_lock ((u_long)18)
#define device_unlock ((u_long)19)
#define device_enable_srq ((u_long)20)
#define device_docmd ((u_long)22)
#define destroy_link ((u_long)23)
#define create_intr_chan ((u_long)25)
#define destroy_intr_chan ((u_long)26)

/* 
 * $Log: not supported by cvs2svn $
 * Revision 1.2  2003/10/27 13:41:12  mrk
 * interim commit before alp[ha2
 *
 * Revision 1.1  2003/10/01 19:30:03  mrk
 * asynGpib and vxi11 now supported
 *
 * Revision 1.3  2003/04/10 15:17:07  mrk
 * open source license
 *
 * Revision 1.2  2003/03/14 16:48:08  mrk
 * Changes for March 2003 version including name changes
 *
 * Revision 1.1  2003/02/20 20:42:22  mrk
 * drvHpE2050=>drvVxi11
 *
 * Revision 1.3  2002/10/21 16:15:55  mrk
 * fix  so it does not generate warnings
 *
 * Revision 1.2  2002/10/21 15:59:48  mrk
 * build a single library
 *
 * Revision 1.1  2002/10/18 17:21:13  mrk
 * moved to here
 *
 * Revision 1.2  2001/02/22 19:57:20  norume
 * Many, many R3.14 changes.  Driver tested on RTEMS-gen68360 and Linux.
 *
 * Revision 1.1.1.1  2001/02/15 14:52:51  mrk
 * Import sources
 *
 * Revision 1.1.1.1  2000/03/21 18:06:35  franksen
 * unbundled gpib first version
 */
#endif /*vxi11coreH*/
