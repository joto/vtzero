#ifndef VTZERO_GEOMETRY_HPP
#define VTZERO_GEOMETRY_HPP

/*****************************************************************************

vtzero - Tiny and fast vector tile decoder and encoder in C++.

This file is from https://github.com/mapbox/vtzero where you can find more
documentation.

*****************************************************************************/

/**
 * @file geometry.hpp
 *
 * @brief Contains classes and functions related to geometry handling.
 */

#include "attributes.hpp"
#include "exception.hpp"
#include "geometry_basics.hpp"
#include "types.hpp"
#include "util.hpp"

#include <protozero/pbf_reader.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <utility>

namespace vtzero {

    /// A simple point class
    struct point {

        /// X coordinate
        int32_t x = 0;

        /// Y coordinate
        int32_t y = 0;

        /// Default construct to 0 coordinates
        constexpr point() noexcept = default;

        /// Constructor
        constexpr point(int32_t x_, int32_t y_) noexcept :
            x(x_),
            y(y_) {
        }

    }; // struct point

    /**
     * Helper function to create a point from any type that has members x
     * and y.
     *
     * If your point type doesn't have members x any y, you can overload this
     * function for your type and it will be used by vtzero.
     */
    template <typename TPoint>
    point create_vtzero_point(const TPoint& p) noexcept {
        return {p.x, p.y};
    }

    /// Points are equal if their coordinates are
    inline constexpr bool operator==(const point a, const point b) noexcept {
        return a.x == b.x && a.y == b.y;
    }

    /// Points are not equal if their coordinates aren't
    inline constexpr bool operator!=(const point a, const point b) noexcept {
        return !(a == b);
    }

    /// A simple point class
    struct unscaled_point {

        /// X coordinate
        int32_t x = 0;

        /// Y coordinate
        int32_t y = 0;

        /// elevation
        int64_t z = 0;

        /// Default construct to 0 coordinates
        constexpr unscaled_point() noexcept = default;

        /// Constructor
        constexpr unscaled_point(int32_t x_, int32_t y_, int64_t z_ = 0) noexcept :
            x(x_),
            y(y_),
            z(z_) {
        }

    }; // struct unscaled_point

    /// unscaled_points are equal if their coordinates are
    inline constexpr bool operator==(const unscaled_point& a, const unscaled_point& b) noexcept {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }

    /// unscaled_points are not equal if their coordinates aren't
    inline constexpr bool operator!=(const unscaled_point& a, const unscaled_point& b) noexcept {
        return !(a == b);
    }

    namespace detail {

        // The null_iterator simply does nothing but has a valid iterator
        // interface. It is used for simple 2D geometries without geometric
        // attributes and for testing.
        template <typename T>
        struct null_iterator {

            bool operator==(null_iterator /*other*/) const noexcept {
                return true;
            }

            bool operator!=(null_iterator /*other*/) const noexcept {
                return false;
            }

            T operator*() const noexcept {
                return 0;
            }

            null_iterator operator++() const noexcept {
                return *this;
            }

            null_iterator operator++(int) const noexcept {
                return *this;
            }

        }; // struct null_iterator

        using dummy_elev_iterator = null_iterator<int64_t>;
        using dummy_attr_iterator = null_iterator<uint64_t>;

        template <typename TIterator>
        class geometric_attribute {

            TIterator m_it{};
            index_value m_key_index{};
            index_value m_scaling_index{};
            uint64_t m_count = 0;
            int64_t m_value = 0;

        public:

            geometric_attribute() noexcept = default;

            geometric_attribute(TIterator it, uint64_t key_index, uint64_t scaling_index, uint64_t count) noexcept :
                m_it(it),
                m_key_index(static_cast<uint32_t>(key_index)),
                m_scaling_index(static_cast<uint32_t>(scaling_index)),
                m_count(count) {
            }

            index_value key_index() const noexcept {
                return m_key_index;
            }

            index_value scaling_index() const noexcept {
                return m_scaling_index;
            }

            bool get_next_value() noexcept {
                if (m_count == 0) {
                    return false;
                }
                const uint64_t raw_value = *m_it++;
                --m_count;
                if (raw_value == 0) {
                    return false;
                }
                m_value += protozero::decode_zigzag64(raw_value - 1);
                return true;
            }

            int64_t value() const noexcept {
                return m_value;
            }

        }; // class geometric_attribute

        template <>
        class geometric_attribute<dummy_attr_iterator> {

        public:

            index_value key_index() const noexcept {
                return {};
            }

            index_value scaling_index() const noexcept {
                return {};
            }

            bool get_next_value() noexcept {
                return false;
            }

            int64_t value() const noexcept {
                return 0;
            }

        }; // class geometric_attribute

        template <int MaxGeometricAttributes, typename TIterator>
        class geometric_attribute_collection {

            std::array<geometric_attribute<TIterator>, MaxGeometricAttributes> m_attrs;

            std::size_t m_size = 0;

        public:

            geometric_attribute_collection(TIterator it, const TIterator end) :
                m_attrs() {
                while (it != end && m_size < MaxGeometricAttributes) {
                    const uint64_t complex_value = *it++;
                    if ((complex_value & 0xfu) != 10) {
                        throw format_exception{"geometric attributes must be of type number list"};
                    }
                    if (it == end) {
                        throw format_exception{"geometric attributes end too soon"};
                    }

                    auto attr_count = *it++;
                    if (it == end) {
                        throw format_exception{"geometric attributes end too soon"};
                    }
                    const uint64_t scaling = *it++;
                    if (it == end) {
                        throw format_exception{"geometric attributes end too soon"};
                    }

                    m_attrs[m_size] = {it, complex_value >> 4u, scaling, attr_count};
                    ++m_size;

                    while (attr_count-- > 0) {
                        ++it;
                        if (attr_count != 0 && it == end) {
                            throw format_exception{"geometric attributes end too soon"};
                        }
                    }
                }
            }

            typename std::array<geometric_attribute<TIterator>, MaxGeometricAttributes>::iterator begin() noexcept {
                return m_attrs.begin();
            }

            typename std::array<geometric_attribute<TIterator>, MaxGeometricAttributes>::iterator end() noexcept {
                return m_attrs.end();
            }

        }; // class geometric_attribute_collection

        template <int MaxGeometricAttributes>
        class geometric_attribute_collection<MaxGeometricAttributes, dummy_attr_iterator> {

            geometric_attribute<dummy_attr_iterator> dummy{};

        public:

            geometric_attribute_collection(dummy_attr_iterator /*it*/, const dummy_attr_iterator /*end*/) noexcept {
            }

            geometric_attribute<dummy_attr_iterator>* begin() noexcept {
                return &dummy;
            }

            geometric_attribute<dummy_attr_iterator>* end() noexcept {
                return &dummy;
            }

        };

        /**
         * Decode a geometry as specified in spec 4.3. This templated class can
         * be instantiated with a different iterator type for testing than for
         * normal use.
         */
        template <int Dimensions, int MaxGeometricAttributes, typename TGeomIterator, typename TElevIterator = dummy_elev_iterator, typename TAttrIterator = dummy_attr_iterator>
        class extended_geometry_decoder {

            static_assert(Dimensions == 2 || Dimensions == 3, "Need 2 or 3 dimensions");

            static inline constexpr int64_t det(const unscaled_point& a, const unscaled_point& b) noexcept {
                return static_cast<int64_t>(a.x) * static_cast<int64_t>(b.y) -
                       static_cast<int64_t>(b.x) * static_cast<int64_t>(a.y);
            }

            TGeomIterator m_geom_it;
            TGeomIterator m_geom_end;

            TElevIterator m_elev_it;
            TElevIterator m_elev_end;

            TAttrIterator m_attr_it;
            TAttrIterator m_attr_end;

            unscaled_point m_cursor;

            // maximum value for m_count before we throw an exception
            uint32_t m_max_count;

            /**
             * The current count value is set from the CommandInteger and
             * then counted down with each next_point() call. So it must be
             * greater than 0 when next_point() is called and 0 when
             * next_command() is called.
             */
            uint32_t m_count = 0;

        public:

            extended_geometry_decoder(TGeomIterator geom_begin, TGeomIterator geom_end,
                                      TElevIterator elev_begin, TElevIterator elev_end,
                                      TAttrIterator attr_begin, TAttrIterator attr_end,
                                      std::size_t max) :
                m_geom_it(geom_begin),
                m_geom_end(geom_end),
                m_elev_it(elev_begin),
                m_elev_end(elev_end),
                m_attr_it(attr_begin),
                m_attr_end(attr_end),
                m_max_count(static_cast<uint32_t>(max)) {
                vtzero_assert(max <= detail::max_command_count());
            }

            uint32_t count() const noexcept {
                return m_count;
            }

            bool done() const noexcept {
                return m_geom_it == m_geom_end &&
                       m_elev_it == m_elev_end;
            }

            bool next_command(const CommandId expected_command_id) {
                vtzero_assert(m_count == 0);

                if (m_geom_it == m_geom_end) {
                    return false;
                }

                const auto command_id = get_command_id(*m_geom_it);
                if (command_id != static_cast<uint32_t>(expected_command_id)) {
                    throw geometry_exception{std::string{"expected command "} +
                                             std::to_string(static_cast<uint32_t>(expected_command_id)) +
                                             " but got " +
                                             std::to_string(command_id)};
                }

                if (expected_command_id == CommandId::CLOSE_PATH) {
                    // spec 4.3.3.3 "A ClosePath command MUST have a command count of 1"
                    if (get_command_count(*m_geom_it) != 1) {
                        throw geometry_exception{"ClosePath command count is not 1"};
                    }
                } else {
                    m_count = get_command_count(*m_geom_it);
                    if (m_count > m_max_count) {
                        throw geometry_exception{"count too large"};
                    }
                }

                ++m_geom_it;

                return true;
            }

            unscaled_point next_point() {
                vtzero_assert(m_count > 0);

                if (m_geom_it == m_geom_end || std::next(m_geom_it) == m_geom_end) {
                    throw geometry_exception{"too few points in geometry"};
                }

                // spec 4.3.2 "A ParameterInteger is zigzag encoded"
                m_cursor.x += protozero::decode_zigzag32(*m_geom_it++);
                m_cursor.y += protozero::decode_zigzag32(*m_geom_it++);

                if (Dimensions == 3 && m_elev_it != m_elev_end) {
                    m_cursor.z += *m_elev_it++;
                }

                --m_count;

                return m_cursor;
            }

            template <typename TGeomHandler>
            detail::get_result_t<TGeomHandler> decode_point(TGeomHandler&& geom_handler) {
                // spec 4.3.4.2 "MUST consist of a single MoveTo command"
                if (!next_command(CommandId::MOVE_TO)) {
                    throw geometry_exception{"expected MoveTo command (spec 4.3.4.2)"};
                }

                // spec 4.3.4.2 "command count greater than 0"
                if (count() == 0) {
                    throw geometry_exception{"MoveTo command count is zero (spec 4.3.4.2)"};
                }

                geometric_attribute_collection<MaxGeometricAttributes, TAttrIterator> geom_attributes{m_attr_it, m_attr_end};

                std::forward<TGeomHandler>(geom_handler).points_begin(count());
                while (count() > 0) {
                    std::forward<TGeomHandler>(geom_handler).points_point(std::forward<TGeomHandler>(geom_handler).convert(next_point()));
                    for (auto& geom_attr : geom_attributes) {
                        if (geom_attr.get_next_value()) {
                            detail::call_points_attr(std::forward<TGeomHandler>(geom_handler), geom_attr.key_index(), geom_attr.scaling_index(), geom_attr.value());
                        } else {
                            detail::call_points_null_attr(std::forward<TGeomHandler>(geom_handler), geom_attr.key_index());
                        }
                    }
                }

                // spec 4.3.4.2 "MUST consist of of a single ... command"
                if (!done()) {
                    throw geometry_exception{"additional data after end of geometry (spec 4.3.4.2)"};
                }

                std::forward<TGeomHandler>(geom_handler).points_end();

                return detail::get_result<TGeomHandler>::of(std::forward<TGeomHandler>(geom_handler));
            }

            template <typename TGeomHandler>
            detail::get_result_t<TGeomHandler> decode_linestring(TGeomHandler&& geom_handler) {
                geometric_attribute_collection<MaxGeometricAttributes, TAttrIterator> geom_attributes{m_attr_it, m_attr_end};

                // spec 4.3.4.3 "1. A MoveTo command"
                while (next_command(CommandId::MOVE_TO)) {
                    // spec 4.3.4.3 "with a command count of 1"
                    if (count() != 1) {
                        throw geometry_exception{"MoveTo command count is not 1 (spec 4.3.4.3)"};
                    }

                    const auto first_point = std::forward<TGeomHandler>(geom_handler).convert(next_point());

                    // spec 4.3.4.3 "2. A LineTo command"
                    if (!next_command(CommandId::LINE_TO)) {
                        throw geometry_exception{"expected LineTo command (spec 4.3.4.3)"};
                    }

                    // spec 4.3.4.3 "with a command count greater than 0"
                    if (count() == 0) {
                        throw geometry_exception{"LineTo command count is zero (spec 4.3.4.3)"};
                    }

                    std::forward<TGeomHandler>(geom_handler).linestring_begin(count() + 1);

                    std::forward<TGeomHandler>(geom_handler).linestring_point(first_point);
                    for (auto& geom_attr : geom_attributes) {
                        if (geom_attr.get_next_value()) {
                            detail::call_points_attr(std::forward<TGeomHandler>(geom_handler), geom_attr.key_index(), geom_attr.scaling_index(), geom_attr.value());
                        } else {
                            detail::call_points_null_attr(std::forward<TGeomHandler>(geom_handler), geom_attr.key_index());
                        }
                    }

                    while (count() > 0) {
                        std::forward<TGeomHandler>(geom_handler).linestring_point(std::forward<TGeomHandler>(geom_handler).convert(next_point()));
                        for (auto& geom_attr : geom_attributes) {
                            if (geom_attr.get_next_value()) {
                                detail::call_points_attr(std::forward<TGeomHandler>(geom_handler), geom_attr.key_index(), geom_attr.scaling_index(), geom_attr.value());
                            } else {
                                detail::call_points_null_attr(std::forward<TGeomHandler>(geom_handler), geom_attr.key_index());
                            }
                        }
                    }

                    std::forward<TGeomHandler>(geom_handler).linestring_end();
                }

                return detail::get_result<TGeomHandler>::of(std::forward<TGeomHandler>(geom_handler));
            }

            template <typename TGeomHandler>
            detail::get_result_t<TGeomHandler> decode_polygon(TGeomHandler&& geom_handler) {
                geometric_attribute_collection<MaxGeometricAttributes, TAttrIterator> geom_attributes{m_attr_it, m_attr_end};

                // spec 4.3.4.4 "1. A MoveTo command"
                while (next_command(CommandId::MOVE_TO)) {
                    // spec 4.3.4.4 "with a command count of 1"
                    if (count() != 1) {
                        throw geometry_exception{"MoveTo command count is not 1 (spec 4.3.4.4)"};
                    }

                    int64_t sum = 0;
                    const auto start_point = next_point();
                    auto last_point = start_point;

                    // spec 4.3.4.4 "2. A LineTo command"
                    if (!next_command(CommandId::LINE_TO)) {
                        throw geometry_exception{"expected LineTo command (spec 4.3.4.4)"};
                    }

                    std::forward<TGeomHandler>(geom_handler).ring_begin(count() + 2);

                    std::forward<TGeomHandler>(geom_handler).ring_point(std::forward<TGeomHandler>(geom_handler).convert(start_point));
                    for (auto& geom_attr : geom_attributes) {
                        if (geom_attr.get_next_value()) {
                            detail::call_points_attr(std::forward<TGeomHandler>(geom_handler), geom_attr.key_index(), geom_attr.scaling_index(), geom_attr.value());
                        } else {
                            detail::call_points_null_attr(std::forward<TGeomHandler>(geom_handler), geom_attr.key_index());
                        }
                    }

                    while (count() > 0) {
                        const auto p = next_point();
                        sum += det(last_point, p);
                        last_point = p;
                        std::forward<TGeomHandler>(geom_handler).ring_point(std::forward<TGeomHandler>(geom_handler).convert(p));
                        for (auto& geom_attr : geom_attributes) {
                            if (geom_attr.get_next_value()) {
                                detail::call_points_attr(std::forward<TGeomHandler>(geom_handler), geom_attr.key_index(), geom_attr.scaling_index(), geom_attr.value());
                            } else {
                                detail::call_points_null_attr(std::forward<TGeomHandler>(geom_handler), geom_attr.key_index());
                            }
                        }
                    }

                    // spec 4.3.4.4 "3. A ClosePath command"
                    if (!next_command(CommandId::CLOSE_PATH)) {
                        throw geometry_exception{"expected ClosePath command (4.3.4.4)"};
                    }

                    sum += det(last_point, start_point);

                    std::forward<TGeomHandler>(geom_handler).ring_point(std::forward<TGeomHandler>(geom_handler).convert(start_point));

                    std::forward<TGeomHandler>(geom_handler).ring_end(sum > 0 ? ring_type::outer :
                                                                      sum < 0 ? ring_type::inner : ring_type::invalid);
                }

                return detail::get_result<TGeomHandler>::of(std::forward<TGeomHandler>(geom_handler));
            }

        }; // class extended_geometry_decoder

        /**
         * Decode a 2d geometry as specified in spec 4.3 from a sequence of 32
         * bit unsigned integers. This templated class can be instantiated with
         * a different iterator type for testing than for normal use.
         */
        template <typename TIterator>
        class geometry_decoder : public extended_geometry_decoder<2, 0, TIterator, dummy_elev_iterator, dummy_attr_iterator> {

        public:

            using iterator_type = TIterator;

            geometry_decoder(iterator_type begin, iterator_type end, std::size_t max) :
                extended_geometry_decoder<2, 0, iterator_type, dummy_elev_iterator, dummy_attr_iterator>(
                                          begin, end,
                                          dummy_elev_iterator{}, dummy_elev_iterator{},
                                          dummy_attr_iterator{}, dummy_attr_iterator{},
                                          max) {
            }

        }; // class geometry_decoder

    } // namespace detail

    /**
     * Decode a point geometry.
     *
     * @tparam TGeomHandler Handler class. See tutorial for details.
     * @param geometry The geometry as returned by feature.geometry().
     * @param geom_handler An object of TGeomHandler.
     * @returns whatever geom_handler.result() returns if that function exists,
     *          void otherwise
     * @throws geometry_error If there is a problem with the geometry.
     * @pre Geometry must be a point geometry.
     */
    template <typename TGeomHandler>
    detail::get_result_t<TGeomHandler> decode_point_geometry(const geometry& geometry, TGeomHandler&& geom_handler) {
        vtzero_assert(geometry.type() == GeomType::POINT);
        detail::geometry_decoder<decltype(geometry.begin())> decoder{geometry.begin(), geometry.end(), geometry.data().size() / 2};
        return decoder.decode_point(std::forward<TGeomHandler>(geom_handler));
    }

    /**
     * Decode a linestring geometry.
     *
     * @tparam TGeomHandler Handler class. See tutorial for details.
     * @param geometry The geometry as returned by feature.geometry().
     * @param geom_handler An object of TGeomHandler.
     * @returns whatever geom_handler.result() returns if that function exists,
     *          void otherwise
     * @throws geometry_error If there is a problem with the geometry.
     * @pre Geometry must be a linestring geometry.
     */
    template <typename TGeomHandler>
    detail::get_result_t<TGeomHandler> decode_linestring_geometry(const geometry& geometry, TGeomHandler&& geom_handler) {
        vtzero_assert(geometry.type() == GeomType::LINESTRING);
        detail::geometry_decoder<decltype(geometry.begin())> decoder{geometry.begin(), geometry.end(), geometry.data().size() / 2};
        return decoder.decode_linestring(std::forward<TGeomHandler>(geom_handler));
    }

    /**
     * Decode a polygon geometry.
     *
     * @tparam TGeomHandler Handler class. See tutorial for details.
     * @param geometry The geometry as returned by feature.geometry().
     * @param geom_handler An object of TGeomHandler.
     * @returns whatever geom_handler.result() returns if that function exists,
     *          void otherwise
     * @throws geometry_error If there is a problem with the geometry.
     * @pre Geometry must be a polygon geometry.
     */
    template <typename TGeomHandler>
    detail::get_result_t<TGeomHandler> decode_polygon_geometry(const geometry& geometry, TGeomHandler&& geom_handler) {
        vtzero_assert(geometry.type() == GeomType::POLYGON);
        detail::geometry_decoder<decltype(geometry.begin())> decoder{geometry.begin(), geometry.end(), geometry.data().size() / 2};
        return decoder.decode_polygon(std::forward<TGeomHandler>(geom_handler));
    }

    /**
     * Decode a geometry.
     *
     * @tparam TGeomHandler Handler class. See tutorial for details.
     * @param geometry The geometry as returned by feature.geometry().
     * @param geom_handler An object of TGeomHandler.
     * @returns whatever geom_handler.result() returns if that function exists,
     *          void otherwise
     * @throws geometry_error If the geometry has type UNKNOWN of if there is
     *                        a problem with the geometry.
     */
    template <typename TGeomHandler>
    detail::get_result_t<TGeomHandler> decode_geometry(const geometry& geometry, TGeomHandler&& geom_handler) {
        detail::geometry_decoder<decltype(geometry.begin())> decoder{geometry.begin(), geometry.end(), geometry.data().size() / 2};
        switch (geometry.type()) {
            case GeomType::POINT:
                return decoder.decode_point(std::forward<TGeomHandler>(geom_handler));
            case GeomType::LINESTRING:
                return decoder.decode_linestring(std::forward<TGeomHandler>(geom_handler));
            case GeomType::POLYGON:
                return decoder.decode_polygon(std::forward<TGeomHandler>(geom_handler));
            default:
                break;
        }
        throw geometry_exception{"unknown geometry type"};
    }

} // namespace vtzero

#endif // VTZERO_GEOMETRY_HPP
