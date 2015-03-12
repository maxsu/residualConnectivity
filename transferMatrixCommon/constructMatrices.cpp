#include "constructMatrices.h"
#include <boost/scoped_array.hpp>
#include "subgraphMacros.hpp"
#include "isConnectedState.h"
#include "isConnectedState.cpp"
namespace discreteGermGrain
{
	template<class T> void constructMatricesInternal(typename T::LargeIntMatrix& outputTransitionMatrix, FinalColumnVector& outputFinal, InitialRowVector& outputInitial, const transferStates& states, std::size_t& nonZeroCount)
	{
		nonZeroCount=0;

		const std::vector<unsigned long long>& allStates = states.getStates();
		int gridDimension = states.getGridDimension();
		const std::size_t stateSize = allStates.size();
		
		typename T::LargeIntMatrix transitionMatrix;
		T::initMatrix(transitionMatrix, stateSize);

		FinalColumnVector final;
		final.resize(stateSize, 1);
		final.fill(0L);

		InitialRowVector initial;
		initial.resize(1, stateSize);
		initial.fill(0L);
		//acceptable transitions for the empty columns are: 
		//Connected (one Single or one set of Upper / Mid* / Low)	to		-1
		//0															to		Single*
		//Otherwise transitions are most easily represented by considering a BINARY vector of allowable continuations, and
		//then converting that across to the three-bit version which depends on the previous state. 

		//First of all, form a binary mask that can be used to determine whether the given binary vector continues some bit 
		//of the graph from the previous state. Those are considered seperately at the end. 
		std::vector<unsigned long long> binaryMasks;
		generateBinaryMasks(states, binaryMasks);

		//Binary masks generated by all the different connected groups in the states.
		std::vector<std::vector<unsigned long long> > stateGroupBinaryMasks;
		generateStateGroupBinaryMasks(states, stateGroupBinaryMasks);

		unsigned long long finalBinaryEncoding = (1ULL << gridDimension);
		//A vector that lets us translate values for the binary vector to the index of that state in states.
		//E.g. we have a binary state 101010101, then state SINGLE,UPPER,LOWER,SINGLE,SINGLE (one value for each group) 
		//has a certain index within states.
		std::vector<std::size_t> cachedIndices;
		//This array will store a group identifier (a number) for every vertex that's present in the binary encoding. The number
		//is the same across the entire connected group that a vertex belongs to. Missing vertices get value 0, and the first
		//group starts at 1
		boost::scoped_array<char> binaryGroups(new char[gridDimension]);
		
		//This vector contains one value per binary group. Each value it a bitfield, so that if the ith bit is activated
		//then that binary group continues the ith group from the previous state.
		std::vector<unsigned long long> binaryGroupContinuations;

		//now iterate over all the binary encodings that can continue the current state.
		for(unsigned long long binaryEncoding = 1ULL; binaryEncoding < finalBinaryEncoding; binaryEncoding++)
		{
			memset(binaryGroups.get(), 0, sizeof(char)*gridDimension);
			//how many groups do we have in the binary encoding for the next state? Start off at 1 if the first bit is on, 0 otherwise
			int nGroups = (binaryEncoding & 1);
			binaryGroups[0] = nGroups;
			for(int i = 1; i < gridDimension; i++)
			{
				if(binaryEncoding & (1ULL << i))
				{
					//we've found a new group if the current one is on and the previous one was off
					if(!(binaryEncoding & (1ULL << (i-1))))
					{
						nGroups++;
					}
					binaryGroups[i] = nGroups;
				}
			}
			binaryGroupContinuations.resize(nGroups);
			//ok, how many different 2-bit values do we need to store? We're wasting space a little by allowing every combination,
			//the reality is that many will not be possible. But these values just don't get filled in...Note that we can use 
			//2 bits here because we no longer need to keep track of the OFF states. 
			generateCachedIndices(binaryEncoding, cachedIndices, nGroups, states);

			//iterate over the states.
			for(std::size_t i = 1; i < stateSize-1; i++)
			{
				const std::vector<unsigned long long> currentStateGroupBinaryMasks = stateGroupBinaryMasks[i];
				if(binaryEncoding & binaryMasks[i])
				{
					memset(&(binaryGroupContinuations[0]), 0, sizeof(unsigned long long)*nGroups);
					//ok, now to convert the binary data across to 3-bit
					for(std::size_t stateGroupMaskCounter = 0; stateGroupMaskCounter < currentStateGroupBinaryMasks.size(); stateGroupMaskCounter++)
					{
						int counter = 0;
						unsigned long long currentMask = currentStateGroupBinaryMasks[stateGroupMaskCounter];
						while(currentMask != 0)
						{
							if(currentMask & 1)
							{
								if(binaryGroups[counter] > 0) binaryGroupContinuations[binaryGroups[counter]-1] |= (1ULL << stateGroupMaskCounter);
							}
							currentMask >>= 1;
							counter++;
						}
					}
					//now to actually do the conversion
					std::size_t state2FromBinary = 0;
					for(int binaryGroupCounter = 0; binaryGroupCounter < nGroups; binaryGroupCounter++)
					{
						bool upperConnected = false;
						bool lowerConnected = false;
						for(int lowerCounter = 0; lowerCounter < binaryGroupCounter; lowerCounter++)
						{
							if(binaryGroupContinuations[lowerCounter] & binaryGroupContinuations[binaryGroupCounter])
							{
								lowerConnected  = true;
								break;
							}
						}
						for(int upperCounter = binaryGroupCounter+1; upperCounter < nGroups; upperCounter++)
						{
							if(binaryGroupContinuations[upperCounter] & binaryGroupContinuations[binaryGroupCounter])
							{
								upperConnected = true;
								break;
							}
						}
						if(upperConnected && lowerConnected) SETSTATE2(state2FromBinary, binaryGroupCounter, MID);
						else if(upperConnected) SETSTATE2(state2FromBinary, binaryGroupCounter, UPPER);
						else if(lowerConnected) SETSTATE2(state2FromBinary, binaryGroupCounter, LOWER);
						else SETSTATE2(state2FromBinary, binaryGroupCounter, SINGLE);
					}
					//Either every group has to continue, or there has to be just one group in which case it can die out
					bool isOK = false;
					if(currentStateGroupBinaryMasks.size() == 1)
					{
						isOK = true;
					}
					else
					{
						unsigned long long orEverything = 0;
						for(int binaryGroupCounter = 0; binaryGroupCounter < nGroups; binaryGroupCounter++) orEverything |= binaryGroupContinuations[binaryGroupCounter];
						isOK = (orEverything == (1ULL << currentStateGroupBinaryMasks.size())-1);
					}
					if(isOK)
					{
						std::size_t relevantState3Index = cachedIndices[state2FromBinary];
						T::addTransition(transitionMatrix, i, relevantState3Index);
						nonZeroCount++;
					}
				}
			}
		}
		//first put in the transitions FROM 0, to other stuff which includes some vertices. These are also the valid starting points. 
		for(std::size_t i = 1; i < stateSize-1; i++)
		{
			unsigned long long state = allStates[i];
			for(int j = 0; j < gridDimension; j++)
			{
				int currentVertexState = GETSTATE3(state, j);
				if(currentVertexState != OFF && currentVertexState != SINGLE) goto skipThisState;
			}
			T::addTransition(transitionMatrix, 0, i);
			initial(i) = 1L;
			nonZeroCount++;
skipThisState:
			;
		}
		//now put in the transitions to -1, from stuff that includes vertices. These are also the valid ending states.
		for(std::size_t i = 1; i < stateSize-1; i++)
		{
			unsigned long long state = allStates[i];
			bool isConnected = isConnectedState(state, gridDimension);
			//if it's connected and there's something there, add a transition to -1. These are also the valid ending points
			if(isConnected)
			{
				T::addTransition(transitionMatrix, i, stateSize-1);
				final(i) = 1L;
				nonZeroCount++;
			}
		}
		//transitions involving only -1 and 0

		//from -1 we can only go to -1
		T::addTransition(transitionMatrix, stateSize-1, stateSize - 1);
		//from 0 we can go to 0
		T::addTransition(transitionMatrix, 0, 0);
		nonZeroCount += 2;

		//....we can also start at 0, and end at -1 or 0
		final(stateSize - 1) = final(0) = 1L;
		initial(0) = 1L;

		outputInitial.swap(initial);
		outputTransitionMatrix.swap(transitionMatrix);
		outputFinal.swap(final);
	}
	struct constructMatricesDense_impl
	{
		typedef TransitionMatrix LargeIntMatrix;
		static inline void initMatrix(TransitionMatrix& transitionMatrix, std::size_t stateSize)
		{
			transitionMatrix.resize(stateSize, stateSize);
			transitionMatrix.fill(0L);
		}
		static inline void addTransition(TransitionMatrix& transitionMatrix, std::size_t first, std::size_t second)
		{
			transitionMatrix(first, second) = 1L;
		}
	};
	void constructMatricesDense(TransitionMatrix& outputTransitionMatrix, FinalColumnVector& outputFinal, InitialRowVector& outputInitial, const transferStates& states, std::size_t& nonZeroCount)
	{
		constructMatricesInternal<constructMatricesDense_impl>(outputTransitionMatrix, outputFinal, outputInitial, states, nonZeroCount);
	}

	struct constructMatricesSparse_impl
	{
		typedef LargeSparseIntMatrix LargeIntMatrix;
		static inline void addTransition(LargeIntMatrix& transitionMatrix, std::size_t first, std::size_t second)
		{
			transitionMatrix[first].push_back(second);
		}
		static inline void initMatrix(LargeIntMatrix& transitionMatrix, std::size_t stateSize)
		{
			transitionMatrix.resize(stateSize);
		}
	};
	void constructMatricesSparse(LargeSparseIntMatrix& outputTransitionMatrix, FinalColumnVector& outputFinal, InitialRowVector& outputInitial, const transferStates& states, std::size_t& nonZeroCount)
	{
		constructMatricesInternal<constructMatricesSparse_impl>(outputTransitionMatrix, outputFinal, outputInitial, states, nonZeroCount);
	}
	struct constructMatricesDenseNoCustomEigen_impl
	{
		typedef LargeDenseIntMatrix LargeIntMatrix;
		static inline void initMatrix(LargeDenseIntMatrix& transitionMatrix, std::size_t stateSize)
		{
			transitionMatrix.resize(stateSize, stateSize);
			transitionMatrix.fill(0L);
		}
		static inline void addTransition(LargeDenseIntMatrix& transitionMatrix, std::size_t first, std::size_t second)
		{
			transitionMatrix(first, second) = 1L;
		}
	};
	void constructMatricesDense(LargeDenseIntMatrix& outputTransitionMatrix, FinalColumnVector& outputFinal, InitialRowVector& outputInitial, const transferStates& states, std::size_t& nonZeroCount)
	{
		constructMatricesInternal<constructMatricesDenseNoCustomEigen_impl>(outputTransitionMatrix, outputFinal, outputInitial, states, nonZeroCount);
	}
}
