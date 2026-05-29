#ifndef __REDUCE_OBJECT_H__
#define __REDUCE_OBJECT_H__

#ifdef __cplusplus
extern "C" { 
#endif
#include <Python.h>
#include <structmember.h>
#include "state.h"
#include "reduce_packer.h"

// #if PY_VERSION_HEX >= 0x030c00f0
// #define MANAGED_WEAKREFS
// #endif

typedef struct _reduce_object {
    PyObject_HEAD
// #ifndef MANAGED_WEAKREFS
//     PyObject *weaklist;
// #endif
    PyObject* wrapped; /* __wrapped__ */
    PyObject* defaults; /* dict[str, Any] */
    PyObject* name; /* str */
    Py_ssize_t nargs; /* len(args) */
    Py_ssize_t nparams; /* len(params) */
    PyObject* kwargs; /* tuple[str, ...] */
    PyObject* params; /* tuple[str, ...] */
    PyObject* param_set; /* frozenset[str] */
    PyObject* args; /* tuple[str, ...] */
} ReduceObject;


#define Reduce_CheckExact(state, obj) Py_IS_TYPE(obj, state->ReduceType)
#define Reduce_Check(state, obj) \
    (Reduce_CheckExact(obj, state) || PyObject_TypeCheck(obj, state->ReduceType))


/* Argument Parser */
static int reduce_parse_args(
    const char* unpack_name,
    const char* func_name,
    const char* arg_name,
    PyObject** out,
    PyObject* args,
    PyObject* kwargs 
){
    PyObject* arg = NULL;
    if (!PyArg_UnpackTuple(
            args, unpack_name, 1, 1, &arg)) {
        *out = NULL;
        return -1;
    }
    if (arg == NULL){
        PyErr_Format(
            PyExc_TypeError, 
            "%s() missing 1 required positonal argument '%s'",
            func_name,
            arg_name
        );
        *out = NULL;
        return -1;
    }
    // NOTE: I don't expect everyone not to use a **kwargs
    // in fact even aiocallback makes this mistake regularly.
    // so for now, let's make a patch for kwargs to get it's size.
    if ((kwargs != NULL) && (PyDict_GET_SIZE(kwargs) != 0)){
        PyErr_Format(PyExc_TypeError,
                     "%s() doesn't accept keyword arguments", 
                     func_name);
        *out = NULL;
        return -1;
    }
    *out = arg;
    return 0;
}



static inline int 
reduce_tp_clear(ReduceObject* self){
    Py_CLEAR(self->wrapped);
    Py_CLEAR(self->defaults);
    Py_CLEAR(self->name);
    Py_CLEAR(self->kwargs);
    Py_CLEAR(self->params);
    Py_CLEAR(self->param_set);
    Py_CLEAR(self->args);
    return 0;
}

static inline void 
reduce_tp_dealloc(ReduceObject* self){
    PyObject_GC_UnTrack(self);
    Py_TRASHCAN_BEGIN(self, reduce_tp_dealloc)
        reduce_tp_clear(self);
        Py_TYPE(self)->tp_free((PyObject *)self);
    Py_TRASHCAN_END
}

static int 
reduce_tp_traverse(ReduceObject* self, visitproc visit, void* arg){
    Py_VISIT(Py_TYPE(self));
    Py_VISIT(self->wrapped);
    Py_VISIT(self->defaults);
    Py_VISIT(self->name);
    Py_VISIT(self->kwargs);
    Py_VISIT(self->params);
    Py_VISIT(self->param_set);
    Py_VISIT(self->args);
    return 0;
}

PyDoc_STRVAR(reduce__doc__, 
"reduceses arbitrary arguments being sent by only selecting "\
"ones that make sense on sending. Useful when chaining together "\
"callbacks where function's children may not need all arguments "\
"incase callback signatures differ from the parent."
);

#define RD_DEBUG(MSG) \
    printf("[DEBUG] %s\n\n", MSG);

static PyObject*
rd_get_name(PyObject* func, mod_state* state){
    PyObject* name = PyObject_GetAttrString(func, "__name__");
    if (name == NULL){
        return PyUnicode_FromString("function");
    }
    PyObject* ret = PyUnicode_FromFormat("%S()", name);
    Py_DECREF(name);
    return ret;
}

static PyObject* 
rd_varnames(PyObject* func, mod_state* state){
    return PyObject_CallOneArg(state->_utils_varnames, func);
}



/* __init__ seems questionable, its the only part im iffy about 
otherwise this C Code is very clean. */
static int rd_init(ReduceObject* self, PyObject* func, mod_state* state){
    int ret = -1;
    
    PyObject* defaults_items = NULL;
    PyObject* args_and_kwargs = NULL;
    PyObject* name = rd_get_name(func, state);

    if (name == NULL){
        return -1;
    }
    self->name = name;

    args_and_kwargs = rd_varnames(func, state);

    if (args_and_kwargs == NULL){
        Py_CLEAR(name);
        return -1;
    }
    self->wrapped = Py_NewRef(func);
    
    /* args , kwargs */
    
    self->args = Py_NewRef(PyTuple_GET_ITEM(args_and_kwargs, 0));
    self->defaults = Py_NewRef(PyTuple_GET_ITEM(args_and_kwargs, 1));
    

    defaults_items = PyMapping_Keys(self->defaults);
    if (defaults_items == NULL){
        goto finish;
    }


    self->kwargs = PyList_AsTuple(defaults_items);
    if (self->kwargs == NULL){
        goto finish;
    }

    self->params = PySequence_Concat(self->args, self->kwargs);
    if (self->params == NULL){
        goto finish;
    }
    

    self->param_set = PyFrozenSet_New(self->params);
    if (self->param_set == NULL){
        goto finish;
    }

    self->nparams = PyTuple_GET_SIZE(self->params);
    self->nargs = PyTuple_GET_SIZE(self->args);

    Py_CLEAR(defaults_items);
    Py_CLEAR(args_and_kwargs);
    return 0;

finish:
    Py_CLEAR(defaults_items);
    Py_CLEAR(args_and_kwargs);
    Py_DECREF(func);
    return ret;
}

static int 
reduce_tp_init(ReduceObject* self, PyObject* args, PyObject* kwargs){
    mod_state* state = get_mod_state_by_def((PyObject*)self);
    PyObject* arg = NULL;
    if (reduce_parse_args(
        "reductable_params._reduce.reduce",
        "__init__", "func", &arg, args, kwargs) < 0){
        return -1;
    }
    return rd_init(self, arg, state);
}


PyDoc_STRVAR(reduce_args__doc__, 
"lists out the required arguments of this wrapped function."
);

static PyObject* 
rd_get_args(ReduceObject* self){
    return Py_NewRef(self->args);
}

// static PyObject* 
// rd_set_args(ReduceObject* self, PyObject *value, void *Py_UNUSED(context)){
//     PyErr_SetString(PyExc_AttributeError, "args property is read-only.");
//     return NULL;
// }

// static PyObject* 
// reduce_args_get(ReduceObject *self, void *Py_UNUSED(arg))
// {
//     return rd_get_args(self);
// }

PyDoc_STRVAR(reduce_kwargs__doc__,
"lists out optional arguments of this wrapped function."
);

// static PyObject* 
// rd_get_kwargs(ReduceObject* self){
//     return Py_NewRef(self->kwargs);
// }


static PyObject* 
reduce__wrapped__get(ReduceObject *self, void *Py_UNUSED(arg))
{
    return Py_NewRef(self->wrapped);
}

// static PyObject* 
// rd_set_kwargs(ReduceObject* self, PyObject *value, void *Py_UNUSED(context)){
//     PyErr_SetString(PyExc_AttributeError, "kwargs property is read-only.");
//     return NULL;
// }

// static PyObject* 
// reduce_kwargs_get(ReduceObject *self, void *Py_UNUSED(arg))
// {
//     return rd_get_kwargs(self);
// }

// TODO: Vectorcall
PyDoc_STRVAR(
    reduce_install__doc__,
"Simillar to `inspect.BoundArguments` but a little bit faster, "
"it is based off CPython's getargs.c's algorythms, this will also "
"attempt to install defaults if any are needed. However this does not "
"allow arbitrary arguments to be passed through. Instead, this should "
"primarly be used for writing callback utilities that require a parent "
"function's signature.\n\n"
":raises TypeError: if argument parsing fails or has a argument that "
"overlaps in either args or kwargs."
);

// static PyObject*
// reduce_install(ReduceObject* self, PyObject* args, PyObject* kwargs){
//     return rd_install(
//         self->name,
//         self->nparams,
//         self->nargs,
//         args,
//         kwargs,
//         self->param_set,
//         self->defaults,
//         self->params
//     );
// }


static PyObject* rd_install_args(
    ReduceObject* self,  
    PyObject *const *args, 
    size_t nargs,
    PyObject* kwnames,
    size_t nkwargs
){
    PyObject* output, *k, *v;
    output = PyDict_Copy(self->defaults);
    if (output == NULL) return NULL;

    /* fast shortcut */
    if (!nargs) goto finish;

    for (size_t n = 0; n < nargs; n++){
        k = PyTuple_GET_ITEM(self->params, n);
        if (k == NULL) goto fail;
        Py_INCREF(k);

        if ((nkwargs > 0) && PySequence_Contains(kwnames, k)){
            Py_DECREF(k);
            rd_raise_positional_error(self->name, k, n);
            goto fail;
        }
        
        v = args[n];

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
finish:
    return output;
fail:
    Py_CLEAR(output);
    return NULL;
}


static int rd_install_kwargs(
    ReduceObject* self,  
    PyObject *const *args, 
    size_t nargs,
    PyObject* kwnames,
    size_t nkwargs,
    PyObject* output
){
    /* fast shortcut */
    if (!nkwargs) 
        return 0;

    PyObject* key;
    PyObject* value;
    PyObject* param_set = self->param_set;

    for (size_t n = 0; n < nkwargs; n++){
        key = PyTuple_GET_ITEM(kwnames, n);
        value = args[n + nargs];
        if ((key == NULL) || (value == NULL)){
            return -1;
        }
        Py_INCREF(key);
        if (!PySet_Contains(param_set, key)){
            /* force up a keyerror if object is not present 
             * in the actual defaults */
            PyErr_SetKeyError(key);
            Py_DECREF(key);
            return -1;
        }

        Py_INCREF(value);
        int err = PyDict_SetItem(output, key, value);
        Py_DECREF(key);
        Py_DECREF(value);
        if (err < 0)
            return -1;
    }
    return 0;
}


static PyObject*
reduce_install_vectorcall(
    ReduceObject* self,  
    PyObject *const *args, 
    size_t nargsf, 
    PyObject *kwnames
){
    Py_ssize_t nargs = PyVectorcall_NARGS(nargsf);
    Py_ssize_t nkwargs = (kwnames == NULL) ? 0 : PyTuple_GET_SIZE(kwnames);
    Py_ssize_t ntotal = nargs + nkwargs;

    if (ntotal < self->nargs){
        rd_raise_not_enough_params(self->name);
        return NULL;
    }
    if (ntotal > self->nparams){
        rd_raise_wrong_size_error(self->name, self->nparams, nargs, ntotal);
        return NULL;
    }

    PyObject* output = rd_install_args(self, args, nargs, kwnames, nkwargs);
    if (output == NULL){
        return NULL;
    }
    if (!nkwargs){
        return output;
    }
    if (rd_install_kwargs(self, args, nargs, kwnames, nkwargs, output) < 0){
        Py_CLEAR(output);
        return NULL;
    }
    return output;
}



PyDoc_STRVAR(
    reduce_tp_call__doc__,
    "Calls reduction wrapper and calls function "
    "while ignoring any unwanted arguments. This is useful "
    "when chaining together callbacks with different function "
    "formations."
);

/* TODO: vectorcall is possible for a future 1.X.X because we 
just use one single argument and performance wouldn't need 
changing.*/
static PyObject* 
reduce_tp_call(
    ReduceObject* self, 
    PyObject *args, 
    PyObject *kwargs
){
    PyObject* arg = NULL;
    if (reduce_parse_args(
        "reduce._reduce.reduce.__call__",
        "__call__", "kwds", &arg, args, kwargs) < 0){
        return NULL;
    }
    
    return reduce_call(
        arg,
        self->wrapped,
        self->defaults,
        self->args,
        self->params,
        self->nargs,
        self->nparams
    );
};


static PyMethodDef reduce_methods[] = {
    {"install", 
     (PyCFunction)reduce_install_vectorcall, 
      METH_FASTCALL | METH_KEYWORDS,
     reduce_install__doc__},
    {"__class_getitem__",
      (PyCFunction)Py_GenericAlias,
      METH_O | METH_CLASS,
      NULL
    },
    {NULL, NULL} /* sentinel */
};

static PyMemberDef reduce_members[] = {
    // XXX: Crashes so we can't do __wrapped__
    {"__wrapped__",
     Py_T_OBJECT_EX,
     offsetof(ReduceObject, wrapped),
     Py_READONLY
    },
    {
        "args",
        Py_T_OBJECT_EX,
        offsetof(ReduceObject, args),
        Py_READONLY
    },
    {
        "kwargs",
        Py_T_OBJECT_EX,
        offsetof(ReduceObject, kwargs),
        Py_READONLY
    },
    {NULL} /* Sentinel */
};

// static PyGetSetDef reduce_getsetlist[] = {
//     {"__wrapped__", (getter)reduce__wrapped__get, NULL, NULL},
//     // {"args", (getter)reduce_args_get, NULL, reduce_args__doc__},
//     // {"kwargs", (getter)reduce_kwargs_get, NULL, reduce_kwargs__doc__},
//     {NULL} /* Sentinel */
// };

static PyType_Slot reduce_slots[] = {
    {Py_tp_dealloc, reduce_tp_dealloc},
    {Py_tp_doc, (void*)reduce__doc__},
    {Py_tp_call, reduce_tp_call},
    {Py_tp_traverse, reduce_tp_traverse},
    {Py_tp_clear, reduce_tp_clear},
    {Py_tp_methods, reduce_methods},
    {Py_tp_init, reduce_tp_init},
    {Py_tp_alloc, PyType_GenericAlloc},
    {Py_tp_new, PyType_GenericNew},
    {Py_tp_free, PyObject_GC_Del},
    {Py_tp_members, reduce_members},
    // {Py_tp_getset, reduce_getsetlist},
    {0, NULL},
};





#ifdef __cplusplus
}
#endif



#endif // __REDUCE_OBJECT_H__