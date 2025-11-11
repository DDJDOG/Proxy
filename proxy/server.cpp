#pragma once
#include "server.h"
#include <iostream>
#include <vector>
#include <memory>
#include "events.h"
#include "gt.hpp"
#include "proton/hash.hpp"
#include "proton/rtparam.hpp"
#include "utils.h"
#include "print.h"

void server::handle_outgoing() {
    ENetEvent evt;
    while (enet_host_service(m_proxy_server, &evt, 0) > 0) {
        m_gt_peer = evt.peer;

        switch (evt.type) {
        case ENET_EVENT_TYPE_CONNECT:
            if (!this->connect()) return;
            break;

        case ENET_EVENT_TYPE_RECEIVE: {
            int packet_type = get_packet_type(evt.packet);

            switch (packet_type) {
            case NET_MESSAGE_GENERIC_TEXT:
                if (events::out::generictext(utils::get_text(evt.packet))) {
                    enet_packet_destroy(evt.packet);
                    return;
                }
                break;

            case NET_MESSAGE_GAME_MESSAGE:
                if (events::out::gamemessage(utils::get_text(evt.packet))) {
                    enet_packet_destroy(evt.packet);
                    return;
                }
                break;

            case NET_MESSAGE_GAME_PACKET: {
                auto packet = utils::get_struct(evt.packet);
                if (!packet) break;

                switch (packet->m_type) {
                case PACKET_STATE:
                    if (events::out::state(packet)) {
                        enet_packet_destroy(evt.packet);
                        return;
                    }
                    break;
                case PACKET_CALL_FUNCTION:
                    if (events::out::variantlist(packet)) {
                        enet_packet_destroy(evt.packet);
                        return;
                    }
                    break;
                case PACKET_PING_REPLY:
                    if (events::out::pingreply(packet)) {
                        enet_packet_destroy(evt.packet);
                        return;
                    }
                    break;
                case PACKET_DISCONNECT:
                case PACKET_APP_INTEGRITY_FAIL:
                    if (gt::in_game) return;
                    break;
                default:
                    PRINTS("gamepacket type: %d\n", packet->m_type);
                }
            } break;

            case NET_MESSAGE_TRACK:
            case NET_MESSAGE_CLIENT_LOG_RESPONSE:
                return;

            default:
                PRINTS("Got unknown packet of type %d.\n", packet_type);
                break;
            }

            if (!m_server_peer || !m_real_server) return;
            enet_peer_send(m_server_peer, 0, evt.packet);
            enet_host_flush(m_real_server);

        } break;

        case ENET_EVENT_TYPE_DISCONNECT:
            if (gt::in_game) return;
            if (gt::connecting) {
                this->disconnect(false);
                gt::connecting = false;
                return;
            }
            break;

        default:
            PRINTS("UNHANDLED\n");
            break;
        }
    }
}

std::vector<server::Item> server::inventory;

void server::handle_incoming() {
    ENetEvent event;

    while (enet_host_service(m_real_server, &event, 0) > 0) {
        switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT:
            PRINTC("connection event\n");
            break;

        case ENET_EVENT_TYPE_DISCONNECT:
            this->disconnect(true);
            return;

        case ENET_EVENT_TYPE_RECEIVE: {
            if (!event.packet->data) break;

            int packet_type = get_packet_type(event.packet);

            switch (packet_type) {
            case NET_MESSAGE_GENERIC_TEXT:
                if (events::in::generictext(utils::get_text(event.packet))) {
                    enet_packet_destroy(event.packet);
                    return;
                }
                break;

            case NET_MESSAGE_GAME_MESSAGE:
                if (events::in::gamemessage(utils::get_text(event.packet))) {
                    enet_packet_destroy(event.packet);
                    return;
                }
                break;

            case NET_MESSAGE_GAME_PACKET: {
                auto packet = utils::get_struct(event.packet);
                if (!packet) break;

                switch (packet->m_type) {
                case PACKET_SEND_INVENTORY_STATE:
                    break; // custom handling placeholder
                case 8:
                    if (!packet->m_int_data) {
                        if (events::out::dicespeed) {
                            try {
                                std::string dice_roll = std::to_string(packet->m_count + 1);
                                gt::send_log("`bThe dice `bwill roll a `#" + dice_roll);
                            } catch (...) {
                                gt::send_log("`bDice Error");
                            }
                        }
                        if (events::out::visdice) {
                            try {
                                packet->m_count = events::out::sayi;
                            } catch (...) {}
                        }
                    }
                    break;
                case PACKET_PING_REQUEST: {
                    gameupdatepacket_t pkt{};
                    pkt.m_type = PACKET_PING_REPLY;
                    pkt.m_int_data = packet->m_int_data;
                    pkt.m_vec_x = 64.f;
                    pkt.m_vec_y = 64.f;
                    pkt.m_vec2_x = 1000.f;
                    pkt.m_vec2_y = 250.f;
                    g_server->send(true, NET_MESSAGE_GAME_PACKET, reinterpret_cast<uint8_t*>(&pkt), sizeof(pkt));
                    gt::send_log("`b[`#S`b]:Ping reply was successfully `2sent!`b(Saved you from a DC or a possible ban)");
                } break;

                case PACKET_CALL_FUNCTION:
                    if (events::in::variantlist(packet)) {
                        enet_packet_destroy(event.packet);
                        return;
                    }
                    break;

                case PACKET_SEND_MAP_DATA:
                    if (events::in::sendmapdata(packet)) {
                        enet_packet_destroy(event.packet);
                        return;
                    }
                    break;

                case PACKET_STATE:
                    if (events::in::state(packet)) {
                        enet_packet_destroy(event.packet);
                        return;
                    }
                    break;

                default:
                    PRINTC("gamepacket type: %d\n", packet->m_type);
                    break;
                }
            } break;

            case NET_MESSAGE_TRACK:
                if (events::in::tracking(utils::get_text(event.packet))) {
                    enet_packet_destroy(event.packet);
                    return;
                }
                break;

            case NET_MESSAGE_CLIENT_LOG_REQUEST:
                return;

            default:
                PRINTS("Got unknown packet of type %d.\n", packet_type);
                break;
            }

            if (!m_gt_peer || !m_proxy_server) return;
            enet_peer_send(m_gt_peer, 0, event.packet);
            enet_host_flush(m_proxy_server);

        } break;

        default:
            PRINTC("UNKNOWN event: %d\n", event.type);
            break;
        }
    }
}

void server::poll() {
    handle_outgoing();
    if (m_real_server) handle_incoming();
}

bool server::start() {
    ENetAddress address;
    enet_address_set_host(&address, create.c_str());
    address.port = m_proxyport;
    m_proxy_server = enet_host_create(&address, 1024, 10, 0, 0);

    if (!m_proxy_server) {
        PRINTS("failed to start the proxy server!\n");
        return false;
    }

    m_proxy_server->checksum = enet_crc32;
    if (enet_host_compress_with_range_coder(m_proxy_server) != 0)
        PRINTS("enet host compressing failed\n");

    PRINTS("started the enet server.\n");
    return setup_client();
}

void server::quit() {
    gt::in_game = false;
    disconnect(true);
}

bool server::setup_client() {
    m_real_server = enet_host_create(nullptr, 1, 2, 0, 0);
    if (!m_real_server) {
        PRINTC("failed to start the client\n");
        return false;
    }

    m_real_server->usingNewPacket = true;
    m_real_server->checksum = enet_crc32;
    if (enet_host_compress_with_range_coder(m_real_server) != 0)
        PRINTC("enet host compressing failed\n");

    enet_host_flush(m_real_server);
    PRINTC("Started enet client\n");
    return true;
}

// Unified disconnect
void server::disconnect(bool reset) {
    m_world.connected = false;
    m_world.local = {};
    m_world.players.clear();

    if (m_server_peer) {
        enet_peer_disconnect(m_server_peer, 0);
        m_server_peer = nullptr;
    }

    if (m_real_server) {
        enet_host_destroy(m_real_server);
        m_real_server = nullptr;
    }

    if (reset) {
        m_user = 0;
        m_token = 0;
        m_server = serverz;
        m_port = portz;
    }
}

bool server::connect() {
    print::set_color(LightGreen);
    PRINTS("Connecting to server.\n");

    ENetAddress address;
    enet_address_set_host(&address, m_server.c_str());
    address.port = m_port;
    PRINTS("port is %d and server is %s\n", m_port, m_server.c_str());

    if (!setup_client()) {
        PRINTS("Failed to setup client when trying to connect to server!\n");
        return false;
    }

    m_server_peer = enet_host_connect(m_real_server, &address, 2, 0);
    if (!m_server_peer) {
        PRINTS("Failed to connect to real server.\n");
        return false;
    }

    return true;
}

