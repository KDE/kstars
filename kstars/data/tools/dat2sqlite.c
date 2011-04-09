/***************************************************************************
       dat2sqlite.c - Convert ngcic.dat file into a new SQLite database
                             -------------------
    begin                : Fri April 08 2011
    copyright            : (C) 2011 by Victor Carbune
    email                : victor.carbune@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>

#include <QSqlDatabase>
#include "ksfilereader.h"

#define MAX_LINE_LENGTH 200

#define CTG_NGC	0
#define CTG_IC	1
#define CTG_M	2
#define CTG_UGC	3
#define CTG_PGC	4

#define CTG_NO	5
/**
 * Global array providing correlation between catalogs and indexes in
 * the database. Example catalogues[CTG_NGC] represents the ID in the
 * database of the NGC catalogue.
 */
int catalogues[5];

/**
 * Open the database where the information will be copied.
 */
sqlite3 *open_database(const char *name)
{
	sqlite3 *db;

	int rc = sqlite3_open(name, &db);
	if (rc) {
		fprintf(stderr, "Unable to open kstars.db file."
				"Check permissions?\n");
		sqlite3_close(db);

		exit(1);
	}

	return db;
}

/**
 * Execute an SQL statement on the database and check if it returns an error.
 */
void configure_kstarsdb(sqlite3 *db, const char *config_sql)
{
	int rc;
	char *err_msg = NULL;

	rc = sqlite3_exec(db, config_sql, NULL, 0, &err_msg);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL Error: %s\n", err_msg);
		sqlite3_free(err_msg);
	}
}

/**
 * Initialize the KStarsDB, by creating the default database structure.
 * SQL Statements for initial configuration are loaded from a file,
 * given as parameter.
 */
void initialize_kstarsdb(sqlite3 *db, const char *sql_file)
{
	FILE *config_file = fopen(sql_file, "r");
	char *p, *err_msg, config_sql[MAX_LINE_LENGTH];

	do {
		p = fgets(config_sql, MAX_LINE_LENGTH, config_file);

		if (p != NULL) {
			int rc = sqlite3_exec(db, config_sql, NULL, 0, &err_msg);
			if (rc != SQLITE_OK) {
				fprintf(stderr, "SQL Error: %s\n", err_msg);
				sqlite3_free(err_msg);
			}
		}
	} while (p != NULL);
}

int register_catalogue(void *catalogues, int columns, char **x, char **y)
{


	return 0;
}

void initialize_catalogues(sqlite3 *db)
{
	int rc;
	char *err_msg;

	rc = sqlite3_exec(db, "SELECT id, name FROM ctg", register_catalogue,
			NULL, &err_msg);


}

/**
 * Convert an entry from a KStars .dat file to an SQL insert query.
 */
int insert_entry(char *dat_entry, sqlite3 *db)
{
	/* Check if the entry is actually a comment */
	if (dat_entry[0] == '#')
		return 0;

	char *p = dat_entry;

	if (p[0] == 'N') {
	}

	return 0;
}
/**
 * Load the current ngcic.dat file, given as parameter to convert data.
 */
void load_ngcic(const char *ngcic_filename, sqlite3 *db)
{
	FILE *ngcic_file = fopen(ngcic_filename, "r");
	char *p, line[MAX_LINE_LENGTH], *sql;

	do {
		p = fgets(line, MAX_LINE_LENGTH, ngcic_file);
		insert_entry(line, db);
	} while (p != NULL);

	fclose(ngcic_file);
}

int main(int argc, char *argv[]) {
	KSFileReader x;

	sqlite3 *kstarsdb;

	if (argc != 4) {
		printf("Usage: %s data_file [database_file] [database_sql]\n", argv[0]);
		exit(1);
	}

	kstarsdb = open_database(argv[2]);

	initialize_kstarsdb(kstarsdb, argv[3]);
	initialize_catalogues(kstarsdb);

	sqlite3_close(kstarsdb);

	return 0;
}
