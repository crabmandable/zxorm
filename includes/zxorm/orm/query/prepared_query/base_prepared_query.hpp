#pragma once

namespace zxorm {
    class BasePreparedQuery {
    public:
        ~BasePreparedQuery() = default;
    protected:
        BasePreparedQuery() = default;

        BasePreparedQuery(BasePreparedQuery&& old) = default;
        BasePreparedQuery& operator=(BasePreparedQuery&& old) = default;

        BasePreparedQuery(const BasePreparedQuery& old) = delete;
        BasePreparedQuery& operator=(const BasePreparedQuery& old) = delete;
    };
};
