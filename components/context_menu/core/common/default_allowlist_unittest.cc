// Copyright (c) 2025 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "brave/components/context_menu/core/common/default_allowlist.h"

#include <cctype>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace brave::context_menu {

TEST(DefaultAllowlistTest, GoogleDocsServicesAreAllowed) {
  EXPECT_TRUE(IsInDefaultAllowlist("https://docs.google.com"));
  EXPECT_TRUE(IsInDefaultAllowlist("https://sheets.google.com"));
  EXPECT_TRUE(IsInDefaultAllowlist("https://slides.google.com"));
}

TEST(DefaultAllowlistTest, DesignToolsAreAllowed) {
  EXPECT_TRUE(IsInDefaultAllowlist("https://figma.com"));
  EXPECT_TRUE(IsInDefaultAllowlist("https://www.figma.com"));
  EXPECT_TRUE(IsInDefaultAllowlist("https://www.canva.com"));
  EXPECT_TRUE(IsInDefaultAllowlist("https://miro.com"));
  EXPECT_TRUE(IsInDefaultAllowlist("https://excalidraw.com"));
}

TEST(DefaultAllowlistTest, ProductivityToolsAreAllowed) {
  EXPECT_TRUE(IsInDefaultAllowlist("https://notion.so"));
  EXPECT_TRUE(IsInDefaultAllowlist("https://www.notion.so"));
}

TEST(DefaultAllowlistTest, DevelopmentToolsAreAllowed) {
  EXPECT_TRUE(IsInDefaultAllowlist("https://codesandbox.io"));
  EXPECT_TRUE(IsInDefaultAllowlist("https://stackblitz.com"));
}

TEST(DefaultAllowlistTest, OtherGoogleServicesNotAllowed) {
  // Other Google services that don't have legitimate custom context menus
  EXPECT_FALSE(IsInDefaultAllowlist("https://mail.google.com"));
  EXPECT_FALSE(IsInDefaultAllowlist("https://drive.google.com"));
  EXPECT_FALSE(IsInDefaultAllowlist("https://calendar.google.com"));
  EXPECT_FALSE(IsInDefaultAllowlist("https://www.google.com"));
}

TEST(DefaultAllowlistTest, HttpNotAllowed) {
  // HTTP variants should NOT be allowed (scheme matters)
  EXPECT_FALSE(IsInDefaultAllowlist("http://docs.google.com"));
  EXPECT_FALSE(IsInDefaultAllowlist("http://figma.com"));
  EXPECT_FALSE(IsInDefaultAllowlist("http://notion.so"));
}

TEST(DefaultAllowlistTest, SubdomainsAreSeparate) {
  // www subdomain vs naked domain
  EXPECT_TRUE(IsInDefaultAllowlist("https://www.figma.com"));
  EXPECT_TRUE(IsInDefaultAllowlist("https://figma.com"));
  // But other subdomains are not
  EXPECT_FALSE(IsInDefaultAllowlist("https://app.figma.com"));
  EXPECT_FALSE(IsInDefaultAllowlist("https://staging.figma.com"));
}

TEST(DefaultAllowlistTest, WwwVariantsExplicit) {
  // www variants must be explicitly listed
  EXPECT_TRUE(IsInDefaultAllowlist("https://www.canva.com"));
  EXPECT_FALSE(IsInDefaultAllowlist("https://canva.com"));

  EXPECT_TRUE(IsInDefaultAllowlist("https://www.notion.so"));
  EXPECT_TRUE(IsInDefaultAllowlist("https://notion.so"));
}

TEST(DefaultAllowlistTest, RandomSitesNotAllowed) {
  EXPECT_FALSE(IsInDefaultAllowlist("https://example.com"));
  EXPECT_FALSE(IsInDefaultAllowlist("https://evil-site.com"));
  EXPECT_FALSE(IsInDefaultAllowlist("https://fake-figma.com"));
}

TEST(DefaultAllowlistTest, EmptyAndInvalidStrings) {
  EXPECT_FALSE(IsInDefaultAllowlist(""));
  EXPECT_FALSE(IsInDefaultAllowlist("not-a-url"));
  EXPECT_FALSE(IsInDefaultAllowlist("docs.google.com"));  // missing scheme
}

TEST(DefaultAllowlistTest, AllowlistHasExpectedSize) {
  // Ensure we have exactly 12 entries as documented
  EXPECT_EQ(kDefaultAllowHidingOrigins.size(), 12u);
}

// Test that lookup is case-sensitive (uppercase should NOT match)
TEST(DefaultAllowlistTest, CaseSensitivity) {
  // The allowlist stores lowercase origins, uppercase should not match
  EXPECT_FALSE(IsInDefaultAllowlist("HTTPS://DOCS.GOOGLE.COM"));
  EXPECT_FALSE(IsInDefaultAllowlist("Https://Docs.Google.Com"));
  EXPECT_FALSE(IsInDefaultAllowlist("https://DOCS.GOOGLE.COM"));
  EXPECT_FALSE(IsInDefaultAllowlist("https://Docs.Google.Com"));

  // Only exact lowercase match should work
  EXPECT_TRUE(IsInDefaultAllowlist("https://docs.google.com"));
}

// Test that trailing slash does NOT match
TEST(DefaultAllowlistTest, TrailingSlashNotMatched) {
  // Origins in allowlist don't have trailing slashes
  EXPECT_FALSE(IsInDefaultAllowlist("https://docs.google.com/"));
  EXPECT_FALSE(IsInDefaultAllowlist("https://figma.com/"));
  EXPECT_FALSE(IsInDefaultAllowlist("https://notion.so/"));

  // Without trailing slash should match
  EXPECT_TRUE(IsInDefaultAllowlist("https://docs.google.com"));
  EXPECT_TRUE(IsInDefaultAllowlist("https://figma.com"));
  EXPECT_TRUE(IsInDefaultAllowlist("https://notion.so"));
}

// Test that origins with paths do NOT match
TEST(DefaultAllowlistTest, PathsNotMatched) {
  // Allowlist only contains origins (scheme + host), not paths
  EXPECT_FALSE(IsInDefaultAllowlist("https://docs.google.com/document/d/123"));
  EXPECT_FALSE(IsInDefaultAllowlist("https://docs.google.com/spreadsheets"));
  EXPECT_FALSE(IsInDefaultAllowlist("https://figma.com/file/abc"));
  EXPECT_FALSE(IsInDefaultAllowlist("https://notion.so/page/123"));

  // Base origins should still match
  EXPECT_TRUE(IsInDefaultAllowlist("https://docs.google.com"));
  EXPECT_TRUE(IsInDefaultAllowlist("https://figma.com"));
  EXPECT_TRUE(IsInDefaultAllowlist("https://notion.so"));
}

// Test that origins with explicit ports do NOT match
TEST(DefaultAllowlistTest, PortsNotMatched) {
  // Default HTTPS port is 443, but explicit port changes the origin
  EXPECT_FALSE(IsInDefaultAllowlist("https://docs.google.com:443"));
  EXPECT_FALSE(IsInDefaultAllowlist("https://docs.google.com:8443"));
  EXPECT_FALSE(IsInDefaultAllowlist("https://figma.com:443"));

  // Without explicit port should match
  EXPECT_TRUE(IsInDefaultAllowlist("https://docs.google.com"));
  EXPECT_TRUE(IsInDefaultAllowlist("https://figma.com"));
}

// Test that all entries in allowlist are lowercase (compile-time guarantee)
TEST(DefaultAllowlistTest, AllEntriesAreLowercase) {
  // Verify each entry is lowercase by checking it equals itself lowercased
  for (const auto& origin : kDefaultAllowHidingOrigins) {
    std::string origin_str(origin);
    std::string lowercase_str = origin_str;
    // Convert to lowercase and compare
    for (char& c : lowercase_str) {
      c = std::tolower(static_cast<unsigned char>(c));
    }
    EXPECT_EQ(origin_str, lowercase_str)
        << "Origin '" << origin_str << "' contains uppercase characters";
  }
}

}  // namespace brave::context_menu
