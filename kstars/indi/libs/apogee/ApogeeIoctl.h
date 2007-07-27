/*  Apogee Control Library

Copyright (C) 2001-2006 Dave Mills  (rfactory@theriver.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/
// ApogeeIoctl.h    Include file for I/O 
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
