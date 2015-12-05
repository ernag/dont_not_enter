#include <Python.h>
#include "min_act.h"
#include <numpy/arrayobject.h>

/* References:
 * http://dan.iel.fm/posts/python-c-extensions/
 * http://docs.python.org/2/extending/extending.html#intermezzo-errors-and-exceptions
 * http://docs.python.org/release/1.5.2p2/ext/parseTuple.html
 * http://docs.python.org/dev/c-api/arg.html
 * http://docs.python.org/2/distutils/introduction.html
 */

/* DOCSTRINGS */
static char module_docstring[] = "This module allows Python to use the same point calculation done in C";
static char calculate_docstring[] = "Calculates the points from a given input array. Puts results in global variables";
static char get_minutes_processed_docstring[] = "Returns how many minutes have been processed";
static char get_days_processed_docstring[] = "Returns how many days have been processed";
static char get_points_this_minute_docstring[] = "Returns the total points in the current minute";
static char get_float_shift_docstring[] = "Returns the float shift used in the algorithm for putting fixed point floats in Q format";
static char get_active_threshold_docstring[] = "Returns the threshold used to determine if a minute is active";
static char finish_docstring[] = "Finish the current block and make sure it's not waiting for more data";
static char clear_docstring[] = "Clear all global variables";

/* FUNCTION PROTOTYPES */
static PyObject *pntcalc_calculate(PyObject *self, PyObject *args);
static PyObject *pntcalc_get_minutes_processed(PyObject *self, PyObject *args);
static PyObject *pntcalc_get_days_processed(PyObject *self, PyObject *args);
static PyObject *pntcalc_get_points_this_minute(PyObject *self, PyObject *args);
static PyObject *pntcalc_get_float_shift(PyObject *self, PyObject *args);
static PyObject *pntcalc_get_active_threshold(PyObject *self, PyObject *args);
static PyObject *pntcalc_finish(PyObject *self, PyObject *args);
static PyObject *pntcalc_clear(PyObject *self, PyObject *args);



/* METHOD DEFINITIONS (for python) */
static PyMethodDef module_methods[] = {
    {"calculate", pntcalc_calculate, METH_VARARGS, calculate_docstring},
    {"get_minutes_processed", pntcalc_get_minutes_processed, METH_VARARGS, get_minutes_processed_docstring},
    {"get_days_processed", pntcalc_get_days_processed, METH_VARARGS, get_days_processed_docstring},
    {"get_points_this_minute", pntcalc_get_points_this_minute, METH_VARARGS, get_points_this_minute_docstring},
    {"get_float_shift", pntcalc_get_float_shift, METH_VARARGS, get_float_shift_docstring},
    {"get_active_threshold", pntcalc_get_active_threshold, METH_VARARGS, get_active_threshold_docstring},
    {"clear", pntcalc_clear, METH_VARARGS, clear_docstring},
    {"finish", pntcalc_finish, METH_VARARGS, finish_docstring},
    {NULL, NULL, 0, NULL}
};

/* PYTHON INIT FUNCTION */
PyMODINIT_FUNC initpntcalc(void)
{
    PyObject *m = Py_InitModule3("pntcalc", module_methods, module_docstring);
    if (m == NULL)
        return;

    /* Load `numpy` functionality. */
    import_array();
}

/* CALCULATE FUNCTION */
static PyObject *
pntcalc_calculate(PyObject *self, PyObject *args) {
    size_t num_rows, start_row;
    uint64_t device_milli;
    uint64_t day_milli;
    PyObject *input_obj;

    /* Parse input tuple, from http://docs.python.org/dev/c-api/arg.html:
     * "y*" = byte array (int8_t *)
     * "i" = signed int (int16_t)
     * "I" = unsigned int (uint16_t)
     * "l" = signed long int (int32_t)
     * "k" = unsigned long int (uint32_t)
     * "K" = unsigned long long int (uin64_t)
     * "f" = float
     * "O" = Python Object
     */
    if (!PyArg_ParseTuple(args,"OkkKK", &input_obj, &num_rows, &start_row, &device_milli, &day_milli)) {
        return NULL;
    }

    /* Interpret input object as numpy array */
    PyObject *input_array_obj = PyArray_FROM_OTF(input_obj, NPY_INT8, NPY_IN_ARRAY);

    /* If that didn't work, throw an exception. */
    if (input_array_obj == NULL) {
        Py_XDECREF(input_array_obj);
        return NULL;
    }

    /* Get pointers to the data as C-types */
    int8_t *input_array = (int8_t*) PyArray_DATA(input_array_obj);

    /* Call the external C function to calculate points */
    MIN_ACT_calc(input_array, num_rows, start_row, device_milli, day_milli);

    /* Clean up. */
    Py_DECREF(input_array_obj);

    /* Build the output tuple*/
    PyObject *ret = Py_BuildValue("[I,I,k]", MIN_ACT_get_prev_block_min(), MIN_ACT_get_prev_block_day(), MIN_ACT_get_points_this_minute());
    return ret;
}

static PyObject *
pntcalc_get_minutes_processed(PyObject *self, PyObject *args) {
    /* Build the output tuple*/
    PyObject *ret = Py_BuildValue("I", MIN_ACT_get_prev_block_min());
    return ret;
}

static PyObject *
pntcalc_get_days_processed(PyObject *self, PyObject *args) {
    /* Build the output tuple*/
    PyObject *ret = Py_BuildValue("I", MIN_ACT_get_prev_block_day());
    return ret;
}

static PyObject *
pntcalc_get_points_this_minute(PyObject *self, PyObject *args) {
    /* Build the output tuple*/
    PyObject *ret = Py_BuildValue("f", MIN_ACT_get_points_this_minute());
    return ret;
}

static PyObject *
pntcalc_get_float_shift(PyObject *self, PyObject *args) {
    /* Build the output tuple*/
    PyObject *ret = Py_BuildValue("i", MIN_ACT_FLOAT_SHIFT);
    return ret;
}

static PyObject *
pntcalc_get_active_threshold(PyObject *self, PyObject *args) {
    /* Build the output tuple*/
    PyObject *ret = Py_BuildValue("f", MIN_ACT_get_active_threshold());
    return ret;
}

static PyObject *
pntcalc_reset_median_filter(PyObject *self, PyObject *args) {
    MIN_ACT_reset_median_filter();
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pntcalc_finish(PyObject *self, PyObject *args) {
    /* Build the output tuple*/
    MIN_ACT_finish();
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pntcalc_clear(PyObject *self, PyObject *args) {
    /* Build the output tuple*/
    MIN_ACT_clear();
    Py_INCREF(Py_None);
    return Py_None;
}
