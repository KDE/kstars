/*----------------------------------------------------------------------------*/
/**
   @file		qfits_wrap.c
   @author		N. Devillard
   @date		Nov 2001
   @version		$Revision$
   @brief
   Interface file between python and the qfits library.
   All symbols are exported as the 'qfits' module under Python.
*/
/*----------------------------------------------------------------------------*/

/*
	$Id$
	$Author$
	$Date$
	$Revision$
*/

/*-----------------------------------------------------------------------------
   								Includes
 -----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "Python.h"
#include "qfits.h"

/*-----------------------------------------------------------------------------
   								Function codes
 -----------------------------------------------------------------------------*/

static PyObject * _wrap_qfits_get_headers(PyObject * self, PyObject * args)
{
	struct stat			sta ;
	qfits_header	*	qh ;
	char			*	filename ;
	int					xtnum ;
	int					n_ext ;
	int					i ;
	char 				key[FITS_LINESZ],
						val[FITS_LINESZ],
						com[FITS_LINESZ];
	char 			* 	pval ;

	PyObject 		*	hlist ;
	PyObject 		*	hcur ;

	if (!PyArg_ParseTuple(args, "s", &filename)) {
		return NULL ;
	}
	/* Check file's existence */
	if (stat(filename, &sta)!=0) {
		/* raise IOError exception */
		PyErr_SetString(PyExc_IOError, "cannot stat file");
		return NULL ;
	}
	/* Check file is FITS */
	if (is_fits_file(filename)!=1) {
		/* return None */
		return Py_BuildValue("") ;
	}
	/* Get number of extensions */
	n_ext = qfits_query_n_ext(filename);
	if (n_ext<0) {
		PyErr_SetString(PyExc_IOError, "cannot get number of FITS extensions");
		return NULL ;
	}

	hlist = PyList_New(1+n_ext) ;
	for (xtnum=0 ; xtnum<=n_ext ; xtnum++) {
		/* Query header */
		if (xtnum==0) {
			qh = qfits_header_read(filename);
		} else {
			qh = qfits_header_readext(filename, xtnum);
		}
		if (qh==NULL) {
			PyErr_SetString(PyExc_IOError, "cannot read header");
            Py_DECREF(hlist);
			return NULL ;
		}
		/* Build up a list of tuples (key,val,com) */
		hcur = PyList_New(qh->n) ;
		for (i=0 ; i<qh->n ; i++) {
			qfits_header_getitem(qh, i, key, val, com, NULL);
			pval = qfits_pretty_string(val);
			PyList_SetItem(hcur,i,Py_BuildValue("(sss)",key,pval,com)) ;
		}
		qfits_header_destroy(qh);
		/* Assign list into main list */
		PyList_SetItem(hlist, xtnum, hcur);
	}
	return hlist ;
}

static PyObject * _wrap_qfits_expand_keyword(PyObject * self, PyObject * args)
{
	char	*	key ;

	if (!PyArg_ParseTuple(args, "s", &key)) {
		return NULL ;
	}
	return Py_BuildValue("s", qfits_expand_keyword(key));
}

static PyObject * _wrap_qfits_datamd5(PyObject * self, PyObject * args)
{
	char	*	filename ;
	char	*	md5hash ;

	if (!PyArg_ParseTuple(args, "s", &filename)) {
		return NULL ;
	}
	md5hash = qfits_datamd5(filename);
	if (md5hash==NULL) { 
		PyErr_SetString(PyExc_IOError, "cannot compute data MD5");
		return NULL ;
	}
	return Py_BuildValue("s", md5hash);
}

static PyObject * _wrap_qfits_version(PyObject * self, PyObject * args)
{
	return Py_BuildValue("s", qfits_version());
}


static PyMethodDef qfits_methods[] = {
	{"get_headers",		_wrap_qfits_get_headers, METH_VARARGS},
	{"expand_keyword",	_wrap_qfits_expand_keyword, METH_VARARGS},
	{"datamd5",			_wrap_qfits_datamd5, METH_VARARGS},
	{"version",			_wrap_qfits_version, METH_VARARGS},
	{NULL, NULL}
};

void initqfits(void)
{
	PyObject * m ;
	m = Py_InitModule("qfits", qfits_methods);
}


/* vim: set ts=4 et sw=4 tw=75 */
