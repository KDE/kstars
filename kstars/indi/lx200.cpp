char LX200::getDeepSkySearchString()
{
    int writeRet, readRet;
    char tempString[2];

    writeRet = portWrite("#:Gy#");

    readRet = portRead(tempString, 2);
    tempString[1] = '\0';

    return tempString[0];

}

char * LX200::getFINDQuality()
{
    int writeRet, readRet;
    char * tempString = new char[3];

    writeRet = portWrite("#:Gq#");

    readRet = portRead(tempString, 3);
    tempString[2] = '\0';

    return tempString;
}

int LX200::getFINDElevationLimit(int elevationBoundary)
{
    int writeRet, readRet;
    char * tempString = new char[4];

    // boundary = 0 ===> LOWER limit
    if (elevationBoundary == 0)
      writeRet = portWrite("#:Go#");
    // boundary = 1 ===> HIGHER limit
    else if (elevationBoundary == 1)
       writeRet = portWrite("#:Gh#");
    else
      return -1;

    readRet = portRead(tempString, 4);
    tempString[2] = '\0';

    int higher;

    sscanf(tempString, "%d", &higher);

    return higher;
}

double LX200::getFINDMagnitudeLimit(int magnitudeBoundary)
{
    int writeRet, readRet;
    char * tempString = new char[6];

    // boundary = 0 ===> FAINTER mag
    if (magnitudeBoundary == 0)
      writeRet = portWrite("#:Gf#");
    // boundary = 1 ===> BRIGHTER mag
    else if (magnitudeBoundary == 1)
       writeRet = portWrite("#:Gb#");
    else
      return -1;

    readRet = portRead(tempString, 6);
    tempString[5] = '\0';

    float magnitude;

    sscanf(tempString, "%f", &magnitude);

    std::cout << tempString << "\n";
    std::cout << magnitude;

    return (double) magnitude;
}

int LX200::getFINDSizeLimit(int sizeBoundary)
{
    int writeRet, readRet;
    char * tempString = new char[5];

    // boundary = 0 ===> SMALLER size
    if (sizeBoundary == 0)
      writeRet = portWrite("#:Gs#");
    // boundary = 1 ===> LARGER size
    // NOTE: #:GI# does not work on my LX-90 Autostar. I don't know yet.
    // NOTE: not neccesairly an error. It's maybe Autostar's larger size is not set
    // NOTE: This happens to site names. When there are not set, nothing is returned, and read times out.
    else if (sizeBoundary == 1)
       writeRet = portWrite("#:GI#");
    else
      return -1;

    readRet = portRead(tempString, 5);
    tempString[3] = '\0';

    int sizeLimit;

    sscanf(tempString, "%d", &sizeLimit);

    std::cout << tempString << "\n";
    std::cout << sizeLimit;

    return sizeLimit;

}

int LX200::getFIELDRadius()
{
    int writeRet, readRet;
    char * tempString = new char[5];

    writeRet = portWrite("#:GF#");

    readRet = portRead(tempString, 5);
    tempString[3] = '\0';

    int radius;

    sscanf(tempString, "%d", &radius);

    std::cout << tempString << "\n";
    std::cout << radius;

    return radius;

}

// Autostar returns 1 if slewing, otherwise 0
//FIND
bool LX200::setFINDMagnitudeLimit(int manitudeBoundary, double magnitude)
{
   char tempString[16];
   
   switch (manitudeBoundary)
   {
     // FAINTER
     case 0:
        sprintf(tempString, "#:Sf %+04.1f#", (float) magnitude);
        break;
     // BRIGHTER
     case 1:
       sprintf(tempString, "#:Sb %+04.1f#", (float) magnitude);
       break;
     default:
      return false;
   }

   return (setStandardProcedure(tempString));
}

bool LX200::setFINDElevationLimit(int elevationBoundary, int degrees)
{
   char tempString[16];

   switch (elevationBoundary)
   {
     // LOWER
     case 0:
        sprintf(tempString, "#:So %02dﬂ#", degrees);
        break;
     case 1:
     // HIGHER
       sprintf(tempString, "#:Sh %02d#", degrees);
       break;
     default:
      return false;
   }

   return (setStandardProcedure(tempString));

}

bool LX200::setFINDSizeLimit(int sizeBoundary, int size)
{

  char tempString[16];

   switch (sizeBoundary)
   {
     // SMALLER
     case 0:
        sprintf(tempString, "#:Ss %03d#", size);
        break;
    // LARGER
     case 1:
       sprintf(tempString, "#:Sl %03d#", size);
       break;
     default:
      return false;
   }

   return (setStandardProcedure(tempString));

}

bool LX200::setFINDStringType(char objects)
{

   char tempString[16];
   sprintf(tempString, "#:Sy %c#", objects);

   return (setStandardProcedure(tempString));

}

void LX200::setHourMode(TTime hMode)
{
  if (hMode == timeFormat)
   return;

   portWrite("#:H");

   timeFormat = hMode;
}

//FIELD
bool LX200::setFIELDRadius(int radius)
{
    char tempString[16];
    sprintf(tempString, "#:SF %03d#", radius);

   return (setStandardProcedure(tempString));
}

void LX200::setReticleFlashRate(int fRate)
{
  switch (fRate)
  {
    case 0:
    portWrite("#:B0#");
    break;
    case 1:
    portWrite("#:B1#");
    break;
    case 2:
    portWrite("#:B2#");
    break;
    case 3:
    portWrite("#:B3#");
    break;
    default:
    break;
  }
}
    


void LX200::toggleSmartDrive (int cToggle)
{

  if (cToggle < 1 || cToggle > 5)
   return;
   
  char tempString[7];
   
  sprintf(tempString, "#:$Q%d#", cToggle);
  
  portWrite(tempString);
}

bool LX200::setObjAlt(int degrees, int minutes)
{
    char * tempString = new char[16];

    sprintf(tempString, "#:Sa %+02dﬂ%02d#", degrees, minutes);

    return (setStandardProcedure(tempString));
}

bool LX200::setObjAz(int degrees, int minutes)
{
    char * tempString = new char[16];

   sprintf(tempString, "#:Sz %03dﬂ%02d#", degrees, minutes);

   return (setStandardProcedure(tempString));
}


// Library function
void LX200::libraryPrevObj()
{
  if (Telescope == LX200AUTOSTAR)
   return;
   
  portWrite("#:LB#");
}

void LX200::libraryNextObj()
{
    if (Telescope == LX200AUTOSTAR)
   return;

  portWrite("#:LN#");
}


void LX200::slewToAltAz()
{
 portWrite("#:MA#");
}


bool LX200::isHighPrecision()
{
  if (Telescope == LX200AUTOSTAR)
    return true;

  else return false;
  /*

  #P toggle High/Low precision. In high precision mode, the user must center a nearby bright star
  I'm not sure how to simulate this yet, the user can 'remotely' move the telescope, but how can we simulate
  pressing 'enter' keypad button? This is TODO

  int bytesRead = 0;

   char tempString[16];

   portWrite("#:P#");

   bytesRead = portRead(tempString, 14);
   tempString[bytesRead] = '\0';

   std::cout << "DEBUG --------> isHIGHPrecision: " << tempString << std::endl;
   //tempString[14] = '\0';

   if (tempString[0] == 'H')
     return true;
   else
     return false;

     */
}

void LX200::setTrackingRate(TFreq tRate)
{

  if (Telescope == LX200AUTOSTAR)
    return;
 
  switch (tRate)
  {
    case MANUAL:
      portWrite("#:TM#");
      break;
    case QUARTZ:
      portWrite("#:TQ#");
      break;
    case INCREMENT:
      portWrite("#:T+#");
      break;
    case DECREMENT:
      portWrite("#:T-#");
      break;
    default:
     break;
  }

}


