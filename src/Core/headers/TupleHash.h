/**
 * @file TupleHash.h
 * @brief Custom hashing implementation for std::tuple to enable its use in unordered containers.
 * 
 * Provides a highly optimized, template-based hashing mechanism that allows std::tuple 
 * to be used as keys in std::unordered_map and std::unordered_set.
 */

#include <tuple>
#include <cstddef>
#include <type_traits>

/**
 * @namespace std
 * @brief Injects custom hash specializations into the standard namespace.
 */
namespace std {

    /**
     * @namespace tuple_hash_impl
     * @brief Internal implementation details for tuple hashing.
     */
    inline namespace tuple_hash_impl {

        /**
         * @struct hash_constants
         * @brief Platform-aware constants for the hashing algorithm.
         * 
         * Provides the golden ratio constant scaled for the system's size_t (32-bit or 64-bit).
         */
        template<typename SizeT>
        struct hash_constants;

        /// @brief 32-bit Golden Ratio constant.
        template<>
        struct hash_constants<std::uint32_t> {
            static constexpr std::uint32_t golden_ratio = 0x9e3779b9;
        };

        /// @brief 64-bit Golden Ratio constant.
        template<>
        struct hash_constants<std::uint64_t> {
            static constexpr std::uint64_t golden_ratio = 0x9e3779b97f4a7c15;
        };

        /**
         * @brief Combines a new value into an existing hash seed.
         * 
         * Uses the established Boost hash_combine formula:
         * seed ^= hash(v) + golden_ratio + (seed << 6) + (seed >> 2)
         * 
         * @param seed The cumulative hash seed to update.
         * @param v The value to hash and combine.
         */
        template <typename T>
        [[gnu::always_inline, msvc::forceinline]]
        inline constexpr void hash_combine(std::size_t& seed, const T& v) noexcept(noexcept(std::hash<T>{}(v)))
        {
            constexpr std::size_t golden = hash_constants<std::size_t>::golden_ratio;
            seed ^= std::hash<T>{}(v) + golden + (seed << 6) + (seed >> 2);
        }

#if __cplusplus >= 201703L
        /**
         * @brief Recursion-free tuple hashing using C++17 fold expressions.
         * 
         * This implementation generates optimal code with zero function call overhead 
         * by expanding all hash_combine calls in a single expression.
         */
        template <typename Tuple, std::size_t... Indices>
        [[nodiscard]] constexpr std::size_t hash_tuple_impl(const Tuple& tuple, std::index_sequence<Indices...>)
            noexcept(noexcept((hash_combine(std::declval<std::size_t&>(), std::get<Indices>(tuple)), ...)))
        {
            std::size_t seed = 0;
            (hash_combine(seed, std::get<Indices>(tuple)), ...);
            return seed;
        }
#else
        /**
         * @brief Fallback C++14 implementation using template recursion.
         */
        template <typename Tuple, std::size_t Index = std::tuple_size<Tuple>::value - 1>
        struct HashValueImpl
        {
            [[gnu::always_inline]]
            static constexpr void apply(std::size_t& seed, const Tuple& tuple)
                noexcept(noexcept(hash_combine(seed, std::get<Index>(tuple))) &&
                        noexcept(HashValueImpl<Tuple, Index - 1>::apply(seed, tuple)))
            {
                HashValueImpl<Tuple, Index - 1>::apply(seed, tuple);
                hash_combine(seed, std::get<Index>(tuple));
            }
        };

        template <typename Tuple>
        struct HashValueImpl<Tuple, 0>
        {
            [[gnu::always_inline]]
            static constexpr void apply(std::size_t& seed, const Tuple& tuple)
                noexcept(noexcept(hash_combine(seed, std::get<0>(tuple))))
            {
                hash_combine(seed, std::get<0>(tuple));
            }
        };

        template <typename Tuple, std::size_t... Indices>
        [[nodiscard]] constexpr std::size_t hash_tuple_impl(const Tuple& tuple, std::index_sequence<Indices...>)
            noexcept(noexcept(HashValueImpl<Tuple>::apply(std::declval<std::size_t&>(), tuple)))
        {
            std::size_t seed = 0;
            HashValueImpl<Tuple>::apply(seed, tuple);
            return seed;
        }
#endif

    } // inline namespace tuple_hash_impl

    /**
     * @brief Template specialization of std::hash for std::tuple.
     * 
     * Enables tuples of hashable types to be used as keys in unordered containers.
     */
    template <typename... TT>
    struct hash<std::tuple<TT...>>
    {
        [[nodiscard]] constexpr std::size_t operator()(const std::tuple<TT...>& tuple) const
            noexcept(noexcept(hash_tuple_impl(tuple, std::index_sequence_for<TT...>{})))
        {
            return hash_tuple_impl(tuple, std::index_sequence_for<TT...>{});
        }
    };

    /**
     * @brief Optimization for empty tuples.
     */
    template <>
    struct hash<std::tuple<>>
    {
        [[nodiscard]] constexpr std::size_t operator()(const std::tuple<>&) const noexcept
        {
            return 0;
        }
    };

    /**
     * @brief Optimization for single-element tuples to avoid seed-combine overhead.
     */
    template <typename T>
    struct hash<std::tuple<T>>
    {
        [[nodiscard]] constexpr std::size_t operator()(const std::tuple<T>& tuple) const
            noexcept(noexcept(std::hash<T>{}(std::get<0>(tuple))))
        {
            return std::hash<T>{}(std::get<0>(tuple));
        }
    };

} // namespace std
