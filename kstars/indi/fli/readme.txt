Current Version 1.11

Changes since 1.1
	- Corrected documentation for function FLIConfigureIOPort()
	- Added some bounds checking on FLISetImageArea()
		Note that on cameras with a FWRev < 0x0300 exceeding the set image
		area will lead to VERY SLOW grabs
	- Added some bounds checking on FLIGrabRow()
	- Removed function FLIGrabFrame(), it was never supported anyways
	- Added FLI_SHUTTER_EXTERNAL_TRIGGER to documentation for FLIControlShutter()
	- Added FLI_SHUTTER_EXTERNAL_TRIGGER_LOW and FLI_SHUTTER_EXTERNAL_TRIGGER_HIGH
	- Added FLIStartBackgroundFlush()
	- Repaired temperature settungs under Linux
	- Made temperature conversion more portable
	
