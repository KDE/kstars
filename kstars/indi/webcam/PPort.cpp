#include "PPort.h"
#include "port.h"

#include <unistd.h>
#include <sys/types.h>
#include <iostream>

using namespace std;

#define TEST_VALIDITY {if (this==NULL) return false;}

PPort::PPort() {
   reset();
}

PPort::PPort(int ioPort) {
   reset();
   setPort(ioPort);
}

void PPort::reset() {
   bitArray=0;
   for (int i=0;i<8;++i) {
      assignedBit[i]=NULL;
   }
   currentPort=NULL;
}

bool PPort::setPort(int ioPort) {
   TEST_VALIDITY;
   if (geteuid() != 0) {
      cerr << "must be setuid root control parallel port"<<endl;
      return false;
   }
   if (currentPort) {
      delete currentPort;
   }
   reset();
   currentPort=new port_t(ioPort);
   return commit();
}

bool PPort::commit() {
   TEST_VALIDITY;
   if (currentPort) {
      currentPort->write_data(bitArray);
      return true;
   } else {
      return false;
   }
}

bool  PPort::setBit(const void * ID,int bit,bool stat) {
   TEST_VALIDITY;
   if (ID != assignedBit[bit]) {
      return false;
   }

   if (stat) {
      bitArray |= (1<<bit);
   } else {
      bitArray &= ~(1<<bit);
   }
   return true;
}

bool PPort::registerBit(const void * ID,int bit) {
   TEST_VALIDITY;
   if (assignedBit[bit] || currentPort==NULL) {
      return false;
   }
   assignedBit[bit]=ID;
   return true;
}

bool PPort::unregisterBit(const void * ID,int bit) {
   TEST_VALIDITY;
   if (assignedBit[bit] != ID) {
      return false;
   }
   assignedBit[bit]=NULL;
   return true;
}

bool PPort::isRegisterBit(const void * ID,int bit) const {
   TEST_VALIDITY;
   return (assignedBit[bit] == ID);
}
