// Copyright (c) 2025 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "components/permissions/request_type.h"

#include "components/content_settings/core/common/content_settings_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace permissions {

// Test that ContentSettingsType::BRAVE_CONTEXT_MENU maps to
// RequestType::kBraveContextMenu
TEST(RequestTypeTest, ContentSettingsTypeToRequestType_ContextMenu) {
  RequestType result =
      ContentSettingsTypeToRequestType(ContentSettingsType::BRAVE_CONTEXT_MENU);
  EXPECT_EQ(result, RequestType::kBraveContextMenu);
}

// Test the reverse mapping from RequestType to ContentSettingsType
TEST(RequestTypeTest, RequestTypeToContentSettingsType_ContextMenu) {
  std::optional<ContentSettingsType> result =
      RequestTypeToContentSettingsType(RequestType::kBraveContextMenu);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), ContentSettingsType::BRAVE_CONTEXT_MENU);
}

// Test that BRAVE_CONTEXT_MENU is a requestable permission type
TEST(RequestTypeTest, IsRequestablePermissionType_ContextMenu) {
  EXPECT_TRUE(
      IsRequestablePermissionType(ContentSettingsType::BRAVE_CONTEXT_MENU));
}

// Test that PermissionKeyForRequestType returns correct key for context menu
TEST(RequestTypeTest, PermissionKeyForRequestType_ContextMenu) {
  const char* key = PermissionKeyForRequestType(RequestType::kBraveContextMenu);
  EXPECT_STREQ(key, "brave_context_menu");
}

// Test bidirectional mapping consistency for all Brave permission types
TEST(RequestTypeTest, BravePermissionTypesBidirectionalMapping) {
  // Test BRAVE_CONTEXT_MENU round-trip
  {
    RequestType request_type = ContentSettingsTypeToRequestType(
        ContentSettingsType::BRAVE_CONTEXT_MENU);
    std::optional<ContentSettingsType> back =
        RequestTypeToContentSettingsType(request_type);
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(back.value(), ContentSettingsType::BRAVE_CONTEXT_MENU);
  }

  // Test BRAVE_GOOGLE_SIGN_IN round-trip
  {
    RequestType request_type = ContentSettingsTypeToRequestType(
        ContentSettingsType::BRAVE_GOOGLE_SIGN_IN);
    std::optional<ContentSettingsType> back =
        RequestTypeToContentSettingsType(request_type);
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(back.value(), ContentSettingsType::BRAVE_GOOGLE_SIGN_IN);
  }

  // Test BRAVE_LOCALHOST_ACCESS round-trip
  {
    RequestType request_type = ContentSettingsTypeToRequestType(
        ContentSettingsType::BRAVE_LOCALHOST_ACCESS);
    std::optional<ContentSettingsType> back =
        RequestTypeToContentSettingsType(request_type);
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(back.value(), ContentSettingsType::BRAVE_LOCALHOST_ACCESS);
  }
}

// Test that all Brave permission types are requestable
TEST(RequestTypeTest, AllBravePermissionTypesAreRequestable) {
  EXPECT_TRUE(
      IsRequestablePermissionType(ContentSettingsType::BRAVE_CONTEXT_MENU));
  EXPECT_TRUE(
      IsRequestablePermissionType(ContentSettingsType::BRAVE_GOOGLE_SIGN_IN));
  EXPECT_TRUE(
      IsRequestablePermissionType(ContentSettingsType::BRAVE_LOCALHOST_ACCESS));
  EXPECT_TRUE(
      IsRequestablePermissionType(ContentSettingsType::BRAVE_ETHEREUM));
  EXPECT_TRUE(IsRequestablePermissionType(ContentSettingsType::BRAVE_SOLANA));
  EXPECT_TRUE(IsRequestablePermissionType(ContentSettingsType::BRAVE_CARDANO));
  EXPECT_TRUE(
      IsRequestablePermissionType(ContentSettingsType::BRAVE_OPEN_AI_CHAT));
}

}  // namespace permissions
