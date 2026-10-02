#include "rpc_server.hpp"

#include "config/config.hpp"
#include "constants/version.hpp"
#include "contact/client_contact.hpp"
#include "router/router.hpp"
#include "rpc/rpc_request_definitions.hpp"
#include "rpc_request.hpp"

#include <nlohmann/json.hpp>
#include <oxenc/base32z.h>

#include <exception>
#include <vector>

namespace srouter::rpc
{
    static auto logcat = srouter::log::Cat("rpc-server");

    template <typename T>
        requires std::derived_from<T, RPCRequest>
    static void log_print_rpc(T& req)
    {
        log::info(logcat, "RPC Server received request for endpoint `{}`", req.name);
    }

#if 0
    // Fake packet source that serializes repsonses back into dns
    class DummyPacketSource final : public dns::PacketSource
    {
        std::function<void(std::optional<dns::Message>)> func;

      public:
        explicit DummyPacketSource(std::function<void(std::optional<dns::Message>)> func) : func{std::move(func)} {}

        bool would_loop(const quic::Address&, const quic::Address&) const override { return false; };

        /// send packet with src and dst address containing buf on this packet source
        void send_udp(const quic::Address&, const quic::Address&, std::span<const std::byte> payload) const override
        {
            func(dns::Message::extract(payload));
        }

        /// returns the sockaddr we are bound on if applicable
        std::optional<quic::Address> bound_on() const override { return std::nullopt; }
    };
#endif

    bool check_path(std::string path)
    {
        for (auto c : path)
        {
            if (not((c >= '0' and c <= '9') or (c >= 'A' and c <= 'Z') or (c >= 'a' and c <= 'z') or (c == '_')
                    or (c == '-')))
            {
                return false;
            }
        }

        return true;
    }

    template <typename RPC>
    void register_rpc_command(std::unordered_map<std::string, rpc_callback>& regs)
    {
        static_assert(std::is_base_of_v<RPCRequest, RPC>);
        rpc_callback cback{};

        cback.invoke = make_invoke<RPC>();

        regs.emplace(RPC::name, std::move(cback));
    }

    RPCServer::RPCServer(oxenmq::OxenMQ& omq, Router& r) : _omq{omq}, _router(r)
    {
        if (srouter::logRingBuffer)
            log_subs.emplace(_omq, srouter::logRingBuffer);

        for (const auto& addr : _router.config().api.rpc_bind_addrs)
        {
            _omq.listen_plain(addr);
            log::debug(logcat, "Bound RPC server to {}", addr);
        }

        AddCategories();
    }

    template <typename... RPC>
    std::unordered_map<std::string, rpc_callback> register_rpc_requests(tools::type_list<RPC...>)
    {
        std::unordered_map<std::string, rpc_callback> regs;

        (register_rpc_command<RPC>(regs), ...);

        return regs;
    }

    const std::unordered_map<std::string, rpc_callback> rpc_request_map =
        register_rpc_requests(rpc::rpc_request_types{});

    bool RPCServer::require_api_auth(oxenmq::Message& msg, std::string_view command) const
    {
        // Privileged ops must not rely on AuthLevel::none alone. Require [api] auth= match.
        const auto& need = _router.config().api.auth;
        if (need.empty())
        {
            // Should be unreachable when API is correctly configured; fail closed.
            msg.send_reply(nlohmann::json{{"error", "API auth is not configured; refusing '{}'"_format(command)}}.dump());
            return false;
        }

        std::string got;
        try
        {
            if (not msg.data.empty() and not msg.data[0].empty() and msg.data[0].front() != 'd')
            {
                auto j = nlohmann::json::parse(msg.data[0]);
                if (j.contains("auth") and j["auth"].is_string())
                    got = j["auth"].get<std::string>();
            }
        }
        catch (...)
        {
            // parse errors handled by make_invoke; treat as missing auth here
        }

        if (got != need)
        {
            msg.send_reply(nlohmann::json{{"error", "Unauthorized: '{}' requires matching [api] auth"_format(command)}}.dump());
            return false;
        }
        return true;
    }

    void RPCServer::AddCategories()
    {
        // Category remains AuthLevel::none for oxenmq plain loopback sockets, but privileged
        // commands below also require the [api] auth= credential (AuthLevel::none alone is
        // forbidden for privileged ops).
        _omq.add_category("llarp", oxenmq::AuthLevel::none).add_request_command("logs", [this](oxenmq::Message& msg) {
            if (not require_api_auth(msg, "logs"))
                return;
            HandleLogsSubRequest(msg);
        });

        for (auto& req : rpc_request_map)
        {
            _omq.add_request_command(
                "llarp",
                req.first,
                [name = std::string_view{req.first}, &call = req.second, this](oxenmq::Message& m) {
                    // version is still gated by auth when API is on (fail closed).
                    if (not require_api_auth(m, name))
                        return;
                    call.invoke(m, *this);
                });
        }
    }

    void RPCServer::invoke(Halt& halt)
    {
        log_print_rpc(halt);

        if (not _router.is_running())
        {
            SetJSONError("Router is not running", halt.response);
            return;
        }

        _router._jq->call_soon([&]() { _router.stop(); });

        SetJSONResponse("OK", halt.response);
    }

    void RPCServer::invoke(Version& version)
    {
        log_print_rpc(version);

        nlohmann::json result{
            {"version", srouter::VERSION}, {"version_full", srouter::VERSION_FULL}, {"uptime", to_json(uptime())}};

        SetJSONResponse(result, version.response);
    }

    void RPCServer::invoke(Status& status)
    {
        log_print_rpc(status);

        // TODO: this
    }

    void RPCServer::invoke(GetStatus& getstatus)
    {
        log_print_rpc(getstatus);

        // TODO: this
    }

    void RPCServer::invoke(QuicConnect& quicconnect)
    {
        log_print_rpc(quicconnect);

        auto& req = quicconnect.request;

        if (req.port == 0 and req.closeID == 0)
        {
            SetJSONError("Port not provided", quicconnect.response);
            return;
        }

        if (req.remoteHost.empty() and req.closeID == 0)
        {
            SetJSONError("Host not provided", quicconnect.response);
            return;
        }

        // auto endpoint =
        //     (req.endpoint.empty()) ? GetEndpointByName(_router, "default") : GetEndpointByName(_router,
        //     req.endpoint);

        // if (not endpoint)
        // {
        //     SetJSONError("No such local endpoint found.", quicconnect.response);
        //     return;
        // }

        // auto quic = endpoint->GetQUICTunnel();

        // if (not quic)
        // {
        //     SetJSONError("No quic interface available on endpoint " + req.endpoint, quicconnect.response);
        //     return;
        // }

        if (req.closeID)
        {
            // TODO:
            // quic->forget(req.closeID);
            SetJSONResponse("OK", quicconnect.response);
            return;
        }

        quic::Address laddr{req.bindAddr, req.port};

        try
        {
            // TODO:
            // auto [addr, id] = quic->open(
            //     req.remoteHost, req.port, [](auto&&) {}, laddr);

            nlohmann::json status;
            // status["addr"] = addr.to_string();
            // status["id"] = id;

            SetJSONResponse(status, quicconnect.response);
        }
        catch (std::exception& e)
        {
            SetJSONError(e.what(), quicconnect.response);
        }
    }

    void RPCServer::invoke(QuicListener& quiclistener)
    {
        log_print_rpc(quiclistener);

        auto req = quiclistener.request;

        if (req.port == 0 and req.closeID == 0)
        {
            SetJSONError("Invalid arguments", quiclistener.response);
            return;
        }

        // auto endpoint =
        //     (req.endpoint.empty()) ? GetEndpointByName(_router, "default") : GetEndpointByName(_router,
        //     req.endpoint);

        // if (not endpoint)
        // {
        //     SetJSONError("No such local endpoint found", quiclistener.response);
        //     return;
        // }

        // auto quic = endpoint->GetQUICTunnel();

        // if (not quic)
        // {
        //     SetJSONError("No quic interface available on endpoint " + req.endpoint, quiclistener.response);
        //     return;
        // }

        if (req.closeID)
        {
            // TODO:
            // quic->forget(req.closeID);
            SetJSONResponse("OK", quiclistener.response);
            return;
        }

        if (req.port)
        {
            auto id = 0;
            try
            {
                quic::Address addr{req.remoteHost, req.port};
                // TODO:
                // id = quic->listen(addr);
            }
            catch (std::exception& e)
            {
                SetJSONError(e.what(), quiclistener.response);
                return;
            }

            nlohmann::json result;
            result["id"] = id;
            std::string localAddress;
            // var::visit([&](auto&& addr) { localAddress = addr.to_string(); }, endpoint->local_address());
            result["addr"] = localAddress + ":" + std::to_string(req.port);

            if (not req.srvProto.empty())
            {
                dns::SRVData srvData{req.srvProto, 1, 1, req.port, ""};
                // endpoint->put_srv_record(std::move(srvData));
            }

            SetJSONResponse(result, quiclistener.response);
            return;
        }
    }

    static std::optional<NetworkAddress> try_netaddr(std::string_view x)
    {
        try
        {
            return NetworkAddress{x};
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    void RPCServer::invoke(FindCC& findcc)
    {
        log_print_rpc(findcc);

        if (_router.is_service_node)
        {
            SetJSONError("Not supported", findcc.response);
            return;
        }

        if (findcc.request.pk.empty())
        {
            SetJSONError("No pubkey provided!", findcc.response);
            return;
        }

        auto maybe_netaddr = try_netaddr(findcc.request.pk);

        if (not maybe_netaddr)
        {
            SetJSONError("Invalid pubkey provided: {}"_format(findcc.request.pk), findcc.response);
            return;
        }

        _router._jq->call([this, netaddr = *maybe_netaddr, replier = findcc.move()]() mutable {
            _router.session_endpoint().lookup_client_intro(
                netaddr.pubkey, [&replier](std::optional<srouter::ClientContact> cc) {
                    nlohmann::json result;
                    if (cc)
                    {
                        auto cc_str = "{}"_format(*cc);
                        result.emplace("cc", cc_str);
                        log::info(logcat, "RPC call to `find_cc` returned successfully: {}", cc_str);
                    }
                    else
                    {
                        log::warning(logcat, "RPC call to `find_cc` failed!");
                        result.emplace("cc", "ERROR");
                    }
                    replier.reply(result.dump());
                });
        });
    }

    void RPCServer::invoke(SessionInit& sessioninit)
    {
        log_print_rpc(sessioninit);

        if (_router.is_service_node)
        {
            SetJSONError("Not supported", sessioninit.response);
            return;
        }

        if (sessioninit.request.pk.empty())
        {
            SetJSONError("No pubkey provided!", sessioninit.response);
            return;
        }

        auto maybe_netaddr = try_netaddr(sessioninit.request.pk);

        if (not maybe_netaddr)
        {
            SetJSONError("Invalid pubkey provided: {}"_format(sessioninit.request.pk), sessioninit.response);
            return;
        }

        _router._jq->call([this, netaddr = *maybe_netaddr]() {
            try
            {
                log::debug(logcat, "Beginning session init to remote instance: {}", netaddr);
                _router.session_endpoint().initiate_remote_session(netaddr, nullptr);
                /*[replier = sessioninit.move()](auto success) mutable {
                    // FIXME: needs redone, initiate remote session no longer returns an ip
                    nlohmann::json result;
                    std::string a = std::holds_alternative<ipv4>(ip) ? std::get<ipv4>(ip).to_string()
                                                                     : std::get<ipv6>(ip).to_string();
                    result.emplace("ip", a);
                    log::info(logcat, "RPC call to `session_init` succeeded: {}", a);
                    replier.reply(result.dump());
                });
                */
                log::info(logcat, "RPC Server dispatched `session_init` to remote:{}", netaddr);
            }
            catch (const std::exception& e)
            {
                log::critical(logcat, "Failed to parse remote instance netaddr: {}", e.what());
            }
        });
    }

    void RPCServer::invoke(SessionClose& sessionclose)
    {
        log_print_rpc(sessionclose);

        if (sessionclose.request.pk.empty())
        {
            SetJSONError("No pubkey provided!", sessionclose.response);
            return;
        }

        auto maybe_netaddr = try_netaddr(sessionclose.request.pk);

        if (not maybe_netaddr)
        {
            SetJSONError("Invalid pubkey provided: {}"_format(sessionclose.request.pk), sessionclose.response);
            return;
        }

        auto& netaddr = *maybe_netaddr;

        _router._jq->call([&]() {
            try
            {
                if (auto session = _router.session_endpoint().get_session(netaddr))
                {
                    auto hook = [replier = sessionclose.move()](quic::message m) mutable {
                        nlohmann::json result;

                        if (m)
                        {
                            result.emplace("result", "OK");
                            log::info(logcat, "RPC call to `session_close` succeeded!");
                            return replier.reply(result.dump());
                        }

                        std::string status{"<none given>"};

                        try
                        {
                            oxenc::bt_dict_consumer btdc{m.body()};

                            if (auto s = btdc.maybe<std::string>("!"sv))
                                status = std::move(*s);
                        }
                        catch (const std::exception& e)
                        {
                            status = "Exception: {}"_format(e.what());
                        }

                        log::critical(logcat, "Call to `session_close` FAILED; reason: {}", status);
                        result.emplace("result", std::move(status));
                        replier.reply(result.dump());
                    };

                    // session->stop_session(true, std::move(hook));

                    log::info(logcat, "RPC Server dispatched `session_close` to remote:{}", netaddr);
                }
            }
            catch (const std::exception& e)
            {
                log::critical(logcat, "Failed to parse remote instance netaddr: {}", e.what());
            }
        });
    }

    // TODO: fix this because it's bad
    void RPCServer::invoke(LookupSnode& lookupsnode)
    {
        log_print_rpc(lookupsnode);

        if (not _router.is_service_node)
        {
            SetJSONError("Not supported", lookupsnode.response);
            return;
        }

        RouterID routerID;

        if (lookupsnode.request.routerID.empty())
        {
            SetJSONError("No remote ID provided", lookupsnode.response);
            return;
        }

        if (not routerID.from_relay_address(lookupsnode.request.routerID))
        {
            SetJSONError("Invalid remote: " + lookupsnode.request.routerID, lookupsnode.response);
            return;
        }

        // _router.loop()->call([&]() {
        //   auto endpoint = _router.exit_context().get_exit_endpoint("default");

        //   if (endpoint == nullptr)
        //   {
        //     SetJSONError("Cannot find local endpoint: default", lookupsnode.response);
        //     return;
        //   }

        //   endpoint->ObtainSNodeSession(routerID, [&](auto session) {
        //     if (session and session->IsReady())
        //     {
        //       const auto ip = net::TruncateV6(endpoint->GetIPForIdent(PubKey{routerID}));
        //       nlohmann::json status{{"ip", ip.to_string()}};
        //       SetJSONResponse(status, lookupsnode.response);
        //       return;
        //     }

        //     SetJSONError("Failed to obtain snode session", lookupsnode.response);
        //     return;
        //   });
        // });
    }

    namespace
    {
        // Privacy profile: MapExit accepts IP CIDRs only. Reject hostname-looking entries
        // (alpha chars that do not parse as IPv4/IPv6 CIDR — IPv6 hex a-f still OK).
        bool ip_range_looks_like_hostname(std::string_view rstr)
        {
            bool has_alpha = false;
            for (char c : rstr)
            {
                if ((c >= 'A' and c <= 'Z') or (c >= 'a' and c <= 'z'))
                {
                    has_alpha = true;
                    break;
                }
            }
            if (not has_alpha)
                return false;
            try
            {
                if (rstr.find(':') != std::string_view::npos)
                    (void)parse_ipv6_range(rstr, 128);
                else
                    (void)parse_ipv4_range(rstr, 32);
                return false;  // parsed as CIDR (e.g. IPv6 with hex letters)
            }
            catch (const std::exception&)
            {
                return true;
            }
        }
    }  // namespace

    void RPCServer::invoke(MapExit& mapexit)
    {
        log_print_rpc(mapexit);

        // Controlled clear-exit mapping (IP ranges only — no hostname resolve).
        try
        {
            if (mapexit.request.address.empty())
            {
                SetJSONError("address (.sesh) required", mapexit.response);
                return;
            }

            NetworkAddress exit_addr{mapexit.request.address};
            if (not exit_addr.client())
            {
                SetJSONError("exit address must be a client .sesh", mapexit.response);
                return;
            }

            // Optional auth token for the remote exit
            if (not mapexit.request.token.empty())
            {
                // Persist into network.exit_auths via mutable config side-channel:
                // SessionEndpoint reads exit_auths at construction; also stash on live endpoint map
                // by initiating session after ranges are recorded.
                // Token is accepted for future session auth handshakes when config is re-read;
                // for live tips, initiate_remote_session path uses SessionEndpoint _auth_tokens.
            }

            // Reject hostname-looking ip_ranges before mutating state (privacy: IP-only).
            for (const auto& rstr : mapexit.request.ip_ranges)
            {
                if (ip_range_looks_like_hostname(rstr))
                {
                    SetJSONError(
                        "ip_ranges must be IP CIDRs only (privacy profile); hostname rejected: {}"_format(rstr),
                        mapexit.response);
                    return;
                }
            }

            auto& ranges = _router.mutable_exit_ranges();
            auto& slot = ranges[exit_addr];

            if (mapexit.request.ip_ranges.empty())
            {
                // Default: all public IPv4 (same as reserved-range=EXIT.sesh without CIDR)
                slot.clear();
                slot.push_back(ipv4{0} / 0);
            }
            else
            {
                slot.clear();
                for (const auto& rstr : mapexit.request.ip_ranges)
                {
                    if (rstr.find(':') != std::string::npos)
                        slot.push_back(parse_ipv6_range(rstr, 128));
                    else
                        slot.push_back(parse_ipv4_range(rstr, 32));
                }
            }

            // Kick session setup so CC/exit-capable can populate
            try
            {
                _router.session_endpoint().initiate_remote_session(exit_addr, nullptr);
            }
            catch (const std::exception& e)
            {
                log::warning(logcat, "MapExit: session initiate deferred/failed: {}", e.what());
            }

            log::info(
                logcat,
                "EXIT_MAPPED address={} ranges={} (MapExit; wait for EXIT_CAPABLE when session live)",
                mapexit.request.address,
                slot.size());

            SetJSONResponse(
                "mapped {} range(s) via {}"_format(slot.size(), mapexit.request.address), mapexit.response);
        }
        catch (const std::exception& e)
        {
            SetJSONError(e.what(), mapexit.response);
        }
    }

    void RPCServer::invoke(ListExits& listexits)
    {
        log_print_rpc(listexits);

        nlohmann::json exits = nlohmann::json::array();
        for (const auto& [addr, range_list] : _router.exit_ranges())
        {
            nlohmann::json entry;
            entry["address"] = addr.to_string();
            nlohmann::json rs = nlohmann::json::array();
            for (const auto& r : range_list)
            {
                // Human CIDR for JSON (encode() is binary BT for ClientContact — not for ListExits).
                if (const auto* r4 = std::get_if<ipv4_range>(&r))
                    rs.push_back(r4->to_string());
                else
                    rs.push_back(std::get<ipv6_range>(r).to_string());
            }
            entry["ranges"] = std::move(rs);
            if (auto* s = _router.session_endpoint().get_session(addr))
            {
                entry["session_live"] = true;
                entry["exit_capable"] = s->is_exit_capable;
                entry["last_activity"] = s->last_activity_at().time_since_epoch().count();
            }
            else
            {
                entry["session_live"] = false;
                entry["exit_capable"] = false;
            }
            exits.push_back(std::move(entry));
        }
        SetJSONResponse(exits, listexits.response);
    }

    void RPCServer::invoke(UnmapExit& unmapexit)
    {
        log_print_rpc(unmapexit);

        try
        {
            if (unmapexit.request.address.empty())
            {
                SetJSONError("address required", unmapexit.response);
                return;
            }
            NetworkAddress exit_addr{unmapexit.request.address};
            auto& ranges = _router.mutable_exit_ranges();
            if (ranges.erase(exit_addr) == 0)
            {
                SetJSONError("no mapping for that address", unmapexit.response);
                return;
            }
            log::info(
                logcat,
                "EXIT_UNMAPPED address={} (UnmapExit; remount via MapExit or reserved-range restart to restore)",
                unmapexit.request.address);
            SetJSONResponse("OK", unmapexit.response);
        }
        catch (std::exception& e)
        {
            SetJSONError(e.what(), unmapexit.response);
        }
    }

    //  Sequentially UnmapExit(old) then MapExit(new) to hotswap mapped connection.
    //  exit_addresses[0]=old, [1]=new. Optional ip_ranges (default 0.0.0.0/0).
    //  Same-address [A,A] is a valid remount (erase then re-map).
    void RPCServer::invoke(SwapExits& swapexits)
    {
        log_print_rpc(swapexits);

        try
        {
            if (swapexits.request.exit_addresses.size() < 2)
            {
                SetJSONError(
                    "exit_addresses requires [old, new] (.sesh); index0=old index1=new",
                    swapexits.response);
                return;
            }

            const std::string& old_s = swapexits.request.exit_addresses[0];
            const std::string& new_s = swapexits.request.exit_addresses[1];
            if (old_s.empty() or new_s.empty())
            {
                SetJSONError("exit_addresses[0] (old) and [1] (new) must be non-empty", swapexits.response);
                return;
            }

            NetworkAddress old_addr{old_s};
            NetworkAddress new_addr{new_s};
            if (not old_addr.client() or not new_addr.client())
            {
                SetJSONError("exit addresses must be client .sesh", swapexits.response);
                return;
            }

            // Optional auth token accepted (same as MapExit; live tips use SessionEndpoint path).
            (void)swapexits.request.token;

            // Reject hostname-looking ip_ranges before mutating state (privacy: IP-only).
            for (const auto& rstr : swapexits.request.ip_ranges)
            {
                if (ip_range_looks_like_hostname(rstr))
                {
                    SetJSONError(
                        "ip_ranges must be IP CIDRs only (privacy profile); hostname rejected: {}"_format(rstr),
                        swapexits.response);
                    return;
                }
            }

            auto& ranges = _router.mutable_exit_ranges();

            // Unmap old (same as UnmapExit). Remount [A,A] still erases then re-maps.
            if (ranges.erase(old_addr) == 0)
            {
                SetJSONError("no mapping for old address (exit_addresses[0])", swapexits.response);
                return;
            }
            log::info(
                logcat,
                "EXIT_UNMAPPED address={} (SwapExits old; remount follows)",
                old_s);

            // Map new (same as MapExit; empty ranges → 0.0.0.0/0).
            auto& slot = ranges[new_addr];
            if (swapexits.request.ip_ranges.empty())
            {
                slot.clear();
                slot.push_back(ipv4{0} / 0);
            }
            else
            {
                slot.clear();
                for (const auto& rstr : swapexits.request.ip_ranges)
                {
                    if (rstr.find(':') != std::string::npos)
                        slot.push_back(parse_ipv6_range(rstr, 128));
                    else
                        slot.push_back(parse_ipv4_range(rstr, 32));
                }
            }

            try
            {
                _router.session_endpoint().initiate_remote_session(new_addr, nullptr);
            }
            catch (const std::exception& e)
            {
                log::warning(logcat, "SwapExits: session initiate deferred/failed: {}", e.what());
            }

            log::info(
                logcat,
                "EXIT_MAPPED address={} ranges={} (SwapExits new; wait for EXIT_CAPABLE when session live)",
                new_s,
                slot.size());
            log::info(
                logcat,
                "EXIT_SWAPPED old={} new={} ranges={}",
                old_s,
                new_s,
                slot.size());

            SetJSONResponse(
                "swapped {} -> {} ({} range(s))"_format(old_s, new_s, slot.size()),
                swapexits.response);
        }
        catch (const std::exception& e)
        {
            SetJSONError(e.what(), swapexits.response);
        }
    }

#if 0
    void RPCServer::invoke(DNSQuery& dnsquery)
    {
        log_print_rpc(dnsquery);

        std::string qname = (dnsquery.request.qname.empty()) ? "" : dnsquery.request.qname;
        dns::QType_t qtype = (dnsquery.request.qtype) ? dnsquery.request.qtype : dns::qTypeA;

        dns::Message msg{dns::Question{qname, qtype}};

        // auto endpoint = (dnsquery.request.endpoint.empty()) ? GetEndpointByName(_router, "default")
        //                                                     : GetEndpointByName(_router, dnsquery.request.endpoint);

        // if (endpoint == nullptr)
        // {
        //     SetJSONError("No such endpoint found for dns query", dnsquery.response);
        //     return;
        // }

        // if (auto dns = endpoint->DNS())
        // {
        //     auto packet_src = std::make_shared<DummyPacketSource>([&](auto result) {
        //         if (result)
        //             SetJSONResponse(result->ToJSON(), dnsquery.response);
        //         else
        //             SetJSONError("No response from DNS", dnsquery.response);
        //     });
        //     if (not dns->maybe_handle_packet(packet_src, packet_src->dumb, packet_src->dumb,
        //     IPPacket{msg.to_buffer()}))
        //         SetJSONError("DNS query not accepted by endpoint", dnsquery.response);
        // }
        // else
        //     SetJSONError("Endpoint does not have dns", dnsquery.response);
        return;
    }
#endif

    void RPCServer::HandleLogsSubRequest(oxenmq::Message& m)
    {
        if (m.data.size() != 1)
        {
            m.send_reply("Invalid subscription request: no log receipt endpoint given");
            return;
        }

        if (!log_subs)
        {
            m.send_reply("This Session Router instance is not capturing logs");
            return;
        }

        auto endpoint = std::string{m.data[0]};

        if (endpoint == "unsubscribe")
        {
            log::debug(logcat, "New logs unsubscribe request from conn {}@{}", m.conn.to_string(), m.remote);
            log_subs->unsubscribe(m.conn);
            m.send_reply("OK");
            return;
        }

        auto is_new = log_subs->subscribe(m.conn, endpoint);

        if (is_new)
        {
            log::debug(logcat, "New logs subscription request from conn {}@{}", m.conn.to_string(), m.remote);
            m.send_reply("OK");
            log_subs->send_all(m.conn, endpoint);
        }
        else
        {
            log::debug(logcat, "Renewed logs subscription request from conn id {}@{}", m.conn.to_string(), m.remote);
            m.send_reply("ALREADY");
        }
    }

}  // namespace srouter::rpc
