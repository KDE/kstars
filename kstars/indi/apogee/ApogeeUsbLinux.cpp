// ApogeeUsb.cpp : Library of basic USB functions for Apogee APn/Alta.
//

#include <assert.h>
#include <sys/time.h>                                                           
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sched.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>


#include <usb.h>
#include "ApogeeUsb.h"
#include "ApogeeUsbErr.h"
#include "ApogeeLinux.h"


#define HANDLE   struct usb_dev_handle;
#define ULONG    unsigned int
#define BOOLEAN  unsigned int
#define USHORT   unsigned short

#define APOGEE_USB_DEVICE "/dev/usb/alta"
#define INVALID_HANDLE_VALUE  -1

#define VND_ANCHOR_LOAD_INTERNAL		0xA0
#define VND_APOGEE_CMD_BASE			0xC0
#define VND_APOGEE_STATUS			( VND_APOGEE_CMD_BASE + 0x0 )
#define VND_APOGEE_CAMCON_REG			( VND_APOGEE_CMD_BASE + 0x2 )
#define VND_APOGEE_BUFCON_REG			( VND_APOGEE_CMD_BASE + 0x3 )
#define VND_APOGEE_SET_SERIAL			( VND_APOGEE_CMD_BASE + 0x4 )
#define VND_APOGEE_SERIAL			( VND_APOGEE_CMD_BASE + 0x5 )
#define VND_APOGEE_EEPROM			( VND_APOGEE_CMD_BASE + 0x6 )
#define VND_APOGEE_SOFT_RESET			( VND_APOGEE_CMD_BASE + 0x8 )
#define VND_APOGEE_GET_IMAGE			( VND_APOGEE_CMD_BASE + 0x9 )
#define VND_APOGEE_STOP_IMAGE			( VND_APOGEE_CMD_BASE + 0xA )

#define USB_ALTA_VENDOR_ID	0x125c
#define USB_ALTA_PRODUCT_ID	0x0010
#define USB_DIR_IN  USB_ENDPOINT_IN
#define USB_DIR_OUT USB_ENDPOINT_OUT


// Global variables used in this DLL
struct usb_dev_handle	*g_hSysDriver;
ULONG	g_UsbImgSizeBytes;
char	controlBuffer[1024];

#define IMAGE_BUFFER_SIZE	126976	// Number of requested bytes in a transfer
//#define IMAGE_BUFFER_SIZE	253952	// Number of requested bytes in a transfer

															
// This is an example of an exported function.
APN_USB_TYPE ApnUsbOpen( unsigned short /*DevNumber*/ )
{

        /*char deviceName[128];*/
	struct usb_bus *bus;
	struct usb_device *dev;
	struct usb_dev_handle *hDevice(NULL);

	usb_init();

	usb_find_busses();
	usb_find_devices();

	/*char string[256];*/

	int found = 0;

	/* find ALTA device */
	for(bus = usb_busses; bus && !found; bus = bus->next) {
		for(dev = bus->devices; dev && !found; dev = dev->next) {
			if (dev->descriptor.idVendor == USB_ALTA_VENDOR_ID && 
			    dev->descriptor.idProduct == USB_ALTA_PRODUCT_ID) {
				hDevice = usb_open(dev);
//				cerr << "Found ALTA USB. Attempting to open... ";
				found = 1;
				if (hDevice) {
//					if (!usb_get_string_simple(hDevice, 
//						dev->descriptor.iSerialNumber, 
//						string, sizeof(string))) 
//							throw DevOpenError();
//					cerr << "Success.\n";
//					cerr << "Serial number: " << string << endl;
				}
				else return APN_USB_ERR_OPEN;
			}
		}
	}

	if (!found) return APN_USB_ERR_OPEN;
//	if (!usb_set_configuration(hDevice, 0x0)) return APN_USB_ERR_OPEN;
	if (!usb_claim_interface(hDevice, 0x0)) return APN_USB_ERR_OPEN;

	g_hSysDriver		= hDevice;
	g_UsbImgSizeBytes	= 0;
//	printf("DRIVER: opened device\n");

	return APN_USB_SUCCESS;		// Success
}


APN_USB_TYPE ApnUsbClose( void )
{

	if ( g_hSysDriver != 0 )
	{
		usb_release_interface(g_hSysDriver, 0x0);
		usb_close(g_hSysDriver);
		g_hSysDriver = 0;
	}

	return APN_USB_SUCCESS;		// Success
}



APN_USB_TYPE ApnUsbReadReg( unsigned short FpgaReg, unsigned short *FpgaData )
{
    int Success;
    unsigned short RegData;

    Success = usb_control_msg((struct usb_dev_handle *)g_hSysDriver, 
                                USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE, VND_APOGEE_CAMCON_REG,
                                            FpgaReg, FpgaReg,(char *)&RegData, 2, 50);
    *FpgaData = RegData;

/*    printf("DRIVER: usb read reg=%x data=%x s=%x\n",FpgaReg,*FpgaData,Success); */
    if ( !Success )
		return APN_USB_ERR_WRITE;
    return APN_USB_SUCCESS;		// Success
}

APN_USB_TYPE ApnUsbWriteReg( unsigned short FpgaReg, unsigned short FpgaData )
{
    char *cbuf;
    int Success;

    cbuf = (char *)&FpgaData;
    Success = usb_control_msg((struct usb_dev_handle *)g_hSysDriver, 
                                  USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,  VND_APOGEE_CAMCON_REG,
                                   0, FpgaReg, cbuf, 2, 50);
/*    printf("DRIVER: usb write reg=%x data=%x s=%x\n",FpgaReg,FpgaData,Success); */
    if ( !Success )
		return APN_USB_ERR_WRITE;
    return APN_USB_SUCCESS;		// Success


}




APN_USB_TYPE ApnUsbWriteRegMulti( unsigned short FpgaReg, unsigned short FpgaData[], unsigned short RegCount )
{
	unsigned short	Counter;

	for ( Counter=0; Counter<RegCount; Counter++ )
	{
		if ( ApnUsbWriteReg( FpgaReg, FpgaData[Counter] ) != APN_USB_SUCCESS )
		{
			return APN_USB_ERR_WRITE;
		}
	}

	return APN_USB_SUCCESS;		// Success
}

APN_USB_TYPE ApnUsbWriteRegMultiMRMD( unsigned short FpgaReg[], 
				      unsigned short FpgaData[],
				      unsigned short RegCount )
{
	unsigned short	Counter;

	for ( Counter=0; Counter<RegCount; Counter++ )
	{
		if ( ApnUsbWriteReg( FpgaReg[Counter], 
				     FpgaData[Counter] ) != APN_USB_SUCCESS )
		{
			return APN_USB_ERR_WRITE;
		}
	}

	return APN_USB_SUCCESS;
}


APN_USB_TYPE ApnUsbReadStatusRegs( unsigned short *StatusReg,
				   unsigned short *HeatsinkTempReg,
				   unsigned short *CcdTempReg,
				   unsigned short *CoolerDriveReg,
				   unsigned short *VoltageReg,
				   unsigned short *TdiCounter,
				   unsigned short *SequenceCounter )
{
	BOOLEAN		Success;
	/*unsigned int	BytesReceived;*/
	unsigned short *Data;
	unsigned char	StatusData[21];	
	
	Success = usb_control_msg(g_hSysDriver,
                                  USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,  VND_APOGEE_STATUS,
                                    0, 0, (char *)&StatusData, 21, 3000);

//	if ( !Success )
//		return APN_USB_ERR_STATUS;
	Data = (unsigned short *)StatusData;

	*HeatsinkTempReg	= Data[0];
	*CcdTempReg		= Data[1];
	*CoolerDriveReg		= Data[2];
	*VoltageReg		= Data[3];
	*TdiCounter		= Data[4];
	*SequenceCounter	= Data[5];
	*StatusReg		= Data[6];

	if ( (StatusData[20] & 0x01) != 0 )
	{
		*StatusReg |= 0x8;
	}

	return APN_USB_SUCCESS;
}


APN_USB_TYPE ApnUsbStartExp( unsigned short ImageWidth,
			     unsigned short ImageHeight )
{
	BOOLEAN Success;
	ULONG	ImageSize;
	unsigned short	BytesReceived;


//	if ( (g_hSysDriver) == 0)
//	{
//		return APN_USB_ERR_OPEN;
//	}

	g_UsbImgSizeBytes = ImageWidth * ImageHeight * 2;
	ImageSize = ImageWidth * ImageHeight;

	if ( g_UsbImgSizeBytes == 0 )
	{
	 	return APN_USB_ERR_START_EXP;
	}

	Success = usb_control_msg(g_hSysDriver,
                                   USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,  VND_APOGEE_GET_IMAGE,
                                   (ImageSize >> 16) & 0xFFFF, 
				   (ImageSize & 0xFFFF), (char *)&BytesReceived, 4, 3000);

//        printf("DRIVER: startexp s=%x\n",Success);
   
//	if ( !Success )
//	{
//		return APN_USB_ERR_START_EXP;
//	}

	return APN_USB_SUCCESS;
}


APN_USB_TYPE ApnUsbStopExp( bool DigitizeData )
{
	BOOLEAN Success;
	unsigned short	BytesReceived;


//	if ( (g_hSysDriver) == 0)
//	{
//		return APN_USB_ERR_OPEN;
//	}

	if ( DigitizeData == false )
	{
		  Success = usb_control_msg(g_hSysDriver,
                               USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,  VND_APOGEE_STOP_IMAGE,
                                            0, 0,(char *)&BytesReceived, 2, 3000);
    
//		if ( !Success )
//		{
//			return APN_USB_ERR_STOP_EXP;
//		}
	}

	return APN_USB_SUCCESS;
}

APN_USB_TYPE ApnUsbGetImage( unsigned short *pMem )
{
	BOOLEAN Success;
	ULONG	ImageBytesRemaining;
	ULONG	ReceivedSize;

	Success = 1;
//	if ( (g_hSysDriver) == 0 )
//	{
//		return APN_USB_ERR_OPEN;
//	}

	ImageBytesRemaining = g_UsbImgSizeBytes;


	////////////////////////
	ULONG LoopCount = g_UsbImgSizeBytes / IMAGE_BUFFER_SIZE;
	ULONG Remainder	= g_UsbImgSizeBytes - ( LoopCount * IMAGE_BUFFER_SIZE );
	ULONG MemIterator = IMAGE_BUFFER_SIZE / 2;
	ULONG Counter;


	for ( Counter=0; Counter<LoopCount; Counter++ )
	{
		ReceivedSize = usb_bulk_read(g_hSysDriver, 0x86, 
				 (char *)pMem, IMAGE_BUFFER_SIZE, 1000);
//        printf("DRIVER: bulkread size=%x\n",ReceivedSize);

		if ( ReceivedSize != IMAGE_BUFFER_SIZE )
		{
			Success = 0;
			break;
		}
		else
		{
			pMem += MemIterator;
	printf(".");
		}
	}
	
	if ( Remainder != 0 )
	{
		ReceivedSize = usb_bulk_read(g_hSysDriver, 0x86, 
				(char *)pMem, Remainder, 1000);
//        printf("DRIVER: bulkread2 size=%x\n",ReceivedSize);

		if ( ReceivedSize != Remainder )
			Success = 0;
	}
	printf("\n");

	if ( !Success )
		return APN_USB_ERR_IMAGE_DOWNLOAD;

	return APN_USB_SUCCESS;		// Success
}


APN_USB_TYPE ApnUsbReset()
{
	BOOLEAN Success;
	unsigned short	BytesReceived;

//	if ( (g_hSysDriver) == 0)
//	{
//		return APN_USB_ERR_OPEN;
//	}

	Success = usb_control_msg(g_hSysDriver,
                                   USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,  VND_APOGEE_SOFT_RESET,
                                   0, 0, (char *)&BytesReceived, 2, 3000);
//        printf("DRIVER: reset s=%x\n",Success);
   
	if ( !Success )
	{
		return APN_USB_ERR_RESET;
	}

	return APN_USB_SUCCESS;
}



















