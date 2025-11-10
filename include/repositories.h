#pragma once
#include "models.h"
#include <vector>
#include <map>
#include <optional>
#include <string>
#include <algorithm>
#include <memory>
#include <fstream>
#include <sstream>

// small CSV helpers used by both CategoryRepository and TransactionRepository
static std::string escapeCsv(const std::string& s) {
    std::string out;
    for (char c: s) {
        if (c == '"') out += "\"\""; // double the quote
        else out += c;
    }
    return out;
}

static std::string unescapeCsv(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '"' && i + 1 < s.size() && s[i+1] == '"') { out += '"'; ++i; }
        else out += s[i];
    }
    return out;
}

// Very small CSV parser: splits on commas, trims surrounding quotes
static std::vector<std::string> splitCsvLine(const std::string& line) {
    std::vector<std::string> parts;
    std::istringstream ss(line);
    std::string token;
    while (std::getline(ss, token, ',')) {
        if (!token.empty() && token.front() == '"') token.erase(token.begin());
        if (!token.empty() && token.back() == '"') token.pop_back();
        parts.push_back(token);
    }
    return parts;
}

class CategoryRepository {
public:
    std::shared_ptr<Category> findByName(const std::string& name) {
        auto it = byName.find(name);
        if (it!=byName.end()) return it->second;
        return nullptr;
    }

    std::shared_ptr<Category> save(const Category& c) {
        auto ptr = std::make_shared<Category>(c);
        byName[c.name] = ptr;
        return ptr;
    }

    std::vector<std::shared_ptr<Category>> all() const {
        std::vector<std::shared_ptr<Category>> out;
        for (auto &p: byName) out.push_back(p.second);
        return out;
    }

    bool remove(const std::string& name) {
        auto it = byName.find(name);
        if (it==byName.end()) return false;
        byName.erase(it);
        return true;
    }

    // Persist categories to CSV: categoryId,name,type
    void saveToCsv(const std::string& path) const {
        std::ofstream ofs(path, std::ios::trunc);
        if (!ofs) return;
        for (auto &p: byName) {
            auto &c = *p.second;
            int t = static_cast<int>(c.type);
            ofs << '"' << escapeCsv(c.categoryId) << '"' << ','
                << '"' << escapeCsv(c.name) << '"' << ','
                << t << '\n';
        }
    }

    void loadFromCsv(const std::string& path) {
        std::ifstream ifs(path);
        if (!ifs) return;
        std::string line;
        while (std::getline(ifs, line)) {
            if (line.empty()) continue;
            std::vector<std::string> parts = splitCsvLine(line);
            if (parts.size() < 3) continue;
            Category c;
            c.categoryId = unescapeCsv(parts[0]);
            c.name = unescapeCsv(parts[1]);
            int ti = 0; try { ti = std::stoi(parts[2]); } catch(...) { ti = 0; }
            c.type = (ti==1) ? CategoryType::Income : CategoryType::Expense;
            save(c);
        }
    }

private:
    std::map<std::string, std::shared_ptr<Category>> byName;
};

class TransactionRepository {
public:
    void save(const Transaction& t) {
        txns.push_back(t);
    }

    // Clear category references for transactions that referenced this category name
    void clearCategory(const std::string& categoryName) {
        for (auto &t: txns) {
            if (t.category && t.category->name == categoryName) t.category = nullptr;
        }
    }

    std::vector<Transaction> findAll() const { return txns; }

    std::vector<Transaction> findByUserAndMonth(const std::string& /*userId*/, int year, int month) const {
        std::vector<Transaction> out;
        for (auto &t: txns) {
            auto tt = std::chrono::system_clock::to_time_t(t.date);
            tm local;
#ifdef _WIN32
            localtime_s(&local, &tt);
#else
            localtime_r(&tt, &local);
#endif
            if (local.tm_year + 1900 == year && (local.tm_mon+1) == month) out.push_back(t);
        }
        return out;
    }

    std::vector<Transaction> findByCategory(const std::string& categoryName) const {
        std::vector<Transaction> out;
        for (auto &t: txns) {
            if (t.category && t.category->name == categoryName) out.push_back(t);
        }
        return out;
    }

    std::vector<Transaction> searchByKeyword(const std::string& kw) const {
        std::vector<Transaction> out;
        for (auto &t: txns) {
            if (t.notes.find(kw) != std::string::npos || t.merchant.find(kw) != std::string::npos) out.push_back(t);
        }
        return out;
    }

    // Persist all transactions into a CSV file. Fields: txnId,amount,epoch,merchant,categoryName,notes
    void saveToCsv(const std::string& path) const {
        std::ofstream ofs(path, std::ios::trunc);
        if (!ofs) return;
        for (auto &t: txns) {
            // epoch seconds
            auto tt = std::chrono::system_clock::to_time_t(t.date);
            std::string categoryName = t.category ? t.category->name : "";
            ofs << '"' << escapeCsv(t.txnId) << '"' << ','
                << t.amount << ',' << tt << ','
                << '"' << escapeCsv(t.merchant) << '"' << ','
                << '"' << escapeCsv(categoryName) << '"' << ','
                << '"' << escapeCsv(t.notes) << '"' << '\n';
        }
    }

    // Load transactions from CSV. If catRepo != nullptr, try to resolve category names.
    void loadFromCsv(const std::string& path, CategoryRepository* catRepo=nullptr) {
        std::ifstream ifs(path);
        if (!ifs) return;
        std::string line;
        while (std::getline(ifs, line)) {
            if (line.empty()) continue;
            std::vector<std::string> parts = splitCsvLine(line);
            if (parts.size() < 6) continue;
            Transaction t;
            t.txnId = unescapeCsv(parts[0]);
            try { t.amount = std::stod(parts[1]); } catch(...) { t.amount = 0; }
            try { std::time_t tt = static_cast<std::time_t>(std::stoll(parts[2])); t.date = std::chrono::system_clock::from_time_t(tt); } catch(...) { t.date = std::chrono::system_clock::now(); }
            t.merchant = unescapeCsv(parts[3]);
            std::string catName = unescapeCsv(parts[4]);
            if (catRepo && !catName.empty()) {
                auto c = catRepo->findByName(catName);
                if (c) t.category = c;
            }
            t.notes = unescapeCsv(parts[5]);
            txns.push_back(t);
        }
    }

private:
    std::vector<Transaction> txns;

    // CSV helpers moved to file-scope above
};

