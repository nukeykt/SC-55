#pragma once

#include <string_view>
#include <charconv>

template <typename T>
bool TryParse(std::string_view s, T& n)
{
    std::from_chars_result r = std::from_chars(s.data(), s.data() + s.size(), n);
    if (r.ec != std::errc{})
    {
        return false;
    }
    return true;
}

class CommandLineReader
{
public:
    CommandLineReader(int argc, char* argv[])
        : m_argc(argc), m_argv(argv), m_index(0)
    {
    }

    bool Next()
    {
        ++m_index;
        if (m_index < m_argc)
        {
            m_arg = m_argv[m_index];
            return true;
        }
        else
        {
            return false;
        }
    }

    template <typename... Strings>
    bool Any(Strings... strings)
    {
        return ((strings == m_arg) || ...);
    }

    std::string_view Arg() const
    {
        return m_arg;
    }

    template <typename T>
    bool TryParse(T& output)
    {
        return ::TryParse(Arg(), output);
    }

private:
    int    m_argc;
    char** m_argv;
    int    m_index;
    std::string_view m_arg;
};
