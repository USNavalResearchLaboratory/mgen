/* python gpsPub */
%module "gpsPub"

%{
#define SWIG_FILE_WITH_INIT
#include "gpsPub.h"
%}

%include "gpsPub.h"
