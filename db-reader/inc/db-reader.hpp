#ifndef DB_READER_HPP
#define DB_READER_HPP

#include <string>

class DbReader{
public:
    DbReader()=default;
    DbReader(std::string &dbPath) : m_dbPath(dbPath) {}
    ~DbReader()=default;
private:
    std::string m_dbPath{};
};
#endif // DB_READER_HPP
