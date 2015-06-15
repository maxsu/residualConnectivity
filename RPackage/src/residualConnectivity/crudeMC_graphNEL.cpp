#include "crudeMCLib.h"
#include "Rcpp.h"
#include "crudeMCInterfaces.h"
#include "graphNELInterface.h"
SEXP crudeMC_graphNEL(SEXP graph_sexp, SEXP vertexCoordinates_sexp, SEXP probability_sexp, SEXP n_sexp, SEXP seed_sexp)
{
BEGIN_RCPP
	//convert number of samples
	double n_double;
	try
	{
		n_double = Rcpp::as<double>(n_sexp);
	}
	catch(Rcpp::not_compatible&)
	{
		throw std::runtime_error("Unable to convert input n to a number");
	}
	long n;
	if(std::abs(n_double - std::round(n_double)) > 1e-3)
	{
		throw std::runtime_error("Input n must be an integer");
	}
	n = (long)std::round(n_double);

	//convert seed
	int seed;
	try
	{
		seed = Rcpp::as<int>(seed_sexp);
	}
	catch(Rcpp::not_compatible&)
	{
		throw std::runtime_error("Input seed must be an integer");
	}

	discreteGermGrain::Context context = graphNELInterface(graph_sexp, vertexCoordinates_sexp, probability_sexp);
	discreteGermGrain::crudeMCArgs args(context);
	args.n = n;
	args.randomSource.seed(seed);

	std::size_t result = discreteGermGrain::crudeMC(args);
	return Rcpp::wrap((int)result);
END_RCPP
}