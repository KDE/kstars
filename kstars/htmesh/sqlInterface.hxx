//#     Filename:       sqlInterface.hxx
//#
//#     Interface inline methods
//#
//#     Author:         Peter Z. Kunszt
//#     
//#     Date:           August 31 2000
//#
//#		Copyright (C) 2000  Peter Z. Kunszt, Alex S. Szalay, Aniruddha R. Thakar
//#                     The Johns Hopkins University
//#

inline
bool htmSqlInterface::err() {
  return err_;
}

inline
MsgStr htmSqlInterface::error() {
  return error_.data();
}
