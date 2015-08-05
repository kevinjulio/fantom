**DarkSilk (DRKSLK) Version 1.0**

DarkSilk Integration/Staging Tree
================================

**Copyright (c) 2015 DarkSilk Developers**

What is DarkSilk?
----------------
Scrypt
PoW/PoS
PoS Interest - 5% Annually
420,000 Blocks for PoW Phase
4.2 Minute Target Spacing
Difficulty Retargets Every Block
42 Coins Per Block
42 Blocks for DarkSilk to be Minted/Mature
42 Hour Minimum Stake Age
90,000,000 Total Coins

50MB Maximum Block Size (50X Bitcoin Core)


DarkSilk includes an Address Index feature, based on the address index API (searchrawtransactions RPC command) implemented in Bitcoin Core but modified implementation to work with the DarkSilk codebase (PoS coins maintain a txindex by default for instance).
Initialize the Address Index by running the -reindexaddr command line argument, it may take 10-15 minutes to build the initial index.

DarkSilk only broadcasts transactions once unlike Bitcoin Core which rebroadcasts transactions which can be used to unmask and link IP addresses and transactions. 
However if your transaction does not successfully broadcast you can use the -resendtx console command to broadcast the transaction again.

DarkSilk implements Gavin Andresens signature cache optimisation from Bitcoin for significantly faster transaction validation.


**Sandstorm Network Information**
Ported Masternodes from Darkcoin and rebranded as Stormnodes and modified to use stealth addresses.
Darksend ported and rebranded as Sandstorm.
Utilisation of InstantX for instant transaction confirmation.
Multiple Stormnodes can be ran from the same IP address.


**MainNet Parameters**
P2P Port = 31000
RPC Port = 31500
Magic Bytes: 0x1f 0x22 0x05 0x31

**TestNet Parameters**
P2P Port = 31750
RPC Port = 31800
Magic Bytes: 0x1f 0x22 0x05 0x30


DarkSilk is dependent upon libsecp256k1 by sipa, please follow this guide to install to Linux
=============================================================================================
//Open a terminal

//Type:
$ cd ~

//Then:
$ sudo apt-get install libtool unzip

//Then:
$ cd darksilk/libs/

//Then:
$ sudo unzip secp256k1-master.zip

//Then:
$ cd secp256k1-master

//Then:
$ libtoolize

//Then:
$ aclocal

//Then:
$ autoheader

//Then:
$ ./autogen.sh

//Then:
$ ./configure

//Then:
$ make

//Then:
$ ./tests

//Then:
$ sudo make install


//end of guide


Build Instructions for QT5 Linux Wallet
======================================
//Create a folder named DarkSilk in /home/ and unpack the contents of ~/DarkSilk to that folder.

//Install dependencies via Terminal:

$ sudo apt-get install make libqt5webkit5-dev libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev qttools5-dev-tools libprotobuf-dev protobuf-compiler build-essential libboost-dev libboost-all-dev libboost-system-dev libboost-filesystem-dev libboost-program-options-dev libboost-thread-dev libssl-dev libdb++-dev libminiupnpc-dev libevent-dev libcurl4-openssl-dev git libpng-dev qrencode libqrencode-dev

//In terminal navigate to the DarkSilk folder.

$ cd /home/DarkSilk

//Then:

$ qmake -qt=qt5 "USE_QRCODE=1" "USE_NATIVE_I2P=1"

//Then:

$ make

//This will compile and build the QT Wallet which takes a little while, please be patient.

//When finished you will have a file called DarkSilk - Simply Double Click


//end of guide


Build Instructions for Terminal Based Linux Wallet
===================================================
//Create a folder named DarkSilk in /home/ and unpack the contents of ~/DarkSilk to that folder.

//Install dependencies via Terminal:

$ sudo apt-get install build-essential libboost-all-dev libcurl4-openssl-dev libdb5.1-dev libdb5.1++-dev qt-sdk make 

//In terminal navigate to the DarkSilk folder.

$ cd /home/DarkSilk/src/

//Then:

$cp crypto obj/crypto -rR

//Enter into the terminal:

$ make -f makefile.unix "USE_NATIVE_I2P=1"

//This will produce a file named darksilkd which is the command line instance of DarkSilk

//Now type:

$ strip darksilkd

//When finished you will have a file called darksilkd

//To run DarkSilk

$ ./darksilkd & 

//It will complain about having no darksilk.conf file, we'll edit the one provided and move it into place

$ cd ..
$ nano darksilk.conf

//Edit the Username and Password fields to anything you choose (but remember them) then save the file

$ mv darksilk.conf /home/DarkSilk/src/
$ cd src/
$ ./darksilkd &

//The server will start. Here are a few commands, google for more.

$ ./darksilkd getinfo
$ ./darksilkd getmininginfo
$ ./darksilkd getnewaddresss


//end of guide


Example Stormnode .conf Configuration
===================================================

rpcallowip=127.0.0.1
rpcuser=MAKEUPYOUROWNUSERNAME
rpcpassword=MAKEUPYOUROWNPASSWORD
rpcport=MAKEUPYOUROWNPORT
server=1
daemon=1
listen=1
staking=0
port=9999
stormnodeaddr=YOUR_IP:9999
stormnode=1
stormnodeprivkey=KEY GENERATED BY COMMAND
snconflock=1 // Stormnodes like Masternodes in Dash a.k.a. Darkcoin can be locked via the configuration file 


Set the stormnodeprivkey to the result of running:

stormnode genkey
//From the RPC console in the local wallet.

//If you are running your stormnode as a hidden service, replace YOUR_IP with your onion hostname (typically found in /var/lib/tor/YOURHIDDENSERVICENAME/hostname).

Port 9999 must be open.

//If you are using the hot/cold system where you are remotely activating your stormnode from your local wallet, you will also put these entries in your local (cold) wallet .conf:

stormnodeaddr=YOUR_IP:9999
stormnode=1
stormnodeprivkey=KEY GENERATED BY COMMAND

//Then, start the daemon on your remote stormnode, start your local wallet, and then you can start your stormnode from the RPC console:

stormnode start


//end of guide
