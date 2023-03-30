/*
* Copyright (c) 2023 Zach Gerstman
* https://github.com/crabmandable/zxorm
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at https://mozilla.org/MPL/2.0/.
*/
#pragma once
#include "zxorm/common.hpp"
#include <utility>
#include <type_traits>

namespace zxorm {
    template<ContinuousContainer C>
    requires(not OptionalT<C>)
    [[nodiscard]] bool safe_resize(C& container, auto size) {
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
        C& _container;
        using PlainC = typename remove_optional<C>::type;

        public:
        MetaContainer(C& c) : _container{c} {}

        PlainC& value() {
            return _container;
        }

        bool has_value() {
            return true;
        }

        template<typename SizeT>
        [[nodiscard]] bool resize(SizeT len) {
            return safe_resize(_container, len);
        }

        void clear() {
            if constexpr (requires { _container.clear(); }) {
                _container.clear();
            } else {
                std::fill_n(_container.begin(), _container.size(), 0);
            }
        }

        auto size () const -> decltype(std::declval<PlainC>().size()) {
            return _container.size();
        }

        auto data () const -> decltype(std::declval<PlainC>().data()) {
            return _container.data();
        }

        auto data () -> decltype(std::declval<PlainC>().data()) {
            return _container.data();
        }
    };

    template<ContinuousContainer C>
    requires(OptionalT<C>)
    class MetaContainer<C> {
        private:
        C& _container;
        using PlainC = typename remove_optional<std::remove_cvref_t<C>>::type;

        public:
        MetaContainer(C& c) : _container{c} {}

        PlainC& value() {
            return _container.value();
        }

        bool has_value() {
            return _container.has_value();
        }

        template<typename SizeT>
        [[nodiscard]] bool resize(SizeT len) {
            if (!_container.has_value()) _container = PlainC{};
            auto& value = _container.value();
            return safe_resize(value, len);
        }

        void clear() {
            _container = std::nullopt;
        }

        auto size () const -> decltype(std::declval<PlainC>().size()) {
            if (!_container.has_value()) return 0;
            return _container.value().size();
        }

        auto data () const -> decltype(std::declval<const PlainC>().data()) {
            if (!_container.has_value()) {
                return nullptr;
            }
            return _container.value().data();
        }

        auto data () -> decltype(std::declval<PlainC>().data()) {
            return _container.value().data();
        }
    };
};
