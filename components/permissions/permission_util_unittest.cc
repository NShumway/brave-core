// Copyright (c) 2025 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "components/permissions/permission_util.h"

#include "components/content_settings/core/common/content_settings_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/gurl.h"

namespace permissions {

// Test that GetPermissionString returns correct string for BRAVE_CONTEXT_MENU
TEST(PermissionUtilTest, GetPermissionString_ContextMenu) {
  std::string result =
      PermissionUtil::GetPermissionString(ContentSettingsType::BRAVE_CONTEXT_MENU);
  EXPECT_EQ(result, "BraveContextMenu");
}

// Test that GetPermissionType returns correct PermissionType for context menu
TEST(PermissionUtilTest, GetPermissionType_ContextMenu) {
  blink::PermissionType out;
  bool success = PermissionUtil::GetPermissionType(
      ContentSettingsType::BRAVE_CONTEXT_MENU, &out);
  EXPECT_TRUE(success);
  EXPECT_EQ(out, blink::PermissionType::BRAVE_CONTEXT_MENU);
}

// Test that IsPermission returns true for BRAVE_CONTEXT_MENU
TEST(PermissionUtilTest, IsPermission_ContextMenu) {
  EXPECT_TRUE(
      PermissionUtil::IsPermission(ContentSettingsType::BRAVE_CONTEXT_MENU));
}

// Test ContentSettingsTypeToPermissionType for context menu
TEST(PermissionUtilTest, ContentSettingsTypeToPermissionType_ContextMenu) {
  blink::PermissionType result =
      PermissionUtil::ContentSettingsTypeToPermissionType(
          ContentSettingsType::BRAVE_CONTEXT_MENU);
  EXPECT_EQ(result, blink::PermissionType::BRAVE_CONTEXT_MENU);
}

// Test GetCanonicalOrigin uses requesting_origin for context menu
// (iframe origins should be stored separately from parent)
TEST(PermissionUtilTest, GetCanonicalOrigin_ContextMenu_UsesRequestingOrigin) {
  GURL requesting_origin("https://iframe.example.com");
  GURL embedding_origin("https://parent.example.com");

  GURL result = PermissionUtil::GetCanonicalOrigin(
      ContentSettingsType::BRAVE_CONTEXT_MENU, requesting_origin,
      embedding_origin);

  // Should return requesting_origin, not embedding_origin
  EXPECT_EQ(result, requesting_origin);
}

// Test GetCanonicalOrigin when requesting and embedding origins are the same
TEST(PermissionUtilTest, GetCanonicalOrigin_ContextMenu_SameOrigin) {
  GURL same_origin("https://same.example.com");

  GURL result = PermissionUtil::GetCanonicalOrigin(
      ContentSettingsType::BRAVE_CONTEXT_MENU, same_origin, same_origin);

  EXPECT_EQ(result, same_origin);
}

// Test that all Brave permission types return true for IsPermission
TEST(PermissionUtilTest, IsPermission_AllBraveTypes) {
  EXPECT_TRUE(
      PermissionUtil::IsPermission(ContentSettingsType::BRAVE_CONTEXT_MENU));
  EXPECT_TRUE(
      PermissionUtil::IsPermission(ContentSettingsType::BRAVE_GOOGLE_SIGN_IN));
  EXPECT_TRUE(
      PermissionUtil::IsPermission(ContentSettingsType::BRAVE_LOCALHOST_ACCESS));
  EXPECT_TRUE(
      PermissionUtil::IsPermission(ContentSettingsType::BRAVE_ETHEREUM));
  EXPECT_TRUE(PermissionUtil::IsPermission(ContentSettingsType::BRAVE_SOLANA));
  EXPECT_TRUE(PermissionUtil::IsPermission(ContentSettingsType::BRAVE_CARDANO));
  EXPECT_TRUE(
      PermissionUtil::IsPermission(ContentSettingsType::BRAVE_OPEN_AI_CHAT));
}

// Test GetPermissionString for other Brave permission types
TEST(PermissionUtilTest, GetPermissionString_OtherBraveTypes) {
  EXPECT_EQ(PermissionUtil::GetPermissionString(
                ContentSettingsType::BRAVE_GOOGLE_SIGN_IN),
            "BraveGoogleSignInPermission");
  EXPECT_EQ(PermissionUtil::GetPermissionString(
                ContentSettingsType::BRAVE_LOCALHOST_ACCESS),
            "BraveLocalhostAccessPermission");
  EXPECT_EQ(
      PermissionUtil::GetPermissionString(ContentSettingsType::BRAVE_ETHEREUM),
      "BraveEthereum");
  EXPECT_EQ(
      PermissionUtil::GetPermissionString(ContentSettingsType::BRAVE_SOLANA),
      "BraveSolana");
  EXPECT_EQ(
      PermissionUtil::GetPermissionString(ContentSettingsType::BRAVE_CARDANO),
      "BraveCardano");
  EXPECT_EQ(PermissionUtil::GetPermissionString(
                ContentSettingsType::BRAVE_OPEN_AI_CHAT),
            "BraveOpenAIChatPermission");
}

}  // namespace permissions
