// Copyright (C) 2024 Jérôme "SirLynix" Leclercq (lynix680@gmail.com)
// This file is part of the "This Space Of Mine" project
// For conditions of distribution and use, see copyright notice in LICENSE

#include <CommonLib/ChunkEntities.hpp>
#include <CommonLib/BlockLibrary.hpp>
#include <CommonLib/Components/ChunkComponent.hpp>
#include <CommonLib/Components/EntityOwnerComponent.hpp>
#include <Nazara/Core/ApplicationBase.hpp>
#include <Nazara/Core/EnttWorld.hpp>
#include <Nazara/Core/TaskSchedulerAppComponent.hpp>
#include <Nazara/Core/Components/NodeComponent.hpp>
#include <Nazara/Physics3D/Components/RigidBody3DComponent.hpp>
#include <cassert>

namespace tsom
{
	ChunkEntities::ChunkEntities(Nz::ApplicationBase& application, Nz::EnttWorld& world, ChunkContainer& chunkContainer, const BlockLibrary& blockLibrary) :
	ChunkEntities(application, world, chunkContainer, blockLibrary, NoInit{})
	{
		FillChunks();
	}

	ChunkEntities::ChunkEntities(Nz::ApplicationBase& application, Nz::EnttWorld& world, ChunkContainer& chunkContainer, const BlockLibrary& blockLibrary, NoInit) :
	m_application(application),
	m_world(world),
	m_blockLibrary(blockLibrary),
	m_chunkContainer(chunkContainer)
	{
		m_onChunkAdded.Connect(chunkContainer.OnChunkAdded, [this](ChunkContainer* /*emitter*/, Chunk* chunk)
		{
			CreateChunkEntity(chunk->GetIndices(), chunk);
		});

		m_onChunkRemove.Connect(chunkContainer.OnChunkRemove, [this](ChunkContainer* /*emitter*/, Chunk* chunk)
		{
			DestroyChunkEntity(chunk->GetIndices());
		});

		m_onChunkUpdated.Connect(chunkContainer.OnChunkUpdated, [this](ChunkContainer* /*emitter*/, Chunk* chunk)
		{
			m_invalidatedChunks.insert(chunk->GetIndices());
		});
	}

	ChunkEntities::~ChunkEntities()
	{
		for (auto it = m_chunkEntities.begin(); it != m_chunkEntities.end(); ++it)
		{
			entt::handle entity = it.value();
			if (entity)
				entity.destroy();
		}
	}

	void ChunkEntities::SetParentEntity(entt::handle entity)
	{
		m_parentEntity = entity;
		if (m_parentEntity)
		{
			auto& parentNode = m_parentEntity.get<Nz::NodeComponent>();
			m_onParentNodeInvalidated.Connect(parentNode.OnNodeInvalidation, this, &ChunkEntities::OnParentNodeInvalidated);
		}
		else
			m_onParentNodeInvalidated.Disconnect();
	}

	void ChunkEntities::Update()
	{
		for (auto it = m_updateJobs.begin(); it != m_updateJobs.end(); )
		{
			UpdateJob& job = *it->second;
			if (job.executionCounter != job.taskCount)
			{
				++it;
				continue;
			}

			job.applyFunc(it->first, std::move(job));
			it = m_updateJobs.erase(it);
		}

		for (const ChunkIndices& chunkIndices : m_invalidatedChunks)
			UpdateChunkEntity(chunkIndices);

		m_invalidatedChunks.clear();
	}

	void ChunkEntities::CreateChunkEntity(const ChunkIndices& chunkIndices, Chunk* chunk)
	{
		entt::handle chunkEntity = m_world.CreateEntity();

		auto& nodeComponent = chunkEntity.emplace<Nz::NodeComponent>(m_chunkContainer.GetChunkOffset(chunkIndices));
		if (m_parentEntity)
		{
			m_parentEntity.get_or_emplace<EntityOwnerComponent>().Register(chunkEntity);
			nodeComponent.SetParent(m_parentEntity);
		}

		auto& chunkComponent = chunkEntity.emplace<ChunkComponent>();
		chunkComponent.chunk = chunk;
		chunkComponent.parentEntity = m_parentEntity;

		chunkEntity.emplace<Nz::RigidBody3DComponent>(Nz::RigidBody3D::StaticSettings(nullptr));

		assert(!m_chunkEntities.contains(chunkIndices));
		m_chunkEntities.insert_or_assign(chunkIndices, chunkEntity);

		HandleChunkUpdate(chunkIndices, chunk);
	}

	void ChunkEntities::DestroyChunkEntity(const ChunkIndices& chunkIndices)
	{
		if (auto it = m_updateJobs.find(chunkIndices); it != m_updateJobs.end())
		{
			UpdateJob& job = *it->second;
			job.cancelled = true;

			m_updateJobs.erase(chunkIndices);
		}

		if (auto it = m_chunkEntities.find(chunkIndices); it != m_chunkEntities.end())
		{
			it.value().destroy();
			m_chunkEntities.erase(it);
		}

		m_invalidatedChunks.erase(chunkIndices);
	}

	void ChunkEntities::FillChunks()
	{
		m_chunkContainer.ForEachChunk([this](const ChunkIndices& chunkIndices, Chunk& chunk)
		{
			CreateChunkEntity(chunkIndices, &chunk);
		});
	}

	void ChunkEntities::HandleChunkUpdate(const ChunkIndices& chunkIndices, const Chunk* chunk)
	{
		// Try to cancel current update job to void useless work
		if (auto it = m_updateJobs.find(chunkIndices); it != m_updateJobs.end())
		{
			UpdateJob& job = *it->second;
			job.cancelled = true;
		}

		std::shared_ptr<ColliderUpdateJob> updateJob = std::make_shared<ColliderUpdateJob>();
		updateJob->taskCount = 1;

		updateJob->applyFunc = [this](const ChunkIndices& chunkIndices, UpdateJob&& job)
		{
			ColliderUpdateJob&& colliderUpdateJob = static_cast<ColliderUpdateJob&&>(job);

			entt::handle chunkEntity = Nz::Retrieve(m_chunkEntities, chunkIndices);
			auto& rigidBody = chunkEntity.get<Nz::RigidBody3DComponent>();
			rigidBody.SetGeom(std::move(colliderUpdateJob.collider), false);
		};

		auto& taskScheduler = m_application.GetComponent<Nz::TaskSchedulerAppComponent>();
		taskScheduler.AddTask([this, chunk, updateJob]
		{
			if (updateJob->cancelled)
				return;

			chunk->LockRead();
			updateJob->collider = chunk->BuildCollider(m_blockLibrary);
			chunk->UnlockRead();

			updateJob->executionCounter++;
		});

		m_updateJobs.insert_or_assign(chunkIndices, std::move(updateJob));
	}

	void ChunkEntities::OnParentNodeInvalidated(const Nz::Node* /*node*/)
	{
		// Refresh physical position
		for (auto it = m_chunkEntities.begin(); it != m_chunkEntities.end(); ++it)
		{
			entt::handle chunkEntity = it->second;

			// FIXME: This signal may be triggered while entities are destroyed
			if (!chunkEntity.all_of<Nz::NodeComponent, Nz::RigidBody3DComponent>())
				continue;

			auto& chunkNode = chunkEntity.get<Nz::NodeComponent>();
			auto& rigidBody = chunkEntity.get<Nz::RigidBody3DComponent>();
			rigidBody.TeleportTo(chunkNode.GetGlobalPosition(), chunkNode.GetGlobalRotation());
		}
	}

	void ChunkEntities::UpdateChunkEntity(const ChunkIndices& chunkIndices)
	{
		assert(m_chunkEntities.contains(chunkIndices));
		const Chunk* chunk = m_chunkContainer.GetChunk(chunkIndices);
		assert(chunk);

		HandleChunkUpdate(chunkIndices, chunk);
	}
}
