#ifndef BUFFER_CACHE_ALT_BLOCK_VERSION_HPP_
#define BUFFER_CACHE_ALT_BLOCK_VERSION_HPP_

#include <stdint.h>

// RSI: Put this in namespace alt.
class block_version_t {
public:
    block_version_t() : value_(0) { }

    block_version_t subsequent() const {
        block_version_t ret;
        ret.value_ = value_ + 1;
        return ret;
    }

    bool operator<(block_version_t other) const {
        return value_ < other.value_;
    }

    bool operator<=(block_version_t other) const {
        return value_ <= other.value_;
    }

    bool operator>=(block_version_t other) const {
        return value_ >= other.value_;
    }

    bool operator==(block_version_t other) const {
        return value_ == other.value_;
    }

    bool operator!=(block_version_t other) const {
        return !operator==(other);
    }

    uint64_t debug_value() const { return value_; }

private:
    uint64_t value_;
};


#endif  // BUFFER_CACHE_ALT_BLOCK_VERSION_HPP_
