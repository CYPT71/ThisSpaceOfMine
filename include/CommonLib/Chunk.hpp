// Copyright (C) 2023 Jérôme "Lynix" Leclercq (lynix680@gmail.com)
// This file is part of the "This Space Of Mine" project
// For conditions of distribution and use, see copyright notice in Config.hpp

#pragma once

#ifndef TSOM_COMMONLIB_CHUNK_HPP
#define TSOM_COMMONLIB_CHUNK_HPP

#include <CommonLib/Direction.hpp>
#include <CommonLib/Export.hpp>
#include <CommonLib/VoxelBlock.hpp>
#include <NazaraUtils/EnumArray.hpp>
#include <NazaraUtils/Signal.hpp>
#include <Nazara/Core/Color.hpp>
#include <Nazara/Math/Matrix4.hpp>
#include <memory>
#include <optional>
#include <vector>

namespace Nz
{
	class JoltCollider3D;
	struct VertexStruct_XYZ_Color_UV;
}

namespace tsom
{
	class TSOM_COMMONLIB_API Chunk
	{
		public:
			inline Chunk(const Nz::Vector3ui& indices, const Nz::Vector3ui& size, float blockSize);
			Chunk(const Chunk&) = delete;
			Chunk(Chunk&&) = delete;
			virtual ~Chunk();

			virtual std::shared_ptr<Nz::JoltCollider3D> BuildCollider() const = 0;
			virtual void BuildMesh(std::vector<Nz::UInt32>& indices, std::vector<Nz::VertexStruct_XYZ_Color_UV>& vertices) const;

			virtual std::optional<Nz::Vector3ui> ComputeCoordinates(const Nz::Vector3f& position) const = 0;
			virtual Nz::EnumArray<Nz::BoxCorner, Nz::Vector3f> ComputeVoxelCorners(const Nz::Vector3ui& indices) const = 0;

			inline unsigned int GetBlockIndex(const Nz::Vector3ui& indices) const;
			inline VoxelBlock GetBlockContent(const Nz::Vector3ui& indices) const;
			inline float GetBlockSize() const;
			inline const VoxelBlock* GetContent() const;
			inline const Nz::Vector3ui& GetIndices() const;
			inline std::optional<VoxelBlock> GetNeighborBlock(Nz::Vector3ui indices, const Nz::Vector3i& offsets) const;
			inline const Nz::Vector3ui& GetSize() const;

			template<typename F> void InitBlocks(F&& func);

			inline void UpdateBlock(const Nz::Vector3ui& indices, VoxelBlock cellType);

			Chunk& operator=(const Chunk&) = delete;
			Chunk& operator=(Chunk&&) = delete;

			NazaraSignal(OnBlockUpdated, Chunk* /*emitter*/, const Nz::Vector3ui& /*indices*/, VoxelBlock /*newBlock*/);

		protected:
			std::vector<VoxelBlock> m_cells;
			Nz::Vector3ui m_indices;
			Nz::Vector3ui m_size;
			float m_blockSize;
	};
}

#include <CommonLib/Chunk.inl>

#endif // TSOM_COMMONLIB_CHUNK_HPP