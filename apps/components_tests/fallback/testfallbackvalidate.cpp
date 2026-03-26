#include <gtest/gtest.h>

#include <components/fallback/validate.hpp>

namespace
{
    TEST(FallbackTest, testIsAllowedIntFallbackKey)
    {
        EXPECT_TRUE(Fallback::isAllowedIntFallbackKey("LightAttenuation_LinearMethod"));
        EXPECT_FALSE(Fallback::isAllowedIntFallbackKey("General_Werewolf_FOV"));
        EXPECT_FALSE(Fallback::isAllowedIntFallbackKey("NonExistentKey"));
        EXPECT_FALSE(Fallback::isAllowedIntFallbackKey(""));
    }

    TEST(FallbackTest, testIsAllowedFloatFallbackKey)
    {
        EXPECT_TRUE(Fallback::isAllowedFloatFallbackKey("General_Werewolf_FOV"));
        EXPECT_FALSE(Fallback::isAllowedFloatFallbackKey("LightAttenuation_LinearMethod"));
        EXPECT_FALSE(Fallback::isAllowedFloatFallbackKey("NonExistentKey"));
        EXPECT_FALSE(Fallback::isAllowedFloatFallbackKey(""));
    }

    TEST(FallbackTest, testIsAllowedNonNumericFallbackKey)
    {
        EXPECT_TRUE(Fallback::isAllowedNonNumericFallbackKey("Blood_Model_0"));
        EXPECT_TRUE(Fallback::isAllowedNonNumericFallbackKey("Blood_Texture_foobar"));
        EXPECT_TRUE(Fallback::isAllowedNonNumericFallbackKey("Level_Up_Level5"));
        EXPECT_FALSE(Fallback::isAllowedNonNumericFallbackKey("General_Werewolf_FOV"));
        EXPECT_FALSE(Fallback::isAllowedNonNumericFallbackKey("NonExistentKey"));
        EXPECT_FALSE(Fallback::isAllowedNonNumericFallbackKey(""));
    }

    TEST(FallbackTest, testIsAllowedUnusedFallbackKey)
    {
        EXPECT_TRUE(Fallback::isAllowedUnusedFallbackKey("Inventory_UniformScaling"));
        EXPECT_FALSE(Fallback::isAllowedUnusedFallbackKey("General_Werewolf_FOV"));
        EXPECT_FALSE(Fallback::isAllowedUnusedFallbackKey("NonExistentKey"));
        EXPECT_FALSE(Fallback::isAllowedUnusedFallbackKey(""));
    }
}
