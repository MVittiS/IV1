// Adapted from https://stackoverflow.com/a/34134071

#include <limits>   

namespace ConstexprNumeric {
    template <typename T>
    T constexpr abs(T x) {
        return x > T(0) ? x : -x;
    }

    template <typename T>
    T constexpr max(T a, T b) {
        return a > b ? a : b;
    }

    template <typename T>
    T constexpr sqrtNewtonRaphson(T x, T curr, T prev) {
        bool tooClose = (abs(curr - prev) 
                        / max(abs(curr), abs(prev)))
                        <= (T(1) - std::numeric_limits<T>::epsilon());

        return tooClose ? curr :
            sqrtNewtonRaphson<T>(x, (curr + x) / (curr * T(2)), curr);
    }
}

/*
* Constexpr version of the square root
* Return value:
*   - For a finite and non-negative value of "x", returns an approximation for the square root of "x"
*   - Otherwise, returns NaN
*/
template<typename T>
T constexpr constSqrt(T x) {
    return x >= T(0) && x < std::numeric_limits<T>::infinity()
        ? ConstexprNumeric::sqrtNewtonRaphson<T>(x, x, T(0))
        : std::numeric_limits<T>::quiet_NaN();
}