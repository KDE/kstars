#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sqlite3

db = sqlite3.connect("data/citydb.sqlite")
db.text_factory = str

cursor = db.cursor()

cursor.execute('''SELECT DISTINCT Name from city''')
all_rows = cursor.fetchall()
all_rows.sort()
for row in all_rows:  
  print('xi18nc("City name (optional, probably does not need a translation)", "{0}")'.format(row[0]))

cursor.execute('''SELECT DISTINCT Province from city''')
all_rows = cursor.fetchall()
all_rows.sort()
for row in all_rows:  
  print('xi18nc("Province name (optional, rarely needs a translation)", "{0}")'.format(row[0]))

cursor.execute('''SELECT DISTINCT Country from city''')
all_rows = cursor.fetchall()
all_rows.sort()
for row in all_rows:  
  print('xi18nc("Country name (optional, but should be translated)", "{0}")'.format(row[0]))

db.close()