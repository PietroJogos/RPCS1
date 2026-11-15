// Tratamento de erros recuperaveis do RPCS1.
//
// Erros esperados (arquivo ausente, configuracao invalida, BIOS com tamanho
// errado...) sao propagados como valores via Result<T>/Status, nunca como
// excecoes. Excecoes ficam reservadas para falhas de programacao e situacoes
// realmente excepcionais (ex.: falta de memoria).
#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace rpcs1 {

enum class ErrorCode {
    InvalidArgument,
    NotFound,
    IoError,
    ParseError,
    InvalidData,
    OutOfRange,
    Unsupported,
    PlatformError,
    Internal,
};

[[nodiscard]] const char* to_string(ErrorCode code) noexcept;

struct Error {
    ErrorCode code = ErrorCode::Internal;
    std::string message;

    // Texto legivel: "<codigo>: <mensagem>".
    [[nodiscard]] std::string describe() const;
};

[[nodiscard]] inline Error make_error(ErrorCode code, std::string message) {
    return Error{code, std::move(message)};
}

// Lancada quando se acessa o valor de um Result que contem erro (bug do chamador).
class BadResultAccess : public std::logic_error {
public:
    explicit BadResultAccess(const Error& error);
};

template <typename T>
class [[nodiscard]] Result {
public:
    using value_type = T;

    Result(const T& value) : storage_(std::in_place_index<0>, value) {}
    Result(T&& value) : storage_(std::in_place_index<0>, std::move(value)) {}
    Result(Error error) : storage_(std::in_place_index<1>, std::move(error)) {}

    template <typename U>
        requires(std::is_constructible_v<T, U &&> &&
                 !std::is_same_v<std::remove_cvref_t<U>, T> &&
                 !std::is_same_v<std::remove_cvref_t<U>, Error> &&
                 !std::is_same_v<std::remove_cvref_t<U>, Result>)
    Result(U&& value) : storage_(std::in_place_index<0>, std::forward<U>(value)) {}

    [[nodiscard]] bool ok() const noexcept { return storage_.index() == 0; }
    explicit operator bool() const noexcept { return ok(); }

    [[nodiscard]] T& value() & {
        check();
        return std::get<0>(storage_);
    }
    [[nodiscard]] const T& value() const& {
        check();
        return std::get<0>(storage_);
    }
    // Em um Result temporário o valor é movido para fora e retornado por valor:
    // retornar T&& criaria referência pendente em `const auto& x = f().value();`.
    [[nodiscard]] T value() && {
        check();
        return std::get<0>(std::move(storage_));
    }

    [[nodiscard]] T value_or(T fallback) const& {
        return ok() ? std::get<0>(storage_) : std::move(fallback);
    }

    // Pre-condicao: !ok().
    [[nodiscard]] const Error& error() const {
        if (ok()) {
            throw std::logic_error("Result::error() chamado em um resultado sem erro");
        }
        return std::get<1>(storage_);
    }

private:
    void check() const {
        if (!ok()) {
            throw BadResultAccess(std::get<1>(storage_));
        }
    }

    std::variant<T, Error> storage_;
};

template <>
class [[nodiscard]] Result<void> {
public:
    Result() = default;
    Result(Error error) : error_(std::move(error)) {}

    [[nodiscard]] bool ok() const noexcept { return !error_.has_value(); }
    explicit operator bool() const noexcept { return ok(); }

    // Pre-condicao: !ok().
    [[nodiscard]] const Error& error() const {
        if (ok()) {
            throw std::logic_error("Status::error() chamado em um status sem erro");
        }
        return *error_;
    }

    static Result success() { return Result{}; }

private:
    std::optional<Error> error_;
};

using Status = Result<void>;

} // namespace rpcs1
