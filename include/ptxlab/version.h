#ifndef PTXLAB_VERSION_H
#define PTXLAB_VERSION_H

#include <string>

namespace ptxlab {

constexpr int VERSION_MAJOR = 0;
constexpr int VERSION_MINOR = 1;
constexpr int VERSION_PATCH = 0;
constexpr const char* VERSION_STRING = "0.1.0";

inline std::string getVersion() {
    return VERSION_STRING;
}

inline std::string getBanner() {
    return "PTXLab (kcc) - Verified Mini-Compiler Targeting PTX v" + getVersion();
}

} // namespace ptxlab

#endif // PTXLAB_VERSION_H
