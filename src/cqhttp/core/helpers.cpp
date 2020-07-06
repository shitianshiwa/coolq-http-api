#include "./helpers.h"

#include "cqhttp/core/core.h"

using namespace std;

namespace cqhttp::helpers {
    string get_update_source_url(string rel_path) {
        auto url = app.config().get_string("update_source", "global");

        if (url == "global" || url == "github") {
            url = "https://raw.githubusercontent.com/richardchien/coolq-http-api-release/master/";
        }

        if (!boost::ends_with(url, "/")) {
            url = url + "/";
        }

        return url + rel_path;
    }
} // namespace cqhttp::helpers
