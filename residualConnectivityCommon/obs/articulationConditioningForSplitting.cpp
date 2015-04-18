#include "obs/articulationConditioningForSplitting.h"
namespace discreteGermGrain
{
	namespace obs
	{
		articulationConditioningForSplitting::articulationConditioningForSplitting(Context const& context, boost::shared_array<const vertexState> state, ::discreteGermGrain::obs::withWeightConstructorType& otherData)
			: ::discreteGermGrain::withSub(context, state), weight(otherData.weight)
		{}
		articulationConditioningForSplitting::articulationConditioningForSplitting(Context const& context, boost::mt19937& randomSource)
			: ::discreteGermGrain::withSub(context, randomSource), weight(1)
		{}
		void articulationConditioningForSplitting::getSubObservation(vertexState* newState, int radius, subObservationConstructorType& other) const
		{
			::discreteGermGrain::withSub::getSubObservation(radius, newState);
			other.weight = weight;
		}
		articulationConditioningForSplitting::articulationConditioningForSplitting(articulationConditioningForSplitting&& other)
			: ::discreteGermGrain::withSub(static_cast<::discreteGermGrain::withSub&&>(other)), weight(other.weight)
		{}
		const mpfr_class& articulationConditioningForSplitting::getWeight() const
		{
			return weight;
		}
	}
}
