/* Copyright (c) 2023 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <third_party/blink/renderer/core/frame/local_frame_mojo_handler.cc>

namespace blink {

// Defined in event_dispatcher.cc
void UpdateContextMenuSettingCache(
    const std::string& origin,
    mojom::blink::ContextMenuContentSetting setting);

void LocalFrameMojoHandler::GetImageAt(const gfx::Point& window_point,
                                       GetImageAtCallback callback) {
  gfx::Point viewport_position =
      frame_->GetWidgetForLocalRoot()->DIPsToRoundedBlinkSpace(window_point);
  std::move(callback).Run(frame_->GetImageAtViewportPoint(viewport_position));
}

void LocalFrameMojoHandler::UpdateContextMenuContentSetting(
    const scoped_refptr<const SecurityOrigin>& origin,
    mojom::blink::ContextMenuContentSetting setting) {
  if (!origin || origin->IsOpaque()) {
    return;
  }
  UpdateContextMenuSettingCache(origin->ToString().Utf8(), setting);
}

}  // namespace blink
