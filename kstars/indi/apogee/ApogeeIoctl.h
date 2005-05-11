// ApogeeIoctl.h    Include file for I/O 
//
// Copyright (c) 2000 Apogee Instruments Inc.
//
//Portions Copyright (c) 2000 The Random Factory.
//
// Define the IOCTL codes we will use.  The IOCTL code contains a command
// identifier, plus other information about the device, the type of access
// with which the file must have been opened, and the type of buffering.
//

extern unsigned short apogee_bit;
extern unsigned short apogee_word;
extern unsigned long apogee_long;
extern short apogee_signed;


// General Ioctl definitions for Apogee CCD device driver 

#define APOGEE_IOC_MAGIC 'j'
#define APOGEE_IOC_MAXNR 100
#define APOGEE_IOCHARDRESET   _IO(APOGEE_IOC_MAGIC,0)

 
// Read single word 
#define IOCTL_GPD_READ_ISA_USHORT  _IOR(APOGEE_IOC_MAGIC,1,apogee_word)   
 
// Write single word 
#define IOCTL_GPD_WRITE_ISA_USHORT _IOW(APOGEE_IOC_MAGIC,2,apogee_word)  
    
// Read line from camera 
#define IOCTL_GPD_READ_ISA_LINE _IOR(APOGEE_IOC_MAGIC,3,apogee_word)  
 
#define IOCTL_GPD_READ_PPI_USHORT _IOR(APOGEE_IOC_MAGIC,1,apogee_word)  
    
// Write single word 
#define IOCTL_GPD_WRITE_PPI_USHORT _IOW(APOGEE_IOC_MAGIC,2,apogee_word)  
 
// Read line from camera 
#define IOCTL_GPD_READ_PPI_LINE  _IOR(APOGEE_IOC_MAGIC,3,apogee_word)  
