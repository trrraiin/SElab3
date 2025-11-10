#include <iostream>
#include <chrono>
#include <thread>
#include <ctime>
#include "models.h"
#include "repositories.h"
#include "services.h"

using namespace std::chrono;

std::string makeId(const std::string& prefix, int n) {
    return prefix + std::to_string(n);
}

int main(){
    TransactionRepository txnRepo;
    CategoryRepository catRepo;

    const std::string categoriesFile = "categories.csv";
    const std::string dataFile = "transactions.csv";
    // load saved categories first so we don't overwrite them with defaults
    catRepo.loadFromCsv(categoriesFile);

    CategorizerService categorizer(catRepo);
    BalanceService balanceSvc(txnRepo);
    TransactionService txnSvc(txnRepo, catRepo, categorizer, balanceSvc);
    ReportService reportSvc(txnRepo);
    SearchService searchSvc(txnRepo);

    // load previous transactions if present (now categories are available for resolution)
    txnRepo.loadFromCsv(dataFile, &catRepo);

    // interactive menu
    int txnCount = (int)txnRepo.findAll().size() + 1;
    while (true) {
        std::cout << "\nSimple Ledger Menu:\n";
        std::cout << "1) Add transaction\n";
        std::cout << "2) Show all transactions\n";
        std::cout << "3) Monthly summary (by year/month)\n";
        std::cout << "4) Yearly summary (by year)\n";
    std::cout << "5) All-time category summary\n";
    std::cout << "6) Show balance\n";
    std::cout << "7) Manage categories\n";
        std::cout << "0) Exit\n";
        std::cout << "Select option: ";
        std::string opt;
        std::getline(std::cin, opt);
        if (opt == "0") break;
        if (opt == "1") {
            // add one transaction
            std::string merchant;
            std::cout << "Merchant: "; std::getline(std::cin, merchant);
            std::string type;
            std::cout << "Type (i=income, e=expense): "; std::getline(std::cin, type);
            std::string amountStr;
            std::cout << "Amount: "; std::getline(std::cin, amountStr);
            double val = 0; try { val = std::stod(amountStr); } catch(...) { std::cout << "Invalid amount. Aborting add.\n"; continue; }
            std::string notes; std::cout << "Notes: "; std::getline(std::cin, notes);
            Transaction t; t.txnId = makeId("t", txnCount++);
            t.amount = (type == "e" || type == "E") ? -std::abs(val) : std::abs(val);
            t.date = std::chrono::system_clock::now(); t.merchant = merchant; t.notes = notes;
            txnSvc.addTransaction(t);
            txnRepo.saveToCsv(dataFile);
            std::cout << "Transaction added and saved!\n";
        } else if (opt == "2") {
            std::cout << "\nAll transactions:\n";
            for (auto &t: txnRepo.findAll()){
                auto tt = std::chrono::system_clock::to_time_t(t.date);
                std::cout << t.txnId << " " << t.merchant << " " << t.amount << " ";
                if (t.category) std::cout << "["<<t.category->name<<"]";
                else std::cout << "[Uncategorized]";
                std::cout << " notes=" << t.notes << "\n";
            }
        } else if (opt == "3") {
            std::string yearStr, monthStr;
            std::cout << "Year (e.g. 2025): "; std::getline(std::cin, yearStr);
            std::cout << "Month (1-12): "; std::getline(std::cin, monthStr);
            int y = 0, m = 0; try { y = std::stoi(yearStr); m = std::stoi(monthStr);} catch(...) { std::cout << "Invalid year/month.\n"; continue; }
            auto totals = reportSvc.incomeExpenseTotalsMonth(y,m);
            std::cout << "Income: " << totals.first << "  Expense: " << totals.second << "  Difference: " << (totals.first - totals.second) << "\n";
            reportSvc.printCategoryChart(y,m);
        } else if (opt == "4") {
            std::string yearStr; std::cout << "Year (e.g. 2025): "; std::getline(std::cin, yearStr);
            int y = 0; try { y = std::stoi(yearStr); } catch(...) { std::cout << "Invalid year.\n"; continue; }
            auto totals = reportSvc.incomeExpenseTotalsYear(y);
            std::cout << "Year "<< y << " Income: " << totals.first << "  Expense: " << totals.second << "  Difference: " << (totals.first - totals.second) << "\n";
            reportSvc.printCategorySummaryYear(y);
        } else if (opt == "5") {
            reportSvc.printCategorySummaryAll();
        } else if (opt == "7") {
            // categories management submenu
            std::cout << "\nCategory Management:\n";
            std::cout << "a) List categories\n";
            std::cout << "b) Add category\n";
            std::cout << "c) Delete category\n";
            std::cout << "Choose: ";
            std::string copt; std::getline(std::cin, copt);
            if (copt == "a") {
                std::cout << "Categories:\n";
                for (auto &c: catRepo.all()) {
                    std::cout << "- " << c->name << " (" << (c->type==CategoryType::Income?"Income":"Expense") << ")\n";
                }
            } else if (copt == "b") {
                std::string name; std::cout << "New category name: "; std::getline(std::cin, name);
                std::string t; std::cout << "Type (i=income, e=expense): "; std::getline(std::cin, t);
                Category c; c.categoryId = std::string("c_") + std::to_string(catRepo.all().size()+1);
                c.name = name; c.type = (t=="i"||t=="I")?CategoryType::Income:CategoryType::Expense;
                catRepo.save(c);
                catRepo.saveToCsv(categoriesFile);
                std::cout << "Category added.\n";
            } else if (copt == "c") {
                std::string name; std::cout << "Category name to delete: "; std::getline(std::cin, name);
                if (!catRepo.remove(name)) {
                    std::cout << "Category not found.\n";
                } else {
                    // clear references in transactions
                    txnRepo.clearCategory(name);
                    // persist both
                    catRepo.saveToCsv(categoriesFile);
                    txnRepo.saveToCsv(dataFile);
                    std::cout << "Category deleted; related transactions set to Uncategorized.\n";
                }
            } else {
                std::cout << "Unknown choice.\n";
            }
        } else if (opt == "6") {
            std::cout << "Balance: " << balanceSvc.calculateBalance() << "\n";
        } else {
            std::cout << "Unknown option" << "\n";
        }
    }

    std::cout << "\nAll transactions:\n";
    for (auto &t: txnRepo.findAll()){
        auto tt = std::chrono::system_clock::to_time_t(t.date);
        std::cout << t.txnId << " " << t.merchant << " " << t.amount << " ";
        if (t.category) std::cout << "["<<t.category->name<<"]";
        else std::cout << "[Uncategorized]";
        std::cout << " notes=" << t.notes << "\n";
    }

    // Print report for current month
    auto now = std::chrono::system_clock::now();
    time_t nowt = std::chrono::system_clock::to_time_t(now);
    tm local;
#ifdef _WIN32
    localtime_s(&local, &nowt);
#else
    localtime_r(&nowt, &local);
#endif
    int year = local.tm_year + 1900;
    int month = local.tm_mon + 1;

    std::cout << "\nGenerating monthly report...\n";
    reportSvc.printCategoryChart(year, month);

    std::cout << "\nBalance: " << balanceSvc.calculateBalance() << "\n";

    return 0;
}
