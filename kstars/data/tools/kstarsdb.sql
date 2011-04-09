PRAGMA foreign_keys = ON
PRAGMA synchronous = OFF

CREATE TABLE ctg (name TEXT, source TEXT, description TEXT)
CREATE TABLE cnst (shortname TEXT, latinname TEXT, boundary TEXT, description TEXT)
CREATE TABLE dso (ra TEXT, dec TEXT, sgn INTEGER, bmag FLOAT, type INTEGER, pa FLOAT, minor FLOAT, major FLOAT, longname TEXT)
CREATE TABLE od (designation TEXT, idCTG INTEGER NOT NULL, idDSO INTEGER NOT NULL)
CREATE INDEX idCTG ON od(idCTG)
CREATE INDEX idDSO ON od(idDSO)
CREATE INDEX designation ON od(designation)
CREATE TABLE dso_url (idDSO INTEGER NOT NULL, title TEXT, url TEXT, type INTEGER)
CREATE INDEX idDSOURL ON dso_url(idDSO)

INSERT INTO ctg VALUES("NGC", "ngcic.dat", "New General Catalogue")
INSERT INTO ctg VALUES("IC", "ngcic.dat", "Index Catalogue of Nebulae and Clusters of Stars")
INSERT INTO ctg VALUES("M", "ngcic.dat", "Messier Catalogue")
INSERT INTO ctg VALUES("PGC", "ngcic.dat", "Principal Galaxies Catalogue")
INSERT INTO ctg VALUES("UGC", "ngcic.dat", "Uppsala General Catalogue")
