//
// xzip: restoring a symbolic link from an archive, inside the extraction directory only.
//

#ifndef __PLZMA_SYMLINK_HPP__
#define __PLZMA_SYMLINK_HPP__ 1

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <string>

#include "plzma_private.hpp"

#if defined(LIBPLZMA_POSIX)
#  include <sys/stat.h>
#  include <unistd.h>
#endif

namespace plzma {
namespace symlinkUtils {

    enum class Outcome {
        created,  // the link exists now
        unsafe,   // refused: it could lead out of the extraction directory (or is not expressible)
        failed    // the OS refused; `err` holds errno
    };

#if defined(LIBPLZMA_POSIX)

    // A link may only be created if following it, from where it sits, can never leave `root`:
    //  - the target is relative, and `..` appears only as a leading run (`../../x`, never `a/../x`),
    //    so what it resolves to depends on the directory it sits in and on nothing created later;
    //  - that run climbs no higher than the link's own depth below `root`.
    // Since no link that escapes is ever created, a later entry written "through" an earlier link
    // (`link -> ..` then `link/evil`) cannot land outside either.
    // `linkPath`'s parent must already exist. An existing non-directory at `linkPath` is replaced.
    inline Outcome createInside(const char * root, const char * linkPath, const char * target, int & err) {
        err = 0;
        if (!root || !linkPath || !target || target[0] == 0 || target[0] == '/' || ::strlen(target) >= PATH_MAX) {
            return Outcome::unsafe;
        }

        size_t ups = 0;
        {
            bool sawName = false;
            const char * c = target;
            while (*c) {
                while (*c == '/') { c++; }
                const char * start = c;
                while (*c && *c != '/') { c++; }
                const size_t len = static_cast<size_t>(c - start);
                if (len == 0 || (len == 1 && start[0] == '.')) {
                    continue;
                }
                if (len == 2 && start[0] == '.' && start[1] == '.') {
                    if (sawName) {
                        return Outcome::unsafe;
                    }
                    ups++;
                } else {
                    sawName = true;
                }
            }
        }

        char rootReal[PATH_MAX];
        if (!::realpath(root, rootReal)) {
            err = errno;
            return Outcome::failed;
        }

        const std::string link(linkPath);
        const size_t slash = link.find_last_of('/');
        const std::string parent = (slash == std::string::npos) ? std::string(".") : (slash == 0 ? std::string("/") : link.substr(0, slash));
        char parentReal[PATH_MAX];
        if (!::realpath(parent.c_str(), parentReal)) {
            err = errno;
            return Outcome::failed;
        }

        const std::string rootStr(rootReal), parentStr(parentReal);
        const std::string rootDir = (rootStr.size() > 1 && rootStr.back() == '/') ? rootStr.substr(0, rootStr.size() - 1) : rootStr;
        if (rootDir != "/") {
            if (parentStr.compare(0, rootDir.size(), rootDir) != 0 ||
                (parentStr.size() > rootDir.size() && parentStr[rootDir.size()] != '/')) {
                return Outcome::unsafe; // outside, or a sibling such as "/a/bc" next to "/a/b"
            }
        }
        const std::string below = (rootDir == "/") ? parentStr : parentStr.substr(rootDir.size());
        size_t depth = 0;
        for (size_t i = 0; i < below.size(); i++) {
            if (below[i] == '/' && i + 1 < below.size() && below[i + 1] != '/') {
                depth++;
            }
        }
        if (ups > depth) {
            return Outcome::unsafe;
        }

        struct stat st;
        if (::lstat(linkPath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                err = EISDIR;
                return Outcome::failed;
            }
            if (::unlink(linkPath) != 0) {
                err = errno;
                return Outcome::failed;
            }
        }
        if (::symlink(target, linkPath) != 0) {
            err = errno;
            return Outcome::failed;
        }
        return Outcome::created;
    }

#else

    inline Outcome createInside(const char *, const char *, const char *, int & err) {
        err = 0;
        return Outcome::unsafe;
    }

#endif

} // namespace symlinkUtils
} // namespace plzma

#endif // !__PLZMA_SYMLINK_HPP__
