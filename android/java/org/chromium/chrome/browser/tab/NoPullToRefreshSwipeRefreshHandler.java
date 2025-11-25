/* Copyright (c) 2025 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

package org.chromium.chrome.browser.tab;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.SwipeRefreshHandler;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.OverscrollAction;
import org.chromium.ui.OverscrollRefreshHandler;
import org.chromium.ui.base.BackGestureEventSwipeEdge;

@NullMarked
public class NoPullToRefreshSwipeRefreshHandler extends TabWebContentsUserData
        implements OverscrollRefreshHandler {
    private static final Class<NoPullToRefreshSwipeRefreshHandler> USER_DATA_KEY =
            NoPullToRefreshSwipeRefreshHandler.class;

    private final Tab mTab;
    private final SwipeRefreshHandler mDelegate;
    private boolean mHandlingPullToRefresh;

    public static NoPullToRefreshSwipeRefreshHandler from(Tab tab) {
        NoPullToRefreshSwipeRefreshHandler handler = get(tab);
        if (handler == null) {
            handler =
                    tab.getUserDataHost()
                            .setUserData(
                                    USER_DATA_KEY, new NoPullToRefreshSwipeRefreshHandler(tab));
        }
        return handler;
    }

    public static @Nullable NoPullToRefreshSwipeRefreshHandler get(Tab tab) {
        assert tab != null;
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    private NoPullToRefreshSwipeRefreshHandler(Tab tab) {
        super(tab);

        assert tab != null;
        mTab = tab;

        SwipeRefreshHandler delegate = SwipeRefreshHandler.get(mTab);
        assert delegate != null;
        mDelegate = delegate;

        WebContents webContents = tab.getWebContents();
        assert webContents != null;
        initWebContents(webContents);
    }

    @Override
    public void initWebContents(WebContents webContents) {
        assert webContents != null;
        webContents.setOverscrollRefreshHandler(this);
    }

    @Override
    public void cleanupWebContents(WebContents webContents) {}

    @Override
    public boolean start(
            @OverscrollAction int type, @BackGestureEventSwipeEdge int initiatingEdge) {
        mHandlingPullToRefresh = type == OverscrollAction.PULL_TO_REFRESH;
        return mHandlingPullToRefresh || mDelegate.start(type, initiatingEdge);
    }

    @Override
    public void pull(float xDelta, float yDelta) {
        if (!mHandlingPullToRefresh) {
            mDelegate.pull(xDelta, yDelta);
        }
    }

    @Override
    public void release(boolean allowRefresh) {
        if (!mHandlingPullToRefresh) {
            mDelegate.release(allowRefresh);
        }
    }

    @Override
    public void reset() {
        mDelegate.reset();
        mHandlingPullToRefresh = false;
    }

    @Override
    public void setEnabled(boolean enabled) {
        mDelegate.setEnabled(enabled);
    }
}
