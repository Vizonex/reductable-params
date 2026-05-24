#include "_reducelib/state.h"
#include "_reducelib/reduce_object.h"
#include "_reducelib/pythoncapi_compat.h"


static int
module_traverse(PyObject *mod, visitproc visit, void *arg)
{
    mod_state* state = get_mod_state(mod);
    
    Py_VISIT(state->ReduceType);

    Py_VISIT(state->function);
    Py_VISIT(state->__name__);
    Py_VISIT(state->_abc_Reductable);
    Py_VISIT(state->_utils_varnames);

    return 0;
}

static int
module_clear(PyObject *mod)
{
    mod_state *state = get_mod_state(mod);

    Py_CLEAR(state->ReduceType);

    Py_CLEAR(state->function);
    Py_CLEAR(state->__name__);
    Py_CLEAR(state->_abc_Reductable);
    Py_CLEAR(state->_utils_varnames);

    return 0;
}

static void
module_free(void *mod)
{
    (void)module_clear((PyObject *)mod);
}



static PyType_Spec reduce_spec = {
    .name = "_reduce.reduce",
    .basicsize = sizeof(ReduceObject),
    .flags = (Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_BASETYPE),
    .slots = reduce_slots,
};


static int
module_exec(PyObject *mod)
{
    mod_state* state = get_mod_state(mod);
    PyObject* module = NULL;
    PyObject* tmp = NULL;
    PyObject* ret = NULL;

/* This is my own macro inspired by CPython's works with _asynciomodule.c */
#define WITH_INTERN_STRING(VAR, VALUE) \
    VAR = PyUnicode_FromString(VALUE); \
    if (VAR == NULL) { \
        goto fail; \
    }

    WITH_INTERN_STRING(state->function, "function");
    WITH_INTERN_STRING(state->__name__, "__name__");

/* I just enjoyed CPython's stuff so much that I am borrowing it. :) */
#define WITH_MOD(NAME) \
    Py_CLEAR(module); \
    module = PyImport_ImportModule(NAME); \
    if (module == NULL) { \
        goto fail; \
    }

#define GET_MOD_ATTR(VAR, NAME) \
    VAR = PyObject_GetAttrString(module, NAME); \
    if (VAR == NULL) { \
        goto fail; \
    }

    WITH_MOD("reductable_params.abc")
    GET_MOD_ATTR(state->_abc_Reductable, "Reducable");

    WITH_MOD("reductable_params.utils");
    GET_MOD_ATTR(state->_utils_varnames, "varnames");

    tmp = PyType_FromModuleAndSpec(mod, &reduce_spec, NULL);
    if (tmp == NULL){
        goto fail;
    }
    state->ReduceType = (PyTypeObject*)tmp;

    tmp = PyUnicode_FromString("register");
    ret = PyObject_CallMethodOneArg(state->_abc_Reductable, tmp, (PyObject*)state->ReduceType);
    Py_CLEAR(tmp);
    if (ret == NULL){
        goto fail;
    }

    if (PyModule_AddType(mod, state->ReduceType) < 0){
        goto fail;
    }
    return 0;
fail:
    Py_CLEAR(tmp);
    // Py_CLEAR(ret);
    return -1;
}

static struct PyModuleDef_Slot module_slots[] = {
    
    {Py_mod_exec, module_exec},
#if PY_VERSION_HEX >= 0x030c00f0
    {Py_mod_multiple_interpreters, Py_MOD_PER_INTERPRETER_GIL_SUPPORTED},
#endif
#if PY_VERSION_HEX >= 0x030d00f0
    {Py_mod_gil, Py_MOD_GIL_NOT_USED},
#endif
    {0, NULL},
};

// static PyMethodDef module_methods[] = {
//     {NULL, NULL},
// };

static PyModuleDef reduce_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "_reduce",
    .m_size = sizeof(mod_state),
    .m_slots = module_slots,
    // .m_methods = module_methods,
    .m_traverse = module_traverse,
    .m_clear = module_clear,
    .m_free = module_free,
};


PyMODINIT_FUNC
PyInit__reduce(void)
{
    return PyModuleDef_Init(&reduce_module);
}
