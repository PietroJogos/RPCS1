#include "core/error.hpp"

namespace rpcs1 {

const char* to_string(ErrorCode code) noexcept {
    switch (code) {
    case ErrorCode::InvalidArgument: return "argumento inválido";
    case ErrorCode::NotFound: return "não encontrado";
    case ErrorCode::IoError: return "erro de E/S";
    case ErrorCode::ParseError: return "erro de sintaxe";
    case ErrorCode::InvalidData: return "dados inválidos";
    case ErrorCode::OutOfRange: return "fora do intervalo";
    case ErrorCode::Unsupported: return "não suportado";
    case ErrorCode::PlatformError: return "erro da plataforma";
    case ErrorCode::Internal: return "erro interno";
    }
    return "erro desconhecido";
}

std::string Error::describe() const {
    std::string text = to_string(code);
    if (!message.empty()) {
        text += ": ";
        text += message;
    }
    return text;
}

BadResultAccess::BadResultAccess(const Error& error)
    : std::logic_error("acesso ao valor de um Result com erro (" + error.describe() + ")") {}

} // namespace rpcs1
