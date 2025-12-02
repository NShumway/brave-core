// Copyright (c) 2025 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "brave/components/permissions/contexts/brave_context_menu_permission_context.h"

#include "components/content_settings/core/common/content_settings_types.h"

namespace permissions {

BraveContextMenuPermissionContext::BraveContextMenuPermissionContext(
    content::BrowserContext* browser_context)
    : ContentSettingPermissionContextBase(
          browser_context,
          ContentSettingsType::BRAVE_CONTEXT_MENU,
          network::mojom::PermissionsPolicyFeature::kNotFound) {}

BraveContextMenuPermissionContext::~BraveContextMenuPermissionContext() =
    default;

bool BraveContextMenuPermissionContext::IsRestrictedToSecureOrigins() const {
  return false;
}

}  // namespace permissions
