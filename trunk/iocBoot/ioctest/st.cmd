dbLoadDatabase("../../dbd/test.dbd")
test_registerRecordDeviceDriver(pdbbase)

#drvAsynIPPortConfigure("A","164.54.9.90:4001",0,0,0)
echoDriverInit("A",0.05,0,0)
interposeInterfaceInit("interpose","A",0)
echoDriverInit("B",0.05,0,1)

dbLoadRecords("../../db/asynRecord.db","P=asynTest,R=:PA:A0,PORT=A,ADDR=0,OMAX=0,IMAX=0")
dbLoadRecords("../../db/test.db","name=client1,port=A,addr=0")
dbLoadRecords("../../db/test.db","name=client2,port=A,addr=0")

dbLoadRecords("../../db/asynRecord.db","P=asynTest,R=:PB:A-1,PORT=B,ADDR=-1,OMAX=0,IMAX=0")
dbLoadRecords("../../db/asynRecord.db","P=asynTest,R=:PB:A0,PORT=B,ADDR=0,OMAX=0,IMAX=0")
dbLoadRecords("../../db/test.db","name=client1,port=B,addr=0")
dbLoadRecords("../../db/test.db","name=client2,port=B,addr=0")
dbLoadRecords("../../db/asynRecord.db","P=asynTest,R=:PB:A1,PORT=B,ADDR=1,OMAX=0,IMAX=0")
dbLoadRecords("../../db/test.db","name=client1,port=B,addr=1")
dbLoadRecords("../../db/test.db","name=client2,port=B,addr=1")
dbLoadRecords("../../db/asynRecord.db","P=asyn,R=Test,PORT=A,ADDR=0,OMAX=0,IMAX=0")
iocInit()
