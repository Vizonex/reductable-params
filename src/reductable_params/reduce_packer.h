#ifndef __REDUCE_PACKER_H__
#define __REDUCE_PACKER_H__

#include <Python.h>

#include "pythoncapi_compat.h"

#ifdef __cplusplus
extern "C" {
#endif


/* This was more or less an optimization & Not wanting cython 
to screw around or trigger segfaults with in the most critical sections.

However there is now a push to transition to Pure C in the future.
SEE: https://github.com/Vizonex/reductable-params/issues/15
*/

/* Simillar to _PyErr_SetKeyError except it's a bit more public */
/* Might be something worth proposing to add to CPython in the future . */
static void 
PyErr_SetKeyError(PyObject* arg){
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

static inline void 
rd_raise_positional_error(PyObject* name, PyObject* key, Py_ssize_t pos){
    PyErr_Format(
        PyExc_TypeError, "argument for %.200S given by name ('%S') and position (%zu)",
        name, key, pos + 1
    );
}

static PyObject* 
reduce_install_args(
    PyObject* name, /* Function's possible name */
    PyObject* defaults,
    PyObject* args, /* tuple[Any, ...] */
    PyObject* kwargs, /* dict */
    Py_ssize_t nargs,  /* PyTuple_GET_SIZE(args) */
    PyObject* params  /* tuple[Any, ...] */
){
    PyObject* output, *k, *v;
    output = PyDict_Copy(defaults);
    if (output == NULL) return NULL;

    for (Py_ssize_t n = 0; n < nargs; n++){
        /* Counting on Zero Failure for params this code should never fail. */
        k = PyTuple_GET_ITEM(params, n);
        if (k == NULL) goto fail;
        Py_INCREF(k);
        
        if (PyDict_Contains(kwargs, k)){
            /* argument possibly present in both tuple and 
                if anything is concerned, this is not what we want */
            Py_DECREF(k);
            rd_raise_positional_error(name, k, n);
            goto fail;
        }
        v = PyTuple_GET_ITEM(args, n);
        /* IndexError cannot normally happen but will still see if this does happen. */
        if (v == NULL){
            Py_DECREF(k);
            goto fail;
        }
        Py_INCREF(v);
        int err = PyDict_SetItem(output, k, v);
        /* cleanup object copies before error checking... */
        Py_DECREF(k);
        Py_DECREF(v);
        if (err < 0){
            goto fail;
        }
    }
    /* TODO: Combine this function with reduce_install_kwargs */
    return output;
fail:
    Py_CLEAR(output);
    return NULL;
};

/* TODO: mark functions as *_impl implying that it's a implementation of or we could use a 
 * "rd_*" to imply that it's a lower level function. */
static int reduce_install_kwargs(
    PyObject* params,
    PyObject* kwargs,
    PyObject* output
){
    PyObject* key, *value;
    Py_ssize_t pos = 0;

    /* Iterations of Python Dictionarys in Free-Threaded Mode 
     * could be considered unsafe or dangerous so we use a 
     * Py_BEGIN_CRITICAL_SECTION to protect all keyword arguments 
     * being installed. */

    Py_BEGIN_CRITICAL_SECTION(kwargs);
    while (PyDict_Next(kwargs, &pos, &key, &value)){
        if (!PySet_Contains(params, key)){
            /* force up a keyerror if object is not present 
             * in the actual defaults */
            PyErr_SetKeyError(key);
            return -1;
        }
        if (PyDict_SetItem(output, key, value) < 0){
            return -1;
        }
    }
    Py_END_CRITICAL_SECTION();
    return 0;
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
        PyTuple_SET_ITEM(args, i, Py_NewRef(v));
    }

    for (Py_ssize_t j = n_required; j < n_params; j++){
        key = PyTuple_GET_ITEM(r_params, j);
        v = PyDict_GetItem(kwds, key);

        /* TODO: inline as if (v != NULL && (PyDict_SetItem(kwargs, key, v) < 0)) 
         * could wind up being a possible improvement. */
        
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