#pragma once
#include "zxorm/common.hpp"
#include <utility>
#include <type_traits>

namespace zxorm {
    template<ContinuousContainer C>
    class MetaContainer {
        private:
        C& container;
        using PlainC = typename remove_optional<C>::type;

        public:
        MetaContainer(C& c) : container{c} {}

        static constexpr bool isOptional = is_optional<C>::value;

        PlainC& value() {
            if constexpr (isOptional) {
                return container.value();
            } else {
                return container;
            }
        }

        bool has_value() {
            if constexpr (isOptional) {
                return container.has_value();
            } else {
                return true;
            }
        }

        // meta resize can fail if tyring to resize an array to something bigger than it is
        // optional containers will be created with the size
        template<typename SizeT>
        [[nodiscard]] bool resize(SizeT len) {
            PlainC* unwrapped;
            if constexpr (isOptional) {
                if (!container.has_value()) container = PlainC{};
                unwrapped = &container.value();
            } else {
                unwrapped = &container;
            }

            if constexpr (requires { unwrapped->resize(len); }) {
                unwrapped->resize(len);
            } else {
                if (unwrapped->size() < len) {
                    return false;
                }
            }

            return true;
        }

        void clear() {
            if constexpr (isOptional) {
                container = std::nullopt;
            } else if constexpr (requires { container.clear(); }) {
                container.clear();
            } else {
                std::fill_n(container.begin(), container.size(), 0);
            }
        }

        auto size () const -> decltype(std::declval<PlainC>().size()) {
           if constexpr (isOptional) {
               if (!container.has_value()) return 0;
               return container.value().size();
           } else {
               return container.size();
           }
        }

        auto data () const -> decltype(std::declval<PlainC>().data()) {
           if constexpr (isOptional) {
               if (!container.has_value()) {
                   return nullptr;
               }
               return container.value().data();
           } else {
               return container.data();
           }
        }

        auto data () -> decltype(std::declval<PlainC>().data()) {
            if constexpr (isOptional) {
                return container.value().data();
            } else {
                return container.data();
            }
        }
    };
};
