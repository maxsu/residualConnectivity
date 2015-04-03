#include "subObs/usingCutVertices.h"
#include <boost/random/bernoulli_distribution.hpp>
#include <boost/graph/biconnected_components.hpp>
#include "isSingleComponentWithRadius.h"
#include <boost/random/binomial_distribution.hpp>
#include <boost/range/algorithm/random_shuffle.hpp>
#include <boost/random/random_number_generator.hpp>
namespace discreteGermGrain
{
	namespace subObs
	{
		usingCutVertices::usingCutVertices(Context const& context, boost::shared_array<const vertexState> state, int radius, ::discreteGermGrain::subObs::usingCutVerticesConstructorType& otherData)
			: ::discreteGermGrain::subObs::subObsWithRadius(context, state, radius)
		{
			potentiallyConnected = isSingleComponentPossible(context, state.get(), otherData.components, otherData.stack);
		}
		bool usingCutVertices::isPotentiallyConnected() const
		{
			return potentiallyConnected;
		}
		usingCutVertices::usingCutVertices(usingCutVertices&& other)
			: ::discreteGermGrain::subObs::subObsWithRadius(static_cast<::discreteGermGrain::subObs::subObsWithRadius&&>(other))
		{
			potentiallyConnected = other.potentiallyConnected;
		}
		void usingCutVertices::constructRadius1Graph(radiusOneGraphType& graph, std::vector<int>& graphVertices) const
		{
			if(radius != 1)
			{
				throw std::runtime_error("Radius must be 1 to call constructRadius1Graph");
			}
			graphVertices.clear();
			std::size_t nVertices = context.nVertices();
			const vertexState* stateRef = state.get();

			//graphVertices[i] is the vertex in the global graph that corresponds to vertex i in the returned graph
			//inverse is the inverse mapping, with 0's everywhere else. 
			int* inverse = (int*)alloca(nVertices * sizeof(int));
			//0 is used to identify a point which is not going to be contained within the returned graph
			memset(inverse, 0, sizeof(int)*nVertices);

			for(std::size_t i = 0; i < nVertices; i++)
			{
				if(stateRef[i].state & (UNFIXED_MASK | ON_MASK))
				{
					graphVertices.push_back((int)i);
					inverse[i] = (int)graphVertices.size();
				}
			}

			graph = radiusOneGraphType(graphVertices.size());
			const Context::inputGraph& originalGraph = context.getGraph();
			for(std::size_t i = 0; i < graphVertices.size(); i++)
			{
				Context::inputGraph::out_edge_iterator start, end;
				boost::tie(start, end) = boost::out_edges(graphVertices[i], originalGraph);
				while(start != end)
				{
					if(inverse[start->m_target] > 0 && (int)i < inverse[start->m_target] - 1)
					{
						boost::add_edge((int)i, inverse[start->m_target] - 1, graph);
					}
					start++;
				}
			}
		}
		void usingCutVertices::getObservation(vertexState* outputState, boost::mt19937& randomSource, observationConstructorType&)const
		{
			boost::random::bernoulli_distribution<float> vertexDistribution(context.getOperationalProbabilityD());
			std::size_t nVertices = context.nVertices();
			memcpy(outputState, state.get(), sizeof(vertexState)*nVertices);
			//generate a full random grid, which includes the subPoints 
			for(std::size_t i = 0; i < nVertices; i++)
			{
				if(outputState[i].state & UNFIXED_MASK)
				{
					if(vertexDistribution(randomSource))
					{
						outputState[i].state = UNFIXED_ON;
					}
					else outputState[i].state = UNFIXED_OFF;
				}
			}
		}
		boost::shared_array<const vertexState> usingCutVertices::estimateRadius1(boost::mt19937& randomSource, int nSimulations, std::vector<int>& scratchMemory, boost::detail::depth_first_visit_restricted_impl_helper<Context::inputGraph>::stackType& stack, mpfr_class& outputProbability) const
		{
			double openProbability = context.getOperationalProbabilityD();
			boost::bernoulli_distribution<float> bern(openProbability);
			if(radius != 1)
			{
				throw std::runtime_error("Radius must be 1 to call constructRadius1Graph");
			}
			//Construct helper graph, containing everything except FIXED_OFF vertices
			radiusOneGraphType graph;
			std::vector<int> graphVertices;
			constructRadius1Graph(graph, graphVertices);
			//assign edge indices
			{
				radiusOneGraphType::edge_iterator start, end;
				int edgeIndex = 0;
				boost::property_map<radiusOneGraphType, boost::edge_index_t>::type edgePropertyMap = boost::get(boost::edge_index, graph);
				boost::tie(start, end) = boost::edges(graph);
				for(; start != end; start++) boost::put(edgePropertyMap, *start, edgeIndex++);
			}

			const vertexState* stateRef = state.get();
			std::size_t nVertices = context.nVertices();
			bool anyNotFixedOff = false;
			for(std::size_t i = 0; i < nVertices; i++)
			{
				if(stateRef[i].state != FIXED_OFF) 
				{
					anyNotFixedOff = true;
					break;
				}
			}
			//if everything is fixed at OFF, then we must have a connected graph
			if(!anyNotFixedOff)
			{
				outputProbability = 1;
				return this->state;
			}
			if(boost::num_vertices(graph) == 1)
			{
				boost::shared_array<vertexState> newStates(new vertexState[1]);
				newStates[0] = vertexState::fixed_off();
				if(bern(randomSource)) newStates[0] = vertexState::fixed_on();

				outputProbability = 1;
				return newStates;
			}

			//get out biconnected components of helper graph
			typedef std::vector<boost::graph_traits<radiusOneGraphType>::edges_size_type> component_storage_t;
			typedef boost::iterator_property_map<component_storage_t::iterator, boost::property_map<radiusOneGraphType, boost::edge_index_t>::type> component_map_t;

			component_storage_t biconnectedIds(boost::num_edges(graph));
			component_map_t componentMap(biconnectedIds.begin(), boost::get(boost::edge_index, graph));

			std::vector<int> articulationVertices;
			boost::biconnected_components(graph, componentMap, std::back_inserter(articulationVertices));

			int nNotAlreadyFixedArticulation = 0;
			//convert list of articulation vertices across to a bitset
			std::vector<Context::inputGraph::vertex_descriptor> unfixedVertices;
			std::vector<Context::inputGraph::vertex_descriptor> fixedVertices;
			
			std::vector<bool> isArticulationVertex(nVertices, false);
			//Mark off each articulation point in the above vector, and count the number of extra points that we're fixing.
			for(std::vector<int>::iterator i = articulationVertices.begin(); i != articulationVertices.end(); i++)
			{
				isArticulationVertex[graphVertices[*i]] = true;
				if(stateRef[graphVertices[*i]].state & UNFIXED_MASK) 
				{
					nNotAlreadyFixedArticulation++;
				}
			}
			for(std::size_t i = 0; i < nVertices; i++)
			{
				if((stateRef[i].state & UNFIXED_MASK) && !(isArticulationVertex[i]))
				{
					unfixedVertices.push_back(i);
				}
				if((stateRef[i].state & FIXED_ON) || isArticulationVertex[i])
				{
					fixedVertices.push_back(i);
				}
			}

			std::size_t nFixedVertices = fixedVertices.size();

			boost::random::binomial_distribution<> extraVertexCountDistribution(unfixedVertices.size(), openProbability);
			boost::random_number_generator<boost::mt19937> generator(randomSource);
			boost::shared_array<vertexState> returnValue;
			int simulationsConnectedCount = 0;
			for(int simulationCounter = 0; simulationCounter < nSimulations; simulationCounter++)
			{
				fixedVertices.resize(nFixedVertices);
				boost::range::random_shuffle(unfixedVertices, generator);
				int extraVertexCount = extraVertexCountDistribution(randomSource);
				fixedVertices.insert(fixedVertices.end(), unfixedVertices.begin(), unfixedVertices.begin() + extraVertexCount);

				bool currentSimulationConnected = isSingleComponentSpecified(context, fixedVertices, scratchMemory, stack);
				if(currentSimulationConnected) simulationsConnectedCount++;
				if(!returnValue && currentSimulationConnected)
				{
					returnValue.reset(new vertexState[nVertices]);
					std::fill(returnValue.get(), returnValue.get() + nVertices, vertexState::fixed_off());
					for(std::vector<Context::inputGraph::vertex_descriptor>::iterator fixedVertexIterator = fixedVertices.begin(); fixedVertexIterator != fixedVertices.end(); fixedVertexIterator++) returnValue[*fixedVertexIterator] = vertexState::fixed_on();
				}
			}
			outputProbability = boost::multiprecision::pow(context.getOperationalProbability(), nNotAlreadyFixedArticulation) * simulationsConnectedCount/(double)nSimulations;
			return returnValue;
		}
	}
}