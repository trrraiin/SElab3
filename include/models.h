#pragma once
#include <string>
#include <vector>
#include <memory>
#include <chrono>

using Decimal = double;

struct User {
    std::string userId;
    std::string name;
};

struct Account {
    std::string accountId;
    std::string name;
    Decimal balance{0.0};
    void adjustBalance(Decimal amount) { balance += amount; }
};

enum class CategoryType { Expense, Income };

struct Category {
    std::string categoryId;
    std::string name;
    CategoryType type{CategoryType::Expense};
};

struct Transaction {
    std::string txnId;
    Decimal amount{0.0};
    std::chrono::system_clock::time_point date;
    std::string merchant;
    std::shared_ptr<Category> category; // may be nullptr before categorization
    std::string notes;
    bool isIncome() const { return category ? category->type==CategoryType::Income : amount>0; }
};
