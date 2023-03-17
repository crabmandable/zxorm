#pragma once
#include <iterator>
#include <memory>
#include "zxorm/orm/statement.hpp"

namespace zxorm {
    template <typename return_t>
    class RecordIterator
    {
    private:
        using row_reader_t = std::function<return_t(Statement&)>;
        std::shared_ptr<Statement> _stmt;
        row_reader_t _row_reader;
    public:
        RecordIterator(std::shared_ptr<Statement> stmt,  row_reader_t row_reader) :
            _stmt{std::move(stmt)}, _row_reader{row_reader} { }

        class iterator
        {
        private:
            Statement* _stmt = nullptr;
            row_reader_t _row_reader;
            return_t _current;
        public:
            using iterator_category = std::input_iterator_tag;

            iterator(Statement* stmt, row_reader_t row_reader): _stmt{stmt}, _row_reader{row_reader} {
                ++(*this);
            }
            iterator() = default;

            iterator& operator++() {
                if (!_stmt || _stmt->done()) {
                    return *this;
                }
                _stmt->step();
                if (!_stmt->done()) {
                    _current = _row_reader(*_stmt);
                }
                return *this;
            }

            return_t& operator*() {
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
            _stmt->rewind();
            return iterator(_stmt.get(), _row_reader);
        }
        iterator end() {
            return iterator();
        }

        std::vector<return_t> to_vector() {
            std::vector<return_t> records;
            for(auto& result: *this) {
                records.emplace_back(result);
            }
            return records;
        }
    };
};
