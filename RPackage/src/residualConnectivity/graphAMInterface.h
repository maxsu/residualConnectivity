#ifndef GRAPH_AM_INTERFACE_HEADER_GUARD
#define GRAPH_AM_INTERFACE_HEADER_GUARD
#include <Rcpp.h>
#include "Context.h"
boost::shared_ptr<residualConnectivity::Context::inputGraph> graphAMConvert(SEXP graph_sexp);
#endif
