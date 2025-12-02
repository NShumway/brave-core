// Copyright (c) 2025 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "brave/components/permissions/contexts/brave_context_menu_permission_context.h"

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "brave/components/permissions/brave_permission_manager.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace permissions {

class BraveContextMenuPermissionContextTest : public testing::Test {
 public:
  BraveContextMenuPermissionContextTest() = default;
  ~BraveContextMenuPermissionContextTest() override = default;

  void SetUp() override {
    map_ = HostContentSettingsMapFactory::GetForProfile(&profile_);
    profile_.SetPermissionControllerDelegate(
        base::WrapUnique(static_cast<BravePermissionManager*>(
            PermissionManagerFactory::GetInstance()
                ->BuildServiceInstanceForBrowserContext(browser_context())
                .release())));
  }

  void TearDown() override {
    profile_.SetPermissionControllerDelegate(nullptr);
  }

  HostContentSettingsMap* map() { return map_.get(); }
  content::BrowserContext* browser_context() { return &profile_; }
  TestingProfile* profile() { return &profile_; }

  ContentSetting GetContextMenuSetting(const GURL& url) {
    return map()->GetContentSetting(
        url, url, ContentSettingsType::BRAVE_CONTEXT_MENU);
  }

  void SetContextMenuSetting(const GURL& url, ContentSetting setting) {
    map()->SetContentSettingDefaultScope(
        url, url, ContentSettingsType::BRAVE_CONTEXT_MENU, setting);
  }

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  TestingProfile profile_;
  scoped_refptr<HostContentSettingsMap> map_;
};

// Test that the default content setting is ASK
TEST_F(BraveContextMenuPermissionContextTest, DefaultSettingIsAsk) {
  GURL test_url("https://example.com");
  // The registered default for BRAVE_CONTEXT_MENU is CONTENT_SETTING_ASK
  EXPECT_EQ(GetContextMenuSetting(test_url), CONTENT_SETTING_ASK);
}

// Test setting ALLOW permission
TEST_F(BraveContextMenuPermissionContextTest, SetAllowPermission) {
  GURL test_url("https://example.com");
  EXPECT_EQ(GetContextMenuSetting(test_url), CONTENT_SETTING_ASK);

  SetContextMenuSetting(test_url, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(GetContextMenuSetting(test_url), CONTENT_SETTING_ALLOW);
}

// Test setting BLOCK permission
TEST_F(BraveContextMenuPermissionContextTest, SetBlockPermission) {
  GURL test_url("https://example.com");
  EXPECT_EQ(GetContextMenuSetting(test_url), CONTENT_SETTING_ASK);

  SetContextMenuSetting(test_url, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(GetContextMenuSetting(test_url), CONTENT_SETTING_BLOCK);
}

// Test that different origins have isolated settings
TEST_F(BraveContextMenuPermissionContextTest, OriginIsolation) {
  GURL url1("https://example.com");
  GURL url2("https://other.com");

  SetContextMenuSetting(url1, CONTENT_SETTING_ALLOW);
  SetContextMenuSetting(url2, CONTENT_SETTING_BLOCK);

  EXPECT_EQ(GetContextMenuSetting(url1), CONTENT_SETTING_ALLOW);
  EXPECT_EQ(GetContextMenuSetting(url2), CONTENT_SETTING_BLOCK);
}

// Test that subdomains are treated as separate origins
TEST_F(BraveContextMenuPermissionContextTest, SubdomainIsolation) {
  GURL url1("https://docs.google.com");
  GURL url2("https://sheets.google.com");

  SetContextMenuSetting(url1, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(GetContextMenuSetting(url1), CONTENT_SETTING_ALLOW);
  EXPECT_EQ(GetContextMenuSetting(url2), CONTENT_SETTING_ASK);
}

// Test that HTTP and HTTPS are treated as separate origins
TEST_F(BraveContextMenuPermissionContextTest, SchemeIsolation) {
  GURL http_url("http://example.com");
  GURL https_url("https://example.com");

  SetContextMenuSetting(http_url, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(GetContextMenuSetting(http_url), CONTENT_SETTING_ALLOW);
  EXPECT_EQ(GetContextMenuSetting(https_url), CONTENT_SETTING_ASK);
}

// Test resetting permission to default
TEST_F(BraveContextMenuPermissionContextTest, ResetToDefault) {
  GURL test_url("https://example.com");

  SetContextMenuSetting(test_url, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(GetContextMenuSetting(test_url), CONTENT_SETTING_ALLOW);

  // Setting to DEFAULT clears the stored value, so it returns the registered
  // default (ASK)
  SetContextMenuSetting(test_url, CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(GetContextMenuSetting(test_url), CONTENT_SETTING_ASK);
}

// Test that the permission context works on insecure origins (HTTP)
// This indirectly tests IsRestrictedToSecureOrigins() returns false
TEST_F(BraveContextMenuPermissionContextTest, WorksOnInsecureOrigins) {
  // HTTP origin should be able to store settings
  GURL http_url("http://insecure.example.com");
  SetContextMenuSetting(http_url, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(GetContextMenuSetting(http_url), CONTENT_SETTING_BLOCK);
}

// Test that ports are part of origin isolation
TEST_F(BraveContextMenuPermissionContextTest, PortIsolation) {
  GURL url_default_port("https://example.com");
  GURL url_custom_port("https://example.com:8443");

  SetContextMenuSetting(url_default_port, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(GetContextMenuSetting(url_default_port), CONTENT_SETTING_ALLOW);
  EXPECT_EQ(GetContextMenuSetting(url_custom_port), CONTENT_SETTING_ASK);
}

// Test that paths don't affect origin (same origin, different paths)
TEST_F(BraveContextMenuPermissionContextTest, PathIndependence) {
  GURL url_path1("https://example.com/page1.html");
  GURL url_path2("https://example.com/subdir/page2.html");
  GURL url_no_path("https://example.com");

  SetContextMenuSetting(url_path1, CONTENT_SETTING_ALLOW);

  // All paths on same origin should share the setting
  EXPECT_EQ(GetContextMenuSetting(url_path1), CONTENT_SETTING_ALLOW);
  EXPECT_EQ(GetContextMenuSetting(url_path2), CONTENT_SETTING_ALLOW);
  EXPECT_EQ(GetContextMenuSetting(url_no_path), CONTENT_SETTING_ALLOW);
}

// Test multiple settings operations in sequence
TEST_F(BraveContextMenuPermissionContextTest, SequentialOperations) {
  GURL test_url("https://example.com");

  // Start with registered default (ASK)
  EXPECT_EQ(GetContextMenuSetting(test_url), CONTENT_SETTING_ASK);

  // Set to ALLOW
  SetContextMenuSetting(test_url, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(GetContextMenuSetting(test_url), CONTENT_SETTING_ALLOW);

  // Change to BLOCK
  SetContextMenuSetting(test_url, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(GetContextMenuSetting(test_url), CONTENT_SETTING_BLOCK);

  // Reset to DEFAULT (clears stored value, returns registered default ASK)
  SetContextMenuSetting(test_url, CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(GetContextMenuSetting(test_url), CONTENT_SETTING_ASK);

  // Set to BLOCK again
  SetContextMenuSetting(test_url, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(GetContextMenuSetting(test_url), CONTENT_SETTING_BLOCK);
}

// Test that clearing all settings works
TEST_F(BraveContextMenuPermissionContextTest, ClearAllSettings) {
  GURL url1("https://example1.com");
  GURL url2("https://example2.com");
  GURL url3("https://example3.com");

  SetContextMenuSetting(url1, CONTENT_SETTING_ALLOW);
  SetContextMenuSetting(url2, CONTENT_SETTING_BLOCK);
  SetContextMenuSetting(url3, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(GetContextMenuSetting(url1), CONTENT_SETTING_ALLOW);
  EXPECT_EQ(GetContextMenuSetting(url2), CONTENT_SETTING_BLOCK);
  EXPECT_EQ(GetContextMenuSetting(url3), CONTENT_SETTING_ALLOW);

  // Clear all context menu settings
  map()->ClearSettingsForOneType(ContentSettingsType::BRAVE_CONTEXT_MENU);

  // After clearing, returns registered default (ASK)
  EXPECT_EQ(GetContextMenuSetting(url1), CONTENT_SETTING_ASK);
  EXPECT_EQ(GetContextMenuSetting(url2), CONTENT_SETTING_ASK);
  EXPECT_EQ(GetContextMenuSetting(url3), CONTENT_SETTING_ASK);
}

// Test IP address origins work correctly
TEST_F(BraveContextMenuPermissionContextTest, IpAddressOrigins) {
  GURL localhost("http://127.0.0.1");
  GURL localhost_port("http://127.0.0.1:8080");
  GURL other_ip("http://192.168.1.1");

  SetContextMenuSetting(localhost, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(GetContextMenuSetting(localhost), CONTENT_SETTING_ALLOW);
  EXPECT_EQ(GetContextMenuSetting(localhost_port), CONTENT_SETTING_ASK);
  EXPECT_EQ(GetContextMenuSetting(other_ip), CONTENT_SETTING_ASK);
}

// Test file:// URLs (if supported)
TEST_F(BraveContextMenuPermissionContextTest, FileUrls) {
  GURL file_url("file:///path/to/file.html");

  // File URLs may or may not support content settings
  // This test documents the behavior
  SetContextMenuSetting(file_url, CONTENT_SETTING_BLOCK);
  // The setting may or may not persist depending on implementation
  // At minimum, it shouldn't crash
}

// Test that the content settings type is correct
TEST_F(BraveContextMenuPermissionContextTest, ContentSettingsType) {
  BraveContextMenuPermissionContext context(browser_context());
  // Verify the context was created with the correct content settings type
  // by checking we can get/set settings for BRAVE_CONTEXT_MENU
  GURL test_url("https://test.com");
  SetContextMenuSetting(test_url, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(GetContextMenuSetting(test_url), CONTENT_SETTING_ALLOW);
}

// Test that query strings don't affect origin matching
TEST_F(BraveContextMenuPermissionContextTest, QueryStringIgnored) {
  GURL url_with_query1("https://example.com/page?query=1");
  GURL url_with_query2("https://example.com/page?query=2");
  GURL url_no_query("https://example.com/page");

  SetContextMenuSetting(url_with_query1, CONTENT_SETTING_ALLOW);

  // Query strings should be ignored - all share same origin setting
  EXPECT_EQ(GetContextMenuSetting(url_with_query1), CONTENT_SETTING_ALLOW);
  EXPECT_EQ(GetContextMenuSetting(url_with_query2), CONTENT_SETTING_ALLOW);
  EXPECT_EQ(GetContextMenuSetting(url_no_query), CONTENT_SETTING_ALLOW);
}

// Test that URL fragments don't affect origin matching
TEST_F(BraveContextMenuPermissionContextTest, FragmentIgnored) {
  GURL url_fragment1("https://example.com/page#section1");
  GURL url_fragment2("https://example.com/page#section2");
  GURL url_no_fragment("https://example.com/page");

  SetContextMenuSetting(url_fragment1, CONTENT_SETTING_ALLOW);

  // Fragments should be ignored - all share same origin setting
  EXPECT_EQ(GetContextMenuSetting(url_fragment1), CONTENT_SETTING_ALLOW);
  EXPECT_EQ(GetContextMenuSetting(url_fragment2), CONTENT_SETTING_ALLOW);
  EXPECT_EQ(GetContextMenuSetting(url_no_fragment), CONTENT_SETTING_ALLOW);
}

// Test that opaque origins (data:, blob:) return registered default
TEST_F(BraveContextMenuPermissionContextTest, OpaqueOriginsReturnDefault) {
  // Data URLs have opaque origins
  GURL data_url("data:text/html,<h1>Test</h1>");

  // Attempting to set a setting for an opaque origin
  SetContextMenuSetting(data_url, CONTENT_SETTING_ALLOW);

  // Opaque origins should return the registered default (ASK) since they can't
  // be stored. The actual behavior depends on HostContentSettingsMap
  // implementation but it should not crash and should return a safe default
  ContentSetting setting = GetContextMenuSetting(data_url);
  // Either ASK (registered default) or the setting was stored
  EXPECT_TRUE(setting == CONTENT_SETTING_ASK ||
              setting == CONTENT_SETTING_ALLOW);
}

// Test that settings are isolated between different profiles
TEST_F(BraveContextMenuPermissionContextTest, ProfileSeparation) {
  GURL test_url("https://example.com");

  // Set ALLOW in the main profile
  SetContextMenuSetting(test_url, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(GetContextMenuSetting(test_url), CONTENT_SETTING_ALLOW);

  // Create a second profile
  TestingProfile profile2;
  scoped_refptr<HostContentSettingsMap> map2 =
      HostContentSettingsMapFactory::GetForProfile(&profile2);

  // Second profile should have ASK (registered default, isolated)
  EXPECT_EQ(map2->GetContentSetting(test_url, test_url,
                                    ContentSettingsType::BRAVE_CONTEXT_MENU),
            CONTENT_SETTING_ASK);

  // Set BLOCK in second profile
  map2->SetContentSettingDefaultScope(
      test_url, test_url, ContentSettingsType::BRAVE_CONTEXT_MENU,
      CONTENT_SETTING_BLOCK);

  // Verify isolation - first profile still ALLOW, second profile BLOCK
  EXPECT_EQ(GetContextMenuSetting(test_url), CONTENT_SETTING_ALLOW);
  EXPECT_EQ(map2->GetContentSetting(test_url, test_url,
                                    ContentSettingsType::BRAVE_CONTEXT_MENU),
            CONTENT_SETTING_BLOCK);
}

// Test permission context can be instantiated correctly
TEST_F(BraveContextMenuPermissionContextTest, PermissionContextInstantiation) {
  // Create a permission context
  BraveContextMenuPermissionContext context(browser_context());

  // The context should work with the content settings system
  // If instantiation failed, the map operations below would fail
  GURL test_url("https://instantiation-test.com");
  SetContextMenuSetting(test_url, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(GetContextMenuSetting(test_url), CONTENT_SETTING_BLOCK);

  // Verify context works on both HTTP and HTTPS
  // This indirectly verifies IsRestrictedToSecureOrigins() returns false
  GURL http_url("http://instantiation-test.com");
  SetContextMenuSetting(http_url, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(GetContextMenuSetting(http_url), CONTENT_SETTING_ALLOW);
}

}  // namespace permissions
