// Copyright (C) 2023 Jérôme "Lynix" Leclercq (lynix680@gmail.com)
// This file is part of the "This Space Of Mine" project
// For conditions of distribution and use, see copyright notice in Config.hpp

#pragma once

#ifndef TSOM_CLIENTLIB_CLIENTBLOCKLIBRARY_HPP
#define TSOM_CLIENTLIB_CLIENTBLOCKLIBRARY_HPP

#include <ClientLib/Export.hpp>
#include <CommonLib/BlockLibrary.hpp>

namespace Nz
{
	class ApplicationBase;
	class RenderDevice;
	class Texture;
}

namespace tsom
{
	class TSOM_CLIENTLIB_API ClientBlockLibrary : public BlockLibrary
	{
		public:
			inline ClientBlockLibrary(Nz::ApplicationBase& applicationBase, Nz::RenderDevice& renderDevice);
			~ClientBlockLibrary() = default;

			void BuildTexture();

			inline const std::shared_ptr<Nz::Texture>& GetBaseColorTexture() const;
			inline const std::shared_ptr<Nz::Texture>& GetDetailTexture() const;
			inline const std::shared_ptr<Nz::Texture>& GetNormalTexture() const;

		private:
			std::shared_ptr<Nz::Texture> m_baseColorTexture;
			std::shared_ptr<Nz::Texture> m_detailTexture;
			std::shared_ptr<Nz::Texture> m_normalTexture;
			Nz::ApplicationBase& m_applicationBase;
			Nz::RenderDevice& m_renderDevice;
	};
}

#include <ClientLib/ClientBlockLibrary.inl>

#endif // TSOM_CLIENTLIB_CLIENTBLOCKLIBRARY_HPP
