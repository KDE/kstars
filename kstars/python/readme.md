# Python Bindings

This directory contains some basic `python` bindings to allow `HTMesh`
indexation from python scripts. The bindings are created with
[pybind11](https://github.com/pybind/pybind11).

## Building/Installing
Make sure you can build `kstars` and have `pybind11` installed.

### Distutils
Run `python3 setup.py install`.

### CMake
Use the flag `-DBUILD_PYKSTARS=ON` when configuring kstars with cmake
and then build the `pykstars` target. The python module
`kstars/python/pykstars.cpython-*.so` will be built and can then be
imported.

## Usage
### Indexer
The class `pykstars.Indexer` is a thin wrapper to interface with the
`SkyMesh` implementation in KStars.

The constructor takes the `HTMesh` level as an argument. The only
interesting method of the Indexer is `get_trixel` which takes the
right ascension and declination of some object and returns the
computed trixel id. The third argument can optionally be set to `True`
if an epoch conversion from `B1950` to `J2000` is required.

```python
>>> import kstars.python.pykstars as pykstars
>>> I = pykstars.Indexer(7)
>>> I.get_trixel(1, 2, True)
123356
>>> I.get_trixel(1, 2)
123353
```

### DBManager
These are just straight python bindings for the `CatalogsDB::DBManager` class.

## Authors
 - Valentin Boettcher <hiro at protagon.space>, hiro98:kde.org
