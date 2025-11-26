// Copyright 2025 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

import BraveCore
import Preferences
import TestHelpers
import Web
import XCTest

@testable import BraveShields
@testable import Data

@MainActor
class BraveShieldsSettingsMigrationTests: CoreDataTestCase {

  let shieldsEnabledURL = URL(string: "https://shields-enabled.com")!
  let adBlockModeURL = URL(string: "https://ad-block-mode.com")!
  let blockScriptsURL = URL(string: "https://block-scripts.com.com")!
  let fingerprintProtectionURL = URL(string: "https://fingerprint-protection.com")!
  let autoShredModeURL = URL(string: "https://auto-shred-mode.com")!

  func testGlobalMigrations() {
    // setup initial values
    Preferences.Shields.blockAdsAndTrackingLevel = .aggressive
    Preferences.Shields.blockScripts.value = true
    Preferences.Shields.fingerprintingProtection.value = false
    Preferences.Shields.shredLevel = .whenSiteClosed

    // verify expected migrations
    let testBraveShieldsSettings = TestBraveShieldsSettings()
    testBraveShieldsSettings._setAdBlockMode = { adBlockMode, url in
      XCTAssertEqual(adBlockMode, .aggressive)
    }
    testBraveShieldsSettings._setBlockScriptsEnabled = { enabled, url in
      XCTAssertTrue(enabled)
    }
    testBraveShieldsSettings._setFingerprintMode = { fingerprintMode, url in
      XCTAssertEqual(fingerprintMode, .allowMode)
    }
    testBraveShieldsSettings._setAutoShredMode = { autoShredMode, url in
      XCTAssertEqual(autoShredMode, .lastTabClosed)
    }
    testBraveShieldsSettings.migrateGlobalSettings()
  }

  func testSiteSpecificMigration() {
    // setup initial values
    let shieldsEnabledDomain = Domain.getOrCreate(forUrl: shieldsEnabledURL, persistent: true)
    shieldsEnabledDomain.shield_allOff = NSNumber(booleanLiteral: true)
    let adBlockModeDomain = Domain.getOrCreate(forUrl: adBlockModeURL, persistent: true)
    adBlockModeDomain.domainBlockAdsAndTrackingLevel = .aggressive
    let blockScriptsDomain = Domain.getOrCreate(forUrl: blockScriptsURL, persistent: true)
    blockScriptsDomain.shield_noScript = NSNumber(booleanLiteral: true)
    let fingerprintProtectionDomain = Domain.getOrCreate(
      forUrl: fingerprintProtectionURL,
      persistent: true
    )
    fingerprintProtectionDomain.shield_fpProtection = NSNumber(booleanLiteral: false)
    let autoShredModeDomain = Domain.getOrCreate(forUrl: fingerprintProtectionURL, persistent: true)
    autoShredModeDomain.shredLevel = .whenSiteClosed

    // verify expected migrations
    let testBraveShieldsSettings = TestBraveShieldsSettings()
    testBraveShieldsSettings._setBraveShieldsEnabled = { enabled, url in
      XCTAssertFalse(enabled)
      XCTAssertEqual(url.domainURL.absoluteString, shieldsEnabledDomain.url ?? "")
    }
    testBraveShieldsSettings._setAdBlockMode = { adBlockMode, url in
      XCTAssertEqual(adBlockMode, .aggressive)
      XCTAssertEqual(url.domainURL.absoluteString, adBlockModeDomain.url ?? "")
    }
    testBraveShieldsSettings._setBlockScriptsEnabled = { enabled, url in
      XCTAssertEqual(url.domainURL.absoluteString, blockScriptsDomain.url ?? "")
    }
    testBraveShieldsSettings._setFingerprintMode = { fingerprintMode, url in
      XCTAssertEqual(fingerprintMode, .allowMode)
      XCTAssertEqual(url.domainURL.absoluteString, fingerprintProtectionDomain.url ?? "")
    }
    testBraveShieldsSettings._setAutoShredMode = { autoShredMode, url in
      XCTAssertEqual(autoShredMode, .lastTabClosed)
      XCTAssertEqual(url.domainURL.absoluteString, autoShredModeDomain.url ?? "")
    }

    let allDomains = Domain.allDomainsWithExlicitShieldSettings()
    testBraveShieldsSettings.migrateShieldsToContentSettings(for: allDomains)
  }

  /// Some shields settings are stored on 2 different CoreData `Domain` models,
  /// but would map to only 1 Content Setting pattern due to using the domain
  /// pattern.
  /// For example, Auto Shred Mode would be stored twice for
  ///  `account.brave.com` & `profile.brave.com` with CoreData, but both would
  ///  be stored on `brave.com` for Content Settings.
  func testSiteSpecificMigrationDomainPattern() {
    // these 3 URLs are all treated as the
    let autoShredAppExitURL = URL(string: "https://a.brave.com")!
    let autoShredNeverURL = URL(string: "https://b.brave.com")!
    let autoShredDefaultURL = URL(string: "https://c.brave.com")!

    let autoShredAppExitDomain = Domain.getOrCreate(
      forUrl: autoShredAppExitURL,
      persistent: true
    )
    autoShredAppExitDomain.shredLevel = .appExit
    let autoShredNeverDomain = Domain.getOrCreate(
      forUrl: autoShredNeverURL,
      persistent: true
    )
    autoShredNeverDomain.shredLevel = .whenSiteClosed
    let autoShredDefaultDomain = Domain.getOrCreate(
      forUrl: autoShredDefaultURL,
      persistent: true
    )

    // verify expected migrations
    let testBraveShieldsSettings = TestBraveShieldsSettings()
    testBraveShieldsSettings._setAutoShredMode = { autoShredMode, url in
      // sorted alphabetically, so expecting a.brave.com to take precedence.
      XCTAssertEqual(autoShredMode, .appExit)
      XCTAssertEqual(url.domainURL.absoluteString, autoShredAppExitDomain.url ?? "")
    }

    let allDomains = Domain.allDomainsWithExlicitShieldSettings()
    testBraveShieldsSettings.migrateShieldsToContentSettings(for: allDomains)
  }

  func testSiteSpecificMigrationHttpsPreferred() {
    // TODO: Test https preferred over http
  }
}
