< envPaths

dbLoadDatabase("../../dbd/testGpibSerial.dbd")
testGpibSerial_registerRecordDeviceDriver(pdbbase)

dbLoadRecords("../../db/testGpib.db","name=gpibTest,L=0,A=0")
dbLoadRecords("../../db/asynRecord.db","P=asyn,R=Test,PORT=L0,ADDR=3,OMAX=0,IMAX=0")

#The following command is for a serial line terminal concentrator
#drvAsynIPPortConfigure("L0","164.54.9.90:4004",0,0)

#The following commands are for a local serial line
#drvAsynSerialPortConfigure("L0","/dev/cua/a",0,0,0)
drvAsynSerialPortConfigure("L0","/dev/ttyS0",0,0,0)
asynSetOption("L0", -1, "baud", "38400")
asynSetOption("L0", -1, "bits", "8")
asynSetOption("L0", -1, "parity", "none")
asynSetOption("L0", -1, "stop", "1")
asynSetOption("L0", -1, "clocal", "Y")
asynSetOption("L0", -1, "crtscts", "N")

asynSetTraceFile("L0",-1,"")
asynSetTraceMask("L0",-1,0x09)
asynSetTraceIOMask("L0",-1,0x2)

iocInit()