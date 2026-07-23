#include "unity.h"
#include "ota_handler.hpp"

TEST_CASE("OtaHandler: null client returns NullParameter", "[ota_handler]")
{
    auto r = fpc::OtaHandler::download(nullptr);
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::NullParameter, (int)r.error());
}
