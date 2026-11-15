#include <memory>
#include <string>

#include "core/error.hpp"
#include "framework/rpcs1_test.hpp"

using namespace rpcs1;

namespace {

Result<int> half(int value) {
    if (value % 2 != 0) {
        return make_error(ErrorCode::InvalidArgument, "valor ímpar");
    }
    return value / 2;
}

Result<std::string> greeting(bool ok) {
    if (!ok) {
        return make_error(ErrorCode::NotFound, "sem saudação");
    }
    return "olá"; // conversão implícita de const char*
}

Status check_positive(int value) {
    if (value <= 0) {
        return make_error(ErrorCode::OutOfRange, "não positivo");
    }
    return Status::success();
}

} // namespace

TEST_CASE("error/Result com valor") {
    auto r = half(10);
    REQUIRE(r.ok());
    CHECK(static_cast<bool>(r));
    CHECK_EQ(r.value(), 5);
    CHECK_EQ(r.value_or(-1), 5);
    CHECK_THROWS(r.error());
}

TEST_CASE("error/Result com erro") {
    auto r = half(3);
    REQUIRE(!r.ok());
    CHECK(r.error().code == ErrorCode::InvalidArgument);
    CHECK_EQ(r.error().message, std::string("valor ímpar"));
    CHECK_EQ(r.value_or(-1), -1);
    CHECK_THROWS(r.value());
}

TEST_CASE("error/BadResultAccess descreve o erro original") {
    auto r = half(7);
    try {
        (void)r.value();
        CHECK(false);
    } catch (const BadResultAccess& e) {
        CHECK(std::string(e.what()).find("valor ímpar") != std::string::npos);
    }
}

TEST_CASE("error/Result aceita tipos conversíveis e só-movíveis") {
    CHECK_EQ(greeting(true).value(), std::string("olá"));
    CHECK(!greeting(false).ok());

    Result<std::unique_ptr<int>> owned = std::make_unique<int>(42);
    REQUIRE(owned.ok());
    std::unique_ptr<int> taken = std::move(owned).value();
    REQUIRE(taken != nullptr);
    CHECK_EQ(*taken, 42);
}

TEST_CASE("error/Status") {
    CHECK(check_positive(1).ok());
    const auto st = check_positive(0);
    REQUIRE(!st.ok());
    CHECK(st.error().code == ErrorCode::OutOfRange);
    CHECK_THROWS(check_positive(5).error());
}

TEST_CASE("error/describe inclui código e mensagem") {
    const Error e = make_error(ErrorCode::IoError, "disco cheio");
    CHECK_EQ(e.describe(), std::string("erro de E/S: disco cheio"));
    const Error bare{ErrorCode::NotFound, ""};
    CHECK_EQ(bare.describe(), std::string("não encontrado"));
}
