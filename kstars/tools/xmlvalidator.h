/***************************************************************************
                          xmlvalidator.h  -  K Desktop Planetarium

                             -------------------
    begin                : Sunday August 21, 2011
    copyright            : (C) 2011 by Lukasz Jaskiewicz
    email                : lucas.jaskiewicz@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef XMLVALIDATOR_H
#define XMLVALIDATOR_H

class QIODevice;
class QString;

/**
  * Class used for validating XML document against XML Schema (XSD files).
  * \author Lukasz Jaskiewicz
  */
class XmlValidator
{
public:
    /**
      * Validate XML document against XML schema definition.
      * \param xml Pointer to QIODevice that holds XML document.
      * \param schema Pointer to QIODevice that holds XML Schema Definition document.
      * \param errorMsg Reference to QString that will hold error messages after validation (if any).
      * \return Returns false if document failed to validate.
      */
    static bool validate(QIODevice *xml, QIODevice *schema, QString &errorMsg);
};

#endif // XMLVALIDATOR_H
