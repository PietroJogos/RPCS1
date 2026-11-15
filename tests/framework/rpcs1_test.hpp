// Framework de testes mínimo do RPCS1 (sem dependências externas).
//
// Uso:
//   TEST_CASE("modulo/descricao") {
//       CHECK(expr);             // registra falha e continua
//       REQUIRE(expr);           // registra falha e interrompe o caso
//       CHECK_EQ(a, b);          // mostra os dois valores em caso de falha
//       CHECK_THROWS(expr);
//   }
//
// O executável aceita filtros por substring do nome e a opção --list.
#pragma once

#include <concepts>
#include <exception>
#include <filesystem>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace rpcs1::test {

using TestFunction = void (*)();

struct TestCase {
    std::string name;
    TestFunction function;
    const char* file;
    int line;
};

std::vector<TestCase>& registry();

struct Registrar {
    Registrar(const char* name, TestFunction function, const char* file, int line);
};

// Lançada por REQUIRE para abortar o caso atual.
struct RequireFailure {};

void report_failure(const char* file, int line, const std::string& message);

template <typename T>
concept Streamable = requires(std::ostream& os, const T& value) { os << value; };

template <typename T>
struct IsOptional : std::false_type {};
template <typename T>
struct IsOptional<std::optional<T>> : std::true_type {};

std::string describe_path(const std::filesystem::path& path);

template <typename T>
std::string describe(const T& value) {
    if constexpr (IsOptional<T>::value) {
        return value ? "optional(" + describe(*value) + ")" : std::string("nullopt");
    } else if constexpr (std::is_same_v<T, bool>) {
        return value ? "true" : "false";
    } else if constexpr (std::is_enum_v<T>) {
        return "enum(" + std::to_string(static_cast<long long>(value)) + ")";
    } else if constexpr (std::is_integral_v<T>) {
        std::ostringstream os;
        if constexpr (std::is_same_v<T, char> || std::is_same_v<T, signed char> ||
                      std::is_same_v<T, unsigned char>) {
            os << static_cast<int>(value);
        } else {
            os << +value;
        }
        os << " (0x" << std::hex << static_cast<unsigned long long>(value) << ")";
        return os.str();
    } else if constexpr (std::is_convertible_v<T, std::string_view>) {
        return "\"" + std::string(std::string_view(value)) + "\"";
    } else if constexpr (std::is_same_v<T, std::filesystem::path>) {
        return describe_path(value);
    } else if constexpr (Streamable<T>) {
        std::ostringstream os;
        os << value;
        return os.str();
    } else {
        return "{valor não imprimível}";
    }
}

// Comparação fora de expressão constante (evita o aviso C4127 com operandos constexpr).
template <typename A, typename B>
bool equals(const A& a, const B& b) {
    return a == b;
}

// Pasta temporária exclusiva, removida ao sair do escopo.
class TempDir {
public:
    TempDir();
    ~TempDir();
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

} // namespace rpcs1::test

#define RPCS1_TEST_CAT_(a, b) a##b
#define RPCS1_TEST_CAT(a, b) RPCS1_TEST_CAT_(a, b)

#define RPCS1_TEST_CASE_IMPL(name, id)                                                          \
    static void id();                                                                           \
    static const ::rpcs1::test::Registrar RPCS1_TEST_CAT(id, _registrar){name, &id, __FILE__,   \
                                                                         __LINE__};             \
    static void id()

#define TEST_CASE(name) RPCS1_TEST_CASE_IMPL(name, RPCS1_TEST_CAT(rpcs1_test_case_, __COUNTER__))

#define RPCS1_CHECK_IMPL(expr, on_fail)                                                         \
    do {                                                                                        \
        bool rpcs1_ok_ = false;                                                                 \
        try {                                                                                   \
            rpcs1_ok_ = static_cast<bool>(expr);                                                \
        } catch (const std::exception& rpcs1_e_) {                                              \
            ::rpcs1::test::report_failure(__FILE__, __LINE__,                                   \
                                          std::string("exceção em ") + #expr + ": " +           \
                                              rpcs1_e_.what());                                 \
            on_fail;                                                                            \
            break;                                                                              \
        }                                                                                       \
        if (!rpcs1_ok_) {                                                                       \
            ::rpcs1::test::report_failure(__FILE__, __LINE__, "falhou: " #expr);                \
            on_fail;                                                                            \
        }                                                                                       \
    } while (false)

#define CHECK(expr) RPCS1_CHECK_IMPL(expr, (void)0)
#define REQUIRE(expr) RPCS1_CHECK_IMPL(expr, throw ::rpcs1::test::RequireFailure{})

// Os operandos são copiados: guardar `const auto&` criaria referência pendente quando a
// expressão devolve uma referência para dentro de um temporário (ex.: `make_vec()[i]`).
#define RPCS1_CHECK_EQ_IMPL(a, b, on_fail)                                                      \
    do {                                                                                        \
        const auto rpcs1_a_ = (a);                                                              \
        const auto rpcs1_b_ = (b);                                                              \
        if (!::rpcs1::test::equals(rpcs1_a_, rpcs1_b_)) {                                       \
            ::rpcs1::test::report_failure(                                                      \
                __FILE__, __LINE__,                                                             \
                std::string("falhou: " #a " == " #b "\n      esquerda: ") +                     \
                    ::rpcs1::test::describe(rpcs1_a_) +                                         \
                    "\n      direita:  " + ::rpcs1::test::describe(rpcs1_b_));                  \
            on_fail;                                                                            \
        }                                                                                       \
    } while (false)

#define CHECK_EQ(a, b) RPCS1_CHECK_EQ_IMPL(a, b, (void)0)
#define REQUIRE_EQ(a, b) RPCS1_CHECK_EQ_IMPL(a, b, throw ::rpcs1::test::RequireFailure{})

#define CHECK_THROWS(expr)                                                                      \
    do {                                                                                        \
        bool rpcs1_threw_ = false;                                                              \
        try {                                                                                   \
            (void)(expr);                                                                       \
        } catch (...) {                                                                         \
            rpcs1_threw_ = true;                                                                \
        }                                                                                       \
        if (!rpcs1_threw_) {                                                                    \
            ::rpcs1::test::report_failure(__FILE__, __LINE__,                                   \
                                          "esperava exceção em: " #expr);                       \
        }                                                                                       \
    } while (false)
