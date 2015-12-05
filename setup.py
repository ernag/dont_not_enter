from distutils.core import setup, Extension
import numpy.distutils.misc_util

setup( name = 'pntcalc',
    version = '0.5',
    description = 'This package calculates the activity points from a given data set using the exact same algorithm used on device.',
    author = 'Stephen Pinto and Nate Yoder',
    author_email = 'nate@whistle.com',
    url = 'http://www.whistle.com',
    ext_modules=[Extension("pntcalc", ["utils/pntcalc/pntcalcmodule.c", "utils/pntcalc/min_act.c"], define_macros=[("PY_MODULE",None)])],
    include_dirs=numpy.distutils.misc_util.get_numpy_include_dirs()
)
