#!/usr/bin/python
import sqlite3
conn = sqlite3.connect('../skycomponents.db')
cur = conn.cursor()
try:
	cur.execute('''DROP TABLE ObjectDesignation''')
except:
	pass
try:
	cur.execute('''DROP TABLE Catalog''')
except:
	pass
try:
	cur.execute('''DROP TABLE DSO''')
except:
	pass

cur.execute('''CREATE TABLE ObjectDesignation (
		id INTEGER NOT NULL  DEFAULT NULL PRIMARY KEY,
		id_Catalog INTEGER DEFAULT NULL REFERENCES Catalog (id),
		UID_DSO INTEGER DEFAULT NULL REFERENCES DSO (UID),
		LongName MEDIUMTEXT DEFAULT NULL,
		IDNumber INTEGER DEFAULT NULL
		);
		''');

cur.execute('''CREATE TABLE Catalog (
		id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT,
		Name CHAR NOT NULL  DEFAULT 'NULL',
		Prefix CHAR DEFAULT NULL,
		Color CHAR DEFAULT '#CC0000',
		Epoch FLOAT DEFAULT 2000.0,
		Author CHAR DEFAULT NULL,
		License MEDIUMTEXT DEFAULT NULL
		);
		''');

cur.execute('''CREATE TABLE DSO (
		UID INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT,
		RA DOUBLE NOT NULL  DEFAULT 0.0,
		Dec DOUBLE DEFAULT 0.0,
		Type INTEGER DEFAULT NULL,
		Magnitude DECIMAL DEFAULT NULL,
		PositionAngle INTEGER DEFAULT NULL,
		MajorAxis FLOAT NOT NULL  DEFAULT NULL,
		MinorAxis FLOAT DEFAULT NULL,
		Flux FLOAT DEFAULT NULL,
		Add1 VARCHAR DEFAULT NULL,
		Add2 INTEGER DEFAULT NULL,
		Add3 INTEGER DEFAULT NULL,
		Add4 INTEGER DEFAULT NULL
		);
		''');


