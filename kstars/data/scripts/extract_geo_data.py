#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sqlite3

db = sqlite3.connect("data/citydb.sqlite")
db.text_factory = str

cursor = db.cursor()

cursor.execute('''SELECT DISTINCT Name, Province, Country from city''')
all_rows = cursor.fetchall()
all_rows.sort()
for row in all_rows:
  name = row[0]
  province = row[1]
  country = row[2]

  if format(province) != '':
    place = "{0} {1}".format(province, country)
  else:
    place = country

  print('xi18nc("City in {0}", "{1}")'.format(place, name))

cursor.execute('''SELECT DISTINCT Province, Country from city''')
all_rows = cursor.fetchall()
all_rows.sort()
for row in all_rows:
  province = row[0]
  country = row[1]

  if format(province) != '':
    print('xi18nc("Region/state in {0}", "{1}")'.format(country, province))

cursor.execute('''SELECT DISTINCT Country from city''')
all_rows = cursor.fetchall()
all_rows.sort()
for row in all_rows:  
  print('xi18nc("Country name", "{0}")'.format(row[0]))

db.close()
