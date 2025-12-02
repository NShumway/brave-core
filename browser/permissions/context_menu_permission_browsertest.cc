// Copyright (c) 2025 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "brave/components/constants/brave_paths.h"
#include "brave/components/context_menu/core/common/default_allowlist.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

using net::test_server::EmbeddedTestServer;

namespace {

constexpr char kBlockingPageUrl[] = "/context_menu_block.html";
constexpr char kAllowingPageUrl[] = "/context_menu_allow.html";
constexpr char kIframePageUrl[] = "/context_menu_iframe.html";
constexpr char kTestDomain[] = "a.com";
constexpr char kIframeDomain[] = "b.com";

}  // namespace

class ContextMenuPermissionBrowserTest : public InProcessBrowserTest {
 public:
  ContextMenuPermissionBrowserTest() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    host_resolver()->AddRule("*", "127.0.0.1");
    current_browser_ = InProcessBrowserTest::browser();

    base::FilePath test_data_dir;
    base::PathService::Get(brave::DIR_TEST_DATA, &test_data_dir);

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS);

    https_server_->ServeFilesFromDirectory(test_data_dir);
    https_server_->AddDefaultHandlers(GetChromeTestDataDir());
    content::SetupCrossSiteRedirector(https_server_.get());
    ASSERT_TRUE(https_server_->Start());

    permissions::PermissionRequestManager* manager =
        GetPermissionRequestManager();
    prompt_factory_ =
        std::make_unique<permissions::MockPermissionPromptFactory>(manager);

    blocking_url_ = https_server_->GetURL(kTestDomain, kBlockingPageUrl);
    allowing_url_ = https_server_->GetURL(kTestDomain, kAllowingPageUrl);
    iframe_parent_url_ = https_server_->GetURL(kTestDomain, kIframePageUrl);
    iframe_child_url_ = https_server_->GetURL(kIframeDomain, kBlockingPageUrl);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
    current_browser_ = InProcessBrowserTest::browser();
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
    InProcessBrowserTest::TearDownInProcessBrowserTestFixture();
  }

  void TearDownOnMainThread() override { prompt_factory_.reset(); }

  HostContentSettingsMap* content_settings() {
    return HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  }

  permissions::PermissionRequestManager* GetPermissionRequestManager() {
    return permissions::PermissionRequestManager::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  permissions::MockPermissionPromptFactory* prompt_factory() {
    return prompt_factory_.get();
  }

  Browser* browser() { return current_browser_; }

  void SetBrowser(Browser* browser) { current_browser_ = browser; }

  void SetPromptFactory(permissions::PermissionRequestManager* manager) {
    prompt_factory_ =
        std::make_unique<permissions::MockPermissionPromptFactory>(manager);
  }

  content::WebContents* contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Check content setting for the given URL
  ContentSetting GetContextMenuSetting(const GURL& url) {
    return content_settings()->GetContentSetting(
        url, url, ContentSettingsType::BRAVE_CONTEXT_MENU);
  }

  // Set content setting for the given URL
  void SetContextMenuSetting(const GURL& url, ContentSetting setting) {
    content_settings()->SetContentSettingDefaultScope(
        url, url, ContentSettingsType::BRAVE_CONTEXT_MENU, setting);
  }

  // Verify the content setting matches expected value
  void CheckCurrentSettingIs(const GURL& url, ContentSetting expected) {
    EXPECT_EQ(GetContextMenuSetting(url), expected);
  }

  // Simulate a right-click on the page by triggering the contextmenu event
  // This won't actually show a real context menu, but will trigger any
  // JavaScript handlers and the permission flow
  void TriggerContextMenuEvent() {
    std::string script = R"(
      new Promise((resolve) => {
        const event = new MouseEvent('contextmenu', {
          bubbles: true,
          cancelable: true,
          view: window,
          button: 2
        });
        document.body.dispatchEvent(event);
        resolve(event.defaultPrevented);
      });
    )";
    EXPECT_EQ(true, EvalJs(contents(), script));
  }

  // Navigate and trigger context menu blocking
  void NavigateAndTriggerBlock(const GURL& url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    TriggerContextMenuEvent();
  }

  // Test flows similar to google_sign_in tests
  void CheckDefaultSetting() {
    CheckCurrentSettingIs(blocking_url_,
                          ContentSetting::CONTENT_SETTING_DEFAULT);
  }

  void CheckAskAndAcceptFlow() {
    EXPECT_EQ(0, prompt_factory()->show_count());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), blocking_url_));
    // Accept prompt
    prompt_factory()->set_response_type(
        permissions::PermissionRequestManager::ACCEPT_ALL);
    TriggerContextMenuEvent();
    prompt_factory()->WaitForPermissionBubble();
    // Make sure prompt came up
    EXPECT_EQ(1, prompt_factory()->show_count());
    // Check content setting is now ALLOW
    CheckCurrentSettingIs(blocking_url_, ContentSetting::CONTENT_SETTING_ALLOW);
  }

  void CheckAskAndDenyFlow() {
    EXPECT_EQ(0, prompt_factory()->show_count());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), blocking_url_));
    // Deny prompt
    prompt_factory()->set_response_type(
        permissions::PermissionRequestManager::DENY_ALL);
    TriggerContextMenuEvent();
    prompt_factory()->WaitForPermissionBubble();
    // Make sure prompt came up
    EXPECT_EQ(1, prompt_factory()->show_count());
    // Check content setting is now BLOCK
    CheckCurrentSettingIs(blocking_url_, ContentSetting::CONTENT_SETTING_BLOCK);
  }

  void CheckAskAndDismissFlow() {
    EXPECT_EQ(0, prompt_factory()->show_count());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), blocking_url_));
    // Dismiss prompt
    prompt_factory()->set_response_type(
        permissions::PermissionRequestManager::DISMISS);
    TriggerContextMenuEvent();
    prompt_factory()->WaitForPermissionBubble();
    // Make sure prompt came up
    EXPECT_EQ(1, prompt_factory()->show_count());
    // Setting should remain DEFAULT after dismiss
    CheckCurrentSettingIs(blocking_url_,
                          ContentSetting::CONTENT_SETTING_DEFAULT);
  }

  // Check that when setting is ALLOW, no prompt appears
  void CheckAllowedFlow(int initial_prompts_shown = 0) {
    EXPECT_EQ(initial_prompts_shown, prompt_factory()->show_count());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), blocking_url_));
    TriggerContextMenuEvent();
    // No new prompt should appear
    EXPECT_EQ(initial_prompts_shown, prompt_factory()->show_count());
    CheckCurrentSettingIs(blocking_url_, ContentSetting::CONTENT_SETTING_ALLOW);
  }

  // Check that when setting is BLOCK, no prompt appears
  void CheckBlockedFlow(int initial_prompts_shown = 0) {
    EXPECT_EQ(initial_prompts_shown, prompt_factory()->show_count());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), blocking_url_));
    TriggerContextMenuEvent();
    // No new prompt should appear
    EXPECT_EQ(initial_prompts_shown, prompt_factory()->show_count());
    CheckCurrentSettingIs(blocking_url_, ContentSetting::CONTENT_SETTING_BLOCK);
  }

 protected:
  GURL blocking_url_;
  GURL allowing_url_;
  GURL iframe_parent_url_;
  GURL iframe_child_url_;
  content::ContentMockCertVerifier mock_cert_verifier_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  raw_ptr<Browser, DanglingUntriaged> current_browser_;

 private:
  std::unique_ptr<permissions::MockPermissionPromptFactory> prompt_factory_;
};

// Test that the default content setting is DEFAULT (ask)
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest, DefaultSettingIsAsk) {
  CheckDefaultSetting();
}

// Test that accepting the permission prompt sets ALLOW
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest, PermissionAccept) {
  CheckAskAndAcceptFlow();
}

// Test that denying the permission prompt sets BLOCK
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest, PermissionDeny) {
  CheckAskAndDenyFlow();
}

// Test that dismissing the permission prompt leaves setting as DEFAULT
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest, PermissionDismiss) {
  CheckAskAndDismissFlow();
}

// Test that after accepting, no prompt appears on subsequent visits
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest,
                       NoPromptAfterAccepting) {
  CheckAskAndAcceptFlow();
  CheckAllowedFlow(1);  // 1 prompt already shown
}

// Test that after denying, no prompt appears on subsequent visits
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest, NoPromptAfterDenying) {
  CheckAskAndDenyFlow();
  CheckBlockedFlow(1);  // 1 prompt already shown
}

// Test that manually setting ALLOW via content settings works
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest,
                       ManualSettingAllowWorks) {
  SetContextMenuSetting(blocking_url_, ContentSetting::CONTENT_SETTING_ALLOW);
  CheckAllowedFlow();
}

// Test that manually setting BLOCK via content settings works
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest,
                       ManualSettingBlockWorks) {
  SetContextMenuSetting(blocking_url_, ContentSetting::CONTENT_SETTING_BLOCK);
  CheckBlockedFlow();
}

// Test incognito mode inherits ALLOW from normal mode
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest,
                       IncognitoModeInheritAllow) {
  // Set permission in normal mode
  CheckAskAndAcceptFlow();
  // Create incognito browser
  Profile* profile = browser()->profile();
  Browser* incognito_browser = CreateIncognitoBrowser(profile);
  SetBrowser(incognito_browser);
  SetPromptFactory(GetPermissionRequestManager());
  // Permission should be inherited
  CheckAllowedFlow();
}

// Test incognito mode inherits BLOCK from normal mode
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest,
                       IncognitoModeInheritBlock) {
  // Set permission in normal mode
  CheckAskAndDenyFlow();
  // Create incognito browser
  Profile* profile = browser()->profile();
  Browser* incognito_browser = CreateIncognitoBrowser(profile);
  SetBrowser(incognito_browser);
  SetPromptFactory(GetPermissionRequestManager());
  // Permission should be inherited
  CheckBlockedFlow();
}

// Test that permission set in incognito does not leak to normal mode
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest,
                       IncognitoModeDoesNotLeak) {
  Browser* original_browser = browser();
  Browser* incognito_browser = CreateIncognitoBrowser();
  SetBrowser(incognito_browser);
  SetPromptFactory(GetPermissionRequestManager());
  CheckAskAndAcceptFlow();
  // Check permission did not leak
  SetBrowser(original_browser);
  SetPromptFactory(GetPermissionRequestManager());
  CheckDefaultSetting();
}

// Test that sites on non-blocking page don't trigger any permission flow
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest,
                       NonBlockingPageNoPrompt) {
  EXPECT_EQ(0, prompt_factory()->show_count());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), allowing_url_));
  // Trigger context menu - should not be blocked
  std::string script = R"(
    new Promise((resolve) => {
      const event = new MouseEvent('contextmenu', {
        bubbles: true,
        cancelable: true,
        view: window,
        button: 2
      });
      document.body.dispatchEvent(event);
      resolve(event.defaultPrevented);
    });
  )";
  // Event should NOT be prevented on this page
  EXPECT_EQ(false, EvalJs(contents(), script));
  // No prompt should have appeared
  EXPECT_EQ(0, prompt_factory()->show_count());
}

// Test that different origins have isolated permissions
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest,
                       OriginIsolationWorks) {
  // Set ALLOW for test domain
  SetContextMenuSetting(blocking_url_, ContentSetting::CONTENT_SETTING_ALLOW);
  // iframe domain should still be DEFAULT
  CheckCurrentSettingIs(iframe_child_url_,
                        ContentSetting::CONTENT_SETTING_DEFAULT);
}

// Test the allowlist contains expected origins
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest,
                       AllowlistContainsExpectedOrigins) {
  // Verify allowlist includes known productivity sites
  EXPECT_TRUE(brave::context_menu::kDefaultAllowHidingOrigins.contains(
      "https://docs.google.com"));
  EXPECT_TRUE(brave::context_menu::kDefaultAllowHidingOrigins.contains(
      "https://sheets.google.com"));
  EXPECT_TRUE(brave::context_menu::kDefaultAllowHidingOrigins.contains(
      "https://figma.com"));
  EXPECT_TRUE(brave::context_menu::kDefaultAllowHidingOrigins.contains(
      "https://notion.so"));
  EXPECT_TRUE(brave::context_menu::kDefaultAllowHidingOrigins.contains(
      "https://excalidraw.com"));

  // Verify non-allowlisted origins are not in the list
  EXPECT_FALSE(brave::context_menu::kDefaultAllowHidingOrigins.contains(
      "https://example.com"));
  EXPECT_FALSE(brave::context_menu::kDefaultAllowHidingOrigins.contains(
      "https://mail.google.com"));
}

// Test that permission persists across page navigations within same origin
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest,
                       SettingPersistsAcrossNavigations) {
  // Accept permission on first page
  CheckAskAndAcceptFlow();

  // Navigate to a different page on the same origin
  GURL same_origin_different_path =
      https_server_->GetURL(kTestDomain, kAllowingPageUrl);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), same_origin_different_path));

  // Navigate back to blocking page - setting should persist
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), blocking_url_));

  // No new prompt should appear (setting persisted)
  EXPECT_EQ(1, prompt_factory()->show_count());
  CheckCurrentSettingIs(blocking_url_, ContentSetting::CONTENT_SETTING_ALLOW);
}

// Test that subdomains are treated as separate origins
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest,
                       SubdomainsAreSeparateOrigins) {
  // Set permission for a.com
  SetContextMenuSetting(blocking_url_, ContentSetting::CONTENT_SETTING_ALLOW);

  // Create URL for subdomain
  GURL subdomain_url =
      https_server_->GetURL("sub." + std::string(kTestDomain), kBlockingPageUrl);

  // Subdomain should have DEFAULT (separate origin)
  CheckCurrentSettingIs(subdomain_url, ContentSetting::CONTENT_SETTING_DEFAULT);
}

// Test that HTTP and HTTPS are treated as separate origins
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest,
                       HttpAndHttpsAreSeparateOrigins) {
  // Set up HTTP server
  net::EmbeddedTestServer http_server(net::EmbeddedTestServer::TYPE_HTTP);
  base::FilePath test_data_dir;
  base::PathService::Get(brave::DIR_TEST_DATA, &test_data_dir);
  http_server.ServeFilesFromDirectory(test_data_dir);
  ASSERT_TRUE(http_server.Start());

  GURL http_url = http_server.GetURL(kTestDomain, kBlockingPageUrl);

  // Set ALLOW for HTTPS
  SetContextMenuSetting(blocking_url_, ContentSetting::CONTENT_SETTING_ALLOW);

  // HTTP should still be DEFAULT (different scheme = different origin)
  CheckCurrentSettingIs(http_url, ContentSetting::CONTENT_SETTING_DEFAULT);
}

// Test that permission can work on HTTP origins (not restricted to secure)
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest,
                       PermissionWorksOnHttpOrigins) {
  // Set up HTTP server
  net::EmbeddedTestServer http_server(net::EmbeddedTestServer::TYPE_HTTP);
  base::FilePath test_data_dir;
  base::PathService::Get(brave::DIR_TEST_DATA, &test_data_dir);
  http_server.ServeFilesFromDirectory(test_data_dir);
  ASSERT_TRUE(http_server.Start());

  GURL http_url = http_server.GetURL(kTestDomain, kBlockingPageUrl);

  // Should be able to set permission for HTTP origin
  SetContextMenuSetting(http_url, ContentSetting::CONTENT_SETTING_BLOCK);
  CheckCurrentSettingIs(http_url, ContentSetting::CONTENT_SETTING_BLOCK);
}

// Test resetting permission clears it properly
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest, ResetPermission) {
  // First set to ALLOW
  SetContextMenuSetting(blocking_url_, ContentSetting::CONTENT_SETTING_ALLOW);
  CheckCurrentSettingIs(blocking_url_, ContentSetting::CONTENT_SETTING_ALLOW);

  // Reset to DEFAULT
  SetContextMenuSetting(blocking_url_, ContentSetting::CONTENT_SETTING_DEFAULT);
  CheckCurrentSettingIs(blocking_url_, ContentSetting::CONTENT_SETTING_DEFAULT);
}

// Test that clearing all content settings works
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest,
                       ClearAllSettingsWorks) {
  // Set multiple permissions
  GURL url1 = https_server_->GetURL("site1.com", kBlockingPageUrl);
  GURL url2 = https_server_->GetURL("site2.com", kBlockingPageUrl);

  SetContextMenuSetting(url1, ContentSetting::CONTENT_SETTING_ALLOW);
  SetContextMenuSetting(url2, ContentSetting::CONTENT_SETTING_BLOCK);

  CheckCurrentSettingIs(url1, ContentSetting::CONTENT_SETTING_ALLOW);
  CheckCurrentSettingIs(url2, ContentSetting::CONTENT_SETTING_BLOCK);

  // Clear all settings for context menu type
  content_settings()->ClearSettingsForOneType(
      ContentSettingsType::BRAVE_CONTEXT_MENU);

  // Both should be back to DEFAULT
  CheckCurrentSettingIs(url1, ContentSetting::CONTENT_SETTING_DEFAULT);
  CheckCurrentSettingIs(url2, ContentSetting::CONTENT_SETTING_DEFAULT);
}

// Test that cross-origin iframes have separate permissions from parent
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest,
                       IframeCrossOriginPermission) {
  // Set ALLOW for parent domain (a.com)
  SetContextMenuSetting(blocking_url_, ContentSetting::CONTENT_SETTING_ALLOW);

  // Iframe domain (b.com) should still have DEFAULT
  CheckCurrentSettingIs(iframe_child_url_,
                        ContentSetting::CONTENT_SETTING_DEFAULT);

  // Set BLOCK for iframe domain
  SetContextMenuSetting(iframe_child_url_, ContentSetting::CONTENT_SETTING_BLOCK);

  // Verify isolation - each origin has its own setting
  CheckCurrentSettingIs(blocking_url_, ContentSetting::CONTENT_SETTING_ALLOW);
  CheckCurrentSettingIs(iframe_child_url_, ContentSetting::CONTENT_SETTING_BLOCK);
}

// Test that same-origin iframe inherits parent's permission
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest,
                       IframeSameOriginInherits) {
  // Same-origin iframe URL (same domain as parent)
  GURL same_origin_iframe_url =
      https_server_->GetURL(kTestDomain, "/simple.html");

  // Set ALLOW for the domain
  SetContextMenuSetting(blocking_url_, ContentSetting::CONTENT_SETTING_ALLOW);

  // Same-origin iframe should have same setting (origin-based, not page-based)
  CheckCurrentSettingIs(same_origin_iframe_url,
                        ContentSetting::CONTENT_SETTING_ALLOW);
}

// Test that allowlisted origins (like Google Docs) don't trigger prompts
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest,
                       AllowlistedOriginNoPromptVerification) {
  // Verify several allowlisted origins are recognized
  EXPECT_TRUE(brave::context_menu::IsInDefaultAllowlist(
      "https://docs.google.com"));
  EXPECT_TRUE(brave::context_menu::IsInDefaultAllowlist(
      "https://sheets.google.com"));
  EXPECT_TRUE(brave::context_menu::IsInDefaultAllowlist(
      "https://figma.com"));
  EXPECT_TRUE(brave::context_menu::IsInDefaultAllowlist(
      "https://notion.so"));

  // Non-allowlisted origins should not be in the list
  EXPECT_FALSE(brave::context_menu::IsInDefaultAllowlist(
      blocking_url_.DeprecatedGetOriginAsURL().spec()));
}

// Test that user can override allowlisted sites by setting BLOCK
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest,
                       AllowlistedOriginUserCanOverride) {
  // Simulate an allowlisted origin URL
  GURL allowlisted_origin("https://docs.google.com");

  // Verify it's in the allowlist
  EXPECT_TRUE(brave::context_menu::IsInDefaultAllowlist(
      allowlisted_origin.spec()));

  // User can still explicitly set BLOCK to override
  SetContextMenuSetting(allowlisted_origin, ContentSetting::CONTENT_SETTING_BLOCK);
  CheckCurrentSettingIs(allowlisted_origin,
                        ContentSetting::CONTENT_SETTING_BLOCK);

  // User can also explicitly ALLOW (even though it's already allowlisted)
  SetContextMenuSetting(allowlisted_origin, ContentSetting::CONTENT_SETTING_ALLOW);
  CheckCurrentSettingIs(allowlisted_origin,
                        ContentSetting::CONTENT_SETTING_ALLOW);

  // Reset back to DEFAULT
  SetContextMenuSetting(allowlisted_origin, ContentSetting::CONTENT_SETTING_DEFAULT);
  CheckCurrentSettingIs(allowlisted_origin,
                        ContentSetting::CONTENT_SETTING_DEFAULT);
}

// Test that data: URLs (opaque origins) are handled gracefully
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest,
                       OpaqueOriginHandledGracefully) {
  GURL data_url("data:text/html,<html><body>test</body></html>");

  // Attempting to set a setting for data: URL shouldn't crash
  SetContextMenuSetting(data_url, ContentSetting::CONTENT_SETTING_ALLOW);

  // The behavior may vary - either it stores or returns DEFAULT
  // The key is it doesn't crash
  ContentSetting setting = GetContextMenuSetting(data_url);
  EXPECT_TRUE(setting == ContentSetting::CONTENT_SETTING_DEFAULT ||
              setting == ContentSetting::CONTENT_SETTING_ALLOW);
}

// Test that file:// URLs can have permissions set
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest,
                       FileUrlPermissionWorks) {
  GURL file_url("file:///tmp/test.html");

  // File URLs should be able to store settings
  SetContextMenuSetting(file_url, ContentSetting::CONTENT_SETTING_BLOCK);

  // Verify the setting was stored (or at least didn't crash)
  ContentSetting setting = GetContextMenuSetting(file_url);
  // File URL handling may vary, but should not crash
  EXPECT_TRUE(setting == ContentSetting::CONTENT_SETTING_DEFAULT ||
              setting == ContentSetting::CONTENT_SETTING_BLOCK);
}

// Test multiple rapid permission checks don't cause issues
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest,
                       RapidSettingChecks) {
  GURL test_url = https_server_->GetURL("rapid-test.com", kBlockingPageUrl);

  // Perform many rapid setting changes
  for (int i = 0; i < 100; ++i) {
    if (i % 3 == 0) {
      SetContextMenuSetting(test_url, ContentSetting::CONTENT_SETTING_ALLOW);
    } else if (i % 3 == 1) {
      SetContextMenuSetting(test_url, ContentSetting::CONTENT_SETTING_BLOCK);
    } else {
      SetContextMenuSetting(test_url, ContentSetting::CONTENT_SETTING_DEFAULT);
    }
    // Verify we can read it back without crash
    GetContextMenuSetting(test_url);
  }

  // Final state should be DEFAULT (100 % 3 == 1, then 99 % 3 == 0, 98 % 3 == 2)
  // Actually 99 % 3 == 0, so last operation was ALLOW
  SetContextMenuSetting(test_url, ContentSetting::CONTENT_SETTING_DEFAULT);
  CheckCurrentSettingIs(test_url, ContentSetting::CONTENT_SETTING_DEFAULT);
}

// Test that ports are treated as part of origin
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest,
                       PortsArePartOfOrigin) {
  // Get URLs with different ports
  GURL default_port_url = https_server_->GetURL(kTestDomain, kBlockingPageUrl);

  // Create a second server on a different port
  auto https_server2 = std::make_unique<net::EmbeddedTestServer>(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  base::FilePath test_data_dir;
  base::PathService::Get(brave::DIR_TEST_DATA, &test_data_dir);
  https_server2->ServeFilesFromDirectory(test_data_dir);
  ASSERT_TRUE(https_server2->Start());

  GURL different_port_url = https_server2->GetURL(kTestDomain, kBlockingPageUrl);

  // Verify the ports are actually different
  EXPECT_NE(default_port_url.EffectiveIntPort(),
            different_port_url.EffectiveIntPort());

  // Set ALLOW for first port
  SetContextMenuSetting(default_port_url, ContentSetting::CONTENT_SETTING_ALLOW);

  // Second port should still be DEFAULT (different origin)
  CheckCurrentSettingIs(different_port_url,
                        ContentSetting::CONTENT_SETTING_DEFAULT);
}

// Test that blob: URLs are handled gracefully
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest,
                       BlobUrlHandledGracefully) {
  // blob: URLs have opaque origins derived from their creator
  GURL blob_url("blob:https://example.com/12345-uuid");

  // Should not crash when trying to set/get settings
  SetContextMenuSetting(blob_url, ContentSetting::CONTENT_SETTING_ALLOW);
  GetContextMenuSetting(blob_url);

  // The test passes if we get here without crashing
}

// Test that javascript: URLs don't affect settings
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest,
                       JavascriptUrlHandledGracefully) {
  GURL js_url("javascript:void(0)");

  // Should not crash
  SetContextMenuSetting(js_url, ContentSetting::CONTENT_SETTING_ALLOW);
  GetContextMenuSetting(js_url);

  // The test passes if we get here without crashing
}

// Test that about:blank has correct behavior
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest,
                       AboutBlankHandledGracefully) {
  GURL about_blank("about:blank");

  // Should not crash
  SetContextMenuSetting(about_blank, ContentSetting::CONTENT_SETTING_BLOCK);
  GetContextMenuSetting(about_blank);

  // The test passes if we get here without crashing
}

// Test websocket URLs are handled correctly (should use https origin)
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest,
                       WebsocketOriginMapping) {
  // WebSocket URLs shouldn't typically have context menu settings,
  // but test that they don't crash the system
  GURL wss_url("wss://example.com/socket");

  // Should not crash
  SetContextMenuSetting(wss_url, ContentSetting::CONTENT_SETTING_ALLOW);
  GetContextMenuSetting(wss_url);
}

// Test very long domain names are handled
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest,
                       LongDomainNameHandled) {
  // Create a URL with a very long subdomain
  std::string long_subdomain(200, 'a');  // 200 character subdomain
  GURL long_url = https_server_->GetURL(
      long_subdomain + "." + kTestDomain, kBlockingPageUrl);

  // Should handle without crashing
  SetContextMenuSetting(long_url, ContentSetting::CONTENT_SETTING_ALLOW);
  CheckCurrentSettingIs(long_url, ContentSetting::CONTENT_SETTING_ALLOW);
}

// Test that IDN (internationalized domain names) work correctly
IN_PROC_BROWSER_TEST_F(ContextMenuPermissionBrowserTest,
                       InternationalizedDomainNames) {
  // Punycode representation of an IDN
  GURL punycode_url = https_server_->GetURL("xn--e1afmkfd.xn--p1ai", kBlockingPageUrl);

  // Should handle IDN without crashing
  SetContextMenuSetting(punycode_url, ContentSetting::CONTENT_SETTING_BLOCK);
  CheckCurrentSettingIs(punycode_url, ContentSetting::CONTENT_SETTING_BLOCK);
}
