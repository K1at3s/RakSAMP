#ifndef KILL_LIST_FORMAT_H
#define KILL_LIST_FORMAT_H
#include <string>
#include <sstream>
#include <iomanip>

inline std::string BuildKillListBox(const std::string &rows, int kills, int assists, int deaths, int damage) {
    const char *dash = "--------------------------------------";
    std::ostringstream out;
    out << std::left << std::setw(16) << "Name" << std::right
        << ' ' << std::setw(4) << "K" << ' ' << std::setw(4) << "A"
        << ' ' << std::setw(4) << "D" << ' ' << std::setw(6) << "DMG"
        << '\n' << dash << '\n' << (rows.empty() ? "(bos)" : rows) << '\n' << dash << '\n'
        << std::left << std::setw(16) << "Total" << std::right
        << ' ' << std::setw(4) << kills << ' ' << std::setw(4) << assists
        << ' ' << std::setw(4) << deaths << ' ' << std::setw(6) << damage;
    return out.str();
}
#endif
