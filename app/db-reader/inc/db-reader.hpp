#ifndef DB_READER_HPP
#define DB_READER_HPP

#include "j1939-db.hpp"
#include <memory>
#include <string>

class DbReader
{
public:
    DbReader() = default;

    DbReader(const std::string& xmlFilePath)
        : m_xmlFilePath(xmlFilePath)
    {
    }

    ~DbReader() = default;

    std::shared_ptr<J1939Database> ParseXmlAndStoreInContainer();

private:
    std::string m_xmlFilePath{};
};
#endif // DB_READER_HPP
