/* Copyright (c) 2024 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {
void BraveHandleBlockedContextMenu(Node* node, Event* event);
}  // namespace blink

// The macro is inserted in the else branch of DispatchEventPostProcess(),
// which means event->defaultPrevented() is true (or DefaultHandled, or
// !is_trusted_or_click). We specifically check for contextmenu events
// that were trusted and had their default prevented.
#define BRAVE_DISPATCH_EVENT_POST_PROCESS_CHECK_CONTEXT_MENU               \
  if (event_->type() == event_type_names::kContextmenu &&                  \
      event_->isTrusted() && event_->defaultPrevented()) {                 \
    BraveHandleBlockedContextMenu(node_, event_);                          \
  }

#include <third_party/blink/renderer/core/dom/events/event_dispatcher.cc>  // IWYU pragma: export

#undef BRAVE_DISPATCH_EVENT_POST_PROCESS_CHECK_CONTEXT_MENU

namespace blink {

// Per-renderer-process cache of context menu content settings.
// Key: serialized origin (e.g., "https://example.com")
// Value: cached setting from browser
//
// On cache miss, we allow hiding (legacy behavior) and asynchronously fetch
// the setting. The browser will show a permission prompt if needed, and the
// IPC callback updates the cache for subsequent right-clicks.
base::flat_map<std::string, mojom::blink::ContextMenuContentSetting>&
GetContextMenuSettingsCache() {
  static base::NoDestructor<
      base::flat_map<std::string, mojom::blink::ContextMenuContentSetting>>
      cache;
  return *cache;
}

// Called by LocalFrameMojoHandler when the browser pushes a setting update.
void UpdateContextMenuSettingCache(
    const std::string& origin,
    mojom::blink::ContextMenuContentSetting setting) {
  if (setting == mojom::blink::ContextMenuContentSetting::kAsk) {
    // Remove from cache if set to "Ask" (default)
    GetContextMenuSettingsCache().erase(origin);
  } else {
    GetContextMenuSettingsCache()[origin] = setting;
  }
}

// Handles the case when a site blocks the context menu via preventDefault().
// Uses a renderer-side cache to avoid repeated IPC:
// - Cache hit with kDisallowHiding: Force native context menu
// - Cache hit with kAllowHiding: Do nothing (let site's custom menu show)
// - Cache miss: Allow hiding (legacy behavior), send async IPC to fetch setting
void BraveHandleBlockedContextMenu(Node* node, Event* event) {
  if (!node || !event) {
    return;
  }

  Document& document = node->GetDocument();
  LocalFrame* frame = document.GetFrame();
  if (!frame) {
    return;
  }

  ExecutionContext* context = document.GetExecutionContext();
  if (!context) {
    return;
  }

  const SecurityOrigin* origin = context->GetSecurityOrigin();
  if (!origin) {
    return;
  }

  // Opaque origins (data:, blob:, sandboxed iframes) can't have persistent
  // settings. Allow hiding (legacy behavior).
  if (origin->IsOpaque()) {
    return;
  }

  // Local files (file://) can't reliably store content settings because
  // their origins serialize inconsistently. Allow hiding (legacy behavior).
  if (origin->IsLocal()) {
    return;
  }

  std::string origin_string = origin->ToString().Utf8();
  auto& cache = GetContextMenuSettingsCache();

  // Check cache first - no IPC needed if we have a cached setting.
  auto it = cache.find(origin_string);
  if (it != cache.end()) {
    switch (it->second) {
      case mojom::blink::ContextMenuContentSetting::kAllowHiding:
        // User has allowed site to hide context menu. Do nothing.
        return;

      case mojom::blink::ContextMenuContentSetting::kDisallowHiding:
        // User wants to force native context menu.
        node->DefaultEventHandler(*event);
        return;

      case mojom::blink::ContextMenuContentSetting::kAsk:
        // Should not happen - we don't cache kAsk. Fall through to IPC.
        break;
    }
  }

  // Cache miss: Allow hiding (legacy behavior) and send async IPC.
  // The browser will check settings, potentially show a prompt, and return
  // the setting which we cache for subsequent right-clicks.
  scoped_refptr<const SecurityOrigin> origin_copy = origin->IsolatedCopy();

  frame->GetLocalFrameHostRemote().OnContextMenuBlockedBySite(
      origin_copy,
      base::BindOnce(
          [](std::string origin_key,
             mojom::blink::ContextMenuContentSetting setting) {
            // Only cache kAllowHiding or kDisallowHiding - not kAsk.
            if (setting != mojom::blink::ContextMenuContentSetting::kAsk) {
              GetContextMenuSettingsCache()[std::move(origin_key)] = setting;
            }
          },
          origin_string));
}

}  // namespace blink
