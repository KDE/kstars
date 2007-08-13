#ifndef VARSTR_H
#define VARSTR_H
//#     Filename:       VarStr.h
//#
//#     Variable string class and tokenizer
//#
//#
//#     Author:         Peter Z Kunszt    
//#     
//#     Creation Date:  August 2000
//#
//#		Copyright (C) 2000  Peter Z. Kunszt, Alex S. Szalay, Aniruddha R. Thakar
//#                     The Johns Hopkins University
//#
//# Modification history:
//#

#ifndef _BOUNDS_EXCEPTION

#ifdef SXDB
#   include <sxException.h>
#   define _BOUNDS_EXCEPTION sxBoundsError
#   define _INTERFACE_EXCEPTION sxInterfaceError
#else
#   include <SpatialException.h>
#   define _BOUNDS_EXCEPTION SpatialBoundsError
#   define _INTERFACE_EXCEPTION SpatialInterfaceError
#endif

#endif

#include <sys/types.h>
#include <stdio.h>
#include <iostream>

// begin add dcd
#include <vector>
#include <string>
#include <algorithm>

/** Dynamic string.

    This is a template for a general-purpose dynamic string.  The
    array grows automatically as needed, but reallocation occurs only
    when the length exceeds the capacity.  The capacity is increased
    in large blocks, the size of which may be optimized.  The public
    data member, increment_, specifies the amount by which the
    capacity is increased during reallocation.  By default, increment_
    is zero, which causes the capacity to double upon each
    reallocation.  A non-zero increment_ is simply added to the
    capacity upon each reallocation.  The capacity is extended by this
    amount or by whatever greater amount is necessary to accommodate
    the new length of the array.
*/

class LINKAGE VarStr {
public:
  /** Destructor. */

  ~VarStr( void );

  /** Default constructor.
      optionally specify initial capacity and reallocation increment.
  */

  VarStr( size_t capacity = 0, size_t increment = 0 );

  /** Construct from a string */

  VarStr( const char * );

  /** Copy constructor.  
      The initial capacity is the current capacity of the duplicated array.*/

  VarStr( const VarStr& );

  /** Assignment/copy operator. does not decrease the capacity. */
  //@{
  VarStr&	operator =( const VarStr& );
  VarStr&	operator =( const char * );
  VarStr&	operator =( const char );
  VarStr&	operator =( const int );
  //@}

  /** Efficient array operator (const version): no bounds checking. */

  char		operator ()( size_t index ) const { return vector_[index]; }

  /** Efficient array operator (non-const version): no bounds checking. */

  char&	operator ()( size_t index ) { return vector_[index]; }

  /** Bounds-checking array operator (const version): throws sxBoundsError.*/

  char		operator []( size_t index ) const;

  /** Bounds-checking array operator (non-const version): throws sxBoundsError.
   */

  char&	operator []( size_t index );

  /** Comparison operators */
  //@{
  int          operator == ( const VarStr &) const;
  int          operator == ( const char *) const;
  int          operator != ( const VarStr &) const;
  int          operator != ( const char *) const;
  //@}

  /** String extension */
  //@{
  friend VarStr &          operator + ( const VarStr &, const VarStr &);
  friend VarStr &          operator + ( const VarStr &, const char * );
  friend VarStr &          operator + ( const char *, const VarStr & );
  //@}

  /** String extension */
  //@{
   VarStr &          operator += ( const VarStr &);
   VarStr &          operator += ( const char *);
   VarStr &          operator += ( const char );
   VarStr &          operator += ( const int32 );
   VarStr &          operator += ( const uint32 );
   VarStr &          operator += ( const int64 );
   VarStr &          operator += ( const uint64 );
   VarStr &          operator += ( const float32 );
   VarStr &          operator += ( const float64 );
  //@}

  /** Binary string extension, represented by operator "*=" */
  //@{
   VarStr &          operator *= ( const uint8 );
   VarStr &          operator *= ( const int16 );
   VarStr &          operator *= ( const uint16 );
   VarStr &          operator *= ( const int32 );
   VarStr &          operator *= ( const uint32 );
   VarStr &          operator *= ( const int64 );
   VarStr &          operator *= ( const uint64 );
   VarStr &          operator *= ( const float32 );
   VarStr &          operator *= ( const float64 );
  //@}

  /** Char conversion */
  operator char* () const { return vector_; }

  /** Bounds-adjusting array operator.  Returns the array
      element at the specified index, extending the array as necessary
      to bring it within bounds.  The fill value, if defined, is the
      initializer for any new elements. */

  char&	at( size_t index );

  /** Returns current occupied length of array */

  size_t	length( void ) const { return length_; }

  /** Append method. efficiently insert given element at end of array.
      Avoids redundant initialization of new array element, except for
      when a reallocation is required.  Returns the new length. */

  size_t	append( const char );

  /** Append method for binary data.  Adds the contents of the given 
      buffer to the string byte by byte. Returns the new length. */

  size_t	append( unsigned char *buf, const int len );

  /** Insert new array elements.  
      count specifies the number of new elements, and offset specifies
      where in the array to insert them.  By default the new elements
      are appended.  The fill value, if defined, is the initializer
      for the new elements.  offset refers to the end of the array:
      the first new element is located at index (length - offset).
      Returns the new length.  Throws sxBoundsError if offset is
      greater than length.*/

  size_t	insert( size_t count, size_t offset = 0, char c = ' ' );

  /** Remove array elements.
      count specifies the number of elements to remove, and offset
      specifies which elements to remove.  By default elements are
      removed from the end of the array.  The unused capacity grows by
      this amount.  offset refers to the end of the array: the first
      removed element is located at index (length - offset - count).
      Returns the new length.  Throws sxBoundsError if (offset+count)
      is greater than length. */

  size_t	cut( size_t count, size_t offset = 0 );

  /** Removes the element specified by offset.
      This is basically a wrapper for the cut method 
      <pre>
      cut(1, length-offset-1)
      </pre>
  */

  void		remove( size_t offset, size_t count = 1 );

  /** Write out the contents as a binary buffer.  Use the low-level stream
      write function.
  */

  void write( std::ostream& _out ) const;

  /** clear method */

  void		clear( void );

  /** return the string itself */
  char *	data() const;

  /** return true if string is empty, false if not */
  bool		empty() const;

private:
  char				*vector_;	// dynamic array of values
  size_t			increment_;     // linear growth increment
  size_t			length_;	// occupied length of vector
  size_t			capacity_;	// allocated length of vector

  friend class VarStrToken;
};

//dcdtmp 
 VarStr &          operator + ( const VarStr &, const VarStr &);
 VarStr &          operator + ( const VarStr &, const char * );
 VarStr &          operator + ( const char *, const VarStr & );

/** Dynamic string tokenizer

    This class tokenizes the dynamic string VarStr. It returns the
    tokens one by one on request. It can also tokenize a standard
    string..
*/

 //dcdtmp switch back to VarStr
typedef VarStr StdStr;

class VarStrToken {
public:
  /** Constructor. Needs to get the VarStr that you want to tokenize*/

  //dcdtmp VarStrToken( const VarStr & );
  VarStrToken( const StdStr & );

  // Construct from a standard string

  VarStrToken( const char * );

  /** Destructor. */

  ~VarStrToken( void );

  /** Get next token. You can optionally specify
      the characters that serve as delimiters. The default is whitespace. */
  //dcdtmp const VarStr & next( const char * = NULL );
  const StdStr & next( const char * = NULL );

private:
  char * save_;
  char * str_;
  char * delimiters_;
  bool start_;
  StdStr token_;
};

#include "VarStr.hxx"
#endif /* VARSTR_H */
