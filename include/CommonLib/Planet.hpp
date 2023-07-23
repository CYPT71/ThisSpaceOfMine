// Copyright (C) 2023 Jérôme "Lynix" Leclercq (lynix680@gmail.com)
// This file is part of the "This Space Of Mine" project
// For conditions of distribution and use, see copyright notice in Config.hpp

#pragma once

#ifndef TSOM_COMMONLIB_PLANET_HPP
#define TSOM_COMMONLIB_PLANET_HPP

#include <CommonLib/DeformedChunk.hpp>
#include <CommonLib/Direction.hpp>
#include <CommonLib/Export.hpp>
#include <NazaraUtils/EnumArray.hpp>
#include <Nazara/Utility/VertexStruct.hpp>
#include <memory>
#include <tuple>
#include <vector>

namespace Nz
{
	class DebugDrawer;
	class JoltCollider3D;
}

namespace tsom
{
	class TSOM_COMMONLIB_API Planet
	{
		public:
			Planet(std::size_t gridDims, float tileSize, float cornerRadius);
			Planet(const Planet&) = delete;
			Planet(Planet&&) = delete;
			~Planet() = default;

			std::shared_ptr<Nz::JoltCollider3D> BuildCollider();

			std::optional<Nz::Vector3ui> ComputeGridCell(const Nz::Vector3f& position) const;

			inline Nz::Vector3f GetCenter() const;
			inline Chunk& GetChunk();
			inline const Chunk& GetChunk() const;
			inline float GetCornerRadius() const;
			inline std::size_t GetGridDimensions() const;
			inline float GetTileSize() const;

			inline void UpdateCornerRadius(float cornerRadius);

			Planet& operator=(const Planet&) = delete;
			Planet& operator=(Planet&&) = delete;

		protected:
			void BuildMesh(std::vector<Nz::UInt32>& indices, std::vector<Nz::VertexStruct_XYZ_Color_UV>& vertices);
			void RebuildGrid();

			std::size_t m_gridDimensions;
			std::unique_ptr<DeformedChunk> m_chunk;
			float m_tileSize;
			float m_cornerRadius;
	};
}

#include <CommonLib/Planet.inl>

#endif // TSOM_COMMONLIB_VOXELGRID_HPP
