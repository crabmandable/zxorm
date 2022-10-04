#pragma once
#include <iterator>
#include "zxorm/orm/statement.hpp"
#include "zxorm/result.hpp"

namespace zxorm {
    template <typename Table>
    class RecordIterator
    {
    private:
        using record_t = Table::object_class;
        Statement _stmt;
    public:
        RecordIterator(Statement&& stmt) : _stmt{std::move(stmt)} { }

        class iterator:  public std::iterator<std::input_iterator_tag, OptionalResult<record_t>>
        {
        private:
            Statement* _stmt = nullptr;
            OptionalResult<record_t> _current = std::nullopt;
        public:
            using value_type = OptionalResult<record_t>;
            using iterator_category = std::input_iterator_tag;

            iterator(Statement& stmt): _stmt{&stmt} {
                ++(*this);
            }
            iterator() = default;

            iterator& operator++() {
                if (!_stmt || _stmt->done()) {
                    return *this;
                }
                auto err = _stmt->step();
                if (err) {
                    _current = err.value();
                } else {
                    _current = Table::get_row(*_stmt);
                }
                return *this;
            }

            OptionalResult<record_t>& operator*() {
                return _current;
            }
            bool operator==(const iterator& other) const {
                // nullptr means end
                if (!_stmt && other._stmt && other._stmt->done()) return true;
                if (!other._stmt && _stmt && _stmt->done()) return true;

                return _stmt == other._stmt &&
                    (!_stmt || _stmt->step_count() == other._stmt->step_count());
            }
            bool operator!=(const iterator& other) const { return !(*this == other); }
        };

        iterator begin() {
            auto err = _stmt.rewind();
            if (err) {
#if __cpp_exceptions
                throw std::runtime_error(std::string("Unable to rewind statement: ") + std::string(err.value()));
#endif
                // this would be pretty confusing
                return iterator();
            }

            return iterator(_stmt);
        }
        iterator end() {
            return iterator();
        }

        Result<std::vector<record_t>> to_vector() {
            std::vector<record_t> records;
            for(OptionalResult<record_t>& result: *this) {
                if (result.is_error()) {
                    return result.error();
                }
                if (result.value()) {
                    records.emplace_back(result.value().value());
                }
            }
            return records;
        }
    };
};
