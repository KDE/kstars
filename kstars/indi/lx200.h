
   /** Gets LX200 Altitude. The Alt is computed in the same way as getLX200DEC()
   *@returns Telescope's altitude */   
   double getLX200Alt();

   /** Gets LX200 Azimuth. The Az is computed in the same way as getLX200RA()
   *@returns Telescope's azimith */
   double getLX200Az();

   /** Gets Sidereal time. The ST is computed in the same way as getLX200RA()
   *@returns Sideral time */
   double getSideralTime();

   /** @returns Calendar date in mm/dd/yy format */
   char * getCalenderDate();

   /** Gets current site latitude. The latitude is computed in the same way as getLX200DEC()
   *@returns Current site latitude */
   double getSiteLatitude();

   /** Gets requested site latitude. The latitude is computed in the same way as getLX200DEC()
   *@param siteNum Site identifier. It can be either 1, 2, 3, or 4.
   *@returns requested site latitude */
   double getSiteLatitude(int siteNum);


   /** Gets current site longitude. The long. is computed in the same way as getLX200RA()
   *@returns Current site longitude */
   double getSiteLongitude();

   /** Gets requested site longitude. The long. is computed in the same way as getLX200RA()
   *@param siteNum Site identifier. It can be either 1, 2, 3, or 4.
   *@returns requested site longitude */
   double getSiteLongitude(int siteNum);


   /** Gets local time. The local time is computed in the same way as getLX200RA()
   *@param timeFormat specifies the format of the returned time. 12 returns time computed using the 12 hour format, and 24
       returnns time computed using the 24 hour format.
   *@returns local time */
   double getLocalTime(int timeFormat);

   /** @returns a string indicating the class that should be returned by the FIND/BROWSE 
   command. If the charecter is uppercase, the object class is return, otherwise, it is ignored. The charecters
   are interpreted as following:
   <li> G - Galaxies \n
   <li> P - Planetary Nebulae \n
   <li> D - Diffuse Nebulae \n
   <li> C - Globular Clusters \n
   <li> O - Open Clusters \n */
   char getDeepSkySearchString();

   /** Gets FIND operation quality. \n
   * \n <b> LX200 classic specific</b>: This operation is avaiable only to LX200 \e classic telescopes.
    @returns A string indicating FIND quality. The string can be any of the following: \n
    <LI> SU (Super)
    <LI> EX (Excellent)
    <LI> VG (Very good)
    <LI> GD (Good)
    <LI> FR (Fair)
    <LI> PR (Poor)
    <LI> VP (Very poor) \n */
   char * getFINDQuality();

   /** Gets FIND operation LOWER or HIGHER limit. \n
   *@param boundary 0 indicates LOWER and 1 indicates HIGHER.
    @returns FIND operation limit */
   int getFINDElevationLimit(int elevationBoundary);

   /** Gets FIND operation FAINTER or BRIGHTER magnitude limit. \n
   *@param boundary 0 indicates FAINTER and 1 indicates BRIGHTER.
    @returns FIND operation manitude limit */   
   double getFINDMagnitudeLimit(int mangnitudeBoundary);

    /** Gets FIND operation SMALLER or LARGER size limit. \n
   *@param boundary 0 indicates SMALLER, and 1 indicates LARGER.
    @returns FIND operation size limit */
   int getFINDSizeLimit(int sizeBoundary);
   
   /** @returns FILED radius */   
   int getFIELDRadius();

   /**
   *@param siteNum Site identifier. It can be either 1, 2, 3, or 4. To get the current site name, do not pass any parameters.
    @returns name of requested site */
   char * getSiteName(int siteNum = -1);

   /** @returns Current site's UTC offset */      
   int getUCTOffset();

   /** @returns Tracking freqency */         
   double getCurrentTrackFreq();

   /** @returns A string contianing version date */            
   char * getVersionDate();
   /** @returns A string contianing version time */            
   char * getVersionTime();
   /** @returns A string contianing a full description of the version */               
   char * getFullVersion();
   /** @returns A string contianing version number (major.minor) */            
   char * getVersionNumber();
   /** @returns product name */            
   char * getProductName();

   /** Gets number of bars on display unit. \n
   * \n <b> LX200 Autostar specific</b>: Autostar returns 1 when slewing, otherwise 0.
   @return number of bars. */
   int getNumberOfBars();

   /** Gets LX200 internal coordinates format. \n
   @returns LX200SHORT for short format, and LX200LONG for long format. */
   TFormat getLX200Format() { return LX200Format; }

   /** Gets LX200 internal coordinates format. \n
   @returns LX200SHORT for short format, and LX200LONG for long format. */
   TTelescope getTelescopeType() { return Telescope; }


    /*********************************
     // SET Commands Groupings
    *********************************/

   /** Sets calendar date. \n
   @returns true if date is valid and was successfully set. */
    bool setCalendarDate(int dd, int mm, int yy);
    
   /** Sets current site longitude. \n
   @returns true if current site longitude is successfully set. */
    bool setSiteLongitude(int degrees, int minutes);
    
    /** Sets current site latitude. \n
   @returns true if current site latitude is successfully set. */
    bool setSiteLatitude(int degrees, int minutes);

    /** Sets UTC offset from local time. \n
    @param hours Offset ranging from -12 hours to +12 hours.
    @returns true if UTC offset successfully set. */
    bool setUTCOffset(int hours);
        
    /** Sets Local time. \n
    @returns true if local time was successfully set. */
    bool setLocalTime(int hours, int minutes, int seconds);

    /** Sets a site name. \n
    @param siteNum Site identifier. It can be either 1, 2, 3, or 4.
    @param siteName A terminated string containing a site name. Must be equal or less than 16 characters long.
    @returns true if site name was successfully set. */
    bool setSiteName(int siteNum, const char * siteName);

    /** Sets object's declination. \n
    @returns true if object's DEC was successfully set. */
    bool setObjDEC(int degrees, int minutes, int seconds);

    /** Overloaded function to set object's declination. \n
    @returns true if object's DEC was successfully set. */
    bool setObjDEC(double DEC);


    /** Sets object's right ascension. \n
    @returns true if object's RA was successfully set. */    
    bool setObjRA(int hours, int minutes, int seconds);

    /** Overloaded function to set object's right ascension. \n
    @returns true if object's RA was successfully set. */
    bool setObjRA(double RA);

    /** Sets Sidereal time. \n
    @returns true if SD was successfully set. */
    bool setSiderealTime(int hours, int minutes, int seconds);
    
     /** Sets Tracking frequency. \n
    @param Freq Tracking frequency in Hz.
    @returns true if tracking frequncy was successfully set. */
    bool setTrackFreq(double Freq);
    
    /** Sets Maximum slew rate (degrees/second). \n
   * \n <b> OLD ... change DOCs LX200 Autostar specific</b>: Autostar maps the slew rate to resemble that of the controller keys: \n
   * <LI> 2 is slowest (0 Key) \n
   * <LI> 3 is medium (5 Key) \n
   * <LI> 4 is fast (9 key) \n \n
    @param slewRate 2, 3, or 4 degrees/second for LX200 \e classic telescopes. Autostar maps the rates as explained above.
    @returns true if slew rate was successfully set. */
    bool setMaxSlewRate(int slewRate);

    /** Sets Alignment mode. \n
    @param alignMode can be either ALTAZ, LAND, or POLAR. */
    void setAlignmentMode(TAlign alignMode);

     /** Overloaded set Alignment mode. \n
    @param alignMode can be either A for ALTAZ, L for LAND, or P for POLAR. */
    void setAlignmentMode(char alignMode);

    
    /** Sets Internal hour mode. \n
    @param hMode can be either TWELVEHOUR or TWENTYFOURHOUR */
    void setHourMode(TTime hMode);

    /** Sets Tracking rate. \n
    @param hMode can be either MANUAL, QUARTZ, INCREMENT, DECREMENT */
    void setTrackingRate(TFreq tRate);

    /** Selects and sets active site. \n
    @param siteNum Site identifier. It can be either 1, 2, 3, or 4.*/
    void selectSite(int siteNum);

    /** Sets FIND operation magnitude search limit \n
   *@param boundary 0 indicates FAINTER, 1 indicates BRIGHTER.
   *@param magnitude is the limiting magnitude of the FIND operation
    @returns true if magnitude limit was successfully set */
    bool setFINDMagnitudeLimit(int magnitudeBoundary, double magnitude);

    /** Sets FIND operation boundary limit \n
   *@param boundary 0 indicates LOWER, 1 indicates HIGHER.
   *@param degrees is the limiting search scope for FIND operation
    @returns true if boundary limit was successfully set */
    bool setFINDElevationLimit(int elevationBoundary, int degrees);

   /** Sets FIND operation size limit \n
   *@param boundary 0 indicates SMALLER, 1 indicates LARGER.
   *@param size is the limiting number of objects searched via the FIND operation
    @returns true is size limit was successfully set */    
    bool setFINDSizeLimit(int sizeBoundary, int size);

    /** Sets FIND operation string \n
    @returns true if I know what it does!! */
    bool setFINDStringType(char objects);

    /** Sets FIELD radius \n
   *@param radius is FIELD's radius in degrees.
    @returns true if FIELD's radius was successfully set */
    bool setFIELDRadius(int radius);

    /** Increases reticle brightness */
    void increaseReticleBrightness();
    /** Decreases reticle brightness */
    void descreaseReticleBrightness();

    /** Sets LX200 internal coordinates format \n
   *@param format can be either LX200SHORT or LX200LONG. */
    void setLX200Format(TFormat format);
    
    
    ///// restructure again, 
    /** Sets Reticle flash rate \n
   *@param fRate specifes flash mode. Valid values are from 0...9. */
    void setReticleFlashRate(int fRate);
    
   /** Starts a home search. This command applies <b>only</b> to 16" LX200.
   @returns One of the following values: \n
   * <LI> 0 if home search failed. \n
   * <LI> 1 if home search found. \n
   * <LI> 2 if home search in progress \n */
   int getHomeSearchStatus();
   
   void turnFanOn();
   void turnFanOff();
   
   void seekHomeAndSave();
   void seekHomeAndSet();
   
   /** Turns the field derotator On or Off. This command applies <b>only</b> to 16" LX200.
   @param turnOn if turnOn is true, the derotator is turned on, otherwise, it is turned off. */
   void fieldDeRotator(bool turnOn);
   
   /** Changes smart drive status. \n
   * \n <b> LX200 classic specific</b>: This operation is avaiable only to LX200 \e classic telescopes.
   *@param cToggle is the smart drive status identifier. Valid values are from 1...5. */
   void toggleSmartDrive (int cToggle);
   
   /** Sets object Altitude. \n
   @returns true if object's altitude is successfully set. */
    bool setObjAlt(int degrees, int minutes);
    
   /** Sets object Azimuth. \n
   @returns true if object's azimuth is successfully set. */
    bool setObjAz(int degrees, int minutes);
      
    /*********************************
   // Motion Commands Groupings
   *********************************/
    /** Slews to AltAz. This command applies <b>only</b> to 16" LX200. */
   void slewToAltAz();

    /** Moves the telescope in the requested direction. \n
   *@param dir can be either NORTH, EAST, SOUTH, or WEST. */
   void move(TDirection dir);

   /** Slews to the current object \n
    @returns one of the following values: \n
    * <LI> 0 if slew is possible. \n
    * <LI> 1 if object is below the horizon. \n
    * <LI> 2 if object is below HIGHER. Autostar \e never returns 2. */
   int slewToObj();

   /** Aborts movement in the requested direction.\n
     @param dir can be either NORTH, EAST, SOUTH, or WEST. */   
   void abortMove(TDirection dir);

   /** Aborts slew in any direction.*/      
   void abortSlew();

   /*********************************
   // Library Commands Groupings
   *********************************/
   
   /** Go to previous object in FIND.\n
   * \n <b> LX200 classic specific</b>: This operation is avaiable only to LX200 \e classic telescopes. */
   void libraryPrevObj();

    /** Go to next object in FIND.\n
   * \n <b> LX200 classic specific</b>: This operation is avaiable only to LX200 \e classic telescopes. */
    void libraryNextObj();

    /** Starts a FIND operation.\n
   * \n <b> LX200 classic Specific</b>: This operation is avaiable only to LX200 \e classic telescopes. */   
   void libraryStartFind();

    /** Retrives current object information.\n
   * \n <b> LX200 classic specific</b>: This operation is avaiable only to LX200 \e classic telescopes.
   * Autosar always return the same string "M31 EX GAL MAG 3.5 SZ178.0'#".\n
   @returns A string containing object information */
   char * libraryObjInfo();


   /** @returns A string containing a list of objects available in the FIELD. Autostar \e always returns "Objects: 0" */
   char * libraryFieldInfo();

   /** Find CNGC type object.\n
   * \n <b> LX200 classic specific</b>: This operation is avaiable only to LX200 \e classic telescopes.
    @param identifier is a four digit NNNN object identifier. */
   void libraryFindCNGC(int identifier);

    /** Find Messier type object.\n
   * \n <b> LX200 classic specific</b>: This operation is avaiable only to LX200 \e classic telescopes.
    @param identifier is a four digit NNNN object identifier. Valid values are 1...110 */
   void libraryFindMessier(int identifier);

    /** Find STAR type object.\n
   * \n <b> LX200 classic specific</b>: This operation is avaiable only to LX200 \e classic telescopes.
    @param identifier is a four digit NNNN object identifier. The planets are STAR 901...909. */
   void libraryFindStarType(int identifer);

    /** Sets NGC type catalog.\n
    @param ID can be either CNGC, IC, or UGC.
    @returns true if library catalog was successfully set */
   bool librarySetNGCType(TNGC ID);

    /** Sets STAR type catalog.\n
    @param ID can be either STAR, SAO, or GCVS.
    @returns true if library catalog was successfully set */
   bool librarySetSTARType(TSTAR ID);

   /*********************************
   // Misc. Commands Groupings
   *********************************/
    /** Synchronizes telescope coordinates to the object's coordinates.\n
    @returns which objects coordinates were used. Autostar \e always return the same string
    * " M31 EX GAL MAG 3.5 SZ178.0'#" */
   char * Sync();
   
    /** Sets FOCUS mode.\n
    @param fMode can be either FOCUSIN, FOCUSOUT, QUITFOCUS, FOCUSFAST, or FOCUSSLOW */
   void focusControl(TFocus fMode);

    /** Checks if current precision mode is high precision.\n
    @returns true if mode is high precision. Autostar \e always return true. */
   bool isHighPrecision();
   
   int returnCurrentSiteNum() { return currentSiteNum;}

     
  private:
  int fd;
  static const char* ports[4];
  enum TError { READ_ERROR, WRITE_ERROR, READOUT_ERROR, PORT_ERROR};

  TSlew SlewType;
  TAlign Alignment;
  TDirection Direction;
  TFormat LX200Format;
  TTelescope Telescope;
  TTime timeFormat;
  bool LX200Connected;
  int currentSiteNum;

  void queryLX200Format();
  void LX200readOut(int timeout);
  int getCurrentSiteNum();

  protected:
   /** Opens a terminal port for read/write \n
   *@param portID is a terminated string to a terminal device. e.g. "/dev/ttyS0"
   *@returns the file descriptor number if successful, or -1 on failure.*/
   int openPort(const char *portID);

   /** Writes to a port\n
   *@param buf is a pointer to the buffer to be written. The buffer string must be a terminated.
   *@returns the number of bytes remaining to be written. That is, if successful, the function will return 0. */
   int portWrite(const char * buf);

    /** Reads from a port\n
   *@param buf is a pointer to the buffer to be written to.
    @param nbytes is the number of bytes to be read. In case of -1, the function will read until it encounters a hash-mark #
   *@returns the number of bytes remaining to be read if \e nbytes > 0. If \e nbytes is -1 then it returns the number of bytes read. */
    int portRead(char *buf, int nbytes, int timeout = 2);

    /** Queries basic data from the telescope. The function attempts to determine the time and coordinates format, and the
    * type of telescope being connected to. It stores the data in internal class members.
    *@returns true if the query operation was successful*/
    bool queryData();
    
    bool setStandardProcedure(const char * writeData);
  
};

#endif
