/*  FITS debayer tool
    Copyright (C) 2017 Csaba Kertesz (csaba.kertesz@gmail.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

 */

#include "fitsdata.h"

#include <QFile>

namespace
{
void Usage()
{
  printf("usage: fitsdebayertool input_fits output_image\n"
         "\n"
         "The program converts an input fits image into a debayered image (png, jpg or tiff).\n\n");
}
}

int main(int argc, char* argv[])
{
  printf("fitsdebayertool 0.1\n\n");

  if (argc != 3)
  {
    Usage();
    return 1;
  }

  if (!QFile(argv[1]).exists())
  {
    printf("The input file (%s) does not exist\n\n", argv[1]);
    return 1;
  }
  QImage Image(FITSData::FITSToImage(QString(argv[1])));

  printf("Save to %s\n", argv[2]);
  Image.save(QString(argv[2]));
  printf("Conversion finished!\n\n");
  return 0;
}
