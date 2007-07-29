//#     Filename:       VarStr.cpp
//#
//#     VarStr class functions
//#
//#     Author:         Peter Z Kunszt
//#     
//#     Date:           July 4 2000
//#
//#		Copyright (C) 2000  Peter Z. Kunszt, Alex S. Szalay, Aniruddha R. Thakar
//#                     The Johns Hopkins University
//#

/* --- VarStr methods ------------------------------------------------------ */
#include "SpatialGeneral.h"
#include "VarStr.h"
#include <string.h>

// destructor
VarStr::~VarStr( void ) {
  if ( vector_ )
    free( vector_ );
}

// default constructor

VarStr::VarStr( size_t capacity, size_t increment ) :
  vector_(NULL), increment_(0), length_(0), capacity_(0)
{
  insert( capacity );
  increment_ = increment;
  length_ = 0;
}

// construct with a string

VarStr::VarStr( const char * str ) : 
  vector_(NULL), increment_(0), length_(0), capacity_(0)
{
  *this += str;
}

// copy constructor

VarStr::VarStr( const VarStr &orig )
{
   capacity_  = orig.capacity_;
   increment_ = orig.increment_;
   length_    = orig.length_;
   vector_    = 0;
   if(orig.vector_)vector_ = (char *)malloc( orig.capacity_ );
   memcpy( vector_, orig.vector_, capacity_ );

}

// assignment/copy operator

VarStr&	VarStr::operator =( const VarStr &orig )
{
   if ( &orig == this ) return *this;

   capacity_  = orig.capacity_;
   increment_ = orig.increment_;
   length_    = orig.length_;
   if(vector_)free( vector_ );
   vector_ = 0;
   if(orig.vector_)vector_ = (char *)malloc( orig.capacity_ );
   memcpy( vector_, orig.vector_, capacity_ );

   return *this;
}

VarStr&	VarStr::operator =( const char *orig ) {
   clear();
   *this += orig;
   return *this;
}

VarStr&	VarStr::operator =( const char orig ) {
   clear();
   *this += orig;
   return *this;
}

VarStr&	VarStr::operator =( const int orig ) {
   clear();
   *this += orig;
   return *this;
}

// bounds-checking array operator (const version)

char	VarStr::operator []( size_t index ) const
{
   if ( index >= length_ )
      throw _BOUNDS_EXCEPTION( "VarStr", "vector_", length_, index );
   return vector_[index];
}

// bounds-checking array operator (non-const version)

char&	VarStr::operator []( size_t index )	
{
   if ( index >= length_ )
      throw _BOUNDS_EXCEPTION( "VarStr", "vector_", length_, index );
   return vector_[index];
}

// comparison operator

int	VarStr::operator ==( const VarStr & orig ) const
{
   if ( length_ == orig.length_ && vector_ && orig.vector_ )
     return ( memcmp( vector_, orig.vector_, length_ ) ? 0 : 1 );
   return (length_ - orig.length_ == 0 ? 1 : 0);
}

// comparison operator : two empty strings are considered equal.

int	VarStr::operator ==( const char * orig ) const
{
   if ( vector_ && orig )
     return ( strcmp( vector_, orig ) == 0 ? 1 : 0 );
   if ( orig )
     return strlen(orig) ? 0 : 1;
   if( vector_ )
     return length_ ? 0 : 1;
   return 1;
}

// comparison operator

int	VarStr::operator !=( const VarStr & orig ) const
{
   if ( length_ == orig.length_ && vector_ && orig.vector_ )
     return ( memcmp( vector_, orig.vector_, length_ ) ? 1 : 0 );
   return (length_ - orig.length_ == 0 ? 0 : 1);
}

// comparison operator : two empty strings are considered equal.

int	VarStr::operator !=( const char * orig ) const
{
   if ( vector_ && orig )
     return ( strcmp( vector_, orig ) == 0 ? 0 : 1 );
   if ( orig )
     return strlen(orig) ? 1 : 0;
   if( vector_ )
     return length_ ? 1 : 0;
   return 0;
}

// extension operator

VarStr & VarStr::operator +=( const VarStr & orig )
{
   size_t len = length_;
   at( length_ + orig.length_ - 1 );
   memcpy( vector_+len, orig.vector_, orig.length_ );
   at( length_ ) = 0; length_--;

   return *this;
}

// extension operator

VarStr & VarStr::operator +=( const char * orig )
{
   if( orig == NULL) return *this;
   size_t len = length_, len2 = strlen(orig);
   at( length_ + len2 - 1 );
   memcpy( vector_+len, orig, len2 );
   at( length_ ) = 0; length_--;

   return *this;
}

// extension operator

VarStr & VarStr::operator +=( const char orig )
{
   at( length_ ) = orig;
   at( length_ ) = 0; length_--;

   return *this;
}

VarStr & VarStr::operator +=( const int32 orig )
{
   char str[50];
   sprintf(str,"%d",orig);
   *this += str;

   return *this;
}

VarStr & VarStr::operator +=( const uint32 orig )
{
   char str[50];
   sprintf(str,"%u",orig);
   *this += str;

   return *this;
}

VarStr & VarStr::operator +=( const int64 orig )
{
   char str[50];
#ifdef SpatialWinNT
   sprintf(str,"%I64d",orig);
#else
   sprintf(str,"%lld",orig);
#endif
   *this += str;

   return *this;
}

VarStr & VarStr::operator +=( const uint64 orig )
{
   char str[50];
#ifdef SpatialWinNT
   sprintf(str,"%I64u",orig);
#else
   sprintf(str,"%llu",orig);
#endif
   *this += str;

   return *this;
}

VarStr & VarStr::operator +=( const float32 orig )
{
   char str[50];
   sprintf(str,"%1.8g",orig);
   *this += str;

   return *this;
}

VarStr & VarStr::operator +=( const float64 orig )
{
   char str[50];
   sprintf(str,"%1.15g",orig);
   *this += str;

   return *this;
}

// extension operator

VarStr & operator +( const VarStr &one, const VarStr &two )
{
   VarStr *res = new VarStr(one);
   *res += two;
   return *res;
}

VarStr & operator +( const VarStr &one, const char * two )
{
   VarStr *res = new VarStr(one);
   *res += two;
   return *res;
}

VarStr & operator +( const char * two, const VarStr &one )
{
   VarStr *res = new VarStr(one);
   *res += two;
   return *res;
}

// at method: bounds-adjusting array operator

char&	VarStr::at( size_t index )
{
   if ( index >= length_ ) insert( 1 + index - length_ );
   return vector_[index];
}

// append method: efficiently insert element at end of array

size_t	VarStr::append( const char t )
{
   (length_ < capacity_ ? vector_[length_++] : at(length_)) = t;
   return length_;
}

// insert method: insert and initialize new array elements

size_t	VarStr::insert( size_t count, size_t offset, char c )
{
   if ( offset > length_ )
      throw _BOUNDS_EXCEPTION("VarStr::insert","offset greater than length");

   size_t newLength	= length_ + count;
   size_t start		= length_ - offset;
   size_t i;

   if ( newLength > capacity_ ) {
      // allocate new vector
      size_t cap = increment_ ? capacity_ + increment_ : 2 * capacity_;
      if ( newLength > cap ) cap = newLength;
      char *vec = (char*) malloc( cap );

      // bitwise copy original occupied region into new vector
      if ( length_ ) {
	 memcpy( vec, vector_, start );
	 memcpy( vec + start + count, vector_ + start, offset );
      }

      for ( i = 0; i < count; ++i ) vec[start+i] = c;

      // construct new unoccupied region with default
      for ( i = newLength; i < cap; ++i ) vec[i] = 0;

      // replace old vector with new vector
      char *oldVec = vector_;
      vector_ = vec;
      capacity_ = cap;

      // destroy original unoccupied region and free discarded vector
      if ( oldVec )
	 free( oldVec );
   }
   else if ( count )
      if ( offset ) {
	// bitwise move displaced portion of occupied region
	memmove(vector_+start+count, vector_+start, offset );

	// construct vacated region with fill or default
	for ( i = 0; i < count; ++i ) vector_[start+i] = c;
      } else 
	for ( i = 0; i < count; ++i ) vector_[length_+i] = c;

   return length_ = newLength;
}

// cut method: remove array elements

size_t	VarStr::cut( size_t count, size_t offset )
{
   if ( count + offset > length_ )
      throw _BOUNDS_EXCEPTION("VarStr::cut","count+offset greater than length");

   if ( count && offset ) {
     size_t i;
     char *start = vector_ + length_ - offset - count;

     // bitwise move displaced portion of occupied region
     memmove( start, start + count, offset );

     // construct vacated region with default
     for ( i = 0; i < count; ++i ) start[offset+i] = 0;
   }
   return length_ -= count;
}

// clear method

void	VarStr::clear( void )
{
  for(size_t i = 0; i < length_; i++)
    vector_[i] = 0;
  length_ = 0;
}

// remove method: call cut

void	VarStr::remove( size_t offset, size_t n )
{
   if ( offset >= length_ )
      throw _BOUNDS_EXCEPTION("VarStr::remove","count greater than length");

   cut(n, length_ - offset - 1);
   return;
}


///////////////////////////////TOKENIZER///////////////////////
// constructors

VarStrToken::VarStrToken( const StdStr & vstr ) : 
  delimiters_(NULL), start_(true) {
  str_ = new char[ vstr.length() + 1];
  strcpy( str_, vstr.vector_ );
}

// Construct from a standard string

VarStrToken::VarStrToken( const char *str ) : 
  delimiters_(NULL), start_(true) {
  str_ = new char[ strlen(str) + 1 ];
  strcpy( str_, str );
}

/** Destructor. */

VarStrToken::~VarStrToken( void ) {
  delete[] str_;
  if(delimiters_) delete[] delimiters_;
}

/** Get next token. You can optionally specify
    the characters that serve as delimiters. The default is whitespace. */
const StdStr &
VarStrToken::next( const char *d ) {

  char *s = NULL;

  if(d) {
    if(delimiters_)delete[] delimiters_;
    delimiters_ = new char[ strlen(d) + 1 ];
    strcpy( delimiters_, d );
    if(start_) {
      start_ = false;
      s = str_;
    }
  } else if(start_) {
    delimiters_ = new char[5];
    strcpy( delimiters_, " \t\n\r");
    start_ = false;
    s = str_;
  }

#if defined(_WIN32) || defined(__macosx)
  // Windows claims its strtok is thread-safe
  token_ = strtok( s, delimiters_ );
#else
  token_ = strtok_r( s, delimiters_, &save_ );
#endif
  return token_;

}

