#pragma once
#include "zxorm/common.hpp"
#include <utility>
#include <type_traits>

namespace zxorm {
    template<ContinuousContainer C>
    requires(not OptionalT<C>)
    [[nodiscard]] bool safeResize(C& container, auto size) {
        if constexpr (requires { container.resize(size); }) {
            container.resize(size);
        } else {
            if (container.size() < size) {
                return false;
            }
        }

        return true;
    }

    template<ContinuousContainer C>
    class MetaContainer {
        private:
        C& container;
        using PlainC = typename remove_optional<C>::type;

        public:
        MetaContainer(C& c) : container{c} {}

        PlainC& value() {
            return container;
        }

        bool has_value() {
            return true;
        }

        template<typename SizeT>
        [[nodiscard]] bool resize(SizeT len) {
            return safeResize(container, len);
        }

        void clear() {
            if constexpr (requires { container.clear(); }) {
                container.clear();
            } else {
                std::fill_n(container.begin(), container.size(), 0);
            }
        }

        auto size () const -> decltype(std::declval<PlainC>().size()) {
            return container.size();
        }

        auto data () const -> decltype(std::declval<PlainC>().data()) {
            return container.data();
        }

        auto data () -> decltype(std::declval<PlainC>().data()) {
            return container.data();
        }
    };

    template<ContinuousContainer C>
    requires(OptionalT<C>)
    class MetaContainer<C> {
        private:
        C& container;
        using PlainC = remove_optional<std::remove_cvref_t<C>>::type;

        public:
        MetaContainer(C& c) : container{c} {}

        PlainC& value() {
            return container.value();
        }

        bool has_value() {
            return container.has_value();
        }

        template<typename SizeT>
        [[nodiscard]] bool resize(SizeT len) {
            if (!container.has_value()) container = PlainC{};
            auto& value = container.value();
            return safeResize(value, len);
        }

        void clear() {
            container = std::nullopt;
        }

        auto size () const -> decltype(std::declval<PlainC>().size()) {
            if (!container.has_value()) return 0;
            return container.value().size();
        }

        auto data () const -> decltype(std::declval<const PlainC>().data()) {
            if (!container.has_value()) {
                return nullptr;
            }
            return container.value().data();
        }

        auto data () -> decltype(std::declval<PlainC>().data()) {
            return container.value().data();
        }
    };
};
