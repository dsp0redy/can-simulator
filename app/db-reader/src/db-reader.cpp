#include "db-reader.hpp"

#include <filesystem>
#include <fstream>
#include <regex>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace
{
using AttributeMap = std::unordered_map<std::string, std::string>;

std::string ReadFileToString(const std::string& path)
{
    std::ifstream input(path);
    if (!input.is_open())
        throw std::runtime_error("Error opening XML file: " + path);

    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::string ToLower(std::string value)
{
    for (char& c : value)
    {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    return value;
}

uint64_t ParseUnsigned(const std::string& value)
{
    size_t parsedChars = 0;
    const uint64_t parsed = std::stoull(value, &parsedChars, 0);
    if (parsedChars != value.size())
        throw std::runtime_error("Invalid unsigned integer value: " + value);
    return parsed;
}

double ParseDouble(const std::string& value)
{
    size_t parsedChars = 0;
    const double parsed = std::stod(value, &parsedChars);
    if (parsedChars != value.size())
        throw std::runtime_error("Invalid floating-point value: " + value);
    return parsed;
}

AttributeMap ParseAttributes(const std::string& attrsText)
{
    AttributeMap attrs;
    const std::regex attrRegex(R"ATTR((\w+)\s*=\s*"([^"]*)")ATTR", std::regex::icase);
    for (std::sregex_iterator it(attrsText.begin(), attrsText.end(), attrRegex), end; it != end; ++it)
        attrs[ToLower((*it)[1].str())] = (*it)[2].str();

    return attrs;
}

const std::string& RequireAttribute(const AttributeMap& attrs, const std::string& name)
{
    const auto it = attrs.find(name);
    if (it == attrs.end())
        throw std::runtime_error("Missing required attribute: " + name);
    return it->second;
}

Endianness ParseEndianness(const std::string& value)
{
    const std::string lowered = ToLower(value);
    if (lowered == "little" || lowered == "littleendian")
        return Endianness::LittleEndian;
    if (lowered == "big" || lowered == "bigendian")
        return Endianness::BigEndian;
    throw std::runtime_error("Invalid endianness value: " + value);
}

Signedness ParseSignedness(const std::string& value)
{
    const std::string lowered = ToLower(value);
    if (lowered == "unsigned")
        return Signedness::Unsigned;
    if (lowered == "signed")
        return Signedness::Signed;
    throw std::runtime_error("Invalid signedness value: " + value);
}

PgnData ParsePgn(const AttributeMap& attrs)
{
    PgnData pgn{};
    pgn.priority = static_cast<uint8_t>(ParseUnsigned(RequireAttribute(attrs, "priority")));
    pgn.dlc = static_cast<uint8_t>(ParseUnsigned(RequireAttribute(attrs, "dlc")));
    pgn.source_address = static_cast<uint8_t>(ParseUnsigned(RequireAttribute(attrs, "source_address")));

    const auto cycleIt = attrs.find("cycle_ms");
    if (cycleIt != attrs.end())
        pgn.cycle_time_ms = static_cast<uint32_t>(ParseUnsigned(cycleIt->second));

    return pgn;
}

void AddSpn(J1939Database& db, const AttributeMap& attrs, uint32_t pgnFromContainer)
{
    uint32_t pgn = pgnFromContainer;
    const auto pgnIt = attrs.find("pgn");
    if (pgnIt != attrs.end())
        pgn = static_cast<uint32_t>(ParseUnsigned(pgnIt->second));

    auto dbIt = db.find(pgn);
    if (dbIt == db.end())
        throw std::runtime_error("SPN references PGN not defined: " + std::to_string(pgn));

    const uint32_t spn = static_cast<uint32_t>(ParseUnsigned(RequireAttribute(attrs, "id")));

    SignalConfig signal{};
    signal.start_bit = static_cast<uint16_t>(ParseUnsigned(RequireAttribute(attrs, "start_bit")));
    signal.bit_size = static_cast<uint16_t>(ParseUnsigned(RequireAttribute(attrs, "bit_size")));
    signal.scale = ParseDouble(RequireAttribute(attrs, "scale"));
    signal.offset = ParseDouble(RequireAttribute(attrs, "offset"));
    signal.min = ParseDouble(RequireAttribute(attrs, "min"));
    signal.max = ParseDouble(RequireAttribute(attrs, "max"));
    signal.endianness = ParseEndianness(RequireAttribute(attrs, "endianness"));
    signal.signedness = ParseSignedness(RequireAttribute(attrs, "signedness"));

    dbIt->second.signals[spn] = signal;
}

void ParseNestedPgnBlocks(const std::string& xml, J1939Database& db)
{
    const std::regex pgnBlockRegex(R"(<\s*pgn\b([^>]*)>([\s\S]*?)<\s*/\s*pgn\s*>)", std::regex::icase);
    const std::regex spnRegex(R"(<\s*spn\b([^>]*)/\s*>)", std::regex::icase);

    for (std::sregex_iterator it(xml.begin(), xml.end(), pgnBlockRegex), end; it != end; ++it)
    {
        const AttributeMap pgnAttrs = ParseAttributes((*it)[1].str());
        const uint32_t pgn = static_cast<uint32_t>(ParseUnsigned(RequireAttribute(pgnAttrs, "id")));
        db[pgn] = ParsePgn(pgnAttrs);

        const std::string spnBody = (*it)[2].str();
        for (std::sregex_iterator spnIt(spnBody.begin(), spnBody.end(), spnRegex), spnEnd; spnIt != spnEnd; ++spnIt)
        {
            const AttributeMap spnAttrs = ParseAttributes((*spnIt)[1].str());
            AddSpn(db, spnAttrs, pgn);
        }
    }
}

void ParseSelfClosingPgnTags(const std::string& xml, J1939Database& db)
{
    const std::regex pgnRegex(R"(<\s*pgn\b([^>]*)/\s*>)", std::regex::icase);

    for (std::sregex_iterator it(xml.begin(), xml.end(), pgnRegex), end; it != end; ++it)
    {
        const AttributeMap pgnAttrs = ParseAttributes((*it)[1].str());
        if (pgnAttrs.find("id") == pgnAttrs.end())
            continue;

        const uint32_t pgn = static_cast<uint32_t>(ParseUnsigned(RequireAttribute(pgnAttrs, "id")));
        if (db.find(pgn) == db.end())
            db[pgn] = ParsePgn(pgnAttrs);
    }
}

void ParseStandaloneSpnTags(const std::string& xml, J1939Database& db)
{
    const std::regex spnRegex(R"(<\s*spn\b([^>]*)/\s*>)", std::regex::icase);

    for (std::sregex_iterator it(xml.begin(), xml.end(), spnRegex), end; it != end; ++it)
    {
        const AttributeMap spnAttrs = ParseAttributes((*it)[1].str());
        if (spnAttrs.find("pgn") == spnAttrs.end())
            continue;
        AddSpn(db, spnAttrs, 0);
    }

    if (db.empty())
        throw std::runtime_error("No PGN/SPN entries found in XML");
}

void ValidateDatabase(const J1939Database& db)
{
    for (const auto& [pgn, data] : db)
    {
        if (data.dlc == 0)
            throw std::runtime_error("PGN has invalid DLC=0: " + std::to_string(pgn));
    }
}
} // namespace

std::shared_ptr<J1939Database> DbReader::ParseXmlAndStoreInContainer()
{
    auto db = std::make_shared<J1939Database>();

    if (!std::filesystem::exists(m_xmlFilePath))
        throw std::runtime_error("Error in XML file path");

    const std::string xml = ReadFileToString(m_xmlFilePath);

    // Nested <pgn>...</pgn> regex parsing can be unstable on very large files.
    if (xml.size() < (2U * 1024U * 1024U) && xml.find("</pgn") != std::string::npos)
        ParseNestedPgnBlocks(xml, *db);
    ParseSelfClosingPgnTags(xml, *db);
    ParseStandaloneSpnTags(xml, *db);
    ValidateDatabase(*db);

    return db;
}
