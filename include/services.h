#pragma once
#include "models.h"
#include "repositories.h"
#include <string>
#include <map>
#include <iostream>
#include <iomanip>
#include <sstream>

// Simple keyword-based categorizer - stub for ML
class CategorizerService {
public:
    CategorizerService(CategoryRepository& repo): catRepo(repo) {
    // seed some default categories (only add if they do not already exist)
    if (!catRepo.findByName("Food")) { Category food{"c_food","Food",CategoryType::Expense}; catRepo.save(food); }
    if (!catRepo.findByName("Transport")) { Category transport{"c_trans","Transport",CategoryType::Expense}; catRepo.save(transport); }
    if (!catRepo.findByName("Salary")) { Category salary{"c_salary","Salary",CategoryType::Income}; catRepo.save(salary); }
    // simple keyword map (ASCII keywords)
    keywordToCategory["eat"] = "Food";
    keywordToCategory["meal"] = "Food";
    keywordToCategory["lunch"] = "Food";
    keywordToCategory["subway"] = "Transport";
    keywordToCategory["bus"] = "Transport";
    keywordToCategory["salary"] = "Salary";
    }

    // returns pair<categoryPtr, confidence(0..1)>
    std::pair<std::shared_ptr<Category>, double> autoCategorize(const Transaction& t) {
        for (auto &kv: keywordToCategory) {
            if (t.merchant.find(kv.first) != std::string::npos || t.notes.find(kv.first) != std::string::npos) {
                auto c = catRepo.findByName(kv.second);
                if (c) return {c, 0.95};
            }
        }
        return {nullptr, 0.0};
    }

private:
    CategoryRepository& catRepo;
    std::map<std::string,std::string> keywordToCategory;
};

class BalanceService {
public:
    BalanceService(TransactionRepository& r): repo(r) {}
    Decimal calculateBalance() const {
        Decimal s = 0;
        for (auto &t: repo.findAll()) s += t.amount;
        return s;
    }
private:
    TransactionRepository& repo;
};

class ReportService {
public:
    ReportService(TransactionRepository& r): repo(r) {}

    // simple category breakdown for given year/month
    std::map<std::string, Decimal> categoryBreakdown(int year, int month) {
        std::map<std::string, Decimal> out;
        auto txns = repo.findByUserAndMonth("", year, month);
        for (auto &t: txns) {
            std::string name = t.category ? t.category->name : "Uncategorized";
            out[name] += t.amount < 0 ? -t.amount : t.amount;
        }
        return out;
    }

    void printCategoryChart(int year, int month) {
        auto map = categoryBreakdown(year, month);
        Decimal total = 0; for (auto &kv: map) total += kv.second;
        std::cout << "Category breakdown for " << year << "-" << std::setw(2) << std::setfill('0') << month << '\n';
        for (auto &kv: map) {
            int pct = total>0 ? int((kv.second/total)*100) : 0;
            int bars = pct/2;
            std::cout << std::setw(12) << std::left << kv.first << " ";
            for (int i=0;i<bars;i++) std::cout<<"#";
            std::cout << " " << kv.second << " (" << pct << "% )\n";
        }
        std::cout << "Total: " << total << '\n';
    }

    // income and expense totals for a specific month (year, month)
    std::pair<Decimal, Decimal> incomeExpenseTotalsMonth(int year, int month) {
        Decimal income = 0, expense = 0;
        auto txns = repo.findByUserAndMonth("", year, month);
        for (auto &t: txns) {
            if (t.amount >= 0) income += t.amount;
            else expense += -t.amount;
        }
        return {income, expense};
    }

    // income and expense totals for a specific year
    std::pair<Decimal, Decimal> incomeExpenseTotalsYear(int year) {
        Decimal income = 0, expense = 0;
        for (auto &t: repo.findAll()) {
            auto tt = std::chrono::system_clock::to_time_t(t.date);
            tm local;
#ifdef _WIN32
            localtime_s(&local, &tt);
#else
            localtime_r(&tt, &local);
#endif
            if (local.tm_year + 1900 == year) {
                if (t.amount >= 0) income += t.amount;
                else expense += -t.amount;
            }
        }
        return {income, expense};
    }

    // category breakdown for a full year
    std::map<std::string, Decimal> categoryBreakdownYear(int year) {
        std::map<std::string, Decimal> out;
        for (auto &t: repo.findAll()) {
            auto tt = std::chrono::system_clock::to_time_t(t.date);
            tm local;
#ifdef _WIN32
            localtime_s(&local, &tt);
#else
            localtime_r(&tt, &local);
#endif
            if (local.tm_year + 1900 == year) {
                std::string name = t.category ? t.category->name : "Uncategorized";
                out[name] += t.amount < 0 ? -t.amount : t.amount;
            }
        }
        return out;
    }

    // category breakdown for all time
    std::map<std::string, Decimal> categoryBreakdownAll() {
        std::map<std::string, Decimal> out;
        for (auto &t: repo.findAll()) {
            std::string name = t.category ? t.category->name : "Uncategorized";
            out[name] += t.amount < 0 ? -t.amount : t.amount;
        }
        return out;
    }

    void printCategorySummaryYear(int year) {
        auto map = categoryBreakdownYear(year);
        Decimal total = 0; for (auto &kv: map) total += kv.second;
        std::cout << "Category breakdown for year " << year << '\n';
        for (auto &kv: map) std::cout << std::setw(12) << std::left << kv.first << " " << kv.second << '\n';
        std::cout << "Total: " << total << '\n';
    }

    void printCategorySummaryAll() {
        auto map = categoryBreakdownAll();
        Decimal total = 0; for (auto &kv: map) total += kv.second;
        std::cout << "Category breakdown (all time):\n";
        for (auto &kv: map) std::cout << std::setw(12) << std::left << kv.first << " " << kv.second << '\n';
        std::cout << "Total: " << total << '\n';
    }

private:
    TransactionRepository& repo;
};

class TransactionService {
public:
    TransactionService(TransactionRepository& r, CategoryRepository& cr, CategorizerService& cat, BalanceService& b)
    : repo(r), catRepo(cr), categorizer(cat), balanceSvc(b) {}

    // import and auto-categorize
    void importTransactions(std::vector<Transaction> txns, double confidenceThreshold=0.8) {
        for (auto &t : txns) {
            auto res = categorizer.autoCategorize(t);
            Transaction copy = t;
            if (res.first && res.second>=confidenceThreshold) {
                copy.category = res.first;
            } else {
                // leave uncategorized (needs review) for demo
            }
            repo.save(copy);
        }
    }

    void addTransaction(const Transaction& t) {
        repo.save(t);
    }

private:
    TransactionRepository& repo;
    CategoryRepository& catRepo;
    CategorizerService& categorizer;
    BalanceService& balanceSvc;
};

class SearchService {
public:
    SearchService(TransactionRepository& r): repo(r) {}
    std::vector<Transaction> searchByCategory(const std::string& cat) { return repo.findByCategory(cat); }
    std::vector<Transaction> searchByKeyword(const std::string& kw) { return repo.searchByKeyword(kw); }
private:
    TransactionRepository& repo;
};
