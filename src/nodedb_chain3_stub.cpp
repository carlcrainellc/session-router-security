#include "nodedb.hpp"

#include "router/router.hpp"
#include "util/logging.hpp"

namespace srouter
{
    static auto logcat = srouter::log::Cat("nodedb");

    void NodeDB::chain3_try_reconcile()
    {
        // Embedded/core-only builds have no OMQ client; chain3 can only cold-fallback.
        log::warning(
            logcat,
            "chain3: OMQ not linked in this build; cold-fallback to local signed bootstrap if present");
        if (_bootstraps.empty())
            throw std::runtime_error{
                "[bootstrap] mode=chain3: RPCs failed and no signed bootstrap.signed cold fallback"};
        log::warning(
            logcat, "chain3 fallback: using local signed file ({} RCs)", _bootstraps.size());
    }

}  // namespace srouter
