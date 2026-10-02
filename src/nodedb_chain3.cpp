#include "nodedb.hpp"

#include "router/router.hpp"
#include "util/logging.hpp"

#include <nlohmann/json.hpp>
#include <oxenmq/address.h>
#include <oxenmq/oxenmq.h>

#include <chrono>
#include <future>
#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace srouter
{
    static auto logcat = srouter::log::Cat("nodedb");

    static constexpr const char* REFUSE_DIV =
        "[bootstrap] mode=chain3 requires >=3 diverse rpc= seeds";
    static constexpr const char* REFUSE_FALLBACK =
        "[bootstrap] mode=chain3: RPCs failed and no signed bootstrap.signed cold fallback";

    void NodeDB::chain3_try_reconcile()
    {
        using namespace std::chrono_literals;

        const auto& bc = _router.config().bootstrap;
        log::info(
            logcat,
            "chain3 begin: rpcs={} height_lag_cap={} local_bootstraps={}",
            bc.rpc.size(),
            bc.height_lag_cap,
            _bootstraps.size());

        // Diversity: require >=3 seeds and >=2 distinct hosts (simple hostname heuristic).
        std::set<std::string> hosts;
        for (const auto& url : bc.rpc)
        {
            std::string host = url;
            if (auto pos = host.find("://"); pos != std::string::npos)
                host = host.substr(pos + 3);
            if (auto pos = host.find('/'); pos != std::string::npos)
                host = host.substr(0, pos);
            if (auto pos = host.find(':'); pos != std::string::npos)
                host = host.substr(0, pos);
            for (char& c : host)
                if (c >= 'A' and c <= 'Z')
                    c = static_cast<char>(c - 'A' + 'a');
            if (not host.empty())
                hosts.insert(host);
        }
        if (bc.rpc.size() < 3 or hosts.size() < 2)
        {
            log::error(
                logcat,
                "chain3 blocked: need >=3 diverse rpc= seeds (got rpcs={} distinct_hosts={})",
                bc.rpc.size(),
                hosts.size());
            if (_bootstraps.empty())
                throw std::runtime_error{REFUSE_DIV};
            log::warning(logcat, "chain3 fallback: diverse seeds missing; using local signed file");
            return;
        }

        struct DaemonView
        {
            std::string url;
            bool ok{false};
            uint64_t height{0};
            std::set<std::string> pubkeys_hex;
            std::string err;
        };
        std::vector<DaemonView> views;
        views.reserve(bc.rpc.size());

        auto* omq = _router.omq_nullable();
        if (omq == nullptr)
        {
            log::warning(
                logcat,
                "chain3 blocked: OMQ unavailable at load_bootstraps; cold-fallback to local signed if present");
            if (_bootstraps.empty())
                throw std::runtime_error{REFUSE_FALLBACK};
            log::warning(
                logcat, "chain3 fallback: no OMQ; using local signed file ({} RCs)", _bootstraps.size());
            return;
        }

        for (const auto& url : bc.rpc)
        {
            DaemonView v;
            v.url = url;
            try
            {
                oxenmq::address addr{url};
                auto conn_prom = std::make_shared<std::promise<bool>>();
                auto conn_fut = conn_prom->get_future();
                auto conn = omq->connect_remote(
                    addr,
                    [conn_prom](oxenmq::ConnectionID) { conn_prom->set_value(true); },
                    [conn_prom](oxenmq::ConnectionID, std::string_view) {
                        try
                        {
                            conn_prom->set_value(false);
                        }
                        catch (...)
                        {}
                    });
                if (conn_fut.wait_for(3s) != std::future_status::ready or not conn_fut.get())
                {
                    v.err = "connect_timeout_or_fail";
                    views.push_back(std::move(v));
                    continue;
                }

                auto info_prom = std::make_shared<std::promise<std::pair<bool, std::string>>>();
                auto info_fut = info_prom->get_future();
                omq->request(
                    conn,
                    "rpc.get_info",
                    [info_prom](bool ok, std::vector<std::string> data) {
                        try
                        {
                            info_prom->set_value(
                                {ok, (ok and not data.empty()) ? std::move(data.back()) : std::string{}});
                        }
                        catch (...)
                        {}
                    });
                if (info_fut.wait_for(5s) != std::future_status::ready)
                {
                    v.err = "get_info_timeout";
                    views.push_back(std::move(v));
                    try
                    {
                        omq->disconnect(conn);
                    }
                    catch (...)
                    {}
                    continue;
                }
                auto [info_ok, info_body] = info_fut.get();
                if (not info_ok or info_body.empty())
                {
                    v.err = "get_info_fail";
                    views.push_back(std::move(v));
                    try
                    {
                        omq->disconnect(conn);
                    }
                    catch (...)
                    {}
                    continue;
                }
                try
                {
                    auto j = nlohmann::json::parse(info_body);
                    if (j.contains("result") and j["result"].contains("height"))
                        v.height = j["result"]["height"].get<uint64_t>();
                    else if (j.contains("height"))
                        v.height = j["height"].get<uint64_t>();
                }
                catch (const std::exception& e)
                {
                    v.err = std::string{"get_info_parse:"} + e.what();
                    views.push_back(std::move(v));
                    try
                    {
                        omq->disconnect(conn);
                    }
                    catch (...)
                    {}
                    continue;
                }

                nlohmann::json req{{"fields", {"pubkey_ed25519"}}};
                auto sn_prom = std::make_shared<std::promise<std::pair<bool, std::string>>>();
                auto sn_fut = sn_prom->get_future();
                omq->request(
                    conn,
                    "rpc.get_service_nodes",
                    [sn_prom](bool ok, std::vector<std::string> data) {
                        try
                        {
                            sn_prom->set_value(
                                {ok, (ok and data.size() >= 1) ? std::move(data.back()) : std::string{}});
                        }
                        catch (...)
                        {}
                    },
                    req.dump());
                if (sn_fut.wait_for(8s) != std::future_status::ready)
                {
                    v.err = "get_service_nodes_timeout";
                    views.push_back(std::move(v));
                    try
                    {
                        omq->disconnect(conn);
                    }
                    catch (...)
                    {}
                    continue;
                }
                auto [sn_ok, sn_body] = sn_fut.get();
                try
                {
                    omq->disconnect(conn);
                }
                catch (...)
                {}
                if (not sn_ok or sn_body.empty())
                {
                    v.err = "get_service_nodes_fail";
                    views.push_back(std::move(v));
                    continue;
                }
                try
                {
                    auto j = nlohmann::json::parse(sn_body);
                    const nlohmann::json* states = nullptr;
                    if (j.contains("result") and j["result"].contains("service_node_states"))
                        states = &j["result"]["service_node_states"];
                    else if (j.contains("service_node_states"))
                        states = &j["service_node_states"];
                    if (states and states->is_array())
                    {
                        for (const auto& st : *states)
                        {
                            if (st.contains("pubkey_ed25519") and st["pubkey_ed25519"].is_string())
                                v.pubkeys_hex.insert(st["pubkey_ed25519"].get<std::string>());
                        }
                    }
                    if (v.pubkeys_hex.empty())
                    {
                        v.err = "empty_sn_list";
                        views.push_back(std::move(v));
                        continue;
                    }
                    v.ok = true;
                }
                catch (const std::exception& e)
                {
                    v.err = std::string{"sn_parse:"} + e.what();
                }
            }
            catch (const std::exception& e)
            {
                v.err = e.what();
            }
            views.push_back(std::move(v));
        }

        std::vector<const DaemonView*> okviews;
        for (const auto& v : views)
            if (v.ok)
                okviews.push_back(&v);

        log::info(logcat, "chain3 query: ok={}/{}", okviews.size(), views.size());
        for (const auto& v : views)
        {
            if (v.ok)
                log::info(
                    logcat,
                    "chain3 view ok url={} height={} pubkeys={}",
                    v.url,
                    v.height,
                    v.pubkeys_hex.size());
            else
                log::warning(logcat, "chain3 view fail url={} err={}", v.url, v.err);
        }

        if (okviews.size() < 2)
        {
            log::warning(
                logcat,
                "chain3 blocked: fewer than 2 successful RPCs (ok={}); cold-fallback",
                okviews.size());
            if (_bootstraps.empty())
                throw std::runtime_error{REFUSE_FALLBACK};
            log::warning(logcat, "chain3 fallback: RPC quorum fail; using local signed file");
            return;
        }

        uint64_t hmin = okviews[0]->height, hmax = okviews[0]->height;
        for (auto* v : okviews)
        {
            hmin = std::min(hmin, v->height);
            hmax = std::max(hmax, v->height);
        }
        std::vector<const DaemonView*> inlag;
        for (auto* v : okviews)
        {
            if (hmax >= v->height and (hmax - v->height) <= bc.height_lag_cap)
                inlag.push_back(v);
            else
                log::warning(
                    logcat,
                    "chain3 drop outlier url={} height={} lag_span={}",
                    v->url,
                    v->height,
                    hmax - hmin);
        }
        if (inlag.size() < 2)
        {
            log::warning(
                logcat,
                "chain3 blocked: height lag (ok_in_cap={} span={} cap={})",
                inlag.size(),
                hmax - hmin,
                bc.height_lag_cap);
            if (_bootstraps.empty())
                throw std::runtime_error{REFUSE_FALLBACK};
            log::warning(logcat, "chain3 fallback: height lag; using local signed file");
            return;
        }

        // 2-of-N intersection: pubkey must appear in at least 2 in-lag views.
        std::map<std::string, int> counts;
        for (auto* v : inlag)
            for (const auto& pk : v->pubkeys_hex)
                counts[pk] += 1;
        std::unordered_set<RouterID> intersection;
        constexpr int need = 2;
        for (const auto& [pk, c] : counts)
        {
            if (c >= need)
            {
                try
                {
                    RouterID rid;
                    if (rid.from_hex(pk))
                        intersection.insert(rid);
                }
                catch (...)
                {}
            }
        }

        if (intersection.empty())
        {
            log::warning(logcat, "chain3 blocked: empty intersection; cold-fallback");
            if (_bootstraps.empty())
                throw std::runtime_error{REFUSE_FALLBACK};
            log::warning(logcat, "chain3 fallback: empty intersection; using local signed file");
            return;
        }

        set_registered_relays(intersection);
        int kept = 0;
        for (const auto& rc : _bootstraps)
        {
            if (intersection.contains(rc.router_id()))
            {
                const auto& rid = rc.router_id();
                if (not _router.is_service_node)
                    known_rids.insert(rid);
                known_rcs.insert_or_assign(rid, rc);
                ++kept;
            }
        }
        log::info(
            logcat,
            "chain3 pass: intersection={} height_span={} kept_local_rcs={} inlag_views={}",
            intersection.size(),
            hmax - hmin,
            kept,
            inlag.size());
    }

}  // namespace srouter
