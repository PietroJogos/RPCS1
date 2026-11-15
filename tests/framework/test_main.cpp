// Executor dos testes unitários do RPCS1.
//
//   rpcs1_tests                 executa todos os testes
//   rpcs1_tests <filtro>...     executa testes cujo nome contém algum filtro
//   rpcs1_tests --list          lista os testes registrados
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "framework/rpcs1_test.hpp"

namespace rpcs1::test {

namespace {

int g_current_failures = 0;

std::string to_utf8(const std::filesystem::path& path) {
    const std::u8string text = path.u8string();
    return std::string(text.begin(), text.end());
}

} // namespace

std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

Registrar::Registrar(const char* name, TestFunction function, const char* file, int line) {
    registry().push_back(TestCase{name, function, file, line});
}

void report_failure(const char* file, int line, const std::string& message) {
    ++g_current_failures;
    std::printf("    %s:%d\n      %s\n", file, line, message.c_str());
}

std::string describe_path(const std::filesystem::path& path) {
    return "path(\"" + to_utf8(path) + "\")";
}

TempDir::TempDir() {
    static std::atomic<unsigned> counter{0};
    std::random_device rd;
    const auto base = std::filesystem::temp_directory_path() / "rpcs1_tests";
    for (int attempt = 0; attempt < 100; ++attempt) {
        auto candidate = base / ("t" + std::to_string(rd()) + "_" + std::to_string(counter++));
        std::error_code ec;
        if (std::filesystem::create_directories(candidate, ec)) {
            path_ = candidate;
            return;
        }
    }
    throw std::runtime_error("não foi possível criar pasta temporária de teste");
}

TempDir::~TempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
}

} // namespace rpcs1::test

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    using namespace rpcs1::test;

    std::vector<std::string> filters;
    bool list_only = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--list") {
            list_only = true;
        } else {
            filters.push_back(arg);
        }
    }

    auto& tests = registry();
    std::sort(tests.begin(), tests.end(),
              [](const TestCase& a, const TestCase& b) { return a.name < b.name; });

    if (list_only) {
        for (const auto& t : tests) {
            std::printf("%s\n", t.name.c_str());
        }
        return 0;
    }

    int passed = 0;
    int failed = 0;
    std::vector<std::string> failed_names;
    const auto start = std::chrono::steady_clock::now();

    for (const auto& t : tests) {
        if (!filters.empty() &&
            std::none_of(filters.begin(), filters.end(), [&](const std::string& f) {
                return t.name.find(f) != std::string::npos;
            })) {
            continue;
        }
        g_current_failures = 0;
        try {
            t.function();
        } catch (const RequireFailure&) {
            // falha já registrada
        } catch (const std::exception& e) {
            report_failure(t.file, t.line, std::string("exceção não tratada: ") + e.what());
        } catch (...) {
            report_failure(t.file, t.line, "exceção desconhecida");
        }
        if (g_current_failures == 0) {
            ++passed;
            std::printf("[ OK    ] %s\n", t.name.c_str());
        } else {
            ++failed;
            failed_names.push_back(t.name);
            std::printf("[ FALHA ] %s (%d verificação(ões))\n", t.name.c_str(),
                        g_current_failures);
        }
    }

    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start)
                        .count();
    std::printf("\n%d teste(s) executado(s): %d passaram, %d falharam (%lld ms)\n",
                passed + failed, passed, failed, static_cast<long long>(ms));
    for (const auto& name : failed_names) {
        std::printf("  FALHOU: %s\n", name.c_str());
    }
    if (passed + failed == 0) {
        std::printf("Nenhum teste corresponde aos filtros.\n");
        return 2;
    }
    return failed == 0 ? 0 : 1;
}
