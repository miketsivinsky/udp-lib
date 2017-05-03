@echo off

rem ******  test config
set use_udp_lib=1
set useTx=1
set useRx=0

rem ****** IP list
set LocalHost=127.0.0.1

set MikeMainIP=192.168.0.85
set MikeIntelIP=192.168.10.2
set MikeTendaIP=192.168.10.3

set CUDA_MainIP=192.168.0.54
set CUDA_TestIP=192.168.10.1

set PeriskopNB_IP=192.168.10.2

rem ****** common part
set PacketSize=1472
set PacketInBuf=64
set PacketGenType=1

rem ****** tx part
set TxBufNum=1010000
set TxHostIP=%CUDA_TestIP%
set TxDelay=0
set TxPeer=%PeriskopNB_IP%
set TxPort=50000

rem ****** rx part
set RxBufNum=1000000
set RxHostIP=%CUDA_TestIP%
set RxPort=50000

rem ********************************************

taskkill /FI "WINDOWTITLE eq netTestTx" > nul
taskkill /FI "WINDOWTITLE eq netTestRx" > nul

if %use_udp_lib%==0 (
	set TxPrg=..\..\bin\x64\release\dev0.exe
	set RxPrg=..\..\bin\x64\release\host0.exe
) else (
	set TxPrg=..\..\bin\x64\release\dev1.exe
	set RxPrg=..\..\bin\x64\release\host1.exe
)

if %use_udp_lib%==0 (
	if %useTx%==1 (
		cmd.exe /C "start "netTestTx" srv\net_tx0.bat %TxPrg% %TxBufNum% %TxPeer% %TxPort% %PacketSize% %PacketGenType% %TxDelay% %TxHostIP%" 
	)
	if %useRx%==1 (
		cmd.exe /C "start "netTestRx" srv\net_rx0.bat %RxPrg% %RxBufNum% %RxHostIP% %RxPort% %PacketSize% %PacketGenType%" 
	)
) else (
	if %useRx%==1 (
		cmd.exe /C "start "netTestRx" srv\net_rx1.bat %RxPrg% %RxBufNum% %RxHostIP% %RxPort% %PacketSize% %PacketInBuf% %PacketGenType%" 
	)
	if %useTx%==1 (
		cmd.exe /C "start "netTestTx" srv\net_tx1.bat %TxPrg% %TxBufNum% %TxHostIP% %TxPort% %PacketSize% %PacketInBuf% %PacketGenType% %TxDelay% %TxPeer%" 
	)
)


