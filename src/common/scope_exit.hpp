#pragma once

template <typename F> struct ScopeExit {
    F fn;
    ~ScopeExit() { fn(); }
};
