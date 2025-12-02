// Copyright (c) 2025 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BRAVE_COMPONENTS_CONTEXT_MENU_CORE_COMMON_DEFAULT_ALLOWLIST_H_
#define BRAVE_COMPONENTS_CONTEXT_MENU_CORE_COMMON_DEFAULT_ALLOWLIST_H_

#include <algorithm>
#include <string_view>

#include "base/containers/fixed_flat_set.h"

namespace brave::context_menu {

// Origins that are pre-allowed to hide the browser's native context menu
// without prompting the user. These are sites with well-known, legitimate
// custom context menus (productivity apps, design tools, IDEs).
//
// This list is checked BEFORE showing a permission prompt - if the origin is
// in this list AND the user has no explicit preference stored, the site is
// silently allowed to hide the context menu.
//
// Conservative policy: No wildcards, explicit subdomains only. Users can
// always override via Site Settings.
//
// IMPORTANT: All entries MUST be lowercase. url::Origin::Serialize() returns
// normalized lowercase origins per RFC 3986 Section 3.2.2, so comparison
// against this list will work correctly. The static_assert below verifies
// this at compile time.
//
// Note: entries must be sorted lexicographically for base::sorted_unique.
inline constexpr auto kDefaultAllowHidingOrigins =
    base::MakeFixedFlatSet<std::string_view>(base::sorted_unique,
                                             {
                                                 "https://codesandbox.io",
                                                 "https://docs.google.com",
                                                 "https://excalidraw.com",
                                                 "https://figma.com",
                                                 "https://miro.com",
                                                 "https://notion.so",
                                                 "https://sheets.google.com",
                                                 "https://slides.google.com",
                                                 "https://stackblitz.com",
                                                 "https://www.canva.com",
                                                 "https://www.figma.com",
                                                 "https://www.notion.so",
                                             });

namespace internal {

// Helper to check if a string is all lowercase at compile time.
constexpr bool IsLowercase(std::string_view str) {
  for (char c : str) {
    if (c >= 'A' && c <= 'Z') {
      return false;
    }
  }
  return true;
}

// Verify all allowlist entries are lowercase at compile time.
constexpr bool AllEntriesAreLowercase() {
  for (std::string_view entry : kDefaultAllowHidingOrigins) {
    if (!IsLowercase(entry)) {
      return false;
    }
  }
  return true;
}

}  // namespace internal

static_assert(internal::AllEntriesAreLowercase(),
              "All allowlist entries must be lowercase (url::Origin::Serialize"
              " returns lowercase per RFC 3986)");

// Returns true if the given origin (as a string like "https://example.com")
// is in the default allowlist for hiding context menus.
//
// Note: The origin parameter should come from url::Origin::Serialize() which
// is already normalized to lowercase. Direct user input should be parsed
// through url::Origin first.
inline bool IsInDefaultAllowlist(std::string_view origin) {
  return kDefaultAllowHidingOrigins.contains(origin);
}

}  // namespace brave::context_menu

#endif  // BRAVE_COMPONENTS_CONTEXT_MENU_CORE_COMMON_DEFAULT_ALLOWLIST_H_
