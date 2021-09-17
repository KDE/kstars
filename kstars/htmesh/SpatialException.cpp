//#		Filename:	SpatialException.cpp
//#
//#		Author:		John Doug Reynolds, P. Kunszt
//#
//#		Date:		Mar 1997
//#
//#	    SPDX-FileCopyrightText: 2000 Peter Z. Kunszt Alex S. Szalay, Aniruddha R. Thakar
//#                     The Johns Hopkins University
//#
//#		Modification history:
//#
//#		Oct. 1998, P. Kunszt : remove Rogue Wave C-string dependency
//#                almost all the interface had to be rewritten.
//#				   Also, use some of the inheritance to avoid
//#				   code duplication. Introduced defaultstr[].
//#     Oct 18, 2001 : Dennis C. Dinge -- Replaced ValVec with std::vector
//#

#include <cstdio>
#include <cstdlib>
#include <string.h>
#include <SpatialException.h>

/* --- SpatialException methods ------------------------------------------------- */
const char *SpatialException::defaultstr[] = { "SDSS Science Archive",
                                               "generic exception",           // These specialized exceptions are
                                               "unimplemented functionality", // currently implemented. If no string
                                               "failed operation",            // is given, this is the standard
                                               "array bounds violation",      // message.
                                               "interface violation" };

#define CONTEXT       0 // indices of exceptions
#define GENERIC       1
#define UNIMPLEMENTED 2
#define FAILURE       3
#define BOUNDS        4
#define INTERFACE     5

SpatialException::~SpatialException() throw()
{
    clear();
}

SpatialException::SpatialException(const char *cstr, int defIndex) throw()
{
    try
    {
        if (cstr)
        {
            str_ = new char[slen(cstr) + 1];
            strcpy(str_, cstr);
        }
        else
        {
            str_ = new char[50];
            sprintf(str_, "%s : %s", defaultstr[CONTEXT], defaultstr[defIndex]);
        }
    }
    catch (...)
    {
        clear();
    }
}

SpatialException::SpatialException(const char *context, const char *because, int defIndex) throw()
{
    try
    {
        const char *tmpc, *tmpb;
        tmpc = context ? context : defaultstr[CONTEXT];
        tmpb = because ? because : defaultstr[defIndex];
        str_ = new char[slen(tmpc) + slen(tmpb) + 50]; // allow extensions
        sprintf(str_, "%s : %s", tmpc, tmpb);
    }
    catch (...)
    {
        clear();
    }
}

SpatialException::SpatialException(const SpatialException &oldX) throw()
{
    try
    {
        if (oldX.str_)
        {
            str_ = new char[slen(oldX.str_) + 1];
            strcpy(str_, oldX.str_);
        }
    }
    catch (...)
    {
        clear();
    }
}

SpatialException &SpatialException::operator=(const SpatialException &oldX) throw()
{
    try
    {
        if (&oldX != this) // beware of self-assignment
        {
            if (oldX.str_)
            {
                str_ = new char[slen(oldX.str_) + 1];
                strcpy(str_, oldX.str_);
            }
        }
    }
    catch (...)
    {
        clear();
    }
    return *this;
}

const char *SpatialException::what() const throw()
{
    try
    {
        return str_;
    }
    catch (...)
    {
        return "";
    }
}

int SpatialException::slen(const char *str) const
{
    if (str)
        return strlen(str);
    return 0;
}

void SpatialException::clear()
{
    delete[] str_;
    str_ = nullptr;
}
/* --- SpatialUnimplemented methods --------------------------------------------- */

SpatialUnimplemented::SpatialUnimplemented(const char *cstr) throw() : SpatialException(cstr, UNIMPLEMENTED)
{
}

SpatialUnimplemented::SpatialUnimplemented(const char *context, const char *because) throw()
    : SpatialException(context, because, UNIMPLEMENTED)
{
}

SpatialUnimplemented::SpatialUnimplemented(const SpatialUnimplemented &oldX) throw() : SpatialException(oldX)
{
}

/* --- SpatialFailure methods --------------------------------------------------- */

SpatialFailure::SpatialFailure(const char *cstr) throw() : SpatialException(cstr, FAILURE)
{
}

SpatialFailure::SpatialFailure(const char *context, const char *because) throw()
    : SpatialException(context, because, FAILURE)
{
}

SpatialFailure::SpatialFailure(const char *context, const char *operation, const char *resource,
                               const char *because) throw()
{
    try
    {
        clear();
        if (!operation && !resource && !because)
        {
            if (!context)
                context = defaultstr[CONTEXT];
            because = "failed operation";
        }
        size_t const len = slen(context) + slen(operation) + slen(resource) + slen(because) + 50;
        str_  = new char[len];
        *str_ = '\0';
        char * ptr = str_;
        if (!context)
            context = defaultstr[CONTEXT];
        ptr += sprintf(ptr, "%s: ", context);
        if (operation)
        {
            ptr += sprintf(ptr, " %s failed ", operation);
        }
        if (resource)
        {
            if (operation)
                ptr += sprintf(ptr, " on \"%s\"", resource);
            else
                ptr += sprintf(ptr, " trouble with \"%s\"", resource);
        }
        if (because)
        {
            if (operation || resource)
                ptr += sprintf(ptr, " because %s", because);
            else
                ptr += sprintf(ptr, " %s", because);
        }
    }
    catch (...)
    {
        clear();
    }
}

SpatialFailure::SpatialFailure(const SpatialFailure &oldX) throw() : SpatialException(oldX)
{
}

/* --- SpatialBoundsError methods ----------------------------------------------- */

SpatialBoundsError::SpatialBoundsError(const char *cstr) throw() : SpatialException(cstr, BOUNDS)
{
}

SpatialBoundsError::SpatialBoundsError(const char *context, const char *array, int32 limit, int32 index) throw()
    : SpatialException(context, array, BOUNDS)
{
    try
    {
        if (limit != -1)
        {
            char * ptr = str_;
            if (array)
                ptr += sprintf(ptr, "[%d]", index);
            else
                ptr += sprintf(ptr, " array index %d ", index);
            if (index > limit)
            {
                ptr += sprintf(ptr, " over upper bound by %d", index - limit);
            }
            else
            {
                ptr += sprintf(ptr, " under lower bound by %d", limit - index);
            }
        }
    }
    catch (...)
    {
        clear();
    }
}

SpatialBoundsError::SpatialBoundsError(const SpatialBoundsError &oldX) throw() : SpatialException(oldX)
{
}

/* --- SpatialInterfaceError methods -------------------------------------------- */

SpatialInterfaceError::SpatialInterfaceError(const char *cstr) throw() : SpatialException(cstr, INTERFACE)
{
}

SpatialInterfaceError::SpatialInterfaceError(const char *context, const char *because) throw()
    : SpatialException(context, because, INTERFACE)
{
}

SpatialInterfaceError::SpatialInterfaceError(const char *context, const char *argument, const char *because) throw()
{
    try
    {
        clear();
        str_  = new char[slen(context) + slen(argument) + slen(because) + 128];
        *str_ = '\0';
        char * ptr = str_;
        if (!context)
            context = defaultstr[CONTEXT];
        ptr += sprintf(ptr, "%s: ", context);
        if (argument && because)
        {
            ptr += sprintf(ptr, " argument \"%s\" is invalid because %s ", argument, because);
        }
        else if (argument && !because)
        {
            ptr += sprintf(ptr, " invalid argument \"%s\" ", argument);
        }
        else if (!argument)
        {
            if (because)
                ptr += sprintf(str_, " %s", because);
            else
                ptr += sprintf(str_, " interface violation");
        }
    }
    catch (...)
    {
        clear();
    }
}

SpatialInterfaceError::SpatialInterfaceError(const SpatialInterfaceError &oldX) throw() : SpatialException(oldX)
{
}
