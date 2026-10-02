#include "route_poker.hpp"

#include "handlers/tun.hpp"
#include "link/link_manager.hpp"
#include "router.hpp"

namespace srouter
{
    static auto logcat = log::Cat("route_poker");

    RoutePoker::RoutePoker(Router& r) : _router{r}
    {
        _enabled = r.config().network.enable_route_poker;
        if (_enabled)
            log::info(logcat, "Route poker enabled by [network] auto-routing");
    }

    template <IP46 IP>
    void RoutePoker::add_route(const IP& ip)
    {
        // Always record the hop so pre-put_up poke_first_hop is not dropped on the floor.
        // When !_up we only store (empty gateway); put_up flushes via refresh/enable before
        // installing default-via-TUN.
        auto& poked_rs = poked_routes<IP>();
        auto [it, new_route] = poked_rs.emplace(ip, IP{});
        auto& gw = it->second;

        if (not _up)
        {
            if (new_route)
                log::debug(logcat, "RoutePoker: recorded first-hop {} pending put_up", ip);
            return;
        }

        auto& current_gw = current_gateway<IP>();
        if (!current_gw)
        {
            gw = IP{};
            return;
        }

        // remove existing mapping as needed
        if (!new_route and gw != IP{})
            disable_route(ip, gw);
        // update and add new mapping
        gw = *current_gw;

        log::info(logcat, "Added route to {} via {}", ip, gw);

        enable_route(ip, gw);
    }
    template void RoutePoker::add_route(const ipv4& ip);
    template void RoutePoker::add_route(const ipv6& ip);

    template <IP46 IP>
    void RoutePoker::disable_route(const IP& ip, const IP& gateway)
    {
        if (_enabled and ip != IP{})
        {
            log::info(logcat, "Deleting route to {} via {}", ip, gateway);
            _router.vpn_platform()->RouteManager().delete_route(ip, gateway);
        }
    }
    template void RoutePoker::disable_route(const ipv4&, const ipv4&);
    template void RoutePoker::disable_route(const ipv6&, const ipv6&);

    template <IP46 IP>
    void RoutePoker::enable_route(const IP& ip, const IP& gateway)
    {
        if (_enabled and ip != IP{})
            _router.vpn_platform()->RouteManager().add_route(ip, gateway);
    }
    template void RoutePoker::enable_route(const ipv4&, const ipv4&);
    template void RoutePoker::enable_route(const ipv6&, const ipv6&);

    void RoutePoker::delete_all_routes()
    {
        for (auto it = poked_routes4.begin(); it != poked_routes4.end();)
            it = delete_route(it);
        for (auto it = poked_routes6.begin(); it != poked_routes6.end();)
            it = delete_route(it);
    }

    void RoutePoker::start()
    {
        if (not _enabled)
        {
            log::info(logcat, "Route poker is NOT enabled for this Session Router instance!");
            return;
        }

        // Discover gateway once at start; put_up() re-discovers.  Avoid 100ms hammer.
        update();
    }

    void RoutePoker::disable_all_routes()
    {
        for (const auto& [ip, gateway] : poked_routes4)
            disable_route(ip, gateway);
        for (const auto& [ip, gateway] : poked_routes6)
            disable_route(ip, gateway);
    }

    void RoutePoker::refresh_all_routes()
    {
        for (const auto& [ip, gw] : poked_routes4)
            add_route(ip);
        for (const auto& [ip, gw] : poked_routes6)
            add_route(ip);
    }

    RoutePoker::~RoutePoker()
    {
        if (not _router.vpn_platform())
            return;

        delete_all_routes();
        _router.vpn_platform()->RouteManager().delete_blackhole();
    }

void RoutePoker::update()
    {
        // Discover non-TUN gateways via VPN RouteManager (same path as linux.hpp).
        // TunEndpoint is full-platform only; RoutePoker lives in the same TU/link unit.
        auto platform = _router.vpn_platform();
        if (not platform)
            return;
        auto& tun_base = _router.tun_endpoint();
        if (not tun_base)
            return;
        auto* tun = dynamic_cast<handlers::TunEndpoint*>(tun_base.get());
        if (tun == nullptr)
            return;
        auto* vpn = tun->get_vpn_interface();
        if (vpn == nullptr)
            return;

        auto gateways = platform->RouteManager().get_non_interface_gateways(*vpn);

        std::optional<ipv4> next4;
        std::optional<ipv6> next6;
        for (auto& g : gateways)
        {
            if (g.is_ipv4() and not next4)
            {
                auto v4 = g.to_ipv4();
                if (v4 != ipv4{})
                    next4 = v4;
            }
            else if (g.is_ipv6() and not next6)
            {
                auto v6 = g.to_ipv6();
                if (v6 != ipv6{})
                    next6 = v6;
            }
        }

        bool changed = false;
        if (next4 != current_gateway4)
        {
            if (next4 and current_gateway4)
                log::info(logcat, "IPv4 default gateway changed from {} to {}", *current_gateway4, *next4);
            else if (current_gateway4)
                log::warning(logcat, "IPv4 default gateway {} has gone away", *current_gateway4);
            else if (next4)
                log::info(logcat, "IPv4 default gateway found at {}", *next4);
            current_gateway4 = next4;
            changed = true;
        }
        if (next6 != current_gateway6)
        {
            if (next6 and current_gateway6)
                log::info(logcat, "IPv6 default gateway changed from {} to {}", *current_gateway6, *next6);
            else if (current_gateway6)
                log::warning(logcat, "IPv6 default gateway {} has gone away", *current_gateway6);
            else if (next6)
                log::info(logcat, "IPv6 default gateway found at {}", *next6);
            current_gateway6 = next6;
            changed = true;
        }

        if (changed)
            refresh_all_routes();
        // Do not call put_up() here — put_up() itself calls update(); auto-up is
        // driven by TunEndpoint on_connected when exit.ranges is non-empty.
    }


    template <IP46 IP>
    inline static constexpr auto ip_name = std::same_as<IP, ipv4> ? "IPv4"sv : "IPv6"sv;

    void RoutePoker::put_up()
    {
        if (_up)
            return;

        if (!_enabled)
        {
            log::warning(logcat, "RoutePoker coming up, but route poking is disabled by config");
            return;
        }

        // Refresh non-TUN gateway discovery before installing routes.
        update();

        if (not _router.vpn_platform())
        {
            log::warning(logcat, "RoutePoker: no vpn platform; refuse put_up");
            return;
        }

        // Fail closed: no default-via-TUN without a discovered non-TUN gateway.
        if (not current_gateway4 and not current_gateway6)
        {
            log::warning(
                logcat,
                "RoutePoker: no non-TUN gateway discovered; refuse default route via TUN");
            return;
        }

        log::info(logcat, "RoutePoker coming up; poking routes");

        vpn::AbstractRouteManager& route = _router.vpn_platform()->RouteManager();

        // black hole all routes if enabled
        if (_router.config().network.blackhole_routes)
            route.add_blackhole();

        // IMPORTANT: flush *all* first-hop /32s (pre-up recorded + established + pending
        // outbound) *before* default-via-TUN, or edge QUIC is blackholed into the TUN and
        // path builds to the exit time out.
        const auto recorded = poked_routes4.size() + poked_routes6.size();
        // Mark _up so add_route actually installs (enable_route) during flush.
        _up = true;
        refresh_all_routes();

        int poked = 0;
        auto poke_remote = [this, &poked](const quic::Address& remote) {
            if (remote.is_any_addr())
                return;
            if (remote.is_ipv4())
                add_route(remote.to_ipv4());
            else
                add_route(remote.to_ipv6());
            ++poked;
        };

        // Established client/relay edges.
        _router.link_manager().endpoint.for_each_relay_conn(
            [&poke_remote](const RouterID&, link::Connection& conn) {
                poke_remote(conn.conn->remote());
            });

        // In-flight dials not yet in client_conns (pending_outbound).
        _router.link_manager().endpoint.for_each_pending_outbound(
            [&poke_remote](const RouterID&, link::Connection& conn) {
                if (conn.conn)
                    poke_remote(conn.conn->remote());
            });

        auto& local = _router.link_manager().local();
        if (not local.is_any_addr())
            poke_remote(local);

        const auto installed = poked_routes4.size() + poked_routes6.size();
        log::info(
            logcat,
            "RoutePoker: flushed {} pre-up recorded hop(s); poked {} live/pending edge remote(s); "
            "pin map size {}",
            recorded,
            poked,
            installed);

        // Fail closed: empty pin list / zero host routes — do not install default-via-TUN.
        if (installed == 0)
        {
            log::warning(
                logcat,
                "RoutePoker: pin list empty after flush; refuse default route via TUN");
            _up = false;
            if (_router.config().network.blackhole_routes)
                route.delete_blackhole();
            return;
        }

        // Default route via Session Router TUN — only AFTER successful first-hop pins.
        if (auto& tun = _router.tun_endpoint())
        {
            try
            {
                tun->add_default_route();
                log::info(logcat, "route poker: default route via TUN installed");
            }
            catch (const std::exception& e)
            {
                log::warning(logcat, "route poker: default route via TUN failed: {}; rolling back", e.what());
                delete_all_routes();
                if (_router.config().network.blackhole_routes)
                    route.delete_blackhole();
                _up = false;
                return;
            }
        }
        else
        {
            log::warning(logcat, "route poker: no TUN endpoint; cannot install default route");
            _up = false;
            return;
        }
        log::info(logcat, "route poker up");
    }

    void RoutePoker::put_down()
    {
        if (!_up)
            return;
        // Clear _up first so a reconnect during slow win32 route DELETE can put_up again.
        _up = false;

        // Delete every recorded host pin (not only current relay_conns).
        delete_all_routes();

        if (_enabled)
        {
            if (auto& tun = _router.tun_endpoint())
                tun->delete_default_route();

            if (_router.vpn_platform())
            {
                vpn::AbstractRouteManager& route = _router.vpn_platform()->RouteManager();
                if (_router.config().network.blackhole_routes)
                    route.delete_blackhole();
            }
            log::info(logcat, "route poker down");
        }
    }

}  // namespace srouter
