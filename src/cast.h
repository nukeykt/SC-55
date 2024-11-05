#pragma once

#include <utility>
#include <source_location>
#include <cstdio>

template <typename R, typename T>
[[nodiscard]]
inline R RangeCast(T value, const std::source_location where = std::source_location::current())
{
    if (!std::in_range<R>(value)) [[unlikely]]
    {
        fprintf(stderr,
                "WARN: %s:%s:%d: Cast value out of range\n",
                where.file_name(),
                where.function_name(),
                (int)where.line());
    }
    return (R)value;
}
