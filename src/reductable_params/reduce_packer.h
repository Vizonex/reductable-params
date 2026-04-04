#ifndef __REDUCE_PACKER_H__
#define __REDUCE_PACKER_H__

#include <Python.h>

#ifdef __cplusplus
extern "C" {
#endif

/* This was more or less an optimization & Not wanting cython 
to screw around or trigger segfaults with in the most critical sections 
The entire reduce module could be moved to C if problems persist.
*/

/* Simillar to _PyErr_SetKeyError except it's a bit more public */
void PyErr_SetKeyError(PyObject* arg){
    /* 
    Larger explaination in _PyErr_SetKeyError 
    but to summarize, we don't want something else to be raised. 
    */
    PyErr_Clear();
    PyObject* exc = PyObject_CallOneArg(PyExc_KeyError, arg);
    if (!exc){
        /* Caller Failed */
        return;
    }
    PyErr_SetObject((PyObject*)Py_TYPE(exc), exc);
    Py_DECREF(exc);
}

static PyObject* reduce_call(
    PyObject* kwds, 
    PyObject* r_wrapped,
    PyObject* r_defaults, 
    PyObject* r_required,
    PyObject* r_params,
    const Py_ssize_t n_required,
    const Py_ssize_t n_params
){
    PyObject* key, *v, *result;
    result = NULL;
    PyObject* kwargs = PyDict_Copy(r_defaults);
    if (kwargs == NULL) goto cleanup;
    PyObject* args = PyTuple_New(n_required);
    if (args == NULL) goto cleanup;
 
    for (Py_ssize_t i = 0; i < n_required; i++){
        key = PyTuple_GET_ITEM(r_required, i);
        v = PyDict_GetItem(kwds, key);
        if (v == NULL){
            PyErr_SetKeyError(key);
            goto cleanup;
        }
        Py_INCREF(v);
        PyTuple_SET_ITEM(args, i, v);
    }

    for (Py_ssize_t j = n_required; j < n_params; j++){
        key = PyTuple_GET_ITEM(r_params, j);
        v = PyDict_GetItem(kwds, key);
        if (v != NULL){
            if (PyDict_SetItem(kwargs, key, v) < 0){
                goto cleanup;
            }
        }
    }

    result = PyObject_Call(r_wrapped, args, kwargs);
cleanup:
    Py_CLEAR(kwargs);
    Py_CLEAR(args);
    return result;
}




#ifdef __cplusplus
}
#endif 


#endif // __REDUCE_PACKER_H__