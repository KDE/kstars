//#     Filename:       VarStr.hxx
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

//* --- VarStr methods ------------------------------------------------------ */
#include <stdlib.h>
#include <iostream>

inline
char *	VarStr::data() const{
  return vector_;
}

inline
bool 	VarStr::empty() const {
  return ( length_ == 0 ? true : false );
}

/////////////<<///////////////////////////////////////////
// write to ostream
//
inline
std::ostream& operator <<( std::ostream& out, const VarStr & s) {
  return(  out << s.data() );
}


// binary mode string extension
   
inline VarStr& 
VarStr::operator *=( const uint8 num )
{
   append( ((unsigned char *)&num), sizeof(num) );
   return *this;
}

// extension operator for short int

inline VarStr& 
VarStr::operator *=( const int16 num )
{
   append( ((unsigned char *)&num), sizeof(num) );
   return *this;
}

// extension operator for unsigned short int

inline VarStr& 
VarStr::operator *=( const uint16 num )
{
   append( ((unsigned char *)&num), sizeof(num) );
   return *this;
}

// extension operator for integer

inline VarStr& 
VarStr::operator *=( const int32 num )
{
   append( ((unsigned char *)&num), sizeof(num) );
   return *this;
}

// extension operator for unsigned integer

inline VarStr& 
VarStr::operator *=( const uint32 num )
{
   append( ((unsigned char *)&num), sizeof(num) );
   return *this;
}

// extension operator for long int

inline VarStr& 
VarStr::operator *=( const int64 num )
{
   append( ((unsigned char *)&num), sizeof(num) );
   return *this;
}

// extension operator for unsigned long int

inline VarStr& 
VarStr::operator *=( const uint64 num )
{
   append( ((unsigned char *)&num), sizeof(num) );
   return *this;
}

// extension operator for float

inline VarStr& 
VarStr::operator *=( const float32 num )
{
   append( ((unsigned char *)&num), sizeof(num) );
   return *this;
}

// extension operator for double

inline VarStr& 
VarStr::operator *=( const float64 num )
{
   append( ((unsigned char *)&num), sizeof(num) );
   return *this;
}

// append method: efficiently insert element at end of array

inline size_t	
VarStr::append( unsigned char *pBuf, const int len )
{
   for( int i = 0; i < len; i++ )
     (length_ < capacity_ ? vector_[length_++] : at(length_)) = pBuf[ i ];
   return length_;
}


inline void
VarStr::write( std::ostream& _out ) const {
  _out.write( vector_, length_ );
  _out.flush();
}

