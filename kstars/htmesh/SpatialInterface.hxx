//#     Filename:       SpatialInterface.hxx
//#
//#     Interface inline methods
//#
//#     Author:         Peter Z. Kunszt, based on A. Szalay's code
//#     
//#     Date:           October 15, 1998
//#
//#		Copyright (C) 2000  Peter Z. Kunszt, Alex S. Szalay, Aniruddha R. Thakar
//#                     The Johns Hopkins University
//#

///////////LOOKUP METHODS////////////////////

inline
uint64 htmInterface::lookupID(float64 ra, float64 dec) const {
  return index_->idByPoint(ra,dec);
}

inline
uint64 htmInterface::lookupID(float64 x, float64 y, float64 z) const {
  SpatialVector v(x,y,z);
  return index_->idByPoint(v);
}

inline
uint64 htmInterface::lookupID(char *nm) const {
  return index_->idByName(nm);
}

inline
const char * htmInterface::lookupName(float64 ra, float64 dec) {
  index_->nameByPoint(ra,dec,name_);
  return name_;
}

inline
const char * htmInterface::lookupName(float64 x, float64 y, float64 z) {
  SpatialVector v(x,y,z);
  index_->nameByPoint(v,name_);
  return name_;
}

inline
const char * htmInterface::lookupName(uint64 id) {
  index_->nameById(id,name_);
  return name_;
}

inline 
void htmInterface::pointById(SpatialVector &vec, uint64 htmid) const {
	index_->pointById(vec, htmid);
	return;
}
//////////OTHERS/////////////////////////////

// GYF
inline 
size_t htmInterface::adjustDepthToID(uint64 htmid){

	size_t newdepth = 0;
	uint64 shiftme = htmid >> 4;
	for(shiftme = htmid >> 4; shiftme != 0; shiftme >>= 2){
		newdepth++;
	}

	delete index_;
	index_ = new SpatialIndex(newdepth);
	return newdepth;
}
inline
void htmInterface::changeDepth(size_t depth, size_t saveDepth) {
  if(index_->maxlevel_ != depth || index_->buildlevel_ != saveDepth) {
	  delete index_;
    index_ = new SpatialIndex(depth, saveDepth);
  }
}


inline
const SpatialIndex & htmInterface::index() const {
  return *index_;
}
