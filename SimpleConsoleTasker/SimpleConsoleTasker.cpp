#include <iostream>
#include <vector>
#include <string>
#include <cstdlib> 
#include "sqlite3.h"

#ifdef _WIN32
#include <windows.h>
#pragma execution_character_set("utf-8")
#endif

void clearScreen() {
#ifdef _WIN32
    std::system("cls");
#else
    std::system("clear");
#endif
}

const std::string RESET = "\033[0m";
const std::string RED = "\033[31m";
const std::string GREEN = "\033[32m";
const std::string YELLOW = "\033[33m";
const std::string BLUE = "\033[34m";
const std::string MAGENTA = "\033[35m";
const std::string CYAN = "\033[36m";

sqlite3* db = nullptr;

bool initDatabase() {
    int rc = sqlite3_open("tasks.db", &db);
    if (rc != SQLITE_OK) {
        std::cerr << RED << "Помилка відкриття БД: " << sqlite3_errmsg(db) << RESET << "\n";
        return false;
    }

    const char* createTableSQL =
        "CREATE TABLE IF NOT EXISTS tasks ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "title TEXT NOT NULL,"
        "description TEXT,"
        "status INTEGER NOT NULL DEFAULT 0);";

    char* errMsg = nullptr;
    rc = sqlite3_exec(db, createTableSQL, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << RED << "Помилка створення таблиці: " << errMsg << RESET << "\n";
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

std::string getStatusString(int status) {
    switch (status) {
    case 0:  return RED + "[Не виконано]" + RESET;
    case 1:  return YELLOW + "[У процесі]" + RESET;
    case 2:  return GREEN + "[Виконано]" + RESET;
    default: return "[Невідомо]";
    }
}

void displayTasks() {
    std::cout << CYAN << "=== Список завдань ===" << RESET << "\n";

    const char* selectSQL = "SELECT id, title, description, status FROM tasks;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, selectSQL, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cout << RED << "Помилка підготовки запиту відображення." << RESET << "\n";
        return;
    }

    bool hasTasks = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        hasTasks = true;
        int id = sqlite3_column_int(stmt, 0);
        const unsigned char* title = sqlite3_column_text(stmt, 1);
        const unsigned char* desc = sqlite3_column_text(stmt, 2);
        int status = sqlite3_column_int(stmt, 3);

        std::string descStr = desc ? reinterpret_cast<const char*>(desc) : "";

        std::cout << MAGENTA << "#" << id << RESET << " | "
            << getStatusString(status) << " | "
            << BLUE << title << RESET << "\n";
        if (!descStr.empty()) {
            std::cout << "    Опис: " << descStr << "\n";
        }
    }

    if (!hasTasks) {
        std::cout << "Завдань поки немає. Саме час додати перше!\n";
    }

    sqlite3_finalize(stmt);
    std::cout << "--------------------\n\n";
}

void addTask() {
    std::string title, description;

    std::cin.ignore(10000, '\n');
    std::cout << YELLOW << "Введіть назву завдання: " << RESET;
    std::getline(std::cin, title);

    std::cout << YELLOW << "Введіть опис (або натисніть Enter, щоб пропустити): " << RESET;
    std::getline(std::cin, description);

    const char* insertSQL = "INSERT INTO tasks (title, description, status) VALUES (?, ?, 0);";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, insertSQL, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_TRANSIENT);
        if (description.empty()) {
            sqlite3_bind_null(stmt, 2);
        }
        else {
            sqlite3_bind_text(stmt, 2, description.c_str(), -1, SQLITE_TRANSIENT);
        }

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            std::cout << GREEN << "Завдання успішно збережено в базу даних!\n" << RESET;
        }
        else {
            std::cout << RED << "Помилка збереження завдання.\n" << RESET;
        }
    }
    else {
        std::cout << RED << "Помилка підготовки запиту.\n" << RESET;
    }
    sqlite3_finalize(stmt);
}

void changeTaskStatus() {
    int id;
    std::cout << YELLOW << "Введіть ID завдання для зміни статусу: " << RESET;
    if (!(std::cin >> id)) {
        std::cin.clear();
        std::cin.ignore(10000, '\n');
        std::cout << RED << "Некоректний формат ID.\n" << RESET;
        return;
    }

    const char* checkSQL = "SELECT status FROM tasks WHERE id = ?;";
    sqlite3_stmt* checkStmt;
    int currentStatus = -1;

    if (sqlite3_prepare_v2(db, checkSQL, -1, &checkStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(checkStmt, 1, id);
        if (sqlite3_step(checkStmt) == SQLITE_ROW) {
            currentStatus = sqlite3_column_int(checkStmt, 0);
        }
    }
    sqlite3_finalize(checkStmt);

    if (currentStatus == -1) {
        std::cout << RED << "Завдання з ID #" << id << " не знайдено.\n" << RESET;
        return;
    }

    std::cout << "Поточний статус завдання: " << getStatusString(currentStatus) << "\n";
    std::cout << "Оберіть новий статус:\n";
    std::cout << "0. " << RED << "Не виконано" << RESET << "\n";
    std::cout << "1. " << YELLOW << "У процесі" << RESET << "\n";
    std::cout << "2. " << GREEN << "Виконано" << RESET << "\n";
    std::cout << "Вибір: ";

    int newStatus;
    if (!(std::cin >> newStatus) || newStatus < 0 || newStatus > 2) {
        std::cin.clear();
        std::cin.ignore(10000, '\n');
        std::cout << RED << "Неправильний статус.\n" << RESET;
        return;
    }

    const char* updateSQL = "UPDATE tasks SET status = ? WHERE id = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, updateSQL, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, newStatus);
        sqlite3_bind_int(stmt, 2, id);

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            std::cout << GREEN << "Статус завдання оновлено!\n" << RESET;
        }
        else {
            std::cout << RED << "Помилка оновлення статусу в БД.\n" << RESET;
        }
    }
    sqlite3_finalize(stmt);
}

void deleteTask() {
    int id;
    std::cout << RED << "Введіть ID завдання для ВИДАЛЕННЯ: " << RESET;
    if (!(std::cin >> id)) {
        std::cin.clear();
        std::cin.ignore(10000, '\n');
        std::cout << RED << "Некоректний формат ID.\n" << RESET;
        return;
    }

    const char* deleteSQL = "DELETE FROM tasks WHERE id = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, deleteSQL, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            if (sqlite3_changes(db) > 0) {
                std::cout << GREEN << "Завдання успішно видалено з бази даних!\n" << RESET;
            }
            else {
                std::cout << YELLOW << "Завдання з ID #" << id << " не знайдено.\n" << RESET;
            }
        }
        else {
            std::cout << RED << "Помилка під час виконання видалення.\n" << RESET;
        }
    }
    sqlite3_finalize(stmt);
}

void editTask() {
    int id;
    std::cout << YELLOW << "Введіть ID завдання для РЕДАГУВАННЯ: " << RESET;
    if (!(std::cin >> id)) {
        std::cin.clear();
        std::cin.ignore(10000, '\n');
        std::cout << RED << "Некоректний формат ID.\n" << RESET;
        return;
    }

    const char* checkSQL = "SELECT id FROM tasks WHERE id = ?;";
    sqlite3_stmt* checkStmt;
    bool exists = false;

    if (sqlite3_prepare_v2(db, checkSQL, -1, &checkStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(checkStmt, 1, id);
        if (sqlite3_step(checkStmt) == SQLITE_ROW) {
            exists = true;
        }
    }
    sqlite3_finalize(checkStmt);

    if (!exists) {
        std::cout << RED << "Завдання з ID #" << id << " не знайдено.\n" << RESET;
        return;
    }

    std::cin.ignore(10000, '\n');

    std::string newTitle, newDescription;

    std::cout << YELLOW << "Введіть нову назву завдання: " << RESET;
    std::getline(std::cin, newTitle);

    std::cout << YELLOW << "Введіть новий опис (або натисніть Enter, щоб залишити порожнім): " << RESET;
    std::getline(std::cin, newDescription);

    const char* updateSQL = "UPDATE tasks SET title = ?, description = ? WHERE id = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, updateSQL, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, newTitle.c_str(), -1, SQLITE_TRANSIENT);

        if (newDescription.empty()) {
            sqlite3_bind_null(stmt, 2);
        }
        else {
            sqlite3_bind_text(stmt, 2, newDescription.c_str(), -1, SQLITE_TRANSIENT);
        }

        sqlite3_bind_int(stmt, 3, id);

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            std::cout << GREEN << "Завдання успішно відредаговано!\n" << RESET;
        }
        else {
            std::cout << RED << "Помилка збереження змін у БД.\n" << RESET;
        }
    }
    sqlite3_finalize(stmt);
}

void printMenu() {
    std::cout << CYAN << "=== Консольний Таск-Менеджер ===" << RESET << "\n";
    std::cout << "1. " << GREEN << "Додати завдання" << RESET << "\n";
    std::cout << "2. " << YELLOW << "Редагувати завдання" << RESET << "\n";
    std::cout << "3. " << RED << "Видалити завдання" << RESET << "\n";
    std::cout << "4. " << BLUE << "Змінити статус завдання" << RESET << "\n";
    std::cout << "0. Вихід\n";
    std::cout << "Оберіть дію: ";
}

int main() {
#ifdef _WIN32
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif

    if (!initDatabase()) {
        std::cout << RED << "Критична помилка ініціалізації бази даних. Вихід.\n" << RESET;
        return 1;
    }

    int choice;
    bool running = true;

    while (running) {
        clearScreen();
        displayTasks();
        printMenu();

        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            continue;
        }

        switch (choice) {
        case 1: addTask(); break;
        case 2: editTask(); break;
        case 3: deleteTask(); break;
        case 4: changeTaskStatus(); break;
        case 0:
            running = false;
            std::cout << GREEN << "Завершення роботи. На все добре!\n" << RESET;
            break;
        default:
            std::cout << RED << "Невірний вибір. Спробуйте ще раз.\n" << RESET;
            break;
        }

        if (running) {
            std::cout << "\nНатисніть Enter, щоб продовжити...";
            std::cin.ignore(10000, '\n');
            std::cin.get();
        }
    }

    if (db) {
        sqlite3_close(db);
    }

    return 0;
}