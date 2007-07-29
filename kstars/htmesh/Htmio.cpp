
#include <Htmio.h>

#define COMMENT '#'

/////////////READ/////////////////////////////////////////
//
void
Htmio::read(std::istream &in, RangeConvex &rc) {
  size_t nconstr;
  SpatialConstraint constr;

  in.setf(std::ios::skipws);
  while(in.peek() == COMMENT)  // ignore comments
      in.ignore(10000,'\n');
  in >> nconstr ; in.ignore(); // ignore endl
  if(!in.good())
    throw SpatialFailure("Htmio:read: Could not read constraint");
  for(size_t i = 0; i < nconstr; i++) {
    if(in.eof())
      throw SpatialFailure("Htmio:read: Premature end-of-file");
    in >> constr;
  if(!in.good())
    throw SpatialFailure("Htmio:read: Could not read constraint");
    rc.add(constr);
  }
}

/////////////READ/////////////////////////////////////////
//
void
Htmio::readRaDec(std::istream &in, RangeConvex &rc) {
  size_t nconstr;
  SpatialConstraint constr;

  while(in.peek() == COMMENT)  // ignore comments
      in.ignore(10000,'\n');
  in >> nconstr ; in.ignore(); // ignore endl
  for(size_t i = 0; i < nconstr; i++) {
    readRaDec(in, constr);
    // was: constr.readRaDec(in);
    rc.add(constr);
  }
}

/////////////WRITE////////////////////////////////////////
//
void
Htmio::write(std::ostream &out, const RangeConvex &rc)  {
  out << "#CONVEX" << std::endl;
  out << rc.constraints_.size() << std::endl;
  for (size_t i = 0; i < rc.constraints_.size() ; i++)
    out << rc.constraints_[i];
}

/////////////>>///////////////////////////////////////////
// read from istream
//
std::istream& operator >>( std::istream& in, RangeConvex & c) {
  Htmio::read(in, c);
  return(in);
}

void
Htmio::ignoreCrLf(std::istream &in)
{
  char c = in.peek();
  while (c == 10 || c == 13) {
    in.ignore();
    c = in.peek();
  }
}


/////////////READ/////////////////////////////////////////
//
void
Htmio::read(std::istream &in, SpatialDomain &sd) {
  size_t nconv;
  char comstr[20];

  while(in.peek() == COMMENT)  // ignore comments
      in.ignore(10000,'\n');
  in >> nconv; 
  ignoreCrLf(in);
  for(size_t i = 0; i < nconv; i++) {
    comstr[0] = '\0';
    if(in.peek() == COMMENT) // here comes a command
      in >> comstr;

    if(strcmp(comstr,"#TRIANGLE")==0) {
      SpatialVector v1,v2,v3;
      in >> v1;
      in >> v2;
      in >> v3;
      RangeConvex cvx(&v1,&v2,&v3);
      sd.add(cvx);
      ignoreCrLf(in);
    } else if(strcmp(comstr,"#RECTANGLE")==0) {
      SpatialVector v1,v2,v3,v4;
      in >> v1;
      in >> v2;
      in >> v3;
      in >> v4;
      RangeConvex cvx(&v1,&v2,&v3,&v4);
      sd.add(cvx);
      ignoreCrLf(in);
    } else if(strcmp(comstr,"#TRIANGLE_RADEC")==0) {
      float64 ra1,ra2,ra3;
      float64 dec1,dec2,dec3;
      in >> ra1 >> dec1;
      in >> ra2 >> dec2;
      in >> ra3 >> dec3;
      SpatialVector v1(ra1,dec1);
      SpatialVector v2(ra2,dec2);
      SpatialVector v3(ra3,dec3);
      RangeConvex cvx(&v1,&v2,&v3);
      sd.add(cvx);
      ignoreCrLf(in);
    } else if(strcmp(comstr,"#RECTANGLE_RADEC")==0) {
      float64 ra1,ra2,ra3,ra4;
      float64 dec1,dec2,dec3,dec4;
      in >> ra1 >> dec1;
      in >> ra2 >> dec2;
      in >> ra3 >> dec3;
      in >> ra4 >> dec4;
      SpatialVector v1(ra1,dec1);
      SpatialVector v2(ra2,dec2);
      SpatialVector v3(ra3,dec3);
      SpatialVector v4(ra4,dec4);
      RangeConvex cvx(&v1,&v2,&v3,&v4);
      sd.add(cvx);
      ignoreCrLf(in);		// added,gyuri,11/12/2002
    } else  if(strcmp(comstr,"#CONVEX_RADEC")==0) {
      RangeConvex conv;
      Htmio::readRaDec(in, conv);
      sd.add(conv);
      ignoreCrLf(in);		// added,gyuri,11/12/2002
    } else {
      RangeConvex conv;
      in >> conv;
      sd.add(conv);
      ignoreCrLf(in);		// added,gyuri,11/12/2002
    }
    comstr[0] = 0;
  }
}

/////////////READ/////////////////////////////////////////
//
void
Htmio::read(std::istream &in, SpatialConstraint &sc) {

  in.setf(std::ios::skipws);
  while(in.peek() == COMMENT)  // ignore comments
      in.ignore(10000,'\n');
  in >> sc.a_ >> sc.d_ ;
  if(!in.good())
    throw SpatialFailure("SpatialConstraint:read: Could not read constraint");
  sc.a_.normalize();
  sc.s_ = acos(sc.d_);
  if     (sc.d_ <= -gEpsilon) sc.sign_ = sc.nEG;
  else if(sc.d_ >=  gEpsilon) sc.sign_ = sc.pOS;
  else                sc.sign_ = sc.zERO;
}

/////////////READ/////////////////////////////////////////
//
void
Htmio::readRaDec(std::istream &in, SpatialConstraint &sc) {
  while(in.peek() == COMMENT)  // ignore comments
      in.ignore(10000,'\n');
  float64 ra,dec;
  in >> ra >> dec >> sc.d_ ; in.ignore();
  sc.a_.set(ra,dec);
  sc.s_ = acos(sc.d_);
  if     (sc.d_ <= -gEpsilon) sc.sign_ = sc.nEG;
  else if(sc.d_ >=  gEpsilon) sc.sign_ = sc.pOS;
  else                sc.sign_ = sc.zERO;
}

