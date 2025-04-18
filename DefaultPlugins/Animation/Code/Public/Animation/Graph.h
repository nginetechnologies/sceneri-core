#pragma once

#include "Blendspace1D.h"

#include <Common/Memory/Variant.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Math/Vector2.h>

namespace ngine::Animation
{
	struct Graph
	{
		using Blendspace = Variant<Blendspace1DBase, Blendspace1D>;

		struct Entry
		{
			Blendspace m_blendspace;
			// Blend between two entries
			UniquePtr<Entry> m_pOtherEntry;
		};

		/*m_animationGraph.Compile(
		  ngine::Animation::Graph::Definition{
		    ngine::Animation::Graph::Definition::Entry
		    {
		      // Move blendspace
		      ngine::Animation::Graph::Definition::OneDimensional
		      {
		        "53c36449-0afa-429c-aa76-48d12595fd43"_asset,
		                    "bf28bd5e-c139-4d08-a405-95f619034e23"_asset,
		                    "6bcd03b0-4a18-4fa1-bd9b-434c849ae740"_asset
		      },
		      // Jump blendspace
		      ngine::Animation::Graph::Definition::OneDimensional
		      {
		        "53c36449-0afa-429c-aa76-48d12595fd43"_asset,
		                    "bf28bd5e-c139-4d08-a405-95f619034e23"_asset,
		                    "6bcd03b0-4a18-4fa1-bd9b-434c849ae740"_asset
		      }
		    }
		  }
		);

		m_animationGraphNew.Compile(
		  ngine::Animation::Graph::Definition{
		    ngine::Animation::Graph::Definition::Entry
		    {
		      ngine::Animation::Graph::Definition::OneDimensional
		      {
		        // Final move
		        ngine::Animation::Graph::Definition::OneDimensional
		        {
		          // Default move
		          ngine::Animation::Graph::Definition::OneDimensional
		          {
		            // Idle
		            "53c36449-0afa-429c-aa76-48d12595fd43"_asset,
		            // Walk
		            "bf28bd5e-c139-4d08-a405-95f619034e23"_asset,
		            // Run
		            "6bcd03b0-4a18-4fa1-bd9b-434c849ae740"_asset
		          },
		          // Final aim
		          ngine::Animation::Graph::Definition::OneDimensional
		          {
		            // Aim in
		            "53c36449-0afa-429c-aa76-48d12595fd43"_asset,
		            // Aim move
		            ngine::Animation::Graph::Definition::OneDimensional
		            {
		              // Aim idle
		              "53c36449-0afa-429c-aa76-48d12595fd43"_asset,
		              // Aim walk
		              "bf28bd5e-c139-4d08-a405-95f619034e23"_asset,
		              // Aim run
		              "6bcd03b0-4a18-4fa1-bd9b-434c849ae740"_asset
		            },
		            // Aim out
		            "6bcd03b0-4a18-4fa1-bd9b-434c849ae740"_asset
		          }
		        },
		        // Jump
		        ngine::Animation::Graph::Definition::OneDimensional
		        {
		          // Start jump
		          "53c36449-0afa-429c-aa76-48d12595fd43"_asset,
		          // Jump idle loop (in air)
		          "bf28bd5e-c139-4d08-a405-95f619034e23"_asset,
		          // Land
		          "6bcd03b0-4a18-4fa1-bd9b-434c849ae740"_asset
		        }
		      },
		      // One shots
		      ngine::Animation::Graph::Definition::DynamicAnimation
		      {
		        "53c36449-0afa-429c-aa76-48d12595fd43"_asset
		      }
		    }
		  }
		);*/
		/*struct Definition
		{
		  //! Animation that can be switched out at runtime
		  //! A default always has to be provided.
		  struct DynamicAnimation : public Asset::Guid
		  {
		    using BaseType = Asset::Guid;
		    using BaseType::BaseType;
		  };

		  struct Entry;
		  struct OneDimensional;
		  struct TwoDimensional;

		  struct Value : public Variant<Asset::Guid, DynamicAnimation, ReferenceWrapper<const OneDimensional>, ReferenceWrapper<const Entry>>
		  {
		    using BaseType = Variant<Asset::Guid, DynamicAnimation, ReferenceWrapper<const OneDimensional>, ReferenceWrapper<const Entry>>;
		    using BaseType::BaseType;
		    Value(const OneDimensional& entry)
		      : BaseType(ReferenceWrapper<const OneDimensional>(entry))
		    {
		    }
		    Value(const Entry& entry)
		      : BaseType(ReferenceWrapper<const Entry>(entry))
		    {
		    }
		  };

		  struct OneDimensional
		  {
		    OneDimensional() = delete;

		    template<typename... Values>
		    OneDimensional(Values&&... values)
		      : m_values(Array<const Value, sizeof...(Values)> { Forward<Values>(values)... })
		    {
		    }

		    ArrayView<const Value, uint8> m_values;
		  };
		  struct TwoDimensional
		  {
		    TwoDimensional() = delete;

		    template<typename... Values>
		    TwoDimensional(const Math::TVector2<uint8> gridSize, Values&&... values)
		      : m_gridSize(gridSize)
		      , m_values(Array<const Value, sizeof...(Values)> { Forward<Values>(values)... })
		    {
		    }

		    Math::TVector2<uint8> m_gridSize;
		    ArrayView<const Value, uint8> m_values;
		  };

		  struct Entry
		  {
		    using ContainedValue = Variant<OneDimensional, TwoDimensional>;

		    Entry() = delete;

		    template<typename... Values>
		    Entry(Values&&... values)
		      : values(Array<const ContainedValue, sizeof...(Values)> { values... })
		    {
		    }

		    ArrayView<const ContainedValue, uint8> values;
		  };

		  Definition(Entry&& blend)
		    : m_entry(Forward<Entry>(blend))
		  {
		  }

		  Entry m_entry;
		};
		void Compile(const Definition& definition);*/

		void Update(const float deltaTime);

		// Should reuse the same temp sampled transforms for the whole frame to avoid having one per entry
		// TODO Vector<ozz::math::SoaTransform, uint32> m_sampledTransforms;

		Entry m_rootEntry;
	};
};
