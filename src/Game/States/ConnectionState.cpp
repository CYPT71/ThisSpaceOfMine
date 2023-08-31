// Copyright (C) 2023 Jérôme "Lynix" Leclercq (lynix680@gmail.com)
// This file is part of the "This Space Of Mine" project
// For conditions of distribution and use, see copyright notice in Config.hpp

#include <Game/States/ConnectionState.hpp>
#include <Game/States/BackgroundState.hpp>
#include <Game/States/GameState.hpp>
#include <ClientLib/ClientSessionHandler.hpp>
#include <CommonLib/SessionHandler.hpp>
#include <Nazara/Core/StateMachine.hpp>
#include <Nazara/Utility/SimpleTextDrawer.hpp>
#include <Nazara/Widgets/LabelWidget.hpp>
#include <fmt/format.h>

namespace tsom
{
	ConnectionState::ConnectionState(std::shared_ptr<StateData> stateData) :
	WidgetState(std::move(stateData)),
	m_connectingLabel(nullptr)
	{
		m_connectingLabel = CreateWidget<Nz::LabelWidget>();
	}

	void ConnectionState::Connect(const Nz::IpAddress& serverAddress, std::string nickname, std::shared_ptr<Nz::State> previousState)
	{
		Disconnect();

		m_previousState = std::move(previousState);
		m_nickname = std::move(nickname);

		// Find a compatible reactor
		NetworkReactor* reactor = nullptr;
		for (auto& reactorCandidate : m_reactors)
		{
			if (reactorCandidate.GetProtocol() == serverAddress.GetProtocol())
			{
				reactor = &reactorCandidate;
				break;
			}
		}

		if (!reactor)
		{
			constexpr std::size_t MaxConnection = 5;

			if (m_reactors.size() == m_reactors.capacity())
				throw std::runtime_error("unable to allocate a new reactor (this shouldn't happen)");

			reactor = &m_reactors.emplace_back(MaxConnection * m_reactors.size(), serverAddress.GetProtocol(), 0, MaxConnection);
		}

		std::size_t peerId = reactor->ConnectTo(serverAddress);
		m_serverSession.emplace(*reactor, peerId, serverAddress);
		ClientSessionHandler& sessionHandler = m_serverSession->SetupHandler<ClientSessionHandler>(*GetStateData().world);

		auto& stateData = GetStateData();
		stateData.networkSession = &m_serverSession.value();
		stateData.sessionHandler = &sessionHandler;

		m_connectingLabel->UpdateText(Nz::SimpleTextDrawer::Draw("Connecting to " + serverAddress.ToString() + "...", 48));
		m_connectingLabel->Center();
		m_connectingLabel->Show();

		m_connectedState = std::make_shared<GameState>(GetStateDataPtr());
	}

	void ConnectionState::Disconnect()
	{
		if (m_serverSession)
		{
			m_serverSession->Disconnect();

			auto& stateData = GetStateData();
			stateData.networkSession = nullptr;
			stateData.sessionHandler = nullptr;
		}

		m_connectingLabel->Hide();
	}

	void ConnectionState::LayoutWidgets(const Nz::Vector2f& newSize)
	{
		m_connectingLabel->Center();
	}

	bool ConnectionState::Update(Nz::StateMachine& fsm, Nz::Time elapsedTime)
	{
		auto ConnectionHandler = [&]([[maybe_unused]] bool outgoingConnection, std::size_t peerIndex, const Nz::IpAddress& remoteAddress, [[maybe_unused]] Nz::UInt32 data)
		{
			if (!m_serverSession || m_serverSession->GetPeerId() != peerIndex)
				return;

			m_connectingLabel->UpdateText(Nz::SimpleTextDrawer::Draw("Authenticating...", 48));
			m_connectingLabel->Center();
			m_connectingLabel->Show();

			Packets::AuthRequest request;
			request.nickname = m_nickname;

			m_serverSession->SendPacket(request);

			m_nextState = std::move(m_connectedState);
			m_nextStateTimer = Nz::Time::Milliseconds(500);
		};

		auto DisconnectionHandler = [&](std::size_t peerIndex, [[maybe_unused]] Nz::UInt32 data, bool timeout)
		{
			if (!m_serverSession || m_serverSession->GetPeerId() != peerIndex)
				return;

			if (timeout)
			{
				m_connectingLabel->UpdateText(Nz::SimpleTextDrawer::Draw("Connection lost.", 48, Nz::TextStyle_Regular, Nz::Color::Red()));
				m_connectingLabel->Center();
				m_connectingLabel->Show();

				m_nextState = m_previousState;
				m_nextStateTimer = Nz::Time::Milliseconds(2000);
			}
			else
			{
				m_nextState = m_previousState;
				m_nextStateTimer = Nz::Time::Milliseconds(200);
			}

			fmt::print("Disconnected from server\n");
			m_serverSession.reset();
		};

		auto PacketHandler = [&](std::size_t peerIndex, Nz::NetPacket&& packet)
		{
			if NAZARA_UNLIKELY(!m_serverSession || m_serverSession->GetPeerId() != peerIndex)
				return;

			m_serverSession->HandlePacket(std::move(packet));
		};

		for (auto& reactor : m_reactors)
			reactor.Poll(ConnectionHandler, DisconnectionHandler, PacketHandler);

		if (m_nextState)
		{
			m_nextStateTimer -= elapsedTime;
			if (m_nextStateTimer <= Nz::Time::Zero())
			{
				fsm.PopStatesUntil(shared_from_this());
				if (m_nextState == m_previousState)
					fsm.PushState(std::make_shared<BackgroundState>(GetStateDataPtr()));
				fsm.PushState(m_nextState);
				m_nextState = nullptr;

				m_connectingLabel->Hide();
			}
		}

		return true;
	}
}
